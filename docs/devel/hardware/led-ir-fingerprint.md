# Piano 的 RGB 灯、红外与侧指纹

2026-10-08 核对原厂最终 DT、Android 驱动绑定、当前 Linux 源码和已有运行记录。以下三项尚未完成实机功能验证；没有操作发光、发射、指纹录入或认证。

| 功能 | 原厂接口 | 当前 Linux 缺口 |
| --- | --- | --- |
| RGB 通道 | PM8550 SID1 上的 `qcom,pm8350c-pwm`，SPMI LPG，通道 1/2/3 为 red/green/blue | 标准驱动和产品DT已有；补齐SDAM/LPG模块加载后已出现三个标准LED接口。发光、可见位置和用途尚待确认。 |
| 红外发射 | SE15 / SPI `0x89c000` / CS0，`ir-spi`，Android 原始波形字符设备 | 已确认原厂约40%占空比、低电平间隔和32位打包；控制器标准绑定及LIRC待接入，光学输出未验证。 |
| 侧指纹 | Goodix，GPIO 控制与高通 QTEE 安全服务 | 供电、复位、中断和安全程序设备查询已接通；录入、认证和 fprintd 接入尚未完成。 |

## RGB 通道

原厂 `/soc/qcom,spmi@c42d000/qcom,pm8550@1/led-controller` 绑定 `qcom-spmi-lpg` / `leds_qcom_lpg`。三个子节点原样指定 `reg=1/2/3`、红/绿/蓝颜色和相应 label。其两个 nvmem 依赖是 **PMK8550 SID0 的 `sdam@8400`、`sdam@8500`**，用于 LPG 通道状态和图案数据，不是相机闪光灯的 `0xee00` 外设。

