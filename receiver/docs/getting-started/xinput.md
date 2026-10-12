# R11：Windows XInput 实验版

这份固件将 USB 游戏手柄接口切换为 **PC 有线 XUSB 1.00 / XInput**，目标是让 Windows 使用 Xbox 兼容驱动，改善游戏和浏览器的按键映射。它沿用 R10 的 Stadia BLE、绑定保存、自动回连、扫描键和双马达震动队列；默认构建仍是 SInput。

这不是 Xbox 主机固件，也不提供主机认证、耳机音频或扳机马达。蓝牙仍连接 Stadia，电脑通过接收器 USB 使用 XInput。

## 烧录与确认

1. CH582F 板烧录 `firmware/CH582-dev-xinput/stick-CH582F-xinput-r11.bin` 或同名 `.hex`。烧录完成后拔插 USB，等待 Windows 枚举新的设备。烧录工具若擦除了 DataFlash，需要重新配对；本版没有更改 R10 的保存地址布局。
2. 打开手柄，等待自动回连；首次配对仍按 [绑定说明](bond-reconnect.md)。不要为切换 USB 模式长按扫描键，长按会清除绑定。
3. 按 `Win + R`，运行 `joy.cpl`。查看是否出现 Xbox 兼容游戏控制器（名称取决于 Windows 驱动），进入属性检查 ABXY、方向键和两根摇杆。
4. 在 Chrome / Edge 打开原测试网站，刷新页面并按一下 A。浏览器可能需要按键交互后才显示设备。网站使用 Gamepad API，最终识别与映射仍取决于驱动、浏览器和网站实现，不能保证每个网站都支持。
5. 在支持 XInput 的游戏中检查 LT、RT 独立值和震动；断开手柄时应释放所有输入，重新开机应恢复回连。Windows 传统 DirectInput 测试面板可能把两个扳机合为一根轴，这是兼容层的行为，不代表固件只传了一根扳机。

若 `joy.cpl` 没有设备，在设备管理器检查 USB 游戏手柄接口是否出现警告、驱动名称及错误码即可；不需要读取 CDC 串口日志。新模式 PID 为 `10C7`（默认 SInput 为 `10C6`），用于分开开发过程中的驱动缓存。CDC 接口仍保留，COM 编号可能变化。

回退镜像为 `firmware/CH582-dev/stick-CH582F-bond-reconnect-r10.bin`。用户已经确认 R10 自动回连正常；R11 的实际 Windows 驱动加载、网页映射和震动还需要烧录验证。

交付 `.bin` 为 176356 字节，SHA256：`E8DD394771C16DC26BA1F07E2CA0208E5FBDEFB9B14B5B8EB7E45E5930F5CA6A`。同目录的 `stick-CH582F-xinput-r11-build-info.json` 保存编译配置与源码校验值。

## 实现与验证

- 原生 WCH USB：接口 0 为 XUSB，接口 1/2 为 CDC；游戏手柄沿用 EP3 IN/OUT，最大包 32 字节。
- Microsoft OS 字符串及 `XUSB10` 兼容 ID；USB 芯片标识形成固定序列号。
- 输入为 20 字节：ABXY、肩键、菜单、摇杆按压、方向键、Guide，两根 signed 16-bit 摇杆及独立 8-bit LT/RT。Y 轴转换为 XInput 正值向上，居中与两端范围精确保留。Stadia 原始摇杆仍是 8-bit，扩展数值范围不会增加物理精度。
- 默认不映射 Capture / Assistant。Guide 在不同 API 中的可见性由 Windows 决定。
- 震动接受 8 字节 XUSB 双马达指令，保留原停止优先、失败退避和断连处理；不会增加马达数量或改善硬件本身的细腻程度。
- 保留忙端点重试和断连中立报告，空闲时每 4 ms 刷新最近报告。USB 刷新周期不代表无线端到端延迟。
- R11 主机测试 594 次检查通过，包括原 BLE/回连测试、完整按键映射、256 个轴值的单调性、USB 多包描述符传输、OS 兼容 ID、控制及端点震动缓冲、CDC 请求、复位/休眠、忙端点和空闲报告。CH582 SInput、CH582 XInput、CH592 XInput 均交叉编译通过，应用无新增警告；SDK 内部 `x32Kpw` 原有未使用警告保留。测试报告在 `build/validation/r11-tests.txt`。
- 上述测试使用模拟寄存器，不能证明 Windows 驱动实际绑定或蓝牙射频表现。

在 `receiver` 目录构建：

```powershell
.\tools\build.ps1 -Chip CH582 -Board dev -UsbMode xinput
.\tools\test.ps1 -Chip all
```

普通构建输出是 `firmware/CH582-dev-xinput/stick.bin`；带 `r11` 名称的交付镜像固定保留。可叠加 `-Variant scan-diag` 单独生成诊断版本。

协议根据 [Microsoft MS-XUSBI](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-xusbi/c79474e7-3968-43d1-8d2f-175d47bef43e) 原创实现，OS 兼容 ID 见 [2.2.4.6](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-xusbi/601a3107-6583-4e80-867e-118829cb573e)。扳机兼容层差异见 [Microsoft XInput 与 DirectInput](https://learn.microsoft.com/en-us/windows/win32/xinput/xinput-and-directinput)，浏览器标准映射见 [W3C Gamepad](https://w3c.github.io/gamepad/#remapping)。
