#!/bin/sh

# Keep the old entry point used by the Makefile.
SCRIPT_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd)

exec sh "$SCRIPT_DIR/run_tests.sh" "$@"
