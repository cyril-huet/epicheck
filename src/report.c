#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "epicheck.h"

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BOLD "\033[1m"
#define RESET "\033[0m"
#define HTML_REPORT_FILE "epicheck-report.html"

static const char *color(const Options *options, const char *value)
{
    if (options->color)
    {
        return value;
    }
    return "";
}

int report_has_errors(const Options *options, const Report *report)
{
    if (report->format_bad > 0)
    {
        return 1;
    }
    if (report->long_functions.len > 0)
    {
        return 1;
    }
    if (report->many_arguments.len > 0)
    {
        return 1;
    }
    if (report->exported > options->max_exported)
    {
        return 1;
    }
    if (report->non_ascii.len > 0)
    {
        return 1;
    }
    if (report->compile_bad > 0 || report->scan_errors > 0)
    {
        return 1;
    }
    if (options->strict)
    {
        if (report->format_missing || report->compile_missing)
        {
            return 1;
        }
    }
    return 0;
}

static void text_status(const Options *options, const char *name, int success,
                        const char *detail)
{
    const char *label;
    const char *status_color;

    if (success)
    {
        label = "OK";
        status_color = GREEN;
    }
    else
    {
        label = "FAIL";
        status_color = RED;
    }

    printf("  %-12s %s%-5s%s %s\n", name, color(options, status_color), label,
           color(options, RESET), detail);
}

static void text_summary(const Options *options, const Report *report)
{
    char detail[128];

    printf("%sSummary%s\n", color(options, BOLD), color(options, RESET));
    if (!options->check_format)
    {
        text_status(options, "format", 1, "disabled");
    }
    else if (report->format_missing)
    {
        text_status(options, "format", !options->strict,
                    "clang-format is not installed");
    }
    else
    {
        snprintf(detail, sizeof(detail), "%d error(s), %d fixed",
                 report->format_bad, report->format_fixed);
        text_status(options, "format", report->format_bad == 0, detail);
    }
    snprintf(detail, sizeof(detail), "%d function(s)",
             report->long_functions.len);
    text_status(options, "length", report->long_functions.len == 0, detail);
    snprintf(detail, sizeof(detail), "%d function(s)",
             report->many_arguments.len);
    text_status(options, "arguments", report->many_arguments.len == 0, detail);
    snprintf(detail, sizeof(detail), "%d / %d", report->exported,
             options->max_exported);
    text_status(options, "exports", report->exported <= options->max_exported,
                detail);
    if (!options->check_ascii)
    {
        text_status(options, "ascii", 1, "disabled");
    }
    else
    {
        snprintf(detail, sizeof(detail), "%d line(s)", report->non_ascii.len);
        text_status(options, "ascii", report->non_ascii.len == 0, detail);
    }
    if (!options->check_compile)
    {
        text_status(options, "compile", 1, "disabled");
    }
    else if (report->compile_missing)
    {
        text_status(options, "compile", !options->strict,
                    "compiler is not installed");
    }
    else
    {
        snprintf(detail, sizeof(detail), "%d / %d passed",
                 report->compile_files - report->compile_bad,
                 report->compile_files);
        text_status(options, "compile", report->compile_bad == 0, detail);
    }
    if (report->scan_errors > 0)
    {
        snprintf(detail, sizeof(detail), "%d filesystem error(s)",
                 report->scan_errors);
        text_status(options, "scan", 0, detail);
    }
    printf("\n");
}

static void text_issues(const char *title, const IssueList *list,
                        const char *unit)
{
    if (list->len == 0)
    {
        return;
    }
    printf("  %s\n", title);
    for (int index = 0; index < list->len; index++)
    {
        const Issue *issue = &list->data[index];

        if (*unit == '\0')
        {
            printf("    %s:%d %s\n", issue->path, issue->line, issue->name);
        }
        else
        {
            printf("    %s:%d %s = %d %s\n", issue->path, issue->line,
                   issue->name, issue->value, unit);
        }
    }
    printf("\n");
}

