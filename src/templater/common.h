#ifndef TEMPLATER_COMMON_H
#define TEMPLATER_COMMON_H

#include <stdbool.h>
#include <stddef.h>

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

/* util.c */
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
char *substr_dup(const char *s, size_t start, size_t end);
void to_lower_inplace(char *s);
bool str_eq(const char *a, const char *b);
bool starts_with(const char *s, const char *prefix);
bool starts_with_at(const char *s, size_t len, size_t pos, const char *prefix);
size_t find_ci(const char *haystack, size_t len, size_t start, const char *needle);

void log_error(BuildCtx *ctx, const char *fmt, ...);
void log_warning(BuildCtx *ctx, const char *fmt, ...);

void sb_append_n(StrBuf *b, const char *s, size_t n);
void sb_append(StrBuf *b, const char *s);

char *read_file(const char *path);
bool write_file(const char *path, const char *data);
int ensure_dir(const char *path);
bool has_html_ext(const char *path);
bool copy_file(const char *src, const char *dst);

/* dom.c */
Node *node_new_document(void);
Node *node_new_element(const char *tag);
Node *node_new_text(const char *text);
Node *node_new_comment(const char *text);
Node *node_new_decl(const char *text);

void node_add_attr(Node *n, const char *name, const char *value);
const char *node_get_attr(const Node *n, const char *name);
void node_remove_attr(Node *n, const char *name);

void node_add_child(Node *parent, Node *child);
void node_free(Node *n);
Node *node_clone(const Node *src);
void node_replace_child(Node *parent, size_t idx, Node **new_nodes, size_t new_count);

bool is_void_tag(const char *tag);
bool is_native_tag(const char *tag);
bool is_def_tag(const char *tag);
bool is_valid_symbol(const char *name);

void strstack_push(StringStack *s, const char *item);
void strstack_pop(StringStack *s);
bool strstack_contains(const StringStack *s, const char *item);
void strstack_free(StringStack *s);

void nodelist_push(NodeList *list, Node *node);
void nodelist_free(NodeList *list);
NamedSlot *slotpayload_get_named(SlotPayload *payload, const char *name);
void slotpayload_free(SlotPayload *payload);

void scope_init(Scope *scope, Scope *parent);
void scope_free(Scope *scope);
DefEntry *scope_find_local_def(Scope *scope, const char *name);
void scope_add_def(Scope *scope, const char *name, Node *def_node);
DefEntry *scope_resolve(Scope *scope, const char *name);

char *escape_html_text(const char *s);
void serialize_node(StrBuf *b, const Node *n);

/* parser.c */
Node *parse_html(const char *src, BuildCtx *ctx);

/* engine.c */
bool process_html_file(const char *input_path, const char *output_path, BuildCtx *ctx);
void process_directory(const char *src, const char *dst, BuildCtx *ctx);

/* index.c */
void generate_recipe_index(const char *src_dir, const char *out_json_path, BuildCtx *ctx);

#endif
