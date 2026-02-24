ui_print "------------------------"
ui_print "安装后请在WebUI 选择并开启所需功能"
ui_print "------------------------"
ui_print "若过度修改导致无法开机" 
ui_print "请重启进LOGO时长按音量- 键禁用所有模块"
ui_print "暂停3秒，请知悉阅读使用说明"
sleep 3

set_perm_recursive $MODPATH 0 0 0777 0777

REPLACE="
"
