# 已知问题

按使用中遇到的现象排列。功能范围见[项目状态](../status.md)。

## 首次分区

引导助手提供创建独立分区的入口，但新平板首次分区尚未完成实机验证。
已有独立分区时可直接选择 ESP 和 root 镜像进行安装，无需重新分区。

## UEFI 菜单与 USB

UEFI 菜单白屏已修复，SimpleInit 和 TianoCore 设置页面可以显示。设置页面的字号和输入仍在改善；合体 BOOT 冷启动进入 UEFI 时，USB 调试接口仍可能无法连接。

如果电脑能识别 UEFI 设备，可以查询固件：

```sh
fastboot -s SunUEFI-piano getvar product
```

返回 `piano-sunuefi` 就说明固件正常。可以 `oem boot-stable` 启动 Linux，或 `reboot` 回 Android，见[UEFI Fastboot](fastboot.md)。电脑看不到设备时见[返回 Android](recovery.md)。

## 电源和睡眠

* 日常使用挂起，不启用磁盘休眠、混合睡眠或先挂起后休眠。
* 关机偶尔会回到 Android，原因未知。
* 双电池、基础 PD 充电可用；高压快充和完整充电周期没有验证。

## 蓝牙

Linux 启动时自动读取本机原厂蓝牙地址，手工刷写通用镜像也不需要单独填写地址。触控笔在默认 120 Hz 下支持定位、压感、轻捏和震动；其他限制见[笔协议](../devel/piano-pen-protocol.md)。

## 指纹、12-bit／FRC、连续 VRR、Windows

这些功能没有完成，见首页的[尚未完成](../../README.md#尚未完成)。

## 其他

* 新装系统需要自己通过 NetworkManager 保存 Wi-Fi。
* Recovery 里的独立 SunUEFI 镜像已放弃，见[返回 Android](recovery.md)。
* 当前运行使用 CSOT 面板配置，其他面板版本和不同内存容量的平板没有测试过。

## 手写笔扩展功能

定位、压感、144 Hz 笔输入、轻捏、震动和磁吸连接恢复可用。完整掌压、笔身滑动及应用快捷手势仍需完善。显示默认 120 Hz，按设备标识自动选择 BOE／CSOT。
