#include "common.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char *slug;
    char *url;
    char *title;
    char *summary;
    int time_min;
    bool has_time_min;
    char *serves;
    char *difficulty;
    char **diets;
    size_t diet_count;
    size_t diet_cap;
    char *method;
    char *published;
} RecipeRecord;

typedef struct {
    RecipeRecord *items;
    size_t count;
    size_t cap;
} RecipeList;

static void recipe_record_free(RecipeRecord *r) {
    if (!r) {
        return;
    }
    free(r->slug);
    free(r->url);
    free(r->title);
    free(r->summary);
    free(r->serves);
    free(r->difficulty);
    for (size_t i = 0; i < r->diet_count; i++) {
        free(r->diets[i]);
    }
    free(r->diets);
    free(r->method);
    free(r->published);
}

static void recipe_list_free(RecipeList *list) {
    for (size_t i = 0; i < list->count; i++) {
        recipe_record_free(&list->items[i]);
    }
    free(list->items);
}

static RecipeRecord *recipe_list_push(RecipeList *list) {
    if (list->count == list->cap) {
        size_t next = list->cap == 0 ? 8 : list->cap * 2;
        list->items = xrealloc(list->items, next * sizeof(RecipeRecord));
        list->cap = next;
    }
    RecipeRecord *rec = &list->items[list->count++];
    memset(rec, 0, sizeof(*rec));
    return rec;
}

static void recipe_add_diet(RecipeRecord *rec, const char *diet) {
    if (!diet || !diet[0]) {
        return;
    }
    if (rec->diet_count == rec->diet_cap) {
        size_t next = rec->diet_cap == 0 ? 4 : rec->diet_cap * 2;
        rec->diets = xrealloc(rec->diets, next * sizeof(char *));
        rec->diet_cap = next;
    }
    rec->diets[rec->diet_count++] = xstrdup(diet);
}

static char *trimmed_copy(const char *s) {
    if (!s) {
        return xstrdup("");
    }
    const char *start = s;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
        start++;
    }
    const char *end = s + strlen(s);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) {
        end--;
    }
    return substr_dup(start, 0, (size_t)(end - start));
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

static const char *attr_or_empty(Node *node, const char *name) {
    const char *v = node_get_attr(node, name);
    return v ? v : "";
}

static void split_diets(RecipeRecord *rec, const char *diets_csv) {
    if (!diets_csv || !diets_csv[0]) {
        return;
    }

    const char *cursor = diets_csv;
    while (*cursor) {
        const char *comma = strchr(cursor, ',');
        if (!comma) {
            comma = cursor + strlen(cursor);
        }
        char *part = substr_dup(cursor, 0, (size_t)(comma - cursor));
        char *trimmed = trimmed_copy(part);
        recipe_add_diet(rec, trimmed);
        free(part);
        free(trimmed);

        if (*comma == ',') {
            cursor = comma + 1;
        } else {
            break;
        }
    }
}

static void parse_time_min(RecipeRecord *rec, const char *time_raw, BuildCtx *ctx, const char *rel_path) {
    if (!time_raw || !time_raw[0]) {
        return;
    }
    char *end = NULL;
    long v = strtol(time_raw, &end, 10);
    if (end && *end == '\0' && v >= 0 && v <= 1000000) {
        rec->has_time_min = true;
        rec->time_min = (int)v;
    } else {
        log_warning(ctx, "recipe metadata invalid data-time-min in %s", rel_path);
    }
}

static void warn_required(BuildCtx *ctx, const char *rel_path, const char *field, const char *value) {
    if (!value || !value[0]) {
        log_warning(ctx, "recipe metadata missing %s in %s", field, rel_path);
    }
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

static void collect_recipe_from_file(const char *src_dir, const char *file_path, RecipeList *list, BuildCtx *ctx) {
    char *content = read_file(file_path);
    if (!content) {
        log_warning(ctx, "failed to read %s while building recipe index", file_path);
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

    const char *kind = node_get_attr(html, "data-kind");
    if (!kind || !str_eq(kind, "recipe")) {
        node_free(doc);
        ctx->current_file = prev_file;
        return;
    }

    char *rel = path_relative_to(file_path, src_dir);

    RecipeRecord *rec = recipe_list_push(list);
    rec->slug = xstrdup(attr_or_empty(html, "data-slug"));
    rec->url = rel;
    rec->title = xstrdup(attr_or_empty(html, "data-title"));
    rec->summary = xstrdup(attr_or_empty(html, "data-summary"));
    rec->serves = xstrdup(attr_or_empty(html, "data-serves"));
    rec->difficulty = xstrdup(attr_or_empty(html, "data-difficulty"));
    rec->method = xstrdup(attr_or_empty(html, "data-method"));
    rec->published = xstrdup(attr_or_empty(html, "data-published"));

    split_diets(rec, attr_or_empty(html, "data-diets"));
    parse_time_min(rec, attr_or_empty(html, "data-time-min"), ctx, rel);

    warn_required(ctx, rel, "data-slug", rec->slug);
    warn_required(ctx, rel, "data-title", rec->title);
    warn_required(ctx, rel, "data-summary", rec->summary);
    warn_required(ctx, rel, "data-time-min", attr_or_empty(html, "data-time-min"));
    warn_required(ctx, rel, "data-published", rec->published);
    if (rec->published[0] && !is_date_format(rec->published)) {
        log_warning(ctx, "recipe metadata invalid data-published format in %s (expected YYYY-MM-DD)", rel);
    }

    node_free(doc);
    ctx->current_file = prev_file;
}

static void scan_dir_recursive(const char *src_dir, const char *dir_path, RecipeList *list, BuildCtx *ctx) {
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
            collect_recipe_from_file(src_dir, full, list, ctx);
        }
    }

    closedir(dir);
}

