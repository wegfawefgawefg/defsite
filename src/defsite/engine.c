#include "common.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

bool process_html_file(const char *input_path, const char *output_path, BuildCtx *ctx) {
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

void process_directory(const char *src, const char *dst, BuildCtx *ctx) {
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
