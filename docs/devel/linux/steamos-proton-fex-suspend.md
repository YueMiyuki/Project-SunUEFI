# Proton/FEX 协作挂起与游戏冻结

记录在 SteamOS ARM64 上通过 ARM64 Proton 启动《原神》时遇到的一次启动冻结，以及
`STEAM_FEX_MULTIBLOCK=0` 规避方法。问题不在游戏文件，也不随 Proton/FEX 版本变化。

## 现象

游戏由官方米哈游启动器拉起，日志停在：

```text
GameManager CreateNewGameWorld Home
Home LoadScene
```

画面能渲染健康忠告或用户协议页，但不会自动继续，按钮也没有响应。进程里有一条线程
持续占用约一个核心，其余线程大多在等待。

实测环境为 SteamOS ARM64、Proton Experimental (ARM64) bleeding-edge
`2610-25`、原神 7.1.0 国服。GE-Proton11-11、Proton 11.0-2e 和 Experimental
三个版本都能复现，替换 FEX 版本也没有改变结果。

## 现场

用 `eu-stack` 和 `/proc/<pid>/task/<tid>/syscall` 可以看到：

* 进程主线程停在 host 侧 `NtSuspendThread`，等一个 Wine 进程内同步对象；
* 另有多条线程停在 `usr1_handler → wait_suspend`，即已经被挂起；
* 一条线程在 FEX JIT 区域持续执行，PC 落在同一个 JIT 基本块内。

这条自旋线程的客户机入口是 `YuanShen.exe + 0x1693540`（镜像基址 `0x140000000`）。
循环位于 `+0x16936b0` 到 `+0x1693741`，核心是：

```asm
lock cmpxchg dword ptr [rbx+0x28], ecx
...
pause
cmp eax, r13d            ; r13d = 0xffffffd6 (-42)
```

`-42` 是“锁已被其它线程持有”的标记。线程在这个基本块内部反复尝试，直到标志位改变。

## 原因

Wine 的 ARM64EC 线程挂起是协作式的。`dlls/ntdll/unix/signal_arm64.c` 中的
`usr1_handler` 在发现当前线程正在模拟 x86 代码时，会向 wineserver 发起
`SELECT_COOPERATIVE_SUSPEND`；被挂起的线程需要在一个安全点响应。

FEX 的挂起点检查由 `EmitSuspendInterruptCheck()` 发出，位置在 JIT 基本块入口：
读取 `CpuStateFrame::SuspendDoorbell`，非零时执行 `brk #0xCAFE`。这对应
`FEXCore/Source/Interface/Core/JIT/JIT.cpp`。

如果客户机代码在一个基本块内部循环（例如上面这把锁的自旋等待），执行流不会回到块
入口，门铃就不会被检查。结果是：

1. 游戏在场景加载后调用 `SuspendThread`；
2. 主线程等待目标线程进入挂起状态；
3. 目标线程在 JIT 块内自旋，永远不检查门铃；
4. `NtSuspendThread` 永远不返回，主线程和整个游戏停住。

因此换 Proton 版本、换 FEX 版本都不会改变现象。问题出在“协作挂起”和“块内自旋”的
组合，不是游戏资源或图形驱动。

## 规避

在启动游戏的 Proton 环境里设置：

```sh
STEAM_FEX_MULTIBLOCK=0
```

Proton 会把它写入该游戏 compatdata 下的 `proton-fex-config.json`：

```json
{ "Config": { "Multiblock": "0" }, "ThunksDB": {} }
```

关闭 multiblock 后，循环回边不再并入同一个 JIT 块，执行会经过块入口，门铃在那里
被检查，挂起能够完成。实测游戏可以越过 `Home LoadScene`，显示用户协议并进入游戏世界。

这套规避只修改 Proton/FEX 的运行配置，不修改游戏文件、启动器或反作弊组件。

从 Steam 启动时，把启动项写成：

```text
STEAM_FEX_MULTIBLOCK=0 %command%
```

该环境变量由启动器进程继承，启动器再拉起的 `YuanShen.exe` 同样生效。Steam 从
2022 年起会为非 Steam 快捷方式解释 `%command%` 和环境变量前缀。直接使用
`proton run` 时在命令前导出变量即可。

启动后可以检查该快捷方式 compatdata 下的 `proton-fex-config.json` 是否包含
`"Multiblock": "0"`；如果仍是空 `Config`，说明变量没有传到 Proton。

注意 `STEAM_COMPAT_FEX_CONFIG` 会同时改写 `TSOEnabled` 和 `Multiblock`，不设置
`TSOEnabled:1` 时会把 TSO 一起关掉；只调整 multiblock 时使用
`STEAM_FEX_MULTIBLOCK`。

## 代价与验证

multiblock 关闭会缩小 JIT 基本块，增加块调度次数，属于用一点性能换正确性。当前实测
游戏可以正常进入世界，帧率和长时间稳定性尚未系统测量。

验证方式：设置变量后重新启动，日志应越过 `Home LoadScene`，出现
`ProtocolDialog`、`submit shader load` 等后续记录；进程里不应再出现主线程停在
`NtSuspendThread`、同时有线程在 JIT 块内 100% 自旋的组合。

## 参考

* Wine `dlls/ntdll/unix/signal_arm64.c`：`usr1_handler` 的协作挂起分支
* Wine `server/thread.c`：`stop_thread()` 与 `suspend_thread()`
* FEX `FEXCore/Source/Interface/Core/JIT/JIT.cpp`：`EmitSuspendInterruptCheck()`
* FEX `FEXCore/include/FEXCore/Core/CoreState.h`：`CpuStateFrame::SuspendDoorbell`
