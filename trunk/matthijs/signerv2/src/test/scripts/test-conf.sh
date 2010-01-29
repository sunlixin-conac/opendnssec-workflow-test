#!/bin/bash
#?@shell@
# test-adapter.sh -- test several adapters.
# Copyright (c) 2009, NLnet Labs. All rights reserved.
# See LICENSE for the license.

ORIGDIR="`pwd`"

cd ..
./test-conf data/conf.xml > test-conf.out

if wdiff test-conf.out data/conf.xml &> /dev/null; then
    echo parsing conf ok
    rm -f test-conf.out
    cd $ORIGDIR
    exit 0
fi

echo parsing conf failed
wdiff test-conf.out data/conf.xml
rm -f test-conf.out
cd $ORIGDIR
exit 1
