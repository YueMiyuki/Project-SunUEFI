# 已知问题

按使用中遇到的现象排列，每项写明影响和绕过办法。功能范围见[项目状态](../status.md)。

## 首次分区

引导助手提供 **创建独立分区**，但新平板上的首次分区还没有在设备上验证过。平板上已经有 `sunuefi_esp` 和 `sunuefi_root` 时，直接选择镜像安装即可，不需要重新分区。分区过程中遇到问题，先回到 Android，再按[故障恢复](recovery.md)处理。

## UEFI 菜单与 USB

SimpleInit 菜单和 TianoCore 设置页可以显示，用音量键移动、电源键确认。UEFI 阶段还不能使用触屏、官方键盘和 USB Host，这些功能只在 Linux 里可用。UEFI Fastboot 在菜单、固件设置和 Shell 里一直保持连接。

电脑能识别 UEFI 设备时，可以查询固件：

```sh
fastboot -s SunUEFI-piano getvar product
```

返回 `piano-sunuefi` 说明固件在运行。之后可以用 `oem boot-stable` 启动 Linux，或用 `reboot` 回 Android，见 [UEFI Fastboot](fastboot.md)。电脑看不到设备时，见[故障恢复](recovery.md)。

## 电源和睡眠

* 日常使用挂起，不启用磁盘休眠、混合睡眠和先挂起后休眠。
* 关机偶尔会回到 Android，原因还没查明。
* 双电池和基础 PD 充电可用，高压快充和完整充电周期没有验证。

## 显示和图形

* 默认 120 Hz，可切换刷新率。12-bit / FRC 和连续可变刷新率（VRR）没有实现。
* 部分 GTK 窗口连续缩放时帧率偏低，约每秒提交 30 帧。
* 夜灯使用通用 sRGB 配置，不是原厂校准的颜色。

## 触控笔

定位、压感、144 Hz 笔输入、轻捏、震动和磁吸连接恢复可用。完整掌压、笔身滑动和应用快捷手势还没有完成。

## 相机和其他硬件

* 闪光灯的两路 LED 已注册，实际发光和曝光同步没有观察到。
* 指纹：底层已接入，用户空间的录入与认证尚未完成验证。
* 常见 Windows 游戏（如《原神》）若需特定参数，属于游戏和运行库自身的机制，与固件或系统无关。
* 红外发射没有验证。
* Windows / WinPE 是目标，还不能启动。

## 其他

* 新装系统需要自己通过 NetworkManager 保存 Wi-Fi。
* Recovery 里的独立 SunUEFI 镜像已放弃，见[故障恢复](recovery.md#原厂-recovery)。
* 测试设备是 CSOT 面板、16 GB 内存的平板，其他面板版本和内存容量没有测试过。
* `debian-kde` 由 CI 构建，实机上的桌面和硬件功能没有像 GNOME 那样逐项验证。
* SteamOS 是实验版，游戏兼容性因游戏而异，见 [SteamOS](steamos.md)。
