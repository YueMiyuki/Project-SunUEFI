# Piano 的 RGB 灯、红外与侧指纹

2026-10-08 核对原厂最终 DT、Android 实际驱动绑定、当前 Linux 源码和已有运行记录。以下三项尚未完成实机功能验证；没有操作发光、发射、指纹录入或认证。

| 功能 | 原厂实际接口 | 当前 Linux 缺口 |
| --- | --- | --- |
| RGB 通道 | PM8550 SID1 上的 `qcom,pm8350c-pwm`，SPMI LPG，通道 1/2/3 为 red/green/blue | 标准驱动和产品DT已有；补齐SDAM/LPG模块加载后已出现三个标准LED接口。实际发光、可见位置和用途尚待确认。 |
| 红外发射 | SE15 / SPI `0x89c000` / CS0，`ir-spi`，Android 原始波形字符设备 | 已确认原厂约40%占空比、低电平间隔和32位打包；控制器标准绑定及LIRC待接入，实际光学输出未验证。 |
| 侧指纹 | Goodix，GPIO 控制与高通 QTEE 安全服务 | Linux 已能加载原厂签名的安全程序；传感器控制、录入、认证和 fprintd 接入尚未完成。 |

## RGB 通道

原厂 `/soc/qcom,spmi@c42d000/qcom,pm8550@1/led-controller` 实际绑定 `qcom-spmi-lpg` / `leds_qcom_lpg`。三个子节点原样指定 `reg=1/2/3`、红/绿/蓝颜色和相应 label。其两个 nvmem 依赖是 **PMK8550 SID0 的 `sdam@8400`、`sdam@8500`**，用于 LPG 通道状态和图案数据，不是相机闪光灯的 `0xee00` 外设。

