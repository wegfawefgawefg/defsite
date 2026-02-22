#include "common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *NATIVE_TAGS[] = {
    "a", "abbr", "address", "area", "article", "aside", "audio", "b", "base", "bdi",
    "bdo", "blockquote", "body", "br", "button", "canvas", "caption", "cite", "code",
    "col", "colgroup", "data", "datalist", "dd", "del", "details", "dfn", "dialog",
    "div", "dl", "dt", "em", "embed", "fieldset", "figcaption", "figure", "footer",
    "form", "h1", "h2", "h3", "h4", "h5", "h6", "head", "header", "hgroup", "hr",
    "html", "i", "iframe", "img", "input", "ins", "kbd", "label", "legend", "li",
    "link", "main", "map", "mark", "menu", "meta", "meter", "nav", "noscript", "object",
    "ol", "optgroup", "option", "output", "p", "param", "picture", "pre", "progress",
    "q", "rp", "rt", "ruby", "s", "samp", "script", "search", "section", "select",
    "slot", "small", "source", "span", "strong", "style", "sub", "summary", "sup", "table",
    "tbody", "td", "template", "textarea", "tfoot", "th", "thead", "time", "title", "tr",
    "track", "u", "ul", "var", "video", "wbr", "svg", "path", "g", "defs", "use", "circle",
    "ellipse", "line", "polygon", "polyline", "rect", "text", "lineargradient", "radialgradient",
    "stop", "symbol", "view", "clippath", "filter", "mask", "foreignobject"
};

static const char *VOID_TAGS[] = {
    "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param",
    "source", "track", "wbr"
};

static const size_t NATIVE_TAG_COUNT = sizeof(NATIVE_TAGS) / sizeof(NATIVE_TAGS[0]);
static const size_t VOID_TAG_COUNT = sizeof(VOID_TAGS) / sizeof(VOID_TAGS[0]);

static Node *node_new(NodeType type) {
    Node *n = xmalloc(sizeof(Node));
    n->type = type;
    n->tag = NULL;
    n->text = NULL;
    n->attrs = NULL;
    n->attr_count = 0;
    n->attr_cap = 0;
    n->children = NULL;
    n->child_count = 0;
    n->child_cap = 0;
    n->parent = NULL;
    return n;
}

Node *node_new_document(void) {
    return node_new(NODE_DOCUMENT);
}

Node *node_new_element(const char *tag) {
    Node *n = node_new(NODE_ELEMENT);
    n->tag = xstrdup(tag);
    return n;
}

Node *node_new_text(const char *text) {
    Node *n = node_new(NODE_TEXT);
    n->text = xstrdup(text);
    return n;
}

Node *node_new_comment(const char *text) {
    Node *n = node_new(NODE_COMMENT);
    n->text = xstrdup(text);
    return n;
}

Node *node_new_decl(const char *text) {
    Node *n = node_new(NODE_DECL);
    n->text = xstrdup(text);
    return n;
}

void node_add_attr(Node *n, const char *name, const char *value) {
    if (n->type != NODE_ELEMENT) {
        return;
    }
    if (n->attr_count == n->attr_cap) {
        size_t next = n->attr_cap == 0 ? 4 : n->attr_cap * 2;
        n->attrs = xrealloc(n->attrs, next * sizeof(Attr));
        n->attr_cap = next;
    }
    n->attrs[n->attr_count].name = xstrdup(name);
    n->attrs[n->attr_count].value = xstrdup(value);
    n->attr_count++;
}

const char *node_get_attr(const Node *n, const char *name) {
    if (!n || n->type != NODE_ELEMENT) {
        return NULL;
    }
    for (size_t i = 0; i < n->attr_count; i++) {
        if (str_eq(n->attrs[i].name, name)) {
            return n->attrs[i].value;
        }
    }
    return NULL;
}

void node_remove_attr(Node *n, const char *name) {
    if (!n || n->type != NODE_ELEMENT) {
        return;
    }
    for (size_t i = 0; i < n->attr_count; i++) {
        if (str_eq(n->attrs[i].name, name)) {
            free(n->attrs[i].name);
            free(n->attrs[i].value);
            if (i + 1 < n->attr_count) {
                memmove(&n->attrs[i], &n->attrs[i + 1], (n->attr_count - i - 1) * sizeof(Attr));
            }
            n->attr_count--;
            return;
        }
    }
}

void node_add_child(Node *parent, Node *child) {
    if (parent->child_count == parent->child_cap) {
        size_t next = parent->child_cap == 0 ? 4 : parent->child_cap * 2;
        parent->children = xrealloc(parent->children, next * sizeof(Node *));
        parent->child_cap = next;
    }
    child->parent = parent;
    parent->children[parent->child_count++] = child;
}

