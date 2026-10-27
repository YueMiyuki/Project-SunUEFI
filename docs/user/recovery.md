# 返回 Android 与故障恢复

SunUEFI 不替换原厂引导程序，也不修改 Android 的系统分区，UEFI 阶段对原厂关键分区由代码强制只读。出问题时，按现象在下表里找对应办法。

| 现象 | 原因 | 怎么做 |
| --- | --- | --- |
| 在 Linux 里想回 Android | 保存的启动路线是 Linux | 运行 `piano-next-boot android --reboot`，或点桌面上的“返回 Android” |
| 在 Android 里想改回 Linux 或 Android | 路线保存在 BOOT 分区的记录区 | 在助手的 **启动** 页重新选择；没有助手时见下文“用命令改启动路线” |
| 屏幕停在原厂 Fastboot | 没有选到系统，或刷写后没有重启 | 电脑上运行 `fastboot reboot` |
| 电脑能看到 `SunUEFI-piano` | 固件在运行，屏幕可能白屏 | 运行 `fastboot -s SunUEFI-piano reboot`，固件先释放 USB、存储、时钟和 SMMU（IOMMU，限制外设访问内存），再冷重启回 Android |
| 完全没有反应 | 死机或卡在早期启动 | 长按电源键强制重启，直到出现开机画面。按多久因机器而异，用你已知的原厂强制重启方式 |
| 想进小米原厂 Recovery | 需要原厂恢复功能 | 在 Android 里运行 `reboot recovery`，或用系统的重启菜单。不会触发 SunUEFI，也不改变保存的启动路线 |

强制重启后仍然回不到 Android 时，先不要继续刷写，记下屏幕状态和电脑上 `fastboot devices` 的输出再求助。

## 用命令改启动路线

在 Android 里用 root 执行。BOOT 分区按当前槽位选择：

```sh
BOOT=/dev/block/by-name/boot$(getprop ro.boot.slot_suffix)
piano-boot-request status --device "$BOOT"
piano-boot-request set --device "$BOOT" --target android
```

`--target` 可选 `android`、`menu`、`linux`、`setup`。BOOT 分区里没有有效记录时，系统默认进入 Android。

## 原厂 Recovery

SunUEFI 不使用 Recovery 分区安装。原厂引导程序不会为独立的 Recovery 镜像加载 vendor_boot 和 pvmfw，启动会停在 Fastboot，所以安装器拒绝 `--recovery`。如果之前自己刷过类似的镜像，请恢复原厂 Recovery 镜像。保存了 Linux 路线的同时进入 Recovery，在设备上还没有试过。

## 关于数据

`fastboot boot` 只在内存里运行。安装器和助手只写 `sunuefi_esp` 和 `sunuefi_root` 两个分区，不碰其他分区。合体 BOOT 会改写 BOOT 分区，恢复原样需要原厂 BOOT 镜像，可以用[合体 BOOT 的还原工具](../devel/boot/combined-boot.md)无损还原。动手之前先备份 BOOT 分区。
