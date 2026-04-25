#!/system/bin/sh

MODDIR=${0%/*}
LOG_FILE="$MODDIR/log.txt"
CSC=$(getprop ro.boot.sales_code)
LOCK_FILE="$MODDIR/.mount_guard.lock"

. "$MODDIR/rebind_mounts.sh"

if [ -f "$LOCK_FILE" ]; then
    old_pid=$(cat "$LOCK_FILE" 2>/dev/null)
    if [ -n "$old_pid" ] && kill -0 "$old_pid" 2>/dev/null; then
        exit 0
    fi
fi

echo $$ > "$LOCK_FILE"
trap 'rm -f "$LOCK_FILE"' EXIT

log_mount "=== 挂载守护开始 pid=$$ ==="

round=0
while [ "$round" -lt 180 ]; do
    ensure_all_targets
    sleep 2
    round=$((round + 1))
done

log_mount "=== 挂载守护结束 pid=$$ ==="
