# 启动架构与系统交接

本页说明从按下电源键到进入 Linux 桌面的整个启动链条，涵盖合体 BOOT、EDK2 UEFI 固件、存储分区与系统交接。

## 开机启动链

```
按下电源
  │
  ▼
原厂引导程序 (XBL / ABL)
  │  按 BOOT 分区的 kernel_size 整段加载内核与 SunUEFI
  ▼
合体 BOOT 选择器 (3 秒倒计时)
  ├── 选 Android ──► 跳到原厂内核入口 ──► HyperOS
  └── 选 UEFI ─────► 进入 EDK2 UEFI 核心 (PianoUEFI-product.img)
                       │
                       ├─► 常驻 USB Fastboot 服务 (SunUEFI-piano)
                       ├─► 显示 SimpleInit 图形菜单
                       │
                       ▼ 启动 Linux
                     读取 sunuefi_esp 中的 /EFI/Piano/stable/boot.img
                       │
                       ▼ ExitBootServices 安全退出
                     释放 USB 控制器、UFS、时钟与 SMMU
                       │
                       ▼
                     Linux 内核 (Image) + 设备树 (board.dtb) + initramfs
                       │
                       ▼
                     挂载根文件系统 (sunuefi_root, ext4)
                       │
                       ▼
                     进入 Linux 桌面 (GNOME / KDE / SteamOS)
```

## 核心机制

### 合体 BOOT

原厂 ABL 在 Recovery 自带内核时不会加载 `pvmfw`，直接替换内核会导致 Recovery 无法启动。SunUEFI 把原厂 Android 内核留在原位，只把入口跳转改向一个小选择器，选择器和 UEFI 核心接在内核之后由 ABL 一同载入内存。选择 Android 时跳回原厂内核，选择 UEFI 时进入 EDK2。原理见[合体 BOOT 详细说明](boot/combined-boot.md)。

### 启动路线记忆

在 Android 引导助手中选择的系统会被保存为默认启动路线，开机时临时用音量键改选不会改变默认值。Android 槽位（A/B）直接读取高通 GPT 的活动槽位属性。在 Linux 下运行 `piano-next-boot android --reboot` 即可切回 Android。

### 两个 Fastboot

SunUEFI 提供与原厂 Bootloader 不同的 Fastboot 服务：

| 服务 | 设备序列号 | 阶段 | 能力 |
| --- | --- | --- | --- |
| 原厂 Bootloader Fastboot | 平板真实序列号 | ABL 阶段 | 允许刷写与擦除分区 |
| UEFI Fastboot | `SunUEFI-piano` | UEFI 运行期间 | 读日志、截图、只读读取分区；拒绝刷写和擦除 |

详细命令见 [UEFI Fastboot](../user/fastboot.md)。

## 存储布局

内部存储划分两个专用分区，不破坏 Android 原有分区：

* `sunuefi_esp`（512 MiB，FAT32，4 KiB 逻辑扇区）：存放 Linux 启动文件；
* `sunuefi_root`（可自选大小，ext4）：存放 Linux 根文件系统。

```
sunuefi_esp/
└── EFI/
    └── Piano/
        └── stable/
            ├── Image          # Linux 内核
            ├── board.dtb      # 经清理与合成的设备树
            ├── initramfs      # 启动阶段内存盘
            └── boot.img       # 组合启动镜像
```

系统交接前，UEFI 读取 `boot.img`，校验内存范围并安全退出 ExitBootServices，最后将控制权移交 Linux 内核。
