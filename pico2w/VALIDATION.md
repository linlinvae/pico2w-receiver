# Switch USB R1 验证（2026-10-11）

- 基于 R5 新增独立 Switch 有线 HID 输出构建；共享蓝牙、配对 Flash、发现包装、严格 VID/PID 验证和回连逻辑。每次连接一只 Xbox Series、Stadia 或第一代 Switch Pro。
- 新模式 9051 项软件检查通过：8805 项 HID/映射/USB 检查，以及同一平台的 246 项回连检查（15 次普通回连、5 次 Switch 快速唤醒）。运行记录在 `build/switch-r1/switch-tests.txt`。
- HID 测试实际编译固定 TinyUSB 的 `hid_device.c`，覆盖 USB 设备/配置/HID 描述符、无 Report ID 的 8 字节输入与输出、GET_REPORT 短缓冲、SET_IDLE、错误类型/Report ID/接口/长度拒绝，以及 interrupt/EP0 输出包不误触发震动和配置。验证传输中蓝牙输入变化不修改 TinyUSB 缓冲、提交失败保留中立报告、传输失败后补发释放、USB 复位/休眠/卸载中立恢复。
- 三种输入分别验证全部标准按键/系统键/Capture 位、16 种十字键组合、摇杆极限/中心/单调方向、模拟扳机 50% 阈值和 Switch 数字扳机、断连释放，以及原配置的映射/死区/反转/宏再转换到 Switch 格式。
- 原 Windows 模式完整回归 9649 项检查通过，记录在 `build/switch-r1/regression-tests.txt`。合计 18700 次软件检查；不代表 18700 个独立硬件场景。
- ARM Release 交叉编译链接通过，应用无编译警告。CMake 提示缺少 lwIP 是既有无 Wi-Fi 构建的提示，蓝牙功能使用 BTstack/CYW43。UF2 为 RP2350 ARM Secure；保留 config `3FB000/3FC000`、bond `3FD000/3FE000` 和最后勘误扇区。准确镜像大小、SHA256、源码哈希和依赖信息见 `firmware/pico2w-switch-r1-build-info.json`。
- 六个固定依赖无跟踪修改，旧 Stadia/Switch Pro 工程 192 个哈希及 R1–R5 固件/原交付包保持一致。Switch 源码包包含许可证、使用说明和文件 MANIFEST，交付时解压核对文件及源码哈希。

**没有刷写 Pico、连接实物手柄、读取串口或在 Switch 上验证。** 用户此前确认的是旧 Windows 蓝牙接收流程；本版本 USB 枚举、三类手柄的 Switch 操作、主机休眠恢复仍待实机验收。使用基础有线 HID 兼容协议，无震动、体感、NFC、音频或主机唤醒；Switch 2 尚未验证。操作见 [Switch USB 说明](docs/switch-usb.md)。

## R5 原交付验证

# R5 配置功能交付验证（2026-10-11）

- ARM Release编译链接通过；UF2为RP2350 ARM Secure，2082应用块+1 ROM-ignore块，应用范围10000000..10082200，低于配置103FB000边界。text536920、data0、bss44472字节。
- 9649项固件检查通过：8862原映射、156 USB（含固定TinyUSB实际Feature ReportID插入/剥离、短报文与坏CRC拒绝）、59电量、7电量工具、246原平台回连、38配置保存平台场景、87 BLE发现、194配置/Flash检查。
- 新检查覆盖C配置CRC标准向量、128个字节损坏、映射/禁用/死区/反转/马达缩放、宏时间回绕/分步推进/不循环/断线休眠取消、事务token/覆盖/幂等/过期、SAVE绑定配置CRC、双银行部分编程中断恢复/写入失败/相同内容免写、配对区保留。
- 实际平台回调的保存场景：连接/配对中拒绝且不关闭无线；成功、Flash失败和关闭HCI超时维持回复，停止扫描并只重启一次；等待重启期间拒绝新连接。不原地重启HCI，避免供应商初始化序号越界。
- 六个固定依赖无跟踪修改，原工程192项及R1–R4.2交付保持原哈希。C配置互操作向量供上位机逐字节对照；V0.2共205项自测通过，界面默认/最小窗口检查见host/VALIDATION.md。
- 完整交付包解压后的90个文件逐项核对MANIFEST SHA256通过；最终固件构建记录中58个源码/工具/测试哈希与当前文件一致。上位机源码包独立解压构建并通过205项自测。
- R5 UF2 SHA256：`4B0E0AE94664D4D5CBEA9EDB208C11955FA925530015E087A4F3214B6BA9497E`。