void node_free(Node *n) {
    if (!n) {
        return;
    }
    free(n->tag);
    free(n->text);
    for (size_t i = 0; i < n->attr_count; i++) {
        free(n->attrs[i].name);
        free(n->attrs[i].value);
    }
    free(n->attrs);
    for (size_t i = 0; i < n->child_count; i++) {
        node_free(n->children[i]);
    }
    free(n->children);
    free(n);
}

Node *node_clone(const Node *src) {
    Node *dst = node_new(src->type);
    if (src->tag) {
        dst->tag = xstrdup(src->tag);
    }
    if (src->text) {
        dst->text = xstrdup(src->text);
    }
    for (size_t i = 0; i < src->attr_count; i++) {
        node_add_attr(dst, src->attrs[i].name, src->attrs[i].value);
    }
    for (size_t i = 0; i < src->child_count; i++) {
        node_add_child(dst, node_clone(src->children[i]));
    }
    return dst;
}

void node_replace_child(Node *parent, size_t idx, Node **new_nodes, size_t new_count) {
    if (!parent || idx >= parent->child_count) {
        return;
    }

    Node *old = parent->children[idx];
    size_t old_count = parent->child_count;
    size_t final_count = old_count - 1 + new_count;

    if (final_count > parent->child_cap) {
        size_t next = parent->child_cap == 0 ? 4 : parent->child_cap;
        while (next < final_count) {
            next *= 2;
        }
        parent->children = xrealloc(parent->children, next * sizeof(Node *));
        parent->child_cap = next;
    }

    if (new_count > 1) {
        memmove(&parent->children[idx + new_count], &parent->children[idx + 1], (old_count - idx - 1) * sizeof(Node *));
    } else if (new_count == 0) {
        memmove(&parent->children[idx], &parent->children[idx + 1], (old_count - idx - 1) * sizeof(Node *));
    }

    for (size_t i = 0; i < new_count; i++) {
        parent->children[idx + i] = new_nodes[i];
        new_nodes[i]->parent = parent;
    }

    parent->child_count = final_count;
    node_free(old);
}

bool is_void_tag(const char *tag) {
    for (size_t i = 0; i < VOID_TAG_COUNT; i++) {
        if (str_eq(tag, VOID_TAGS[i])) {
            return true;
        }
    }
    return false;
}

bool is_native_tag(const char *tag) {
    for (size_t i = 0; i < NATIVE_TAG_COUNT; i++) {
        if (str_eq(tag, NATIVE_TAGS[i])) {
            return true;
        }
    }
    return false;
}

bool is_def_tag(const char *tag) {
    return starts_with(tag, "def-") && strlen(tag) > 4;
}

bool is_valid_symbol(const char *name) {
    if (!name || !name[0]) {
        return false;
    }
    if (!(isalpha((unsigned char)name[0]))) {
        return false;
    }
    for (size_t i = 0; name[i]; i++) {
        char c = name[i];
        if (!(isalnum((unsigned char)c) || c == '-')) {
            return false;
        }
    }
    return true;
}

void strstack_push(StringStack *s, const char *item) {
    if (s->count == s->cap) {
        size_t next = s->cap == 0 ? 8 : s->cap * 2;
        s->items = xrealloc(s->items, next * sizeof(char *));
        s->cap = next;
    }
    s->items[s->count++] = xstrdup(item);
}

void strstack_pop(StringStack *s) {
    if (s->count == 0) {
        return;
    }
    free(s->items[s->count - 1]);
    s->count--;
}

bool strstack_contains(const StringStack *s, const char *item) {
    for (size_t i = 0; i < s->count; i++) {
        if (str_eq(s->items[i], item)) {
            return true;
        }
    }
    return false;
}

void strstack_free(StringStack *s) {
    for (size_t i = 0; i < s->count; i++) {
        free(s->items[i]);
    }
    free(s->items);
}

void nodelist_push(NodeList *list, Node *node) {
    if (list->count == list->cap) {
        size_t next = list->cap == 0 ? 4 : list->cap * 2;
        list->items = xrealloc(list->items, next * sizeof(Node *));
        list->cap = next;
    }
    list->items[list->count++] = node;
}

void nodelist_free(NodeList *list) {
    for (size_t i = 0; i < list->count; i++) {
        node_free(list->items[i]);
    }
    free(list->items);
}

