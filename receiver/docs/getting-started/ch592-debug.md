> 当前用户开发板已更正为 CH582F，请按 [CH582 调试步骤](ch582-debug.md) 使用对应固件。本文保留供后续 CH592 硬件参考，不能套用当前 CH582 板。

# CH592F 开发板烧录与调试

2026-10-07：已交叉编译并完成主机测试；首轮真机日志确认 USB/CDC、BLE 就绪与扫描键有效，手柄连接尚未成功。以下验收表仍待完成。扫描失败排障见 `scan-diagnosis.md`。

## 烧录

使用支持数据传输的 USB-C 线，确认 CH592F 型号、ISP 按键说明和应用扫描键接线。当前默认扫描键为 PB22；LED 和内部 DC/DC 关闭。LED 不闪不代表程序未运行。

1. 安装沁恒官方 [WCHISPTool](https://www.wch.cn/downloads/WCHISPTool_Setup_exe.html)。
2. 按商家说明进入 USB ISP。上游兼容板流程为按住 BOOT 插 USB，工具识别后松开；本板以其说明为准。
3. 工具选 CH592 系列、USB 下载，加载 `firmware/CH592-dev/stick.bin` 或 `.hex`。
4. 下载成功后按 RST 或重新上电。
5. Windows 应枚举 HID 和 CDC 串口；USB VID/PID 为 `2E8A:10C6`，名称 `Stick (SInput)`。

USB ISP 与 CDC 日志不需要外接串口模块。断点调试需要适配的硬件调试接口/工具；CDC 提供的是运行日志。

## 日志与连接

设备管理器确认 CDC 对应 COM 号。串口监视器依赖 Python 与 pyserial，进入 `receiver` 目录执行：

```powershell
python -m pip install pyserial
python .\tools\serial_monitor.py --port COM7 --no-color
```

COM7 换成实际端口。监视器开启 DTR 后才发送 CDC 日志。打开后按 RST，预期看到 `Board=CH592F-dev LED=0 DCDC=0`、`Init done, press BOOT to scan`。

手柄应已转换蓝牙模式。按上游流程进入配对模式（Stadia + Y），再短按应用扫描键。预期日志为 `Scanning for Stadia`、`Found Stadia`、`Connected`、HID 特征句柄、`HID notifications enabled`。

Steam 控制器设置中逐项检查输入，必要时执行“设置设备输入”。已连接时按扫描键先断开再扫描，扫描中按键取消；连续震动写入失败达到阈值后需手动重试。

默认 INFO；排障可用 `tools/build.ps1 -LogLevel 4` 重编译。DEBUG 高频日志会影响时序，测性能时恢复 INFO。保存日志原文、`build-info.json`、系统/Steam 版本和板级接线。

## 实测验收表

| 测试 | 操作及通过条件 | 结果 |
|---|---|---|
| USB/日志 | 上电、开 COM、复位；HID/CDC 正常，BLE 就绪 | 待测 |
| 输入 | ABXY、方向、肩键、双扳机、摇杆/按压、Menu/Options/Guide/截图正确；扳机零到满量程 | 待测 |
| 马达/停止 | 游戏或支持 SInput 的程序分别驱动左、右、双马达并停止；通道正确，停止不被吞掉 | 待测 |
| 断连释放 | 按住键/偏转摇杆时关手柄；断链事件后主机释放输入 | 待测 |
| 重扫/取消 | 已连接按键重扫，扫描中取消；状态正确 | 待测 |
| 重复连接 | 关手柄、接收器拔插，重复 20 次；手动扫描均成功 | 待测 |
| 长时运行 | 输入与震动同时运行至少 30 分钟；无重启、卡键、持续震动 | 待测 |
| USB 休眠 | Windows 休眠恢复、拔插后重新枚举并按手动流程连接 | 待测 |
| 目标游戏 | 每款常用 Steam/非 Steam 游戏同时验证输入与物理震动 | 待测 |

断连释放从 BLE 断链事件开始；连接监控超时和 USB 主机轮询会增加等待，不能承诺关手柄的瞬间就释放。

无日志时区分 ISP 设备与运行后的 CDC；扫描键无反应时核对 PB22/PB23；已连接但无 `HID notifications enabled` 时保留 GATT 错误日志，检查服务发现和 CCCD 句柄。

通过此表后，迁移开发板确认过的板级配置到自制接收器，并完整重跑验收。

当前构建按官方 Central 示例使用外部 32.768 kHz（BOARD_LF_CLOCK_SOURCE=0），应用与所有 SDK 编译单元通过强制预包含板级配置保持一致。板上必须有相应晶振电路。扫描诊断第三版会检测起振并记录100ms RTC计数；未起振时停止BLE初始化、保持USB CDC，详见扫描诊断文档。
