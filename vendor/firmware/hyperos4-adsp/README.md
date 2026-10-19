# Piano ADSP — HyperOS 4

从运行 HyperOS `OS4.0.4.0.XPYCNXM` 的 Piano 提取，vendor 标识为
`OS4.0.2.1.XPYCN`，来源为 `/vendor/firmware_mnt/image`。

包含完整 ADSP、ADSP-DTB 与三个服务描述文件，保持原始字节和文件名。
`manifest.json` 记录每个文件的大小及 SHA256。构建时成套覆盖上游旧版
ADSP，不改其他设备固件，也不写入 Android 固件分区。

这些文件是 Qualcomm／小米的专有固件，不适用 SunUEFI 的源码许可。
未取得额外再分发授权；保留原权利人的权利，仅用于该设备的硬件运行。
