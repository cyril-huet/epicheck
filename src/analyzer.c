#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "epicheck.h"

#define MAX_LINE 4096
#define MAX_SIGNATURE 16384

typedef struct
{
    char name[MAX_NAME];
    int start;
    int lines;
    int arguments;
    int exported;
} Function;

static int starts_word(const char *text, const char *word)
{
    size_t length = strlen(word);

    if (strncmp(text, word, length) != 0)
    {
        return 0;
    }
    if (text[length] == '\0')
    {
        return 1;
    }
    if (isspace((unsigned char)text[length]))
    {
        return 1;
    }
    if (text[length] == '(')
    {
        return 1;
    }
    return 0;
}

static int invalid_function_start(const char *text)
{
    const char *words[] = { "if",     "for",     "while",     "switch",
                            "catch",  "else",    "do",        "return",
                            "sizeof", "typedef", "struct",    "enum",
                            "union",  "class",   "namespace", "_Static_assert",
                            NULL };

    for (int index = 0; words[index] != NULL; index++)
    {
        if (starts_word(text, words[index]))
        {
            return 1;
        }
    }
    return 0;
}

static int contains_word(const char *text, const char *word)
{
    size_t length = strlen(word);
    const char *position = text;

    position = strstr(position, word);
    while (position != NULL)
    {
        int left = 0;
        int right = 0;

        if (position == text)
        {
            left = 1;
        }
        else if (!isalnum((unsigned char)position[-1]) && position[-1] != '_')
        {
            left = 1;
        }
        if (!isalnum((unsigned char)position[length])
            && position[length] != '_')
        {
            right = 1;
        }

        if (left && right)
        {
            return 1;
        }
        position++;
        position = strstr(position, word);
    }
    return 0;
}

static int counted_line(const char *line)
{
    char copy[MAX_LINE];
    char *text;

    snprintf(copy, sizeof(copy), "%s", line);
    text = trim_string(copy);
    if (*text == '\0')
    {
        return 0;
    }
    while (*text != '\0')
    {
        if (isspace((unsigned char)*text))
        {
            text++;
            continue;
        }
        if (strchr("{}[]();", *text) == NULL)
        {
            return 1;
        }
        text++;
    }
    return 0;
}

static void copy_hidden_character(char *output, int *output_index, char value)
{
    if (value == '\n')
    {
        output[*output_index] = '\n';
    }
    else
    {
        output[*output_index] = ' ';
    }
    (*output_index)++;
}

static void clean_line(char *line, int *block_comment)
{
    char output[MAX_LINE];
    int input_index = 0;
    int output_index = 0;
    int string = 0;
    int character = 0;

    while (line[input_index] != '\0' && output_index < MAX_LINE - 1)
    {
        char current = line[input_index];
        char next = line[input_index + 1];

        if (*block_comment)
        {
            copy_hidden_character(output, &output_index, current);
            if (current == '*' && next == '/')
            {
                copy_hidden_character(output, &output_index, next);
                *block_comment = 0;
                input_index += 2;
            }
            else
            {
                input_index++;
            }
        }
        else if (string || character)
        {
            output[output_index++] = current;
            if (current == '\\' && next != '\0' && output_index < MAX_LINE - 1)
            {
                output[output_index++] = next;
                input_index += 2;
            }
            else
            {
                if (string && current == '"')
                {
                    string = 0;
                }
                if (character && current == '\'')
                {
                    character = 0;
                }
                input_index++;
            }
        }
        else if (current == '/' && next == '*')
        {
            output[output_index++] = ' ';
            output[output_index++] = ' ';
            *block_comment = 1;
            input_index += 2;
        }
        else if (current == '/' && next == '/')
        {
            break;
        }
        else
        {
            if (current == '"')
            {
                string = 1;
            }
            else if (current == '\'')
            {
                character = 1;
            }
            output[output_index++] = current;
            input_index++;
        }
    }
    output[output_index] = '\0';
    snprintf(line, MAX_LINE, "%s", output);
}

static int brace_change(const char *line)
{
    int change = 0;
    int string = 0;
    int character = 0;

    for (int index = 0; line[index] != '\0'; index++)
    {
        char current = line[index];
        char next = line[index + 1];

        if (string || character)
        {
            if (current == '\\' && next != '\0')
            {
                index++;
            }
            else if (string && current == '"')
            {
                string = 0;
            }
            else if (character && current == '\'')
            {
                character = 0;
            }
        }
        else if (current == '"')
        {
            string = 1;
        }
        else if (current == '\'')
        {
            character = 1;
        }
        else if (current == '{')
        {
            change++;
        }
        else if (current == '}')
        {
            change--;
        }
    }
    return change;
}

