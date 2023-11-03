#定时方式采用crontab，不会导致额外的电量消耗和资源占用，某些模块采用的是while循环执行（比如循环吃掉垃圾这个模块）会导致额外的性能消耗，不建议使用此方式

#提示：想要看定时进程是否启动，请使用scene软件，在进程管理那边，选择其他进程，搜索crond，如果有这个进程代表启动成功
#开始执行...


# 强制全局240Hz采样率
pm disable com.samsung.android.game.gos/com.samsung.android.game.gos.service.GameIntentService
echo "set_scan_rate,1" > /sys/devices/virtual/sec/tsp/cmd
echo "set_game_mode,1" > /sys/devices/virtual/sec/tsp/cmd






