# 文档导航

[项目首页](../README.md)介绍项目和使用方法，[项目状态](status.md)列出各功能的当前情况。

## 使用

* [快速上手](user/getting-started.md)
* [安装指南](user/install-from-artifact.md)：用引导助手分区、安装镜像并选择系统
* 备用方式：[电脑安装](user/install-from-computer.md)、[临时启动固件](user/temporary-boot.md)
* [返回 Android 与故障恢复](user/recovery.md)
* [UEFI Fastboot](user/fastboot.md)
* [已知问题](user/known-issues.md)
* [HMCL 图形加速](user/hmcl.md)
* [官方键盘与 F1–F12](user/keyboard.md)

## 开发

* [构建手册](devel/building.md)、[测试](devel/testing.md)、[发布要求](devel/release.md)
* [仓库地图](devel/repository-map.md)、[本地输入](devel/local-inputs.md)、[内核角色](devel/linux/kernel.md)、[补丁登记](devel/patches.md)
* 启动状态与切换：[启动请求](devel/boot/reboot-request.md)、[Linux 下次启动](devel/boot/linux-next-boot.md)、[Recovery 入口](devel/boot/recovery-entry.md)、[原生 BOOT 重打包](devel/boot/combined-boot.md)、[Android 模块](devel/boot/android-module.md)
* 多发行版：[BSP 配置包](../linux/bsp/README.md)、[发行版装配](../linux/rootfs/README.md)、[桌面配置](../linux/desktops/README.md)、[Linux 服务](devel/linux/services.md)、[Proton/FEX 协作挂起](devel/linux/steamos-proton-fex-suspend.md)
* [贡献指南](../CONTRIBUTING.md)、[第三方来源](../THIRD_PARTY.md)、[许可](../LICENSE.md)

## 硬件专题

| 主题 | 页面 |
| --- | --- |
| 显示 | [色深与面板](devel/hardware/display-depth.md)、[面板选择](devel/hardware/panel-selection.md)、[夜灯后端](devel/hardware/gcv2-night-light.md)、[夜灯验证](devel/hardware/gcv2-validation.md)、[自动亮度](devel/hardware/auto-brightness.md) |
| 音频 | [音频时钟](devel/hardware/audio-clock.md) |
| 相机 | [相机与闪光灯](devel/hardware/camera-flash.md)、[视频编解码](devel/hardware/video-codec.md) |
| 无线 | [蓝牙](devel/hardware/bluetooth.md) |
| 输入与电源 | [触控笔协议](devel/hardware/pen-protocol.md)、[电源键](devel/hardware/power-button.md)、[传感器](devel/hardware/sensors.md)、[SSC 数据](devel/hardware/ssc-vector.md)、[LED / 红外 / 指纹](devel/hardware/led-ir-fingerprint.md) |
| 启动 | [启动时间](devel/boot/boot-time.md) |

## UEFI 和 Linux 交接的设计记录

下面的页面写于开发过程中，描述各自时间点的设计与测试，保留供查阅，不代表当前状态。

| 主题 | 页面 |
| --- | --- |
| 产品构建与核心 | [产品构建](archive/uefi-core/piano-product-build.md)、[启动策略](archive/uefi-core/piano-product-boot-policy.md)、[共同 pump](archive/uefi-core/piano-product-pump.md) |
| 生命周期 | [产品 owners](archive/uefi-core/piano-product-owners.md)、[OS 控制](archive/uefi-core/piano-product-os-controller.md)、[退出](archive/uefi-core/piano-product-os-exit-contract.md) |
| 内存与 DMA / SMMU | [内存](archive/memory-dma/piano-platform-memory-contract.md)、[DMA 基础](archive/memory-dma/dma-smmu-milestone.md) |
| 显示 | [GOP](archive/uefi-core/piano-gop-framebuffer-publishing.md)、[映射](archive/display-clock/piano-display-mapping-contract.md) |
| USB | [驻留服务](archive/storage-usb/piano-usb-persistent-service.md)、[日志快照](archive/storage-usb/piano-usb-log-snapshot.md) |
| UFS | [BlockIO / USB](archive/storage-usb/blockio-usb-progress.md)、[退出](archive/storage-usb/ufs-ebs-lifecycle.md) |
| Linux | [构建历史](archive/linux-bringup/linux-reproducible-builds.md)、[完整 Linux 基线](archive/linux-bringup/piano-full-linux-candidate.md)、[EFI 会话](archive/linux-bringup/piano-linux-efi-session.md) |

当前功能以[项目状态](status.md)为准。已删除的一次性实验记录可从 Git 历史查阅。
