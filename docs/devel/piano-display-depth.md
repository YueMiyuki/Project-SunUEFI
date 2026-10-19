# 显示位深与 HDR

## 当前实现

Piano 的默认内核使用 RGB101010 和 DSC 1.1：输入是 **10 bpc**，压缩传输为 **8 bpp**，slice 为 `800×24`。压缩位率不等于面板位深。BOE、CSOT 两份原厂最终设备树采用相同的十位传输配置，驱动按实际面板选择初始化序列。

标准 DRM/KMS 接口包括：

| 接口 | 实际能力 |
| --- | --- |
| `max bpc` | 固定 10..10，与当前面板和 DSC 配置一致 |
| `GAMMA_LUT` | 1024 项，GCv2 硬件输出曲线；GNOME 夜灯使用此接口 |
| `DEGAMMA_LUT` | 257 项、每色 16 位，IGCv5 输入曲线 |
| `CTM` | 现有 PCC 色彩矩阵 |

GC 和 IGC 在同一 REGDMA 缓冲区提交到实际使用的两路 DSPP，完成后一起生效。设备已完成恒等、非线性、恢复旁路的标准 LUT 更新，传输完成状态正常。实现及资源管理见 [GCv2／IGCv5 后端](piano-gcv2-night-light.md)。

GNOME 51 的显示设置有 HDR 开关，但没有单独的 8／10／12 位色深选择器。HDR 设置依赖合成器确认 BT.2100 输出能力；当前只有十位传输和基础色彩处理，**HDR 仍未完成**。

## 原厂的色彩处理

固定的 [MiCode 显示驱动](https://github.com/MiCode/vendor_opensource_display-drivers/tree/aa06fd1757c28dce96fbe4e04d4530ec21b52aac)及同机 Android DRM 状态表明：

- 原厂 SDR／P3 模式使用 IGCv5、GCv2 和 Gamut v4。IGC 开启高精度扩展与内部 dither，GC 开启高精度扩展。
- IGC 的高精度格式是 257 个普通点加 128 个暗部扩展点，不能按均匀 385 点解释。原厂 QDCM 的 sRGB 表与普通步长 `1/256`、暗部步长 `1/1024` 精确对应；独立的 Gamma 2.2 表误差不超过一个十六位码值。默认后端仍使用普通 257 点格式，高精度候选另行验证。
- 原厂 DSI 的 `hdr_properties` 描述静态原色和亮度能力；它不等于 HDMI／DP 的动态 `HDR_OUTPUT_METADATA`，也不证明 DDIC 直接接收 PQ 像素。
- 尚需取得真实 HDR 图层播放时的曲线、色域和亮度处理，对照 SDR 确认原厂在哪一层进行转换和色调映射。

参考源码：[`reg_dmav2_setup_dspp_igcv5`](https://github.com/MiCode/vendor_opensource_display-drivers/blob/aa06fd1757c28dce96fbe4e04d4530ec21b52aac/msm/sde/sde_hw_reg_dma_v1_color_proc.c#L5366)、[`dsi_panel_parse_hdr_config`](https://github.com/MiCode/vendor_opensource_display-drivers/blob/aa06fd1757c28dce96fbe4e04d4530ec21b52aac/msm/dsi/dsi_panel.c#L4061)。

HyperOS 4 的实际配置位于 `/odm/etc/display/qdcm_calib_data_xiaomi_p81_35_02_0b_vid_mode_dualdsi_dsc_panel.json` 和对应的 `p81_42_02_0a` 文件。JSON 中的十六进制 payload 还需还原字节对顺序，才能读取内部曲线和 XML：

- `native_hdr` 的输出 Gamma 为 2.2，包含 P3 面板校准、17 点色调映射、实时亮度补偿和内容统计；CSOT 配置中的满屏峰值约 844.5 nit，SDR 参考峰值为 400 nit。这些是配置值，不是本次物理亮度测量。
- HDR 模式配置了 LUMA dither，但 `temporalEnable=0`；配置内容不能代替实际播放时的寄存器状态，也不能单凭该字段认定面板 FRC。
- [`0045` 高精度候选](../../patches/linux/7.2.9/drafts/0045-drm-igcv5-high-precision-grid.patch)暴露均匀 1025 项 `DEGAMMA_LUT`，由驱动选取两组硬件采样点，仍不启用 dither。组合 DMA 扩大到 16 KiB；实机恒等、非线性和旁路恢复已完成，提交 2142 个 word，两路 DSPP 均返回完成状态。尚未加入默认序列，也不代表 HDR 或面板 FRC 已验证。

## 12-bit 与抖动

[小米官方规格](https://www.mi.com/global/product/xiaomi-pad-8-pro/)宣称支持 12-bit 色深。当前可确认的原厂链路是十位传输；额外两位是否来自面板内部 FRC、显示处理或组合机制，仍需确认具体实现。

不能直接打开高通普通 pingpong dither 来代替。原厂 [`_sde_encoder_setup_dither`](https://github.com/MiCode/vendor_opensource_display-drivers/blob/aa06fd1757c28dce96fbe4e04d4530ec21b52aac/msm/sde/sde_encoder.c#L4002)在 DSC10／30bpp 配置下关闭该模块，其位深表只覆盖 6／7／8 位输出。IGC 的 16 位 LUT、内部 dither 和面板 12-bit FRC 也不是同一个概念。

## HDR 接入要求

后续实现需要完整处理内容的传递函数、色域、面板亮度和色调映射，再通过标准 DRM/KMS 能力交给桌面使用。不能只添加 HDR 属性或虚构 EDID 来显示开关。Mutter 当前还从 EDID 联合检查 BT.2020、PQ 和静态 HDR metadata 能力；无 EDID 的内屏需要真实的能力描述与对应输出处理。

[`tools/android/piano_drm_snapshot.c`](../../tools/android/piano_drm_snapshot.c)可在 Android 和 Linux 中只读导出 DRM 属性及 LUT blob。将同一画面设置下的 SDR 与实际 HDR 播放状态对照，同时确认活动 CRTC、HDR 图层和电源状态；blob 存在或 DMA 完成不能单独证明物理 HDR 输出。
