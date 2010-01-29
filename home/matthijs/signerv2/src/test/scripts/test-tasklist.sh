#!/bin/bash
#?@shell@
# test-adapter.sh -- test several adapters.
# Copyright (c) 2009, NLnet Labs. All rights reserved.
# See LICENSE for the license.

ORIGDIR="`pwd`"

cd ..

./test-tasklist aap noot mies koala.aap wal.noot peper.noot > test-tasklist.out

if wdiff test-tasklist.out data/taskqueue.known_good &> /dev/null; then
    echo parsing tasklist ok
    rm -f test-tasklist.out
    cd $ORIGDIR
    exit 0
fi

echo parsing tasklist failed
wdiff test-tasklist.out data/taskqueue.known_good
rm -f test-tasklist.out
cd $ORIGDIR
exit 1
