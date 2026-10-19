# Piano ADSP — HyperOS 4

从运行 HyperOS `OS4.0.4.0.XPYCNXM` 的 Piano 提取，vendor 标识为
`OS4.0.2.1.XPYCN`，来源为 `/vendor/firmware_mnt/image`。

包含完整 ADSP、ADSP-DTB 与三个服务描述文件，保持原始字节和文件名。
`manifest.json` 记录每个文件的大小及 SHA256。构建时成套覆盖上游旧版
ADSP，不改其他设备固件，也不写入 Android 固件分区。

配套 MCA PD 协议为版本 2：HyperOS 4 在 `0x2100b` 插入了属性，
后续 PD 档位查询、PPS 选择和固定电压请求的编号相应增加 1。
`piano-board-identity.dtso` 声明所用协议版本，内核 `piano_mca`
统一转换请求编号；不要将本组固件与只支持旧协议的充电驱动混用。

设备上已确认新版驱动可以读到正确的 PDO（充电器提供的电压／电流档位），
同一条 5 V 连接的输入电流从约 0.5 A 恢复到 1.49 A；原厂同线约为
1.59 A。这是普通 PD 充电修复，不代表已测得小米高压快充。

内核 MiPPS 认证候选位于
[`0040-power-piano-native-mipps.patch`](../../../patches/linux/7.2.9/drafts/0040-power-piano-native-mipps.patch)。
它在现有充电工作队列内完成 HMAC-SHA256 挑战应答，使用标准
`power_supply` 的 `authentic` 属性报告结果，不另装认证服务，也不改动
充电电流和温控上限。候选已在设备上加载并完成普通 PD 启动；小米充电器
认证尚未实测，未加入默认发布补丁序列。当前要求 ADSP 提供有效的小米
SVID 和充电器 ID；固件漏报身份时的处理还需继续对照原厂。

这些文件是 Qualcomm／小米的专有固件，不适用 SunUEFI 的源码许可。
未取得额外再分发授权；保留原权利人的权利，仅用于该设备的硬件运行。
