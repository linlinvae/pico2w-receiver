# 来源与许可

- CH582 USB、XInput、日志、平台及按键模块复制自本地 `../receiver/src/`，其来源为 O-IceDawn-O/stadia-stick，基线提交 e0d4a701130c24648e7f189064089644584e9248，及本地 R11 修改。保留版权注释和 Apache-2.0 许可证 `LICENSE`。原项目未修改。
- Bluepad32，Ricardo Quesada 及贡献者，Apache-2.0。固定版本见 `dependencies.lock.json`，完整许可保留在 `resources/bluepad32/LICENSE` 和 `licenses/Bluepad32.txt`。
- BTstack，BlueKitchen GmbH。按上游完整条款使用，许可证保留在 `resources/bluepad32/external/btstack/LICENSE` 和 `licenses/BTstack.txt`；该许可包含个人、非商业用途的条件，不应将本项目 Apache-2.0 许可证视作对 BTstack 的重新授权。Switch Pro 本版不应用上游为其他手柄准备的 GAP 安全降级补丁。
- BetterJoy，David Khachaturov 及贡献者。仅作为协议参考，不链接或复制其 C# 实现。原完整许可证保留在 `resources/BetterJoy/LICENSE` 和 `licenses/BetterJoy.txt`，包含其所列依赖的条款。
- ESP-IDF 与 WCH SDK 的各自原始版权和许可保留在依赖目录。WCH SDK 仅用于沁恒 MCU。

Bluepad32 公共示例入口为 public domain，本版入口按其平台初始化方式编写；应用层串口协议、状态机、映射适配和测试为本次独立开发。
