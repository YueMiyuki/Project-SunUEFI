# 构建手册

所有构建都在电脑或 CI 上进行，不会向连接的平板写入。统一入口是仓库根目录的 `./build.sh`。

## 取得源码和检查

```sh
./build.sh sources     # 取得固定版本的上游子模块，不递归下载测试数据
./build.sh check       # 按需手动运行主机检查，不需要平板
```

上游源码放在 `upstream/`，保持固定提交。本地修改在 `patches/`、`uefi/`、`linux/`、`tools/`。构建在 `build/` 下的副本里应用补丁，不要改构建副本。

## 构建容器

容器基础镜像记录在 `config/build-container.json`，定义是 `containers/Dockerfile`：

Debian 基础镜像优先从 ECR Public 的官方镜像站下载；遇到镜像元数据下载失败时，自动改用 Docker Hub，两个来源固定同一个 OCI 摘要。下载来源变化不改变基础镜像内容。

```sh
python3 tools/build_container.py
```

APT 包版本会被记录，但不承诺位级可复现。

## UEFI 和安装工具

```sh
docker run --rm -v "$PWD:/workspace" -w /workspace sunuefi-builder bash -euc '
  git config --global --add safe.directory /workspace
  git config --global --add safe.directory "/workspace/*"
  python3 -m venv .venv
  source .venv/bin/activate
  python -m pip install -r requirements-build.txt
  ./build.sh uefi
  ./build.sh module
  ./build.sh installer --product artifacts/product/PianoUEFI-product.img --output artifacts/installer-uefi
'
```

`./build.sh uefi` 使用 `vendor/piano` 的板级输入和 `patches/firmware`，输出 `artifacts/product/PianoUEFI-product.img` 与 `manifest.json`。完整 UEFI 构建还需要本地提取的原厂材料，见[本地输入](local-inputs.md)。

`./build.sh module` 读取刚构建的产品，编译前置选择器和六个静态 ARM64 工具，生成 `artifacts/android/SunUEFI-Piano-版本.zip`、`manifest.json` 与 `SHA256SUMS`。它不需要平板、原厂 BOOT 或私有采集目录；Android 内核在模块安装时从设备当前 BOOT 提取。构建机可以是 x86_64 或 ARM64，默认从经过签名校验的 Debian ARM64 软件包准备编译 sysroot，不执行其中的 ARM64 程序。具体输入和安装行为见 [Android 引导助手](android-module.md)。

## 完整 Debian / GNOME

Actions 默认 `all` 同时构建 `debian-gnome`、`debian-kde`、`nyarch-gnome`、
`steamos`。KDE 沿用下面的 Debian 流程，将根系统步骤改为
`./build.sh release-rootfs --desktop kde`。

需要 ARM64 构建机，容器需要 `--privileged` 才能 chroot 并挂载：

```sh
./build.sh linux
./build.sh mesa
./build.sh sensors
./build.sh ffmpeg --install-dependencies
./build.sh release-rootfs
./build.sh package --root-size-mib 8192
./build.sh installer --bundle artifacts/release
```

* 默认内核源码基线为正式版 Linux `7.3`，使用 `-piano-sunuefi-cachy` 后缀。硬件适配、Gunyah 和对应版本的 CachyOS BORE 按主题维护，固定输入见 [`patches/linux/7.3/series.json`](../../patches/linux/7.3/series.json)。当前选择记录在 `build/release/source-manifest.json`；内核与模块输出到 `artifacts/kernels/release/`。只准备源码：`python3 tools/prepare_release_kernel.py --refresh`。具体改动见[内核与调度配置](kernel-roles.md)。
* 重建内核时，旧产物清单先作废，编译完成后才原子发布新的，避免旧 Image 与新模块混用。O 目录与产物目录有文件锁。
* 基础根系统已构建后，用 `./build.sh release-rootfs --resume` 更新内核、硬件包和配置，不重新运行 debootstrap。
* 其他目标：`./build.sh rootfs --distro ID --desktop ID --plan` 列出 Ubuntu / Deepin / Arch 与 KDE 等组合的计划，目前只有 Debian / GNOME 完整验证。

本地和 GitHub 的完整构建使用同一个 `release-rootfs` 入口，默认包含中文、
拼音输入法、Chromium、GNOME 动画、蓝牙地址自动初始化和触控笔组件。
根系统生成后检查这些包与必需文件，记录到 `manifest.json` 的 `default_system`；
缺失时构建失败，不依赖首次启动后的手工补装。APT 使用有签名的更新仓库，
因此不同日期构建的软件包版本可能不同，不要求二进制逐字节一致。

