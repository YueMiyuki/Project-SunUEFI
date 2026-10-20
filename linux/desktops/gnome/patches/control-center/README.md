# GNOME 系统信息

`0001-about-gunyah-primary-domain.patch` 区分高通的主系统域和普通客户虚拟机。
只有设备树同时声明 Gunyah 和 Qualcomm HLOS 的 VMID 3 时，才隐藏
GNOME “关于”页面的通用 `vm-other` 行。未知域和其他虚拟化类型保留原有展示。

这是桌面展示调整。内核设备树、systemd 的 `Virtualization` 属性、
`systemd-detect-virt` 和 Gunyah 驱动接口都不改变。不能删掉固件节点来影响识别，
这些节点也是虚拟化驱动发现资源管理器的依据。

Nyarch 使用旁边 `arch/control-center` 的完整 GNOME 51.1 包配方，
由 `tools/build_nyarch_rootfs.py` 放入构建。源码来自 GNOME 正式发布归档，
保留发行版原有功能、翻译和拆分的软件包。Debian 的 GNOME 48 包尚待接入。
