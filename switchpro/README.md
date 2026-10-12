# Switch Pro → XInput：独立接收器固件 R1

目标是第一代任天堂原装 Switch Pro（HAC-013，VID/PID 057E:2009），无线连接后向 Windows 输出 XInput。使用原版 ESP32-WROOM-32 作为经典蓝牙接收端，CH582F 作为 USB 输出端。该模块型号是当前开发基线，尚未得到用户实物型号确认。

```text
Switch Pro ──经典蓝牙── ESP32-WROOM-32 ──双向 UART── CH582F ──USB── Windows
                                 ←── 游戏震动回传 ──←
```

本目录独立于 `../receiver/`。现有 Stadia 源码、构建脚本、R10/R11 镜像均保留；当前 CH582 XInput 镜像另存于 `preserved/CH582-dev-xinput/`。`preserved/receiver-manifest.json` 记录开发前 121 个原项目文件的 SHA256，`tools/verify-preserved.ps1` 可检查是否变化。新 CH582 固件不初始化 BLE、不写 DataFlash，不覆盖原 Stadia 地址和绑定记录。

## 硬件与接线

需要 **原版 ESP32，支持 BR/EDR 的 ESP32-WROOM-32/32D/32E**。ESP32-S3、C3、C6 不能作为本版本的替代品。现有 CH582F 无法仅靠改固件获得经典蓝牙能力。

默认两端为 115200、8N1、3.3V 电平，不使用流控：

| ESP32-WROOM-32 开发板 | CH582F 开发板 | 用途 |
|---|---|---|
| GPIO17（TX） | PA8（UART1 RX） | 输入及状态 |
| GPIO16（RX） | PA9（UART1 TX） | 震动及配对控制 |
| GND | GND | 共地 |
| 独立 USB 供电 | USB 接电脑 | 给各自开发板供电 |

表内引脚来自芯片 UART 配置，**尚未核对你这块开发板是否引出、是否被板上外围占用**。CH582 端 LED 默认关闭，避免占用 PA8。若改用 CH582 UART1 重映射端口，执行 `tools/build-ch582.ps1 -UartRemap 1`，此时 ESP32 TX 接 PB12，RX 接 PB13。ESP32 的 GPIO16/17 适用于 WROOM，不能直接套用带 PSRAM 的 WROVER 板。

两块开发板分别通过自己的 USB 供电，串口只连 TX/RX/GND；不把两个板的电源输出相连，也不把 5V 接到串口引脚。不要默认 CH582 板的 3.3V 稳压器能给 ESP32 供电。裸 ESP32 模块需要另行设计电源和下载电路，当前固件按 4MB Flash 开发板编译。

## 固件与烧录

两端固件须配套使用。`CH582-SwitchPro-r1` 只负责 USB/串口，单独烧入现有板后无法无线连接 Switch Pro。

| 目标 | 输出文件 | 地址 |
|---|---|---|
| CH582F | `firmware/CH582-SwitchPro-r1/switchpro-CH582F-r1.bin` 或 `.hex` | WCHISP 选择 CH582，应用从 0x0；不擦 DataFlash |
| 原版 ESP32，4MB | `firmware/ESP32-SwitchPro-r1/switchpro-ESP32-r1-full.bin` | 合并镜像从 0x0 写入 |

ESP32 目录同时保存独立 app、bootloader、分区表、ELF 和构建信息。**合并镜像用于首次烧录，会擦掉其中 NVS 地址范围的已有配对信息。** 后续更新使用 `tools/flash-esp32.ps1 -Port COM实际端口`，分别写入 bootloader、分区表和 app，跳过 NVS；首次也可以用该脚本。脚本只有显式运行才烧录，不扫描或猜测端口。每份镜像都有 `build-info.json` 和 SHA256。尚未自动对任何实物烧录或读取串口日志。

首次连接：

1. 核对并连接 TX/RX/GND，给 ESP32 和 CH582 分别烧入对应镜像。
2. CH582 接电脑，ESP32 上电；ESP32 会开始寻找 Pro 手柄。
3. 长按 Pro 顶部 SYNC 键直到玩家灯循环闪烁。固件完成初始化后点亮第一个玩家灯。
4. 在 Windows `joy.cpl` 或此前通过的 XInput 测试工具中检查按键和摇杆。
5. 单独检查游戏震动、断开手柄后输入释放、两端断电后的回连。

