#!/bin/bash
#?@shell@
# test-adapter.sh -- test several adapters.
# Copyright (c) 2009, NLnet Labs. All rights reserved.
# See LICENSE for the license.

ORIGDIR="`pwd`"

cd ..
./test-signconf data/signconf.xml > test-signconf.out

if wdiff test-signconf.out data/signconf.xml &> /dev/null; then
    echo parsing signconf ok
    rm -f test-signconf.out
    cd $ORIGDIR
    exit 0
fi

echo parsing signconf failed
wdiff test-signconf.out data/signconf.xml
rm -f test-signconf.out
cd $ORIGDIR
exit 1
