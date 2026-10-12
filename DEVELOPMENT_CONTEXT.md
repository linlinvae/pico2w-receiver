# Stadia 手柄项目：前期对话背景

## 来源与读取状态

- 用户提供的 Gemini 分享链接：https://share.gemini.google/6CPXdCccbO2m
- 跳转后的分享页：https://gemini.google.com/share/1e5d6191b3a7
- 标题：谷歌Stadia游戏手柄解析。
- 已通过浏览器读取页面上的全部对话，包含页面末尾的“我使用的是有线模式”及其回复。
- 本笔记保存需求背景与讨论线索，不代表对 Gemini 技术回答的独立核验。

## 用户明确提供的信息

1. 关注 Google Stadia 游戏手柄及其内部芯片。
2. 关注官方蓝牙模式转换、蓝牙连接下没有震动的问题。
3. 询问自行编译支持蓝牙震动的固件并烧入原厂主板是否可行。
4. 关注碳膜、霍尔、TMR 摇杆方案，进一步询问 TMR 摇杆品牌。
5. 实际使用问题：手柄硬件正常，进入游戏后无法操作。
6. 最后明确纠正连接方式：实际使用的是 **USB 有线模式**。用户于 2026-10-06 确认 USB 有线问题已经解决；当前关注点转为蓝牙震动的可行方案。
7. 尚未明确提出具体软件功能、开发语言、界面、发布形式或硬件改装实施要求。

## 对话讨论顺序

1. Stadia 手柄资料：外观与按键、USB-C、耳机接口、Wi-Fi、蓝牙转换及使用限制。
2. 芯片问题：先出现“扫码芯片”的输入，随后改为询问具体芯片。
3. Gemini 列举主控、无线、音频、存储和电源管理芯片及职责。
4. 官方蓝牙转换准备、引导模式组合键、下载与安装固件、配对与重置操作。
5. 蓝牙震动原因及 Steam Input、Stadia-to-XInput、ViGEmBus 等主机端映射方案。
6. 自定义固件的可行性，涉及签名、安全启动、调试接口、闭源驱动，以及换主板改装路线。
7. 碳膜、霍尔和 TMR 摇杆对比。
8. K-Silver、Favor Union、GuliKit、Ginfull 等 TMR 品牌比较。
9. 游戏内无法操作的排查，涉及输入协议、Steam Input、虚拟 Xbox 控制器、设备占用及手柄顺位。
10. 用户明确使用有线模式后，Gemini 给出 USB 识别、joy.cpl 输入检查、协议转换及冲突排查建议。

## 可供后续开发调查的线索

- 先识别实际设备、连接方式、固件版本、目标操作系统及游戏，再判断兼容性问题。
- 若目标是 Windows 游戏兼容工具，可调查实体 Stadia HID 输入到虚拟 Xbox/XInput 控制器的映射。
- 输入映射与震动输出应分别验证：游戏能收到按键，不等于原厂手柄能接收震动命令。
- 可调查设备发现、HID 描述符与报告、按键和轴映射、热插拔、震动反馈、实体与虚拟设备重复输入等问题。
- Steam Input、Stadia-to-XInput、XOutput、StadiEm、reWASD、ViGEmBus、HidHide 等仅是候选调查线索；使用前需核实适用性、维护状态和许可。
- TMR 改装与固件研究属于此前的兴趣方向，目前不能视为已决定的实现路线。

## 必须独立核实的 Gemini 说法

以下均是分享对话中的说法，不能直接成为实现依据：

- 芯片型号清单（包括 LPC55S69、BCM43458、CS47L35、W25Q128JVPQ、BQ25601）、频率和外围连接。
- 安全启动签名强制执行、SWD/JTAG 熔断及“自定义固件完全不可行”的绝对结论。
- 蓝牙震动的实际支持情况、HID 输出报告格式，以及 Windows 蓝牙协议栈的行为。
- 将 HID、DirectInput 和 XInput 简单等同或混用的描述，以及“非 Steam 游戏完全不识别 DirectInput”的概括。
- Steam Input 或第三方映射工具是否能在指定连接方式下真正驱动 Stadia 马达。
- WebHID/Gamepad 测试网页必然独占设备、USB 音频设备导致游戏枚举故障等解释。
- 官方转换工具的当前可用性、截止日期、组合键、配对与重置步骤，以及转换后音频功能。
- TMR 品牌排名、精度、功耗、寿命、免焊支持与 Stadia 电气/机械兼容性。

