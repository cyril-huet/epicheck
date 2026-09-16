#include <stdio.h>
#include <string.h>

#include "epicheck.h"

int main(int argc, char **argv)
{
    Options options;
    Report report;
    int failed;

    options_init(&options);
    if (!options_parse(argc, argv, &options))
    {
        return 2;
    }
    if (options.install_hook)
    {
        return install_hook(argv[0], &options);
    }

    memset(&report, 0, sizeof(report));
    scan_project(&options, &report);
    failed = report_has_errors(&options, &report);
    if (!report_print(&options, &report))
    {
        report_free(&report);
        return 2;
    }
    report_free(&report);
    if (failed)
    {
        return 1;
    }
    return 0;
}
