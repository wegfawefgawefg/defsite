#include "common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
    int parse_errors;
} Parser;

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

Node *parse_html(const char *src, BuildCtx *ctx) {
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
