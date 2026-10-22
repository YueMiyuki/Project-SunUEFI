# Piano Goodix — HyperOS 4

`gfpia.elf` 是从本机原厂固件目录的 `gfpia.mdt` 和 `gfpia.b00..b08`
按 ELF 文件偏移还原的完整签名程序，保持各段原始字节。
重叠段已核对一致，未修改代码或签名。

它由高通 QTEE 加载，用于 Goodix 指纹传感器通信。
文件不含用户录入的指纹模板、解锁密码或运行时认证数据。
来源、大小和 SHA256 记录在 `manifest.json`。

这是 Qualcomm／小米的专有固件，不适用本项目的源码许可。
保留原权利人的权利，仅用于该设备的硬件运行。
