#!/bin/sh

MYDIR=/home/sion/temp/opendnssec

export LD_LIBRARY_PATH=$MYDIR/install/lib:/home/sion/work/subversion/ldns/lib
export SOFTHSM_CONF=$MYDIR/install/etc/opendnssec/softhsm.conf
#export ENFORCER_TIMESHIFT=now

cd $MYDIR

echo "Test1: mysql DB in conf.xml"
cp $MYDIR/home/sion/testing/lite-sion-conf.xml $MYDIR/install/etc/opendnssec/conf.xml
cp $MYDIR/home/sion/testing/kasp.xml $MYDIR/install/etc/opendnssec/kasp.xml
cp $MYDIR/home/sion/testing/sion-softhsm.conf $MYDIR/install/etc/opendnssec/softhsm.conf
cp $MYDIR/home/sion/testing/sion-zonelist.xml $MYDIR/install/etc/opendnssec/zonelist.xml

$MYDIR/install/bin/softhsm --init-token --slot 0 --label "alice"
$MYDIR/install/bin/softhsm --init-token --slot 1 --label "alice2"

#Import the xml config from $MYDIR in to the DB
$MYDIR/install/bin/ksmutil setup

#Start the enforcerd
$MYDIR/install/sbin/enforcerd -d &

sleep 10

$MYDIR/install/bin/ksmutil backup done

killall -HUP enforcerd

sleep 10
killall -INT enforcerd
echo "Look in the system logs and see what happened."

