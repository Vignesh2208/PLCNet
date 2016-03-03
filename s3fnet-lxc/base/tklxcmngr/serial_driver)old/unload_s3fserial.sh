#!/bin/sh
module="S3fserial"
device="s3fserial"

# invoke rmmod with all arguments we got
/sbin/rmmod $module $* || exit 1

# Remove stale nodes

rm -f /dev/${device} /dev/${device}[0-3] 