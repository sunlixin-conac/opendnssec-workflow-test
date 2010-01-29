#!/bin/bash
#?@shell@
# test-adapter.sh -- test several adapters.
# Copyright (c) 2009, NLnet Labs. All rights reserved.
# See LICENSE for the license.

VALGRIND="valgrind --show-reachable=yes --leak-check=full"
PIDFILE="/opt/var/run/opendnssec/signerengine.pid"

$VALGRIND ../../ods-signerd -vvvvvvv &> test-valgrind.out
sleep $1
if kill -TERM `cat $PIDFILE`; then

if grep "ERROR SUMMARY: 0 errors from 0 contexts" test-valgrind.out &> /dev/null; then
    echo valgrind no errors
    if grep "All heap blocks were freed -- no leaks are possible" test-valgrind.out &> /dev/null; then
        echo valgrind no memory leaks
        rm -f test-valgrind.out
        exit 0
    else
        echo valgrind memory leaks
        cat test-valgrind.out
        rm -f test-valgrind.out
        exit 1
    fi
else
    echo valgrind errors:
    cat test-valgrind.out
    rm -f test-valgrind.out
    exit 1
fi

fi

echo kill failed
cat test-valgrind.out
rm -f test-valgrind.out
exit 1
