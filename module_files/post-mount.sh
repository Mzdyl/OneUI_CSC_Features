#!/system/bin/sh

MODDIR=${0%/*}
LOG_FILE="$MODDIR/log.txt"
CSC=$(getprop ro.boot.sales_code)

. "$MODDIR/rebind_mounts.sh"

rebind_all_targets "post-mount"
