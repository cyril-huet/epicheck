#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "epicheck.h"

static void allocation_error(void)
{
    perror("epicheck");
    exit(2);
}

void issue_add(IssueList *list, const char *path, const char *name, int line,
               int value)
{
    Issue *new_data;
    Issue *issue;

    if (list->len == list->cap)
    {
        if (list->cap == 0)
        {
            list->cap = 16;
        }
        else
        {
            list->cap = list->cap * 2;
        }
        new_data = realloc(list->data, (size_t)list->cap * sizeof(*new_data));
        if (new_data == NULL)
        {
            allocation_error();
        }
        list->data = new_data;
    }
    issue = &list->data[list->len];
    list->len++;
    memset(issue, 0, sizeof(*issue));
    snprintf(issue->path, sizeof(issue->path), "%s", path);
    snprintf(issue->name, sizeof(issue->name), "%s", name);
    issue->line = line;
    issue->value = value;
}

void compile_issue_add(CompileIssueList *list, const char *path,
                       const char *output)
{
    CompileIssue *new_data;
    CompileIssue *issue;

    if (list->len == list->cap)
    {
        if (list->cap == 0)
        {
            list->cap = 8;
        }
        else
        {
            list->cap = list->cap * 2;
        }
        new_data = realloc(list->data, (size_t)list->cap * sizeof(*new_data));
        if (new_data == NULL)
        {
            allocation_error();
        }
        list->data = new_data;
    }
    issue = &list->data[list->len];
    list->len++;
    memset(issue, 0, sizeof(*issue));
    snprintf(issue->path, sizeof(issue->path), "%s", path);
    if (output != NULL && *output != '\0')
    {
        snprintf(issue->output, sizeof(issue->output), "%s", output);
    }
    else
    {
        snprintf(issue->output, sizeof(issue->output), "%s",
                 "no compiler output");
    }
}

char *trim_string(char *string)
{
    char *end;

    while (*string == ' ' || *string == '\t' || *string == '\r'
           || *string == '\n')
    {
        string++;
    }
    end = string + strlen(string);
    while (end > string
           && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'
               || end[-1] == '\n'))
    {
        end--;
    }
    *end = '\0';
    return string;
}

int string_ends_with(const char *string, const char *suffix)
{
    size_t string_length = strlen(string);
    size_t suffix_length = strlen(suffix);

    if (string_length < suffix_length)
    {
        return 0;
    }
    if (strcmp(string + string_length - suffix_length, suffix) == 0)
    {
        return 1;
    }
    return 0;
}

int source_file(const char *path)
{
    const char *extensions[] = { ".c",  ".h",   ".cc",  ".cpp", ".cxx",
                                 ".hh", ".hpp", ".hxx", NULL };

    for (int index = 0; extensions[index] != NULL; index++)
    {
        if (string_ends_with(path, extensions[index]))
        {
            return 1;
        }
    }
    return 0;
}

int implementation_file(const char *path)
{
    const char *extensions[] = { ".c", ".cc", ".cpp", ".cxx", NULL };

    for (int index = 0; extensions[index] != NULL; index++)
    {
        if (string_ends_with(path, extensions[index]))
        {
            return 1;
        }
    }
    return 0;
}

int cpp_file(const char *path)
{
    const char *extensions[] = { ".cc", ".cpp", ".cxx", NULL };

    for (int index = 0; extensions[index] != NULL; index++)
    {
        if (string_ends_with(path, extensions[index]))
        {
            return 1;
        }
    }
    return 0;
}

int executable_available(const char *name)
{
    char *path;
    char *copy;
    char *directory;
    char candidate[PATH_MAX];

    if (strchr(name, '/') != NULL)
    {
        return access(name, X_OK) == 0;
    }
    path = getenv("PATH");
    if (path == NULL)
    {
        return 0;
    }
    copy = strdup(path);
    if (copy == NULL)
    {
        allocation_error();
    }
    directory = strtok(copy, ":");
    while (directory != NULL)
    {
        snprintf(candidate, sizeof(candidate), "%s/%s", directory, name);
        if (access(candidate, X_OK) == 0)
        {
            free(copy);
            return 1;
        }
        directory = strtok(NULL, ":");
    }
    free(copy);
    return 0;
}

static void copy_process_output(char *output, size_t output_size, size_t *used,
                                const char *buffer, ssize_t amount)
{
    size_t available;
    size_t copy_size;

    if (output_size == 0 || *used >= output_size - 1)
    {
        return;
    }
    available = output_size - 1 - *used;
    copy_size = (size_t)amount;
    if (copy_size > available)
    {
        copy_size = available;
    }
    memcpy(output + *used, buffer, copy_size);
    *used += copy_size;
    output[*used] = '\0';
}

int run_process(char *const argv[], char *output, size_t output_size)
{
    int pipe_fd[2];
    pid_t child;
    char buffer[512];
    ssize_t amount;
    size_t used = 0;
    int status;

    if (output_size > 0)
    {
        output[0] = '\0';
    }
    if (pipe(pipe_fd) != 0)
    {
        return 0;
    }
    child = fork();
    if (child == -1)
    {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return 0;
    }
    if (child == 0)
    {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    close(pipe_fd[1]);
    amount = read(pipe_fd[0], buffer, sizeof(buffer));
    while (amount > 0)
    {
        copy_process_output(output, output_size, &used, buffer, amount);
        amount = read(pipe_fd[0], buffer, sizeof(buffer));
    }
    close(pipe_fd[0]);
    if (waitpid(child, &status, 0) == -1)
    {
        return 0;
    }
    if (!WIFEXITED(status))
    {
        return 0;
    }
    if (WEXITSTATUS(status) != 0)
    {
        return 0;
    }
    return 1;
}
