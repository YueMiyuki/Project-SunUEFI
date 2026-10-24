# Project SunUEFI

小米平板 8 Pro（代号 `piano`，骁龙 8 至尊版 / SM8750）的统一 UEFI 与 Linux 适配项目。

目标是让这块平板通过 UEFI 获得完整的 Linux 和 Windows 体验，同时保留原厂 Android。

## 重点功能：合体 BOOT

平板的原厂 Android 基本保持不动。SunUEFI 在它旁边加入一套 **UEFI（EDK2）**，与原厂内核共用同一个 **BOOT 镜像**，让同一块平板可以从内部存储直接启动其他系统（如 Debian），同时保留原厂 Android 的日常启动与 Recovery；**Linux 已经能体验近乎完美，Windows 还在计划之中。**

> **合体 BOOT 的实现原理**
>
> 原厂 ABL 在 `recovery` 自带内核时不会加载 `pvmfw`，导致 Recovery 无法启动；替换 BOOT 内核又会让 HyperOS 起不来。
>
> 所以**内核留在原位，只把入口分支改向一个小选择器**。选择器和 UEFI 接在内核之后，由 ABL 按 header 的 `kernel_size` 整段加载；普通启动与 Recovery 跳回原厂入口，**只有请求 UEFI 时才进 EDK2**。

