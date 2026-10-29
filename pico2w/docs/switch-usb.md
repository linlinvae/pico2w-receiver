# Pico 2 W → Switch USB 接收器 R1.1

2026-10-29：R1.1 同步 Windows R5.1 的公共蓝牙搜索恢复修复：清理空闲残留 BLE 建连请求、周期停止/重启搜索、异常 setup 槽超时恢复。9062 项 Switch 模式软件检查与 ARM 构建通过；实物回连及 Switch 枚举仍待验证。原 R1 文件与包保留。

本版本在现有 R5 蓝牙接收器上新增 Switch 有线 HID 输出。同一份固件识别 Xbox Series、Google Stadia 和第一代 Switch Pro，每次连接一只手柄。

连接：**手柄 → 蓝牙 → Pico 2 W → Micro-USB 数据线 → Switch 底座 USB-A**。

软件测试、交叉编译与 UF2 检查通过后仍需实机验收；不能把电脑模拟测试等同于 Switch 已识别或三种手柄都在 Switch 上测试成功。

## 刷入与连接

1. 按住 Pico 的 BOOTSEL，插入电脑，出现 RP2350 磁盘后松开。
2. 将 `firmware/pico2w-switch-r1-1.uf2` 拖到该磁盘。磁盘消失后拔下 Pico。
3. 使用 Micro-USB **数据线**将 Pico 插入 Switch 底座的 USB-A，Switch 正常开机并插入底座。
4. 先只打开一只手柄。此前配对记录保留时可尝试正常开机回连；首次配对或手柄切换过主机时进入配对状态：
   - Xbox Series：长按顶部配对键，Xbox 灯快速闪烁。
   - Switch Pro：长按 USB-C 旁的 SYNC，玩家灯循环闪烁。
   - 已启用蓝牙模式的 Stadia：按住 Y + Stadia，橙灯闪烁。
5. Pico LED 常亮表示蓝牙手柄就绪；慢闪表示等待连接，**常亮不代表 Switch 已识别 USB**。
6. 在 Switch 的“手柄 → 更改握法/顺序”中按 L + R，确认出现一个手柄，再检查输入。

USB 使用既有开源项目采用的 `0F0D:0092` 有线 HID 兼容布局，不是 Nintendo Pro USB 协议。因此不依赖“Pro 手柄的有线通信”设置；若已有设置可保持。蓝牙 Switch Pro 仍作为输入手柄读取。

换手柄时先关闭当前手柄，等 Pico LED 恢复慢闪后再打开另一只。不会让三只手柄同时作为三个玩家。短按运行中的 BOOTSEL 在空闲时恢复扫描；长按至少 3 秒再松开会清除所有配对并重启，除非确实需要重新配对，不要长按。

## 按键和功能

| 输入 | Switch 输出 |
|---|---|
| Xbox/Stadia A、B、X、Y | B、A、Y、X（按物理位置） |
| Switch Pro A、B、X、Y | A、B、X、Y |
| LB/RB 或 L/R | L/R |
| LT/RT 或 ZL/ZR | ZL/ZR；模拟扳机达到 50% 才按下 |
| Back/View、Start/Menu | Minus、Plus |
| Xbox/Stadia/Home 系统键 | Home |
| 协议库提供的 Capture/Share 位 | Capture；具体源手柄字段仍需实测 |
| 左右摇杆按下 | L3/R3 |

支持两个 8-bit 摇杆、十字键（含斜向）、Home、标准按键和数字扳机。中心为 128，十字键中立值 8。相反方向互相抵消，另一个方向轴保留。

本 HID 模式**没有震动、陀螺仪、NFC、音频或主机唤醒功能**。USB 描述符保留兼容输出报告，但不将它误当成震动或配置命令。USB 端点间隔 1 ms 是调度参数，不是实测端到端延迟。手柄断连、USB 复位、暂停或卸载会释放输入；蓝牙断连发现时间仍受原有协议栈和链路监督超时影响。

R5 已永久保存的映射、死区、反转和宏继续用于公共输入映射。设置里的按键名称仍是 Xbox 位置命名，之后再转成 Switch 位置映射。Switch 模式不提供 R5 的配置/电量 HID 接口，也不能在 Pico Control 中作为 XInput 手柄测试。要编辑设置或回退 Windows 模式，重新刷入原 `pico2w-xinput-r5.uf2`；未清除 Flash 时配置与配对记录保留。

## 实机验收

对三种手柄**分别**重复以下检查：

1. Switch 菜单识别一个手柄；L+R 确认加入。
2. A/B/X/Y 位置、L/R、Minus/Plus、Home、摇杆按下和 Capture（如源手柄提供）正确。
3. 双摇杆中心、上下左右和极限正确；十字键四向与斜向正确。
4. Xbox/Stadia 扳机在阈值附近按下和释放；Switch Pro ZL/ZR 正常。
5. 按住按钮时关闭手柄，确认 Switch 上释放旧输入，再开机回连。
6. 保持 Pico 插入时换另外两种手柄；旧按键、Capture 或摇杆不残留。
7. 拔插 Pico、Switch 进入休眠后唤醒，确认恢复；Pro 顶部 SYNC 休眠后普通按键唤醒。

若蓝牙已连接但 Switch 没有手柄，先确认数据线与底座 USB 口，反馈 Switch 型号、系统版本、手柄型号和失败步骤。无需读取串口日志。当前版本以第一代 Switch（含 OLED）为目标，Switch 2 的 HID 兼容性未验证。

## 构建与验证

```powershell
.\pico2w\tools\test.ps1
.\pico2w\tools\test-switch.ps1
.\pico2w\tools\build-switch.ps1 -Python <你的Python可执行文件路径>
.\pico2w\tools\package-switch.ps1
```

默认独立构建目录为 `build/switch-r1-1/`，支持 `-BuildDirectory <目录>`；本机新构建使用按源路径哈希区分的 D 盘目录。输出为 `firmware/pico2w-switch-r1-1.{uf2,bin,elf}` 及 `-build-info.json`。旧 PC 固件文件和交付包有单独哈希审计。Flash 的配置、蓝牙 TLV 与勘误保留区布局不变，UF2 检查拒绝覆盖这些扇区。不自动刷写硬件。

协议参考：GP2040-CE 的 [SwitchDescriptors.h](https://github.com/OpenStickCommunity/GP2040-CE/blob/main/headers/drivers/switch/SwitchDescriptors.h) 和 [SwitchDriver.cpp](https://github.com/OpenStickCommunity/GP2040-CE/blob/main/src/drivers/switch/SwitchDriver.cpp)。描述符适配保留 MIT 声明，见 `licenses/GP2040-Switch.txt`；Bluetooth 使用现有固定依赖，未下载新协议库。
