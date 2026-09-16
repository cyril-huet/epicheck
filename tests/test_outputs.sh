#!/bin/sh

run_output_tests()
{
    json_success="$TEST_ROOT/success.json"
    json_failure="$TEST_ROOT/failure.json"
    html_report="$TEST_ROOT/epicheck-report.html"
    escape_character=$(printf '\033')

    printf '%s\n' 'Output formats'

    expect_output_contains \
        'print successful text report' success 'Result: all checks passed' \
        "$EPICHECK" "$TEST_ROOT/valid" --no-format --no-compile --no-ascii

    expect_output_contains \
        'print failed text report' failure 'Result: checks failed' \
        "$EPICHECK" "$TEST_ROOT/long" --no-format --no-compile --no-ascii \
        --max-lines 2

    expect_output_not_contains \
        'disable terminal colors' "$escape_character" \
        "$EPICHECK" "$TEST_ROOT/valid" --no-format --no-compile --no-ascii \
        --no-color

    "$EPICHECK" "$TEST_ROOT/valid" --no-format --no-compile --no-ascii \
        --output json >"$json_success"

    expect_success \
        'produce valid JSON document' \
        python3 -m json.tool "$json_success"

    expect_file_contains \
        'report successful JSON result' "$json_success" '"success": true'

    "$EPICHECK" "$TEST_ROOT/long" --no-format --no-compile --no-ascii \
        --max-lines 2 --output json >"$json_failure" || true

    expect_file_contains \
        'report failed JSON result' "$json_failure" '"success": false'

    expect_file_contains \
        'include issue rule in JSON' "$json_failure" \
        '"rule": "function-length"'

    expect_output_contains \
        'announce generated HTML report' failure \
        'HTML report created: epicheck-report.html' \
        "$EPICHECK" "$TEST_ROOT/long" --no-format --no-compile --no-ascii \
        --max-lines 2 --output html

    expect_file_exists \
        'create HTML report file' "$html_report"

    expect_file_contains \
        'write HTML document type' "$html_report" '<!doctype html>'

    "$EPICHECK" "$TEST_ROOT/html&report" --no-format --no-compile \
        --no-ascii --output html >"$LAST_OUTPUT"

    expect_file_contains \
        'escape special characters in HTML' "$html_report" 'html&amp;report'

    expect_output_contains \
        'produce GitHub error annotation' failure '::error file=' \
        "$EPICHECK" "$TEST_ROOT/long" --no-format --no-compile --no-ascii \
        --max-lines 2 --output github

    printf '\n'
}