当前产品 `board.dtb` 已保留上述节点和依赖，节点未写 status，按 DT 规则启用。当前配置已有 `LEDS_QCOM_LPG=m`、`LEDS_CLASS_MULTICOLOR=m`、`NVMEM_SPMI_SDAM=m`；[上游 LPG 驱动](https://github.com/torvalds/linux/blob/master/drivers/leds/rgb/leds-qcom-lpg.c)已经支持该 compatible、通道和标准 LED class，[绑定说明](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/leds/leds-qcom-lpg.yaml)也接受这些字段。**无需再加一个重复 RGB overlay。**

Linux起初没有RGB，LPG的deferred-probe原因为未取得LPG chan SDAM。正确提供者模块名为 `nvmem_qcom_spmi_sdam`；加载它及 `leds_qcom_lpg` 后，platform device绑定 `qcom-spmi-lpg`，出现 `red`、`green`、`blue`，初始brightness为0、max_brightness为255。没有读取SDAM内容或写brightness，尚不证明可见灯的位置或用途。

[通用模块加载文件](../../../linux/bsp/common/etc/modules-load.d/piano-rgb.conf)已写入BSP manifest和完整root stager，也已安装到当前平板；下一次正常启动会按同一策略加载，不添加daemon或重复DT节点。记录在 `内部记录目录 `sdam@7000/sm1510_present@5d` cell 曾因无效位范围阻止其提供者注册；打包器现保留完整原始字节并移除无效位切片，详见[启动设备树处理](../linux/kernel.md#piano-启动配置)。LPG 使用的是 8400/8500 两个 SDAM。

## 红外发射

原厂最终 DT 的子节点是 `/soc/qcom,qupv3_2_geni_se@8c0000/spi@89c000/ir-spi@0`，最大 SPI 时钟 19.2 MHz， Android 绑定 `ir_spi`。GPIO28/29/30/31 分别是 MISO/MOSI/CLK/CS，复用到 QUP2 SE7。原厂 HAL 清单有 `android.hardware.ir-service.example`。

[固定 MiCode 驱动](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/blob/45fb9bd6ae5ba2942fc1d53e4b6b46ef76992f71/drivers/media/rc/ir-spi.c)注册 `/dev/ir_spi`，以 32 位 SPI word、1.92 MHz 默认时钟发送用户空间提供的原始位图。它把 `LIRC_SET_SEND_MODE` 用作缓冲区长度设置，并没有标准 rc-core 的脉冲/间隔接口。当前 ROM 模块的 probe 反汇编也确认 32 位和 1.92 MHz；不能据 ioctl 名字把它当成标准 LIRC 驱动。

原厂服务的 `ConsumerIr::transmit` 只把频率和原始脉冲数组转交给 legacy `consumerir_device_t`，底层返回值随后被忽略并返回AIDL成功，不能据此认定发射完成；这与 [AOSP AIDL 服务](https://android.googlesource.com/platform/hardware/interfaces/+/a2a9fa50039d69643527020ab6706b319f0e6c62/ir/aidl/default/main.cpp)一致。编码位于本ROM的 `/odm/lib64/hw/consumerir.qcom.so`（SHA256 `fc9e6f2c7770b0f5eb765af8e6eec17494ba7a081a47051606188bec17164f9a`），不是服务主二进制。已按目录元数据只提取这两个明确文件；没有执行。该模块是ODM目录consumerir草案，但现有属性采集未包含 `ro.hardware.consumerir`/`ro.hardware`，尚未通过运行时maps确认它被选择。

`consumerir_transmit_mi`（VA `0x40f8`）以固定 **1.92MHz** 生成波形：周期采样点数为 `round(1920000 / carrier)`，高电平点数为 `floor(2 × period / 5)`。38kHz请求得到51点周期、20高/31低，名义载波约37647Hz。偶数脉冲段调制载波，奇数间隔段全部为0并复位载波计数器。样本从原生little-endian `u32`的bit31向bit0填入，不做byte-swap，由原厂驱动的32位SPI word接口发送；最终写入长度向上对齐1024字节，填充区清零。累积时间边界减5µs、时间转换与长度上限等细节保存在私有 `ir-encoding-facts.json`，不能照搬成无边界检查的实现。

该HAL只打开 `/dev/ir_spi`、设置传输字节数、写入并关闭，没有GPIO、regulator或额外idle写操作。编码的间隔和填充为低电平，但这不能证明晶体管/光学输出极性，或在无填充的末尾脉冲后控制器最终保持的电平。

最小标准适配方向是：将控制器转为 `qcom,geni-spi`、时钟名 `se`，逐项按当前GENI/GPI的clock、interconnect、DMA binding转换资源；子节点用 `ir-spi-led`，配置开启 `LIRC` 和 `IR_SPI`。现有产品DT仍是厂商 `qcom,spi-geni` / `se-clk`，不能只替换子节点compatible。标准驱动每个载波使用16个位样本、SPI时钟为载波×16；请求40%占空比时当前公式得到7/16高位，约43.75%，并不等同于原厂1.92MHz量化波形。移植可复用标准rc-core/LIRC接口及已确认的低电平间隔，但需明确是否保留原厂编码、word顺序与约40%占空比。原厂两层均无供电控制，标准驱动的regulator请求仍需按无可控供电接口处理，不能虚构supply或默默依赖dummy regulator。相关标准接口见 [IR SPI绑定](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/leds/irled/ir-spi-led.yaml)。

[控制器独立草案](../../../linux/dts/piano-ir-spi-controller.dtso)已使用当前内核headers编译并与参考产品DT合并检查，尚未接入产品构建或部署。GCC ID123、GIC SPI462、SE7、GPIO28–31都与原厂最终DT和当前 `sm8750.dtsi` 对应；DMA改为GPI2三cell参数 `0/1,7,SPI`，三个ICC路径使用现有标准provider及两cell参数。新default/sleep pinctrl位于 `piano_tlmm_ml`，保留原厂6mA/2mA、bias-disable，未添加输出电平操作。共享QUP2 wrapper、GPI2掩码和Linux拥有的SID `0x423/0x436`均未改变；同地址I2C节点仍禁用。

该草案只准备标准SPI控制器，现有CS0子节点已改为 `ir-spi-led` 但保持 **disabled**，没有供电、假设备或发射操作。合并检查证明资源能解析，不证明控制器已绑定或完成DMA；下一次正常产品集成后先检查GENI/SPI provider绑定及probe日志，不写SPI数据。当前GENI驱动会检查SE协议和FIFO状态，必要时加载SPI固件或取得GPI通道，因此仍可能报告协议/固件/依赖错误。属性差异保存在私有 `ir-controller-overlay-review.json`；未声称整份继承厂商DT通过上游schema校验。

[标准发送驱动草案](../../../patches/linux/7.2.9/drafts/piano-ir-spi-rc-core.patch)在现有 `ir-spi` 内新增显式 `xiaomi,piano-ir-spi` 设备分支，使用rc-core/LIRC回调及 `spi_sync_transfer()`，没有用户空间直接MMIO或私有发送syscall。原有 `ir-spi-led` 的16位编码和供电请求保持原样。Piano分支默认40%占空比、固定1.92MHz、native 32-bit MSB-first打包；载波限制到原厂声明范围30–56kHz，LIRC占空比请求按采样分辨率检查。不存在的可控供电不请求dummy regulator，也不添加supply。

输入按正常LIRC脉冲/间隔解释：必须为正值、奇数个、最多1024段，总时长不超过rc-core的1秒上限。采用累计时间量化，保留请求时长，不复制原厂减5µs或负值处理。对齐前额外保留一个零word，使最后一个SPI word必定为低电平；完成和失败使用SPI核心返回结果。草案已在独立副本用当前ARM64编译参数生成对象，并通过 `git apply --check`；未构建完整模块、未做schema校验、未接入配置或产品DT、未部署或发射。控制器草案仍保持原样和child disabled；未来启用时必须同时接入新compatible、LIRC/IR_SPI配置与模块打包，再验证SPI和光学行为。

## 侧指纹

HyperOS 4 的 `mfp-daemon` 使用 Goodix HAL、`/dev/xiaomi-fp`
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
[`0014-qcomtee-large-shared-memory.patch`](../../../patches/linux/7.3/0014-qcomtee-large-shared-memory.patch)
让大缓冲区使用可回收的 CMA 连续内存，小缓冲区保留原来的路径，
并继续登记和注销安全世界的共享内存桥。
[`piano-cma.dtso`](../../../linux/dts/piano-cma.dtso) 将默认池从 36 MiB
扩大到 256 MiB，保留原厂允许的分配地址范围。这部分内存空闲时仍能
供 Linux 使用，不是永久划走的固件内存。

传感器控制依据[固定 MiCode 驱动](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/blob/45fb9bd6ae5ba2942fc1d53e4b6b46ef76992f71/drivers/input/fingerprint/mi_fp/fp_driver.c)
提供供电、复位和中断：原厂 DT 指定 GPIO73 复位、GPIO85 IRQ，
`fp_3v3_vreg-supply` 指向 PM-HUMU L9。认证数据由安全服务处理；
Goodix 客户端使用 `QSEECom_send_modified_cmd()` 发送请求。
[`0017-uio-piano-fingerprint-control.patch`](../../../patches/linux/7.3/0017-uio-piano-fingerprint-control.patch)
提供内核供电、复位和中断控制，设备树位于
[`piano-fingerprint.dtso`](../../../linux/dts/piano-fingerprint.dtso)。
打开 `piano-goodix` UIO 设备时打开供电并释放复位，关闭时停止中断、
拉低复位并归还电源引用；没有 MMIO 映射，也不把中断当成认证结果。
Linux 已完成供电、复位和关闭。普通桌面应用的认证仍需 libfprint 后端。

原厂 `pm_humu_l9` 为 3.300 V。现有主线 PLDO 使用不同的离散电压档位；
必须选择不超过原厂上限的 3.296 V。之前上取整到 3.304 V 时，
`regulator_enable()` 尚未返回就发生系统复位；改用较低档位后，
打开与关闭均已正常完成。不要为适应档位把原厂电压上限放宽。

Goodix 客户端的设备信息命令为路由 1、命令 1000，128 字节请求、
64 字节响应，另带传感器共享缓冲区。Linux 能加载并正常关闭安全程序。
QSEECOMPAT 的 lookup 返回 `architecture=2`，命令应使用 64 位地址，
不能按外层固件的 ELF32 封装选择地址宽度。修正后传输层返回成功，
最初响应中的缓冲区状态为 `-1`，原因是小缓冲区来自 4 GiB 以上的伙伴
分配器。该安全程序接受的共享区域必须在 4 GiB 以下；使用默认 CMA
池中的 4 KiB 缓冲区后，传输、应用和缓冲区状态均返回成功，
传感器类型为 5。无需给每条命令申请大缓冲区，也无需为这个查询启动
文件／时间监听服务。

[`0018` 内核修正](../../../patches/linux/7.3/0018-qcomtee-public-buffers-cma.patch)
使应用共享内存使用设备树声明的连续内存池，保留驱动私有小消息的
普通分配方式。地址范围由池的配置决定，不通过伪造指针或用户空间
内存热补丁处理。

原生客户端位于
[`piano-goodix.c`](../../../linux/userspace/fingerprint/piano-goodix.c)，
使用固定版本的 Qualcomm MinkIPC／quic-teec 与 QCBOR，由四个发行版
共用的运行时构建器静态编译；来源记录在
[`config/fingerprint.json`](../../../config/fingerprint.json)。
执行 `sudo piano-fingerprint-ctl info` 可查询设备信息，`--help` 不访问设备。
签名程序按固件清单随镜像提供，不包含用户模板或密码。

`sudo piano-fingerprint-ctl initialize` 完成传感器初始化、原厂模型加载和
交互策略设置，再正常关闭传感器。实机返回传感器类型 5、芯片类型 49、
132 行 × 24 列阵列，以及 132 × 16 的导航区域。初始化响应中的芯片类型为零
表示沿用设备查询值，与原厂 HAL 的处理一致，不代表未知芯片。

该设备要求防伪和识别模型，两者均从同版原厂 `libgf_hal.so` 的导出数据符号
提取，属于通用算法参数，不是用户模板。CAP S／T 的选择依据设备返回的标志，
芯片类型 49 使用 `DLWeightLightDubaiU4_12_10_14MisTouch`。
来源库 SHA256、符号、文件偏移、大小和输出校验值全部记录在
[`固件清单`](../../../vendor/firmware/hyperos4-fingerprint/manifest.json)，
不会安装原厂可执行库。

协议顺序如下，所有命令都通过公开 QTEE 接口进入原厂签名程序：

| 命令 | 功能 |
| --- | --- |
| 1000 | 查询传感器类型和功能标志 |
| 1001 | 读取传感器参数与校准数据 |
| 1113 | 分块加载防伪／识别模型；每块 511900 字节，逻辑模型命令为 1111 |
| 1005、1109 | 完成初始化并应用交互策略 |
| 1009 | 进入待机检测 |
| 1016 | 读取并确认传感器中断 |
| 1002 | 关闭传感器，随后关闭安全程序并归还供电 |

`sudo piano-fingerprint-ctl watch 30` 在限定时间内监听 UIO 中断，最多 120 秒。
收到中断后先通过安全程序确认，再重新启用 IRQ；输出只有事件标志和中断计数，
不保存或输出图像、模板和认证令牌。启动与待机过程已收到中断并完成确认；
这不等于手指检测、匹配或解锁已经完成。Ctrl+C 可正常结束并释放硬件。

`sudo piano-fingerprint-ctl list 1000` 通过命令 1014 选择 Linux UID 1000 的
独立分组，再通过 1015 枚举模板。原生状态目录为
`/var/lib/piano-goodix/<uid>`，不复用 Android 路径，也不读取或修改原厂模板。
新建分组已正常返回空列表；最大模板数量取自设备信息，并按响应容量解析。
标准 fprintd 用户元数据仍由后续的 libfprint 集成管理。

`sudo piano-fingerprint-ctl prepare-auth` 初始化传感器并完成 Keymaster 到
Goodix 的认证数据绑定。没有录入或认证操作，也不输出绑定数据。
这一步已在重新启动后的 Linux 上独立完成，不依赖 Android 先运行指纹服务。

原厂加载的 64 位 `keymaster64` 报告 API 5.3／TA 5.3000。
同版 `libkeymasterdeviceutils.so` 和 `libqtikeymint.so` 表明，原生服务 151
打开指定安全应用的环境，应用内服务 150 提供命令接口。客户端执行
`0x200`／`0x207` 协商；需要时通过 `0x220e` 取得测试启动的共享参数，
用 `0x220f` 完成 HMAC sharing。请求保留准确 CBOR 长度，不把对齐填充算入协议。

绑定请求仍沿用原厂 CA 的 `0x205, 2` 和 256 字节响应，再通过 Goodix 路由 2、
命令 109 传入签名安全程序。Goodix 必须先加载并初始化；之前在它尚未加载时
查询，会得到另一个错误。只有明确返回 `-10020` 时才初始化测试启动的
sharing key，已有绑定可用时直接使用，避免重置其他客户端的有效会话。
响应的偏移和长度均检查边界，临时参数和加密绑定数据在结束时清零。
没有把密钥、令牌或指纹模板保存到普通文件。

另已补齐安全程序使用的
[旧 GPFS 文件命令](../../../linux/bsp/patches/qtee/README.md)，当前仅作为待集成源码保留，
不增加默认后台服务。诊断使用的 RPMB 提供者拒绝写入，未修改原厂安全存储。

完整命令协议和 libfprint 后端仍在适配，因此不能在 Linux 中录入或验证指纹。

随后bde3普通重启发现，虽然modules-load文件已安装，旧modprobe黑名单仍使systemd把LPG记为deny-listed。已仅移除 `leds_qcom_lpg` 这一条，并让完整root stager与通用BSP使用同一份tracked modprobe配置。重新启动标准modules-load后插入LPG并出现三个接口，随后一次完整正常重启也已确认三个接口自动出现。当前记录在 `bde3-rgb-startup.txt` 与 `bde3-rgb-policy-fixed.txt`，不能用之前手动modprobe结果替代启动策略验证。
