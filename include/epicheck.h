#ifndef EPICHECK_H
#define EPICHECK_H

#include <limits.h>
#include <stddef.h>

#ifndef PATH_MAX
#    define PATH_MAX 4096
#endif

/* Project limits */
#define EPICHECK_VERSION "1.1.0"
#define MAX_NAME 128
#define MAX_OUTPUT 4096
#define MAX_FLAGS 1024
#define MAX_EXCLUDES 64

/* Available report formats */
typedef enum
{
    OUTPUT_TEXT,
    OUTPUT_JSON,
    OUTPUT_HTML,
    OUTPUT_GITHUB
} OutputFormat;

/* Issue found while checking a source file */
typedef struct
{
    char path[PATH_MAX];
    char name[MAX_NAME];
    int line;
    int value;
} Issue;

/* Dynamic list of source issues */
typedef struct
{
    Issue *data;
    int len;
    int cap;
} IssueList;

/* Compilation error and compiler output */
typedef struct
{
    char path[PATH_MAX];
    char output[MAX_OUTPUT];
} CompileIssue;

/* Dynamic list of compilation errors */
typedef struct
{
    CompileIssue *data;
    int len;
    int cap;
} CompileIssueList;

/* Options selected from the command line */
typedef struct
{
    const char *root;
    int fix;
    int check_format;
    int check_ascii;
    int check_compile;
    int color;
    int install_hook;
    int strict;
    int max_lines;
    int max_args;
    int max_exported;
    OutputFormat output;
    char cflags[MAX_FLAGS];
    char cxxflags[MAX_FLAGS];
    char excludes[MAX_EXCLUDES][PATH_MAX];
    int exclude_count;
} Options;

/* Results collected while checking a project */
typedef struct
{
    int files;
    int functions;
    int format_bad;
    int format_fixed;
    int format_missing;
    int exported;
    int compile_files;
    int compile_bad;
    int compile_missing;
    int scan_errors;
    IssueList format_errors;
    IssueList long_functions;
    IssueList many_arguments;
    IssueList exported_functions;
    IssueList non_ascii;
    CompileIssueList compile_errors;
} Report;

/* Command-line options */
void options_init(Options *options);
int options_parse(int argc, char **argv, Options *options);
void print_usage(const char *program);

/* Project analysis */
void scan_project(const Options *options, Report *report);
void analyze_source(const char *path, const Options *options, Report *report);
int install_hook(const char *program, const Options *options);

/* Report output */
int report_print(const Options *options, const Report *report);
int report_has_errors(const Options *options, const Report *report);
void report_free(Report *report);

/* Shared utilities */
void issue_add(IssueList *list, const char *path, const char *name, int line,
               int value);
void compile_issue_add(CompileIssueList *list, const char *path,
                       const char *output);
char *trim_string(char *string);
int string_ends_with(const char *string, const char *suffix);
int source_file(const char *path);
int implementation_file(const char *path);
int cpp_file(const char *path);
int executable_available(const char *name);
int run_process(char *const argv[], char *output, size_t output_size);

#endif