核验应优先依据官方资料、实际拆解、相关开源项目源码、HID 描述符及真实设备测试。

## 后续实施前需要补充的信息

- 目标系统及版本；具体游戏、平台与启动方式。
- 手柄是否已转换蓝牙固件；USB VID/PID、设备名称与 HID 描述符。
- joy.cpl 或其他输入测试中各按键、轴、扳机是否正常。
- USB 震动是否可复现，以及测试工具和步骤。
- 希望开发的是诊断工具、输入映射软件、震动兼容工具、固件研究项目还是硬件改装项目。

这些是未来实施所需信息；当前任务仅是阅读并理解背景，尚未开始开发或改变设备配置。

## 2026-10-06：蓝牙震动方案调查

- Linux 内核已有 Stadia 震动驱动，匹配 USB 和 Bluetooth；SDL 源码也实现了报告 ID 0x05 与两个 16 位马达强度通道。不能将蓝牙无震动归因于原厂固件根本没有震动功能。
  - https://github.com/torvalds/linux/blob/master/drivers/hid/hid-google-stadiaff.c
  - https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_stadia.c
- Windows 输出路径限制：WinStadia 作者明确记录蓝牙 HID over GATT 输出报告被系统驱动以无效参数拒绝；StadiaEmu 也报告用户态 HID/GATT/WinRT 尝试受阻。输入转 XInput 本身不能保证蓝牙震动。
  - https://github.com/aisk/WinStadia
  - https://github.com/lukasfuscic19/StadiaEmu
- 优先候选：ESP32-S3 外置 BLE 接收器，通过 USB 模拟 Xbox 360 控制器，转发游戏震动；保留原厂手柄及蓝牙固件。需带原生 USB OTG 的板卡，实际兼容性、延迟、重连和震动稳定性仍需真机验证。
  - https://github.com/Scalee/stadia-dongle
  - https://github.com/jazz-zzzz/stadia-dongle-esp （分支 README 提供中文配置界面、双手柄和附加按键映射）
- 软件实验候选：Stadia-X 将 USB 蓝牙适配器交给 WSL2/Linux，再把输入与震动桥接到 Windows；增加 WSL、USB 直通、蓝牙设备管理和虚拟手柄依赖，适合作为验证原型。
  - https://github.com/offvault/Stadia-X
- 另一候选：Linux 设备通过网络桥接至 Windows，已有输入/震动双向桥接项目。
  - https://github.com/jocxfin/stadia-w-rumble-windows
- 上述社区项目仅完成资料调查，未在用户设备上安装、刷写或实测。开发方向尚未由用户选定。

## 2026-10-06：用户提供的更低成本项目

- 项目：https://github.com/O-IceDawn-O/stadia-stick
- 本地参考源码：`reference/stadia-stick/`，提交 `e0d4a701130c24648e7f189064089644584e9248`，保持未修改。
- CH582F/CH592F 通过 BLE 连接一个 Stadia，并输出 USB SInput。不是原生 XInput；作者只报告 Windows 11 部分 Steam 游戏成功。
- 作者历史成交价：CH582F ¥8.8，优惠后 ¥6.8；CH592F ¥11。当前市场价与优惠未独立验证。
- 主机 GCC 运行上游两组协议测试，16 项检查通过；未交叉编译或进行硬件实测。
- 源码中的蓝牙输入及物理震动转发链路完整，适合作为低成本原型。主要限制/改进点：手动配对，无持久化绑定；断连未给主机发送中立输入；Assistant 丢映射；能力掩码存在多报；震动错误恢复和类型校验需改进。
- 本项目使用无回执 GATT 写入且作者报告可用，提示此前“手柄必须采用有回执写入”的社区解释不可一概而论。
- 完整评估保存在 `STADIA_STICK_FEASIBILITY.md`。用户尚未要求修改或烧录该项目。

