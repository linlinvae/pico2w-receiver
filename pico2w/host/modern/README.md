# Pico Control V0.3 — shadcn/ui 桌面版

React + shadcn/ui 原创界面，由 WebView2 承载；保留现有 C# HID / XInput、配置校验和短时震动逻辑。适用于 Windows 10/11 **x64**、.NET Framework 4.6.2 及以上、Microsoft Edge WebView2 Runtime。当前电脑已确认具备运行环境。界面资源全部在本地，无需账号或联网加载页面。

## 启动

完整解压 `PicoControl-v0.3-windows.zip` 到任意本地目录，再双击 `PicoControl.exe`。DLL 和 `ui/` 必须保留在旁边，不能只复制 EXE。旧 V0.1/V0.2 的程序及发行包保持原样。

若提示缺少 WebView2 Runtime，可从 [微软官方页面](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) 安装 Evergreen Runtime。Microsoft Edge 浏览器本身不等于 WebView2 Runtime。本程序不自动安装系统组件。界面运行数据保存于 `%LOCALAPPDATA%/PicoControl/WebView`，演示验证使用独立本地目录。

## 使用

- **接收器概览**：顶部选择接收器，查看无线手柄、电量及来源。BLE 显示百分比，Switch 显示五档，Xbox 原始值保留原单位。未知不会显示为零。
- **输入测试**：选择 XInput 槽位并按键确认其对应当前 Pico，检查按键、双摇杆、独立 LT/RT。显示的是映射后的输出；不能自动把某个槽位视为所选接收器。
- **震动测试**：勾选槽位确认后分别测试强/弱/双马达，每次 0.7 秒、约 30% 指令强度。停止按钮、切换页面/设备/槽位、断开、失去窗口焦点或退出会停止。接收器配置的马达缩放也会生效。
- **手柄配置**：需要 R5 固件。先“读取接收器”，再编辑映射、轴向死区/反转、震动强度或宏。“立即应用”只改当前运行配置并回读核对；断电恢复上次永久保存的内容。“永久保存”会打开说明弹窗，关闭手柄后才能确认，保存会自动重启接收器。

界面会区分“编辑中”“已应用但未保存”和“已永久保存”。永久保存后等待接收器恢复，再重读并拔插验证。刷新接收器状态不会自动提交配置。恢复默认只改本地编辑区，仍需应用/保存。

配置供三种手柄共用一档。15 个标准键可映射或禁用；LT/RT 不在映射列表内。Switch 的 B/A/Y/X 默认对应 XInput A/B/X/Y。死区为 X/Y 各轴处理，0–50%，四轴可独立反转。马达强度 0–100%。

宏为一只标准按键触发、最多16步，每步20–5000毫秒、总计30秒。一次按下执行一次，长按不循环。触发键被占用，其他输入继续生效；宏输出在映射后合并，LT/RT取较大值。按键组合可写 `A+B`、`LB`、`↑`，留空表示宏释放，不释放物理上仍按住的键。断线、USB休眠和新配置取消宏。可增删步骤，或从 B 触发 A 连按示例开始。

旧 R4.1/R4.2 固件可继续查看状态、输入和震动；读取配置会提示需要 R5。本版界面升级不要求重刷已经使用 R5 的接收器。

## 构建

源码包包含旧通信代码、新界面源码、WebView2 SDK 所需的 DLL 和配置互操作向量。需要 Node.js 24 或兼容版本、pnpm 11，以及系统 .NET Framework 编译器。

```powershell
cd modern/ui
pnpm install --frozen-lockfile
cd ../..
./modern/build.ps1 -Test
```

UI 依赖固定在 `pnpm-lock.yaml`。shadcn/ui 官方 CLI 生成 Button、Card、Badge、Select、Tabs、Slider、Switch、Checkbox、Table 和 AlertDialog 等组件；自定义布局及颜色，组件仍基于 React/Radix。本项目不需要 .NET SDK，不更改接收器固件。

构建输出为 `dist-v0.3/`，实际使用时无需 Node 或 pnpm。`modern/build-ui-sandbox.ps1` 是仅更新 JS 的受限环境辅助方案，保留已有 Vite 编译 CSS；修改样式时必须执行完整 Vite 构建。

## 开发检查

`PicoControl.exe --demo` 运行清楚标注的示例模式，不访问设备。`--preview <输出目录> [small]` 在 WebView2 演示模式操作真实控件、验证消息往返并导出六页截图。演示模式所有震动与配置操作都使用模拟实现。`--self-test <报告>` 不访问设备。`--probe <报告>` 仅只读查询 HID/XInput，不发送配置或震动。

前端经本地 WebView2 消息与桌面端通信，无本地 HTTP 服务。原始 HID 写入仍检查确切选定路径、VID/PID、序列号、Feature 能力及状态，不猜测其他接收器；回复关联、配置事务、CRC及回读保持原实现。窗口关闭会取消后续配置命令，已经发出的保存由固件完成。

程序自测与界面验证范围见 `VALIDATION.md`。应用代码 Apache-2.0，shadcn/ui/React/Radix等第三方版权见 `THIRD-PARTY-NOTICES.txt`，WebView2 SDK 许可见 `WEBVIEW2-SDK-LICENSE.txt`。
