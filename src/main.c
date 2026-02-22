#include "templater/common.h"

#include <stdio.h>

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <input_dir> <output_dir>\n", prog);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 2;
    }

    const char *src_dir = argv[1];
    const char *out_dir = argv[2];

    BuildCtx ctx;
    ctx.error_count = 0;
    ctx.warning_count = 0;
    ctx.current_file = NULL;

    process_directory(src_dir, out_dir, &ctx);

    char index_path[MAX_PATH_LEN];
    snprintf(index_path, sizeof(index_path), "%s/search-index.json", out_dir);
    generate_recipe_index(src_dir, index_path, &ctx);

    if (ctx.error_count > 0) {
        fprintf(stderr, "Build failed with %d error(s), %d warning(s).\n", ctx.error_count, ctx.warning_count);
        return 1;
    }

    fprintf(stderr, "Build complete with %d warning(s).\n", ctx.warning_count);
    return 0;
}
