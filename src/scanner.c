#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "epicheck.h"

static int default_ignored_directory(const char *name)
{
    const char *ignored[] = {
        ".git",    "build", "cmake-build-debug", "cmake-build-release",
        ".vscode", ".idea", "node_modules",      "__pycache__",
        ".venv",   NULL
    };

    for (int index = 0; ignored[index] != NULL; index++)
    {
        if (strcmp(name, ignored[index]) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static const char *relative_path(const Options *options, const char *path)
{
    size_t root_length = strlen(options->root);

    if (strncmp(path, options->root, root_length) == 0)
    {
        path += root_length;
        if (*path == '/')
        {
            path++;
        }
    }
    return path;
}

static int excluded_path(const Options *options, const char *path,
                         const char *name)
{
    const char *relative = relative_path(options, path);

    for (int index = 0; index < options->exclude_count; index++)
    {
        const char *pattern = options->excludes[index];

        if (fnmatch(pattern, relative, 0) == 0)
        {
            return 1;
        }
        if (fnmatch(pattern, name, 0) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static void check_format(const char *path, const Options *options,
                         Report *report)
{
    char output[256];
    char *check_arguments[] = { "clang-format", "--dry-run", "-Werror",
                                (char *)path, NULL };
    char *fix_arguments[] = { "clang-format", "-i", (char *)path, NULL };

    if (!options->check_format)
    {
        return;
    }
    if (!executable_available("clang-format"))
    {
        report->format_missing = 1;
        return;
    }
    if (run_process(check_arguments, output, sizeof(output)))
    {
        return;
    }
    if (options->fix)
    {
        if (run_process(fix_arguments, output, sizeof(output)))
        {
            report->format_fixed++;
            return;
        }
    }
    report->format_bad++;
    issue_add(&report->format_errors, path, "format", 1, 0);
}

static void check_ascii(const char *path, const Options *options,
                        Report *report)
{
    FILE *file;
    int character;
    int line = 1;
    int reported = 0;

    if (!options->check_ascii)
    {
        return;
    }
    file = fopen(path, "rb");
    if (file == NULL)
    {
        report->scan_errors++;
        return;
    }
    character = fgetc(file);
    while (character != EOF)
    {
        if (character == '\n')
        {
            line++;
            reported = 0;
        }
        else if (character > 127 && !reported)
        {
            issue_add(&report->non_ascii, path, "non-ascii", line, character);
            reported = 1;
        }
        character = fgetc(file);
    }
    fclose(file);
}

static void path_directory(const char *path, char *directory, size_t size)
{
    char *slash;

    snprintf(directory, size, "%s", path);
    slash = strrchr(directory, '/');
    if (slash == NULL)
    {
        snprintf(directory, size, ".");
    }
    else if (slash == directory)
    {
        slash[1] = '\0';
    }
    else
    {
        *slash = '\0';
    }
}

static int split_flags(char *text, char **arguments, int maximum)
{
    char *read = text;
    char *write = text;
    int count = 0;

    while (*read != '\0')
    {
        char quote = '\0';

        while (*read == ' ' || *read == '\t')
        {
            read++;
        }
        if (*read == '\0')
        {
            break;
        }
        if (count >= maximum)
        {
            return -1;
        }
        arguments[count++] = write;
        while (*read != '\0')
        {
            if (quote == '\0' && (*read == '\'' || *read == '"'))
            {
                quote = *read++;
                continue;
            }
            if (quote != '\0' && *read == quote)
            {
                quote = '\0';
                read++;
                continue;
            }
            if (*read == '\\' && read[1] != '\0')
            {
                read++;
            }
            else if (quote == '\0' && (*read == ' ' || *read == '\t'))
            {
                break;
            }
            *write++ = *read++;
        }
        if (quote != '\0')
        {
            return -1;
        }
        while (*read == ' ' || *read == '\t')
        {
            read++;
        }
        *write++ = '\0';
    }
    return count;
}

static int build_compile_arguments(const char *path, const Options *options,
                                   char **arguments, char *flag_storage,
                                   char *include_root, char *include_main,
                                   char *include_local)
{
    const char *compiler;
    const char *flags;
    char directory[PATH_MAX];
    int count = 0;
    int extra;

    if (cpp_file(path))
    {
        compiler = getenv("CXX");
    }
    else
    {
        compiler = getenv("CC");
    }
    if (compiler == NULL || *compiler == '\0')
    {
        if (cpp_file(path))
        {
            compiler = "c++";
        }
        else
        {
            compiler = "cc";
        }
    }
    if (cpp_file(path))
    {
        flags = options->cxxflags;
    }
    else
    {
        flags = options->cflags;
    }
    arguments[count++] = (char *)compiler;
    arguments[count++] = "-Wall";
    arguments[count++] = "-Wextra";
    arguments[count++] = "-Werror";
    arguments[count++] = "-pedantic";
    if (cpp_file(path))
    {
        arguments[count] = "-std=c++20";
    }
    else
    {
        arguments[count] = "-std=c11";
    }
    count++;
    snprintf(flag_storage, MAX_FLAGS, "%s", flags);
    extra = split_flags(flag_storage, arguments + count, 96 - count);
    if (extra < 0)
    {
        return 0;
    }
    count += extra;
    snprintf(include_root, PATH_MAX + 3, "-I%s", options->root);
    snprintf(include_main, PATH_MAX + 11, "-I%s/include", options->root);
    path_directory(path, directory, sizeof(directory));
    snprintf(include_local, PATH_MAX + 3, "-I%s", directory);
    arguments[count++] = include_root;
    arguments[count++] = include_main;
    arguments[count++] = include_local;
    arguments[count++] = "-fsyntax-only";
    arguments[count++] = (char *)path;
    arguments[count] = NULL;
    return 1;
}

static void check_compile(const char *path, const Options *options,
                          Report *report)
{
    char *arguments[128];
    char flag_storage[MAX_FLAGS];
    char include_root[PATH_MAX + 3];
    char include_main[PATH_MAX + 11];
    char include_local[PATH_MAX + 3];
    char output[MAX_OUTPUT];
    const char *compiler;

    if (!options->check_compile)
    {
        return;
    }
    if (!implementation_file(path))
    {
        return;
    }
    if (cpp_file(path))
    {
        compiler = getenv("CXX");
    }
    else
    {
        compiler = getenv("CC");
    }
    if (compiler == NULL || *compiler == '\0')
    {
        if (cpp_file(path))
        {
            compiler = "c++";
        }
        else
        {
            compiler = "cc";
        }
    }
    if (!executable_available(compiler))
    {
        report->compile_missing = 1;
        return;
    }
    report->compile_files++;
    if (!build_compile_arguments(path, options, arguments, flag_storage,
                                 include_root, include_main, include_local))
    {
        report->compile_bad++;
        compile_issue_add(&report->compile_errors, path,
                          "invalid compiler flags");
        return;
    }
    if (!run_process(arguments, output, sizeof(output)))
    {
        report->compile_bad++;
        compile_issue_add(&report->compile_errors, path, output);
    }
}

static void check_file(const char *path, const Options *options, Report *report)
{
    report->files++;
    analyze_source(path, options, report);
    check_format(path, options, report);
    check_ascii(path, options, report);
    check_compile(path, options, report);
}

static void scan_path(const char *path, const Options *options, Report *report)
{
    struct stat status;
    const char *name = strrchr(path, '/');

    if (name == NULL)
    {
        name = path;
    }
    else
    {
        name++;
    }
    if (excluded_path(options, path, name))
    {
        return;
    }
    if (lstat(path, &status) != 0)
    {
        report->scan_errors++;
        return;
    }
    if (S_ISLNK(status.st_mode))
    {
        return;
    }
    if (S_ISREG(status.st_mode))
    {
        if (source_file(path))
        {
            check_file(path, options, report);
        }
    }
    else if (S_ISDIR(status.st_mode))
    {
        DIR *directory;
        struct dirent *entry;

        if (default_ignored_directory(name))
        {
            return;
        }
        directory = opendir(path);
        if (directory == NULL)
        {
            report->scan_errors++;
            return;
        }
        entry = readdir(directory);
        while (entry != NULL)
        {
            char child[PATH_MAX];
            int length;

            if (strcmp(entry->d_name, ".") != 0
                && strcmp(entry->d_name, "..") != 0)
            {
                length = snprintf(child, sizeof(child), "%s/%s", path,
                                  entry->d_name);
                if (length < 0 || (size_t)length >= sizeof(child))
                {
                    report->scan_errors++;
                }
                else
                {
                    scan_path(child, options, report);
                }
            }
            entry = readdir(directory);
        }
        closedir(directory);
    }
}

void scan_project(const Options *options, Report *report)
{
    scan_path(options->root, options, report);
}
