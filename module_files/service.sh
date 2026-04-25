#!/system/bin/sh

MODDIR=${0%/*}
LOG_FILE="$MODDIR/log.txt"
CSC=$(getprop ro.boot.sales_code)

. "$MODDIR/rebind_mounts.sh"

sh "$MODDIR/mount_guard.sh" >/dev/null 2>&1 &

sleep 10
rebind_all_targets "late-start"

# 智能管理器 智能启动应用程序
settings put system aiprel_switch_status 1

count=0
while [ "$count" -lt 120 ]; do
    if [ "$(getprop sys.boot_completed 2>/dev/null)" = "1" ]; then
        sleep 5
        rebind_all_targets "boot-completed"
        restart_camera_services
        sleep 8
        rebind_all_targets "after-camera-restart"
        break
    fi

    sleep 1
    count=$((count + 1))
done
