#!/system/bin/sh

CSC=${CSC:-$(getprop ro.boot.sales_code)}

log_mount() {
    [ -n "$LOG_FILE" ] || return
    echo "[$(date '+%H:%M:%S')] $1" >> "$LOG_FILE"
}

is_file_mountpoint() {
    grep -F " $1 " /proc/self/mountinfo >/dev/null 2>&1
}

bind_file_once() {
    local label=$1
    local src_path=$2
    local dst_path=$3

    [ -f "$src_path" ] || {
        log_mount "跳过 $label: 源文件不存在 $src_path"
        return 1
    }

    [ -f "$dst_path" ] || {
        log_mount "跳过 $label: 目标文件不存在 $dst_path"
        return 1
    }

    if is_file_mountpoint "$dst_path"; then
        umount -l "$dst_path" >/dev/null 2>&1
    fi

    mount --bind "$src_path" "$dst_path" >/dev/null 2>&1 || return 1
    return 0
}

bind_file_retry() {
    local label=$1
    local src_path=$2
    local dst_path=$3
    local attempt=1
    local max_attempts=${4:-5}

    while [ "$attempt" -le "$max_attempts" ]; do
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
        return 0
    fi

    bind_file_retry "$label" "$src_path" "$dst_path" 2
}

rebind_all_targets() {
    local stage=$1

    log_mount "=== 重新挂载阶段: $stage ==="

    bind_file_retry "csc"     "$MODDIR/optics/configs/carriers/$CSC/conf/system/cscfeature.xml"                "/optics/configs/carriers/$CSC/conf/system/cscfeature.xml"                3
    bind_file_retry "carrier" "$MODDIR/optics/configs/carriers/$CSC/conf/system/customer_carrier_feature.json" "/optics/configs/carriers/$CSC/conf/system/customer_carrier_feature.json" 3
    bind_file_retry "ff"      "$MODDIR/system/etc/floating_feature.xml"                                         "/system/etc/floating_feature.xml"                                         3
    bind_file_retry "camera"  "$MODDIR/system/cameradata/camera-feature.xml"                                    "/system/cameradata/camera-feature.xml"                                    3
}

ensure_all_targets() {
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
