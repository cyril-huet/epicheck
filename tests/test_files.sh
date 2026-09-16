#!/bin/sh

run_file_tests()
{
    printf '%s\n' 'Files and directories'

    expect_success \
        'accept empty directory' \
        "$EPICHECK" "$TEST_ROOT/empty" --no-format --no-compile --no-ascii

    expect_output_contains \
        'scan nested directories' success 'Files: 1, functions: 1' \
        "$EPICHECK" "$TEST_ROOT/nested" --no-format --no-compile --no-ascii

    expect_success \
        'ignore symbolic link loop' \
        "$EPICHECK" "$TEST_ROOT/symlink" --no-format --no-compile --no-ascii

    expect_failure \
        'reject missing project path' \
        "$EPICHECK" "$TEST_ROOT/does-not-exist" --no-format --no-compile \
        --no-ascii

    printf '\n'
}
