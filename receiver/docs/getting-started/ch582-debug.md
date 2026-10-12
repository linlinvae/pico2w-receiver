# CH582F 开发板（R10）

用户已确认 R9 固件可用。R10 在此基础上增加绑定保存和自动回连，使用 `receiver/firmware/CH582-dev/stick-CH582F-bond-reconnect-r10.bin` 或对应 `.hex`。操作及上板验收见 [保存绑定与自动回连](bond-reconnect.md)。本轮不需要串口日志采集。

按此前已成功的 BOOT + USB 方式进入 ISP，WCHISPTool 选择 CH582，加载 R10 文件烧录并重启。首次把手柄置于橙灯闪烁的配对模式，短按扫描键并释放；成功连接后检查按键／摇杆输入。之后保持开发板正常运行，开关手柄应自动回连；开发板断电再上电也应自动回连。

要更换手柄或重新配对，在固件已运行时长按扫描键至少 3 秒，松开后把目标手柄置于配对模式。启动时按住 BOOT 属于 ISP 操作，与运行时清除绑定不同。烧录工具若擦除了 DataFlash，需重新配对。

构建与检查：

```powershell
& .\receiver\tools\test.ps1 -Chip all
& .\receiver\tools\build.ps1 -Chip CH582
```

默认 `BOARD_LF_CLOCK_SOURCE=1`，使用 SDK 校准的内部 32K；固定 25 ms 连接间隔，保留 R9 的 MTU 顺序／请求接受检查／发现空闲超时修复。LED、DC/DC 默认关闭，PB22 低有效，INT_SOFT 未定义，栈预留 2048 字节。只有明确验证了自制板配置后，才指定 `-BoardDefines BOARD_LF_CLOCK_SOURCE=0` 使用外部 32.768 kHz。

R9 的原诊断镜像 `firmware/CH582-dev-scan-diag/stick-CH582F-scan-diag-r9.bin` 已保留：174948 字节，SHA256 `582D77F32AF4D1C8057B100C04ECBB010408FE5AFD83BF6C1C3E076D5D72C4DB`。历史故障与修复见 [回归对照](upstream-regression.md)。
