#!/system/bin/sh

CSC=${CSC:-$(getprop ro.boot.sales_code)}
STATE_DIR=${STATE_DIR:-"$MODDIR/.state"}
MOUNT_LIST=${MOUNT_LIST:-"$STATE_DIR/mounts.list"}
CONFIG_PATH=${CONFIG_PATH:-"/data/adb/csc_config"}
DEBUG=${DEBUG:-0}
[ -f "$CONFIG_PATH/debug" ] && DEBUG=1
DEBUG_VERBOSE=${DEBUG_VERBOSE:-0}
[ -f "$CONFIG_PATH/debug_verbose" ] && DEBUG_VERBOSE=1

log_mount() {
    [ -n "$LOG_FILE" ] || return
    echo "[$(date '+%H:%M:%S')] $1" >> "$LOG_FILE"
}

debug_mount() {
    [ "$DEBUG" = "1" ] && log_mount "[DEBUG] $1"
}

debug_mount_verbose() {
    [ "$DEBUG_VERBOSE" = "1" ] && log_mount "[DEBUG] $1"
}

file_size() {
    wc -c "$1" 2>/dev/null | awk '{print $1}'
}

file_sum() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" 2>/dev/null | awk '{print $1}'
    elif command -v md5sum >/dev/null 2>&1; then
        md5sum "$1" 2>/dev/null | awk '{print $1}'
    else
        echo "unavailable"
    fi
}

is_file_mountpoint() {
    grep -F " $1 " /proc/self/mountinfo >/dev/null 2>&1
}

log_mount_state() {
    local label=$1
    local src_path=$2
    local dst_path=$3
    local verbose=${4:-0}
    local mount_line
    local log_fn="debug_mount"

    [ "$DEBUG" = "1" ] || return
    [ "$verbose" = "1" ] && [ "$DEBUG_VERBOSE" != "1" ] && return
    [ "$verbose" = "1" ] && log_fn="debug_mount_verbose"

    mount_line=$(grep -F " $dst_path " /proc/self/mountinfo 2>/dev/null | tail -n 1)
    $log_fn "状态 [$label]: src_exists=$([ -f "$src_path" ] && echo 1 || echo 0) dst_exists=$([ -f "$dst_path" ] && echo 1 || echo 0) mounted=$([ -n "$mount_line" ] && echo 1 || echo 0)"
    $log_fn "状态 [$label]: src_size=$(file_size "$src_path") dst_size=$(file_size "$dst_path") src_sum=$(file_sum "$src_path") dst_sum=$(file_sum "$dst_path")"
    [ -n "$mount_line" ] && $log_fn "mountinfo [$label]: $mount_line"
}

log_process_view() {
    local label=$1
    local dst_path=$2
    local proc_name=$3
    local pid=$4
    local proc_file
    local mount_line

    [ "$DEBUG" = "1" ] || return
    [ -n "$pid" ] || {
        debug_mount "进程视角 [$label/$proc_name]: 未找到 pid"
        return
    }
    [ -d "/proc/$pid" ] || {
        debug_mount "进程视角 [$label/$proc_name]: /proc/$pid 不存在"
        return
    }

    proc_file="/proc/$pid/root$dst_path"
    mount_line=$(grep -F " $dst_path " "/proc/$pid/mountinfo" 2>/dev/null | tail -n 1)
    debug_mount "进程视角 [$label/$proc_name]: pid=$pid namespace=$(readlink "/proc/$pid/ns/mnt" 2>/dev/null) mounted=$([ -n "$mount_line" ] && echo 1 || echo 0)"
    debug_mount "进程视角 [$label/$proc_name]: file_exists=$([ -f "$proc_file" ] && echo 1 || echo 0) size=$(file_size "$proc_file") sum=$(file_sum "$proc_file")"
    [ -n "$mount_line" ] && debug_mount "进程 mountinfo [$label/$proc_name]: $mount_line"
}

log_system_views() {
    local label=$1
    local dst_path=$2
    local system_server_pid

    [ "$DEBUG" = "1" ] || return

    system_server_pid=$(pidof system_server 2>/dev/null | awk '{print $1}')
    log_process_view "$label" "$dst_path" "init" "1"
    log_process_view "$label" "$dst_path" "system_server" "$system_server_pid"
}

record_mount_pair() {
    local label=$1
    local src_path=$2
    local dst_path=$3

    mkdir -p "$STATE_DIR"
    printf '%s|%s|%s\n' "$label" "$src_path" "$dst_path" >> "$MOUNT_LIST"
    debug_mount "记录挂载项: label=$label src=$src_path dst=$dst_path"
}

bind_file_once() {
    local label=$1
    local src_path=$2
    local dst_path=$3
    local err_file="$STATE_DIR/mount_error.$$"
    local err_msg

    debug_mount "尝试挂载 [$label]: $dst_path <- $src_path"
    log_mount_state "$label.before" "$src_path" "$dst_path"

    [ -f "$src_path" ] || {
        log_mount "跳过 $label: 源文件不存在 $src_path"
        return 1
    }

    [ -f "$dst_path" ] || {
        log_mount "跳过 $label: 目标文件不存在 $dst_path"
        return 1
    }

    if is_file_mountpoint "$dst_path"; then
        debug_mount "目标已是挂载点，先 lazy umount: $dst_path"
        umount -l "$dst_path" >/dev/null 2>&1
        log_mount_state "$label.after-umount" "$src_path" "$dst_path"
    fi

    mkdir -p "$STATE_DIR"
    mount --bind "$src_path" "$dst_path" >/dev/null 2>"$err_file" && {
        rm -f "$err_file"
        log_mount_state "$label.after-bind" "$src_path" "$dst_path"
        log_system_views "$label.after-bind" "$dst_path"
        return 0
    }

    err_msg=$(cat "$err_file" 2>/dev/null)
    rm -f "$err_file"
    [ -n "$err_msg" ] && log_mount "挂载错误 [$label]: $err_msg"
    return 1
}

