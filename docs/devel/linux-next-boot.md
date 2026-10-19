# Linux 下次启动入口

`piano-next-boot` 使用现有合体 BOOT 请求页选择 Android、启动菜单、Linux 或 UEFI 设置。默认只保存下次路线；`--reboot` 才请求 systemd 正常重启。本工具不重包内核、不切槽、不写 misc/PMIC，也不增加后台服务。

当前选择会持续保留，后续正常开机继续沿用，直到改选其他路线。一次性请求自动消费尚未实现。已部署的新选择器让原厂 Recovery 优先，持久 Linux 偏好与实际 Recovery 的组合检查仍待完成。

Android 与 Linux 可通过同一份合体 BOOT 相互切换。原生工具校验当前启动记录，写入后同步并读回，再执行正常重启。

## 使用

```sh
piano-next-boot status
piano-next-boot android          # 保存下次进入原厂 Android
piano-next-boot menu             # 保存下次进入启动选择菜单
piano-next-boot android --reboot # 保存成功后正常重启
```

普通用户通过 pkexec 调用。本地活动会话中的 sudo／wheel 管理员可使用这一项 polkit 授权；其他用户需要管理员认证。默认账户和密码见项目首页。root 可直接调用同一 CLI。

两个`.desktop`入口为“返回 Android”和“SunUEFI 启动选择菜单”。按钮先显示GTK确认对话框，提醒保存当前工作，再走相同CLI/helper。取消不保存请求；失败显示错误。桌面环境需要标准polkit认证代理及Python GI/GTK3，CLI仅需Python标准库、pkexec和原生工具。

## 安装配置

Android 助手在安装引导、重装或切换到 Linux 时，将当前活动槽和 UEFI 标识写入 ESP 的 `EFI/SunUEFI/boot-request.json`。Linux 优先读取 `/boot/efi/EFI/SunUEFI/boot-request.json`，所以 OTA 从 A 槽切换到 B 槽后也会使用新的 BOOT。

旧安装方式仍可使用本地的 `/etc/piano/boot-request.json`。格式相同：

```json
{
  "version": 1,
  "boot_device": "实际 /dev/disk/by-partlabel/boot_a 或 boot_b",
  "app_generation": "实际 16-byte APP generation，32位十六进制"
}
```

上面是字段说明，不能直接当有效配置。仓库的`boot-request.example.json`采用null占位，发布镜像不应预填某台测试设备的boot_a。缺配置或分区不存在时明确报错，不扫描分区或自动改到另一槽。每次操作先读取指定分区中当前有效的 APP generation，再以该值约束实际写入；Android 更新同一分区的合体 BOOT 后，不再因 Linux 保存旧标识而拒绝切换。若操作期间 BOOT 再次改变，原生工具仍拒绝写入。

APP generation为SPLITv1元数据+96处16字节，也是NEXTv1+40处的绑定值。原生`piano-boot-request status --device ...`新增`app_generation` JSON字段，便于安装读回核对。`set/consume --expect-generation HEX32`在最初定位后与打开可写FD、重新定位时两次核对该值；不匹配时不写请求页。旧参数仍兼容，但产品helper总是传pin，不重复实现SPLIT parser或CRC。

Android=0、menu=1、Linux=2、Setup=3；设置Android以更高sequence写target0，取消旧Linux请求。共享CRC、双请求页、fsync/readback和原块设备ro恢复仍由原生工具负责。

## 打包位置

| 仓库源 | guest安装位置 |
| --- | --- |
| `linux/userspace/piano-next-boot` | `/usr/bin/piano-next-boot`，0755 root |
| `linux/userspace/piano-next-boot-helper` | `/usr/libexec/piano-next-boot-helper`，0755 root |
| `linux/userspace/piano-next-boot-configure` | `/usr/sbin/piano-next-boot-configure`，0755 root，仅安装器/root设置配置 |
| 原生 `piano-boot-request` ELF | `/usr/local/sbin/piano-boot-request`，0755 root |
| `org.sunuefi.boot-request.policy` | `/usr/share/polkit-1/actions/org.sunuefi.boot-request.policy`，0644 root |
| `49-piano-boot-request.rules` | `/usr/share/polkit-1/rules.d/49-piano-boot-request.rules`，0644 root |
| 两份 `org.sunuefi.*.desktop` | `/usr/share/applications/`，0644 root |
| `boot-request.example.json` | `/usr/share/doc/piano-next-boot/boot-request.example.json`，仅示例 |

polkit action只绑定固定helper executable；有限规则仅放行local+active+sudo/wheel管理员，其他active用户仍要求auth_admin，inactive/any拒绝。不保留长期授权。helper只接受四个固定target/status及可选--reboot，不接受外部device/config/binary/任意命令。配置只容许实际boot_a/boot_b partlabel并验证为块设备；原生工具实际检查该分区内的SunUEFI布局、request CRC及generation。helper不是setuid，不需新daemon。卸载该rules文件即可回到全体active用户auth_admin；不会改变sudoers或账号密码。

release root builder 已接入这些文件、静态原生命令及 Python GI/GTK3、pkexec 依赖，不预填设备配置。安装器或受信任 root 终端应明确指定已安装入口：

```sh
piano-next-boot-configure --device /dev/disk/by-partlabel/实际已安装的boot分区
piano-next-boot status
```

配置命令从实际分区读回 APP generation，原子保存配置，不写 BOOT 请求；它没有扫描当前槽或覆盖原厂镜像。研究设备的实际 boot_a 配置与只读 status 匹配，CLI 返回 Android 已完成；这不等于所有用户都应配置 boot_a。

源码阶段的必要检查覆盖 generation 不符时拒绝写入、共享 CRC、请求页范围及非法参数；Python、desktop 和 polkit 格式检查通过。实际部署、重启和 UI 检查范围见上方当前状态。
