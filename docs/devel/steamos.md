# SteamOS ARM64

这一目标使用 Valve SteamOS ARM64 根系统，沿用 SunUEFI 的合体 BOOT、UEFI、内核、设备树和独立存储分区。无需替换 ABL，也不使用 SteamOS-ARM-Port 的掌机分区安装器。

这是实验构建目标。默认进入 KDE，登录管理器另有 **Steam Gaming Mode** 会话。Steam 登录、Gamescope 的平板显示与输入、Proton/FEX 游戏兼容性需要实机验证；有 ARM64 客户端不代表所有 Windows 游戏都能运行。

## 下载与安装

在 GitHub Actions 的 **Build products → Run workflow** 中选择 `steamos`。构建完成后下载 Android Module、ESP-steamos 和 Root-steamos。助手里的安装顺序与 Debian 相同：创建独立存储分区，选择 ESP 和 root 镜像，刷写并启动。

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
./build.sh installer --bundle artifacts/release-7.2.9
```

`config/steamos.json` 固定 Valve 镜像、casync 数据源、完整镜像 SHA256 和提取器提交。`config/steam-client.json` 固定公开 ARM64 handheld 客户端各个包的地址、大小和 SHA256。构建不会读取电脑上已登录的 Steam 目录。

`tools/build_steamos_rootfs.py` 组装根系统，`tools/stage_arch_piano.py` 与 Nyarch 共用固件、内核模块和设备服务。`tools/stage_steamos_desktop.py` 处理 KDE、Gamescope 和头显专用服务的差异。原来的 VR 会话及其自动删除游戏的磁盘清理逻辑不启用。

## FFmpeg 和图形运行时

FFmpeg 默认从源码构建为原生 pacman 包并替换根系统中的版本。使用与 Debian 相同的三个 V4L2 补丁，保留 Valve 原包的完整编解码配置和依赖。当前基础镜像使用 FFmpeg 7.0，修补版为同一主要库 ABI 的 7.1.5；构建器拒绝用它覆盖其他 ABI 代际。构建失败时整个 rootfs 构建失败，不会悄悄退回原包。

这会覆盖系统 FFmpeg 及使用它的程序。Steam 或 Electron 自带的私有编解码库不属于系统包，不能用不同 ABI 的共享库强行替换；其硬件视频路径需要分别确认。

图形部分保留 Valve 成套的 ARM64 Mesa 和 x86 客体运行时，不向 SteamOS 安装 Debian 的共享库。传感器依赖在 SteamOS 内重编译为原生包。平板硬件是否全部适用于这套 Valve 图形栈仍需实机验证。

Nyarch 的 FFmpeg 已进入不同 ABI 代际，不能直接安装这里的 7.1 包；本次 SteamOS 默认配置不代表 Nyarch 已经完成对应移植。

## 来源

根系统提取器来自 [SteamOS-ARM-Port](https://github.com/hashtagbasit/SteamOS-ARM-Port/tree/e54dac93afccf76e5fefe998399b6c1b1a23f3a1)，按 GPL-2.0 保留来源。Valve 系统、Steam 客户端及附带组件继续适用各自许可；参考仓库的 GPL 许可不覆盖这些二进制。具体下载身份和构建结果保存在 `SOURCE.json`、根系统 `manifest.json` 和 `packages.tsv`。
