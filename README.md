# Pico 2 W 蓝牙手柄接收器

用一块 **Raspberry Pi Pico 2 W**，将 Xbox Series、第一代 Switch Pro 或 Google Stadia 蓝牙手柄接入 Windows；另提供独立的 Switch USB 固件。每次连接一只手柄，无需 ESP32、CH582F 或外接蓝牙模块。

Bluetooth gamepad receiver for Raspberry Pi Pico 2 W: Xbox Series / Switch Pro / Stadia → USB XInput on Windows, with a separate experimental Switch USB firmware.

**[下载与版本](docs/releases.md) · [详细使用说明](pico2w/README.md) · [Switch USB](pico2w/docs/switch-usb.md) · [Pico Control](pico2w/host/modern/README.md) · [验证记录](pico2w/VALIDATION.md)**

```text
Xbox Series / Switch Pro / Stadia
              │ 蓝牙，每次一只
         Pico 2 W
              │ Micro-USB 数据线
     Windows PC（XInput R5）
          或 Switch 底座（独立 Switch USB R1）
```

## 选择固件与下载

以下为已有交付版本，本次整理没有发布新固件。直接使用时下载 UF2；需要源码和完整交付记录时选择 ZIP。

| 用途 | 当前版本与直接下载 | 说明 |
| --- | --- | --- |
| Windows 游戏 | [XInput R5 UF2](pico2w/firmware/pico2w-xinput-r5.uf2?raw=true) / [完整包](pico2w/releases/pico2w-xinput-r5-package.zip?raw=true) | XInput 输入、电量 HID、映射、死区、轴反转、震动缩放、宏和配置保存 |
| Windows 上位机 | [Pico Control V0.3 ZIP](pico2w/host/releases/PicoControl-v0.3-windows.zip?raw=true) | 查看电量与输入、测试震动、编辑 R5 配置；完整解压后启动 |
| Switch 底座 | [Switch USB R1 UF2](pico2w/firmware/pico2w-switch-r1.uf2?raw=true) / [完整包](pico2w/releases/pico2w-switch-r1-package.zip?raw=true) | 实验性有线 HID 输出，**Switch 实机识别待验证**；无震动和体感 |

Windows 和 Switch 使用两份独立固件，刷机选择 USB 输出模式。Switch R1 不提供 Pico Control 所需的配置和电量接口；编辑配置需刷回 Windows R5。下载 SHA256 与旧版本入口见[版本说明](docs/releases.md)。

## 支持的手柄

| 手柄 | 蓝牙协议 | 支持范围 |
| --- | --- | --- |
| Xbox Series X/S 原装手柄 | BLE HID | 以型号 1914 为目标，严格识别 `045E:0B13` |
| 第一代 Switch Pro 原装手柄 | Bluetooth BR/EDR HID | `057E:2009`；Windows 按键按物理位置映射为 Xbox 布局 |
| Google Stadia | BLE HID | `18D1:9400`，手柄必须已启用蓝牙模式 |

当前只连接一只手柄，三种手柄自动识别。未支持多手柄同时连接、Xbox 专有无线、Xbox 360 无线、第三方仿制手柄或 Switch 2 Pro。Windows XInput 不传输体感、NFC、音频或 Xbox 扳机马达。

## 五步开始使用 Windows

1. 准备 **Pico 2 W（RP2350，带无线）**和可传输数据的 Micro-USB 线；普通 Pico 2 没有无线，本项目不以第一代 Pico W 为构建目标。
2. 按住板上 **BOOTSEL** 接入电脑，出现 **RP2350** 磁盘后松开，将 `pico2w-xinput-r5.uf2` 拖入。写入完成后重新插拔 USB。
3. 只打开一只目标手柄并进入配对：Xbox Series 长按顶部配对键；Switch Pro 长按 USB-C 旁的 SYNC；已启用蓝牙的 Stadia 按住 **Y + Stadia**，直到橙灯闪烁。
4. Pico LED 慢闪表示等待连接，常亮表示手柄就绪。执行 `Win + R` → `joy.cpl` 检查输入。
5. 完整解压 Pico Control V0.3 ZIP，双击 `PicoControl.exe`，保留旁边的 DLL 和 `ui/`。需要 Windows 10/11 x64、.NET Framework 4.6.2+ 和 WebView2 Runtime，详见[上位机说明](pico2w/host/modern/README.md)。

正常拔插和刷入本项目 UF2 保留配对。换手柄时关闭当前手柄，等待 LED 慢闪再打开另一只。运行中短按 BOOTSEL 可在空闲时恢复扫描；**长按至少 3 秒再松开会清除接收器全部配对并重启**。

R5 配置：在 Pico Control 中选择接收器，打开“手柄配置”并读取。立即应用只改 RAM；永久保存前关闭手柄，保存后接收器重启。配置共用一档，配对区独立保留。

