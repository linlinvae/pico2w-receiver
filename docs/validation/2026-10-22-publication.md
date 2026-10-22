# 仓库公开整理验证（2026-10-22）

本轮将仓库主题改为 Pico 2 W 接收器，补充首页、直接下载与 SHA256、构建/反馈指南、根目录许可与第三方来源，以及 GitHub Issue 模板和仓库完整性工作流。

## 本地验证

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify-repository.ps1` 通过：9 份 preservation manifests 的 350 条哈希匹配，6 项下载 SHA256 匹配，文档相对链接指向可发布的文件或目录。
- 原有固件、构建记录、ZIP、第三方许可证及 manifests 均未改写；只修正文档中的当前构建版本和仓库使用路径。
- 已对工作区可发布文本和 14 个原始 ZIP 内的文本执行常见 GitHub/OpenAI/AWS 凭据及私钥头格式检查，未发现匹配；原有 1 个历史提交的跟踪文本检查也未发现匹配。这是模式检查，不是完整安全审计。
- `git diff --check` 通过。新增 GitHub Actions 工作流在 Windows 上重复执行保留文件、下载哈希、文档链接与差异空白检查。

## 范围

本轮没有修改运行代码、重新编译固件、重新打包交付物、刷写硬件或操作手柄。首页列出的 R5、Switch R1 和 Pico Control 自测数字引用各版本原始验证记录，不是本轮重新执行的测试。Switch 实机识别、R5 新增配置功能实机验收和实际 USB/端到端延迟仍以相应验证文档为准。

GitHub 可见性、名称、远端提交与 Actions 结果需在推送后通过 GitHub 接口另行确认；本文件记录的是提交前本地检查。
