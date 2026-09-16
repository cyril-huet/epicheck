#!/bin/sh

write_valid_c()
{
    file=$1
    printf '%s\n' \
        'int answer(void)' \
        '{' \
        '    return 42;' \
        '}' >"$file"
}

write_invalid_c()
{
    file=$1
    printf '%s\n' \
        'int broken(void)' \
        '{' \
        '    return ; this is invalid' \
        '}' >"$file"
}

create_fake_clang_format()
{
    mkdir -p "$TEST_ROOT/fake-bin"
    cat >"$TEST_ROOT/fake-bin/clang-format" <<'EOF'
#!/bin/sh

if [ "$1" = "--dry-run" ]
then
    if grep -q 'BAD_FORMAT' "$3"
    then
        exit 1
    fi
    exit 0
fi

if [ "$1" = "-i" ]
then
    temporary="$2.tmp"
    sed 's/BAD_FORMAT/FORMATTED/' "$2" >"$temporary"
    mv "$temporary" "$2"
    exit 0
fi

exit 1
EOF
    chmod +x "$TEST_ROOT/fake-bin/clang-format"
}

create_fixtures()
{
    mkdir -p "$TEST_ROOT/valid"
    mkdir -p "$TEST_ROOT/cpp"
    mkdir -p "$TEST_ROOT/invalid"
    mkdir -p "$TEST_ROOT/non-ascii"
    mkdir -p "$TEST_ROOT/long"
    mkdir -p "$TEST_ROOT/arguments"
    mkdir -p "$TEST_ROOT/exports"
    mkdir -p "$TEST_ROOT/cflags"
    mkdir -p "$TEST_ROOT/cxxflags"
    mkdir -p "$TEST_ROOT/exclude/ignored"
    mkdir -p "$TEST_ROOT/multiple-excludes/first"
    mkdir -p "$TEST_ROOT/multiple-excludes/second"
    mkdir -p "$TEST_ROOT/format"
    mkdir -p "$TEST_ROOT/empty"
    mkdir -p "$TEST_ROOT/nested/one/two"
    mkdir -p "$TEST_ROOT/symlink"
    mkdir -p "$TEST_ROOT/not-git"
    mkdir -p "$TEST_ROOT/html&report"

    write_valid_c "$TEST_ROOT/valid/valid.c"
    write_invalid_c "$TEST_ROOT/invalid/invalid.c"

    printf '%s\n' \
        'int cpp_answer()' \
        '{' \
        '    return 42;' \
        '}' >"$TEST_ROOT/cpp/valid.cpp"

    printf '/* café */\nint value(void) { return 1; }\n' \
        >"$TEST_ROOT/non-ascii/non_ascii.c"

    printf '%s\n' \
        'int long_function(void)' \
        '{' \
        '    int first = 10;' \
        '    int second = 20;' \
        '    int third = 12;' \
        '    return first + second + third;' \
        '}' >"$TEST_ROOT/long/long.c"

    printf '%s\n' \
        'int many_arguments(int first, int second, int third)' \
        '{' \
        '    return first + second + third;' \
        '}' >"$TEST_ROOT/arguments/arguments.c"

    printf '%s\n' \
        'int first_export(void) { return 1; }' \
        'int second_export(void) { return 2; }' \
        'int third_export(void) { return 3; }' \
        >"$TEST_ROOT/exports/first.c"

    printf '%s\n' \
        'int fourth_export(void) { return 4; }' \
        'int fifth_export(void) { return 5; }' \
        'int sixth_export(void) { return 6; }' \
        >"$TEST_ROOT/exports/second.c"

    printf '%s\n' \
        '#ifndef FIRST_VALUE' \
        '# error FIRST_VALUE is missing' \
        '#endif' \
        '#ifndef SECOND_VALUE' \
        '# error SECOND_VALUE is missing' \
        '#endif' \
        'int flags_work(void) { return FIRST_VALUE + SECOND_VALUE; }' \
        >"$TEST_ROOT/cflags/flags.c"

    printf '%s\n' \
        '#ifndef CPP_VALUE' \
        '# error CPP_VALUE is missing' \
        '#endif' \
        'int cpp_flags_work() { return CPP_VALUE; }' \
        >"$TEST_ROOT/cxxflags/flags.cpp"

    write_valid_c "$TEST_ROOT/exclude/valid.c"
    write_invalid_c "$TEST_ROOT/exclude/ignored/invalid.c"

    write_valid_c "$TEST_ROOT/multiple-excludes/valid.c"
    write_invalid_c "$TEST_ROOT/multiple-excludes/first/invalid.c"
    write_invalid_c "$TEST_ROOT/multiple-excludes/second/invalid.c"

    printf '%s\n' \
        '/* BAD_FORMAT */' \
        'int formatted(void)' \
        '{' \
        '    return 1;' \
        '}' >"$TEST_ROOT/format/format.c"

    write_valid_c "$TEST_ROOT/nested/one/two/nested.c"
    write_valid_c "$TEST_ROOT/symlink/valid.c"
    ln -s . "$TEST_ROOT/symlink/loop"
    write_valid_c "$TEST_ROOT/html&report/valid.c"

    create_fake_clang_format
}
