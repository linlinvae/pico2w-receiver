# Pico 2 W 蓝牙手柄接收器 — Windows R5 / Switch USB R1

首次使用请先看[仓库首页](../README.md)的固件选择、五步刷机和验证范围；[下载与版本](../docs/releases.md)提供直接下载、SHA256 与回退入口。下文保留各版本的详细技术说明。

**新增 Switch USB 接收器 R1**：三种蓝牙手柄每次连接一只，通过 Pico 板载 USB 向 Switch 底座输出一个有线 HID 手柄。刷入 `firmware/pico2w-switch-r1.uf2`；连接、按键映射、功能限制与实机验收见 [Switch USB 使用说明](docs/switch-usb.md)。这是独立 USB 输出版本，旧 R5 Windows 固件与 V0.2 上位机交付包保持原样。Switch R1 软件测试与编译通过，Switch 实机识别尚未验证；不提供震动和体感。

R5 支持标准按键重映射、左右轴向死区、轴反转、震动缩放、单触发键宏，以及断电保留配置。当前上位机为 Pico Control V0.3（shadcn/ui界面）。先刷 `firmware/pico2w-xinput-r5.uf2` 并重新插入 USB，再完整解压 `host/releases/PicoControl-v0.3-windows.zip` 并运行 `PicoControl.exe` →“手柄配置”→“读取接收器”。已经使用R5无需为界面升级再刷机。立即应用只改RAM；永久保存前关闭手柄，保存后接收器自动重启。配置共用一档，不改配对区。新功能已构建及模拟测试，尚待实物验收。下文保留既有R4.2连接基线说明。

独立实验固件：使用 Pico 2 W 接收一只蓝牙手柄，通过板载 Micro-USB 向 Windows 输出一个 wired XUSB / XInput 控制器。无需 CH582F、ESP32 或外接模块。`receiver/` 和 `switchpro/` 的原固件保留。

**用户已确认三种手柄连接、输入和 R3 自动回连正常。R4 在此基础上增加 2 ms USB 输入与电脑电量查看，继续只连接一只手柄；R4 实物反馈手柄已连接，但电量接口无法启动。R4.1 修正 HID 描述符；R4.2 继续修复 Switch 顶部 SYNC 休眠后的快速唤醒恢复，当前R4.1电量HID已实际读取成功（Switch 2/4档）；R4.2新休眠流程仍待复测。** R1、R2、R3、R4、R4.1 固件及交付包原样保留。震动、清配对等功能尚无专项实测结论。

R4 的 USB 接口 0 输出 XInput，接口 1 是使用系统 HID 驱动的只读电量状态接口。开发 PID 改为 `10C8`（R3 是 `10C7`），避免 Windows 沿用旧单接口设备的驱动缓存；配对区不变。无需 Wi-Fi、串口日志或第三方驱动。当前用户电脑已确认复合设备和 XInput 驱动正常启动；R4 的电量 HID 接口因描述符缺少 Usage 报代码10，R4.1 补充该字段并同步报告描述符长度。刷入 R4.2 后重新插入，再继续使用现有电量工具；无需清除配对。

R4.2 将 Classic master 链路监督超时从 BTstack 默认20秒设为5秒。连接中的同一只 Switch 发送 HID control 接入请求时，临时允许其进入供应商既有的 READY 旧连接替换流程；其他手柄/地址仍被单连接门控拒绝。断开回调立即恢复接入许可，扫描仍延迟100ms，避免快速到来的 HID interrupt 请求被拒绝。

观察到 Switch 0x30 连续报告后，即使按键值不变，每个有效包都更新到达时间。连续6秒没有有效0x30/0x21报告时释放旧输入、电量和设备槽，并恢复搜索；事件驱动0x3F模式、未观察到连续报告的Switch、Xbox和Stadia不使用这个报告静默超时。不是根据按键多久没变化判断休眠。

R2 将恢复扫描推迟到断线清理之后；空闲时每 2 秒确认允许连接并重新请求 BLE 扫描、重试经典蓝牙搜索，绕过“扫描已开启”软件标志造成的空返回；检查已失效的设备槽并释放旧输入。配对或正常连接期间不执行空闲扫描修复，不按输入报告的静默时间判定断线。详见 `CHANGELOG.md`。