CH582 默认应用键为 PB22、低有效：短按启动扫描/重连；长按 3 秒请求 ESP32 关闭蓝牙、清除其配对密钥与地址名单并重启，随后配对新手柄。只清除 ESP32 记录，原 Stadia 的 CH582 DataFlash 保留。回连依赖 ESP32 的 NVS 和手柄被按键唤醒，不能把“固件保留密钥”当作已经完成真机回连验收。

## 映射与边界

默认按物理位置映射，符合 Xbox 游戏操作位置：Switch B→Xbox A、A→B、Y→X、X→Y。L/R→LB/RB，ZL/ZR→LT/RT，−→Back，+→Start，Home→Guide，摇杆按下→LS/RS。ZL/ZR 是数字扳机，输出只能是 0 或 255。

可以在 `tools/build-esp32.ps1 -Action menuconfig` 的 `Switch Pro bridge` 下开启按字母映射，此时任天堂 A/B/X/Y 对应 Xbox 同名字母。摇杆使用 Bluepad32 获取的校准结果，转为 16 位 XInput，Y 正方向为上；不会先压缩成 Stadia 的 8 位轴。

Capture 没有标准 XInput 按键槽，本版不额外映射；陀螺仪、加速度、NFC、DSU/Cemuhook、鼠标和键盘模拟不输出。仅接受一个第一代原装 Pro，第三方同类手柄、Joy-Con、Switch 2 Pro 不在本版验收范围内。

XInput 的 large/strong 和 small/weak 震动通过 Bluepad32 转为 Switch 震动频段，两侧执行器接收相同的频段数据；这不是原样重现所有任天堂 HD Rumble 效果。停止命令优先于后续非零命令，串口丢 ACK 会重试，配对操作去重。ACK 表示 ESP32 已接收并提交处理，不能证明手柄马达实际执行。

ESP32 每 20ms 发状态，CH582 在 250ms 未收到有效状态时释放输入；ESP32 也检查真实手柄报告的新鲜度。串口命令 100ms 刷新，300ms 未收到命令时停止震动，单次马达作用时长限制为 200ms。这些是保护和调度参数，不代表实测无线延迟。

## 编译与测试

从本项目目录运行：

```powershell
& .\tools\test.ps1
& .\tools\build-ch582.ps1
& .\tools\build-esp32.ps1
& .\tools\verify-preserved.ps1
```

CH582 构建只读取 `../receiver/` 中固定版本的 SDK 和 WCH 编译器。主机检查需要 PATH 中的 GCC。ESP32 的 ESP-IDF 5.5.1、工具链及 Python 环境全部在本目录 `resources/` 中；未修改全局 PATH 或安装全局驱动。新电脑可使用 Python 3.12 与 Git，运行 `tools/setup-esp32.ps1 -Python 'C:\实际路径\python.exe'`，再运行构建脚本。依赖版本见 `dependencies.lock.json`。

当前验证记录见 [VALIDATION.md](VALIDATION.md)。主机测试和交叉编译不能验证无线配对、串口接线、Windows 枚举或真实震动。

## BetterJoy 参考与来源

已阅读并固定 [BetterJoy](https://github.com/Davidobot/BetterJoy) 的提交 `b6715638a3ed1084f8968e8cafebbc6fe2ed0096`，参考其 `Joycon.cs` 中的输入模式 0x30、震动启用 0x48、SPI 校准和震动编码，以及 `SwapAB/SwapXY` 配置思路。它在电脑上通过 HIDAPI 读取手柄，再通过 ViGEm 创建虚拟控制器。本版由 CH582 直接输出 USB XInput，使用 ESP32 的蓝牙控制器，电脑无需运行 BetterJoy 或安装其虚拟手柄驱动。

协议底层使用 [Bluepad32](https://github.com/ricardoquesada/bluepad32) 和 [BTstack](https://github.com/bluekitchen/btstack)，未移植 BetterJoy 的 Windows/C# 程序。BetterJoy 的震动策略取两个 XInput 马达强度最大值，本版保留两个强度，并交由 Bluepad32 的 Switch 编码处理。单手柄无需配对 Joy-Con 的逻辑。所有参考源码保存在 `resources/`；来源与许可见 [THIRD_PARTY.md](THIRD_PARTY.md)。

恢复当前 Stadia 功能时，使用 `preserved/CH582-dev-xinput/stick-CH582F-xinput-r11.bin` 或原 `../receiver/firmware/CH582-dev-xinput/` 镜像，只重刷 CH582 应用，不擦 DataFlash。