当前产品 `board.dtb` 已保留上述节点和依赖，节点未写 status，按 DT 规则启用。当前配置已有 `LEDS_QCOM_LPG=m`、`LEDS_CLASS_MULTICOLOR=m`、`NVMEM_SPMI_SDAM=m`；[上游 LPG 驱动](https://github.com/torvalds/linux/blob/master/drivers/leds/rgb/leds-qcom-lpg.c)已经支持该 compatible、通道和标准 LED class，[绑定说明](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/leds/leds-qcom-lpg.yaml)也接受这些字段。**无需再加一个重复 RGB overlay。**

实际Linux起初没有RGB，LPG的deferred-probe原因为未取得LPG chan SDAM。正确提供者模块名为 `nvmem_qcom_spmi_sdam`；加载它及 `leds_qcom_lpg` 后，实际platform device绑定 `qcom-spmi-lpg`，出现 `red`、`green`、`blue`，初始brightness为0、max_brightness为255。没有读取SDAM内容或写brightness，尚不证明实际可见灯的位置或用途。

[通用模块加载文件](../../linux/bsp/common/etc/modules-load.d/piano-rgb.conf)已写入BSP manifest和完整root stager，也已安装到当前平板；下一次正常启动会按同一策略加载，不添加daemon或重复DT节点。实际记录在 `private/provisioning/recovery-priority-20261008/a8-rgb-provider-load.txt`。另一个无关的 `sdam@7000/sm1510_present@5d` cell描述导致其提供者注册失败，但LPG使用的8400/8500已成功；不将这个告警掩盖或扩大解释为全部SDAM正常。

## 红外发射

原厂最终 DT 的子节点是 `/soc/qcom,qupv3_2_geni_se@8c0000/spi@89c000/ir-spi@0`，最大 SPI 时钟 19.2 MHz，实际 Android 绑定 `ir_spi`。GPIO28/29/30/31 分别是 MISO/MOSI/CLK/CS，复用到 QUP2 SE7。原厂 HAL 清单有 `android.hardware.ir-service.example`。

[固定 MiCode 驱动](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/blob/45fb9bd6ae5ba2942fc1d53e4b6b46ef76992f71/drivers/media/rc/ir-spi.c)注册 `/dev/ir_spi`，以 32 位 SPI word、1.92 MHz 默认时钟发送用户空间提供的原始位图。它把 `LIRC_SET_SEND_MODE` 用作缓冲区长度设置，并没有标准 rc-core 的脉冲/间隔接口。当前 ROM 模块的 probe 反汇编也确认 32 位和 1.92 MHz；不能据 ioctl 名字把它当成标准 LIRC 驱动。

原厂服务的 `ConsumerIr::transmit` 只把频率和原始脉冲数组转交给 legacy `consumerir_device_t`，底层返回值随后被忽略并返回AIDL成功，不能据此认定发射完成；这与 [AOSP AIDL 服务](https://android.googlesource.com/platform/hardware/interfaces/+/a2a9fa50039d69643527020ab6706b319f0e6c62/ir/aidl/default/main.cpp)一致。实际编码位于本ROM的 `/odm/lib64/hw/consumerir.qcom.so`（SHA256 `fc9e6f2c7770b0f5eb765af8e6eec17494ba7a081a47051606188bec17164f9a`），不是服务主二进制。已按目录元数据只提取这两个明确文件；没有执行。该模块是ODM目录唯一consumerir候选，但现有属性采集未包含 `ro.hardware.consumerir`/`ro.hardware`，尚未通过运行时maps确认它被选择。

`consumerir_transmit_mi`（VA `0x40f8`）以固定 **1.92MHz** 生成波形：周期采样点数为 `round(1920000 / carrier)`，高电平点数为 `floor(2 × period / 5)`。38kHz请求得到51点周期、20高/31低，名义实际载波约37647Hz。偶数脉冲段调制载波，奇数间隔段全部为0并复位载波计数器。样本从原生little-endian `u32`的bit31向bit0填入，不做byte-swap，由原厂驱动的32位SPI word接口发送；最终写入长度向上对齐1024字节，填充区清零。累积时间边界减5µs、时间转换与长度上限等精确细节保存在私有 `ir-encoding-facts.json`，不能照搬成无边界检查的实现。

该HAL只打开 `/dev/ir_spi`、设置传输字节数、写入并关闭，没有GPIO、regulator或额外idle写操作。编码的间隔和填充为低电平，但这不能证明晶体管/光学输出极性，或在无填充的末尾脉冲后控制器最终保持的电平。

最小标准适配方向是：将控制器转为 `qcom,geni-spi`、时钟名 `se`，逐项按当前GENI/GPI的clock、interconnect、DMA binding转换资源；子节点用 `ir-spi-led`，配置开启 `LIRC` 和 `IR_SPI`。现有产品DT仍是厂商 `qcom,spi-geni` / `se-clk`，不能只替换子节点compatible。标准驱动每个载波使用16个位样本、SPI时钟为载波×16；请求40%占空比时当前公式得到7/16高位，约43.75%，并不等同于原厂1.92MHz量化波形。移植可复用标准rc-core/LIRC接口及已确认的低电平间隔，但需明确是否保留原厂编码、实际word顺序与约40%占空比。原厂两层均无供电控制，标准驱动的regulator请求仍需按真实无可控供电接口处理，不能虚构supply或默默依赖dummy regulator。相关标准接口见 [IR SPI绑定](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/leds/irled/ir-spi-led.yaml)。

[控制器独立草案](../../linux/dts/piano-ir-spi-controller.dtso)已使用当前内核headers编译并与参考产品DT合并检查，尚未接入产品构建或部署。GCC ID123、GIC SPI462、SE7、GPIO28–31都与原厂最终DT和当前 `sm8750.dtsi` 对应；DMA改为GPI2三cell参数 `0/1,7,SPI`，三个ICC路径使用现有标准provider及两cell参数。新default/sleep pinctrl位于真实 `piano_tlmm_ml`，保留原厂6mA/2mA、bias-disable，未添加输出电平操作。共享QUP2 wrapper、GPI2掩码和Linux拥有的SID `0x423/0x436`均未改变；同地址I2C节点仍禁用。

该草案只准备标准SPI控制器，现有CS0子节点已改为 `ir-spi-led` 但保持 **disabled**，没有供电、假设备或发射操作。合并检查证明资源能解析，不证明控制器已绑定或完成DMA；下一次正常产品集成后先检查GENI/SPI provider绑定及probe日志，不写SPI数据。当前GENI驱动会检查真实SE协议和FIFO状态，必要时加载SPI固件或取得GPI通道，因此仍可能报告协议/固件/依赖错误。精确属性差异保存在私有 `ir-controller-overlay-review.json`；未声称整份继承厂商DT通过上游schema校验。

[标准发送驱动候选](../../patches/linux/7.2.9/drafts/piano-ir-spi-rc-core.patch)在现有 `ir-spi` 内新增显式 `xiaomi,piano-ir-spi` 设备分支，使用真实rc-core/LIRC回调及 `spi_sync_transfer()`，没有用户空间直接MMIO或私有发送syscall。原有 `ir-spi-led` 的16位编码和供电请求保持原样。Piano分支默认40%占空比、固定1.92MHz、native 32-bit MSB-first打包；载波限制到原厂声明范围30–56kHz，LIRC占空比请求按采样分辨率检查。不存在的可控供电不请求dummy regulator，也不添加supply。

输入按正常LIRC脉冲/间隔解释：必须为正值、奇数个、最多1024段，总时长不超过rc-core的1秒上限。采用累计时间量化，保留真实请求时长，不复制原厂减5µs或负值处理。对齐前额外保留一个零word，使最后一个SPI word必定为低电平；完成和失败使用SPI核心返回结果。候选已在独立副本用当前实际ARM64编译参数生成对象，并通过 `git apply --check`；未构建完整模块、未做schema校验、未接入配置或产品DT、未部署或发射。控制器草案仍保持原样和child disabled；未来启用时必须同时接入新compatible、LIRC/IR_SPI配置与模块打包，再验证实际SPI和光学行为。

## 侧指纹

HyperOS 4 的实际 `mfp-daemon` 使用 Goodix HAL、`/dev/xiaomi-fp`
和高通 QTEE。`libQSEEComAPI.so` 通过 `libminkdescriptor.so` 打开
UID 122 的 QSEECOMPAT 服务，再加载名为 **`gfpia`** 的签名安全程序。
原厂进程的映射和文件描述符已确认这条路径，不能套用其他小米平板的
FPC 驱动或 Goodix USB 驱动。

Linux 7.3 的 `qcomtee` 驱动提供标准 `/dev/tee0`。
使用高通公开的 [quic-teec](https://github.com/quic/quic-teec) 客户端，
已在实机取得同一个 QSEECOMPAT 服务，并成功加载、释放由原厂分段文件
还原的完整签名 `gfpia`。程序大小为 11,681,880 字节，QTEE 版本为
5.2.0；安全程序和加载接口均没有修改或绕过签名。

原始 QTEE 共享内存分配器使用 `alloc_pages_exact()`，超过伙伴分配器
支持的最大阶数时会报错。项目的
[`0014-qcomtee-large-shared-memory.patch`](../../patches/linux/7.3/0014-qcomtee-large-shared-memory.patch)
让大缓冲区使用可回收的 CMA 连续内存，小缓冲区保留原来的路径，
并继续登记和注销安全世界的共享内存桥。
[`piano-cma.dtso`](../../linux/dts/piano-cma.dtso) 将默认池从 36 MiB
扩大到 256 MiB，保留原厂允许的分配地址范围。这部分内存空闲时仍能
供 Linux 使用，不是永久划走的固件内存。

传感器控制依据[固定 MiCode 驱动](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/blob/45fb9bd6ae5ba2942fc1d53e4b6b46ef76992f71/drivers/input/fingerprint/mi_fp/fp_driver.c)
提供供电、复位和中断：原厂 DT 指定 GPIO73 复位、GPIO85 IRQ，
`fp_3v3_vreg-supply` 指向 PM-HUMU L9。认证数据由安全服务处理；
Goodix 客户端使用 `QSEECom_send_modified_cmd()` 发送请求。
[`0017-uio-piano-fingerprint-control.patch`](../../patches/linux/7.3/0017-uio-piano-fingerprint-control.patch)
提供内核供电、复位和中断控制，设备树位于
[`piano-fingerprint.dtso`](../../linux/dts/piano-fingerprint.dtso)。
打开 `piano-goodix` UIO 设备时打开供电并释放复位，关闭时停止中断、
拉低复位并归还电源引用；没有 MMIO 映射，也不把中断当成认证结果。
Linux 已完成供电、复位和关闭，完整认证仍需实现。

原厂 `pm_humu_l9` 为 3.300 V。现有主线 PLDO 使用不同的离散电压档位；
必须选择不超过原厂上限的 3.296 V。此前上取整到 3.304 V 时，
`regulator_enable()` 尚未返回就发生系统复位；改用较低档位后，
实际打开与关闭均已正常完成。不要为适应档位把原厂电压上限放宽。

Goodix 客户端的设备信息命令为路由 1、命令 1000，128 字节请求、
64 字节响应，另带传感器共享缓冲区。Linux 能加载并正常关闭安全程序，
但该查询仍返回安全服务错误，尚未取得传感器信息。完整命令协议和
libfprint 后端仍在适配，因此不能在 Linux 中录入或验证指纹。

随后bde3普通重启发现，虽然modules-load文件已安装，旧modprobe黑名单仍使systemd把LPG记为deny-listed。已仅移除 `leds_qcom_lpg` 这一条，并让完整root stager与通用BSP使用同一份tracked modprobe配置。重新启动标准modules-load后实际插入LPG并出现三个接口，随后一次完整正常重启也已确认三个接口自动出现。当前证据在 `bde3-rgb-startup.txt` 与 `bde3-rgb-policy-fixed.txt`，不能用之前手动modprobe结果替代启动策略验证。