**本轮未自动刷机、发送实物配置、删除配对或震动物理手柄。** 新协议枚举、宏/死区/映射及真实断电保存仍待用户刷入R5后手动验收。先读取默认，测试A→B和死区，再测试B触发A连按；关闭手柄后保存，等待重启、重新插拔再读；随后回归三种手柄及旧电量工具。长按BOOTSEL仍只清配对，不清配置。

## R4.2 原交付验证

# R4.2 交付验证（2026-10-10）

用户报告Switch短按顶部SYNC休眠后接收器未释放，再唤醒不能回连；用户尚未等待30秒，所以尚不能证明超过原20秒默认失联超时仍永久卡住。源码确认incoming=false会阻止供应商既有的READY旧连接替换流程。

- 只读检查当前实物为REV_0401（R4.1），电量USB/HID MI_01的ProblemCode均为0。通过已有原生HID工具真实读取到Switch Pro，中、2/4档；没有读取串口或操纵手柄。上次HID描述符修复已获实机枚举/Feature读取证据。
- R4.2使用应用链接包装，放行当前Switch相同地址的HID control接入；仍调用原Bluepad32函数，断开回调立即恢复接入许可，中断通道无需等待延迟扫描。
- 246平台检查通过，包含原三种手柄15次模拟回连、5次原供应商Classic函数执行的Switch旧槽替换及双通道接入，其他地址/PSM/非Switch拒绝、短事件拒绝、保留READY死槽清理、健康连续报告且按键不变、停止报告后的6秒清理、事件驱动模式及BLE静默保持连接、清配对保护。
- 新fixture从固定Bluepad32源文件提取Classic incoming函数原文；旧incoming=false路径直接调用该函数会拒绝同地址请求，应用包装开启例外后进入旧槽替换。无线/HCI控制器仍为模拟，并未捕获用户真实休眠链路。
- 8862映射、136 USB、59电量、7工具协议、87 BLE检查通过。电量测试也确认Switch原解析包装会转发报告到达通知。
- 固定BTstack头文件确认默认Classic监督超时20秒；GAP API设置8000*0.625ms=5秒，只作用于可设置的Classic master链路。6秒兜底只对已经观察到0x30连续模式的Switch有效，其他控制器及0x3F事件模式不使用报告静默超时。
- ARM Release编译链接通过，无warning/error，picotool识别R4.2、Pico2W、RP2350 ARM Secure。ELF包含Classic incoming包装、Switch到达通知及监督超时API，无Core1启动符号。
- UF2 2059应用块+1官方ROM-ignore块，应用范围0x10000000..0x10080B00，未触及配对区；text531112、data0、bss43704 bytes。源码/工具/测试及输出SHA256与构建记录一致。
- 六个固定依赖无跟踪修改，原工程192项及R1/R2/R3/R4/R4.1每版五项交付产物保持原哈希。
- R4.2 UF2 SHA256：`01A7F5C2843BAA7A14083392DBDDBD24B56FAA9A7B01863E38D19B12C5EE908B`。

未自动刷机、删除配对或修改Windows驱动。R4.2实际短按SYNC休眠、等慢闪后唤醒及立即唤醒需要复测。2ms USB和电量协议保留，实际500Hz仍非总线实测结论。

