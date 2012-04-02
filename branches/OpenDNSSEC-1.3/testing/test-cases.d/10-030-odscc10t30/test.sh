#!/usr/bin/env bash
#
# Configure no module location and expect failure

ods_reset_env &&

! log_this ods-control-start ods-control start &&
syslog_waitfor 10 'ods-enforcerd: .*PKCS#11 module load failed' &&
syslog_waitfor 10 'ods-signerd: .*\[hsm\].*PKCS#11 module load failed' &&
return 0

ods_kill
return 1