## 2026-10-07：CH592F 开发板实施

- 用户已购入 CH592F 开发板，明确要求先优化程序、用开发板调试，后续再移植到自制接收器。
- 新开发目录为 `receiver/`，原始 `reference/stadia-stick/` 保持无修改。保留上游 Apache-2.0 许可证与来源。
- 已修断连输入释放、震动停止优先级与错误退避、USB 输出缓存并发、扫描/断连状态、电池/CCCD 错误路径、错误能力声明；板级引脚/极性/DC/DC 集中配置。
- 默认 CH592F、PB22 扫描键低有效、LED 与内部 DC/DC 关闭。照片不能确认按键实际连 PB22/PB23、LED 接线或 DC/DC 外围，首次烧录前需核对。
- 官方 CH592 SDK 与沁恒 Windows RISC-V 编译器已放入项目依赖目录并固定提交，完整固件交叉编译通过。5 组主机测试共 62 项检查通过，BLE/USB 使用模拟调用或寄存器。
- 已生成 `receiver/firmware/CH592-dev/stick.bin`、hex、elf、map、build-info.json；调试说明见 `receiver/docs/getting-started/ch592-debug.md`，验证记录见 `receiver/VALIDATION.md`。
- 尚未烧录或完成真机测试。仍为单手柄 SInput、手动扫描，无持久化绑定；Assistant 与 CCCD 描述符发现暂未扩展。硬件/游戏性能不能从主机测试推断。

### 同日：用户反馈扫描不到手柄

- Windows 显示名称 `StadiaZ6JZ-2e23`，完整名称符合现有 `Stadia` 前缀过滤。
- 通过系统枚举确认本板 HID/CDC，CDC 为 COM12。已用 `receiver/tools/capture-log.ps1` 直接读取实物日志并保存为 `receiver/build/hardware-logs/first-scan.txt`。
- 日志确认 BLE 初始化就绪、PB22 配置的扫描键有效、扫描 API 成功启动并按 1.5 秒重复；第二次按键取消扫描。约 4 秒扫描未看到 `Found Stadia`，旧 INFO 日志没有广播统计，因此不能确定无线接收还是名称过滤失败。
- 新增独立扫描诊断版 `receiver/firmware/CH592-dev-scan-diag/stick.bin`，记录各轮广播/Scan Response/名称数量及前 4 个包数据，保留手动扫描。已构建，尚待用户通过 USB ISP 烧录；不能远程代替按下物理启动键。
- 主机测试新增真实名称、Shortened Name 和空指针边界，总计 65 项检查通过；另在诊断配置下重复 BLE 的 21 项检查通过。标准版与诊断版均交叉编译成功。
- 下一步：用户烧入诊断版后再读取 COM12，判断 `reports=0`、无扫描响应、名称过滤、连接失败或服务发现失败。操作与解释见 `receiver/docs/getting-started/scan-diagnosis.md`。

### 同日：第一版扫描诊断实测与第二版

- 用户已烧入第一版诊断固件，COM12 日志确认 ScanDiag=1；第12～41轮普通广播 reports/scan-rsp/names 均为0，见 receiver/build/hardware-logs/scan-diagnostic.txt。用户确认手柄橙灯持续闪烁。日志有启动缓冲和时间跳跃，非完整连续上电捕获。
- 检查官方固定 SDK 示例发现本地遗漏扩展与定向广播事件；第一版0计数不能证明所有RF接收为0。已补普通/扩展共用名称解析，不完整/截断扩展数据跳过名称匹配，定向广播只计数不猜测手柄身份；新增配置、SDK设备数/完成状态、其他事件日志。
- 第二版 ScanDiagRev=2，具体文件 receiver/firmware/CH592-dev-scan-diag/stick-scan-diag-r2.bin，173600字节，SHA256 24873FA99F74E54B122E8C0B5574F2A0F64817647888238E5F26186B08D1B776。当前canonical stick.bin相同。标准/诊断交叉编译均通过，71项主机检查和诊断BLE29项通过；第二版未上板，遗漏是否根因未确认。
- 下一步用户烧入r2后重新捕获COM12，观察ext/direct/other/sdk-devs/status，再决定RF/时钟/协议栈路径排障。