NamedSlot *slotpayload_get_named(SlotPayload *payload, const char *name) {
    for (size_t i = 0; i < payload->named_count; i++) {
        if (str_eq(payload->named[i].name, name)) {
            return &payload->named[i];
        }
    }
    if (payload->named_count == payload->named_cap) {
        size_t next = payload->named_cap == 0 ? 4 : payload->named_cap * 2;
        payload->named = xrealloc(payload->named, next * sizeof(NamedSlot));
        payload->named_cap = next;
    }

    NamedSlot *slot = &payload->named[payload->named_count++];
    slot->name = xstrdup(name);
    slot->nodes.items = NULL;
    slot->nodes.count = 0;
    slot->nodes.cap = 0;
    slot->used = false;
    return slot;
}

void slotpayload_free(SlotPayload *payload) {
    nodelist_free(&payload->default_nodes);
    for (size_t i = 0; i < payload->named_count; i++) {
        free(payload->named[i].name);
        nodelist_free(&payload->named[i].nodes);
    }
    free(payload->named);
}

void scope_init(Scope *scope, Scope *parent) {
    scope->parent = parent;
    scope->defs = NULL;
    scope->def_count = 0;
    scope->def_cap = 0;
}

void scope_free(Scope *scope) {
    for (size_t i = 0; i < scope->def_count; i++) {
        free(scope->defs[i].name);
        node_free(scope->defs[i].def_node);
    }
    free(scope->defs);
}

DefEntry *scope_find_local_def(Scope *scope, const char *name) {
    for (size_t i = 0; i < scope->def_count; i++) {
        if (str_eq(scope->defs[i].name, name)) {
            return &scope->defs[i];
        }
    }
    return NULL;
}

void scope_add_def(Scope *scope, const char *name, Node *def_node) {
    if (scope->def_count == scope->def_cap) {
        size_t next = scope->def_cap == 0 ? 4 : scope->def_cap * 2;
        scope->defs = xrealloc(scope->defs, next * sizeof(DefEntry));
        scope->def_cap = next;
    }
    scope->defs[scope->def_count].name = xstrdup(name);
    scope->defs[scope->def_count].def_node = node_clone(def_node);
    scope->def_count++;
}

DefEntry *scope_resolve(Scope *scope, const char *name) {
    Scope *cur = scope;
    while (cur) {
        DefEntry *match = scope_find_local_def(cur, name);
        if (match) {
            return match;
        }
        cur = cur->parent;
    }
    return NULL;
}

char *escape_html_text(const char *s) {
    StrBuf b = {0};
    size_t n = strlen(s);

    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        const char *rep = NULL;
        if (c == '&') {
            rep = "&amp;";
        } else if (c == '<') {
            rep = "&lt;";
        } else if (c == '>') {
            rep = "&gt;";
        }

        if (rep) {
            sb_append(&b, rep);
        } else {
            sb_append_n(&b, &c, 1);
        }
    }

    if (!b.data) {
        return xstrdup("");
    }
    return b.data;
}

static void serialize_attr(StrBuf *b, const char *name, const char *value) {
    sb_append(b, " ");
    sb_append(b, name);
    sb_append(b, "=\"");
    for (const char *p = value; *p; p++) {
        if (*p == '&') {
            sb_append(b, "&amp;");
        } else if (*p == '"') {
            sb_append(b, "&quot;");
        } else if (*p == '<') {
            sb_append(b, "&lt;");
        } else if (*p == '>') {
            sb_append(b, "&gt;");
        } else {
            sb_append_n(b, p, 1);
        }
    }
    sb_append(b, "\"");
}

void serialize_node(StrBuf *b, const Node *n) {
    switch (n->type) {
    case NODE_DOCUMENT:
        for (size_t i = 0; i < n->child_count; i++) {
            serialize_node(b, n->children[i]);
        }
        break;
    case NODE_TEXT:
        if (n->text) {
            sb_append(b, n->text);
        }
        break;
    case NODE_COMMENT:
        sb_append(b, "<!--");
        if (n->text) {
            sb_append(b, n->text);
        }
        sb_append(b, "-->");
        break;
    case NODE_DECL:
        sb_append(b, "<!");
        if (n->text) {
            sb_append(b, n->text);
        }
        sb_append(b, ">");
        break;
    case NODE_ELEMENT:
        sb_append(b, "<");
        sb_append(b, n->tag ? n->tag : "");
        for (size_t i = 0; i < n->attr_count; i++) {
            serialize_attr(b, n->attrs[i].name, n->attrs[i].value);
        }
        sb_append(b, ">");
        if (!is_void_tag(n->tag ? n->tag : "")) {
            for (size_t i = 0; i < n->child_count; i++) {
                serialize_node(b, n->children[i]);
            }
            sb_append(b, "</");
            sb_append(b, n->tag ? n->tag : "");
            sb_append(b, ">");
        }
        break;
    }
}