[开始使用](#开始使用) · [功能与已知问题](#功能与已知问题) · [故障恢复](docs/user/recovery.md)

## 现在能做什么

从内部磁盘启动的 Debian 13 / GNOME 已经可以：

* 用 Adreno GPU 加速绘制桌面，屏幕以 3200×2136、默认 120 Hz 显示，支持刷新率切换、自动亮度和 HDR 内容播放。
* 用手指点按、拖动窗口、长按弹出菜单和使用手势。
* 接上官方磁吸键盘打字、使用触控板，键盘背光可以通过 GNOME 的标准选项调节。
* 官方键盘的 **右 Ctrl + `1`～`0`、`-`、`=`** 对应 **F1–F12**，见[键盘快捷键与 UEFI 接入进度](docs/user/keyboard.md)。
* 使用 Wi-Fi 和蓝牙。
* 用四个扬声器播放声音，并用 GNOME 夜灯调整屏幕色温。
* 打开前后相机预览并录像，摄像头工作时亮起实体绿色隐私灯。
* 使用麦克风录音、自动旋转和传感器；合盖、熄屏、挂起后可以正常恢复。
* 使用触控笔定位、压感绘画、轻捏和震动，及磁吸连接恢复。
* 使用默认中文界面、拼音输入法和 Chromium 浏览器，GNOME 动画默认开启。
* 在 Linux 和原厂 Android 之间来回切换。

UEFI 显示硬件初始化进度，Linux 启动正常有日志提示。仍存在的限制集中列在下文[功能与已知问题](#功能与已知问题)。

## 实机画面

| | |
| --- | --- |
| ![Android 与 UEFI 开机选择](docs/assets/gallery/boot-selection.webp)<br>开机选择 | ![TianoCore 启动画面](docs/assets/gallery/uefi-splash.webp)<br>UEFI 启动画面 |
| ![GNOME 系统信息与相机预览](docs/assets/gallery/gnome-camera.webp)<br>Debian / GNOME 与相机 | ![Android 引导助手界面预览](docs/assets/gallery/android-boot-helper-preview.webp)<br>Android 引导助手 |


| Minecraft · Vulkan / Turnip | Minecraft · OpenGL / Freedreno |
| --- | --- |
| ![原版 Minecraft 26.3 Vulkan 实机画面](docs/assets/gallery/minecraft-vulkan.webp) | ![原版 Minecraft 26.3 OpenGL 实机画面](docs/assets/gallery/minecraft-opengl.webp) |

两张均为原版 Minecraft 26.3，无优化模组，在平板 3K 屏幕上运行。照片分别显示约 446 FPS 和 174 FPS，为不同场景的瞬时读数。Vulkan 的分辨率显示差异可能与驱动 bug 或 Wayland 缩放有关。

### SteamOS 与游戏

| Steam 游戏模式 | 原神 · 游戏内 |
| --- | --- |
| ![小米平板 8 Pro 上的中文 Steam 游戏模式](docs/assets/gallery/steamos-gaming-mode.webp) | ![小米平板 8 Pro 运行原神，角色已进入游戏世界](docs/assets/gallery/steamos-genshin-world.webp) |
| **原神 · 官方启动器** | **魔法少女的魔女审判** |
| ![SteamOS 中的原神官方启动器](docs/assets/gallery/steamos-genshin-launcher.webp) | ![小米平板 8 Pro 运行魔法少女的魔女审判剧情画面](docs/assets/gallery/steamos-witch-trial.webp) |

## 开始使用

准备一台已解锁 Bootloader、已安装 KernelSU／SukiSU 的小米平板 8 Pro，并备份重要数据。日常安装在平板上的 **SunUEFI 引导助手** 中完成，无需电脑命令或填写序列号。

1. **下载三个包**：从 [Build products](https://github.com/slwyts/Project-SunUEFI/actions/workflows/build-products.yml) 的完整构建下载引导助手、ESP 和 Root。ESP 为四个发行版共用，Root 按想使用的发行版选择。解压后得到模块 ZIP、`esp.sparse.img` 和对应的 `root-发行版.sparse.img`。
2. **安装并打开助手**：在模块管理器中安装模块 ZIP，提示出现时按音量＋安装引导；重启后，点击模块卡片上的 **打开 WebUI**。
3. **分配存储空间**：在 **存储 → 创建独立分区** 中选择容量并确认，完成后重启。已有独立分区时跳过这一步。
4. **刷写镜像**：在 **存储** 页分别选择下载好的 `esp.sparse.img` 和 `root-发行版.sparse.img`，点击 **安装所选镜像**，等待完成。
5. **进入 Linux**：在 **启动** 页选择 **UEFI → Linux**，点击 **保存并重启**。

首次启动会自动把 root 文件系统扩展到已创建的独立分区容量，无需手动扩容；不会占用额外的 Android 空间。

Root 提供 `debian-gnome`、`debian-kde`、`nyarch-gnome` 和 `steamos` 四种选择；例如 Nyarch 的下载包名为 **`SunUEFI-Root-nyarch-gnome`**。

首次分区尚未完成实机验证，相关限制见[已知问题](docs/user/known-issues.md#首次分区)。详细操作见[安装指南](docs/user/install-from-artifact.md)；[电脑安装](docs/user/install-from-computer.md)与[临时启动](docs/user/temporary-boot.md)作为备用方式保留。Magisk 用户打开助手的方法见[模块说明](docs/devel/android-module.md)。

| 登录平板的 Linux 系统 | 默认账号密码 |
| --- | --- |
| 用户名 | `piano` |
| 密码 | `piano123456` |

在登录页面选择 `piano`，输入密码 `piano123456` 即可进入桌面。系统默认自动登录；锁屏解锁和 `sudo` 也使用这个密码，可在系统设置中修改。

首次启动需要生成部分设备配置，可能比后续启动更久。

仅临时运行 UEFI 时，另下载 **`SunUEFI-UEFI-Fastboot`**，解压后执行 `fastboot boot SunUEFI-UEFI-Fastboot.img`。这是不含原厂 Android 内核的独立镜像，与助手里的 UEFI 使用同一核心；不会安装 Linux 或改写 BOOT。操作说明见[临时启动固件](docs/user/temporary-boot.md)。

HyperOS 更新后，在助手的 **维护** 页点击 **重新安装引导**。想回 Android，可以在开机选择页按音量＋选择 Android、短按电源确认；Linux 下也可运行 `piano-next-boot android --reboot`。遇到无法启动的情况，见[故障恢复](docs/user/recovery.md)。

## 开机时发生什么

SunUEFI 把开机选择器、原厂 Android 内核和 UEFI 核心放在同一个启动镜像里，称为**合体 BOOT**。开机时选择 Android 就进入 HyperOS；选择 UEFI 可以启动 Linux 或进入固件菜单。小米原厂的底层引导程序和 Recovery 保留。

```mermaid
flowchart LR
    A["原厂引导程序<br/>XBL / ABL"] --> B["合体 BOOT 中的选择页<br/>Android / UEFI · 3 秒"]
    B -->|"Android"| C["原厂 Android 内核<br/>HyperOS"]
    B -->|"Linux / 菜单"| D["SunUEFI (EDK2)"]
    D --> E["ESP 中的 Linux 内核<br/>sunuefi_esp"]
    E --> F["ext4 根文件系统<br/>sunuefi_root"]
    F --> G["Debian / GNOME"]
    D -.->|"目标，尚未实现"| H["Windows"]
```

开机选择页停留约三秒，音量键切换，短按电源确认；没有操作时沿用保存的选择。Android 路线直接进入原厂内核，Linux 路线由 UEFI 从独立存储分区加载系统。实现细节见[合体 BOOT](docs/devel/android-boot-repack.md)。

### 启动路线会被记住

在助手中保存的选择会成为默认启动路线；开机时用音量键临时改选，不会改变这个默认值。首次安装默认进入 Android。

* 在 Android 的 SunUEFI 引导助手中选择 Android 或 UEFI，保存并重启；已安装 Linux 时可以直接进入 Linux。
* 在 Linux 里执行 `piano-next-boot android --reboot` 回到 Android。
* 用 Android 标准的 `reboot recovery` 进入小米原厂 Recovery，不改动已保存的路线。

引导助手提供中文／英文界面。HyperOS 更新后，点击 **重新安装引导** 即可使用新系统的内核恢复引导，保留启动选择；目前需要手动点击。使用方法见[模块说明](docs/devel/android-module.md)，底层实现见[启动状态与切换](docs/devel/reboot-request.md)。

### UEFI 里的 Fastboot

SunUEFI 运行时在后台提供一个 USB Fastboot 服务，设备名为 `SunUEFI-piano`。它用来调试：即使屏幕没有画面，也能从电脑读取日志、导出屏幕截图、只读读取分区内容，或请求启动已安装的 Linux。

它和另外两个容易混淆的东西不同：

| 名称 | 什么时候出现 | 能做什么 |
| --- | --- | --- |
| 原厂 Bootloader Fastboot | 开机时按键进入，或 `adb reboot bootloader` | 小米原厂功能，可刷写分区 |
| UEFI Fastboot（本项目） | SunUEFI 运行期间 | 日志、截图、只读读取、请求启动；不支持刷写和擦除 |
| Linux 的 USB 网络 / SSH | Linux 启动之后 | 普通的 Linux 远程登录与调试 |

交接给 Linux 时 UEFI Fastboot 就会退出。从冷启动的合体 BOOT 进入时，USB 初始化曾经超时，所有后台场景也还没有走通，所以现在不能依赖它。命令见 [UEFI Fastboot](docs/user/fastboot.md)。

### 同一个核心

项目只维护一套 UEFI 核心和一套产品镜像 `PianoUEFI-product.img`。合体 BOOT、临时的 `fastboot boot` 启动、安装器用到的都是同一份核心，不再按功能拆成多套测试镜像。

## 功能与已知问题

当前运行使用 CSOT 面板配置，选择方式见[面板选择](docs/devel/panel-selection.md)。

### 可以使用

| 功能 | 说明 |
| --- | --- |
| 磁盘启动 Linux | 从合体 BOOT 经 UEFI 进入内部 ESP 和 ext4 系统，普通重启沿用选择 |
| 返回原厂 Android | Android 路线直通，Linux 里可以一条命令切回 |
| GNOME 桌面与 GPU 加速 | 原生双 DSI 显示、Adreno 加速 |
| 屏幕显示与亮度 | 3200×2136、默认 120 Hz，刷新率切换、手动及自动亮度 |
| 触屏 | 点按、拖动、长按、手势，报告速率约 144 Hz |
| 官方键盘与触控板 | 打字、指针、点击、键盘背光 |
| Wi-Fi | 扫描并联网 |
| 蓝牙 | 可用 |
| 触控笔 | 定位与压感绘画、笔输入、轻捏按键、笔端震动和磁吸连接恢复 |
| 四扬声器 | 沿用原厂功放增益，音量可调 |
| 夜灯 | GNOME 夜灯调整色温 |
| HDR 内容 | 可播放；当前显示传输为十位，12-bit／FRC 与连续 VRR 的实现范围见[显示说明](docs/devel/piano-display-depth.md) |
| 前后相机 | 预览、录像、自动曝光／对焦／白平衡及实体隐私灯 |
| 麦克风 | 原生单声道录音，睡眠恢复 |
| 传感器 | 自动旋转、光线、距离、罗盘及标准 SensorProxy 接口 |
| 电源 | 双电池、PD 充电、锁屏、合盖、熄屏、挂起与唤醒；键盘背光默认 80% |
| 默认省电设置 | 2 分钟熄屏，电池 5 分钟、外接电源 10 分钟自动挂起 |
| 启动画面 | UEFI 硬件初始化进度及 Linux 启动提示 |
| 默认桌面 | 中文界面、拼音输入法、Chromium 浏览器，GNOME 动画开启 |

### 可以使用，但有问题

| 功能 | 能做到什么 | 现存问题 |
| --- | --- | --- |
| UEFI 菜单 / 固件设置 | 中文菜单与放大的 TianoCore 设置页面，支持音量键移动、电源键确认和双音量键返回 | UEFI 阶段触屏和键盘尚未接通 |
| UEFI Fastboot | 日志、截图、只读读取 | 冷启动 USB 初始化曾超时，部分后台场景未走通 |
| 闪光灯 / RGB 灯 / 红外 | 闪光灯的标准 LED 接口已注册；RGB 灯接口已出现 | 闪光灯实际发光未观察；红外发射未验证 |

### 尚未完成

* **触控笔的扩展功能**：完整掌压、笔身滑动及应用快捷手势仍需完善。
* **12-bit／FRC、连续可变刷新率**：不等同于十位传输、HDR 内容播放或固定刷新率切换，尚未提供完整支持。
* **指纹解锁**：内核层面理论可用，正在适配录入、认证和桌面集成。
* **Windows / WinPE**：目标，尚未启动。

更详细的现状和设备记录见[项目状态](docs/status.md)，用户可见的问题和绕过办法见[已知问题](docs/user/known-issues.md)。

## 自己构建

先取得固定版本的上游源码，再在构建容器里生成 UEFI 和安装工具包：

```sh
./build.sh sources
docker build -t sunuefi-builder -f containers/Dockerfile .
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

模块输出在 `artifacts/android/`，与 `artifacts/product/` 中的 UEFI 使用同一份核心。GitHub Actions 默认构建 Debian GNOME、Debian KDE、Nyarch GNOME 和 SteamOS，每个系统目标都提供配套的模块、ESP 和 root 镜像；也可以手动选择单个目标。

完整的 Debian / GNOME 系统包需要 ARM64 构建机，以及允许 chroot 的 `--privileged` 容器，依次运行 `./build.sh linux`、`mesa`、`sensors`、`ffmpeg --install-dependencies`、`release-rootfs`、`package`、`installer`。需要主机检查时手动运行 `./build.sh check`，不用连接平板。完整流程、输入材料和各步骤的输出位置见[构建手册](docs/devel/building.md)与[公开构建链](docs/devel/public-build.md)。

**SteamOS ARM64（实验版）**：使用 Valve ARM64 根系统、Steam 客户端、KDE 与 Gamescope，默认进入游戏模式，可以切换桌面模式。预装 ARM64 Proton／FEX、DXVK 和 vkd3d-proton，Milthm 与原神已运行；原神需单独设置 FEX 参数。安装仍通过助手选择 ESP 和 root 镜像，详见 [SteamOS 构建说明](docs/devel/steamos.md)。

## 项目站在哪些项目之上

SunUEFI 不是从零重写所有东西。上游源码保持固定版本放在 `upstream/`，我们自己的修改放在 `patches/`、`uefi/`、`linux/` 和构建工具里。

| 项目 | 负责什么 | 我们在上面做了什么 |
| --- | --- | --- |
| [TianoCore EDK2](https://github.com/tianocore/edk2) / [Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium) | UEFI 基础代码和高通平台集成 | Piano 平台、启动交接、存储、输入、显示继承、UEFI Fastboot，以及对原厂分区的只读保护和安全退出 |
| [mu_wide_platforms](https://github.com/sunflower2333/mu_wide_platforms) | 基于 Project Mu 的高通设备 UEFI，包含小米平板 8 Pro 的 Piano 平台与 ACPI | 作为同机 UEFI 初始化、ACPI 和 Windows 引导适配的参考 |
| [Xiaomi Pad 6S Pro Linux](https://github.com/code002-2/Xiaomi-pad-6s-pro-Linux) / [debian-sheng](https://github.com/ianchb/debian-sheng) | 小米平板的多发行版适配，以及充电、手写笔和指纹方案 | 参考 MiPPS 认证、磁吸笔连接和 TEE 指纹接入，按 Piano 的实际硬件移植 |
| [simple-init](https://github.com/BigfootACA/simple-init) | UEFI 里的图形启动菜单 | 平板布局、中文、实体按键，以及和固件后台服务的联动 |
| [linux-piano](https://github.com/blu-sharky/linux-piano) | SM8750 内核、设备树和板级驱动基线 | 合入稳定版内核，修正内存 / DMA、显示、音频等 |
| [CachyOS/kernel-patches](https://github.com/CachyOS/kernel-patches) / [BORE](https://github.com/firelzrd/bore-scheduler) | 注重交互响应的 CPU 调度改进 | 将对应内核版本的 BORE 补丁集成到四个发行版共用的 ARM64 内核 |
| [librevm](https://github.com/code002-2/librevm) | Android Gunyah 驱动的主线接口适配 | 移植到当前内核，由 UEFI 传递每次启动的资源信息；虚拟机运行支持仍在调试 |
| [BigfootACA/linux](https://github.com/BigfootACA/linux) · Junhao Xie | 高通显示驱动改进 | 采用 DSI PLL 初始化、DPU 视频流水线停止顺序及 DSI PHY 接口时钟管理补丁，改善显示初始化与熄屏恢复；精确来源见 [THIRD_PARTY.md](THIRD_PARTY.md) |
| [debian-piano](https://github.com/blu-sharky/debian-piano) | Debian 根系统构建、设备服务、相机和输入辅助程序 | 专用 ESP / root、多发行版 BSP（板级配置包）、统一打包 |
| [piano-firmware](https://github.com/blu-sharky/piano-firmware) | Wi-Fi、蓝牙、DSP、触控等设备固件和功放预设 | 按来源装入系统；设备参数从本机原厂数据读取。它不是整套 HyperOS，也不含原厂相机算法 |
| [piano-mesa](https://github.com/blu-sharky/piano-mesa) | Adreno 图形用户空间 | 与内核和根系统配套的 Mesa 软件包 |
| [piano-sensors](https://github.com/blu-sharky/piano-sensors) | 高通传感器（SSC）通信与桌面桥接 | 数据类型、读取接口、标准 SensorProxy 与自动亮度策略 |
| [SteamOS-ARM-Port](https://github.com/hashtagbasit/SteamOS-ARM-Port) | Valve ARM64 根系统提取方法和移动设备移植参考 | 复用固定版本提取器，接入 SunUEFI 的 ESP/root、内核及 BSP；不采用其设备启动器和分区方案 |
| [MiCode](https://github.com/MiCode) 与本机 HyperOS | 原厂设备树、驱动、配置的参照 | 对照真实命令、时序和参数，按标准接口移植 |

Debian 软件通过 APT 安装，Nyarch 和 SteamOS 使用原生 pacman 包。Nyarch 从 Arch Linux ARM 和应用源码构建，不依赖旧版 ISO；SteamOS 仍是实验目标。Ubuntu、Deepin 等配置暂未纳入默认构建。

## 文档

* [快速上手](docs/user/getting-started.md)、[安装指南](docs/user/install-from-artifact.md)、[返回 Android](docs/user/recovery.md)、[UEFI Fastboot](docs/user/fastboot.md)、[已知问题](docs/user/known-issues.md)
* [项目状态](docs/status.md)：功能和设备记录
* [文档导航](docs/README.md)：开发、专题和历史记录
* [第三方来源](THIRD_PARTY.md)、[许可](LICENSE.md)、[贡献指南](CONTRIBUTING.md)、[AI 协作规范](AGENTS.md)