static int recipe_cmp(const void *a, const void *b) {
    const RecipeRecord *ra = (const RecipeRecord *)a;
    const RecipeRecord *rb = (const RecipeRecord *)b;
    int p = strcmp(rb->published ? rb->published : "", ra->published ? ra->published : "");
    if (p != 0) {
        return p;
    }
    return strcmp(ra->title ? ra->title : "", rb->title ? rb->title : "");
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

static void serialize_recipe_json(StrBuf *b, const RecipeRecord *r) {
    sb_append(b, "  {\n");

    sb_append(b, "    \"slug\": "); json_append_escaped(b, r->slug); sb_append(b, ",\n");
    sb_append(b, "    \"url\": "); json_append_escaped(b, r->url); sb_append(b, ",\n");
    sb_append(b, "    \"title\": "); json_append_escaped(b, r->title); sb_append(b, ",\n");
    sb_append(b, "    \"summary\": "); json_append_escaped(b, r->summary); sb_append(b, ",\n");
    sb_append(b, "    \"time_min\": ");
    if (r->has_time_min) {
        char num[32];
        snprintf(num, sizeof(num), "%d", r->time_min);
        sb_append(b, num);
    } else {
        sb_append(b, "null");
    }
    sb_append(b, ",\n");

    sb_append(b, "    \"serves\": "); json_append_escaped(b, r->serves); sb_append(b, ",\n");
    sb_append(b, "    \"difficulty\": "); json_append_escaped(b, r->difficulty); sb_append(b, ",\n");

    sb_append(b, "    \"diets\": [");
    for (size_t i = 0; i < r->diet_count; i++) {
        json_append_escaped(b, r->diets[i]);
        if (i + 1 < r->diet_count) {
            sb_append(b, ", ");
        }
    }
    sb_append(b, "],\n");

    sb_append(b, "    \"method\": "); json_append_escaped(b, r->method); sb_append(b, ",\n");
    sb_append(b, "    \"published\": "); json_append_escaped(b, r->published); sb_append(b, "\n");

    sb_append(b, "  }");
}

static void warn_duplicate_slugs(const RecipeList *list, BuildCtx *ctx) {
    for (size_t i = 0; i < list->count; i++) {
        if (!list->items[i].slug || !list->items[i].slug[0]) {
            continue;
        }
        for (size_t j = i + 1; j < list->count; j++) {
            if (str_eq(list->items[i].slug, list->items[j].slug)) {
                log_warning(ctx, "duplicate recipe data-slug '%s'", list->items[i].slug);
            }
        }
    }
}

void generate_recipe_index(const char *src_dir, const char *out_json_path, BuildCtx *ctx) {
    RecipeList list = {0};
    scan_dir_recursive(src_dir, src_dir, &list, ctx);

    if (list.count == 0) {
        unlink(out_json_path);
        recipe_list_free(&list);
        return;
    }

    warn_duplicate_slugs(&list, ctx);
    qsort(list.items, list.count, sizeof(RecipeRecord), recipe_cmp);

    StrBuf out = {0};
    sb_append(&out, "[\n");
    for (size_t i = 0; i < list.count; i++) {
        serialize_recipe_json(&out, &list.items[i]);
        if (i + 1 < list.count) {
            sb_append(&out, ",");
        }
        sb_append(&out, "\n");
    }
    sb_append(&out, "]\n");

    if (!write_file(out_json_path, out.data ? out.data : "[]\n")) {
        log_error(ctx, "failed to write %s", out_json_path);
    } else {
        fprintf(stderr, "Generated recipe discovery index: %s (%zu items)\n", out_json_path, list.count);
    }

    free(out.data);
    recipe_list_free(&list);
}