## R4.1 原交付验证（2026-10-10）

只读检查用户已插入的 R4 硬件：复合设备及 XInput MI_00 状态OK；电量HID MI_01报Code10 / 0xC00000B9，Windows明确报告“声明了一个非常量主项目，但是未提供相应的用法。”这是实际设备枚举证据。

- 修复 Input 主项目缺少局部 Usage；HID报告描述符27字节，接口声明长度与编译期断言一致。
- 8862 bridge、136 USB、59 battery、7工具协议、183平台和87 BLE检查通过。新增主项目局部 Usage 生命周期测试拒绝旧R4描述符，接受修正描述符及合法常量填充，拒绝截断字段。旧TinyUSB测试只检查传输，不会执行Windows HID语义解析，因此未发现此问题。
- ARM Release编译链接通过，无warning/error；text530632、data0、bss43700 bytes。构建记录与源码、工具、测试、产物逐项SHA256匹配。
- BIN中确认存在修正后的27字节报告及对应HID接口长度；UF2 2057个应用块+1个官方ROM-ignore块，应用写入范围0x10000000..0x10080900，未触及配对区。
- 六个固定依赖无跟踪修改，原工程192项及R1/R2/R3/R4每版五项产物哈希保持一致。
- R4.1 UF2 SHA256：`93B14A210A65E591D22E18AD5F6E17001868BBEABDCE027BDBC24D3607AB772E`。

未自动刷机或修改Windows驱动。修正后HID实际启动及Feature电量读取待刷入复测；现有电量工具可继续使用，2ms输入和蓝牙逻辑没有改变。

## R4 原交付验证（2026-10-10）

保留用户确认可用的 R3，新增 USB 2 ms 输入与电量查看。继续单手柄；R4 实物测试待用户执行。

- 8862 次 bridge、131 次 USB、59 次电量客户端、7 次 Windows 电量工具协议、183 次平台、87 次 BLE 发现检查通过；平台仍包含三种手柄 15 次模拟回连，BLE 发现仍含 50 次已配对广播唤醒。
- USB 测试链接固定 TinyUSB HID 实现，实际执行 HID 接口打开、Feature GET_REPORT（含 Report ID 插入）和报告描述符请求；验证 74 字节双接口配置、XUSB 2 ms IN、8 ms OUT、1 ms 不发送/2 ms 发送、忙缓存与中立报告优先。
- 电量测试执行实际应用模块，模拟 Battery Service 回调，检查启动延迟、0% 有效、100%、非法值/旧 CID 拒绝、每30秒读取、120秒过期、断开清空、失败客户端释放、五档及充电标志、短报告/其他手柄拒绝、Xbox HID 原始范围。BLE 无线与 GATT 服务端没有实物仿真。
- Windows PowerShell 5.1 编译工具的 C# 原生 HID/SetupAPI 声明并执行协议自测，无 USB 设备访问；未实际运行硬件 HID 枚举/读取。
- ARM 编译链接无 warning/error，目标 Pico 2 W / RP2350 ARM Secure，picotool 确认 R4.0。ELF 包含三个应用包装和静态 Battery Service 客户端池；没有 Core1 启动符号。
- UF2 2057 个应用块 + 1 个官方 ROM-ignore 块，写入范围 0x10000000..0x10080900，配对 Flash 区没有触及；text 530640、data 0、bss 43700 bytes。
- UF2 1053696 bytes，BIN 526544 bytes；源码/工具/测试和产物 SHA256 与构建记录逐项一致。
- 六个固定源码依赖无跟踪修改，原工程192个哈希通过，R1/R2/R3 各5个固件及交付产物哈希通过。
- R4 UF2 SHA256：`C3C7614AB12D6242A34D4A9F7F4371324C7E8CFCA4232E454EFAFC37FF24B06E`。