### 同日：第二版实测与外部时钟配置修正

- 用户烧录r2后捕获COM12，ScanDiagRev=2；RST后第1～21轮 reports/ext/direct/other/sdk-devs 全0、status=00。日志 receiver/build/hardware-logs/scan-diagnostic-r2.txt。interval/window16、max-results0读取成功；扩展事件补齐未解决该轮故障。
- 固定SDK Central .cproject要求CLK_OSC32K=0，CONFIG.h说明Central外部32K；此前构建未指定，HAL默认内部32000Hz。用户确认卖家资料标明开发板有32.768kHz晶振，未实测起振。
- r3把BOARD_LF_CLOCK_SOURCE=0放board_config，build/test state强制预包含，HAL/驱动/应用宏一致；启动最多1秒检测外部时钟变化，失败保留CDC停止BLE，成功HAL后测100msRTC计数和BLE/VTF状态。没有增加BB包装：SDK反汇编已证明BB_DevInit注册PFIC VTF3。
- 标准/诊断/custom交叉编译通过，71+29主机检查通过。r3文件 receiver/firmware/CH592-dev-scan-diag/stick-scan-diag-r3.bin，170804字节，SHA256 C7AF66E2C0DEE2F25CA6EC81F564CFA1286F0F4836736B804720E9DC3A120A64。起振与扫描结果仍需用户烧录后读取日志验证，不能称硬件故障已解决。自制板保留外部32.768kHz晶振。

### 同日：用户更正开发板为 CH582F

- 当前实际开发板用户确认CH582F，此信息覆盖此前CH592F假设。既有实物日志运行CH592镜像，不能作为正常CH582 BLE验证；不能据此认定RF坏、时钟根因或扩展事件根因。下一步用正确固件重新验收。
- 已固定下载官方openwch/ch583 SDK bd508ad7ceed48377619837051412a651952857f（EVT v2.4、支持CH582），setup/build/test/Make默认CH582，CH592保留显式选项；SDK未修改。
- CH582 R4正确使用CH58x库、libISP583、驱动和startup_CH583，不用CH592调度汇编。修复PFIC/RTC诊断字段兼容，Board=CH582F-dev、ScanDiagRev=4，R8_CHIP_ID预期82，不匹配停止BLE并保留CDC。序列标识用对应SDK GetMACAddress替代硬编码ROM地址。
- CH582标准/诊断、CH592诊断交叉编译通过；共享32项+每芯片27BLE/12USB/29诊断BLE，两套共168次主机检查通过，非RF模拟。
- 文件receiver/firmware/CH582-dev-scan-diag/stick-CH582F-scan-diag-r4.bin，171200字节，SHA256 9F1B8C9684FE04D3B7319106D4405A31AEBC797FE2802D6C64EE37F17E04585C。用户尚未烧录R4。WCHISP选择CH582核对实际识别型号，再读CDC（端口可能因MAC序列更新改变），确认ChipID82和时钟/扫描日志。
- 当前操作文档receiver/docs/getting-started/ch582-debug.md、README；历史ch592-debug/scan-diagnosis/VALIDATION已明确标注型号更正。

### 同日：CH582 R4 实测扫描成功、连接0x3E，准备R5

