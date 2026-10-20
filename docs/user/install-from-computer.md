# 备用：从电脑刷写镜像

日常安装推荐使用平板上的[引导助手](install-from-artifact.md)。本页用于从电脑更新**已有的** `sunuefi_esp` 和 `sunuefi_root` 分区。

从同一次 [Build products](https://github.com/slwyts/Project-SunUEFI/actions/workflows/build-products.yml) 下载 `SunUEFI-ESP` 和所需的 `SunUEFI-Root-发行版`，解压得到 `esp.sparse.img` 与 `root-发行版.sparse.img`。二者都是 Android 稀疏镜像，使用原厂 Fastboot 刷写，不能直接用 `dd`。

## 刷写

电脑需安装 Android platform-tools。平板需解锁 Bootloader，并已通过助手建立独立存储分区。刷写 root 会覆盖独立系统中的文件。

在 Android 中进入原厂 Fastboot，查看设备序列号：

```sh
adb reboot bootloader
fastboot devices
```

将下面的 `SERIAL` 换成实际序列号，root 文件名换成下载的发行版：

```sh
fastboot -s SERIAL -S 128M flash sunuefi_root root-debian-gnome.sparse.img
fastboot -s SERIAL -S 128M flash sunuefi_esp esp.sparse.img
fastboot -s SERIAL reboot
```

引导仍通过 Android 中的助手安装和维护。没有独立分区时，先使用助手的“创建独立分区”；不要把上述镜像刷入 Android 的其他分区。

Linux 首次启动会自动扩展根文件系统，蓝牙地址也由系统自动读取。默认登录账户是 `piano`，密码是 `piano123456`。

源码中仍保留 `tools/install_piano.py`，供开发者使用本地完整构建包、安装计划和 SSH 公钥注入；CI 的普通下载不再附带这套脚本。故障恢复见[返回 Android](recovery.md)。
