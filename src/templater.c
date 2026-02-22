#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096
#define MAX_EXPANSION_DEPTH 64

typedef enum {
    NODE_DOCUMENT,
    NODE_ELEMENT,
    NODE_TEXT,
    NODE_COMMENT,
    NODE_DECL
} NodeType;

typedef struct {
    char *name;
    char *value;
} Attr;

typedef struct Node Node;
struct Node {
    NodeType type;
    char *tag;
    char *text;
    Attr *attrs;
    size_t attr_count;
    size_t attr_cap;
    Node **children;
    size_t child_count;
    size_t child_cap;
    Node *parent;
};

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} StringStack;

typedef struct {
    int error_count;
    int warning_count;
    const char *current_file;
} BuildCtx;

typedef struct {
    char *name;
    Node *def_node;
} DefEntry;

typedef struct Scope Scope;
struct Scope {
    Scope *parent;
    DefEntry *defs;
    size_t def_count;
    size_t def_cap;
};

typedef struct {
    Node **items;
    size_t count;
    size_t cap;
} NodeList;

typedef struct {
    char *name;
    NodeList nodes;
    bool used;
} NamedSlot;

typedef struct {
    NodeList default_nodes;
    NamedSlot *named;
    size_t named_count;
    size_t named_cap;
} SlotPayload;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
    int parse_errors;
} Parser;

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

static const size_t NATIVE_TAG_COUNT = sizeof(NATIVE_TAGS) / sizeof(NATIVE_TAGS[0]);
static const char *VOID_TAGS[] = {
    "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param",
    "source", "track", "wbr"
};
static const size_t VOID_TAG_COUNT = sizeof(VOID_TAGS) / sizeof(VOID_TAGS[0]);

static void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    return ptr;
}

static void *xrealloc(void *ptr, size_t size) {
    void *next = realloc(ptr, size);
    if (!next) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    return next;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *out = xmalloc(n + 1);
    memcpy(out, s, n + 1);
    return out;
}

static char *substr_dup(const char *s, size_t start, size_t end) {
    if (end < start) {
        end = start;
    }
    size_t n = end - start;
    char *out = xmalloc(n + 1);
    memcpy(out, s + start, n);
    out[n] = '\0';
    return out;
}

static void to_lower_inplace(char *s) {
    for (; *s; s++) {
        *s = (char)tolower((unsigned char)*s);
    }
}

static bool str_eq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static bool starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool starts_with_at(const char *s, size_t len, size_t pos, const char *prefix) {
    size_t p_len = strlen(prefix);
    if (pos + p_len > len) {
        return false;
    }
    return strncmp(s + pos, prefix, p_len) == 0;
}

static size_t find_ci(const char *haystack, size_t len, size_t start, const char *needle) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || start >= len) {
        return (size_t)-1;
    }
    for (size_t i = start; i + needle_len <= len; i++) {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++) {
            char h = (char)tolower((unsigned char)haystack[i + j]);
            char n = (char)tolower((unsigned char)needle[j]);
            if (h != n) {
                match = false;
                break;
            }
        }
        if (match) {
            return i;
        }
    }
    return (size_t)-1;
}