- 用户烧R4，COM12日志receiver/build/hardware-logs/scan-CH582-r4.txt。ChipID82匹配，外部32K起振检查通过、RTC短测3244ticks/100ms（非精度结论）。首次RST后扫描23广播8响应5名称、识别StadiaZ6JZ-2e23；Connectedhandle1后0.31秒0x3E断。按建议近距离重试RSSI-32、第二次1.68秒仍0x3E，无HID服务发现/订阅确认。
- SDKscan完成30为bleGAPUserCanceled，找到后cancel预期；不可混淆HCI30。启动IRQ=0在Role启动前取得不能认定中断坏。0x3E标准为底层连接未能建立。
- 检查本地原先把公共出厂MAC标成随机静态地址，非官方Central方式。R5改GAP_ConfigDeviceAddr(PUBLIC,NULL)，检查init/addrstatus再ready，补本机/目标addrRSSI、连接interval/latency/timeout、MTUstatus、前8个非通知GATT消息；保持其它参数。是否解决断链待实物验证。
- R5文件receiver/firmware/CH582-dev-scan-diag/stick-CH582F-scan-diag-r5.bin，171900bytes，SHA256 4DBCD1ADBE4FD9BE2D3CC4619C7B607BE3E6AF12C0C99475C7B1E2FADF571EAA。180次两SDK模拟检查通过，CH582标准/诊断、CH592诊断交叉编译成功。用户待烧R5。

### 同日：R5实物仍0x3E

- 已捕获receiver/build/hardware-logs/scan-CH582-r5.txt：Rev5、ChipID82、Own public D00C5E2676D5；Stadia public D8E004292E23 RSSI-26。18广播6响应4名称，连接interval20/latency0/timeout100，MTUrequest返回00。0.321秒后0x3E断，之后RxMTUrspmethod03/status14(bleNotConnected)。公共地址调整本轮未解决建链。
- 已向用户异步询问是否关闭Windows/手机等其他配对设备蓝牙，待答；建议先同一R5排除抢连再重试，不要仅凭0x3E断定硬件或CCCD。R5日志45秒已结束、COM12释放。

### 同日：用户排除其他设备蓝牙，R6待烧录

- 用户确认已关闭其他设备蓝牙，只有开发板连接。无需重复询问该项。
- R6修改初始化连接间隔范围为官方Central的min20/max100（25–125ms），latency0/timeout100不变；增加扫描请求参数及主循环TMOS处理后每约1秒运行时RTC/系统毫秒日志，仅诊断版启用。板级诊断版本6。
- CH582标准/诊断交叉编译无警告，两SDK180次模拟检查通过。文件receiver/firmware/CH582-dev-scan-diag/stick-CH582F-scan-diag-r6.bin及hex，172232字节，SHA256 685F1A42C2ADE03B0CD42734F149215E8D5830DFF230C2E092AA8C0843BF2A96；尚待实物验证，不能声明连接修复。
- 当前无串口读取任务运行；COM12可用于用户烧录后的45秒采集。下一步用户烧录R6后，读取scan-CH582-r6.txt，指导保持橙灯/30厘米、RST后单次扫描，再比较连接实际参数/断链时间/时钟连续性。

### 同日：R6烧录后外部32K检查失败，R7待烧录

- 用户已烧录R6。采集receiver/build/hardware-logs/scan-CH582-r6.txt（COM12）、scan-CH582-r6-COM13.txt、scan-CH582-r6-powercycle.txt。读到Rev6/ID82，两个启动RTC样本为2650、468ticks/100ms；随后两次启动External32Knotrunning停止BLE。未扫描，未验证连接间隔修改。不能再把R4一次短启动样本解释为一直稳定。
- COM12消失后Get-PnpDevice确认当前CDC为COM13（VID2E8A/PID10C6，端口状态OK）；以后先用COM13核对。最后采集已结束，无串口会话运行。
- 用户确认没有PA10/11外接线、外设、面包板，仅USB。其他设备蓝牙也关闭；无需重复问。请继续区分启动配置与硬件，不断言晶振坏。
- R7实现SDK LSECFG_Current(200)启动至少250ms/至多3秒，恢复100%电流后最多1秒检查3次连续约100ms RTC增量在理论值±10%；负载电容不改，有界失败保留CDC。增加Reset cause/CK32K/XT32K日志，保留R6连接范围。±10%只检查明显停顿/错频，不保证BLE ppm精度。
- CH582标准/诊断、CH592诊断交叉编译无警告；源码/镜像哈希匹配，SDK/reference未修改。R7镜像receiver/firmware/CH582-dev-scan-diag/stick-CH582F-scan-diag-r7.bin及hex，172948字节，SHA256 5C8BD4666F61FE7ED36427BF34F1C22A57BE47898B9DAC4DC4CC075CE2F4A5D3。晶振启动流程不在已有主机状态机模拟覆盖内，待硬件验证。
- 下一步用户烧R7后开COM13采集，建议断电10秒重插，单次扫描；先看3次Externalcheckstable=1/2/3，再看HAL后RTC、Runningms和实际连接。若仍timeout，可用XT32K/Reset日志与板实物晶振标识进一步定位。不要要求烧BLE_ROM，当前为静态链接BLE库。

