> 当前更正（2026-10-07）：用户确认上游固件在同一CH582F板/同一Stadia上连接成功。优先排查修改版回归，R7停止推荐；当前R8恢复SDK内部32K配置，待实测。见[回归对照](upstream-regression.md)及[CH582调试](ch582-debug.md)。下文为历史记录；第一～三版的CH592固件日志不能用于证明CH582硬件故障，R4–R6外部时钟检查也不能推出整板无法运行BLE。

# 扫描失败：2026-10-07 实物日志与下一步

用户的 Windows 列表显示 `StadiaZ6JZ-2e23`。当前解析器接受 `Stadia` 前缀；已用这个完整名称和 Scan Response/Shortened Name 广播字段增加测试，均通过。列表中的名称本身不能证明手柄此刻仍处于配对广播状态。

## 已取得的实物证据

系统枚举确认接收器 CDC 为 COM12，VID/PID 为 `2E8A:10C6`；同设备也有 USB HID 手柄接口。实际串口日志保存在 `build/hardware-logs/first-scan.txt`：

```text
[T+0.220] [INF] [BLE] Init done, press BOOT to scan
[T+5.471] [INF] [BTN] BOOT: start scan
[T+5.471] [INF] [BLE] Scanning for Stadia...
[T+6.973] [INF] [BLE] Scanning for Stadia...
[T+8.474] [INF] [BLE] Scanning for Stadia...
[T+9.519] [INF] [BTN] BOOT: stop scan
[T+9.519] [INF] [BLE] Scan cancelled
```

因此：USB 枚举、CDC、BLE 初始化、按键触发和扫描 API 接受请求已在本板看到。此次约 4 秒扫描中没有 `Found Stadia`。旧 INFO 日志不显示广播数量/原始数据，不能据此区分“未收到广播”和“收到但不符合名称过滤”。第二次按扫描键确实取消扫描，等待过程中不要反复按键。

## 诊断固件

当前第三版诊断文件为 `firmware/CH592-dev-scan-diag/stick-scan-diag-r3.bin` 或对应 `.hex`（当前 `stick.bin`/`stick.hex` 也为相同第三版）。构建命令：

```powershell
& .\tools\build.ps1 -Variant scan-diag
```

保留原扫描/连接行为和 PB22 按键定义，不自动扫描。启动日志包含 `ScanDiag=1 Button=B mask=0x00400000 active-low=1` 和 `ScanDiagRev=3`。每轮扫描记录广播数、Scan Response 数、名称字段数、扩展/定向/其他 GAP 事件数量及 SDK 结果数/状态；为控制开销，仅展开该轮前 4 个广播的地址、RSSI、类型、原始数据和名称。这是排障版本，不用于测延迟。