static void print_compile_output(const CompileIssue *issue)
{
    const char *position = issue->output;
    int lines = 0;

    printf("  Compilation failed: %s\n", issue->path);
    while (*position != '\0' && lines < 12)
    {
        printf("    ");
        while (*position != '\0' && *position != '\n')
        {
            putchar(*position++);
        }
        putchar('\n');
        if (*position == '\n')
        {
            position++;
        }
        lines++;
    }
    if (*position != '\0')
    {
        printf("    ...\n");
    }
    printf("\n");
}

static void text_details(const Options *options, const Report *report)
{
    int has_details = report->format_errors.len || report->long_functions.len
        || report->many_arguments.len
        || report->exported > options->max_exported || report->non_ascii.len
        || report->compile_errors.len;

    if (!has_details)
    {
        return;
    }
    printf("%sDetails%s\n", color(options, BOLD), color(options, RESET));
    text_issues("Formatting errors", &report->format_errors, "");
    text_issues("Functions too long", &report->long_functions, "lines");
    text_issues("Functions with too many arguments", &report->many_arguments,
                "arguments");
    if (report->exported > options->max_exported)
    {
        text_issues("Exported functions", &report->exported_functions, "");
    }
    text_issues("Non-ASCII characters", &report->non_ascii, "byte");
    for (int index = 0; index < report->compile_errors.len; index++)
    {
        print_compile_output(&report->compile_errors.data[index]);
    }
}

static void print_text(const Options *options, const Report *report)
{
    int failed = report_has_errors(options, report);
    const char *result_color;
    const char *result_text;

    if (failed)
    {
        result_color = RED;
        result_text = "checks failed";
    }
    else
    {
        result_color = GREEN;
        result_text = "all checks passed";
    }

    printf("\n%sEPICheck %s%s\n", color(options, BOLD), EPICHECK_VERSION,
           color(options, RESET));
    printf("Project: %s\n", options->root);
    printf("Files: %d, functions: %d\n\n", report->files, report->functions);
    text_summary(options, report);
    text_details(options, report);
    printf("%sResult:%s %s%s%s\n\n", color(options, BOLD),
           color(options, RESET), color(options, result_color), result_text,
           color(options, RESET));
}

static void json_string(const char *text)
{
    putchar('"');
    while (*text != '\0')
    {
        unsigned char character = (unsigned char)*text;

        if (character == '"' || character == '\\')
        {
            printf("\\%c", character);
        }
        else if (character == '\n')
        {
            printf("\\n");
        }
        else if (character == '\r')
        {
            printf("\\r");
        }
        else if (character == '\t')
        {
            printf("\\t");
        }
        else if (character < 32)
        {
            printf("\\u%04x", character);
        }
        else
        {
            putchar(character);
        }
        text++;
    }
    putchar('"');
}

static void json_issue_separator(int *first)
{
    if (!*first)
    {
        printf(",\n");
    }
    *first = 0;
}

static void json_issue(int *first, const char *rule, const Issue *issue,
                       const char *message)
{
    json_issue_separator(first);
    printf("    {\"rule\": ");
    json_string(rule);
    printf(", \"path\": ");
    json_string(issue->path);
    printf(", \"line\": %d, \"value\": %d, \"message\": ", issue->line,
           issue->value);
    json_string(message);
    printf("}");
}

static void json_issue_list(int *first, const char *rule, const IssueList *list,
                            const char *message)
{
    for (int index = 0; index < list->len; index++)
    {
        json_issue(first, rule, &list->data[index], message);
    }
}

static void json_compile_issues(int *first,
                                const CompileIssueList *compile_errors)
{
    for (int index = 0; index < compile_errors->len; index++)
    {
        const CompileIssue *issue = &compile_errors->data[index];

        json_issue_separator(first);
        printf("    {\"rule\": \"compile\", \"path\": ");
        json_string(issue->path);
        printf(", \"line\": 1, \"value\": 0, \"message\": ");
        json_string(issue->output);
        printf("}");
    }
}

static void json_general_issue(int *first, const char *rule,
                               const char *message)
{
    json_issue_separator(first);
    printf("    {\"rule\": ");
    json_string(rule);
    printf(", \"path\": \"\", \"line\": 0, \"value\": 0, "
           "\"message\": ");
    json_string(message);
    printf("}");
}