bind_file_retry() {
    local label=$1
    local src_path=$2
    local dst_path=$3
    local attempt=1
    local max_attempts=${4:-5}

    debug_mount "开始重试挂载 [$label]: max_attempts=$max_attempts dst=$dst_path"
    while [ "$attempt" -le "$max_attempts" ]; do
        debug_mount "挂载尝试 [$label]: attempt=$attempt"
        if bind_file_once "$label" "$src_path" "$dst_path"; then
            log_mount "挂载成功 [$label] $dst_path <- $src_path (attempt=$attempt)"
            return 0
        fi

        sleep 1
        attempt=$((attempt + 1))
    done

    log_mount "挂载失败 [$label] $dst_path <- $src_path"
    return 1
}

ensure_bind_file() {
    local label=$1
    local src_path=$2
    local dst_path=$3

    if is_file_mountpoint "$dst_path"; then
        debug_mount_verbose "守护检查 [$label]: 已挂载 $dst_path"
        log_mount_state "$label.ensure-ok" "$src_path" "$dst_path" 1
        return 0
    fi

    debug_mount "守护检查 [$label]: 未挂载，准备补挂 $dst_path"
    bind_file_retry "$label" "$src_path" "$dst_path" 2
}

rebind_all_targets() {
    local stage=$1
    local did_dynamic=0

    log_mount "=== 重新挂载阶段: $stage ==="
    debug_mount "阶段环境: stage=$stage pid=$$ namespace=$(readlink /proc/$$/ns/mnt 2>/dev/null) CSC=$CSC MODDIR=$MODDIR MOUNT_LIST=$MOUNT_LIST"

    if [ -f "$MOUNT_LIST" ]; then
        debug_mount "使用动态挂载清单: $MOUNT_LIST"
        while IFS='|' read -r label src_path dst_path; do
            [ -n "$label" ] || continue
            [ -n "$src_path" ] || continue
            [ -n "$dst_path" ] || continue

            bind_file_retry "$label" "$src_path" "$dst_path" 3
            did_dynamic=1
        done < "$MOUNT_LIST"
    fi

    [ "$did_dynamic" = "1" ] && {
        debug_mount "动态挂载清单处理完成: stage=$stage"
        return
    }

    debug_mount "未找到动态挂载清单，使用硬编码兜底路径"
    bind_file_retry "csc"     "$MODDIR/optics/configs/carriers/$CSC/conf/system/cscfeature.xml"                "/optics/configs/carriers/$CSC/conf/system/cscfeature.xml"                3
    bind_file_retry "carrier" "$MODDIR/optics/configs/carriers/$CSC/conf/system/customer_carrier_feature.json" "/optics/configs/carriers/$CSC/conf/system/customer_carrier_feature.json" 3
    bind_file_retry "ff"      "$MODDIR/system/etc/floating_feature.xml"                                         "/system/etc/floating_feature.xml"                                         3
    bind_file_retry "camera"  "$MODDIR/system/cameradata/camera-feature.xml"                                    "/system/cameradata/camera-feature.xml"                                    3
}

ensure_all_targets() {
    local did_dynamic=0

    debug_mount_verbose "守护轮询: pid=$$ namespace=$(readlink /proc/$$/ns/mnt 2>/dev/null)"
    if [ -f "$MOUNT_LIST" ]; then
        debug_mount_verbose "守护使用动态挂载清单: $MOUNT_LIST"
        while IFS='|' read -r label src_path dst_path; do
            [ -n "$label" ] || continue
            [ -n "$src_path" ] || continue
            [ -n "$dst_path" ] || continue

            ensure_bind_file "$label" "$src_path" "$dst_path"
            did_dynamic=1
        done < "$MOUNT_LIST"
    fi

    [ "$did_dynamic" = "1" ] && return

    debug_mount_verbose "守护未找到动态挂载清单，使用硬编码兜底路径"
    ensure_bind_file "csc"     "$MODDIR/optics/configs/carriers/$CSC/conf/system/cscfeature.xml"                "/optics/configs/carriers/$CSC/conf/system/cscfeature.xml"
    ensure_bind_file "carrier" "$MODDIR/optics/configs/carriers/$CSC/conf/system/customer_carrier_feature.json" "/optics/configs/carriers/$CSC/conf/system/customer_carrier_feature.json"
    ensure_bind_file "ff"      "$MODDIR/system/etc/floating_feature.xml"                                         "/system/etc/floating_feature.xml"
    ensure_bind_file "camera"  "$MODDIR/system/cameradata/camera-feature.xml"                                    "/system/cameradata/camera-feature.xml"
}

restart_camera_services() {
    local restarted=0
    local service_name

    for service_name in cameraserver sec-camera-provider vendor.camera.provider-ext vendor.camera-provider vendor.camera-provider-2-4; do
        if [ "$(getprop "init.svc.$service_name" 2>/dev/null)" = "running" ]; then
            log_mount "重启相机服务: $service_name"
            setprop ctl.restart "$service_name"
            restarted=1
        fi
    done

    if [ "$restarted" = "0" ]; then
        log_mount "未找到可重启的相机服务"
        return 1
    fi

    return 0
}
