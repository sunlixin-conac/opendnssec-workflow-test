#!/bin/sh
#
# purge-unused-keys.sh - Removes keys from SoftHSM that is not used by OpenDNSSEC
#
# Author Jerry Lundstrom <jerry@opendnssec.org>
#
# Copyright (c) 2012 OpenDNSSEC AB (svb). All rights reserved.
# Copyright (c) 2012 .SE (The Internet Infrastructure Foundation).
#               All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

mymktemp ()
{
	local tempfile="/tmp/$0.$$-$1"
	if [ -e "$tempfile" ]; then
		echo "Temporary file already exists: $tempfile" >&2
		exit 1
	fi
	if ! touch -- "$tempfile" 2>/dev/null; then
        	echo "Unable to create temporary file: $tempfile" >&2
        	exit 1
	fi
	echo "$tempfile"
}

if [ -z "$1" ]; then
	echo "usage: $0 <softhsm module>"
	exit 0
fi

SOFTHSM_MODULE="$1"
if [ ! -r "$SOFTHSM_MODULE" ]; then
	echo "Can't access softhsm module, no read permission" >&2
	exit 1
fi

echo ""
echo "WARNING: This will remove keys that are not used in OpenDNSSEC from SoftHSM"
echo "         Be sure that OpenDNSSEC is not running and that you have backed up"
echo "         both OpenDNSSEC database and SoftHSM database."
echo ""
echo -n "I wish to continue [yes/NO]: "

read in
if [ "$in" != "yes" ]; then
	exit 0
fi

PKCS11TOOL=`which pkcs11-tool 2>/dev/null`
if [ ! -x "$PKCS11TOOL" ]; then
	echo "Can't find pkcs11-tool or no permission to execute" >&2
	exit 1
fi

ODSKSMUTIL=`which ods-ksmutil 2>/dev/null`
if [ ! -x "$ODSKSMUTIL" ]; then
	echo "Can't find ods-ksmutil or no permission to execute" >&2
	exit 1
fi

PERL=`which perl 2>/dev/null`
if [ ! -x "$PERL" ]; then
	echo "Can't find perl or no permission to execute" >&2
	exit 1
fi

echo -n "SoftHSM slot to purge: "
read SLOT
if [ "$SLOT" -lt 0 ] 2>/dev/null; then
	echo "No slot or invalid slot given, exiting" >&2
	exit 1
fi

echo -n "PIN: "
read PIN
if [ "$PIN" = "" ]; then
	echo "No pin given, exiting" >&2
	exit 1
fi

KEYS_USED=`mymktemp keys-used`
KEYS_USED_RAW=`mymktemp keys-used-raw`
KEYS_RAW=`mymktemp keys-raw`
KEYS=`mymktemp keys`
KEYS_RM=`mymktemp keys-rm`

echo -n "Fetching keys used by OpenDNSSEC ... "
if ! "$ODSKSMUTIL" key list --verbose >"$KEYS_USED_RAW" 2>/dev/null; then
	echo "Unable to get keys from OpenDNSSEC" >&2
	exit 1
fi

if ! "$PERL" -ne 'if(/([a-z0-9]{32})/o){print "$1\n";}' "$KEYS_USED_RAW" >"$KEYS_USED" 2>/dev/null; then
	echo "Unable to filter out key ids from OpenDNSSEC key list" >&2
	exit 1
fi

KEYS_USED_CNT=`wc -l -- "$KEYS_USED" | awk '{print $1}'`
echo "found $KEYS_USED_CNT keys used."

echo -n "Fetching keys in SoftHSM ... "
if ! "$PKCS11TOOL" --module "$SOFTHSM_MODULE" --slot "$SLOT" --pin "$PIN" -O >"$KEYS_RAW" 2>/dev/null; then
	echo "Unable to get keys from SoftHSM" >&2
	exit 1
fi

if ! "$PERL" -ne 'if(/ID:\s+([a-z0-9]{32})/o){print "$1\n";}' "$KEYS_RAW" >"$KEYS" 2>/dev/null; then
	echo "Unable to filter out key ids from SoftHSM keys" >&2
	exit 1
fi

KEYS_CNT=`wc -l -- "$KEYS" | awk '{print $1}'`
echo "found $KEYS_CNT keys."

echo -n "Compiling list of keys to be removed ... "
if ! "$PERL" -e 'use strict; \
my %used;
exit(1) unless (open(FILE, $ARGV[0]));
while(<FILE>) {
	chomp;
	$used{$_}=1;
}
close(FILE);
exit(1) unless (open(FILE, $ARGV[1]));
while(<FILE>) {
	chomp;
	print "$_\n" unless(exists $used{$_});
}
close(FILE);
exit(0);' "$KEYS_USED" "$KEYS" >"$KEYS_RM" ; then
	echo "Unable to compile list of keys to be removed" >&2
	exit 1
fi

KEYS_RM_CNT=`wc -l -- "$KEYS_RM" | awk '{print $1}'`
echo "$KEYS_RM_CNT keys."

echo -n "Removing keys ... "

while read KEY; do
	if ! "$PKCS11TOOL" --module "$SOFTHSM_MODULE" --slot "$SLOT" --pin "$PIN" -b --type privkey --id "$KEY"; then
		echo "Unable to delete key $KEY" >&2
		exit 1
	fi
done <"$KEYS_RM"
echo "done."

rm -- "$KEYS_USED" "$KEYS_USED_RAW" "$KEYS_RAW" "$KEYS" "$KEYS_RM"
exit 0

