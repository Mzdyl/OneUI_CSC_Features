#!/system/bin/sh
# OneUI CSC Features - Refactored post-fs-data.sh

MODDIR=${0%/*}
LOG_FILE="$MODDIR/log.txt"
CONFIG_PATH="/data/adb/csc_config"
ARCH=$(getprop ro.product.cpu.abi)
CSC=$(getprop ro.boot.sales_code)
TOOL="$MODDIR/libs/$ARCH/csc_tool"

# --- 辅助函数 ---

# 统一日志输出
log() {
    echo "[$(date '+%H:%M:%S')] $1" >> "$LOG_FILE"
    echo "$1"
}

# 清理过大日志 (保留最后 1000 行)
prepare_log() {
    if [ -f "$LOG_FILE" ]; then
        echo "$(tail -n 1000 "$LOG_FILE")" > "$LOG_FILE"
    fi
    log "=== 模块启动: CSC=$CSC, ARCH=$ARCH ==="
}

# 动态寻找文件路径
find_file() {
    local filename=$1
    # 锁定在 /optics 分区中寻找当前销售代码 (CSC) 的配置
    local path=$(find "/optics/configs/carriers/$CSC" -name "$filename" 2>/dev/null | head -n 1)
    if [ -f "$path" ]; then
        echo "$path"
        return 0
    fi
    return 1
}

# 安全挂载函数
safe_mount() {
    local src=$1
    local target=$2
    if [ ! -f "$src" ] || [ ! -f "$target" ]; then
        log "错误: 挂载源或目标不存在 ($src -> $target)"
        return 1
    fi
    
    # 清理旧挂载 (容错)
    umount -l "$target" >/dev/null 2>&1
    
    mount --bind "$src" "$target"
    if [ $? -eq 0 ]; then
        log "成功挂载: $target"
        return 0
    else
        log "失败: 无法挂载 $target"
        return 1
    fi
}

# 处理单个文件逻辑 (Decode -> Patch -> Encode -> Mount)
process_feature_file() {
    local label=$1      # 标识符: csc, ff, carrier
    local filename=$2   # 原始文件名
    local config_name=$3 # 用户 JSON 配置名

    log "正在处理 $label ($filename)..."
    
    # 1. 寻找原始文件
    local origin_path=$(find_file "$filename")
    if [ -z "$origin_path" ]; then
        # 特殊处理 floating_feature，它通常在 /etc/
        if [ "$label" = "ff" ] && [ -f "/etc/floating_feature.xml" ]; then
            origin_path="/etc/floating_feature.xml"
        else
            log "跳过: 未找到原始文件 $filename"
            return
        fi
    fi

    # 2. 解码 (如果是加密的 XML/JSON)
    local decoded_file="$MODDIR/decoded_$label"
    $TOOL --decode "$origin_path" "$decoded_file" >> "$LOG_FILE" 2>&1
    
    # 如果解码失败（可能是明文），则直接拷贝
    if [ ! -f "$decoded_file" ]; then
        cp "$origin_path" "$decoded_file"
    fi

    # 3. 应用补丁 (Patching + CSC_SORT)
    local user_config="$CONFIG_PATH/$config_name"
    local patched_file="$MODDIR/patched_$label"
    if [ -f "$user_config" ]; then
        $TOOL --patch "$decoded_file" "$user_config" "$patched_file" >> "$LOG_FILE" 2>&1
    else
        log "注意: 未找到用户配置 $config_name，将保持原始状态"
        cp "$decoded_file" "$patched_file"
    fi

    # 4. 编码 (重新加密)
    local final_file="$MODDIR/final_$label"
    $TOOL --encode "$patched_file" "$final_file" >> "$LOG_FILE" 2>&1
    
    # 5. 挂载
    # 优先挂载加密后的 final_file，如果不存在则尝试挂载 patched_file (明文)
    if [ -f "$final_file" ]; then
        safe_mount "$final_file" "$origin_path"
        restorecon "$origin_path"
    else
        log "注意: 加密失败，尝试挂载明文文件"
        safe_mount "$patched_file" "$origin_path"
        restorecon "$origin_path"
    fi
}

# --- 主流程 ---

prepare_log

# 检查工具是否存在
if [ ! -f "$TOOL" ]; then
    log "致命错误: 未在 $TOOL 找到核心工具"
    # 回退到尝试使用老的二进制(如果存在)或退出
fi

# 处理 CSC
process_feature_file "csc" "cscfeature.xml" "csc.json"

# 处理 Carrier
process_feature_file "carrier" "customer_carrier_feature.json" "carrier.json"

# 处理 Floating Feature
process_feature_file "ff" "floating_feature.xml" "ff.json"

log "=== 处理完成 ==="
