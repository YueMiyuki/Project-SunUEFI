# SteamOS ARM64

这一目标使用 Valve SteamOS ARM64 根系统，沿用 SunUEFI 的合体 BOOT、UEFI、内核、设备树和独立存储分区。无需替换 ABL，也不使用 SteamOS-ARM-Port 的掌机分区安装器。

这是实验构建目标。默认启动入口为 **Steam Gaming Mode**，KDE 作为桌面模式。
游戏模式的完整显示与输入、模式切换、Proton/FEX 游戏兼容性仍在调试；
有 ARM64 客户端不代表所有 Windows 游戏都能运行。

## 下载与安装

在 GitHub Actions 的 **Build products → Run workflow** 中选择 `steamos`。构建完成后下载 `SunUEFI-Module`、`SunUEFI-ESP` 和 `SunUEFI-Root-steamos`。ESP 与其他发行版共用。助手里的安装顺序与 Debian 相同：创建独立存储分区，选择 ESP 和 root 镜像，刷写并启动。

root 镜像容量为 16 GiB，独立 root 分区至少需要相同容量。账户为 `piano`，密码为 `piano123456`。首次启动 Steam 需要联网检查客户端更新并登录。

暂不支持 Valve 的 A/B 系统 OTA；更新系统使用 SunUEFI 助手重新刷写镜像。Steam 客户端自身的正常更新保留。

## 构建

使用项目的 ARM64 Debian 构建容器，安装 `requirements-build.txt`，并以 root 和私有挂载命名空间运行。与 CI 相同的步骤：

```sh
./build.sh sources
./build.sh uefi
./build.sh module
./build.sh linux
./build.sh steamos-rootfs
./build.sh package \
  --rootfs build/distros/steamos-piano/rootfs \
  --rootfs-manifest build/distros/steamos-piano/manifest.json \
  --initramfs build/distros/steamos-piano/initramfs/initramfs.cpio.gz \
  --root-size-mib 16384
./build.sh installer --bundle artifacts/release
```

`config/steamos.json` 固定 Valve 镜像、casync 数据源、完整镜像 SHA256 和提取器提交。`config/steam-client.json` 固定公开 ARM64 handheld 客户端各个包的地址、大小和 SHA256。构建不会读取电脑上已登录的 Steam 目录。

`tools/build_steamos_rootfs.py` 组装根系统，`tools/stage_arch_piano.py` 与 Nyarch 共用固件、内核模块和设备服务。`tools/stage_steamos_desktop.py` 处理 KDE、Gamescope 和头显专用服务的差异。原来的 VR 会话及其自动删除游戏的磁盘清理逻辑不启用。

## FFmpeg 和图形运行时

FFmpeg 默认从源码构建为原生 pacman 包并替换根系统中的版本。使用与 Debian 相同的三个 V4L2 补丁，保留 Valve 原包的完整编解码配置和依赖。当前基础镜像使用 FFmpeg 7.0，修补版为同一主要库 ABI 的 7.1.5；构建器拒绝用它覆盖其他 ABI 代际。构建失败时整个 rootfs 构建失败，不会悄悄退回原包。

这会覆盖系统 FFmpeg 及使用它的程序。Steam 或 Electron 自带的私有编解码库不属于系统包，不能用不同 ABI 的共享库强行替换；其硬件视频路径需要分别确认。

Valve 原生 Mesa 不识别本机的 Adreno 830v1。ARM64 图形包因此使用
`linux/bsp/steamos/mesa/PKGBUILD` 在 SteamOS 中重编译，加入与 Debian /
Nyarch 相同的芯片识别补丁，保留标准 Mesa 路径和 Valve 的软件包名称。
不向 SteamOS 安装 Debian 共享库。x86 客体图形运行库仍保留 Valve 版本，
游戏翻译路径尚需单独验证。

实机已运行 Plasma Wayland，OpenGL 使用 Freedreno，Vulkan 使用 Turnip；
这些结果不能替代硬件视频解码或游戏兼容性验证。

