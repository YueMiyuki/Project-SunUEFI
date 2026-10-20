# 内核角色与固定来源

同一个 `stable` 名称在 RAM 基线、完整集成、EFI 文件组装和真实磁盘部署中有不同用途。**角色不同而提交不同，不等于 pin 漂移。** 下面记录 2026-10-07 的分工；当前运行和验收结论以 [状态](../status.md) 中绑定的镜像/会话为准。

| 角色 | 固定 commit | 消费位置/用途 |
| --- | --- | --- |
| RAM/救援 Stable 基线 | `7a33c60fd6eda8a9c20dfda636d4e0a4efa4cbb8` | `linux/kernel-profiles.json`、主仓库 Gitlink；`build_kernel.py` 的 stable profile |
| RAM Next 快照 | `7704c4c5bb127673b4f0ead839919db573559e38` | 同一 JSON 的 next profile；独立 RAM smoke，不代表完整板级功能 |
| 完整 Stable 公开基线 | `352508459733d3e6d349ea5581a8dd2fd8bb4180` | `build_piano_full_kernel.py` 默认基线；允许明确指定并验证其后代作为本地完整候选 |
| Stable EFI 文件组装候选 | `d42158782b81c4aaa47c8643f1400a471785370b` | `assemble_piano_linux.py` 的 stable 固定来源；组装检查仍保留运行时 DDR/退休/readback 门槛 |
| 完整 Next 迁移候选 | `498569101e34cbb6ec9c27ebe609949279a59bce` | `build_piano_next_full.py`；官方 base 为 `67f0943b394d920b6c142aad8c6af94340342ae7` |
| 当前磁盘 Stable（Kernel69） | `efe5734c24510c3c194f765511b49be9f13b4aa0` | 已部署磁盘版本的身份；不是上述构建工具的默认 pin，配套模块与入口状态见当前记录 |

内核源码由独立仓库维护。当前 `.gitmodules` 的 `../linux-piano` 描述同级仓库安排；相对 URL 在公开托管环境中是否可用，取决于主仓库 remote 所解析的目标以及固定对象能否取得。公开 kernel remote、全部必要 commit 的可获取性和独立克隆验收尚待完成。保留现有 Gitlink/pins；本地 linked-worktree 的绝对 `.git` 指针不能当作可迁移的克隆步骤。

## 更新一个角色

先明确更新哪个角色、旧产物与恢复基线怎样保留，再更新该角色的消费入口。RAM Stable 涉及 Gitlink 与 JSON 时需保持二者一致；完整候选和组装角色按各自工具校验，不为追求相同版本而连带改变救援 pin。

候选必须记录实际 source/base、配置片段与最终 config、编译工具、Image、DTB、initramfs/rootfs、kernel release 和对应模块身份。来源验证、配套检查、启动路径和实机结果分别记录。磁盘 raw 启动成功不会自动证明标准 EFI-stub 或 recovery 入口成功。

补丁缩减沿用 `audit_kernel_upstream.py` 的祖先关系、patch-id、changed-byte fingerprint 和 unknown 区分。缺历史对象或证明不完整时保留 unknown，不能凭名称相似自动删补丁。相关背景见 [Linux 构建记录](../linux-reproducible-builds.md) 与 [完整迁移记录](../piano-next-full-migration.md)。


## Piano 启动配置

打包时由 `tools/package_release.py` 将折叠后的原厂 DT 转成主线使用的最终设备树。
旧 SDE／CPAS／VIDC／SPF 节点由主线显示、CAMSS、Iris 和音频节点接管；旧 ICC
参数和被误识别为 GPIO 的厂商标志不再作为标准绑定传给内核。ADSP 的固件、
内存和电源资源保留，只删除主线 PAS 驱动不使用的旧 ICC 属性。

原厂在本板同样无法探测的四个可选 PMIC，以及不属于 AP 的两个硬件投票 RSC
节点，在最终树中禁用；AP 的 `rsc@16500000` 和实际外设供电保持启用。
`piano-platform.config` 排除 NXP 的 SCMI 扩展，保留 Qualcomm 使用的标准协议。
USB PHY 没有连接显示拓扑时不创建空的 DRM 桥，USB PHY 本身照常工作。

这套配置已从正常启动的内核日志核对：上述引用、SPMI、SCMI、DRM 桥及 RSC
报错消失，ADSP、音频、键盘、蓝牙和 USB 调试连接保持工作。

磁盘启动统一使用 `linux/userspace/disk-bootstrap`，支持发布镜像的 LABEL 和
开发镜像的 PARTUUID。ext4 先正常回放日志，再转为读写；救援环境包含磁吸键盘
所需的 GENI I²C、GPI、Nanosic 驱动和 MCU RAM 固件。
