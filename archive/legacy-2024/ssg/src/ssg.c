/*
    This is a single file static site generator written in C by Claude Opus 3.5.
    It goes through the src directory, and does 2 things.
    1. Copies all non-html files to the generated directory. (makes things easy for the http server)
    2. Processes all html files, and replaces a template tag with the contents of the file.

    The template tag is <!-- template: filename.html -->.
    The filename.html should be in the same directory as the html file that references it.

    Recursion is not supported. Templates only work one level deep.

    There are no dependencies. Compile with gcc.
        gcc static_site_generator.c -o generate

    To run, execute the generated binary. There are no arguments.
        ./generate
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <regex.h>
#include <fcntl.h>

#define MAX_PATH 1024
#define MAX_LINE 4096
#define MAX_ERRORS 100
#define BUFFER_SIZE 4096

char errors[MAX_ERRORS][MAX_PATH];
int error_count = 0;

void add_error(const char *error)
{
    if (error_count < MAX_ERRORS)
    {
        strncpy(errors[error_count], error, MAX_PATH - 1);
        errors[error_count][MAX_PATH - 1] = '\0';
        error_count++;
    }
}

int ends_with(const char *str, const char *suffix)
{
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    return (str_len >= suffix_len) &&
           (strcmp(str + str_len - suffix_len, suffix) == 0);
}

void copy_file(const char *src_path, const char *dest_path)
{
    int src_fd = open(src_path, O_RDONLY);
    if (src_fd == -1)
    {
        char error[MAX_PATH];
        snprintf(error, MAX_PATH, "Error opening source file: %s", src_path);
        add_error(error);
        return;
    }

    int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd == -1)
    {
        char error[MAX_PATH];
        snprintf(error, MAX_PATH, "Error opening destination file: %s", dest_path);
        add_error(error);
        close(src_fd);
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0)
    {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written != bytes_read)
        {
            char error[MAX_PATH];
            snprintf(error, MAX_PATH, "Error writing to file: %s", dest_path);
            add_error(error);
            break;
        }
    }

    close(src_fd);
    close(dest_fd);
}

void process_html_file(const char *input_path, const char *output_path)
{
    FILE *input_file = fopen(input_path, "r");
    if (!input_file)
    {
        char error[MAX_PATH];
        snprintf(error, MAX_PATH, "Error opening input file: %s", input_path);
        add_error(error);
        return;
    }

    FILE *output_file = fopen(output_path, "w");
    if (!output_file)
    {
        char error[MAX_PATH];
        snprintf(error, MAX_PATH, "Error opening output file: %s", output_path);
        add_error(error);
        fclose(input_file);
        return;
    }

    regex_t regex;
    if (regcomp(&regex, "<!-- template: ([^-]+) -->", REG_EXTENDED) != 0)
    {
        add_error("Error compiling regex");
        fclose(input_file);
        fclose(output_file);
        return;
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), input_file))
    {
        regmatch_t matches[2];
        if (regexec(&regex, line, 2, matches, 0) == 0)
        {
            char template_name[MAX_PATH];
            int len = matches[1].rm_eo - matches[1].rm_so;
            strncpy(template_name, line + matches[1].rm_so, len);
            template_name[len] = '\0';

            char template_path[MAX_PATH];
            char *dir = dirname(strdup(input_path));
            snprintf(template_path, MAX_PATH, "%s/%s", dir, template_name);
            free(dir);

            FILE *template_file = fopen(template_path, "r");
            if (template_file)
            {
                char template_line[MAX_LINE];
                while (fgets(template_line, sizeof(template_line), template_file))
                {
                    fputs(template_line, output_file);
                }
                fclose(template_file);
            }
            else
            {
                char error[MAX_PATH];
                snprintf(error, MAX_PATH, "Warning: Template %s not found for %s", template_name, input_path);
                add_error(error);
                fputs(line, output_file);
            }
        }
        else
        {
            fputs(line, output_file);
        }
    }

    regfree(&regex);
    fclose(input_file);
    fclose(output_file);
}

void process_directory(const char *src, const char *dest)
{
    DIR *dir = opendir(src);
    if (!dir)
    {
        char error[MAX_PATH];
        snprintf(error, MAX_PATH, "Error opening directory: %s", src);
        add_error(error);
        return;
    }

    mkdir(dest, 0755);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char src_path[MAX_PATH], dest_path[MAX_PATH];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);

        struct stat path_stat;
        stat(src_path, &path_stat);

        if (S_ISDIR(path_stat.st_mode))
        {
            process_directory(src_path, dest_path);
        }
        else
        {
            if (ends_with(src_path, ".html"))
            {
                process_html_file(src_path, dest_path);
            }
            else
            {
                copy_file(src_path, dest_path);
            }
            printf("Processed: %s -> %s\n", src_path, dest_path);
        }
    }

    closedir(dir);
}

int main()
{
    const char *src_dir = "./src";
    const char *rendered_dir = "./generated";

    process_directory(src_dir, rendered_dir);

    if (error_count == 0)
    {
        printf("\033[0;32mStatic site generation complete.\033[0m\n");
    }
    else
    {
        printf("\033[0;31mStatic site generation completed with errors:\033[0m\n");
        for (int i = 0; i < error_count; i++)
        {
            printf("- %s\n", errors[i]);
        }
        printf("\033[0;31mGeneration failed due to errors.\033[0m\n");
        printf("\033[0;33mFix the errors and run again. :)\033[0m\n");
    }

    return 0;
}