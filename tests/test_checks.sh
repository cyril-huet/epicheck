#!/bin/sh

run_check_tests()
{
    printf '%s\n' 'Project checks'

    expect_success \
        'accept valid C source' \
        "$EPICHECK" "$TEST_ROOT/valid" --no-format --no-compile --no-ascii

    expect_success \
        'compile valid C source' \
        "$EPICHECK" "$TEST_ROOT/valid" --no-format --compile --no-ascii

    expect_success \
        'compile valid C++ source' \
        "$EPICHECK" "$TEST_ROOT/cpp" --no-format --compile --no-ascii

    expect_failure \
        'detect invalid C source' \
        "$EPICHECK" "$TEST_ROOT/invalid" --no-format --compile --no-ascii

    expect_failure \
        'detect non-ASCII character' \
        "$EPICHECK" "$TEST_ROOT/non-ascii" --no-format --no-compile --ascii

    expect_success \
        'disable ASCII check' \
        "$EPICHECK" "$TEST_ROOT/non-ascii" --no-format --no-compile \
        --no-ascii

    expect_failure \
        'detect long function' \
        "$EPICHECK" "$TEST_ROOT/long" --no-format --no-compile --no-ascii \
        --max-lines 2

    expect_success \
        'accept function under higher line limit' \
        "$EPICHECK" "$TEST_ROOT/long" --no-format --no-compile --no-ascii \
        --max-lines 10

    expect_failure \
        'detect too many function arguments' \
        "$EPICHECK" "$TEST_ROOT/arguments" --no-format --no-compile \
        --no-ascii --max-args 2

    expect_success \
        'accept function under higher argument limit' \
        "$EPICHECK" "$TEST_ROOT/arguments" --no-format --no-compile \
        --no-ascii --max-args 3

    expect_failure \
        'detect too many exported functions' \
        "$EPICHECK" "$TEST_ROOT/exports" --no-format --no-compile \
        --no-ascii --max-exported 2

    expect_success \
        'apply export limit to each file' \
        "$EPICHECK" "$TEST_ROOT/exports" --no-format --no-compile \
        --no-ascii --max-exported 3

    expect_success \
        'use custom C compiler flags' \
        "$EPICHECK" "$TEST_ROOT/cflags" --no-format --no-ascii \
        --cflags '-DFIRST_VALUE=20 -DSECOND_VALUE=22'

    expect_success \
        'use custom C++ compiler flags' \
        "$EPICHECK" "$TEST_ROOT/cxxflags" --no-format --no-ascii \
        --cxxflags '-DCPP_VALUE=42'

    expect_success \
        'exclude one matching directory' \
        "$EPICHECK" "$TEST_ROOT/exclude" --no-format --no-ascii \
        --exclude ignored

    expect_success \
        'exclude multiple matching directories' \
        "$EPICHECK" "$TEST_ROOT/multiple-excludes" --no-format --no-ascii \
        --exclude first --exclude second

    expect_failure \
        'detect formatting error' \
        env PATH="$TEST_ROOT/fake-bin:$PATH" "$EPICHECK" \
        "$TEST_ROOT/format" --format --no-compile --no-ascii

    expect_success \
        'fix formatting error' \
        env PATH="$TEST_ROOT/fake-bin:$PATH" "$EPICHECK" \
        "$TEST_ROOT/format" --format --no-compile --no-ascii --fix

    printf '\n'
}
