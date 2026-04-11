#!/system/bin/sh
# OneUI CSC Features

MODDIR=${0%/*}
LOG_FILE="$MODDIR/log.txt"
CONFIG_PATH="/data/adb/csc_config"
ARCH=$(getprop ro.product.cpu.abi)
CSC=$(getprop ro.boot.sales_code)
TOOL="$MODDIR/libs/$ARCH/csc_tool"

# ===== Debug 开关 =====
DEBUG=0   # 0=关闭  1=开启

# --- 辅助函数 ---

# 统一日志输出
log() {
    echo "[$(date '+%H:%M:%S')] $1" >> "$LOG_FILE"
    echo "$1" >&2
}

debug() {
    [ "$DEBUG" = "1" ] && log "[DEBUG] $1"
}

prepare_log() {
    : > "$LOG_FILE"
    log "=== 模块启动: CSC=$CSC, ARCH=$ARCH, DEBUG=$DEBUG ==="
}

# 动态寻找原始文件路径
find_file() {
    local filename=$1
    local carriers="/optics/configs/carriers"
    local base=$(find "$carriers" -type d -name "$CSC" 2>/dev/null | head -n 1)
    debug "查找文件: $filename (base=$base)"
    local path=$(find "$base" -name "$filename" 2>/dev/null | head -n 1)

    if [ -f "$path" ]; then
        debug "找到文件: $path"
        echo "$path"
        return 0
    fi
    debug "未找到文件: $filename"
    return 1
}

# 使用 mount --bind 显式挂载文件
# 参数: $1=源文件路径(模块内处理后的文件) $2=目标文件路径(系统原始文件)
mount_bind_file() {
    local src_file=$1
    local target_path=$2

    # 路径标准化: 如果是 /etc 开头，转为 /system/etc
    local normalized_target="$target_path"
    case "$normalized_target" in
        /etc/*) normalized_target="/system$target_path" ;;
    esac

    # 确保目标文件存在
    if [ ! -f "$normalized_target" ]; then
        log "错误: 目标文件不存在 $normalized_target"
        return 1
    fi

    # 先卸载可能存在的旧挂载
    if mount | grep -q "$normalized_target"; then
        debug "检测到旧挂载，正在卸载: $normalized_target"
        umount -l "$normalized_target" 2>/dev/null
    fi

    # 执行 mount --bind
    mount --bind "$src_file" "$normalized_target" >> "$LOG_FILE" 2>&1

    # 检查挂载是否成功
    if mount | grep -q "$normalized_target"; then
        log "✓ 成功挂载: $normalized_target"
        return 0
    else
        log "✗ 挂载失败: $normalized_target"
        return 1
    fi
}

# 通用特性文件处理函数
# 参数: $1=标签 $2=文件名 $3=配置名 $4=是否明文 $5=固定路径(可选)
process_feature_file() {
    local label=$1
    local filename=$2
    local config_name=$3
    local is_plain=${4:-0}  # 0=加密, 1=明文
    local fixed_path=${5:-""}

    log "正在处理 $label ($filename)..."

    # 1. 确定原始文件路径
    local origin_path=""
    if [ -n "$fixed_path" ]; then
        origin_path="$fixed_path"
    else
        origin_path=$(find_file "$filename")
        # floating_feature 特殊 fallback
        if [ -z "$origin_path" ] && [ "$label" = "ff" ]; then
            [ -f "/system/etc/floating_feature.xml" ] && origin_path="/system/etc/floating_feature.xml"
            [ -z "$origin_path" ] && [ -f "/etc/floating_feature.xml" ] && origin_path="/etc/floating_feature.xml"
        fi
    fi

    if [ -z "$origin_path" ] || [ ! -f "$origin_path" ]; then
        log "跳过: 未找到原始文件 $filename"
        return
    fi

    # 2. 文件路径定义
    local decoded_file="$MODDIR/decoded_$label"
    local patched_file="$MODDIR/patched_$label"
    local final_file="$MODDIR/final_$label"
    local user_config="$CONFIG_PATH/$config_name"

    # 3. 检查已处理文件是否存在（重启后直接使用缓存的处理后文件）
    if [ -f "$final_file" ]; then
        log "$label 已存在缓存，直接使用..."
        mount_bind_file "$final_file" "$origin_path"
        return
    fi

    log "开始处理 $label..."

    # 4. 解码/复制 (根据文件类型)
    if [ "$is_plain" = "1" ]; then
        debug "$label 使用明文模式"
        cp -f "$origin_path" "$decoded_file"
    else
        debug "执行 decode: $origin_path"
        $TOOL --decode "$origin_path" "$decoded_file" >> "$LOG_FILE" 2>&1
        if [ ! -f "$decoded_file" ]; then
            log "错误: decode 失败 ($label)"
            return
        fi
    fi

    # 5. Patch (应用用户配置)
    if [ -f "$user_config" ]; then
        debug "执行 patch: $config_name"
        $TOOL --patch "$decoded_file" "$user_config" "$patched_file" >> "$LOG_FILE" 2>&1
    else
        log "注意: 未找到配置 $config_name，保持原始状态"
        cp -f "$decoded_file" "$patched_file"
    fi

    # 6. Encode (仅加密文件需要)
    if [ "$is_plain" = "1" ]; then
        final_file="$patched_file"
    else
        debug "执行 encode"
        $TOOL --encode "$patched_file" "$final_file" >> "$LOG_FILE" 2>&1
        if [ ! -f "$final_file" ]; then
            log "警告: encode 失败，尝试部署明文"
            final_file="$patched_file"
        fi
    fi

    # 7. 使用 mount --bind 显式挂载
    if [ -f "$final_file" ]; then
        mount_bind_file "$final_file" "$origin_path"
        log "✓ $label 处理完成并已挂载"
    else
        log "✗ 错误: patch 失败 ($label)"
    fi
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

# 处理各类特性文件
# 参数: 标签 | 文件名 | 配置名 | 模式(0=加密,1=明文) | 固定路径(可选)
process_feature_file "csc"      "cscfeature.xml"                "csc.json"              0
process_feature_file "carrier"  "customer_carrier_feature.json" "carrier.json"          0
process_feature_file "ff"       "floating_feature.xml"          "ff.json"               1

log "=== 脚本执行完毕，mount --bind 挂载完成 ==="