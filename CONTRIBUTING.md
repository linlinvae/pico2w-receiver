# 开发与反馈

当前开发主题是 Pico 2 W 单手柄蓝牙接收器。`receiver/` 和 `switchpro/` 保存早期方案，不移动或改写已有回退固件、发行包与 preservation manifests 覆盖的文件。

## 反馈问题

请使用 GitHub Issue 模板，提供固件文件名、上位机版本、板卡/手柄型号、系统版本、复现步骤和预期结果。请区分蓝牙连接、USB 枚举与游戏输入；有设备管理器错误码时一并提供。默认不需要串口日志，附件请先移除个人信息和蓝牙地址。

## 固件构建与验证

在仓库根目录的 Windows PowerShell 中运行：

```powershell
.\pico2w\tools\setup.ps1
.\pico2w\tools\test.ps1 -Gcc gcc
.\pico2w\tools\build.ps1 -Python python
# 独立 Switch USB 模式
.\pico2w\tools\test-switch.ps1 -Gcc gcc
.\pico2w\tools\build-switch.ps1 -Python python
```

Git、Python 3 和主机测试用的 GCC 需预先可用；`setup.ps1` 按锁文件准备 SDK、ARM 工具链、CMake、Ninja 等依赖。脚本不自动刷机。软件测试、编译与 UF2 检查不能代替手柄、USB 主机和无线实测。

## Pico Control V0.3

需要 Node.js 24 或兼容版本、pnpm 11、Windows .NET Framework 编译器。

```powershell
Push-Location .\pico2w\host\modern\ui
pnpm install --frozen-lockfile
Pop-Location
.\pico2w\host\modern\build.ps1 -Test
```

实际运行还需要 WebView2 Runtime。详细检查与预览方式见 [modern/README.md](pico2w/host/modern/README.md)。

## 提交要求

- 遵循各级 `AGENTS.md`；固定依赖版本，不修改下载的供应商源码。
- 修改固件时运行相关算法、USB、蓝牙状态机测试及交叉编译；准确记录未完成的硬件验收。
- 修改文档或仓库结构时执行 `powershell -NoProfile -File .\tools\verify-repository.ps1`，核对链接与保留文件哈希。
- 不提交凭据、SDK/工具链、缓存、临时构建输出或重复解压目录；保留第三方版权、来源与许可证。
- 新版本使用新的文件名和构建记录，保留旧固件回退入口；不要为文档更新重新打包旧交付物。
- 提交前检查 `git diff --check` 和实际差异。GitHub Actions 执行仓库完整性检查，不声称完成硬件测试。
