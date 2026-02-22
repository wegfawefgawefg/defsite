#include "common.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *next = realloc(ptr, size);
    if (!next) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    return next;
}

char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *out = xmalloc(n + 1);
    memcpy(out, s, n + 1);
    return out;
}

char *substr_dup(const char *s, size_t start, size_t end) {
    if (end < start) {
        end = start;
    }
    size_t n = end - start;
    char *out = xmalloc(n + 1);
    memcpy(out, s + start, n);
    out[n] = '\0';
    return out;
}

void to_lower_inplace(char *s) {
    for (; *s; s++) {
        *s = (char)tolower((unsigned char)*s);
    }
}

bool str_eq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

bool starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

bool starts_with_at(const char *s, size_t len, size_t pos, const char *prefix) {
    size_t p_len = strlen(prefix);
    if (pos + p_len > len) {
        return false;
    }
    return strncmp(s + pos, prefix, p_len) == 0;
}

size_t find_ci(const char *haystack, size_t len, size_t start, const char *needle) {
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

void log_error(BuildCtx *ctx, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_msg(ctx, "ERROR: ", fmt, ap);
    va_end(ap);
    ctx->error_count++;
}

void log_warning(BuildCtx *ctx, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_msg(ctx, "WARN: ", fmt, ap);
    va_end(ap);
    ctx->warning_count++;
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

void sb_append_n(StrBuf *b, const char *s, size_t n) {
    sb_reserve(b, b->len + n + 1);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void sb_append(StrBuf *b, const char *s) {
    sb_append_n(b, s, strlen(s));
}

char *read_file(const char *path) {
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

bool write_file(const char *path, const char *data) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    size_t n = strlen(data);
    bool ok = fwrite(data, 1, n, f) == n;
    fclose(f);
    return ok;
}

int ensure_dir(const char *path) {
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

bool has_html_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) {
        return false;
    }
    return str_eq(dot, ".html") || str_eq(dot, ".htm");
}

bool copy_file(const char *src, const char *dst) {
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
