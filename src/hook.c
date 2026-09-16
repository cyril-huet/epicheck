#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "epicheck.h"

static int absolute_path(const char *path, char *output, size_t size)
{
    char current[PATH_MAX];
    int length;

    if (path[0] == '/')
    {
        length = snprintf(output, size, "%s", path);
    }
    else
    {
        if (getcwd(current, sizeof(current)) == NULL)
        {
            return 0;
        }
        length = snprintf(output, size, "%s/%s", current, path);
    }
    if (length < 0)
    {
        return 0;
    }
    if ((size_t)length >= size)
    {
        return 0;
    }
    return 1;
}

static int locate_program(const char *program, char *output, size_t size)
{
    char *path;
    char *copy;
    char *directory;

    if (strchr(program, '/') != NULL)
    {
        if (access(program, X_OK) != 0)
        {
            return 0;
        }
        return absolute_path(program, output, size);
    }
    path = getenv("PATH");
    if (path == NULL)
    {
        return 0;
    }
    copy = strdup(path);
    if (copy == NULL)
    {
        return 0;
    }
    directory = strtok(copy, ":");
    while (directory != NULL)
    {
        char candidate[PATH_MAX];

        snprintf(candidate, sizeof(candidate), "%s/%s", directory, program);
        if (access(candidate, X_OK) == 0)
        {
            int found = absolute_path(candidate, output, size);

            free(copy);
            return found;
        }
        directory = strtok(NULL, ":");
    }
    free(copy);
    return 0;
}

static int find_git_root(const Options *options, char *output, size_t size)
{
    char captured[PATH_MAX];
    char *arguments[] = {
        "git", "-C", (char *)options->root, "rev-parse", "--show-toplevel", NULL
    };
    char *root;

    if (!run_process(arguments, captured, sizeof(captured)))
    {
        return 0;
    }
    root = trim_string(captured);
    if (strlen(root) >= size)
    {
        return 0;
    }
    snprintf(output, size, "%s", root);
    return 1;
}

static int write_hook(const char *path, const char *program)
{
    FILE *file = fopen(path, "w");

    if (file == NULL)
    {
        return 0;
    }
    fprintf(file, "#!/bin/sh\n\n");
    fprintf(file, "ROOT=$(git rev-parse --show-toplevel)\n");
    fprintf(file, "echo \"[EPICheck] checking project before push...\"\n");
    fprintf(file, "\"%s\" \"$ROOT\" --strict\n", program);
    fprintf(file, "STATUS=$?\n");
    fprintf(file, "if [ \"$STATUS\" -ne 0 ]; then\n");
    fprintf(file, "    echo \"[EPICheck] push blocked.\"\n");
    fprintf(file, "fi\n");
    fprintf(file, "exit \"$STATUS\"\n");
    if (fclose(file) != 0)
    {
        return 0;
    }
    if (chmod(path, 0755) != 0)
    {
        return 0;
    }
    return 1;
}

int install_hook(const char *program, const Options *options)
{
    char executable[PATH_MAX];
    char root[PATH_MAX];
    char hook[PATH_MAX];

    if (!locate_program(program, executable, sizeof(executable)))
    {
        fprintf(stderr, "epicheck: cannot locate its executable\n");
        return 2;
    }
    if (!find_git_root(options, root, sizeof(root)))
    {
        fprintf(stderr, "epicheck: '%s' is not a Git repository\n",
                options->root);
        return 2;
    }
    if (snprintf(hook, sizeof(hook), "%s/.git/hooks/pre-push", root)
        >= (int)sizeof(hook))
    {
        fprintf(stderr, "epicheck: hook path is too long\n");
        return 2;
    }
    if (access(hook, F_OK) == 0)
    {
        fprintf(stderr, "epicheck: a pre-push hook already exists at %s\n",
                hook);
        return 2;
    }
    if (!write_hook(hook, executable))
    {
        perror("epicheck: cannot install hook");
        return 2;
    }
    printf("EPICheck hook installed: %s\n", hook);
    return 0;
}
