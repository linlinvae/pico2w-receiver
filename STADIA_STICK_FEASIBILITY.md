# stadia-stick 可行性评估

评估日期：2026-10-06。

项目：https://github.com/O-IceDawn-O/stadia-stick

已检出的 main 提交：`e0d4a701130c24648e7f189064089644584e9248`。

源码保存在 `reference/stadia-stick/`，未修改上游文件。主机测试产物位于 `audit/stadia-stick/`。

## 结论

CH582F/CH592F 单手柄 BLE → USB SInput 方案具有工程可行性，值得用作低成本验证原型。它采用与 ESP32 接收器相同的总体思路：由独立 MCU 接管蓝牙与震动下发，Windows 只与 USB 接收器通信。

主要取舍是 USB 协议、重连体验和扩展空间。当前实现输出 SInput，而非原生 Xbox/XInput；Windows 11 部分 Steam 游戏的成功经验不能扩大成所有 Windows 游戏的即插即用承诺。

推荐顺序：先用兼容核心板和对应固件验证实际目标游戏，再决定是否以此代码为开发基线。暂不需要改动 Stadia 原厂主板或其蓝牙固件。

## 核验范围

已检查 README、板卡说明、构建配置、BLE Central、USB 描述符及端点处理、主循环、Stadia/SInput 协议转换、两组现有测试。

本机 GCC 编译并运行上游的两组纯 C 测试：Stadia 协议 7 项、SInput 构建/解析 9 项，共 16 项检查通过。它们验证报文处理，不能证明无线链路、Windows/Steam 握手或长时间震动稳定性。

未进行：固件交叉编译、烧录、真实 BLE 抓包、游戏测试、距离/延迟/丢包测量。仓库没有随附 WCH SDK，本环境也未配置对应 RISC-V 工具链。远程仓库存在 `v1.0.0` 标签；本次未能成功读取 GitHub Release 资产列表，因此不确认预编译二进制与当前 main 的对应关系。

## 为什么技术上可行

1. 硬件能力：沁恒资料确认 CH582/CH592 系列带 BLE 和 USB；无需另外购买一个蓝牙模块。
2. 输入路径：BLE 服务发现和通知 → `stadia_parse_report()` → `sinput_build_report()` → USB HID IN。
3. 震动路径：USB HID OUT → `sinput_parse_haptic()` → `stadia_build_rumble_report()` → `StadiaBLE_SendOutput()`。
4. SInput ERM 命令的左右 8 位幅度乘以 257，形成 Stadia 的两个 16 位小端通道。直接写入 GATT 报告特征时使用 4 字节负载，USB HID 的报告 ID 不再作为前缀发送。
5. SDL 官方已有 SInput 驱动，使用 type 2 ERM 震动命令。项目不是仅在主机侧虚拟出一个“有震动能力”的控制器，而是实现了往物理手柄发送命令的路径。

证据：

- https://www.wch.cn/products/productsCenter/mcuInterface?categoryId=74
- https://www.wch-ic.com/products/CH592.html
- https://github.com/HandHeldLegend/SInput-HID
- https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_sinput.c
- https://github.com/O-IceDawn-O/stadia-stick/blob/e0d4a701130c24648e7f189064089644584e9248/src/stadia_protocol.c
- https://github.com/O-IceDawn-O/stadia-stick/blob/e0d4a701130c24648e7f189064089644584e9248/src/stadia_ble.c

## 成本与板卡选择

README 的 ¥6.8 是作者购买 ¥8.8 CH582F 板卡后叠加 ¥2 优惠的成交价；CH592F 作者成交价为 ¥11。这是作者的历史采购记录，本次没有独立核实当前售价、运费和优惠资格。

复用已有 USB 数据线时，物料可以接近一块核心板。数据线必须支持数据传输。核心板应带可用的 USB 数据接口、BLE 天线、BOOT/RESET；若希望不改源码，尤其应核对 PB22 BOOT 和 PA8 LED。仅购买裸芯片或缺少 USB 接线的板卡不具备同等即用条件。

USB ISP 烧录路线不需要额外的硬件调试器；开发自定义固件另需 WCH SDK、兼容交叉编译器及 Make。两种芯片的二进制不可默认混用。

## 使用边界