static void print_json(const Options *options, const Report *report)
{
    int first = 1;
    const char *success;

    if (report_has_errors(options, report))
    {
        success = "false";
    }
    else
    {
        success = "true";
    }

    printf("{\n  \"version\": \"%s\",\n", EPICHECK_VERSION);
    printf("  \"success\": %s,\n", success);
    printf("  \"root\": ");
    json_string(options->root);
    printf(",\n  \"summary\": {\"files\": %d, \"functions\": %d, "
           "\"exported\": %d},\n",
           report->files, report->functions, report->exported);
    printf("  \"issues\": [\n");
    json_issue_list(&first, "format", &report->format_errors,
                    "file is not formatted");
    json_issue_list(&first, "function-length", &report->long_functions,
                    "function exceeds the line limit");
    json_issue_list(&first, "function-arguments", &report->many_arguments,
                    "function exceeds the argument limit");
    if (report->exported > options->max_exported)
    {
        json_issue_list(&first, "exported-function",
                        &report->exported_functions,
                        "too many exported functions");
    }
    json_issue_list(&first, "non-ascii", &report->non_ascii,
                    "non-ASCII byte found");
    json_compile_issues(&first, &report->compile_errors);
    if (report->scan_errors > 0)
    {
        json_general_issue(&first, "scan", "filesystem traversal failed");
    }
    if (options->strict && report->format_missing)
    {
        json_general_issue(&first, "format-tool",
                           "clang-format is not installed");
    }
    if (options->strict && report->compile_missing)
    {
        json_general_issue(&first, "compiler", "compiler is not installed");
    }
    if (!first)
    {
        printf("\n");
    }
    printf("  ]\n}\n");
}

static void html_escape(FILE *file, const char *text)
{
    while (*text != '\0')
    {
        if (*text == '&')
        {
            fprintf(file, "&amp;");
        }
        else if (*text == '<')
        {
            fprintf(file, "&lt;");
        }
        else if (*text == '>')
        {
            fprintf(file, "&gt;");
        }
        else if (*text == '"')
        {
            fprintf(file, "&quot;");
        }
        else if (*text == '\'')
        {
            fprintf(file, "&#39;");
        }
        else
        {
            fputc(*text, file);
        }
        text++;
    }
}

static void html_header(FILE *file, const Options *options,
                        const Report *report)
{
    int failed = report_has_errors(options, report);
    const char *status_class;
    const char *status_text;

    if (failed)
    {
        status_class = "failed";
        status_text = "Checks failed";
    }
    else
    {
        status_class = "passed";
        status_text = "All checks passed";
    }

    fprintf(file, "<!doctype html>\n<html lang=\"en\">\n<head>\n");
    fprintf(file, "  <meta charset=\"utf-8\">\n");
    fprintf(file,
            "  <meta name=\"viewport\" content=\"width=device-width, "
            "initial-scale=1\">\n");
    fprintf(file, "  <title>EPICheck report</title>\n");
    fprintf(file, "  <style>\n");
    fprintf(file,
            "    :root { color-scheme: light dark; font-family: system-ui, "
            "sans-serif; }\n");
    fprintf(file,
            "    body { max-width: 1000px; margin: 40px auto; padding: 0 "
            "20px; }\n");
    fprintf(file,
            "    header, section { border: 1px solid #8885; border-radius: "
            "12px; padding: 20px; margin-bottom: 20px; }\n");
    fprintf(file,
            "    .passed { color: #18864b; } .failed { color: #c73535; } "
            ".skipped { color: #777; }\n");
    fprintf(file,
            "    .stats { display: flex; gap: 28px; flex-wrap: wrap; }\n");
    fprintf(file, "    table { width: 100%%; border-collapse: collapse; }\n");
    fprintf(file,
            "    th, td { text-align: left; padding: 10px; border-bottom: "
            "1px solid #8885; vertical-align: top; }\n");
    fprintf(file,
            "    code, pre { background: #8882; border-radius: 5px; "
            "padding: 2px 5px; } pre { white-space: pre-wrap; }\n");
    fprintf(file, "  </style>\n</head>\n<body data-status=\"%s\">\n",
            status_class);
    fprintf(file,
            "<header>\n  <h1>EPICheck %s</h1>\n"
            "  <p>Project: <code>",
            EPICHECK_VERSION);
    html_escape(file, options->root);
    fprintf(file, "</code></p>\n  <div class=\"stats\">\n");
    fprintf(file, "    <strong>%d files</strong><strong>%d functions</strong>",
            report->files, report->functions);
    fprintf(file, "<strong class=\"%s\">%s</strong>\n  </div>\n</header>\n",
            status_class, status_text);
}

