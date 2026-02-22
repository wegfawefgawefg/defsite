#include "common.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char *key;
    char *value;
} MetaField;

typedef struct {
    char *url;
    MetaField *meta;
    size_t meta_count;
    size_t meta_cap;
} DiscoveryRecord;

typedef struct {
    DiscoveryRecord *items;
    size_t count;
    size_t cap;
} DiscoveryList;

static void record_free(DiscoveryRecord *r) {
    if (!r) {
        return;
    }
    free(r->url);
    for (size_t i = 0; i < r->meta_count; i++) {
        free(r->meta[i].key);
        free(r->meta[i].value);
    }
    free(r->meta);
}

static void list_free(DiscoveryList *list) {
    for (size_t i = 0; i < list->count; i++) {
        record_free(&list->items[i]);
    }
    free(list->items);
}

static DiscoveryRecord *list_push(DiscoveryList *list) {
    if (list->count == list->cap) {
        size_t next = list->cap == 0 ? 8 : list->cap * 2;
        list->items = xrealloc(list->items, next * sizeof(DiscoveryRecord));
        list->cap = next;
    }
    DiscoveryRecord *rec = &list->items[list->count++];
    memset(rec, 0, sizeof(*rec));
    return rec;
}

static bool path_starts_with(const char *full, const char *prefix) {
    size_t n = strlen(prefix);
    return strncmp(full, prefix, n) == 0;
}

static char *path_relative_to(const char *full, const char *base) {
    if (path_starts_with(full, base)) {
        const char *p = full + strlen(base);
        if (*p == '/') {
            p++;
        }
        return xstrdup(p);
    }
    return xstrdup(full);
}

static bool is_date_format(const char *s) {
    if (!s || strlen(s) != 10) {
        return false;
    }
    return (s[4] == '-' && s[7] == '-')
           && (s[0] >= '0' && s[0] <= '9')
           && (s[1] >= '0' && s[1] <= '9')
           && (s[2] >= '0' && s[2] <= '9')
           && (s[3] >= '0' && s[3] <= '9')
           && (s[5] >= '0' && s[5] <= '9')
           && (s[6] >= '0' && s[6] <= '9')
           && (s[8] >= '0' && s[8] <= '9')
           && (s[9] >= '0' && s[9] <= '9');
}

static Node *find_html_node(Node *node) {
    if (!node) {
        return NULL;
    }
    if (node->type == NODE_ELEMENT && node->tag && str_eq(node->tag, "html")) {
        return node;
    }
    for (size_t i = 0; i < node->child_count; i++) {
        Node *found = find_html_node(node->children[i]);
        if (found) {
            return found;
        }
    }
    return NULL;
}

static const char *record_meta_get(const DiscoveryRecord *rec, const char *key) {
    for (size_t i = 0; i < rec->meta_count; i++) {
        if (str_eq(rec->meta[i].key, key)) {
            return rec->meta[i].value;
        }
    }
    return "";
}

static void record_meta_set(DiscoveryRecord *rec, const char *key, const char *value) {
    for (size_t i = 0; i < rec->meta_count; i++) {
        if (str_eq(rec->meta[i].key, key)) {
            free(rec->meta[i].value);
            rec->meta[i].value = xstrdup(value ? value : "");
            return;
        }
    }

    if (rec->meta_count == rec->meta_cap) {
        size_t next = rec->meta_cap == 0 ? 8 : rec->meta_cap * 2;
        rec->meta = xrealloc(rec->meta, next * sizeof(MetaField));
        rec->meta_cap = next;
    }

    rec->meta[rec->meta_count].key = xstrdup(key);
    rec->meta[rec->meta_count].value = xstrdup(value ? value : "");
    rec->meta_count++;
}

static size_t collect_meta_from_html_attrs(DiscoveryRecord *rec, const Node *html) {
    size_t count = 0;
    for (size_t i = 0; i < html->attr_count; i++) {
        const Attr *a = &html->attrs[i];
        if (!a->name || !starts_with(a->name, "data-")) {
            continue;
        }

        const char *key = a->name + 5;
        if (!key[0]) {
            continue;
        }

        record_meta_set(rec, key, a->value ? a->value : "");
        count++;
    }
    return count;
}

