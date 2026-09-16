#!/bin/sh

run_hook_tests()
{
    git_repository="$TEST_ROOT/git-project"
    hook_file="$git_repository/.git/hooks/pre-push"

    git init -q "$git_repository"

    printf '%s\n' 'Git pre-push hook'

    expect_failure \
        'reject hook outside Git repository' \
        "$EPICHECK" "$TEST_ROOT/not-git" --install-hook

    expect_success \
        'install hook inside Git repository' \
        "$EPICHECK" "$git_repository" --install-hook

    expect_file_executable \
        'make installed hook executable' "$hook_file"

    expect_file_contains \
        'run strict checks from installed hook' "$hook_file" '--strict'

    printf '\n'
}