static void html_summary_row(FILE *file, const char *name, const char *state,
                             const char *detail)
{
    const char *class_name;

    if (strcmp(state, "PASS") == 0)
    {
        class_name = "passed";
    }
    else if (strcmp(state, "SKIP") == 0)
    {
        class_name = "skipped";
    }
    else
    {
        class_name = "failed";
    }
    fprintf(file, "    <tr><th>");
    html_escape(file, name);
    fprintf(file, "</th><td class=\"%s\"><strong>%s</strong></td><td>",
            class_name, state);
    html_escape(file, detail);
    fprintf(file, "</td></tr>\n");
}

static void html_format_summary(FILE *file, const Options *options,
                                const Report *report)
{
    char detail[128];

    if (!options->check_format)
    {
        html_summary_row(file, "Formatting", "SKIP", "Disabled");
    }
    else if (report->format_missing)
    {
        if (options->strict)
        {
            html_summary_row(file, "Formatting", "FAIL",
                             "clang-format is not installed");
        }
        else
        {
            html_summary_row(file, "Formatting", "SKIP",
                             "clang-format is not installed");
        }
    }
    else
    {
        snprintf(detail, sizeof(detail), "%d error(s), %d fixed",
                 report->format_bad, report->format_fixed);
        if (report->format_bad > 0)
        {
            html_summary_row(file, "Formatting", "FAIL", detail);
        }
        else
        {
            html_summary_row(file, "Formatting", "PASS", detail);
        }
    }
}

static void html_compile_summary(FILE *file, const Options *options,
                                 const Report *report)
{
    char detail[128];

    if (!options->check_compile)
    {
        html_summary_row(file, "Compilation", "SKIP", "Disabled");
    }
    else if (report->compile_missing)
    {
        if (options->strict)
        {
            html_summary_row(file, "Compilation", "FAIL",
                             "Compiler is not installed");
        }
        else
        {
            html_summary_row(file, "Compilation", "SKIP",
                             "Compiler is not installed");
        }
    }
    else
    {
        snprintf(detail, sizeof(detail), "%d / %d passed",
                 report->compile_files - report->compile_bad,
                 report->compile_files);
        if (report->compile_bad > 0)
        {
            html_summary_row(file, "Compilation", "FAIL", detail);
        }
        else
        {
            html_summary_row(file, "Compilation", "PASS", detail);
        }
    }
}

static void html_summary(FILE *file, const Options *options,
                         const Report *report)
{
    char detail[128];
    const char *state;

    fprintf(file, "<section>\n  <h2>Summary</h2>\n  <table>\n");
    html_format_summary(file, options, report);
    snprintf(detail, sizeof(detail), "%d function(s)",
             report->long_functions.len);
    if (report->long_functions.len > 0)
    {
        state = "FAIL";
    }
    else
    {
        state = "PASS";
    }
    html_summary_row(file, "Function length", state, detail);
    snprintf(detail, sizeof(detail), "%d function(s)",
             report->many_arguments.len);
    if (report->many_arguments.len > 0)
    {
        state = "FAIL";
    }
    else
    {
        state = "PASS";
    }
    html_summary_row(file, "Arguments", state, detail);
    snprintf(detail, sizeof(detail), "%d / %d", report->exported,
             options->max_exported);
    if (report->exported > options->max_exported)
    {
        state = "FAIL";
    }
    else
    {
        state = "PASS";
    }
    html_summary_row(file, "Exported functions", state, detail);
    if (!options->check_ascii)
    {
        html_summary_row(file, "ASCII", "SKIP", "Disabled");
    }
    else
    {
        snprintf(detail, sizeof(detail), "%d line(s)", report->non_ascii.len);
        if (report->non_ascii.len > 0)
        {
            state = "FAIL";
        }
        else
        {
            state = "PASS";
        }
        html_summary_row(file, "ASCII", state, detail);
    }
    html_compile_summary(file, options, report);
    fprintf(file, "  </table>\n</section>\n");
}

