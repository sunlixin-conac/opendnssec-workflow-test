#!/bin/bash
#?@shell@
# test-adapter.sh -- test several adapters.
# Copyright (c) 2009, NLnet Labs. All rights reserved.
# See LICENSE for the license.

ORIGDIR="`pwd`"

cd ..
./test-zonelist data/zonelist.xml > test-zonelist.out

if wdiff test-zonelist.out data/zonelist.xml &> /dev/null; then
    echo parsing zonelist ok
    rm -f test-zonelist.out
    cd $ORIGDIR
    exit 0
fi

echo parsing zonelist failed
wdiff test-zonelist.out data/zonelist.xml
rm -f test-zonelist.out
cd $ORIGDIR
exit 1
