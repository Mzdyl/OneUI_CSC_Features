#!/system/bin/sh
# OneUI CSC Features

MODDIR=${0%/*}
LOG_FILE="$MODDIR/log.txt"
CONFIG_PATH="/data/adb/csc_config"
ARCH=$(getprop ro.product.cpu.abi)
CSC=$(getprop ro.boot.sales_code)
TOOL="$MODDIR/libs/$ARCH/csc_tool"
HASH_DIR="$MODDIR/hashes"

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
    mkdir -p "$HASH_DIR"
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

# 计算文件联合 Hash
# 参数: 传入需要纳入校验的多个文件路径
calc_hash() {
    # 将存在的文件内容合并后计算 md5
    cat "$@" 2>/dev/null | md5sum | awk '{print $1}'
}

# 部署文件以供 Magisk/KSU OverlayFS/Magic Mount 挂载
deploy_for_mount() {
    local src_file=$1
    local origin_path=$2
    
    # 路径标准化: 如果是 /etc 开头，转为 /system/etc，确保 Magisk/KSU 正确识别挂载点
    local target_path="$origin_path"
    case "$target_path" in
        /etc/*) target_path="/system$target_path" ;;
    esac

    local module_target_path="$MODDIR$target_path"
    
    debug "准备部署挂载点: $module_target_path"
    
    # 创建对应的目录树
    mkdir -p "$(dirname "$module_target_path")"
    
    # 复制文件到模块对应目录，KSU/Magisk 会在稍后自动 OverlayFS 挂载它
    cp -f "$src_file" "$module_target_path"
    
    # 同步 SELinux 上下文
    if [ -f "$origin_path" ]; then
        local ctx=$(ls -Z "$origin_path" 2>/dev/null | awk '{print $1}')
        [ -n "$ctx" ] && chcon "$ctx" "$module_target_path" 2>/dev/null
    fi

    log "成功部署: $target_path"
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

    # 3. Hash 校验 (支持增量更新)
    local hash_file="$HASH_DIR/$label.md5"
    local current_hash=$(calc_hash "$origin_path" "$user_config" "$TOOL")
    local old_hash=""
    [ -f "$hash_file" ] && old_hash=$(cat "$hash_file")

    # 检查已部署文件路径
    local check_path="$origin_path"
    case "$check_path" in
        /etc/*) check_path="/system$check_path" ;;
    esac

    if [ "$current_hash" = "$old_hash" ] && [ -f "$MODDIR$check_path" ]; then
        log "增量校验通过: $label 无变动，跳过。"
        return
    fi

    log "检测到 $label 变动，开始处理..."

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

    # 7. 部署挂载
    if [ -f "$final_file" ]; then
        deploy_for_mount "$final_file" "$origin_path"
        echo "$current_hash" > "$hash_file"
        log "✓ $label 处理完成"
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
process_feature_file "camera"   "camera-feature.xml"            "camera-feature.json"   1 "/system/cameradata/camera-feature.xml"

log "=== 脚本执行完毕，等待系统 OverlayFS 挂载 ==="