Gamescope 使用参考项目的移动设备适配，并增加分离 DRM 节点的处理：
GPU 节点负责渲染，实际支持 KMS 的 DPU 节点负责输出。实机游戏模式已
显示 Steam 的全屏设置界面，输出为 3200×2136 / 120 Hz。会话使用
`gamescope-session.service`，供 SteamOS Manager 识别游戏模式和切换桌面。
完整的登录后菜单、模式切换和游戏仍需继续验证。

背光沿用 SteamOS 的受限 sysfs 写入接口，会话启动前取得相应权限。
`patches/gamescope/0002-backlight-avoid-double-dimming.patch` 修正移动版
Gamescope 的软件亮度回退：设备已有硬件背光时，不能因写权限不足再把
当前亮度乘进画面。否则移动光标触发 GPU 合成时会变暗，键盘操作恢复
直接输出后又变亮。没有硬件背光时仍保留软件调暗，并保持合成输出。
Gamescope 构建缓存同时记录源码、补丁和包配置的摘要，修改后会重新构建。

FFmpeg 的 H.264 8-bit 测试已实际调用 Iris，解码 720p 的 60 帧并正常退出。
HEVC 10-bit 测试返回格式错误，仍需修复；不能把编码器列表或驱动节点出现
当作所有视频格式已经可用。

Nyarch 的 FFmpeg 使用其当前软件包对应的适配和 ABI，不能直接安装这里的 7.1 包。

## 与掌机体验对齐

基础镜像是 Valve `VARIANT_ID=vr`，不是 Steam Deck 的桌面镜像。对照
[Steam Deck 3.9.3 官方包清单](https://steamdeck-images.steamos.cloud/steamdeck/20261015.100/steamdeck-20261015.100-3.9.3.manifest.pacman)，
补充 Bluetooth、NetworkManager 的 KDE 界面、显示设置和系统信息。
ARM64 使用 Plasma 6.2.5 / Frameworks 6.14 的兼容组件，源码及摘要固定在
`config/steamos.json`，全部构建为 pacman 包，不直接混装更新的 x86 库。

`stage_steamos_desktop.py` 禁用头显专用服务、原设备 MAC 地址设置、
`by-partsets/shared/home` 和 offload 挂载，桌面入口改为原生 Plasma Wayland。
只禁用 offload target 不够：用户登录和临时文件依赖仍可能启动它下面的
挂载任务，导致反复等待不存在的分区。

移除 Frame 留下的系统服务与用户会话 CPU 绑核限制，让应用可使用全部八核。
SteamOS Manager 不提供依赖 Valve A/B 分区的出厂恢复，也不提供 Steam Deck
专用 BIOS、底座和风扇操作。重装系统仍通过 SunUEFI 助手刷写 root 镜像。
ARM64 Steam 客户端当前把 CPU 名称报告为 `Unknown`，频率取 CPU0 的最高频率；
内核 `/proc/cpuinfo` 已提供完整 SoC 名称，八个核心和两组频率也正常暴露。

`prepare_steam_client.py` 处理首次登录时的客户端解包，直接启动 ARM64
程序，保留退出码 42 的正常重启行为。预装文件附带客户端安装清单，
避免首次启动仅因清单缺失而重下整套客户端。在线更新与登录仍由 Steam 负责。

2026-10-21 实机记录：清理原设备服务后，Linux 7.3 的内核和系统服务启动
约 18 秒，不含 UEFI；Steam 登录界面已显示。完整游戏模式、模式切换、
硬件视频解码与 7.3 的 ADSP 稳定性仍在调试。对照同一 SteamOS 根系统，
7.2.9 的 ADSP 和传感器正常，7.3 存在 DSP 崩溃，尚未发布本轮升级。

## 来源

根系统提取器来自 [SteamOS-ARM-Port](https://github.com/hashtagbasit/SteamOS-ARM-Port/tree/e54dac93afccf76e5fefe998399b6c1b1a23f3a1)，按 GPL-2.0 保留来源。Valve 系统、Steam 客户端及附带组件继续适用各自许可；参考仓库的 GPL 许可不覆盖这些二进制。具体下载身份和构建结果保存在 `SOURCE.json`、根系统 `manifest.json` 和 `packages.tsv`。
