# 开发记录

这里保存开发过程中的设计和测试记录。每一页写于它自己的时间点，只反映当时的判断，不代表当前实现。当前功能以[项目状态](../status.md)为准，当前实现的说明在 [`docs/devel/`](../devel/building.md)。

| 目录 | 内容 |
| --- | --- |
| `display-clock/` | UEFI 继承原厂显示状态时，对显示时钟、电源轨、SMMU 和帧缓冲映射的读取与占用设计 |
| `fastboot-ram-boot/` | 通过 Fastboot 把镜像传进内存直接启动的早期方案、命令核对和内存核对 |
| `linux-bringup/` | Linux 早期启动适配：EFI 内存、设备树、内核与用户空间构建、调试驱动、基线调研 |
| `memory-dma/` | DDR、CMA 和 SMEM 的内存布局，SMMU，DSP 与 PCIe 的 DMA，Linux 自管 DMA 的设计与实验 |
| `pogo-keyboard/` | 官方键盘（磁吸触点接口）在 UEFI 里的 I²C、GENI 运行时设计和来源核对 |
| `storage-usb/` | UFS、BlockIO、USB 的设计、链路核对和阶段进度 |
| `uefi-core/` | 产品核心的启动策略、退出流程、显示、存储、启动画面和启动日志的设计 |