### 软件包的本地构建

SteamOS 的独立目标、原生 FFmpeg 软件包与相同 ESP/root 打包流程见 [SteamOS ARM64](steamos.md)。

* **CPU 型号**：`linux/dts/piano-cpu-model.dtso` 给八个 CPU 节点加标准 `model` 属性，内核小补丁把它输出为 `/proc/cpuinfo` 的 `model name`，让 GNOME 显示 Snapdragon 8 Elite。MIDR、核心拓扑、时钟和板型号保持原值。
* **gnome-settings-daemon**（`./build.sh gsd`）：在副本里编译带 Piano 自动亮度策略的标准包，使用原厂 lux 阈值和延迟，不修改传感器读数，再经 APT 安装。
* **Mutter**（`./build.sh mutter`）：修正空 Gamma 曲线的恢复，以及无 EDID 的内置屏幕颜色配置。保留标准颜色服务和用户配置，不伪造 EDID；通用 sRGB 是未校准的默认值，不等于原厂色彩校准。来源与补丁见[Mutter 适配说明](../../linux/desktops/gnome/patches/mutter/README.md)。

## Nyarch / GNOME

在相同 ARM64 构建容器中，完成 `sources`、`uefi`、`module`、`linux` 后运行
`./build.sh nyarch-gnome-rootfs`。它下载 Arch Linux ARM 基础系统，通过签名仓库安装
当前软件包，构建 Nyarch 应用和 Piano 原生硬件包，再安装 Material UwU、
模糊主题与桌面扩展。源码和扩展选择见 `config/nyarch.json`。

打包时向 `./build.sh package` 传入
`--rootfs build/distros/nyarch-gnome-piano/rootfs`、
`--rootfs-manifest build/distros/nyarch-gnome-piano/manifest.json`、
`--initramfs build/distros/nyarch-gnome-piano/initramfs/initramfs.cpio.gz` 和
`--root-size-mib 16384`，之后按相同流程运行 `installer`。

所有发行版通过 `tools/stage_piano_firmware.py` 安装同一套设备固件。
`config/release.json` 的 `firmware.overlays` 选择完整的 HyperOS 4 ADSP/
ADSP-DTB 更新，覆盖旧版对应文件；来源、大小和 SHA256 随固件清单保存。

## 合体 BOOT 相关工具

```sh
./build.sh trampoline --stock-boot 当前ROM的boot.img --output 输出目录   # 生成前置选择器入口
./build.sh boot-repack                                                   # 编译原生 BOOT 重打包 / 还原工具
./build.sh boot-request --sysroot ...                                    # 编译 piano-boot-request
./build.sh module                                                        # 从当前 UEFI 产品编译并打包 Android 模块
./build.sh module-package --inspect                                      # 低层打包工具：只检查已有输入
```

模块支持从当前原厂 BOOT 安装合体引导、更新已有核心，以及在 HyperOS 更新后通过“重新安装引导”恢复启动选择器。分区和镜像刷写在助手的“存储”页完成。见[原生 BOOT 重打包](android-boot-repack.md)、[Android 模块](android-module.md)。

## CI

GitHub Actions 的 **Build products** 默认构建 Debian GNOME、Debian KDE、Nyarch GNOME 和 SteamOS。手动运行时可以选择 `all`、任一发行版，或仅构建 `uefi`／`linux`。

完整构建提供以下下载：

| 下载名称 | 内容 |
| --- | --- |
| `SunUEFI-Module` | Android 引导助手模块 ZIP |
| `SunUEFI-UEFI-Fastboot` | 用于 `fastboot boot` 的独立 UEFI 镜像 |
| `SunUEFI-ESP` | 共用的稀疏 ESP 镜像 |
| `SunUEFI-Root-发行版` | 对应发行版的稀疏 root 镜像 |
| `logs` | 合并的构建日志、来源清单和校验记录 |

模块、UEFI 和 ESP 每轮只上传一份，各发行版分别上传 root 镜像。安装时将所选发行版的 root 与同一轮 ESP 配套使用；日常安装步骤见[安装指南](../user/install-from-artifact.md)。

不再单独运行 `host-checks.yml` 的 portable 检查。需要对应检查时，在本地运行 `./build.sh check`。依赖和来源记录见[公开构建链](public-build.md)。

## 继续阅读

[仓库地图](repository-map.md)、[测试](testing.md)、[发布要求](release.md)、[贡献指南](../../CONTRIBUTING.md)。
