#!/bin/sh

TEST_COUNT=0
TEST_PASSED=0
TEST_FAILED=0
EXPECTED_TESTS=50
LAST_OUTPUT="$TEST_ROOT/last-output.txt"

test_passed()
{
    name=$1
    TEST_COUNT=$((TEST_COUNT + 1))
    TEST_PASSED=$((TEST_PASSED + 1))
    printf '[%02d/%02d] PASS %s\n' "$TEST_COUNT" "$EXPECTED_TESTS" "$name"
}

test_failed()
{
    name=$1
    TEST_COUNT=$((TEST_COUNT + 1))
    TEST_FAILED=$((TEST_FAILED + 1))
    printf '[%02d/%02d] FAIL %s\n' "$TEST_COUNT" "$EXPECTED_TESTS" "$name"
    if [ -f "$LAST_OUTPUT" ]
    then
        sed 's/^/           /' "$LAST_OUTPUT"
    fi
}

expect_success()
{
    name=$1
    shift

    if "$@" >"$LAST_OUTPUT" 2>&1
    then
        test_passed "$name"
    else
        test_failed "$name"
    fi
}

expect_failure()
{
    name=$1
    shift

    if "$@" >"$LAST_OUTPUT" 2>&1
    then
        printf '%s\n' 'command unexpectedly succeeded' >"$LAST_OUTPUT"
        test_failed "$name"
    else
        test_passed "$name"
    fi
}

expect_output_contains()
{
    name=$1
    expected_result=$2
    pattern=$3
    shift
    shift
    shift

    "$@" >"$LAST_OUTPUT" 2>&1
    status=$?
    result_is_correct=0

    if [ "$expected_result" = "success" ] && [ "$status" -eq 0 ]
    then
        result_is_correct=1
    fi
    if [ "$expected_result" = "failure" ] && [ "$status" -ne 0 ]
    then
        result_is_correct=1
    fi
    if [ "$result_is_correct" -eq 1 ] \
        && grep -Fq -- "$pattern" "$LAST_OUTPUT"
    then
        test_passed "$name"
    else
        test_failed "$name"
    fi
}

expect_output_not_contains()
{
    name=$1
    pattern=$2
    shift
    shift

    "$@" >"$LAST_OUTPUT" 2>&1
    status=$?
    if [ "$status" -eq 0 ] && ! grep -Fq -- "$pattern" "$LAST_OUTPUT"
    then
        test_passed "$name"
    else
        test_failed "$name"
    fi
}

expect_file_contains()
{
    name=$1
    file=$2
    pattern=$3

    if [ -f "$file" ] && grep -Fq -- "$pattern" "$file"
    then
        test_passed "$name"
    else
        printf 'missing pattern: %s\nfile: %s\n' "$pattern" "$file" \
            >"$LAST_OUTPUT"
        test_failed "$name"
    fi
}

expect_file_exists()
{
    name=$1
    file=$2

    if [ -f "$file" ]
    then
        test_passed "$name"
    else
        printf 'missing file: %s\n' "$file" >"$LAST_OUTPUT"
        test_failed "$name"
    fi
}

expect_file_executable()
{
    name=$1
    file=$2

    if [ -x "$file" ]
    then
        test_passed "$name"
    else
        printf 'file is not executable: %s\n' "$file" >"$LAST_OUTPUT"
        test_failed "$name"
    fi
}

finish_tests()
{
    printf '\n%d/%d tests passed.\n' "$TEST_PASSED" "$TEST_COUNT"

    if [ "$TEST_COUNT" -ne "$EXPECTED_TESTS" ]
    then
        printf 'Expected %d tests, but %d ran.\n' "$EXPECTED_TESTS" \
            "$TEST_COUNT" >&2
        return 1
    fi
    if [ "$TEST_FAILED" -ne 0 ]
    then
        return 1
    fi
    return 0
}