static void log_msg(BuildCtx *ctx, const char *kind, const char *fmt, va_list ap) {
    fprintf(stderr, "%s", kind);
    if (ctx->current_file && ctx->current_file[0] != '\0') {
        fprintf(stderr, "[%s] ", ctx->current_file);
    }
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

static void log_error(BuildCtx *ctx, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_msg(ctx, "ERROR: ", fmt, ap);
    va_end(ap);
    ctx->error_count++;
}

static void log_warning(BuildCtx *ctx, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_msg(ctx, "WARN: ", fmt, ap);
    va_end(ap);
    ctx->warning_count++;
}

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

static Node *node_new_document(void) {
    return node_new(NODE_DOCUMENT);
}

static Node *node_new_element(const char *tag) {
    Node *n = node_new(NODE_ELEMENT);
    n->tag = xstrdup(tag);
    return n;
}

static Node *node_new_text(const char *text) {
    Node *n = node_new(NODE_TEXT);
    n->text = xstrdup(text);
    return n;
}

static Node *node_new_comment(const char *text) {
    Node *n = node_new(NODE_COMMENT);
    n->text = xstrdup(text);
    return n;
}

static Node *node_new_decl(const char *text) {
    Node *n = node_new(NODE_DECL);
    n->text = xstrdup(text);
    return n;
}

static void node_add_attr(Node *n, const char *name, const char *value) {
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

static const char *node_get_attr(const Node *n, const char *name) {
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

static void node_remove_attr(Node *n, const char *name) {
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

static void node_add_child(Node *parent, Node *child) {
    if (parent->child_count == parent->child_cap) {
        size_t next = parent->child_cap == 0 ? 4 : parent->child_cap * 2;
        parent->children = xrealloc(parent->children, next * sizeof(Node *));
        parent->child_cap = next;
    }
    child->parent = parent;
    parent->children[parent->child_count++] = child;
}

static void node_free(Node *n) {
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

static Node *node_clone(const Node *src) {
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

static void node_replace_child(Node *parent, size_t idx, Node **new_nodes, size_t new_count) {
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

static bool is_void_tag(const char *tag) {
    for (size_t i = 0; i < VOID_TAG_COUNT; i++) {
        if (str_eq(tag, VOID_TAGS[i])) {
            return true;
        }
    }
    return false;
}

static bool is_native_tag(const char *tag) {
    for (size_t i = 0; i < NATIVE_TAG_COUNT; i++) {
        if (str_eq(tag, NATIVE_TAGS[i])) {
            return true;
        }
    }
    return false;
}

static bool is_def_tag(const char *tag) {
    return starts_with(tag, "def-") && strlen(tag) > 4;
}

static bool is_valid_symbol(const char *name) {
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

static void strstack_push(StringStack *s, const char *item) {
    if (s->count == s->cap) {
        size_t next = s->cap == 0 ? 8 : s->cap * 2;
        s->items = xrealloc(s->items, next * sizeof(char *));
        s->cap = next;
    }
    s->items[s->count++] = xstrdup(item);
}

static void strstack_pop(StringStack *s) {
    if (s->count == 0) {
        return;
    }
    free(s->items[s->count - 1]);
    s->count--;
}

static bool strstack_contains(const StringStack *s, const char *item) {
    for (size_t i = 0; i < s->count; i++) {
        if (str_eq(s->items[i], item)) {
            return true;
        }
    }
    return false;
}

static void strstack_free(StringStack *s) {
    for (size_t i = 0; i < s->count; i++) {
        free(s->items[i]);
    }
    free(s->items);
}

static void nodelist_push(NodeList *list, Node *node) {
    if (list->count == list->cap) {
        size_t next = list->cap == 0 ? 4 : list->cap * 2;
        list->items = xrealloc(list->items, next * sizeof(Node *));
        list->cap = next;
    }
    list->items[list->count++] = node;
}

static void nodelist_free(NodeList *list) {
    for (size_t i = 0; i < list->count; i++) {
        node_free(list->items[i]);
    }
    free(list->items);
}

static NamedSlot *slotpayload_get_named(SlotPayload *payload, const char *name) {
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

static void slotpayload_free(SlotPayload *payload) {
    nodelist_free(&payload->default_nodes);
    for (size_t i = 0; i < payload->named_count; i++) {
        free(payload->named[i].name);
        nodelist_free(&payload->named[i].nodes);
    }
    free(payload->named);
}

static void scope_init(Scope *scope, Scope *parent) {
    scope->parent = parent;
    scope->defs = NULL;
    scope->def_count = 0;
    scope->def_cap = 0;
}

static void scope_free(Scope *scope) {
    for (size_t i = 0; i < scope->def_count; i++) {
        free(scope->defs[i].name);
        node_free(scope->defs[i].def_node);
    }
    free(scope->defs);
}

static DefEntry *scope_find_local_def(Scope *scope, const char *name) {
    for (size_t i = 0; i < scope->def_count; i++) {
        if (str_eq(scope->defs[i].name, name)) {
            return &scope->defs[i];
        }
    }
    return NULL;
}

static void scope_add_def(Scope *scope, const char *name, Node *def_node) {
    if (scope->def_count == scope->def_cap) {
        size_t next = scope->def_cap == 0 ? 4 : scope->def_cap * 2;
        scope->defs = xrealloc(scope->defs, next * sizeof(DefEntry));
        scope->def_cap = next;
    }
    scope->defs[scope->def_count].name = xstrdup(name);
    scope->defs[scope->def_count].def_node = node_clone(def_node);
    scope->def_count++;
}

static DefEntry *scope_resolve(Scope *scope, const char *name) {
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

static char *escape_html_text(const char *s) {
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
            size_t rep_len = strlen(rep);
            size_t needed = b.len + rep_len + 1;
            if (needed > b.cap) {
                size_t next = b.cap == 0 ? 32 : b.cap;
                while (next < needed) {
                    next *= 2;
                }
                b.data = xrealloc(b.data, next);
                b.cap = next;
            }
            memcpy(b.data + b.len, rep, rep_len);
            b.len += rep_len;
            b.data[b.len] = '\0';
        } else {
            size_t needed = b.len + 2;
            if (needed > b.cap) {
                size_t next = b.cap == 0 ? 32 : b.cap;
                while (next < needed) {
                    next *= 2;
                }
                b.data = xrealloc(b.data, next);
                b.cap = next;
            }
            b.data[b.len++] = c;
            b.data[b.len] = '\0';
        }
    }
    if (!b.data) {
        return xstrdup("");
    }
    return b.data;
}

static void sb_reserve(StrBuf *b, size_t needed) {
    if (needed <= b->cap) {
        return;
    }
    size_t next = b->cap == 0 ? 64 : b->cap;
    while (next < needed) {
        next *= 2;
    }
    b->data = xrealloc(b->data, next);
    b->cap = next;
}

static void sb_append_n(StrBuf *b, const char *s, size_t n) {
    sb_reserve(b, b->len + n + 1);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

static void sb_append(StrBuf *b, const char *s) {
    sb_append_n(b, s, strlen(s));
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

static void serialize_node(StrBuf *b, const Node *n) {
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

static void parser_skip_ws(Parser *p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos])) {
        p->pos++;
    }
}

static bool parser_eof(const Parser *p) {
    return p->pos >= p->len;
}

static bool parser_peek(const Parser *p, char c) {
    return !parser_eof(p) && p->src[p->pos] == c;
}

static char *parser_read_name(Parser *p) {
    size_t start = p->pos;
    if (start >= p->len) {
        return NULL;
    }
    char c = p->src[p->pos];
    if (!(isalpha((unsigned char)c) || c == '_' || c == ':')) {
        return NULL;
    }
    p->pos++;
    while (p->pos < p->len) {
        char ch = p->src[p->pos];
        if (isalnum((unsigned char)ch) || ch == '-' || ch == '_' || ch == ':' || ch == '.') {
            p->pos++;
        } else {
            break;
        }
    }
    char *name = substr_dup(p->src, start, p->pos);
    to_lower_inplace(name);
    return name;
}

static char *parser_read_attr_value(Parser *p) {
    parser_skip_ws(p);
    if (parser_eof(p)) {
        return xstrdup("");
    }
    if (p->src[p->pos] == '"' || p->src[p->pos] == '\'') {
        char quote = p->src[p->pos++];
        size_t start = p->pos;
        while (p->pos < p->len && p->src[p->pos] != quote) {
            p->pos++;
        }
        char *v = substr_dup(p->src, start, p->pos);
        if (p->pos < p->len && p->src[p->pos] == quote) {
            p->pos++;
        }
        return v;
    }
    size_t start = p->pos;
    while (p->pos < p->len) {
        char c = p->src[p->pos];
        if (isspace((unsigned char)c) || c == '>' || c == '/') {
            break;
        }
        p->pos++;
    }
    return substr_dup(p->src, start, p->pos);
}

static void parser_parse_nodes(Parser *p, Node *parent, const char *closing_tag);

static void parser_parse_comment(Parser *p, Node *parent) {
    p->pos += 4;
    size_t start = p->pos;
    size_t end = find_ci(p->src, p->len, p->pos, "-->");
    if (end == (size_t)-1) {
        char *t = substr_dup(p->src, start, p->len);
        node_add_child(parent, node_new_comment(t));
        free(t);
        p->pos = p->len;
        p->parse_errors++;
        return;
    }
    char *t = substr_dup(p->src, start, end);
    node_add_child(parent, node_new_comment(t));
    free(t);
    p->pos = end + 3;
}

static void parser_parse_decl(Parser *p, Node *parent) {
    p->pos += 2;
    size_t start = p->pos;
    while (p->pos < p->len && p->src[p->pos] != '>') {
        p->pos++;
    }
    char *t = substr_dup(p->src, start, p->pos);
    node_add_child(parent, node_new_decl(t));
    free(t);
    if (p->pos < p->len && p->src[p->pos] == '>') {
        p->pos++;
    }
}

static void parser_parse_text(Parser *p, Node *parent) {
    size_t start = p->pos;
    while (p->pos < p->len && p->src[p->pos] != '<') {
        p->pos++;
    }
    if (p->pos > start) {
        char *t = substr_dup(p->src, start, p->pos);
        node_add_child(parent, node_new_text(t));
        free(t);
    }
}

static void parser_parse_raw_text(Parser *p, Node *parent, const char *tag) {
    char closing[256];
    snprintf(closing, sizeof(closing), "</%s", tag);
    size_t end = find_ci(p->src, p->len, p->pos, closing);
    if (end == (size_t)-1) {
        char *t = substr_dup(p->src, p->pos, p->len);
        node_add_child(parent, node_new_text(t));
        free(t);
        p->pos = p->len;
        p->parse_errors++;
        return;
    }
    if (end > p->pos) {
        char *t = substr_dup(p->src, p->pos, end);
        node_add_child(parent, node_new_text(t));
        free(t);
    }
    p->pos = end;
}

static void parser_parse_start_tag(Parser *p, Node *parent) {
    p->pos++;
    char *tag = parser_read_name(p);
    if (!tag) {
        node_add_child(parent, node_new_text("<"));
        return;
    }

    Node *elem = node_new_element(tag);
    free(tag);

    bool self_closing = false;
    while (!parser_eof(p)) {
        parser_skip_ws(p);
        if (starts_with_at(p->src, p->len, p->pos, "/>")) {
            self_closing = true;
            p->pos += 2;
            break;
        }
        if (parser_peek(p, '>')) {
            p->pos++;
            break;
        }
        char *attr_name = parser_read_name(p);
        if (!attr_name) {
            p->pos++;
            continue;
        }
        parser_skip_ws(p);
        char *attr_value = xstrdup("");
        if (parser_peek(p, '=')) {
            p->pos++;
            free(attr_value);
            attr_value = parser_read_attr_value(p);
        }
        node_add_attr(elem, attr_name, attr_value);
        free(attr_name);
        free(attr_value);
    }

    node_add_child(parent, elem);

    if (self_closing || is_void_tag(elem->tag)) {
        return;
    }

    if (str_eq(elem->tag, "script") || str_eq(elem->tag, "style")) {
        parser_parse_raw_text(p, elem, elem->tag);
    }

    parser_parse_nodes(p, elem, elem->tag);
}

static void parser_parse_close_tag(Parser *p, char **name_out) {
    *name_out = NULL;
    if (!starts_with_at(p->src, p->len, p->pos, "</")) {
        return;
    }
    p->pos += 2;
    parser_skip_ws(p);
    *name_out = parser_read_name(p);
    parser_skip_ws(p);
    while (p->pos < p->len && p->src[p->pos] != '>') {
        p->pos++;
    }
    if (p->pos < p->len && p->src[p->pos] == '>') {
        p->pos++;
    }
}

static void parser_parse_nodes(Parser *p, Node *parent, const char *closing_tag) {
    while (!parser_eof(p)) {
        if (closing_tag && starts_with_at(p->src, p->len, p->pos, "</")) {
            size_t save = p->pos;
            char *end_name = NULL;
            parser_parse_close_tag(p, &end_name);
            bool matched = end_name && str_eq(end_name, closing_tag);
            free(end_name);
            if (matched) {
                return;
            }
            p->pos = save;
            node_add_child(parent, node_new_text("<"));
            p->pos++;
            continue;
        }

        if (starts_with_at(p->src, p->len, p->pos, "<!--")) {
            parser_parse_comment(p, parent);
        } else if (starts_with_at(p->src, p->len, p->pos, "<!")) {
            parser_parse_decl(p, parent);
        } else if (starts_with_at(p->src, p->len, p->pos, "<")) {
            if (starts_with_at(p->src, p->len, p->pos, "</")) {
                char *end_name = NULL;
                parser_parse_close_tag(p, &end_name);
                free(end_name);
            } else {
                parser_parse_start_tag(p, parent);
            }
        } else {
            parser_parse_text(p, parent);
        }
    }
}

static Node *parse_html(const char *src, BuildCtx *ctx) {
    Parser p;
    p.src = src;
    p.len = strlen(src);
    p.pos = 0;
    p.parse_errors = 0;

    Node *doc = node_new_document();
    parser_parse_nodes(&p, doc, NULL);

    if (p.parse_errors > 0) {
        log_warning(ctx, "parser recovered from %d malformed HTML region(s)", p.parse_errors);
    }
    return doc;
}

static void collect_defs_for_scope(Node *scope_root, Scope *scope, BuildCtx *ctx) {
    for (size_t i = 0; i < scope_root->child_count; i++) {
        Node *child = scope_root->children[i];
        if (child->type != NODE_ELEMENT || !is_def_tag(child->tag)) {
            continue;
        }
        const char *symbol = child->tag + 4;
        if (!is_valid_symbol(symbol)) {
            log_error(ctx, "invalid component definition tag <%s>", child->tag);
            continue;
        }
        if (scope_find_local_def(scope, symbol)) {
            log_error(ctx, "duplicate component definition for symbol '%s' in same scope", symbol);
            continue;
        }
        scope_add_def(scope, symbol, child);
    }
}

static bool should_expand_component(Node *node, Scope *scope, DefEntry **entry_out, BuildCtx *ctx) {
    if (node->type != NODE_ELEMENT) {
        return false;
    }
    if (is_def_tag(node->tag) || str_eq(node->tag, "prop") || str_eq(node->tag, "slot")) {
        return false;
    }
    if (is_native_tag(node->tag)) {
        return false;
    }
    DefEntry *resolved = scope_resolve(scope, node->tag);
    if (resolved) {
        *entry_out = resolved;
        return true;
    }
    log_warning(ctx, "unknown invocation symbol <%s>; leaving unchanged", node->tag);
    return false;
}

static NodeList clone_nodelist(const NodeList *src) {
    NodeList out = {0};
    for (size_t i = 0; i < src->count; i++) {
        nodelist_push(&out, node_clone(src->items[i]));
    }
    return out;
}

static NodeList *slot_lookup_payload(SlotPayload *payload, const char *name) {
    if (!name || !name[0]) {
        return &payload->default_nodes;
    }
    for (size_t i = 0; i < payload->named_count; i++) {
        if (str_eq(payload->named[i].name, name)) {
            payload->named[i].used = true;
            return &payload->named[i].nodes;
        }
    }
    return NULL;
}

static void substitute_props_slots(Node *node, const Node *invocation, SlotPayload *payload, BuildCtx *ctx) {
    size_t i = 0;
    while (i < node->child_count) {
        Node *child = node->children[i];
        if (child->type != NODE_ELEMENT) {
            i++;
            continue;
        }

        if (str_eq(child->tag, "prop")) {
            const char *name = node_get_attr(child, "name");
            const char *fallback = node_get_attr(child, "default");
            const char *value = NULL;
            if (!name || !name[0]) {
                log_error(ctx, "<prop> missing required name attribute");
                value = "";
            } else {
                value = node_get_attr(invocation, name);
                if (!value) {
                    value = fallback ? fallback : "";
                    if (!fallback) {
                        log_warning(ctx, "missing prop '%s' on <%s>", name, invocation->tag);
                    }
                }
            }
            char *escaped = escape_html_text(value);
            Node *text_node = node_new_text(escaped);
            free(escaped);
            Node *repl[1] = {text_node};
            node_replace_child(node, i, repl, 1);
            i++;
            continue;
        }

        if (str_eq(child->tag, "slot")) {
            const char *slot_name = node_get_attr(child, "name");
            NodeList *src = slot_lookup_payload(payload, slot_name);
            if (!src || src->count == 0) {
                node_replace_child(node, i, NULL, 0);
                continue;
            }
            NodeList clones = clone_nodelist(src);
            node_replace_child(node, i, clones.items, clones.count);
            free(clones.items);
            i += src->count;
            continue;
        }

        substitute_props_slots(child, invocation, payload, ctx);
        i++;
    }
}

static void collect_slot_payload(const Node *invocation, SlotPayload *payload) {
    for (size_t i = 0; i < invocation->child_count; i++) {
        Node *child = invocation->children[i];
        Node *clone = node_clone(child);
        if (clone->type == NODE_ELEMENT) {
            const char *slot_name = node_get_attr(clone, "slot");
            if (slot_name && slot_name[0] != '\0') {
                char *slot_name_copy = xstrdup(slot_name);
                node_remove_attr(clone, "slot");
                NamedSlot *named = slotpayload_get_named(payload, slot_name_copy);
                free(slot_name_copy);
                nodelist_push(&named->nodes, clone);
                continue;
            }
        }
        nodelist_push(&payload->default_nodes, clone);
    }
}

static Node *make_synthetic_root_from_def(const Node *def_node) {
    Node *root = node_new_document();
    for (size_t i = 0; i < def_node->child_count; i++) {
        node_add_child(root, node_clone(def_node->children[i]));
    }
    return root;
}

static void process_scope(Node *scope_root, Scope *parent_scope, BuildCtx *ctx, StringStack *stack, int expansion_depth);

static bool expand_component(Node *invocation,
                             const DefEntry *resolved_def,
                             Scope *caller_scope,
                             BuildCtx *ctx,
                             StringStack *stack,
                             int expansion_depth,
                             Node ***out_nodes,
                             size_t *out_count) {
    *out_nodes = NULL;
    *out_count = 0;

    if (expansion_depth >= MAX_EXPANSION_DEPTH) {
        log_error(ctx, "max expansion depth (%d) exceeded while expanding <%s>", MAX_EXPANSION_DEPTH, invocation->tag);
        return false;
    }

    if (strstack_contains(stack, invocation->tag)) {
        log_error(ctx, "recursive component cycle detected at <%s>", invocation->tag);
        return false;
    }

    SlotPayload payload = {0};
    collect_slot_payload(invocation, &payload);

    Node *synthetic = make_synthetic_root_from_def(resolved_def->def_node);
    substitute_props_slots(synthetic, invocation, &payload, ctx);

    for (size_t i = 0; i < payload.named_count; i++) {
        if (!payload.named[i].used && payload.named[i].nodes.count > 0) {
            log_warning(ctx, "unknown named slot '%s' provided to <%s>", payload.named[i].name, invocation->tag);
        }
    }

    strstack_push(stack, invocation->tag);
    process_scope(synthetic, caller_scope, ctx, stack, expansion_depth + 1);
    strstack_pop(stack);

    *out_count = synthetic->child_count;
    if (*out_count > 0) {
        *out_nodes = synthetic->children;
        for (size_t i = 0; i < *out_count; i++) {
            (*out_nodes)[i]->parent = NULL;
        }
        synthetic->children = NULL;
        synthetic->child_count = 0;
        synthetic->child_cap = 0;
    }

    node_free(synthetic);
    slotpayload_free(&payload);
    return true;
}

static void process_scope(Node *scope_root, Scope *parent_scope, BuildCtx *ctx, StringStack *stack, int expansion_depth) {
    Scope local;
    scope_init(&local, parent_scope);
    collect_defs_for_scope(scope_root, &local, ctx);

    size_t i = 0;
    while (i < scope_root->child_count) {
        Node *child = scope_root->children[i];

        if (child->type != NODE_ELEMENT) {
            i++;
            continue;
        }

        if (is_def_tag(child->tag)) {
            node_replace_child(scope_root, i, NULL, 0);
            continue;
        }

        DefEntry *resolved = NULL;
        if (should_expand_component(child, &local, &resolved, ctx)) {
            Node **expanded_nodes = NULL;
            size_t expanded_count = 0;
            bool ok = expand_component(child, resolved, &local, ctx, stack, expansion_depth, &expanded_nodes, &expanded_count);
            if (ok) {
                node_replace_child(scope_root, i, expanded_nodes, expanded_count);
                free(expanded_nodes);
                i += expanded_count;
                continue;
            }
        }

        process_scope(child, &local, ctx, stack, expansion_depth);
        i++;
    }

    scope_free(&local);
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char *buf = xmalloc((size_t)size + 1);
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) {
        free(buf);
        return NULL;
    }
    buf[size] = '\0';
    return buf;
}

static bool write_file(const char *path, const char *data) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    size_t n = strlen(data);
    bool ok = fwrite(data, 1, n, f) == n;
    fclose(f);
    return ok;
}

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        errno = ENOTDIR;
        return -1;
    }
    if (mkdir(path, 0755) == 0) {
        return 0;
    }
    if (errno == EEXIST) {
        return 0;
    }
    return -1;
}

static bool has_html_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) {
        return false;
    }
    return str_eq(dot, ".html") || str_eq(dot, ".htm");
}

static bool copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        return false;
    }
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    char buf[8192];
    bool ok = true;
    while (!feof(in)) {
        size_t n = fread(buf, 1, sizeof(buf), in);
        if (ferror(in)) {
            ok = false;
            break;
        }
        if (n > 0 && fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    fclose(in);
    fclose(out);
    return ok;
}

static bool process_html_file(const char *input_path, const char *output_path, BuildCtx *ctx) {
    char *input = read_file(input_path);
    if (!input) {
        log_error(ctx, "failed to read %s", input_path);
        return false;
    }

    const char *prev_file = ctx->current_file;
    ctx->current_file = input_path;

    Node *doc = parse_html(input, ctx);
    free(input);

    StringStack stack = {0};
    process_scope(doc, NULL, ctx, &stack, 0);
    strstack_free(&stack);

    StrBuf out = {0};
    serialize_node(&out, doc);
    node_free(doc);

    bool ok = write_file(output_path, out.data ? out.data : "");
    if (!ok) {
        log_error(ctx, "failed to write %s", output_path);
    }

    free(out.data);
    ctx->current_file = prev_file;
    return ok;
}

static void process_directory(const char *src, const char *dst, BuildCtx *ctx) {
    if (ensure_dir(dst) != 0) {
        log_error(ctx, "failed to create directory %s: %s", dst, strerror(errno));
        return;
    }

    DIR *dir = opendir(src);
    if (!dir) {
        log_error(ctx, "failed to open directory %s: %s", src, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (str_eq(name, ".") || str_eq(name, "..")) {
            continue;
        }

        char src_path[MAX_PATH_LEN];
        char dst_path[MAX_PATH_LEN];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, name);

        struct stat st;
        if (stat(src_path, &st) != 0) {
            log_error(ctx, "stat failed for %s: %s", src_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            process_directory(src_path, dst_path, ctx);
            continue;
        }

        bool ok = false;
        if (has_html_ext(src_path)) {
            ok = process_html_file(src_path, dst_path, ctx);
        } else {
            ok = copy_file(src_path, dst_path);
            if (!ok) {
                log_error(ctx, "failed to copy %s to %s", src_path, dst_path);
            }
        }

        if (ok) {
            printf("Processed: %s -> %s\n", src_path, dst_path);
        }
    }

    closedir(dir);
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <input_dir> <output_dir>\n", prog);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 2;
    }

    BuildCtx ctx;
    ctx.error_count = 0;
    ctx.warning_count = 0;
    ctx.current_file = NULL;

    process_directory(argv[1], argv[2], &ctx);

    if (ctx.error_count > 0) {
        fprintf(stderr, "Build failed with %d error(s), %d warning(s).\n", ctx.error_count, ctx.warning_count);
        return 1;
    }

    fprintf(stderr, "Build complete with %d warning(s).\n", ctx.warning_count);
    return 0;
}
