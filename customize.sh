# 注意 这不是占位符！！这个代码的作用是将模块里的东西全部塞系统里，然后挂上默认权限
# Magisk 模块脚本配置

# 说明：
# 1. 将你要替换的文件放入 system 文件夹 (删除 placeholder 文件)
# 2. 将模块信息写入 module.prop
# 3. 在这个文件中进行设置 (customize.sh)
# 4. 如果你需要在启动时执行命令, 请把它们加入 post-fs-data.sh 或 service.sh
# 5. 如果需要修改系统属性(build.prop), 请把它加入 system.prop

#监听音量键
Volume_key_monitoring() {
	local choose
	local branch
	while :; do
		choose="$(getevent -qlc 1 | awk '{ print $3 }')"
		case "$choose" in
			KEY_VOLUMEUP) branch="0" ;;
			KEY_VOLUMEDOWN) branch="1" ;;
			*) continue ;;
		esac
		echo "$branch"
		break
	done
}

# 如果你需要启用 Magic Mount 请把它设置为 true 不启用则设置为 false
# 大多数模块都需要启用它
AUTOMOUNT=true
SKIPMOUNT=false
PROPFILE=true
POSTFSDATA=true
LATESTARTSERVICE=true
SKIPUNZIP=0


D="`grep_prop description $TMPDIR/module.prop`"
CSC="getprop ro.boot.sales_code"

#开始安装
sleep 0.07
echo -en "\nOneUI CSC Features\nby Mzdyl\n\n"
ui_print "- $D    "

ui_print "- 你的设备地区是 $CSC\n尝试自动适配"
mv $MODPATH/optics/configs/carriers/TGY $MODPATH/optics/configs/carriers/$CSC

ui_print "—————————————————————————————————————"
ui_print "- 按音量键＋: 安装全功能版（有BUG）"
ui_print "- 按音量键－: 安装精简功能版（无BUG，应该）"
ui_print "—————————————————————————————————————"
sleep 0.07



if [[ $(Volume_key_monitoring) == 0 ]]; then
	ui_print "全功能版开始安装"
	REPLACE="
	/system/priv-app/ShareLive
	/system/app/AllShareAware
	/system/app/DAAgent
	/system/app/MdxKitService
"
else
	ui_print "精简功能版开始安装"
	sleep 0.5
	rm -rf "$MODPATH/system/priv-app/AppLock"
	rm -rf "$MODPATH/system/priv-app/BixbyTouch"
	rm -rf "$MODPATH/system/priv-app/ShareLive"
	rm -rf "$MODPATH/system/heimdallddata/"
	rm -rf "$MODPATH/system/etc/default-permissions"
	rm -rf "$MODPATH/system/etc/sysconfig/bixbytouchapp.xml"
	rm -rf "$MODPATH/system/etc/permissions/privapp-permissions-com.samsung.android.app.sharelive.xml"
	rm -rf "$MODPATH/system/etc/permissions/privapp-permissions-com.samsung.android.applock.xml"
	rm -rf "$MODPATH/system/etc/permissions/privapp-permissions-com.samsung.android.bixbytouch.xml"
	rm -rf "$MODPATH/system/etc/permissions/privapp-permissions-com.samsung.android.mdx.xml"
	rm -rf "$MODPATH/system/app/AllShareAware"
	rm -rf "$MODPATH/system/app/ChinaHiddenMenu"
	rm -rf "$MODPATH/system/app/ChnFileShareKitService"
	rm -rf "$MODPATH/system/app/DAAgent"
	rm -rf "$MODPATH/system/app/MdxKitService"
fi

ui_print "添加 切换至使用更好的 WLAN 网络"
settings put global sem_wifi_switch_to_better_wifi_supported 1
ui_print "添加 关闭 ADB 安装验证"
settings put global verifier_verify_adb_installs 0

set_perm_recursive  $MODPATH  0  0  0777  0777


# 列出你想在系统中直接删除的所有路径 一行一个路径 只能文件夹 不能文件 并且只能system里面的文件夹
# 此命令会删除下列路径文件夹内的所有文件

REPLACE="
/system/app/MinusOnePage
/system/priv-app/Firewall
"
# 这个文件 (customize.sh) 将被安装脚本在 util_functions.sh 之后 source 化（设置为环境变量）
# 如果你需要自定义操作, 请在这里以函数方式定义它们 然后在 update-binary 里调用这些函数
# 不要直接向 update-binary 添加代码 因为这会让你很难将模块迁移到新的模板版本
# 尽量不要对 update-binary 文件做其他修改 尽量只在其中执行函数调用