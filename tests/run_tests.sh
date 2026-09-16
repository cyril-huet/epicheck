#!/bin/sh

set -u

SCRIPT_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd)
EPICHECK_INPUT=${1:-./epicheck}
EPICHECK_DIR=$(dirname "$EPICHECK_INPUT")
EPICHECK_NAME=$(basename "$EPICHECK_INPUT")
EPICHECK=$(CDPATH= cd "$EPICHECK_DIR" && pwd)/$EPICHECK_NAME
TEST_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/epicheck-tests.XXXXXX")

cleanup()
{
    rm -rf "$TEST_ROOT"
}

trap cleanup EXIT HUP INT TERM

if [ ! -x "$EPICHECK" ]
then
    printf 'epicheck test error: executable not found: %s\n' "$EPICHECK" >&2
    exit 2
fi

. "$SCRIPT_DIR/common.sh"
. "$SCRIPT_DIR/fixtures.sh"
. "$SCRIPT_DIR/test_cli.sh"
. "$SCRIPT_DIR/test_checks.sh"
. "$SCRIPT_DIR/test_outputs.sh"
. "$SCRIPT_DIR/test_files.sh"
. "$SCRIPT_DIR/test_hook.sh"

create_fixtures
cd "$TEST_ROOT" || exit 2

printf 'Running EPICheck integration tests\n\n'

run_cli_tests
run_check_tests
run_output_tests
run_file_tests
run_hook_tests

finish_tests
