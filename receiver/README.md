# Stadia 接收器：CH582F 开发版

基于 [O-IceDawn-O/stadia-stick](https://github.com/O-IceDawn-O/stadia-stick)，上游提交为 `e0d4a701130c24648e7f189064089644584e9248`。这是本地修改版本，沿用 Apache-2.0 许可证，版权见 `LICENSE`；原始源码保留在 `../reference/stadia-stick/`。

目标：先在已确认型号的 CH582F USB-C 开发板验证单个 Stadia 的输入与双马达震动，再将同一套协议和 BLE 代码移植到自制接收器（CH582F，或后续明确选定的 CH592F）。

## 已完成的优化

- 断连后排队发送按键释放、摇杆居中、扳机归零报告；USB 忙时保留重试，新连接输入等待中立报告发送。
- 震动缓存最新幅度并锁存停止指令，非停止输出最快每 10 ms 一次。失败退避为 10–500 ms，新指令不清掉失败计数；连续 50 次失败后断连，已记住的手柄可自动重连。
- 仅接受 ERM（type 2）震动；能力响应不再声明玩家灯、RGB 和背键。
- BOOT 重扫等待断连事件；扫描取消后忽略迟到的结束事件；立即失败的扫描/连接请求返回可重试状态。
- 校验广播字段与特征条目长度，重置电池读取状态，释放被拒绝的 CCCD 写入缓冲。HID 订阅未完成时不发送震动；必要 HID 发现失败会断连，电池发现超时可跳过。
- USB 输出使用背压与主循环快照，保护停止和功能查询指令；配置、复位和恢复状态参与发送判断。
- GPIO/DC/DC 集中配置，取消全 GPIO 初始化。栈预留 2 KB，链接时检查静态 RAM 与栈重叠。
- R10 启用 WCH 原生绑定密钥保存；HID 订阅成功后另存手柄地址，上电和掉线后自动回连。地址记录采用双页、CRC、提交标记和写后校验，同一手柄不重复擦写。
- R11 增加独立的 Windows XInput 实验版，使用 PC 有线 XUSB 1.00、标准按键/摇杆、独立扳机及双马达输出。保留 R10 BLE/绑定/回连；Windows 实际驱动加载及网页映射待烧录确认，见 [XInput 烧录说明](docs/getting-started/xinput.md)。
- 短按扫描键在释放时扫描／重连，扫描中短按暂停；运行时长按满 3 秒清除保存信息并进入新手柄配对扫描。

## 开发板固件

**用户已更正：手中的开发板是 CH582F。此前 CH592 固件及扫描日志不能作为这块板的正常 BLE 验证依据。**

当前使用 **`firmware/CH582-dev/stick-CH582F-bond-reconnect-r10.bin`** 或对应 `.hex`，使用步骤见 [保存绑定与回连](docs/getting-started/bond-reconnect.md)。R9 已获用户确认可用，并保留原诊断镜像用于回退。R10 沿用内部 32K、固定 25 ms 连接间隔和 R9 服务发现修复。固件读取实际芯片 ID，不匹配时停止 BLE 初始化；CH592 镜像不能烧入 CH582F。

CH582 使用沁恒 CH583 系列 SDK（也覆盖 CH582）的 CH58x 蓝牙/ISP 库、驱动和 startup_CH583；CH592 使用独立 CH59x SDK/库和调度汇编。芯片之间不可混刷这些应用镜像。更换芯片不需要重做 Stadia 协议和 SInput 逻辑，但必须选择匹配的底层库/启动代码。

R10 主机检查覆盖断电中断写入恢复、清除记录、计时回绕、已保存地址匹配、退避回连、取消连接和清除期间迟到回调。首次配对允许协议栈默认 30 秒的安全协商窗口，再恢复 R9 的 10 秒发现空闲保护。编译和模拟检查不能代替实际绑定、断电回连、输入及震动测试。历史修复见 [回归对照](docs/getting-started/upstream-regression.md)。

| 配置 | 默认值 | 限制 |
|---|---|---|
| 芯片 | CH582F | 用户更正的实际开发板型号；启动读取 ID 再确认 |
| USB | 原生 USB CDC + SInput HID | 使用数据线 |
| 扫描键 | PB22，低电平有效 | 沿用上游，此前日志显示 PB22 触发有效，正确固件仍需复验 |
| 状态 LED | 禁用 | 接线与极性未知，以 CDC 日志看状态 |
| 内部 DC/DC | 禁用 | 外围电路未确认，先使用 LDO 路径 |
| BLE/RTC 低频时钟 | 校准的内部 32K | 沿用已可用固件；外部晶振为显式板级选项 |

**烧录前核对扫描键是否连接 PB22。** 如果该板接 PB23，可这样重新编译：

```powershell
& .\tools\build.ps1 -BoardDefines BOARD_BUTTON_PIN=GPIO_Pin_23
```

按键丝印/照片不能替代原理图或实物通断检查。ISP 启动脚由芯片决定，修改应用按键宏不会修改 ISP 启动方式。LED 不闪不代表固件没有运行。

## 构建与测试

本目录内已经准备官方 SDK 和编译器，无需完整 IDE 或 GNU Make。进入本目录执行：

```powershell
& .\tools\test.ps1       # PATH 中需要主机 gcc
& .\tools\build.ps1      # 默认 CH582F 开发板
& .\tools\build.ps1 -UsbMode xinput # 单独生成 Windows XInput 实验版
```

换电脑后，安装 Git，运行 `tools/setup.ps1 -Chip CH582` 下载 `dependencies.lock.json` 固定提交的沁恒 SDK 和 Windows 编译器。脚本拒绝覆盖有本地修改的依赖目录；依赖各自的许可保留在下载内容中。已有 MounRiver 编译器可指定：

```powershell
& .\tools\build.ps1 -ToolchainBin 'C:\实际安装目录\RISC-V Embedded GCC\bin'
```

当前默认 CH582。明确使用 CH592 板时执行 `tools/setup.ps1 -Chip CH592`、`tools/build.ps1 -Chip CH592`；`tools/test.ps1 -Chip all` 检查两种 SDK 配置。CH582 SDK 固定提交 `bd508ad7ceed48377619837051412a651952857f`；全部依赖见 dependencies.lock.json。

烧录与实测见 [CH582 调试步骤](docs/getting-started/ch582-debug.md)。其余 `docs` 是上游历史资料，未全部同步本地修改，以本 README 和新调试步骤为准。

## 自制接收器移植

优先保持与已验证开发板相同的芯片型号；目前是 CH582F。后续采用 CH592F 需切换 SDK/固件重新验证。板级选项在 `src/board_config.h`，包含 LED/按键端口、引脚、极性、DC/DC 和低频时钟选择。`-Board custom` 输出到单独的 `firmware/CH582-custom/`（默认 CH582）。

例如，**原理图确认 LED 在 PA8 且低电平点亮后**：

```powershell
& .\tools\build.ps1 -Board custom -BoardDefines BOARD_LED_ENABLED=1,BOARD_LED_PIN=GPIO_Pin_8,BOARD_LED_PORT_A=1,BOARD_LED_ACTIVE_LOW=1
```

同芯片下协议和 BLE 代码可复用；换 MCU/USB 控制器/BLE 栈仍需修改平台与驱动。USB D+/D−、电源、时钟、启动脚、复位和天线必须按自制板原理图核对。自制板应保留外部 32.768 kHz 晶振及负载电路。先保留 BOOT、RST、调试焊盘，通过开发板验收后再缩小硬件。

## 本阶段边界

- 默认 USB 为 **SInput**；可单独烧录 **R11 XInput 实验版**，两者使用不同开发 PID。R10 为已验证回连的回退版本，每款游戏/网站的识别和震动回传需实测。
- R10 已实现绑定保存和自动回连，实际断电保持和重新加密仍需上板确认。保存地址与保存配对密钥是两个不同层次，详见 [回连说明](docs/getting-started/bond-reconnect.md)。
- Assistant 键仍未映射；其余上游映射保留。
- HID CCCD 仍沿用上游“输入句柄 + 1”假设；若订阅失败，记录真实 GATT 结构再改描述符发现。
- 无线距离、连接参数、时延、丢包、长时震动、USB 休眠恢复待测。10 ms 输出节流与 1 ms USB 轮询均不等于实际无线延迟。

协议核对：[SDL 官方 SInput 驱动](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_sinput.c)、[SInput 官方说明](https://docs.handheldlegend.com/s/sinput/doc/sinput-hid-protocol-dev-SNSaEw36nc)、[沁恒 CH592 SDK](https://github.com/openwch/ch592)。
