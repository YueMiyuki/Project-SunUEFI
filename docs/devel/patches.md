# 补丁管理与应用顺序

固件改动通过 Git patch 和准备脚本共同完成，分别服务于不同上游的固定输入。本页记录补丁的目标、应用顺序和校验入口。

## 产品构建中的顺序

`tools/build_product.sh` 先建立 `build/firmware-workspace/` 副本，在副本中依次应用补丁，上游子模块保持干净：

| 顺序 | 步骤 | 目标 | 说明 |
| --- | --- | --- | --- |
| 1 | `tools/apply_firmware_patches.py` | `upstream/Mu-Silicium`、`simple-init` | 应用 `patches/firmware/series.json` 中登记的 Git 补丁 |
| 2 | `tools/prepare_product_handoff.py apply` | Mu Basecore DXE | 安装晚期 APP 身份检查和退出钩子 |
| 3 | `tools/prepare_product_pump.py apply` | Mu DXE Event、Silicium 库绑定 | 安装事件分发与产品 pump 支持 |
| 4 | `tools/prepare_product_ui.py apply` | Mu DisplayEngine、SetupBrowser、UiApp | 安装菜单导航与退出处理 |
| 5 | `tools/prepare_nv_runtime_guard.py apply` | Basecore RuntimeDxe Variable.c | 安装变量运行时保护（编译参数 `PIANO_NV_BOOT_ONLY=1`） |
| 6 | `tools/build_simpleinit.sh --product-gui-pump` | SimpleInit | 编译 SimpleInit 图形菜单 |
| 7 | `tools/prepare_product.py` | `pianoProductPkg` | 组装组件、早期内存校验，生成产品平台描述 |
| 8 | `tools/build_stage0.sh product` | Mu Basecore | 编译固件（自动应用 Mu `Resources/MuPatches/` 补丁） |

准备完成后，可单独校验各步骤是否到位（在工作区目录下运行）：

```sh
python3 build/firmware-workspace/tools/prepare_product_handoff.py verify
python3 build/firmware-workspace/tools/prepare_product_pump.py verify
python3 build/firmware-workspace/tools/prepare_product_ui.py verify
python3 build/firmware-workspace/tools/prepare_nv_runtime_guard.py verify
```

## Linux 补丁

Linux 内核补丁按内核版本分目录维护在 `patches/linux/`，通过 `patches/linux/7.3/series.json` 登记，在 `./build.sh linux` 阶段自动应用。补丁详情见[内核与调度配置](linux/kernel.md)。
