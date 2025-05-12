ui_print "------------------------"
ui_print "安装后请自行到"
ui_print "/data/adb/modules/auto_modify_cscfeature"
ui_print "修改对应 example 文件，并修改后缀后开启使用"
ui_print "------------------------"
ui_print "请勿直接套用！！！"
ui_print "无法开机时 进LOGO 长按音量- 键禁用所有 magisk 模块"
ui_print "暂停3秒，请知悉阅读使用说明"
sleep 3

set_perm_recursive $MODPATH 0 0 0777 0777

REPLACE="
"
