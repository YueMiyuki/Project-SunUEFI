# 项目状态

更新日期：2026-10-12。本页按功能列出当前状态。首页的表格是它的简化版；各项的技术细节在 `docs/devel/` 的对应页面中。

## 测试设备

所有结果来自同一台小米平板 8 Pro（`piano`，型号 `25091RP04C`，SM8750P，16 GB 内存），原厂已更新到 Android 17／HyperOS 4（`OS4.0.4.0.XPYCNXM`，vendor `OS4.0.2.1.XPYCN`），Bootloader 已解锁，当前运行使用 CSOT 面板配置（由原厂引导程序传来的标识选择，不等同于对物理供应商的识别）。Linux 对面板的选择见[面板选择](devel/panel-selection.md)；其他面板、内存容量和 ROM 版本没有测试过。

这台机器划出了 `sunuefi_esp`（512 MiB FAT32，4 KiB 扇区，与 UFS 逻辑扇区一致）和 `sunuefi_root`（63.5 GiB ext4），安装了 Debian 13 / GNOME。当前 Linux 内核为 `7.2.9-piano-gnome-gd33a42990baf`，从合体 BOOT 经 UEFI 启动，已实际来回切换 Android 与 Linux。

## 启动与安装

| 项目 | 状态 |
| --- | --- |
| 合体 BOOT：Android 直通 | 可用。普通开机进入原厂 Android |
| 合体 BOOT：保存 Linux 路线并重启 | 可用。路线在 Android 侧保存，重启后经 UEFI 进入内部磁盘 Linux |
| Linux 返回 Android | 可用。`piano-next-boot android --reboot`，见[Linux 下次启动入口](devel/linux-next-boot.md) |
| 原厂 Recovery | 保留。`reboot recovery` 进入 Mi Recovery。持久 Linux 偏好与 Recovery 同时存在时的行为还没有在设备上试过 |
| 独立 Recovery 安装 | 不可用。ABL 对“Recovery 自带内核”的分支不加载 vendor_boot 和 pvmfw，启动停在 Fastboot；合体 BOOT 走普通启动路径，绕开了这个问题。安装器拒绝 `--recovery` |
| 启动路线管理 | 模块保存 Android／UEFI 路线，普通开机沿用；前置选择页显示约三秒，音量键可临时改选，电源键确认。临时选择不改保存的路线 |
| 合体 BOOT 在线安装 | 模块从当前 BOOT 提取 Android 内核并安装或更新同一份 UEFI 核心；HyperOS 更新后可点击“重新安装引导”。已有核心升级已在设备上完成，所有 OTA 场景尚未覆盖。见[原生 BOOT 重打包](devel/android-boot-repack.md) |
| 一键安装器 | 更新已有的 `sunuefi_esp` 与 `sunuefi_root`；蓝牙地址由 Linux 自动读取，不依赖安装阶段配置。新平板首次分区返回 `NEW_INSTALL_NOT_READY` |
| Android Root 模块 | 提供可安装 ZIP，包含启动选择、引导检查、重新安装和独立存储管理。SukiSU 安装和 WebUI 可用；新分区、调整容量和删除归还空间的写入流程仍需实机验证，见[引导助手](devel/android-module.md) |

## UEFI

| 项目 | 状态 |
| --- | --- |
| 存储与保护 | 能读取 UFS 的 GPT、BlockIO 和 ESP 文件；原厂关键分区由代码强制只读 |
| 交接与退出 | 启动 Linux 前清理 USB、UFS、时钟与 SMMU，无法确认安全时拒绝交接 |
| 显示 | 合体 BOOT 进入 UEFI 后可显示菜单；延后原厂 ClockDxe 的 CESTA/media 和 DCD 启动初始化，保留继承的显示状态。见[显示定位](devel/uefi-display-checkpoints.md) |
| 菜单 / Setup / Shell | SimpleInit 菜单、TianoCore 中文设置页面与四倍字号可用；音量键移动、电源键确认、双音量键返回可用，见[设置页面](devel/uefi-console.md)；Shell 尚未完成实机验证 |
| 触屏、键盘、USB Host | UEFI 阶段没有完成，不使用 Linux 的结果代替 |
| UEFI Fastboot | 已实现：`oem status`、`oem ramlog`、`oem screenshot`、只读 `fetch`、`stage`（≤64 MiB）、`oem boot-stable`、`reboot`。拒绝 `flash`/`erase`。冷启动合体 BOOT 进入时 USB 初始化曾超时；完整命令见[UEFI Fastboot](user/fastboot.md) |
| EFI 变量 | 保存在内存，没有持久存储 |
| 任意 `.img` / `.efi` 启动 | 没有完成，目前只启动 ESP 中的 Linux |

## Linux 与硬件