未自动烧录、读取串口或开启双手柄。Windows 新 PID 10C8 的复合接口枚举、XInput 自动绑定、三种手柄实际电量、回连及 USB 总线实际500Hz需要上板验证。USB设置为2ms不等于蓝牙端到端延迟为2ms，也不是已实测500Hz。

## R3 原交付验证（2026-10-10）

用户确认 R2 的 Switch Pro、XSX 自动回连正常；Stadia 关闭再开机时接收器 LED 一直慢闪，插拔接收器后才恢复。R3 针对 BLE 广播发现修复，用户后续已确认 Stadia 自动回连成功。

- 目标 `pico2_w` / `rp2350-arm-s`，Cortex-M33，Release，Pico SDK 2.2.0，ARM GNU 14.2.Rel1。构建链接通过，构建日志无编译器 warning/error。
- 8862 次 bridge、102 次 USB、183 次平台生命周期检查通过；平台检查确认主动扫描配置、BLE 就绪后缓存刷新和清配对时缓存清除，原三种手柄 15 次模拟重连仍通过。
- 87 次 BLE 广播检查通过，包括 50 次已配对地址的空/定向广播唤醒、地址类型匹配、稀疏 LE 数据库、主动扫描响应中的 Stadia 名称、普通 HID、发现门控、畸形包拒收、最大长度与原始缓冲区不修改。
- BLE 回放使用固定 Bluepad32 发现函数及其三个辅助函数的原文，未重写原解析逻辑。原函数拒绝已配对地址的空定向广播，而 R3 应用包装后发起连接。这是广播发现层验证；没有模拟完整配对加密/GATT/HID 链路，也没有捕获用户设备的真实广播。
- ARM ELF 中同时保留原发现函数和应用包装；反汇编确认 `uni_bt_packet_handler` 的广播入口转到应用包装，包装继续调用原函数。
- 六个固定依赖源码无跟踪文件修改；原工程 192 个哈希通过；R1、R2 的 UF2/BIN/ELF/构建记录/ZIP 各五个产物哈希通过。
- picotool 识别版本 R3.0，Pico 2 W，RP2350 ARM Secure；UF2 包含 2035 个连续应用块、1 个官方 ROM-ignore 块，应用范围 `0x10000000..0x1007F300`，未触及配对区，链接 Flash 上限检查通过。
- text 524960 bytes，data 0，bss 43432；BIN 520864 bytes，UF2 1042432 bytes。
- R3 UF2 SHA256：`07DABCEC600A04D03192E2F8F2B4A94453691BB79B1FC7CED25652DA381BFA76`。

未自动烧录或读取串口；R3 未清除用户配对记录。优先用原记录测试 Stadia 开机连接，然后保持接收器插入，关机、等接收器 LED 慢闪、正常开机，重复 5 次。再检查 Xbox/Switch 回连和三种手柄切换。

## R2 原交付验证记录（2026-10-10）

用户实测反馈：R1 的 XSX、Switch Pro、Stadia 连接和输入正常；三个手柄关闭再开机均不能自动回连，插拔接收器后恢复。R2 针对共用连接恢复流程修复。后续用户确认 Switch Pro、XSX 回连正常，Stadia 仍失败；上方 R3 继续处理。