## 当前验证范围

| 功能 / 版本 | 已有证据 | 仍待验证 |
| --- | --- | --- |
| 三类手柄基础连接与输入、R3 自动回连 | 用户已确认正常 | 不代表后续每个新增功能均已实测 |
| R4.1 电量 HID | Windows 枚举正常，实际读取到 Switch Pro 2/4 档 | Xbox / Stadia 电量字段取决于手柄提供的服务 |
| R4.2 Switch SYNC 休眠恢复 | 软件回归与编译通过 | 实物休眠和快速唤醒复测 |
| R5 配置 / Flash 保存 | 9649 项固件软件检查通过 | 实机配置与保存验收；震动尚无专项实测结论 |
| Pico Control V0.3 | 213 项自测、WebView2 控件和消息检查通过 | 本轮界面验证未操作物理手柄 |
| Switch USB R1 | 9051 项软件检查及 Windows 模式回归通过 | Switch USB 枚举、三类手柄操作、主机休眠恢复 |

这些数字为原交付记录，详见[固件验证](pico2w/VALIDATION.md)和[上位机验证](pico2w/host/modern/VALIDATION.md)。Windows USB 输入间隔配置为 **2 ms（名义 500 Hz）**，尚无 USB 总线或端到端延迟实测结论。本次文档整理与保留文件检查见[仓库验证记录](docs/validation/2026-10-22-publication.md)。

## 源码与构建

从仓库根目录运行 Windows PowerShell；需要 Git、Python 3，主机 C 测试另需 GCC。下载工具、SDK 和构建输出留在项目本地，不纳入 Git。

```powershell
git clone https://github.com/linlinvae/pico2w-receiver.git
cd pico2w-receiver
.\pico2w\tools\setup.ps1
.\pico2w\tools\test.ps1 -Gcc gcc
.\pico2w\tools\build.ps1 -Python python
```

`build.ps1` 输出 Windows R5。Switch 使用 `test-switch.ps1` 和 `build-switch.ps1`，详见 [Switch 构建说明](pico2w/docs/switch-usb.md)。依赖版本、提交和工具哈希固定在 [dependencies.lock.json](pico2w/dependencies.lock.json)。上位机构建步骤见[开发说明](CONTRIBUTING.md)。

| 路径 | 内容 |
| --- | --- |
| [pico2w/src](pico2w/src) / [tests](pico2w/tests) / [tools](pico2w/tools) | 当前源码、模拟测试和构建脚本 |
| [pico2w/firmware](pico2w/firmware) / [releases](pico2w/releases) | UF2、BIN、ELF、构建信息与历次交付包 |
| [pico2w/host](pico2w/host) | Pico Control 源码、Windows 发行包及验证记录 |
| [pico2w/docs](pico2w/docs) | Switch 使用方式与配置协议 |
| [receiver](receiver/README.md) / [switchpro](switchpro/README.md) | 历史 CH582F/CH592、ESP32 + CH582F 方案，保留原路径及回退产物 |
| [audit](audit) / [DEVELOPMENT_CONTEXT.md](DEVELOPMENT_CONTEXT.md) | 可行性研究与早期开发背景 |

## 常见问题与反馈

- **Windows 不识别**：先换数据线/USB 口，刷机后重新插拔；记录设备管理器中的设备名称、错误码和硬件 ID。R5 开发身份为 `VID_2E8A&PID_10C8`。
- **手柄无法连接**：只打开目标手柄，确认型号及蓝牙模式；切换过其他主机后可能需要重新配对。
- **电量显示未知**：表示尚未得到有效读数，不能推断电池为空。Switch 显示 0–4 档，不伪造百分比。
- **Switch 没有识别**：确认刷入 Switch R1、使用底座 USB-A 和数据线；LED 常亮只说明蓝牙已连接。

提交 [Issue](https://github.com/linlinvae/pico2w-receiver/issues/new/choose) 时附上固件版本、板卡/手柄型号、系统版本、复现步骤和预期结果。默认不要求串口日志。参与开发前请读 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证与来源

本项目原创应用代码采用 [Apache-2.0](LICENSE)。Pico SDK、Bluepad32、BTstack、TinyUSB、CYW43、Switch HID 描述符及上位机组件各有自己的许可；归属与范围见 [NOTICE.md](NOTICE.md)、[Pico 固件 NOTICE](pico2w/NOTICE.md)、[licenses](pico2w/licenses) 和[上位机第三方声明](pico2w/host/modern/THIRD-PARTY-NOTICES.txt)。

早期 Stadia 方案基于 [O-IceDawn-O/stadia-stick](https://github.com/O-IceDawn-O/stadia-stick)，参考提交 `e0d4a701130c24648e7f189064089644584e9248`。第三方许可证、历史固件和 preservation manifests 保留原内容。