static void html_issue_row(FILE *file, const char *rule, const Issue *issue,
                           const char *message)
{
    fprintf(file, "    <tr><td>");
    html_escape(file, rule);
    fprintf(file, "</td><td><code>");
    html_escape(file, issue->path);
    fprintf(file, "</code></td><td>%d</td><td>", issue->line);
    html_escape(file, message);
    if (issue->name[0] != '\0')
    {
        fprintf(file, "<br><small>");
        html_escape(file, issue->name);
        fprintf(file, "</small>");
    }
    if (issue->value > 0)
    {
        fprintf(file, "<br><small>Measured value: %d</small>", issue->value);
    }
    fprintf(file, "</td></tr>\n");
}

static void html_issue_list(FILE *file, const char *rule, const IssueList *list,
                            const char *message)
{
    for (int index = 0; index < list->len; index++)
    {
        html_issue_row(file, rule, &list->data[index], message);
    }
}

static int html_issue_count(const Options *options, const Report *report)
{
    int count = report->format_errors.len + report->long_functions.len
        + report->many_arguments.len + report->non_ascii.len
        + report->compile_errors.len + report->scan_errors;

    if (report->exported > options->max_exported)
    {
        count += report->exported_functions.len;
    }
    if (options->strict)
    {
        count += report->format_missing + report->compile_missing;
    }
    return count;
}

static void html_compile_issues(FILE *file, const CompileIssueList *list)
{
    for (int index = 0; index < list->len; index++)
    {
        fprintf(file, "    <tr><td>Compilation</td><td><code>");
        html_escape(file, list->data[index].path);
        fprintf(file, "</code></td><td>1</td><td><pre>");
        html_escape(file, list->data[index].output);
        fprintf(file, "</pre></td></tr>\n");
    }
}

static void html_general_issue(FILE *file, const char *rule,
                               const char *message)
{
    fprintf(file, "    <tr><td>");
    html_escape(file, rule);
    fprintf(file, "</td><td>-</td><td>-</td><td>");
    html_escape(file, message);
    fprintf(file, "</td></tr>\n");
}

static void html_issues(FILE *file, const Options *options,
                        const Report *report)
{
    fprintf(file, "<section>\n  <h2>Issues</h2>\n");
    if (html_issue_count(options, report) == 0)
    {
        fprintf(file, "  <p>No issues found.</p>\n</section>\n");
        return;
    }
    fprintf(file,
            "  <table>\n    <thead><tr><th>Rule</th><th>File</th>"
            "<th>Line</th><th>Details</th></tr></thead>\n"
            "    <tbody>\n");
    html_issue_list(file, "Formatting", &report->format_errors,
                    "File is not formatted");
    html_issue_list(file, "Function length", &report->long_functions,
                    "Function exceeds the line limit");
    html_issue_list(file, "Arguments", &report->many_arguments,
                    "Function exceeds the argument limit");
    if (report->exported > options->max_exported)
    {
        html_issue_list(file, "Exported function", &report->exported_functions,
                        "Too many exported functions");
    }
    html_issue_list(file, "ASCII", &report->non_ascii, "Non-ASCII byte found");
    html_compile_issues(file, &report->compile_errors);
    if (report->scan_errors > 0)
    {
        html_general_issue(file, "Scan", "Filesystem traversal failed");
    }
    if (options->strict && report->format_missing)
    {
        html_general_issue(file, "Formatting", "clang-format is not installed");
    }
    if (options->strict && report->compile_missing)
    {
        html_general_issue(file, "Compilation", "Compiler is not installed");
    }
    fprintf(file, "    </tbody>\n  </table>\n</section>\n");
}