### 同日重要纠偏：用户同板同手柄上游固件连接成功，R7撤回，R8回归时钟

- 用户明确回答：原项目固件已在这块CH582F和同一只Stadia上实测连接成功。不要再优先怀疑整板或要求证明硬件可用。助手已说明此前过早转向晶振排障，暂停R7，已向用户致歉。没有R7上板日志。
- 上游reference提交e0d4...的Makefile没有CLK_OSC32K覆盖，当前固定SDK CONFIG.h默认1；本地R3–R7强制0是关键配置变化。源码+本地SDK推断默认内部32K，用户成功二进制的具体SDK/编译配置尚未独立核对，不能声称唯一根因已确定或R8等同上游。
- R8默认BOARD_LF_CLOCK_SOURCE=1，应用/HAL/驱动一致，build-info source1/32000Hz；撤回R7起振电流调整和HAL后的100ms诊断等待。可选显式外部路径仅保留原有限起振检查。默认内部不执行Externalcheck，不要求外部晶振。
- 保留USB缓存/断线中立报告/震动队列优化；现有public地址检查、25–125ms连接范围、LED/DC/DC关闭仍保留，因此R8不是全部回退到上游。差异表receiver/docs/getting-started/upstream-regression.md。若仍失败继续基于上游可连接binary缩小地址/电源等差异，避免叠加猜测。
- R8两SDK180次检查通过；CH582标准/诊断、CH592诊断、CH582 custom显式外部clock0编译通过；SDK内部时钟分支原有x32Kpw未使用警告，应用无新增警告。SDK/reference未改，源码/镜像hash相符。
- R8文件receiver/firmware/CH582-dev-scan-diag/stick-CH582F-scan-diag-r8.bin及hex，174380bytes，SHA256 49AB347260A1424CBF50AF5DB4A561019B2C875829A814482B9632AEEC00E1CA。待用户烧录/实测，不能声称优化版连接已修复。
- 当前无串口任务。下轮用户烧R8后先确认当前CDC（最近COM13），采集R8日志；启动应Rev8/source1/Chip82，单次扫描至HIDnotificationsenabled，再测输入/震动。R7无需继续烧录。

### 同日：R8接通后固定3秒门限主动断链，R9修复发现时序

