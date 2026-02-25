# OneUI CSC Features - customize.sh

# 设置权限
set_perm_recursive $MODPATH 0 0 0755 0755
set_perm_recursive $MODPATH/libs 0 0 0755 0755

# 处理配置文件
CONFIG_DIR="/data/adb/csc_config"
ui_print "- 正在初始化配置文件路径: $CONFIG_DIR"
mkdir -p "$CONFIG_DIR"

for f in csc.json ff.json carrier.json; do
  if [ ! -f "$CONFIG_DIR/$f" ]; then
    ui_print "  释放默认配置: $f"
    mv "$MODPATH/configs/$f" "$CONFIG_DIR/$f"
  else
    ui_print "  跳过已存在的配置: $f"
  fi
done

# 权限修正
set_perm_recursive "$CONFIG_DIR" 0 0 0755 0755

ui_print "------------------------"
ui_print "安装完成！"
ui_print "请在 WebUI 中开启所需功能。"
ui_print "数据存储在: $CONFIG_DIR"
ui_print "------------------------"
sleep 2
