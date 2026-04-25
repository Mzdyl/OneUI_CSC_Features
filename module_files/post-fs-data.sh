#!/system/bin/sh
# OneUI CSC Features

MODDIR=${0%/*}
LOG_FILE="$MODDIR/log.txt"
CONFIG_PATH="/data/adb/csc_config"
ARCH=$(getprop ro.product.cpu.abi)
CSC=$(getprop ro.boot.sales_code)
TOOL="$MODDIR/libs/$ARCH/csc_tool"
STATE_DIR="$MODDIR/.state"
RUNTIME_DIR="$STATE_DIR/runtime"
GENERATED_LIST="$STATE_DIR/generated_paths.list"

. "$MODDIR/rebind_mounts.sh"

# ===== Debug 开关 =====
DEBUG=0   # 0=关闭  1=开启

# --- 辅助函数 ---

log() {
    echo "[$(date '+%H:%M:%S')] $1" >> "$LOG_FILE"
    echo "$1" >&2
}

debug() {
    [ "$DEBUG" = "1" ] && log "[DEBUG] $1"
}

cleanup_empty_dirs() {
    local dir=$1

    while [ -n "$dir" ] && [ "$dir" != "$MODDIR" ] && [ "$dir" != "/" ]; do
        rmdir "$dir" 2>/dev/null || break
        dir=$(dirname "$dir")
    done
}

cleanup_previous_generated() {
    if [ -f "$GENERATED_LIST" ]; then
        while IFS= read -r rel_path; do
            [ -n "$rel_path" ] || continue
            local abs_path="$MODDIR$rel_path"
            if [ -e "$abs_path" ]; then
                rm -f "$abs_path"
                cleanup_empty_dirs "$(dirname "$abs_path")"
            fi
        done < "$GENERATED_LIST"
    fi

    : > "$GENERATED_LIST"
}

cleanup_legacy_artifacts() {
    rm -f "$MODDIR"/decoded_* "$MODDIR"/patched_* "$MODDIR"/final_* 2>/dev/null
    rm -rf "$MODDIR/system/optics" "$MODDIR/optics" "$MODDIR/system/cameradata" 2>/dev/null
    rm -f "$MODDIR/system/etc/floating_feature.xml" 2>/dev/null
}

prepare_log() {
    mkdir -p "$STATE_DIR" "$RUNTIME_DIR"
    : > "$LOG_FILE"
    log "=== 模块启动: CSC=$CSC, ARCH=$ARCH, DEBUG=$DEBUG ==="
}

find_file() {
    local filename=$1
    local carriers="/optics/configs/carriers"
    local matches
    local count
    local chosen

    if [ ! -d "$carriers" ]; then
        log "错误: 未找到 carriers 目录 $carriers"
        return 1
    fi

    matches=$(find "$carriers" -type f -name "$filename" -path "*/$CSC/*" 2>/dev/null | sort)
    count=$(printf '%s\n' "$matches" | sed '/^$/d' | wc -l | tr -d ' ')

    if [ "$count" = "0" ]; then
        debug "未找到文件: $filename"
        return 1
    fi

    if [ "$count" != "1" ]; then
        log "警告: $filename 找到 $count 个候选路径，按排序选择第一项"
        printf '%s\n' "$matches" | sed '/^$/d' | while IFS= read -r item; do
            log "候选: $item"
        done
    fi

    chosen=$(printf '%s\n' "$matches" | sed '/^$/d' | head -n 1)
    log "选中原始文件: $chosen"
    echo "$chosen"
}

normalize_mount_target() {
    local target_path=$1

    case "$target_path" in
        /etc/*) echo "/system$target_path" ;;
        *) echo "$target_path" ;;
    esac
}

resolve_mount_targets() {
    local origin_path=$1
    local real_path=""

    normalize_mount_target "$origin_path"

    real_path=$(readlink -f "$origin_path" 2>/dev/null)
    if [ -n "$real_path" ] && [ "$real_path" != "$origin_path" ]; then
        normalize_mount_target "$real_path"
    fi
}

record_generated_path() {
    printf '%s\n' "$1" >> "$GENERATED_LIST"
}

copy_selinux_context() {
    local source_path=$1
    local target_path=$2
    local ctx=""

    [ -e "$source_path" ] || return
    ctx=$(ls -Z "$source_path" 2>/dev/null | awk '{print $1}')
    [ -n "$ctx" ] && chcon "$ctx" "$target_path" 2>/dev/null
}

deploy_for_mount() {
    local src_file=$1
    local origin_path=$2
    local mount_targets

    mount_targets=$(resolve_mount_targets "$origin_path" | awk '!seen[$0]++')
    if [ -z "$mount_targets" ]; then
        log "错误: 无法解析挂载目标 $origin_path"
        return 1
    fi

    printf '%s\n' "$mount_targets" | while IFS= read -r target_path; do
        local module_target_path

        [ -n "$target_path" ] || continue
        module_target_path="$MODDIR$target_path"

        mkdir -p "$(dirname "$module_target_path")"
        cp -f "$src_file" "$module_target_path"
        copy_selinux_context "$target_path" "$module_target_path"
        record_generated_path "$target_path"

        log "部署挂载文件: $target_path <- $(basename "$src_file")"
        bind_file_retry "$target_path" "$module_target_path" "$target_path" 5
    done
}

process_feature_file() {
    local label=$1
    local filename=$2
    local config_name=$3
    local is_plain=${4:-0}
    local fixed_path=${5:-""}
    local origin_path=""
    local decoded_file="$RUNTIME_DIR/decoded_$label"
    local patched_file="$RUNTIME_DIR/patched_$label"
    local final_file="$RUNTIME_DIR/final_$label"
    local user_config="$CONFIG_PATH/$config_name"
    local deploy_file=""

    log "正在处理 $label ($filename)..."

    if [ -n "$fixed_path" ]; then
        origin_path="$fixed_path"
        log "使用固定原始文件: $origin_path"
    else
        origin_path=$(find_file "$filename")
        if [ -z "$origin_path" ] && [ "$label" = "ff" ]; then
            [ -f "/system/etc/floating_feature.xml" ] && origin_path="/system/etc/floating_feature.xml"
            [ -z "$origin_path" ] && [ -f "/etc/floating_feature.xml" ] && origin_path="/etc/floating_feature.xml"
            [ -n "$origin_path" ] && log "使用 floating_feature fallback: $origin_path"
        fi
    fi

    if [ -z "$origin_path" ] || [ ! -f "$origin_path" ]; then
        log "跳过: 未找到原始文件 $filename"
        return
    fi

    if [ -f "$user_config" ]; then
        log "$label 配置文件: $user_config"
    else
        log "$label 未找到配置文件: $user_config，将保持原始状态"
    fi

    rm -f "$decoded_file" "$patched_file" "$final_file"

    if [ "$is_plain" = "1" ]; then
        cp -f "$origin_path" "$decoded_file"
    else
        debug "执行 decode: $origin_path"
        "$TOOL" --decode "$origin_path" "$decoded_file" >> "$LOG_FILE" 2>&1
        if [ ! -f "$decoded_file" ]; then
            log "错误: decode 失败 ($label)"
            return
        fi
    fi

    if [ -f "$user_config" ]; then
        debug "执行 patch: $config_name"
        "$TOOL" --patch "$decoded_file" "$user_config" "$patched_file" >> "$LOG_FILE" 2>&1
    else
        cp -f "$decoded_file" "$patched_file"
    fi

    if [ ! -f "$patched_file" ]; then
        log "错误: patch 失败 ($label)"
        return
    fi

    if [ "$is_plain" = "1" ]; then
        deploy_file="$patched_file"
    else
        debug "执行 encode"
        "$TOOL" --encode "$patched_file" "$final_file" >> "$LOG_FILE" 2>&1
        if [ -f "$final_file" ]; then
            deploy_file="$final_file"
        else
            log "警告: encode 失败，改为部署明文结果 ($label)"
            deploy_file="$patched_file"
        fi
    fi

    log "$label 输出文件: $(basename "$deploy_file")"
    deploy_for_mount "$deploy_file" "$origin_path"
}

# --- 主流程 ---

prepare_log

debug "MODDIR=$MODDIR"
debug "CONFIG_PATH=$CONFIG_PATH"
debug "TOOL=$TOOL"

if [ ! -f "$TOOL" ]; then
    log "致命错误: 未找到核心工具 $TOOL"
    exit 1
fi

chmod +x "$TOOL"

cleanup_previous_generated
cleanup_legacy_artifacts

process_feature_file "csc"      "cscfeature.xml"                "csc.json"              0
process_feature_file "carrier"  "customer_carrier_feature.json" "carrier.json"          0
process_feature_file "ff"       "floating_feature.xml"          "ff.json"               1
process_feature_file "camera"   "camera-feature.xml"            "camera-feature.json"   1 "/system/cameradata/camera-feature.xml"

rebind_all_targets "post-fs-data"

sh "$MODDIR/mount_guard.sh" >/dev/null 2>&1 &

log "=== 脚本执行完毕 ==="