- 用户烧R8后说连接马上断开。采集COM13到receiver/build/hardware-logs/scan-CH582-r8.txt，会话22035已结束并释放串口。
- 关键证据：Rev8/source1/ID82；T56.173连handle1，interval61=76.25ms/timeout100；T57.178发HID发现，T57.633MTUrsp才到（method03/status00）；T59.173日志HIDdiscoveryincomplete主动Terminate，T59.235reason16。明确本轮不是3E，不能再归为RF/晶振不稳。初始Discovery API返回未记录，拒绝/blePending为代码推断而非观测。
- R9取消main连接后绝对3秒deadline；BLE模块按匹配的发现响应/成功初始请求更新progress，10秒无进展保护。延迟MTU时每50ms重新安排初始发现；SUCCESS才进DISC_SVC，blePending延迟重试，其它初始错误断开并日志。健康订阅+可选电池流程完成关闭deadline。恢复上游固定25ms interval min=max20，其它时钟/public地址/USB/震动优化保留。
- main改调StadiaBLE_CheckDiscoveryTimeout每轮。测试新增17场景：慢MTU、busy请求重试、超过3秒仍完成全HID订阅与输入、真实无进展终止、无关通知/错误不能续期、计时回绕、断开清理。两SDK248次通过（32+2*(BLE47+USB12+诊断BLE49)），receiver/build/validation/r9-tests.txt。
- CH582标准/诊断和CH592诊断编译通过，应用无新增警告；SDK内部分支x32Kpw未使用警告保留。标准ld曾返回5无更多诊断（两次）；手动no-relax成功，后正常链接也成功，不能认定根因。build.ps1加入一次关闭relaxation的失败重试并记录linkerRelaxation，持续失败仍报错。SDK/reference无修改。
- R9文件receiver/firmware/CH582-dev-scan-diag/stick-CH582F-scan-diag-r9.bin及hex，174948bytes，SHA256 582D77F32AF4D1C8057B100C04ECBB010408FE5AFD83BF6C1C3E076D5D72C4DB。源码/hash相符。尚待实物HID保持/输入/震动验证。
- 下一步用户烧R9后COM13采集（以实际设备端口为准），应Rev9/source1/linkinterval20；单次扫描等待至少15秒，确认MTU后HID发现→CCCD→HIDnotificationsenabled，能保持后测试输入/震动。不要再执行R7外部时钟测试。

### 2026-10-07：R11 Windows XInput 实验版，待烧录

- 用户希望电脑识别为 Xbox 模式，以改善测试网站识别及映射。已实现独立 -UsbMode xinput（默认 sinput 保留），原创协议代码依据微软 MS-XUSBI v20240916；原生 WCH USB、无新协议栈或动态分配。PC wired XUSB 1.00，不提供 Xbox 主机认证/音频/扳机马达。
- XUSB 接口0，CDC接口1/2，游戏手柄EP3，32字节最大包；MSFT100 OS字符串及XUSB10兼容ID，独立开发PID10C7（SInput10C6），MAC产生序列号。20字节输入，Xbox标准按钮/16-bit数值摇杆（原始精度仍8-bit）/独立8-bit扳机，Y正向上；8字节双马达输出，保留停止优先及退避。每4ms空闲刷新缓存；待发中立/新输入优先。Capture/Assistant无映射。
- R10 BLE/绑定/自动回连/DataFlash地址不变；用户明确不再读取串口日志，未打开串口。R11真实Windows驱动绑定、浏览器standard映射和震动还未验证，不能声称已被Windows识别。首次烧录若ISP清了DataFlash需重新配对。
- 主机594项检查通过 receiver/build/validation/r11-tests.txt；CH582 SInput、CH582 XInput、CH592 XInput交叉编译通过。SDK原有x32Kpw警告保留，应用无新增警告；SDK与reference Git状态干净。OS描述符/控制请求多包/CDC/rumble缓存/休眠/复位/4ms刷新均模拟验证。ISR不调用LOG/BLE。
- 交付 receiver/firmware/CH582-dev-xinput/stick-CH582F-xinput-r11.bin、hex与-build-info.json；bin176356 bytes，SHA256 E8DD394771C16DC26BA1F07E2CA0208E5FBDEFB9B14B5B8EB7E45E5930F5CA6A。源码与metadata hash一致。R10回退bin原hash B9BE13F53C824FCD536BD359A4ABF81C14B0E5A19669A6936A62B994C45A7676已校验未改。
- 使用说明 receiver/docs/getting-started/xinput.md。下一步用户烧录R11、拔插USB、Win+R joy.cpl检查Xbox兼容控制器，再Chrome/Edge刷新测试网页并按A；若无法识别可反馈设备管理器驱动名称/错误码，无需串口日志。DirectInput传统面板合并LT/RT不代表固件缺少独立扳机。
