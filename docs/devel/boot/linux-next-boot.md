# Linux 下次启动入口

`piano-next-boot` 使用现有合体 BOOT 请求页选择 Android、启动菜单、Linux 或 UEFI 设置。默认只保存下次路线；`--reboot` 才请求 systemd 正常重启。本工具不重包内核、不切槽、不写 misc/PMIC，也不增加后台服务。

当前选择会持续保留，后续正常开机继续沿用，直到改选其他路线。一次性请求自动消费尚未实现。已部署的新选择器让原厂 Recovery 优先，持久 Linux 偏好与 Recovery 的组合检查仍待完成。

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

## 直接识别活动槽位

不需要安装配置，也不在 ESP 或 root 中保存 BOOT 槽位副本。工具通过标准分区名找到 `boot_a`、`boot_b` 所在磁盘，只读校验主备 GPT，根据高通的活动、优先级、成功和剩余尝试标志选出当前可启动槽位，再核对内核提供的分区范围。

高通 GPT 属性使用 bit 48～49 表示优先级、bit 50 表示活动槽、bit 51～53 表示剩余尝试、bit 54 表示启动成功、bit 55 表示不可启动。字段定义来自高通公开的 [partition_parser.h](https://github.com/msm8916-mainline/lk2nd/blob/master/platform/msm_shared/include/partition_parser.h)，选择逻辑参考 [ab_partition_parser.c](https://github.com/msm8916-mainline/lk2nd/blob/master/platform/msm_shared/ab_partition_parser.c)。本机当前 GPT 与 Android 报告的活动 B 槽一致。

GPT 只决定原厂加载 A、B 哪个 BOOT；Android／UEFI／Linux 的选择仍写入该 BOOT 内 SunUEFI 自有的双请求页。工具不改 GPT、槽位优先级或原厂内核。主备 GPT 不一致、活动槽位不或当前 BOOT 没有 SunUEFI 时明确报错，不回退到旧槽位。

每次操作都从 BOOT 读取当前 APP generation，写入前后复核活动槽位；原生写入工具再核对 generation、请求页内容、同步及读回。ESP 未挂载也不影响系统切换。

APP generation为SPLITv1元数据+96处16字节，也是NEXTv1+40处的绑定值。原生`piano-boot-request status --device ...`新增`app_generation` JSON字段，便于安装读回核对。`set/consume --expect-generation HEX32`在最初定位后与打开可写FD、重新定位时两次核对该值；不匹配时不写请求页。旧参数仍兼容，但产品helper总是传pin，不重复实现SPLIT parser或CRC。

Android=0、menu=1、Linux=2、Setup=3；设置Android以更高sequence写target0，取消旧Linux请求。共享CRC、双请求页、fsync/readback和原块设备ro恢复仍由原生工具负责。

## 打包位置

| 仓库源 | guest安装位置 |
| --- | --- |
| `linux/userspace/piano-next-boot` | `/usr/bin/piano-next-boot`，0755 root |
| `linux/userspace/piano-next-boot-helper` | `/usr/libexec/piano-next-boot-helper`，0755 root |
| 原生 `piano-boot-request` ELF | `/usr/local/sbin/piano-boot-request`，0755 root |
| `org.sunuefi.boot-request.policy` | `/usr/share/polkit-1/actions/org.sunuefi.boot-request.policy`，0644 root |
| `49-piano-boot-request.rules` | `/usr/share/polkit-1/rules.d/49-piano-boot-request.rules`，0644 root |
| 两份 `org.sunuefi.*.desktop` | `/usr/share/applications/`，0644 root |

polkit action只绑定固定helper executable；有限规则仅放行local+active+sudo/wheel管理员，其他active用户仍要求auth_admin，inactive/any拒绝。不保留长期授权。helper只接受四个固定target/status及可选--reboot，不接受外部device/config/binary/任意命令。原生工具检查选定分区内的SunUEFI布局、request CRC及generation。helper不是setuid，不需新daemon。卸载该rules文件即可回到全体active用户auth_admin；不会改变sudoers或账号密码。

release root builder 为 Debian、Nyarch 和 SteamOS 安装同一套工具、静态原生命令及 Python GI/GTK3、pkexec 依赖，直接使用，无需额外配置命令。

源码阶段的必要检查覆盖 generation 不符时拒绝写入、共享 CRC、请求页范围及非法参数；Python、desktop 和 polkit 格式检查通过。部署、重启和 UI 检查范围见上方当前状态。
