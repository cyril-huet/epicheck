#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "epicheck.h"

void options_init(Options *options)
{
    memset(options, 0, sizeof(*options));
    options->root = ".";
    options->check_format = 1;
    options->check_ascii = 1;
    options->check_compile = 1;
    options->color = isatty(STDOUT_FILENO);
    options->max_lines = 40;
    options->max_args = 4;
    options->max_exported = 10;
    options->output = OUTPUT_TEXT;
}

void print_usage(const char *program)
{
    printf("Usage: %s [path] [options]\n\n", program);
    printf("Checks:\n");
    printf("  --format / --no-format    enable or disable clang-format\n");
    printf("  --ascii / --no-ascii      enable or disable ASCII checks\n");
    printf("  --compile / --no-compile  enable or disable compilation\n");
    printf("  --strict, --push           enable every check\n");
    printf("  --fix                      fix formatting errors\n\n");
    printf("Configuration:\n");
    printf("  --exclude PATTERN          exclude matching paths\n");
    printf("  --cflags FLAGS             extra flags for C files\n");
    printf("  --cxxflags FLAGS           extra flags for C++ files\n");
    printf("  --max-lines N              maximum lines per function\n");
    printf("  --max-args N               maximum arguments per function\n");
    printf("  --max-exported N           maximum exported functions\n\n");
    printf("Output:\n");
    printf("  --output FORMAT            text, json, html or github\n");
    printf("  --no-color                 disable ANSI colors\n\n");
    printf("Other:\n");
    printf("  --install-hook             install a pre-push hook\n");
    printf("  --version                  display the version\n");
    printf("  --help, -h                 display this help\n");
}

static int parse_positive_int(const char *text, int *value)
{
    char *end;
    long result;

    errno = 0;
    result = strtol(text, &end, 10);
    if (errno != 0)
    {
        return 0;
    }
    if (text[0] == '\0' || end[0] != '\0')
    {
        return 0;
    }
    if (result <= 0 || result > 1000000)
    {
        return 0;
    }
    *value = (int)result;
    return 1;
}

static int parse_output(const char *text, OutputFormat *output)
{
    if (strcmp(text, "text") == 0)
    {
        *output = OUTPUT_TEXT;
        return 1;
    }
    if (strcmp(text, "json") == 0)
    {
        *output = OUTPUT_JSON;
        return 1;
    }
    if (strcmp(text, "html") == 0)
    {
        *output = OUTPUT_HTML;
        return 1;
    }
    if (strcmp(text, "github") == 0)
    {
        *output = OUTPUT_GITHUB;
        return 1;
    }
    return 0;
}

static int add_exclude(Options *options, const char *pattern)
{
    if (options->exclude_count >= MAX_EXCLUDES)
    {
        fprintf(stderr, "epicheck: too many exclude patterns\n");
        return 0;
    }
    snprintf(options->excludes[options->exclude_count], PATH_MAX, "%s",
             pattern);
    options->exclude_count++;
    return 1;
}

static int option_takes_value(const char *argument)
{
    const char *names[] = { "--exclude",   "--cflags",   "--cxxflags",
                            "--max-lines", "--max-args", "--max-exported",
                            "--output",    NULL };

    for (int index = 0; names[index] != NULL; index++)
    {
        if (strcmp(argument, names[index]) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int read_value(int argc, char **argv, int *index, const char **value)
{
    if (*index + 1 >= argc)
    {
        fprintf(stderr, "epicheck: option '%s' requires a value\n",
                argv[*index]);
        return 0;
    }
    *index = *index + 1;
    *value = argv[*index];
    return 1;
}

static int parse_limit(const char *name, const char *value, int *destination)
{
    if (parse_positive_int(value, destination))
    {
        return 1;
    }
    fprintf(stderr, "epicheck: invalid value for %s: '%s'\n", name, value);
    return 0;
}

static int parse_value_option(const char *name, const char *value,
                              Options *options)
{
    if (strcmp(name, "--exclude") == 0)
    {
        return add_exclude(options, value);
    }
    if (strcmp(name, "--cflags") == 0)
    {
        snprintf(options->cflags, sizeof(options->cflags), "%s", value);
        return 1;
    }
    if (strcmp(name, "--cxxflags") == 0)
    {
        snprintf(options->cxxflags, sizeof(options->cxxflags), "%s", value);
        return 1;
    }
    if (strcmp(name, "--max-lines") == 0)
    {
        return parse_limit(name, value, &options->max_lines);
    }
    if (strcmp(name, "--max-args") == 0)
    {
        return parse_limit(name, value, &options->max_args);
    }
    if (strcmp(name, "--max-exported") == 0)
    {
        return parse_limit(name, value, &options->max_exported);
    }
    if (strcmp(name, "--output") == 0)
    {
        if (parse_output(value, &options->output))
        {
            return 1;
        }
        fprintf(stderr, "epicheck: invalid output format '%s'\n", value);
        return 0;
    }
    return 0;
}

static void enable_all_checks(Options *options)
{
    options->strict = 1;
    options->check_format = 1;
    options->check_ascii = 1;
    options->check_compile = 1;
}

static int parse_switch(const char *argument, const char *program,
                        Options *options)
{
    if (strcmp(argument, "--format") == 0)
    {
        options->check_format = 1;
    }
    else if (strcmp(argument, "--no-format") == 0)
    {
        options->check_format = 0;
    }
    else if (strcmp(argument, "--ascii") == 0)
    {
        options->check_ascii = 1;
    }
    else if (strcmp(argument, "--no-ascii") == 0)
    {
        options->check_ascii = 0;
    }
    else if (strcmp(argument, "--compile") == 0)
    {
        options->check_compile = 1;
    }
    else if (strcmp(argument, "--no-compile") == 0)
    {
        options->check_compile = 0;
    }
    else if (strcmp(argument, "--strict") == 0)
    {
        enable_all_checks(options);
    }
    else if (strcmp(argument, "--push") == 0)
    {
        enable_all_checks(options);
    }
    else if (strcmp(argument, "--fix") == 0)
    {
        options->fix = 1;
    }
    else if (strcmp(argument, "--no-color") == 0)
    {
        options->color = 0;
    }
    else if (strcmp(argument, "--install-hook") == 0)
    {
        options->install_hook = 1;
    }
    else if (strcmp(argument, "--version") == 0)
    {
        printf("epicheck %s\n", EPICHECK_VERSION);
        exit(0);
    }
    else if (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0)
    {
        print_usage(program);
        exit(0);
    }
    else
    {
        return 0;
    }
    return 1;
}

int options_parse(int argc, char **argv, Options *options)
{
    int path_count = 0;

    for (int index = 1; index < argc; index++)
    {
        const char *argument = argv[index];

        if (option_takes_value(argument))
        {
            const char *value;

            if (!read_value(argc, argv, &index, &value))
            {
                return 0;
            }
            if (!parse_value_option(argument, value, options))
            {
                return 0;
            }
        }
        else if (argument[0] == '-')
        {
            if (!parse_switch(argument, argv[0], options))
            {
                fprintf(stderr, "epicheck: unknown option '%s'\n", argument);
                return 0;
            }
        }
        else
        {
            options->root = argument;
            path_count++;
        }
    }
    if (path_count > 1)
    {
        fprintf(stderr, "epicheck: only one project path is allowed\n");
        return 0;
    }
    if (options->output != OUTPUT_TEXT)
    {
        options->color = 0;
    }
    return 1;
}
