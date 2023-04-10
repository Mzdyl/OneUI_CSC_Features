#!/system/bin/sh
# Please don't hardcode /magisk/modname/... ; instead, please use $MODDIR/...
# This will make your scripts compatible even if Magisk change its mount point in the future
MODDIR=${0%/*}

# This script will be executed in post-fs-data mode
# More info in the main Magisk thread
mount --bind $MODDIR/optics /optics
#mkdir -p $MODDIR/system/vendor/etc
#cp -af /vendor/etc/floating_feature.xml $MODDIR/system/vendor/etc
#
## BixbyTouch
#sed -i '/<\/SecFloatingFeatureSet>/i\<SEC_FLOATING_FEATURE_COMMON_SUPPORT_BIXBY_TOUCH>TRUE<\/SEC_FLOATING_FEATURE_COMMON_SUPPORT_BIXBY_TOUCH>' $MODDIR/system/vendor/etc/floating_feature.xml
## Auto Power on and off
#sed -i '/<\/SecFloatingFeatureSet>/i\<SEC_FLOATING_FEATURE_SETTINGS_SUPPORT_AUTO_POWER_ON_OFF>TRUE<\/SEC_FLOATING_FEATURE_SETTINGS_SUPPORT_AUTO_POWER_ON_OFF>' $MODDIR/system/vendor/etc/floating_feature.xml

settings put global sem_wifi_switch_to_better_wifi_supported 1