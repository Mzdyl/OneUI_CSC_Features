#!/system/bin/sh
# OneUI CSC Features - Refactored post-fs-data.sh

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
    echo "$1"
}

# Debug 日志
debug() {
    [ "$DEBUG" = "1" ] && log "[DEBUG] $1"
}

# 每次启动重新生成日志
prepare_log() {
    : > "$LOG_FILE"
    log "=== 模块启动: CSC=$CSC, ARCH=$ARCH, DEBUG=$DEBUG ==="
}

# 动态寻找文件路径
find_file() {
    local filename=$1
    local base="/optics/configs/carriers/$CSC"

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

# 安全挂载函数
safe_mount() {
    local src=$1
    local target=$2

    debug "准备挂载: $src -> $target"

    if [ ! -f "$src" ] || [ ! -f "$target" ]; then
        log "错误: 挂载源或目标不存在 ($src -> $target)"
        return 1
    fi
    
    umount -l "$target" >/dev/null 2>&1
    mount --bind "$src" "$target"

    local ret=$?

    if [ $ret -eq 0 ]; then
        log "成功挂载: $target"
        debug "mount 返回值: $ret"
        return 0
    else
        log "失败: 无法挂载 $target"
        debug "mount 返回值: $ret"
        return 1
    fi
}

# 处理单个文件逻辑
process_feature_file() {
    local label=$1
    local filename=$2
    local config_name=$3

    log "正在处理 $label ($filename)..."

    # 1. 寻找原始文件
    local origin_path=$(find_file "$filename")
    local decoded_file="$MODDIR/decoded_$label"

    if [ -z "$origin_path" ]; then
        if [ "$label" = "ff" ] && [ -f "/etc/floating_feature.xml" ]; then
            origin_path="/etc/floating_feature.xml"
            debug "使用 fallback floating_feature: $origin_path"
            cp "$origin_path" "$decoded_file"
        else
            log "跳过: 未找到原始文件 $filename"
            return
        fi
    fi

    debug "origin_path=$origin_path"

    # 2. 解码

    debug "执行 decode: $TOOL --decode $origin_path $decoded_file"

    $TOOL --decode "$origin_path" "$decoded_file" >> "$LOG_FILE" 2>&1

    # 3. Patch
    local user_config="$CONFIG_PATH/$config_name"
    local patched_file="$MODDIR/patched_$label"

    debug "user_config=$user_config"

    if [ -f "$user_config" ]; then
        debug "执行 patch"
        $TOOL --patch "$decoded_file" "$user_config" "$patched_file" >> "$LOG_FILE" 2>&1
    else
        log "注意: 未找到用户配置 $config_name，将保持原始状态"
        cp "$decoded_file" "$patched_file"
    fi

    # 4. Encode
    local final_file="$MODDIR/final_$label"

    debug "执行 encode"
    $TOOL --encode "$patched_file" "$final_file" >> "$LOG_FILE" 2>&1

    # 5. 挂载
    if [ -f "$final_file" ]; then
        debug "使用加密文件挂载"
        safe_mount "$final_file" "$origin_path"
        restorecon "$origin_path"
    else
        log "注意: 加密失败，尝试挂载明文文件"
        debug "使用明文挂载"
        safe_mount "$patched_file" "$origin_path"
        restorecon "$origin_path"
    fi
}

# --- 主流程 ---

prepare_log

debug "MODDIR=$MODDIR"
debug "CONFIG_PATH=$CONFIG_PATH"
debug "TOOL=$TOOL"

if [ ! -f "$TOOL" ]; then
    log "致命错误: 未在 $TOOL 找到核心工具"
fi

process_feature_file "csc" "cscfeature.xml" "csc.json"
process_feature_file "carrier" "customer_carrier_feature.json" "carrier.json"
process_feature_file "ff" "floating_feature.xml" "ff.json"

log "=== 处理完成 ==="