# 安装指南

推荐在平板上的 **SunUEFI 引导助手** 中完成安装，不需要填写序列号或运行电脑命令。

## 准备安装包

平板需要解锁 Bootloader，并已安装 KernelSU／SukiSU。安装前备份重要数据。

从 [Build products](https://github.com/slwyts/Project-SunUEFI/actions/workflows/build-products.yml) 的完整构建下载同一版本的三个包：

| 下载包 | 解压后使用的文件 | 用途 |
| --- | --- | --- |
| `SunUEFI-Module` | `SunUEFI-Piano-版本.zip` | 引导助手模块 |
| `SunUEFI-ESP` | `esp.sparse.img` | 启动镜像 |
| `SunUEFI-Root-发行版` | `root-发行版.sparse.img` | Linux 系统镜像 |

Root 可选择 `debian-gnome`、`debian-kde`、`nyarch-gnome` 或 `steamos`。ESP、模块和临时 UEFI 镜像各只有一份；`logs.zip` 用于排查构建问题，普通安装无需下载。

使用 0.5.2 或更新的助手刷写稀疏镜像。将模块 ZIP 和两个 `.img` 文件保存在平板上。模块 ZIP 不要再解压；两个镜像可以放在任意方便的位置，之后通过文件选择器选取。手动选择 `uefi` 或 `linux` 的开发构建不包含完整系统。

此外还有可选的 `SunUEFI-UEFI-Fastboot`，包含独立的
`SunUEFI-UEFI-Fastboot.img`，供 `fastboot boot` 临时启动 UEFI 使用。
助手安装流程不需要下载它，见[临时启动固件](temporary-boot.md)。

## 1. 安装引导助手

在 KernelSU／SukiSU 的模块页面选择模块 ZIP。安装提示出现时：

* **音量＋**：安装或更新引导，保留当前 Android 内核。
* **音量－**：只安装助手，可以稍后在 **维护 → 重新安装引导** 中安装引导。

已有相同版本引导时自动跳过。安装完成后正常重启，在模块卡片上点击 **打开 WebUI**。官方 Magisk 需要 MMRL 等支持 WebUI 的管理器，见[模块说明](../devel/android-module.md)。

首次安装引导，或 HyperOS 更新恢复原厂 BOOT 后重新安装时，如果提示无法确认 BL 已解锁，可能是状态伪装模块隐藏了真实状态。只有临时 Root、实际未解锁的设备写入引导可能变砖。确认实际已解锁后，在 30 秒内连续按下并松开三次实体音量下继续；按音量上或超时都会退出，不写入。确认只对本次安装有效；已有 SunUEFI 引导的更新、启动切换和独立存储操作不检查 BL 属性，也不要求这项按键确认。

## 2. 创建独立存储分区

打开 **存储** 页，点击 **创建独立分区**，选择分配给新系统的容量并确认。完成后正常重启一次，再打开助手。已有独立分区时跳过这一步。

首次分区尚未完成实机验证，见[已知问题](known-issues.md#首次分区)。

## 3. 安装系统镜像

在 **存储** 页的 **安装或更新系统** 中，分别选择 `esp.sparse.img` 和 `root-发行版.sparse.img`，点击 **安装所选镜像**，确认后等待完成。刷写会覆盖独立分区中原有的系统和文件。

## 4. 启动 Linux

在 **启动** 页选择 **UEFI**，选择 **Linux**，点击 **保存并重启**。以后普通开机会沿用这个选择，也可以在开机选择页临时选择 Android。

Linux 默认用户名为 `piano`，密码为 `piano123456`，桌面自动登录。当前 Linux 内核和系统服务启动约 39 秒，不含 UEFI；首次启动可能更久。

## 后续更新

* **更新 Linux**：在存储页选择新版 ESP 和 root 镜像，再安装一次。root 镜像会覆盖原有 Linux 文件，更新前备份自己的数据。
* **HyperOS 更新后恢复引导**：在维护页点击 **重新安装引导**。
* **安装或启动遇到问题**：查看[故障恢复](recovery.md)，必要时使用[备用电脑安装](install-from-computer.md)。