R3 保留 R2 的恢复流程，改用主动 BLE 扫描。原 Bluepad32 每个广播包都要求包含 HID Appearance，缺失时即使地址已有配对记录也会忽略；R3 对已有 BLE 配对地址的可连接广播，以及名称以 Stadia 开头的广播/扫描响应，补充内部发现元数据，再继续原来的加密、GATT 身份读取和严格 VID/PID/解析器验证。其他控制器的正常发现流程仍由原库处理。配对记录只读，不增加 Flash 写入；不修改供应商源码。

## 兼容基线

| 手柄 | 无线协议 | 接受范围 | 输入与震动 |
|---|---|---|---|
| Xbox Series X/S 原装手柄 | BLE HID | Microsoft VID `045E`、PID `0B13`，Xbox One/Series 解析器；以型号 1914 为目标 | 按键、双摇杆、独立模拟 LT/RT、双马达 |
| 第一代 Switch Pro 原装手柄 | Bluetooth BR/EDR HID | Nintendo `057E:2009`，Switch Pro 解析器 | 按键、双摇杆、数字 ZL/ZR、双通道震动转换 |
| Google Stadia | BLE HID | Google `18D1:9400`，已启用蓝牙模式 | 按键、双摇杆、独立模拟 LT/RT、双马达 |

XSX 以 Bluepad32 文档中的 BLE 固件基线为依据，用户手柄的实际固件版本尚未检查。固件严格限制上述身份，未加入 Xbox 专有无线、Xbox 360 无线、第三方仿制手柄或 Switch 2 Pro。

一次连接一只手柄。三种协议在同一固件内自动识别，无需选择模式。按键按位置映射为 Xbox：Switch 的 B→A、A→B、Y→X、X→Y；Minus→Back、Plus→Start、Home→Guide。Capture/Share/Assistant 暂不映射。XInput 传输不包含陀螺仪、NFC、音频或 Xbox 扳机马达；目标是 Windows PC。

## 刷机

1. 使用带数据传输功能的 Micro-USB 线。按住板上 **BOOTSEL**，插到电脑，再松开。
2. 打开出现的 **RP2350** 磁盘，将 `firmware/pico2w-xinput-r5.uf2` 拖入。磁盘消失、开发板重启，再重新插拔 USB 使 Windows 更新 HID 描述符。
3. 启动后板载 LED 慢闪表示等待连接，常亮表示手柄已就绪。没有手柄时电脑仍能看到一个输入为零的 USB 控制器。
4. 让一只目标手柄进入配对状态，其他手柄先关机。XSX：长按顶部配对键到 Xbox 灯快闪；Switch Pro：长按 USB-C 旁的 SYNC；Stadia：在已启用蓝牙模式的手柄上按住 Y + Stadia 到橙灯闪烁。
5. 连接后执行 `Win + R` → `joy.cpl`，再在游戏或手柄测试页检查输入。

第一次配对需要进入手柄配对模式。接收器保存经典蓝牙与 BLE 密钥，正常拔插和再次刷本项目的 UF2 不删除它们。手柄切到其他主机后可能需要再次进入配对模式；这也取决于手柄自身保存的配对记录。

切换类型：关闭当前手柄，等待 LED 恢复慢闪，再打开或配对另一只。单连接版本会连接先就绪的手柄，测试时只打开目标手柄。

运行中短按 BOOTSEL：未连接时重新启动扫描。**运行中长按 BOOTSEL 至少 3 秒，然后松开：停止无线、清除本接收器保存的所有配对记录、自动重启。** 长按时 LED 快闪；需要松开才重启，以免进入 ROM 刷机模式。插电时按住 BOOTSEL 的作用仍然是进入刷机磁盘。

## 实物验证顺序

**R4.2 验证**：刷入后重新插入接收器，先打开 `joy.cpl` 确认 XInput 输入，再双击 `tools/battery-monitor.cmd` 查看电量。依次连接三种手柄，确认工具显示当前类型，断开后恢复“未连接”。接收器保持插入，重复三种手柄关机/开机回连。2 ms 是配置目标，实际 USB 周期需 USB 总线抓包验证；XInputGetState 的数据变化频率不能直接代表 USB 上报率。

Switch 休眠复测：连接后短按顶部 SYNC，使手柄休眠；先等待接收器 LED 慢闪（目标约5–6秒），再按普通按键/Home唤醒，重复5次；随后测试短按休眠后立即唤醒。不要长按 SYNC 进入重新配对，也不要长按接收器 BOOTSEL。

