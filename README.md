# Google Stadia 与多手柄无线接收器开发

本仓库保存当前源码、研究记录、交付固件和 Windows 上位机发行包。

| 目录 | 成果 |
| --- | --- |
| [receiver](receiver/README.md) | CH582F Stadia 蓝牙接收器，保留 CH592 构建选项、SInput 与 Windows XInput 版本 |
| [switchpro](switchpro/README.md) | ESP32 与 CH582F 组合的 Switch Pro 接收器 |
| [pico2w](pico2w/README.md) | Pico 2 W 单手柄接收器，支持 Xbox Series、Switch Pro 与 Stadia；Windows R5 和独立 Switch USB R1 固件 |
| [Pico Control](pico2w/host/modern/README.md) | Windows 上位机 V0.3，含电量、输入、震动和 R5 接收器配置界面 |
| [audit](audit/8bitdo-pico2w-feasibility.md) | 技术可行性调查 |

## 使用与验证

请先阅读对应目录的 README 和 VALIDATION.md。各硬件方案的实物验证状态不同；编译与模拟测试通过不等于所有无线、震动或目标主机功能均已实测。

- Pico 固件位于 `pico2w/firmware/`，历次完整交付包位于 `pico2w/releases/`。
- Windows 上位机发行包位于 `pico2w/host/releases/`；V0.3 使用 `PicoControl-v0.3-windows.zip`，完整解压后运行。
- CH582/CH592 固件位于 `receiver/firmware/`，组合方案固件位于 `switchpro/firmware/`。
- 历史背景见 [DEVELOPMENT_CONTEXT.md](DEVELOPMENT_CONTEXT.md)；后续进展以各子项目文档为准。

## 依赖与来源

各子项目的 `dependencies.lock.json` 固定 SDK、工具链与上游版本；按照各目录的 setup/build 说明恢复依赖。下载的 SDK、工具链、参考资料、原理图、构建缓存及重复验证目录仅保留在本机，未纳入本仓库。

原始参考项目为 [O-IceDawn-O/stadia-stick](https://github.com/O-IceDawn-O/stadia-stick)，参考提交 `e0d4a701130c24648e7f189064089644584e9248`。本地 `reference/stadia-stick/` 保持独立且未修改。如需对照，请另行克隆并检出该提交。

各子项目保留自己的 LICENSE、NOTICE 或 THIRD_PARTY 文件，第三方组件遵循其各自许可。

## 更新同步

用户已要求每次项目更新完成后检查、提交并推送 GitHub，具体约定见 [AGENTS.md](AGENTS.md)。本地提交和远程推送分别确认；连接受阻时保留本地提交并报告待同步状态。
