# UEFI 设置页面

SimpleInit 提供触摸尺寸的图形菜单；TianoCore 设置页面使用 UEFI 文本控制台。
在 3200×2136 屏幕上，默认 8×19 字形的 80×25 页面只占很小一块区域。

产品构建只为 GraphicsConsole 的字符绘制增加四倍缩放。该驱动内部使用
800×534 的逻辑画布，把文本块放大到物理屏幕；公开 GOP 的模式和帧缓冲仍是
3200×2136，SimpleInit、截图和操作系统使用原来的接口，不切换物理显示模式。
80×25 字符区放大后约为 2560×1900。

构建工作副本中的五组 HII 字符串加入 `zh-CN`，常用设置页面默认中文，
技术设备名和未翻译的文字仍用英文。中文来自 Noto Sans CJK SC 的 16×19
宽字符位图，通过标准 HII 字体包注册；构建记录源字体和生成文件摘要。
没有修改固定的上游检出目录。

实体按键通过标准 `SimpleTextIn` 协议接入。按键设备同时安装
`ConsoleInDevice` 标记，让标准控制台汇聚驱动能够找到它。进入设置页时，
音量键映射上下移动，短按电源映射回车，同时按音量两键映射返回。

源码位置：

* `uefi/components/product-support/Library/ConsoleScale/`：缩放和中文翻译。
* `tools/prepare_product_console.py`、`tools/prepare_product_ui.py`：在构建副本生成字形并接入 GraphicsConsole。
* `tools/prepare_gui_profile.py`：默认语言。
* `uefi/core/PianoKeys.c`：标准按键设备与映射。

2026-10-12，正式产品经合体 BOOT 进入设置页面，实机确认四倍字号、
常用菜单中文、音量键移动、电源键确认与双音量键返回均可使用。
此前使用同一缩放代码与生成字形导出 3200×2136 预览，检查了宽字符与英文混排。
2026-10-25，正常开机进入 UEFI 后 USB 可连接，并完成菜单、设置和 Shell 之间的切换；Fastboot 全程保留。
触屏、磁吸键盘及触控板的 UEFI 驱动尚未完成，Linux 下可用不等于 UEFI 下可用。

## 启动项与横屏

SimpleInit 默认旋转改为 0°，使用 GOP 的 3200×2136 横屏画布。界面旋转由
SimpleInit/LVGL 处理，物理显示模式保持原样；实机已确认横屏显示。

标准 UEFI 启动管理器读取 `Boot####` 和 `BootOrder`。ESP 中的 `boot.img`
是 Android 格式的 Linux 启动包，不能直接当作 EFI 应用交给标准启动管理器。
产品现在先查找真实 ESP 和启动文件，再登记一个指向固件内
`PianoLinuxBoot.efi` 的 Piano Linux 启动项。该应用向驻留核心请求启动，
随后复用同一套文件加载、安全退出和内核交接流程。

入口在每次启动时发现并登记，变量仍由内存后端保存；没有注册虚构设备，
也没有扩大为对任意系统镜像的支持。这条标准入口已构建，设备上的启动结果
仍需验证。

USB 初始化与 Linux 加载结果会保存在固件内存中，并在菜单运行时每五秒输出摘要，
避免早期日志被 UFS 文件读取记录覆盖。摘要只重发已保存的状态，不重新访问 USB
寄存器。读取失败时可区分 ESP 文件读取、镜像解析和后续交接阶段。

## 中文默认语言与 FAT 挂载

中文设置页加入后，Linux 菜单启动返回且标准启动列表为空。实机摘要显示
`LocateHandleBuffer(SimpleFileSystem)=Not Found`，没有任何已挂载的 FAT 卷；
Android 下 ESP 与 `EFI/Piano/stable/boot.img` 均存在。

固定 Mu_Basecore 的 `FatPkg/EnhancedFatDxe/UnicodeCollation.c` 在启动驱动前
使用 `PlatformLang` 与模块的默认语言匹配 UnicodeCollation2。
固件提供的 EnglishDxe 只声明 `en`，全局默认 `zh-CN` 时两种语言均无法匹配，
FAT 驱动返回 Unsupported，不发布 SimpleFileSystem。

产品 DSC 只为 `Fat.inf` 设置 `PcdUefiVariableDefaultPlatformLang="en-US"`，
提供文件名比较服务的回退语言；中文界面与全局 `PlatformLang` 保持不变。
该修正已完成正式产品编译，并从菜单成功启动 Linux，GNOME 正常运行。
标准 BIOS 启动列表的实际显示仍需单独确认。

## USB 复位与供电

此前正常 BOOT 进入 UEFI 时，ABL 已关闭 PHY，DWC31 2.00a 的 `DCTL.CSFTRST` 持续置位，200 ms 后超时；只开控制器时钟或清除 SUSPHY／ENBLSLPM 不能恢复。

`PianoUsbPhy.c` 按同机 `mu_wide_platforms` 的 UsbConfigDxe 和原厂 XBL 表初始化 M31／QMP PHY，并通过原厂 RPMh 协议申请 `ldod2`、`ldog3`、`ldof1`、`ldob5`、`ldob15` 供电。中继器原先虽有使能位，但 `FD08=00`，缺少实际供电；补齐后正常 BOOT 可枚举，`CSFTRST` 在 20 ms 内完成。

PHY 是 8 位寄存器，32 位读取会复制字节，例如写入 `1` 返回 `0x01010101`。退出时按有效位校验，不能把这种正常读回误报为失败。设备核心先停止 DMA、释放缓冲并撤销 SMMU，再停 PHY、恢复 RPMh 原投票和归还时钟；任何一步无法确认完成仍拒绝交接。

本轮已从合体 BOOT 读取身份、日志及 3200×2136 截图，切换设置、Shell 后连接保留；Fastboot 重启后重新枚举，并成功启动原有 Linux。Linux 保留日志记录 PHY 停止、15 项供电恢复、UFS／USB 资源退出全部成功。

正常文件读取不再逐块写入默认 RAM 日志：`BLOCKIO_READ` 的成功提交、完成和
协议内容改为 `DEBUG_VERBOSE`，需要逐次 DMA 记录时可在调试构建中开启。
分配、映射、退出和错误记录继续保留，失败请求与响应仍会输出。
这样可以避免读取 Linux 文件时覆盖前面的 USB 初始化现场。