1. 在 WCHISPTool 按已成功使用的 USB ISP 流程烧入该诊断文件，重启。
2. 确认运行固件后的实际 CDC 端口；本次枚举是 COM12，换 USB 插口后可能改变。
3. 项目根目录执行下面命令，或在任意串口工具中以 115200、8N1 打开 CDC 并开启 DTR。脚本不需要 Python/pyserial：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\receiver\tools\capture-log.ps1 -Port COM12 -Seconds 45
```

4. 读取期间先按 RST，让手柄保持配对模式的橙灯闪烁，然后只短按扫描键一次，等待至少 10 秒。暂时不要在 Windows 点击连接手柄。
5. 保存 `[SCAN]` 与 `[BLE]` 行。串口在日志读取期间由脚本占用；该次读取结束后才用其他工具打开。

## 如何解读

| 日志 | 含义与后续方向 |
|---|---|
| `ScanDiag=0`，或没有此启动行 | 未运行新诊断固件/没有捕获启动日志，先核对烧录文件并按 RST |
| 没有 `BOOT: start scan` | 先核对按键、BLE 就绪状态；本次旧固件实测按键有效 |
| `reports=0` | 第二/三版中应用本轮没有收到普通/扩展/定向广播事件；结合 sdk-devs、status 和 other 检查无线接收与 SDK 事件路径 |
| `reports>0 names=0` | 收到广播但没有名称字段；看 `scan-rsp` 和原始数据，定位主动扫描响应 |
| 有其他设备名称，没有 Stadia | 手柄配对状态、已有连接或接收条件待核对 |
| `name=StadiaZ6JZ-2e23` / `Found Stadia` | 名称识别已成功，继续看连接结果及错误码 |
| `Connected` 后没有 `HID notifications enabled` | 转为服务发现/CCCD 排障 |

广播名称可能不在展开的前 4 个包中，因此缺少具体 `name=` 行不能单独证明整个扫描未收到名称；结合每轮总数判断。`reports=0` 也不能单凭一轮认定硬件损坏。

## 第一版诊断的实物结果与第二版修改

用户已烧入第一版，COM12 实物日志确认 `ScanDiag=1`；日志保存为 `build/hardware-logs/scan-diagnostic.txt`。第 12～41 轮均显示 `reports=0 scan-rsp=0 names=0 found=0`。用户确认手柄橙灯持续闪烁。捕获启动缓冲后日志时间从 T+0.938 跳到 T+25.490，因此不能把该文件当作完整连续的上电日志。

第一版只统计 `GAP_DEVICE_INFO_EVENT`，所以 0 计数不能排除 SDK 发出了扩展或定向广播事件。对照固定 SDK 的官方 Central 示例，发现它同时处理 `GAP_EXT_ADV_DEVICE_INFO_EVENT` 和 `GAP_DIRECT_DEVICE_INFO_EVENT`；本地程序原先遗漏这两种事件。第二版共用普通/扩展的名称解析，正确复制不同布局中的地址/数据字段，仅识别完整扩展数据；定向广播没有名称，只计数，不猜测目标。分片扩展广播重组尚未实现，第二版会计数和记录原始包，但跳过不完整/截断的数据匹配。

第二版另外记录扫描间隔/窗口、最大结果参数读取状态、每轮 SDK 设备数和完成状态及未处理的 GAP 事件。没有改变扫描参数来猜测修复，也没有修改 SDK 文件。目前这只是已证实遗漏的修补和诊断增强，尚不能断言它是本次实物故障的根因。

第二版已通过标准版 71 项主机检查、诊断配置的 29 项 BLE 检查及两种配置的交叉编译；它尚待烧入实物验证。镜像 173600 字节，SHA-256 `24873FA99F74E54B122E8C0B5574F2A0F64817647888238E5F26186B08D1B776`。第二版已上板；结果与第三版说明见下文。

## 第二版实测与第三版时钟配置

用户已烧入第二版，`build/hardware-logs/scan-diagnostic-r2.txt` 确认 `ScanDiagRev=2`。本次捕获中有 RST 引起的端口重新枚举，重开 COM12 后拿到连续日志：T+5.183 扫描键触发，RST 后第 1～21 轮 `reports/ext/direct/other/sdk-devs` 均为 0，完成 `status=0x00`。实际配置 interval/window=16，max-results=0 的参数读取成功；0 是 SDK 头文件标注的无限结果默认值，没有证据要求改为 10。名称解析尚未执行，补扩展事件没有解决本轮问题。

进一步核对固定 SDK 发现：官方 Central `.cproject` 明确 `CLK_OSC32K=0`，CONFIG.h 注释要求包含主机角色使用外部 32K；此前本地构建未定义该宏，HAL 使用默认内部 32000 Hz。用户确认卖家资料标明板上有 32.768 kHz 晶振；这仍不是起振的实物测量。

第三版将 `BOARD_LF_CLOCK_SOURCE=0` 定义在 board_config.h，构建强制预包含该文件，使 SDK HAL、外设驱动、应用的 CLK_OSC32K/CAB_LSIFQ 一致。构建元数据记录有效时钟选项与频率。先启动外部振荡器并最多等待 1 秒检测 32 次时钟电平变化；失败则保留 USB 日志并停止 BLE 初始化，避免进入 SDK RTC_InitTime 的无限等待。成功进入 HAL 后测量 100 ms RTC 计数，正常应约 3277；另记录 BLEB/BLEL 中断使能和 VTF3 入口。

静态检查也确认 SDK 的 BB_DevInit 会将 BB_IRQLibFunction 注册到 PFIC VTF3，因此 ELF 中的弱 BB_IRQHandler 不能单独当作中断入口缺失；未盲目增加 BB 中断包装。

第三版文件 `stick-scan-diag-r3.bin`，170804 字节，SHA-256 `C7AF66E2C0DEE2F25CA6EC81F564CFA1286F0F4836736B804720E9DC3A120A64`。标准开发板、诊断开发板、自制板分支均编译通过，主机检查 71 项及诊断 BLE 29 项通过；第三版尚待用户烧录。源码/构建检查证明时钟配置已统一，不能代替起振、无线扫描和手柄连接的实测。下一次烧录先确认 ScanDiagRev=3、RTC source=0 及 RTC ticks/100ms，再看是否收到广播。

自制接收器也应保留外部 32.768 kHz 晶振及相应负载电路。内部时钟可通过板级宏选择做对照实验，但不作为当前 Central 的默认工作配置。

## CH582F 型号更正与 R4

用户确认实际板为 CH582F。此前 CH592 程序能够出现 CDC/扫描计时不代表无线底层兼容；先前 0 广播计数在错误目标固件条件下取得，因此撤回据此对本板正常无线接收、时钟根因的推断。此前外部晶振配置与扩展事件遗漏仍是源码中已查实的问题，但不能称其为本次板子的故障原因。

R4 已使用 CH582 SDK/库/启动代码构建，兼容 CH582 的 PFIC 字段和 RTC API，加入 `R8_CHIP_ID` 检查（预期 0x82）、CH582 启动名称，并通过对应 SDK 的 GetMACAddress 生成序列标识。CH582 标准/诊断均编译通过，两套 SDK 主机状态测试均通过。文件 `firmware/CH582-dev-scan-diag/stick-CH582F-scan-diag-r4.bin`，171200 字节，SHA256 9F1B8C9684FE04D3B7319106D4405A31AEBC797FE2802D6C64EE37F17E04585C。下一步只使用此正确目标固件重新取得串口和广播证据。

## CH582 R4 实物连接失败及 R5

正确目标日志 scan-CH582-r4.txt 证实芯片0x82、外部32K启动与RTC运行、广播接收、完整Stadia名称识别。首轮23广播/8响应/5名称，发现后SDK取消扫描status=0x30（bleGAPUserCanceled），随后连接事件成功，但约0.31秒后0x3E断链。用户按指示近距离重试（目标RSSI=-32dBm），第二次约1.68秒后同码断链，未确认HID订阅。由此排障从“扫描不到”转为“连接未能保持”。

R5仅调整本机地址为HAL的公共出厂MAC、检查初始化/地址配置错误路径，并增加本机/目标地址、连接参数、MTU与有限GATT日志。原有将公共MAC标成随机静态地址不是官方Central初始化方式；是否导致本轮问题尚未确定。R5 `firmware/CH582-dev-scan-diag/stick-CH582F-scan-diag-r5.bin`，171900字节，SHA256 4DBCD1ADBE4FD9BE2D3CC4619C7B607BE3E6AF12C0C99475C7B1E2FADF571EAA。保持其它参数，继续对照实物结果。

标准原因码解释见 Bluetooth SIG [Controller Error Codes](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/architecture%2C-mixing%2C-and-conventions/controller-error-codes.html)。

## CH582 R5 实物结果

日志 build/hardware-logs/scan-CH582-r5.txt 已确认 Rev5、ChipID82、本机公共地址 D00C5E2676D5、初始化成功。目标Stadia地址D8E004292E23、公共地址类型、RSSI=-26dBm；首轮18广播、6响应、4名称。连接事件实际interval20（25ms）、latency0、timeout100（1s），MTU请求API返回成功。约0.321秒后0x3E断链；随后MTU响应method03、status14（SDK bleNotConnected）在空闲态收到。没有完成MTU/HID交互，因此公共地址修改未解决本轮建链失败；后续先排除其他已配对主机抢连，再做参数/时序对照，不能直接认定CCCD或芯片硬件坏。

## CH582 R6 待上板

用户已确认关闭其他设备蓝牙。R6恢复官方Central初始化连接间隔范围25–125ms（原为固定25ms），保留其它连接参数，记录请求参数并增加活动期间每约1秒的RTC/系统毫秒对照。CH582标准/诊断编译及两SDK180次模拟检查通过。镜像stick-CH582F-scan-diag-r6.bin为172232字节，SHA256 685F1A42C2ADE03B0CD42734F149215E8D5830DFF230C2E092AA8C0843BF2A96。尚待读取硬件日志，不能把参数调整当作已证实的修复。

## CH582 R6 时钟检查失败、R7 待上板

R6日志scan-CH582-r6.txt、scan-CH582-r6-COM13.txt、scan-CH582-r6-powercycle.txt确认启动版本6、芯片82。COM12消失后检测到COM13；两个启动RTC样本2650、468ticks/100ms，随后两次启动外部32K检查失败，BLE停止，未完成参数对照。用户确认仅USB连接，没有外设、杜邦线或面包板。时钟启动设置/外围仍需区分，不能直接定性硬件故障。

R7保持负载电容配置，短时SDK200%驱动起振后恢复100%，以三次连续约100ms计数验证明显异常而非晶振精度；启动总等待约4秒有界。增加复位原因、CK32K/XT32K寄存器日志。CH582标准/诊断和CH592诊断编译无警告，尚未上板。172948字节，SHA256 5C8BD4666F61FE7ED36427BF34F1C22A57BE47898B9DAC4DC4CC075CE2F4A5D3。
