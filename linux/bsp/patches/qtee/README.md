# QTEE 文件协议适配

补丁针对 Qualcomm MinkIPC 的固定提交
`ce145804d9e994e0a7913f8d224d52e84bce6608`。
Piano HyperOS 4 的安全程序仍使用 GPFS 的旧绝对路径命令：
0 读取、1 写入、2 删除、3 重命名；公开实现只处理 4..11 的数据区／持久区命令。

`0001-gpfs-legacy-protocol.patch` 沿用公开实现的路径解析和文件操作，
补齐这四个命令，并在读取到文件末尾时结束循环。命令编号与结构偏移
对照同机 `/vendor/lib64/libdrmfs.so`；Linux 自有目录中的读取、写入和删除
已通过真实 QTEE 回调完成。

这份补丁保留给 QTEE 服务集成，当前镜像没有默认安装该后台服务。
原生指纹客户端的初始化、模板枚举及 Keymaster 认证数据绑定不依赖它。
此前的 `-10020` 由本次启动尚未完成 HMAC sharing 引起，现已接通原厂
初始化流程。完整录入和认证继续适配，不能用文件服务注册成功代替。

固件和客户端说明见
[指纹适配](../../../../docs/devel/hardware/led-ir-fingerprint.md)。
