# 本机原厂分区备份

`persist-20261025.img` 是本项目开发平板的完整 32 MiB `persist` 分区备份，
在 SunUEFI 向该分区写入项目文件前保存。备份时冻结 ext4 写入，复制并
比较分区、设备端副本与电脑端镜像的 SHA-256，随后恢复正常挂载。

大小和摘要见 [`manifest.json`](manifest.json)。此目录不作为产品输入，
不装入 UEFI、Android 模块或发行版安装包。镜像用于原设备恢复，不作为
其他平板的通用 `persist` 镜像。
