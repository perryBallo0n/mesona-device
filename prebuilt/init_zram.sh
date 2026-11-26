#!/system/bin/sh
if [ -e /sys/block/zram0/disksize ]; then
    swapoff /dev/block/zram0 2>/dev/null
    echo 1 > /sys/block/zram0/reset 2>/dev/null
    echo 134217728 > /sys/block/zram0/disksize
    if [ -x /system/xbin/mkswap ] && [ -x /system/xbin/swapon ]; then
        /system/xbin/mkswap /dev/block/zram0 && /system/xbin/swapon /dev/block/zram0
    elif [ -x /system/xbin/busybox ]; then
        /system/xbin/busybox mkswap /dev/block/zram0 && /system/xbin/busybox swapon /dev/block/zram0
    fi
fi
