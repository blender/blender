#!/bin/sh

# Run in valgrind, with leak checking enabled

valgrind -q --leak-check=full "$@" 2> .valgrind-log

# Save the test result

result="$?"

# Valgrind should generate no error messages

log_contents="`cat .valgrind-log`"

if [ "$log_contents" != "" ]; then
        cat .valgrind-log >&2
        result=1
fi

rm -f .valgrind-log

exit $result
