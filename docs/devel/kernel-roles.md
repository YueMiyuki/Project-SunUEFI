# 内核来源与配置

Debian GNOME、Debian KDE、Nyarch GNOME 和 SteamOS 共用同一套设备内核。
默认构建使用正式版 Linux 7.3，版本名为 `7.3.0-piano-sunuefi-cachy`。
桌面名称属于 root 镜像，内核名称不带 `gnome` 或构建目录的 Git 后缀。

## 源码与硬件适配

Linux 基线为上游 `v7.3`，提交
`c03c1a14507dfaf8b856c2608f9ac216021fc222`。Piano 的公开硬件适配来自
`blu-sharky/linux-piano`，在新基线上继续保留本项目的显示、音频、输入、
充电、睡眠和编解码修正。

补丁按平台、显示、输入与无线、媒体、音频、Gunyah、BORE 分组，
后续接口修正单独追加。原始来源、补丁摘要、应用顺序与每一步的源码树
记录在 [`series.json`](../../patches/linux/7.3/series.json)；
旧版本补丁保留作来源参照，不会再叠加到新版本上。

`./build.sh linux` 从固定上游提交准备源码，在 `build/kernel-worktrees/`
中应用补丁，使用独立编译副本生成 `artifacts/kernels/release/`。
`upstream/` 中的工作区不被改写。本地与 CI 使用同一入口和配置。

7.3 改动了 DRM 原子提交、显存分配、内存反向映射等内部接口，因此升级
包含实际的驱动移植。保留旧功能代码并不保证运行行为完全相同，升级后
仍需检查实机显示、输入、无线、音频、相机和电源。更新时必须同时安装
新内核的模块、`v4l2loopback` 相机模块和 initramfs，不能只替换 `Image`。

## CachyOS 与通用配置

* 引入 CachyOS 为 Linux 7.3 整理的 **BORE 6.8.0**，保留其默认参数。
  它在公平调度器基础上根据任务的运行特征调整调度，目标是改善负载下的
  交互响应；具体帧率、功耗和温度收益需要测量，不能仅凭启用选项判断。
* 启用 **MGLRU**，让内核按多代页面访问情况回收内存。既有 Zstd ZRAM
  继续保留，这两个功能属于 Linux 本身。
* 使用 **300 Hz** 定时配置、动态抢占并默认完整抢占，保留空闲时停用
  周期时钟的 `NO_HZ_IDLE` 和 `schedutil` 动态调频。
* 不另设 CPU 频率上限，不引入 x86 专用优化或已经不适用于当前 BORE 的
  sysctl 参数。300 Hz 也不等于屏幕刷新率或输入采样率。

具体配置位于 [`piano-platform.config`](../../linux/configs/piano-platform.config)，
来源及许可见 [THIRD_PARTY.md](../../THIRD_PARTY.md)。

## Gunyah

原厂启动链已由 Gunyah 占用 EL2。这里接入其资源管理器和虚拟机接口，
不会替换原厂 Hypervisor，也不等于直接提供 KVM。
驱动编进内核，以保留借出内存时取消内核直接映射的处理。

原厂设备树中的资源句柄只对本次启动有效。UEFI 在早期保留这些数据，
在 raw 和 EFI 两条 Linux 交接路径中更新目标设备树。没有取得有效数据
时只禁用 Gunyah 资源管理器，Linux 仍可正常启动。
源码位于 `uefi/handoff/early-memory/PianoColdBootObjects.c` 和
`uefi/core/PianoProductBootObjects.c`。

GNOME 的“关于”页面对高通主系统域省略通用的 `vm-other` 行，
底层 systemd 检测与 Gunyah 接口保留。桌面补丁与打包状态见
[系统信息适配](../../linux/desktops/gnome/patches/control-center/README.md)。
Gunyah 虚拟机运行仍需实机验证，不能以设备节点出现代替。


## Piano 启动配置

打包时由 `tools/package_release.py` 将折叠后的原厂 DT 转成主线使用的最终设备树。
旧 SDE／CPAS／VIDC／SPF 节点由主线显示、CAMSS、Iris 和音频节点接管；旧 ICC
参数和被误识别为 GPIO 的厂商标志不再作为标准绑定传给内核。ADSP 的固件、
内存和电源资源保留，只删除主线 PAS 驱动不使用的旧 ICC 属性。

显示和相机的旧 GPIO 稳压器、CCI、调试 UART，以及没有消费者的旧触控供电
和 USB 空 PHY 节点也不参与探测。实际显示、相机和蓝牙使用各自的主线节点，
不再等待 Android 的 `pinctrl@f000000`。主线替代节点必须存在且启用，才停用
对应的旧显示／相机节点。Demura 的零地址、零长度占位项禁用，有效的内存
预留保持不变。

原厂 `sdam@7000/sm1510_present@5d` 在一字节内声明了偏移 5、长度 5 的
无效位范围。主线没有该厂商标志的消费者，因此保留完整原始字节，去掉无效的
位切片，不猜测其正确位宽。这样 SDAM 提供者及同区的 OCP 记录可以正常注册。

原厂在本板同样无法探测的四个可选 PMIC，以及不属于 AP 的两个硬件投票 RSC
节点，在最终树中禁用；AP 的 `rsc@16500000` 和实际外设供电保持启用。
`piano-platform.config` 排除 NXP 的 SCMI 扩展，保留 Qualcomm 使用的标准协议。
USB PHY 没有连接显示拓扑时不创建空的 DRM 桥，USB PHY 本身照常工作。

这套配置已从正常启动的内核日志核对：上述引用、SPMI、SCMI、DRM 桥及 RSC
报错消失，ADSP、音频、键盘、蓝牙和 USB 调试连接保持工作。

磁盘启动统一使用 `linux/userspace/disk-bootstrap`，支持发布镜像的 LABEL 和
开发镜像的 PARTUUID。ext4 先正常回放日志，再转为读写；救援环境包含磁吸键盘
所需的 GENI I²C、GPI、Nanosic 驱动和 MCU RAM 固件。

相机和视频初始化脚本在内存映射已经为 `DMA` 或 `DMA-FQ` 时直接沿用，
可以正常重启服务。驱动绑定后重复写 IOMMU group 的类型会被内核拒绝，
不能把这类服务重启失败误判为相机硬件故障，更不能解绑正在使用的映射。
