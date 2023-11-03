#!/system/bin/sh
# 开机之前执行
# 请不要硬编码 /magisk/modname/... ; 请使用 $MODDIR/...
# 这将使你的脚本更加兼容 即使Magisk在未来改变了它的挂载点

MODDIR=${0%/*}
# 这个脚本将以 post-fs-data 模式执行(系统启动前执行)
# 更多信息请访问 Magisk 主题

mount --bind $MODDIR/optics /optics
mount --bind $MODDIR/prism /prism

# 强制全局240Hz采样率
echo 1 > /proc/touchpanel/game_switch_enable

# 切换至使用更好的 WLAN 网络
settings put global sem_wifi_switch_to_better_wifi_supported 1

# 禁用锁帧
settings put global package_policy_disabled 1

# 禁用 ADB 安装应用时的安全验证(加快软件安装速度)
settings put global verifier_verify_adb_installs 0