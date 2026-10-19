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

* [构建手册](devel/building.md)、[公开构建链](devel/public-build.md)、[测试](devel/testing.md)、[发布要求](devel/release.md)
* [仓库地图](devel/repository-map.md)、[本地输入](devel/local-inputs.md)、[内核角色](devel/kernel-roles.md)、[补丁登记](devel/patches.md)
* 启动状态与切换：[启动请求](devel/reboot-request.md)、[Linux 下次启动](devel/linux-next-boot.md)、[Recovery 入口](devel/recovery-entry.md)、[原生 BOOT 重打包](devel/android-boot-repack.md)、[Android 模块](devel/android-module.md)
* 多发行版：[BSP 配置包](../linux/bsp/README.md)、[发行版装配](../linux/rootfs/README.md)、[桌面配置](../linux/desktops/README.md)、[Linux 服务](devel/linux-services.md)
* [贡献指南](../CONTRIBUTING.md)、[第三方来源](../THIRD_PARTY.md)、[许可](../LICENSE.md)

## 硬件专题

| 主题 | 页面 |
| --- | --- |
| 显示 | [色深与面板](devel/piano-display-depth.md)、[面板选择](devel/panel-selection.md)、[夜灯后端](devel/piano-gcv2-night-light.md)、[夜灯验证](devel/piano-gcv2-validation.md)、[自动亮度](devel/piano-auto-brightness.md) |
| 音频 | [音频时钟](devel/piano-audio-clock.md) |
| 相机 | [相机与闪光灯](devel/piano-camera-flash.md)、[视频编解码](devel/piano-video-codec.md) |
| 无线 | [蓝牙](devel/piano-bluetooth.md) |
| 输入与电源 | [触控笔协议](devel/piano-pen-protocol.md)、[电源键](devel/piano-power-button.md)、[传感器](devel/piano-sensors.md)、[SSC 数据](devel/piano-ssc-vector.md)、[LED / 红外 / 指纹](devel/piano-led-ir-fingerprint.md) |
| 启动 | [启动时间](devel/piano-boot-time.md) |

## UEFI 和 Linux 交接的设计记录

下面的页面写于开发过程中，描述各自时间点的设计与测试，保留供查阅，不代表当前状态。

| 主题 | 页面 |
| --- | --- |
| 产品构建与核心 | [产品构建](piano-product-build.md)、[启动策略](piano-product-boot-policy.md)、[共同 pump](piano-product-pump.md) |
| 生命周期 | [产品 owners](piano-product-owners.md)、[OS 控制](piano-product-os-controller.md)、[退出](piano-product-os-exit-contract.md) |
| 内存与 DMA / SMMU | [内存](piano-platform-memory-contract.md)、[DMA 基础](dma-smmu-milestone.md) |
| 显示 | [GOP](piano-gop-framebuffer-publishing.md)、[映射](piano-display-mapping-contract.md) |
| USB | [驻留服务](piano-usb-persistent-service.md)、[日志快照](piano-usb-log-snapshot.md) |
| UFS | [BlockIO / USB](blockio-usb-progress.md)、[退出](ufs-ebs-lifecycle.md) |
| Linux | [构建历史](linux-reproducible-builds.md)、[完整 Linux 基线](piano-full-linux-candidate.md)、[EFI 会话](piano-linux-efi-session.md) |

当前功能以[项目状态](status.md)为准。已删除的一次性实验记录可从 Git 历史查阅。