- 目标 `pico2_w` / `rp2350-arm-s`，Cortex-M33，Release，Pico SDK 2.2.0，ARM GNU 14.2.Rel1。构建链接通过，构建日志无编译器 warning/error。
- 8862 次 bridge、102 次 USB 检查通过；181 次平台连接生命周期检查通过，包括三种手柄共 15 次模拟断开重连。
- 新测试使用固定 Bluepad32 / BTstack 的真实头文件、真实 `platform.c` 与模拟无线状态，检查异步取消后搜索重试、实际扫描与软件状态失同步、配对中的设备槽、漏掉断线回调、失效 HCI 连接、保持按住的输入、BOOTSEL 操作。提取保留的 R1 平台源码运行同一测试，失败于断线回调内立即扫描的检查；R2 通过。
- 六个固定依赖源码无跟踪文件修改；原 Stadia / Switch Pro 工程 192 个哈希通过，R1 UF2/BIN/ELF/构建记录和 ZIP 五个产物哈希通过。
- picotool 识别名称为 Pico 2 W XInput Receiver，版本 R2.0，Pico 2 W，RP2350 ARM Secure。
- UF2 包含 2030 个连续应用块、1 个官方 ROM-ignore 块，应用范围 `0x10000000..0x1007EE00`，配对存储区未触及，链接 Flash 上限检查通过。
- text 523704 bytes，data 0，bss 43316；BIN 519608 bytes，UF2 1039872 bytes。
- R2 UF2 SHA256：`7D865C13E75F2CE420B8224EA8FDE0EFC3E45997A102001E3F4E3CE194F4F6A1`。

未自动烧录或读取串口。实测请保持接收器插入，每种手柄关机后等 LED 慢闪，再正常开机，重复 5 次；随后测试三种手柄交替连接。保存配对记录不因本次 UF2 更新而清除。没有实物日志，代码中的恢复缺口及模拟测试不能证明它们就是用户设备上唯一的触发原因。

## R1 原交付验证记录（2026-10-10）

- 目标：`pico2_w` / `rp2350-arm-s`，Cortex-M33，Release。
- ARM GNU 14.2.Rel1 编译链接通过；应用及最终重编译无编译器警告。SDK 配置阶段提示未初始化 lwIP；R1 不启用 Wi-Fi，Bluetooth 构建支持已启用。
- Bluepad32、BTstack、Pico SDK、TinyUSB、CYW43、mbedTLS 六个固定源码提交校验通过，供应商源码无跟踪文件修改。
- 8862 次 bridge 检查通过：轴端点/方向/单调性、模拟和数字扳机、按钮位置、支持身份与错误协议拒绝、断线释放、连接正常无新输入时保留按住状态、停止优先、USB 休眠/复位后的震动清除。
- 102 次 USB 检查通过：真实 TinyUSB 头文件下的应用类驱动、描述符与 OS Compatible ID、GET_REPORT/SET_REPORT、短包拒收、IN 忙时不覆盖缓存、中立报告优先、失败重试、OUT 重新挂接、挂起/恢复/复位。
- `setup.ps1` 与 `build.ps1` 在当前 Windows 工作区执行成功；编译工具安装在本项目中。
- 官方 picotool 识别 UF2 为 RP2350 ARM Secure，名称/版本/板型正确。
- UF2 含 2028 个连续应用块和 1 个官方 ROM-ignore 块；应用写入范围 `0x10000000..0x1007EC00`，未触及配对 Flash banks。链接阶段也检查 Flash 上限。
- 静态段：text 523104 bytes，data 0，bss 43300；BIN 519008 bytes，UF2 1038848 bytes。
- UF2 SHA256：`C837ACFD15A004F5A57F00949EEEA9F3CCB3F2E6263F557E2F400B068358388C`。
- 原 Stadia / Switch Pro 工程 192 个文件哈希与保留清单一致；原 receiver 的 121 个文件同时符合之前建立的清单。

**R1 交付时的待验证项（历史记录）**：Pico 2 W 上三种手柄的配对/重连、Windows XInput 自动绑定、真实按钮与摇杆、模拟扳机、双马达及停止、运行中 BOOTSEL 长按清配对、USB 拔插/休眠和实际蓝牙断线释放时间。用户后续确认三种手柄连接和输入正常，并报告本次处理的回连失败。未自动烧录用户硬件，未读取串口。

主机检查通过表示逻辑与协议布局符合这些测试，不能代替真实射频、手柄固件及 Windows 驱动验证。首次建议仅打开 XSX 手柄测试，完成后再换 Switch Pro 与 Stadia。