| 项目 | 当前 stadia-stick | 前面调查的 ESP32-S3 Xbox 接收器路线 |
|---|---|---|
| 蓝牙震动路径 | 已实现，作者报告真机成功 | 已有社区实现，仍需本机验证 |
| USB 呈现 | SInput HID | Xbox 360/XInput 兼容设备 |
| Windows 游戏兼容 | 优先 Steam Input 或支持 SInput 的 SDL 应用 | 面向 Xbox/XInput 游戏的兼容性通常更直接 |
| 首次设置 | README 要求 Steam 重新设置设备输入，校正扳机 | 取决于固件版本与游戏 |
| 手柄数量 | 1 个 | 取决于项目，有双手柄分支 |
| 断电重连 | 当前需手动进入手柄配对模式并按板卡 BOOT | 某些项目保存绑定并支持回连 |
| 配置界面 | 无；主要通过源码修改 | 某些分支有 Web 配置 |
| 扩展余量 | SRAM 较少；适合精简功能 | 更适合复杂配置及额外功能 |

SInput 协议获得 SDL 支持，不等于所有已有 SDL 游戏都内置了对应驱动，也不等于 Windows 自动将其变成 XInput。非 Steam 游戏需要逐款验证；通过 Steam 启动或增加主机映射层是候选路径，但应单独验证震动回传。

## 从源码发现的改进点

1. **断连后没有发送中立输入。** `stadia_ble.c:403` 清掉输入 ready，`main.c:99` 的断连回调仅清掉待发震动，输入发送又要求已连接。USB 接收器仍在，主机可能保留最后一次按键或摇杆状态。建议在断连时排队发送全部按键释放、摇杆归中、扳机归零的报告，并处理 USB 端点繁忙重试。
2. **没有持久化蓝牙绑定及自动回连。** `stadia_ble.c:153` 设置 `bonding=FALSE`，上电等待 BOOT 扫描。这与 README 的手动连接说明一致，属于当前功能取舍。若要改善，应增加绑定管理、上电回连与明确的重新配对入口。
3. **Assistant 键被丢弃。** Stadia 解析输出 bit 18，但 `sinput_builder.c` 的最终按键转换只包含辅助 bit 16、17。若后续需要宏或快捷键，需补映射及对应能力掩码。
4. **能力声明包含未实现内容。** `sinput_builder.c:149`、`:153` 宣称玩家 LED/RGB，`:170` 宣称背键，当前输出回调仅处理功能查询与震动。建议让能力掩码准确反映物理手柄和固件实际实现，减少主机误判。
5. **震动错误恢复容易被新命令重置。** `main.c:89` 每收到震动都清零连续失败计数；持续游戏反馈可能导致错误计数难以达到恢复阈值。建议按最后一次成功写入时间判断故障，增加发送频率控制并确保停止震动命令优先。
6. **震动解析没有校验 type。** `sinput_parse_haptic()` 把所有 haptic 命令按 type 2 布局解释。现有 SDL 普通震动走 type 2，因此当前路径合理；扩展使用时应拒绝或正确处理其他类型。

上述为代码审查发现，其中断连卡键风险尚未真机复现。现有测试未覆盖断连状态机、蓝牙写入失败和真实 USB 并发。

## 关于 GATT 写入方式的修正

本项目使用 `GATT_WriteNoRsp()`。作者注释称，尽管手柄特征没有声明该属性，实际仍接受无回执写入；这样避开 WCH 栈写请求等待回执的问题。

这与某些 ESP32 项目将 Windows 故障归因于“手柄只接受有回执写入”的解释不一致。因此，不能把回执方式的单一解释当成普遍事实。当前可确认的是不同主机栈的行为存在差异、独立 BLE 接收器是可行路线；无回执写入对目标手柄是否长期稳定，需要真机测试。

USB 的 1 ms 轮询及能力声明也不代表无线端实际 1000 Hz。源码配置 `DEFAULT_CONN_INTERVAL=20`，按 BLE 连接间隔单位对应请求 25 ms；最终连接参数及实际输入/震动时延需要测量。

## 建议的真机验收

1. 对应芯片固件烧录后，确认 USB HID 与 CDC 日志枚举正常。
2. Steam 中逐项验证摇杆、方向键、所有按钮和两个模拟扳机；分别测左右马达、双马达与停止震动。
3. 在实际目标游戏内连续测试输入与震动，观察日志和异常重启。
4. 摇杆保持偏转或按键保持按下时关闭手柄，检查游戏是否及时释放输入。
5. 验证断电、手柄关机、休眠恢复和重复连接；记录需要的人工操作。
6. 单独验证非 Steam 目标游戏，以及是否依赖 Steam Input；测无线距离和端到端延迟。

若实际目标是单手柄 Steam 游戏，CH582F 是合理的低成本起点。若主要需求是非 Steam 的 XInput 游戏、双手柄或丰富配置，应先评估协议扩展成本，再决定 CH582/CH592 与 ESP32-S3 的取舍。
