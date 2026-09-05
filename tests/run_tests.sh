#!/bin/sh
# Test suite for futoshiki-solver.
#
# Runs the solver against the example puzzles and several invalid inputs,
# checking exit codes and output. Exits non-zero on the first failure.
#
# Usage: make test   (or: ./tests/run_tests.sh [path-to-binary])

set -u

# --- Setup -----------------------------------------------------------------

BINARY="${1:-./futoshiki}"
TESTS_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
EXAMPLES_DIR="$TESTS_DIR/../examples"

pass_count=0
fail_count=0

# check <name> <expected-exit> <substring-in-output> <command...>
check() {
    name="$1"
    expected_exit="$2"
    expected_output="$3"
    shift 3

    actual_output=$("$@" 2>&1)
    actual_exit=$?

    if [ "$actual_exit" -eq "$expected_exit" ] &&
        printf '%s' "$actual_output" | grep -qF "$expected_output"; then
        pass_count=$((pass_count + 1))
        printf 'ok   %s\n' "$name"
    else
        fail_count=$((fail_count + 1))
        printf 'FAIL %s\n' "$name"
        printf '     expected exit %s, got %s\n' "$expected_exit" "$actual_exit"
        printf '     expected output containing: %s\n' "$expected_output"
        printf '     actual output: %s\n' "$(printf '%s' "$actual_output" | head -3)"
    fi
}

# --- Solver tests ----------------------------------------------------------

# Each example must be solved (exit 0) and respect its givens.
check "example puzzle1 solves" 0 "Solved" \
    "$BINARY" --quiet --seed 42 "$EXAMPLES_DIR/puzzle1.fut"
check "example puzzle2 solves" 0 "Solved" \
    "$BINARY" --quiet --seed 42 "$EXAMPLES_DIR/puzzle2.fut"
check "example puzzle3 solves" 0 "Solved" \
    "$BINARY" --quiet --seed 42 "$EXAMPLES_DIR/puzzle3.fut"

# Givens must survive the search. In the boxed output, line 4 is grid row 0:
# puzzle1 has a given 5 at (0,4) -> row 0 must end with "| 5 |";
# puzzle2 has a given 4 at (0,0) -> row 0 must start with "| 4 |".
puzzle1_output=$("$BINARY" -q -s 42 "$EXAMPLES_DIR/puzzle1.fut" 2>&1)
if printf '%s\n' "$puzzle1_output" | sed -n '4p' | grep -qE '\| 5 \|$'; then
    pass_count=$((pass_count + 1))
    printf 'ok   %s\n' "example puzzle1 keeps given"
else
    fail_count=$((fail_count + 1))
    printf 'FAIL %s\n' "example puzzle1 keeps given"
fi
puzzle2_output=$("$BINARY" -q -s 42 "$EXAMPLES_DIR/puzzle2.fut" 2>&1)
if printf '%s\n' "$puzzle2_output" | sed -n '4p' | grep -qE '^\| 4 \|'; then
    pass_count=$((pass_count + 1))
    printf 'ok   %s\n' "example puzzle2 keeps given"
else
    fail_count=$((fail_count + 1))
    printf 'FAIL %s\n' "example puzzle2 keeps given"
fi

# Reading from stdin must behave like reading the file.
check "stdin input works" 0 "Solved" \
    sh -c '"$1" --quiet --seed 42 < "$2"' _ "$BINARY" "$EXAMPLES_DIR/puzzle1.fut"

# --seed must make runs reproducible.
reproducible_a=$("$BINARY" -q -s 42 "$EXAMPLES_DIR/puzzle1.fut" 2>&1)
reproducible_b=$("$BINARY" -q -s 42 "$EXAMPLES_DIR/puzzle1.fut" 2>&1)
if [ "$reproducible_a" = "$reproducible_b" ] && [ -n "$reproducible_a" ]; then
    pass_count=$((pass_count + 1))
    printf 'ok   %s\n' "seed is reproducible"
else
    fail_count=$((fail_count + 1))
    printf 'FAIL %s\n' "seed is reproducible"
fi

# --- Validation tests ------------------------------------------------------

check "rejects out-of-range constraint" 1 "coordinates out of range" \
    sh -c 'printf "gt 9 0 0 0\n" | "$1" -q' _ "$BINARY"
check "rejects self-comparing constraint" 1 "cannot compare a cell with itself" \
    sh -c 'printf "gt 1 1 1 1\n" | "$1" -q' _ "$BINARY"
check "rejects unknown directive" 1 "unknown directive" \
    sh -c 'printf "lt 0 0 0 1\n" | "$1" -q' _ "$BINARY"
check "rejects duplicate fixed cell" 1 "duplicate fixed cell" \
    sh -c 'printf "fixed 0 0 3\nfixed 0 0 4\n" | "$1" -q' _ "$BINARY"
check "rejects same row+value givens" 1 "same row and value" \
    sh -c 'printf "fixed 0 0 3\nfixed 0 1 3\n" | "$1" -q' _ "$BINARY"
check "rejects out-of-range fixed value" 1 "fixed value must be in the range" \
    sh -c 'printf "fixed 0 0 9\n" | "$1" -q' _ "$BINARY"
check "rejects missing file" 1 "cannot open" \
    "$BINARY" -q /nonexistent/puzzle.fut
check "rejects unknown option" 1 "unknown option" \
    "$BINARY" --bogus

# --- Summary ---------------------------------------------------------------

printf '\n%d passed, %d failed\n' "$pass_count" "$fail_count"
[ "$fail_count" -eq 0 ]