| 项目 | 状态 | 已知问题 |
| --- | --- | --- |
| 显示 | 3200×2136，默认 120 Hz，十位输出、夜灯、标准 Gamma／Degamma、30／48／50／60／120 Hz 无缝切换及熄屏恢复 | HDR、12-bit FRC、按内容自动选刷新率仍未完成；144 Hz 可能花屏，笔输入需继续适配 |
| GPU | Adreno 加速的 GNOME | 部分 GTK 窗口连续缩放约 30 次/秒画面提交，需继续区分客户端处理和帧回调等待；见[窗口缩放](devel/piano-window-resize.md) |
| 触屏 | 点按、拖动、长按、手势，约 144 Hz | 360 Hz 未实现；依赖主机端触点算法和 uinput |
| 键盘 / 触控板 | 可用，键盘背光走 UPower / GNOME | 特殊键和睡眠恢复未完成 |
| 触控笔 | 默认 120 Hz 下可定位和压感绘画，使用标准手写笔输入；轻捏与震动可用。内核支持磁吸识别、发射器供电、笔电量和充满停电；跨系统配对失效时自动重新配对 | 首次连接延迟、144 Hz 笔输入、完整掌压、低电量无线充电及睡眠期间取放仍需完善，见[笔协议](devel/piano-pen-protocol.md) |
| Wi-Fi | 可用 | 新系统需自行保存网络连接 |
| 蓝牙 | 可用，启动时自动读取本机原厂地址，手工刷通用 ESP 也可初始化 | 原厂地址缺失时提供稳定硬件标识派生方案，见[蓝牙地址](devel/piano-bluetooth.md) |
| 扬声器 | 四路都能发声，沿用原厂功放增益 235 | 睡眠恢复未测 |
| 麦克风 | 录音、原生单声道 ALSA / PipeWire 输入，硬件增益 +40 dB | 睡眠恢复未完成 |
| 相机 | 前后路预览、录像，CAMSS/TFE 硬件 ISP | 曝光、对焦和首帧处理仍在改善；3A 为软件实现，与原厂画质有差距。见[相机与闪光灯](devel/piano-camera-flash.md)，手动接口见[相机控制](devel/piano-camera-controls.md) |
| 相机隐私灯 | 摄像头工作时实体绿色指示灯亮起，驱动随视频流控制 | 前后摄像头切换及并发关闭行为仍需完整检查 |
| 闪光灯 | 两路标准 LED 已注册，限值已读回 | 实际发光和曝光同步未观察 |
| 传感器 | SSC / FastRPC / iio-sensor-proxy 已装，多种数据可读 | 桌面联动和单位未全部完成，见[传感器](devel/piano-sensors.md) |
| 自动亮度 | GNOME 策略包沿用原厂 lux 阈值与延迟，传感器数据不做修改 | 依赖传感器联动完成度，见[自动亮度](devel/piano-auto-brightness.md) |
| 夜灯 / 色温 | GNOME 夜灯可用，自动生成内置屏幕的颜色配置 | 通用 sRGB 配置，不是原厂校准 |
| 电源 | 双电池、基础 PD 充电、UPower；合盖和短按电源锁屏熄屏；2 分钟空闲熄屏，电池 5 分钟、外接电源 10 分钟自动挂起；RTC 和电源键唤醒 | 待机耗电仍需优化，休眠未启用。见[电源键](devel/piano-power-button.md) |
| 指纹 | 原厂 QSEECom 通路已定位 | Linux 无后端 |
| 红外 / RGB 灯 | RGB 灯接口出现，红外 LIRC 候选已准备 | 红外发射未验证 |
| 默认桌面 | 中文界面、拼音输入法、Chromium 浏览器，GNOME 动画开启 | — |
| Windows / WinPE | 目标 | 未启动 |

### 启动时间

2026-10-12，菜单选择 Linux 后成功进入 GNOME，`systemd-analyze` 测得
1.917 秒内核 + 37.140 秒用户空间，共 39.057 秒，不含开机选择器和 UEFI 阶段。
首次启动、硬件状态和服务等待可能增加耗时。具体测量方法和历史对比见[启动时间](devel/piano-boot-time.md)。

### 其他发行版

`linux/bsp/`、`linux/desktops/` 和 `linux/rootfs/` 为 Debian、Ubuntu、Deepin、Arch 以及 GNOME、KDE 提供配置，但目前只有 Debian / GNOME 完整构建并在设备上运行过。

## 接下来的适配

以下是继续开发的目标，不代表当前镜像已经支持。

| 项目 | 要完成的内容 |
| --- | --- |
| 系统镜像 | 提供 Debian / GNOME、Debian / KDE、Nyarch / GNOME。Nyarch 基于 ARM64 滚动软件包和当前上游源码装配，不使用旧 ISO；保留默认中文、账号密码、动画和硬件自动配置 |
| FFmpeg | 将完整的、带设备补丁的 FFmpeg 及配套库纳入本地和 CI 构建，并更新平板上安装的版本 |
| 显示与手写笔 | 保留默认 120 Hz，完善 144 Hz 下笔输入及跨系统切换后的笔唤醒。通过真实 DRM/KMS 能力接入 HDR 和色彩控制，对照原厂确认 10-bit + FRC 的实现 |
| 电源 | 对照 Android 优化待机耗电、性能和唤醒体验；现阶段优先挂起，休眠暂不作为移动平台目标 |
| 相机 | 对照原厂 ISP、曝光和对焦流程，改善首帧和专业控制；减少帧复制，目标为后置 4K60、前置 1080p30，并接通手电筒及随摄像头工作启停的实体隐私灯 |
| 麦克风 | 保持标准单声道输入及硬件增益配置，核对睡眠后音频恢复 |

120 Hz 的默认选择由内核面板驱动补丁
[`0022-panel-piano-prefer-120hz-cold-start.patch`](../patches/linux/7.2.9/0022-panel-piano-prefer-120hz-cold-start.patch)
提供，并在 `config/release.json` 中记录；不是登录桌面后才切换刷新率。
BOE／CSOT 仍按设备标识选择，不固定面板供应商，也不移除其他显示模式。
当前合盖可关闭背光、开盖可恢复；完整睡眠与休眠目标仍被禁用，需要完成恢复验证后再启用。

## 文档记录约定

状态更新写明设备、软件版本和实际观察。驱动加载、编译通过、USB 命令成功和功能可用是不同的结论，分别记录。新构建不继承旧构建的设备结果。一次性实验记录保留在 Git 历史中。