static void collect_entry_from_file(const char *src_dir, const char *file_path, DiscoveryList *list, BuildCtx *ctx) {
    char *content = read_file(file_path);
    if (!content) {
        log_warning(ctx, "failed to read %s while building discovery index", file_path);
        return;
    }

    const char *prev_file = ctx->current_file;
    ctx->current_file = file_path;
    Node *doc = parse_html(content, ctx);
    free(content);

    Node *html = find_html_node(doc);
    if (!html) {
        node_free(doc);
        ctx->current_file = prev_file;
        return;
    }

    char *rel = path_relative_to(file_path, src_dir);
    DiscoveryRecord rec = {0};
    rec.url = rel;
    size_t meta_count = collect_meta_from_html_attrs(&rec, html);
    if (meta_count == 0) {
        record_free(&rec);
        node_free(doc);
        ctx->current_file = prev_file;
        return;
    }

    const char *published = record_meta_get(&rec, "published");
    if (published[0] && !is_date_format(published)) {
        log_warning(ctx, "metadata invalid data-published format in %s (expected YYYY-MM-DD)", rel);
    }

    DiscoveryRecord *dst = list_push(list);
    *dst = rec;

    node_free(doc);
    ctx->current_file = prev_file;
}

static void scan_dir_recursive(const char *src_dir, const char *dir_path, DiscoveryList *list, BuildCtx *ctx) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (str_eq(entry->d_name, ".") || str_eq(entry->d_name, "..")) {
            continue;
        }
        char full[MAX_PATH_LEN];
        snprintf(full, sizeof(full), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            scan_dir_recursive(src_dir, full, list, ctx);
            continue;
        }

        if (has_html_ext(full)) {
            collect_entry_from_file(src_dir, full, list, ctx);
        }
    }

    closedir(dir);
}

static int record_cmp(const void *a, const void *b) {
    const DiscoveryRecord *ra = (const DiscoveryRecord *)a;
    const DiscoveryRecord *rb = (const DiscoveryRecord *)b;
    return strcmp(ra->url ? ra->url : "", rb->url ? rb->url : "");
}

static void json_append_escaped(StrBuf *b, const char *s) {
    sb_append(b, "\"");
    for (const char *p = s ? s : ""; *p; p++) {
        switch (*p) {
        case '\\': sb_append(b, "\\\\"); break;
        case '"': sb_append(b, "\\\""); break;
        case '\n': sb_append(b, "\\n"); break;
        case '\r': sb_append(b, "\\r"); break;
        case '\t': sb_append(b, "\\t"); break;
        default: sb_append_n(b, p, 1); break;
        }
    }
    sb_append(b, "\"");
}

static void json_append_meta(StrBuf *b, const DiscoveryRecord *r) {
    sb_append(b, "{");
    if (r->meta_count > 0) {
        sb_append(b, "\n");
    }

    for (size_t i = 0; i < r->meta_count; i++) {
        sb_append(b, "      ");
        json_append_escaped(b, r->meta[i].key);
        sb_append(b, ": ");
        json_append_escaped(b, r->meta[i].value);
        if (i + 1 < r->meta_count) {
            sb_append(b, ",");
        }
        sb_append(b, "\n");
    }

    if (r->meta_count > 0) {
        sb_append(b, "    ");
    }
    sb_append(b, "}");
}

static void serialize_record_json(StrBuf *b, const DiscoveryRecord *r) {
    sb_append(b, "  {\n");
    sb_append(b, "    \"url\": ");
    json_append_escaped(b, r->url ? r->url : "");
    sb_append(b, ",\n");
    sb_append(b, "    \"meta\": ");
    json_append_meta(b, r);
    sb_append(b, "\n");
    sb_append(b, "  }");
}

static void warn_duplicate_slugs(const DiscoveryList *list, BuildCtx *ctx) {
    for (size_t i = 0; i < list->count; i++) {
        const char *slug_i = record_meta_get(&list->items[i], "slug");

        if (!slug_i[0]) {
            continue;
        }

        for (size_t j = i + 1; j < list->count; j++) {
            const char *slug_j = record_meta_get(&list->items[j], "slug");

            if (str_eq(slug_i, slug_j)) {
                log_warning(ctx, "duplicate metadata slug '%s' in discovery index", slug_i);
            }
        }
    }
}

void generate_discovery_index(const char *src_dir, const char *out_json_path, BuildCtx *ctx) {
    DiscoveryList list = {0};
    scan_dir_recursive(src_dir, src_dir, &list, ctx);

    if (list.count == 0) {
        unlink(out_json_path);
        list_free(&list);
        return;
    }

    warn_duplicate_slugs(&list, ctx);
    qsort(list.items, list.count, sizeof(DiscoveryRecord), record_cmp);

    StrBuf out = {0};
    sb_append(&out, "[\n");
    for (size_t i = 0; i < list.count; i++) {
        serialize_record_json(&out, &list.items[i]);
        if (i + 1 < list.count) {
            sb_append(&out, ",");
        }
        sb_append(&out, "\n");
    }
    sb_append(&out, "]\n");

    if (!write_file(out_json_path, out.data ? out.data : "[]\n")) {
        log_error(ctx, "failed to write %s", out_json_path);
    } else {
        fprintf(stderr, "Generated discovery index: %s (%zu items)\n", out_json_path, list.count);
    }

    free(out.data);
    list_free(&list);
}