先测 XSX，再测 Switch Pro、Stadia；每种都检查：配对、A/B/X/Y 与十字键、双摇杆方向和中心、LT/RT、震动停止、按住按键时关闭手柄后的释放、接收器拔插后的重新连接。传统 `joy.cpl` 可能把 LT/RT 合并为 Z 轴，可用下面的 XInput 测试脚本分别检查。

```powershell
python .\pico2w\tools\test_windows.py --index 0 --seconds 30
# 用户主动执行此命令才测试震动；先确认 index 对应本接收器。
python .\pico2w\tools\test_windows.py --index 0 --seconds 20 --rumble
```

脚本列出 0–3 四个 XInput 槽位，输出独立扳机及摇杆数值；带 `--rumble` 时分别震动 0.7 秒并停止。脚本不读取串口。蓝牙断线释放以协议栈报告断链为准，受连接监督超时影响，不承诺固定毫秒数。Xbox/Stadia 和事件驱动模式没有新报告时保留按住状态；Switch 已观察到的连续报告模式使用上述6秒失联兜底。

如果 Windows 不识别为 Xbox 兼容设备，先记录设备管理器中的设备名称、错误码和硬件 ID `VID_2E8A&PID_10C8`。这是沿用本项目的开发身份；固件提供 `MSFT100` / `XUSB10` 兼容描述符，实际自动绑定仍需 Windows 实测。

## Windows 上位机 V0.3

最新界面发行包为 `host/releases/PicoControl-v0.3-windows.zip`，采用 React + shadcn/ui 与 WebView2；保留现有电量、输入、震动、R5 配置及保存功能。需要 x64 Windows、.NET Framework 4.6.2+ 和 WebView2 Runtime；本机具备运行环境。完整解压 `host/releases/PicoControl-v0.3-windows.zip` 后启动，保留 DLL 和 ui 文件夹。213 项程序自测、实际 WebView2 正常/最小窗口各16项控件与消息检查通过，本轮未操作物理手柄。说明见 [host/modern/README.md](host/modern/README.md)。

### V0.2 原说明

独立图形程序位于 `host/dist-v0.2/PicoControl.exe`，双击即可运行，不需要 Python、管理员权限或额外驱动。包含概览、电量、按键/摇杆/扳机与震动测试，以及 R5 的按键映射、死区、宏、配置保存。旧 R4.1/R4.2 可继续使用原有查看/测试功能；设置功能需要 R5。旧 V0.1 程序和包保持原样。

输入/震动测试先选择 XInput 槽位后按键确认，槽位不能自动视为选定 Pico。配置写入只针对顶部选择的接收器，独立于槽位。详细用法见 [host/README.md](host/README.md)，验证范围见 [host/VALIDATION.md](host/VALIDATION.md)，协议与 Flash 布局见 [docs/config-protocol.md](docs/config-protocol.md)。

## 电量查看

双击 `tools/battery-monitor.cmd`，在 Windows PowerShell 5.1 中运行图形工具，无需 Python、管理员权限或驱动替换。程序每秒查询接收器 HID Feature 状态，BLE 电量首次查询在手柄就绪约 1 秒后启动，之后每 30 秒读取，也接受通知。无有效值或读数超过 120 秒时显示“未知”，断开/切换清除旧值。电量查询失败不会主动断开手柄。

| 手柄 | 显示方式 |
|---|---|
| Switch Pro | 输入报告的 0–4 五档：空、低、中、较高、满；有充电标志时显示充电中，不伪造精确百分比 |
| Google Stadia | BLE Battery Service `180F` / Battery Level `2A19`，有效值 0–100% |
| XSX | 优先 BLE 电池服务的百分比；不提供该服务时，如 HID 电量字段可用，则显示原始强度及描述符范围；不会把原始值误当百分比 |

Xbox 手柄固件/电池类型可能影响字段是否存在。工具显示未知时，表示本次没有取得有效电量，不能据此推断电池为空。Windows 自带 XInput 电池 API 不会自动读取这个新增 HID 状态接口。

