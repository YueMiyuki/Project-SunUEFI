# UEFI Fastboot

SunUEFI 运行期间在后台提供一个 USB Fastboot 服务，用来调试和请求启动。即使物理屏幕白屏、没有菜单，也能通过它读日志、截图。它在固件把控制权交给 Linux 时退出。

## 和另外两个“Fastboot”的区别

| | 设备序列号 | 什么时候 | 刷写 |
| --- | --- | --- | --- |
| 原厂 Bootloader Fastboot | 平板真实序列号 | `adb reboot bootloader` | 可以 |
| UEFI Fastboot | `SunUEFI-piano` | 固件运行期间 | 拒绝 |
| Linux 的 USB 网络 / SSH | — | Linux 起来之后 | 不适用 |

下面所有命令都带 `-s SunUEFI-piano`，确保操作的是 UEFI 服务而不是原厂 Bootloader。

服务在 UEFI 菜单、固件设置和 Shell 中保持连接，切换页面不会退出。正常开机进入 UEFI 和临时 Fastboot 启动使用同一套 USB 初始化。

## 状态

```sh
fastboot -s SunUEFI-piano getvar product        # piano-sunuefi
fastboot -s SunUEFI-piano getvar version
fastboot -s SunUEFI-piano getvar storage-policy
fastboot -s SunUEFI-piano oem status
```

## 日志和截图

```sh
fastboot -s SunUEFI-piano oem ramlog
fastboot -s SunUEFI-piano get_staged uefi-ramlog.txt      # 最多 256 KiB

fastboot -s SunUEFI-piano oem screenshot
fastboot -s SunUEFI-piano get_staged uefi-screenshot.bmp  # 24 位 BMP
```

截图读取的是帧缓冲，不代表物理屏幕当前显示的内容。

## 读取分区

```sh
fastboot -s SunUEFI-piano fetch 分区名 文件
```

只读，每次请求最多 64 KiB，由工具分块。

## 启动与退出

| 命令 | 作用 |
| --- | --- |
| `oem boot-stable` | 读取 ESP 中的 `\EFI\Piano\stable\boot.img` 并启动 Linux |
| `oem setup` / `oem shell` / `oem simpleinit` | 切换到 UEFI 设置、Shell 或 SimpleInit 菜单 |
| `reboot` | 清理资源后重启，沿用已保存的启动路线 |
| `continue` | 同样重启，不会改变启动选择或直接启动 Linux |
| `stage 文件` | 下载到内存，上限 64 MiB（目标是 1 GiB） |
| `oem sha256`、`oem discard` | 对暂存数据计算哈希或丢弃 |

## 不支持的

* `flash`、`erase`、`flashing`、槽位切换和未知的 `oem` 命令一律拒绝。
* `oem log` 不存在，用 `oem ramlog`。
* `boot 文件` 还不是通用的系统 / EFI 启动入口。
