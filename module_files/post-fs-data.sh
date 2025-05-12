MODDIR=${0%/*}

LOG_FILE="$MODDIR/log.txt"
echo "日志开始: $(date)" > $LOG_FILE
CSC="`getprop ro.boot.sales_code`"
echo "获取销售代码: $CSC" | tee -a $LOG_FILE
ARCH="`getprop ro.product.cpu.abi`"
echo "获取架构类型: $ARCH" | tee -a $LOG_FILE

# 查找配置目录并记录到日志
CSC_DIR=$(find /optics/configs/carriers -type d -name "$CSC")
if [ -z "$CSC_DIR" ]; then
    echo "错误: 未找到对应的配置目录!" | tee -a $LOG_FILE
    exit 1
else
    echo "找到的配置目录: $CSC_DIR" | tee -a $LOG_FILE
fi

# 配置文件路径并记录到日志
CONFIG_FILE="$CSC_DIR/conf/system/cscfeature.xml"
CARRIER_CONFIG_FILE="$CSC_DIR/conf/system/customer_carrier_feature.json"
if [ ! -f "$CONFIG_FILE" ]; then
    echo "错误: 配置文件 $CONFIG_FILE 不存在!" | tee -a $LOG_FILE
    exit 1
else
    echo "配置文件路径: $CONFIG_FILE" | tee -a $LOG_FILE
fi

# 开始解码 CSC 文件并记录输出
echo "开始解码文件..." | tee -a $LOG_FILE
$MODDIR/libs/$ARCH/decode_csc decode $CONFIG_FILE $MODDIR/decode_csc.xml >> $LOG_FILE 2>&1
$MODDIR/libs/$ARCH/decode_csc decode $CARRIER_CONFIG_FILE $MODDIR/decode_carrier.json >> $LOG_FILE 2>&1

echo "开始复制 floating_feature.xml 文件..." | tee -a $LOG_FILE
cp /etc/floating_feature.xml $MODDIR/ff.xml

# 开始自动修改 CSC 特性文件并记录输出
echo "开始自动修改 CSC 特性文件..." | tee -a $LOG_FILE
$MODDIR/libs/$ARCH/auto_modify_cscfeature $MODDIR/decode_csc.xml $MODDIR/ml_csc.txt >> $LOG_FILE 2>&1

echo "开始自动修改 floating 特性文件..." | tee -a $LOG_FILE
$MODDIR/libs/$ARCH/auto_modify_cscfeature $MODDIR/ff.xml $MODDIR/ml_ff.txt SecFloatingFeatureSet >> $LOG_FILE 2>&1

echo "开始自动修改 运营商 特性文件..." | tee -a $LOG_FILE
$MODDIR/libs/$ARCH/auto_modify_jsoncsc $MODDIR/decode_carrier.json $MODDIR/ml_carrier.txt >> $LOG_FILE 2>&1

# 挂载 CSC 文件并记录输出
echo "开始挂载 CSC 文件..." | tee -a $LOG_FILE
$MODDIR/libs/$ARCH/decode_csc encode $MODDIR/decode_csc.xml $MODDIR/csc.xml >> $LOG_FILE 2>&1
mount --bind $MODDIR/csc.xml $CONFIG_FILE >> $LOG_FILE 2>&1
if [ $? -eq 0 ]; then
    echo "挂载成功: $MODDIR/csc.xml 到 $CONFIG_FILE" | tee -a $LOG_FILE
else
    echo "挂载失败" | tee -a $LOG_FILE
    exit 1
fi

echo "开始挂载 运营商 文件..." | tee -a $LOG_FILE
$MODDIR/libs/$ARCH/decode_csc encode $MODDIR/decode_carrier.json $MODDIR/carrier.json>> $LOG_FILE 2>&1
mount --bind $MODDIR/carrier.json $CARRIER_CONFIG_FILE >> $LOG_FILE 2>&1
if [ $? -eq 0 ]; then
    echo "挂载成功: $MODDIR/carrier.json 到 $CARRIER_CONFIG_FILE" | tee -a $LOG_FILE
else
    echo "挂载失败" | tee -a $LOG_FILE
    exit 1
fi

echo "开始挂载 floating 文件..." | tee -a $LOG_FILE
mount --bind $MODDIR/ff.xml /etc/floating_feature.xml >> $LOG_FILE 2>&1
if [ $? -eq 0 ]; then
    echo "挂载成功: $MODDIR/ff.xml 到 /etc/floating_feature.xml" | tee -a $LOG_FILE
else
    echo "挂载失败" | tee -a $LOG_FILE
    exit 1
fi

# 记录脚本结束
echo "脚本执行结束: $(date)" | tee -a $LOG_FILE