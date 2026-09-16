#!/bin/sh

run_cli_tests()
{
    printf '%s\n' 'Command-line options'

    expect_output_contains \
        'display long help' success 'Usage:' \
        "$EPICHECK" --help

    expect_output_contains \
        'display short help' success 'Usage:' \
        "$EPICHECK" -h

    expect_output_contains \
        'display version' success 'epicheck 1.1.0' \
        "$EPICHECK" --version

    expect_failure \
        'reject unknown option' \
        "$EPICHECK" --unknown-option

    expect_failure \
        'reject unknown output format' \
        "$EPICHECK" --output xml

    expect_failure \
        'reject missing option value' \
        "$EPICHECK" --output

    expect_failure \
        'reject zero line limit' \
        "$EPICHECK" --max-lines 0

    expect_failure \
        'reject negative argument limit' \
        "$EPICHECK" --max-args -1

    expect_failure \
        'reject invalid export limit' \
        "$EPICHECK" --max-exported many

    expect_failure \
        'reject two project paths' \
        "$EPICHECK" "$TEST_ROOT/valid" "$TEST_ROOT/cpp"

    expect_failure \
        'reject removed config option' \
        "$EPICHECK" --config settings.conf

    expect_success \
        'accept options before project path' \
        "$EPICHECK" --no-format --no-compile --no-ascii \
        "$TEST_ROOT/valid"

    printf '\n'
}