static int print_html(const Options *options, const Report *report)
{
    FILE *file = fopen(HTML_REPORT_FILE, "w");

    if (file == NULL)
    {
        fprintf(stderr, "epicheck: cannot create %s\n", HTML_REPORT_FILE);
        return 0;
    }
    html_header(file, options, report);
    html_summary(file, options, report);
    html_issues(file, options, report);
    fprintf(file, "<footer><small>Generated by EPICheck %s</small></footer>\n",
            EPICHECK_VERSION);
    fprintf(file, "</body>\n</html>\n");
    if (fclose(file) != 0)
    {
        fprintf(stderr, "epicheck: cannot finish %s\n", HTML_REPORT_FILE);
        return 0;
    }
    printf("HTML report created: %s\n", HTML_REPORT_FILE);
    return 1;
}

static void github_escape(const char *text)
{
    while (*text != '\0')
    {
        if (*text == '%')
        {
            printf("%%25");
        }
        else if (*text == '\r')
        {
            printf("%%0D");
        }
        else if (*text == '\n')
        {
            printf("%%0A");
        }
        else if (*text == ',')
        {
            printf("%%2C");
        }
        else
        {
            putchar(*text);
        }
        text++;
    }
}

static void github_issue(const char *rule, const Issue *issue,
                         const char *message)
{
    printf("::error file=");
    github_escape(issue->path);
    printf(",line=%d,title=EPICheck %s::", issue->line, rule);
    github_escape(message);
    if (issue->name[0] != '\0')
    {
        printf(" (");
        github_escape(issue->name);
        printf(")");
    }
    printf("\n");
}

static void github_issue_list(const char *rule, const IssueList *list,
                              const char *message)
{
    for (int index = 0; index < list->len; index++)
    {
        github_issue(rule, &list->data[index], message);
    }
}

static void github_general_issue(const char *rule, const char *message)
{
    printf("::error title=EPICheck %s::", rule);
    github_escape(message);
    printf("\n");
}

static void print_github(const Options *options, const Report *report)
{
    const char *result;

    github_issue_list("format", &report->format_errors,
                      "file is not formatted");
    github_issue_list("function length", &report->long_functions,
                      "function exceeds the line limit");
    github_issue_list("function arguments", &report->many_arguments,
                      "function exceeds the argument limit");
    if (report->exported > options->max_exported)
    {
        github_issue_list("exports", &report->exported_functions,
                          "too many exported functions");
    }
    github_issue_list("ASCII", &report->non_ascii, "non-ASCII byte found");
    for (int index = 0; index < report->compile_errors.len; index++)
    {
        Issue issue;

        memset(&issue, 0, sizeof(issue));
        snprintf(issue.path, sizeof(issue.path), "%s",
                 report->compile_errors.data[index].path);
        issue.line = 1;
        github_issue("compile", &issue,
                     report->compile_errors.data[index].output);
    }
    if (report->scan_errors > 0)
    {
        github_general_issue("scan", "filesystem traversal failed");
    }
    if (options->strict && report->format_missing)
    {
        github_general_issue("format tool", "clang-format is not installed");
    }
    if (options->strict && report->compile_missing)
    {
        github_general_issue("compiler", "compiler is not installed");
    }
    if (report_has_errors(options, report))
    {
        result = "failed";
    }
    else
    {
        result = "passed";
    }
    printf("EPICheck: %d file(s), %d function(s), %s\n", report->files,
           report->functions, result);
}

int report_print(const Options *options, const Report *report)
{
    if (options->output == OUTPUT_JSON)
    {
        print_json(options, report);
    }
    else if (options->output == OUTPUT_HTML)
    {
        return print_html(options, report);
    }
    else if (options->output == OUTPUT_GITHUB)
    {
        print_github(options, report);
    }
    else
    {
        print_text(options, report);
    }
    return 1;
}

void report_free(Report *report)
{
    free(report->format_errors.data);
    free(report->long_functions.data);
    free(report->many_arguments.data);
    free(report->exported_functions.data);
    free(report->non_ascii.data);
    free(report->compile_errors.data);
}