状态协议：Report ID `1`，共 32 字节。去掉 ID 后的 31 字节 payload：0–2=`P2W`，3=协议版本1，4=手柄类型（0无/1XSX/2Switch/3Stadia），5=有效，6=来源（1BAS/2Switch/3Xbox HID），7=百分比或255未知，8=Switch档位或255，9=充电标志或255，10=未知原因（0未连接/1查询中/2不可用或错误/3无效或过期），11=USB输入配置2ms，12–15=读数年龄毫秒LE，16–18=HID原始值/逻辑最小/最大，其余保留0。Report1保持只读；R5新增Report2/3配置命令和回复，电量工具不发送配置。

## 构建与测试

所有下载、编译工具和构建产物均位于 `pico2w/`，不做全局安装，不自动烧录。Windows 需要 Git、Python 3，主机单元测试另需 GCC。

```powershell
.\pico2w\tools\setup.ps1
.\pico2w\tools\test.ps1 -Gcc gcc
.\pico2w\tools\build.ps1 -Python python
```

`dependencies.lock.json` 固定 SDK、Bluepad32、BTstack 和下载工具版本/哈希。当前 `build.ps1` 在构建前检查依赖无修改、旧工程及 R1–R4.2 保留产物哈希，成功后生成 Windows R5 UF2、BIN、ELF 和 `firmware/pico2w-xinput-r5-build-info.json`。Switch 输出使用独立 `build-switch.ps1`，详见 [Switch USB 说明](docs/switch-usb.md)。`resources/`、`build/` 不属于交付源码，可通过 setup 重建。

独立解压到不包含旧工程的目录也可构建；此时旧文件保留审计不适用。当前工作区的旧工程已经执行完整哈希校验。

工程采用 Cortex-M33、单核前台 polling run loop。Bluetooth 与 `tud_task()` 在同一上下文处理，USB 中断仅交给 TinyUSB 入队；应用不使用动态分配。Pico SDK 的 `arch_none` 默认是后台中断上下文，本工程在应用 CMake 中替换 Bluepad32 的该链接依赖，改为 polling。BTstack 新版本将 `hids_client.c` 改名为 `hids_host.c`，应用 CMake 适配源文件列表，未修改供应商源码。

R3 在应用链接时包装 `uni_bt_le_on_gap_event_advertising_report`，通过 `ble_discovery.c` 在必要时补充发现元数据。主机广播测试提取固定供应商文件中的发现函数及三个辅助函数原文进行编译；没有重写其解析逻辑。测试证明原函数拒绝缺少 Appearance 的已配对广播，而应用包装后会发起连接；未捕获用户真实广播；用户后续已确认 R3 回连成功。

USB 输入周期 2 ms（名义 500 Hz），输出端点 8 ms；这些是固件/USB 调度参数，不是实测端到端延迟。8 字节 XUSB 震动命令反向传给 Bluepad32。主机仅更新一次的持续震动会由固件每约 100 ms 刷新 250 ms 有限时长命令；零震动优先处理，USB 复位/休眠/卸载与手柄断线会清除震动状态。后续控制器不会继承旧震动。

Flash `0x3FD000..0x3FEFFF` 保留给 BTstack 的两个 4 KiB TLV bank；最后 4 KiB 保留给 RP2350 芯片勘误处理。链接断言和 UF2 检查阻止应用覆盖配对区。UF2 包含官方 picotool 生成的 ROM-ignore 兼容块；校验脚本单独检查该块。

## 来源与许可证

蓝牙实现使用 [Bluepad32](https://github.com/ricardoquesada/bluepad32)、[BTstack](https://github.com/bluekitchen/btstack)，板级/USB 使用 [Pico SDK 2.2.0](https://github.com/raspberrypi/pico-sdk/releases/tag/2.2.0) 与 TinyUSB。控制器资料见 [Bluepad32 支持列表](https://bluepad32.readthedocs.io/en/latest/supported_gamepads/)；[BetterJoy](https://github.com/davidobot/BetterJoy) 作为之前讨论的 Switch 协议参考，本工程未引入其 PC 驱动代码。USB 布局依据 [Microsoft MS-XUSBI](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-xusbi/) 和原接收器已经工作的协议实现。

本工程原创应用代码采用 Apache-2.0。BOOTSEL 采样改编自 Raspberry Pi 的 BSD-3-Clause 示例。`licenses/` 包含相关许可证，`NOTICE.md` 记录归属；Pico 2 W 上的 BTstack/CYW43 使用范围以附带 Raspberry Pi 许可证为准。