static int count_arguments(const char *arguments)
{
    char copy[MAX_SIGNATURE];
    char *text;
    int depth = 0;
    int count = 1;
    int content = 0;

    snprintf(copy, sizeof(copy), "%s", arguments);
    text = trim_string(copy);
    if (*text == '\0' || strcmp(text, "void") == 0)
    {
        return 0;
    }
    for (int index = 0; text[index] != '\0'; index++)
    {
        if (!isspace((unsigned char)text[index]))
        {
            content = 1;
        }
        if (strchr("([{<", text[index]) != NULL)
        {
            depth++;
        }
        else if (strchr(")]}>", text[index]) != NULL && depth > 0)
        {
            depth--;
        }
        else if (text[index] == ',' && depth == 0)
        {
            count++;
        }
    }
    if (content)
    {
        return count;
    }
    return 0;
}

static int parse_signature(char *signature, Function *function)
{
    char *text = trim_string(signature);
    char *brace = strchr(text, '{');
    char *left_parenthesis;
    char *right_parenthesis;
    char *name_end;
    char *name_start;
    size_t name_length;

    if (brace != NULL)
    {
        *brace = '\0';
    }
    text = trim_string(text);
    if (*text == '#' || invalid_function_start(text) || strchr(text, ';'))
    {
        return 0;
    }
    left_parenthesis = strrchr(text, '(');
    right_parenthesis = strrchr(text, ')');
    if (left_parenthesis == NULL || right_parenthesis == NULL
        || right_parenthesis < left_parenthesis)
    {
        return 0;
    }
    name_end = left_parenthesis;
    while (name_end > text && isspace((unsigned char)name_end[-1]))
    {
        name_end--;
    }
    name_start = name_end;
    while (name_start > text
           && (isalnum((unsigned char)name_start[-1]) || name_start[-1] == '_'
               || name_start[-1] == ':' || name_start[-1] == '~'))
    {
        name_start--;
    }
    name_length = (size_t)(name_end - name_start);
    if (name_length == 0 || name_length >= MAX_NAME)
    {
        return 0;
    }
    memcpy(function->name, name_start, name_length);
    function->name[name_length] = '\0';
    if (invalid_function_start(function->name))
    {
        return 0;
    }
    *right_parenthesis = '\0';
    function->arguments = count_arguments(left_parenthesis + 1);
    *right_parenthesis = ')';
    function->exported = !contains_word(text, "static");
    return 1;
}

static int append_signature(char *signature, const char *line)
{
    size_t current = strlen(signature);
    size_t added = strlen(line);

    if (current + added + 2 >= MAX_SIGNATURE)
    {
        return 0;
    }
    if (current != 0)
    {
        signature[current++] = ' ';
    }
    memcpy(signature + current, line, added + 1);
    return 1;
}

static void finish_function(const char *path, const Function *function,
                            const Options *options, Report *report)
{
    report->functions++;
    if (function->lines > options->max_lines)
    {
        issue_add(&report->long_functions, path, function->name,
                  function->start, function->lines);
    }
    if (function->arguments > options->max_args)
    {
        issue_add(&report->many_arguments, path, function->name,
                  function->start, function->arguments);
    }
    if (function->exported && implementation_file(path))
    {
        report->exported++;
        issue_add(&report->exported_functions, path, function->name,
                  function->start, 0);
    }
}

static int possible_signature(const char *line)
{
    if (strchr(line, '(') == NULL)
    {
        return 0;
    }
    if (invalid_function_start(line))
    {
        return 0;
    }
    if (line[0] == '#')
    {
        return 0;
    }
    return 1;
}

void analyze_source(const char *path, const Options *options, Report *report)
{
    FILE *file = fopen(path, "r");
    char line[MAX_LINE];
    char signature[MAX_SIGNATURE] = "";
    Function function;
    int line_number = 0;
    int block_comment = 0;
    int candidate = 0;
    int brace_depth = 0;

    if (file == NULL)
    {
        report->scan_errors++;
        return;
    }
    memset(&function, 0, sizeof(function));
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char copy[MAX_LINE];
        char *text;

        line_number++;
        clean_line(line, &block_comment);
        snprintf(copy, sizeof(copy), "%s", line);
        text = trim_string(copy);
        if (!candidate && brace_depth == 0 && possible_signature(text))
        {
            candidate = 1;
            function.start = line_number;
            signature[0] = '\0';
        }
        if (candidate)
        {
            if (!append_signature(signature, text)
                || (strchr(text, ';') && !strchr(text, '{')))
            {
                candidate = 0;
            }
            else if (strchr(text, '{') != NULL)
            {
                candidate = 0;
                if (parse_signature(signature, &function))
                {
                    function.lines = counted_line(line);
                    brace_depth = brace_change(line);
                    if (brace_depth == 0)
                    {
                        finish_function(path, &function, options, report);
                    }
                }
            }
        }
        else if (brace_depth > 0)
        {
            function.lines += counted_line(line);
            brace_depth += brace_change(line);
            if (brace_depth <= 0)
            {
                finish_function(path, &function, options, report);
            }
        }
    }
    fclose(file);
}
