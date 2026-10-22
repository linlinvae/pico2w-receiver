# 下载、版本与回退

本页列出已有交付物。本次仓库整理不改变固件版本、二进制或历史 ZIP 内容。GitHub 的 **Code → Download ZIP** 是整个源码仓库；只使用接收器时下载下面的 UF2 和所需上位机即可。

## 当前入口

| 文件 | 用途 |
| --- | --- |
| [pico2w-xinput-r5.uf2](../pico2w/firmware/pico2w-xinput-r5.uf2?raw=true) | Windows XInput R5 固件 |
| [pico2w-xinput-r5-package.zip](../pico2w/releases/pico2w-xinput-r5-package.zip?raw=true) | R5 原始完整交付包，内含文档反映交付当时状态 |
| [PicoControl-v0.3-windows.zip](../pico2w/host/releases/PicoControl-v0.3-windows.zip?raw=true) | 当前 Windows 上位机，完整解压后运行 |
| [PicoControl-v0.3-source.zip](../pico2w/host/releases/PicoControl-v0.3-source.zip?raw=true) | V0.3 原始上位机源码包 |
| [pico2w-switch-r1.uf2](../pico2w/firmware/pico2w-switch-r1.uf2?raw=true) | 独立 Switch USB R1 实验固件，实机待验证 |
| [pico2w-switch-r1-package.zip](../pico2w/releases/pico2w-switch-r1-package.zip?raw=true) | Switch R1 原始完整交付包 |

最新使用方式以[仓库首页](../README.md)、[Pico 文档](../pico2w/README.md)及 [Switch 文档](../pico2w/docs/switch-usb.md)为准。Windows R5 与 Switch R1 需分别刷入，不能同时作为两种 USB 输出。

## 校验下载

在 PowerShell 中执行 `Get-FileHash .\文件名 -Algorithm SHA256`，与下表或固件 `*-build-info.json` 比对。

| 文件 | SHA256 |
| --- | --- |
| [pico2w-xinput-r5.uf2](../pico2w/firmware/pico2w-xinput-r5.uf2) | `4B0E0AE94664D4D5CBEA9EDB208C11955FA925530015E087A4F3214B6BA9497E` |
| [pico2w-xinput-r5-package.zip](../pico2w/releases/pico2w-xinput-r5-package.zip) | `915173BD67135B6FDE3E2D8B485BFCB348571FCB7857115A09E92E62AC8F9912` |
| [PicoControl-v0.3-windows.zip](../pico2w/host/releases/PicoControl-v0.3-windows.zip) | `23E8FCDF6FCDB1E7EBB000C11E1449D5ADC64E6E1E435DE5EE615698CBACD6B3` |
| [PicoControl-v0.3-source.zip](../pico2w/host/releases/PicoControl-v0.3-source.zip) | `9BDEC13C33C462D6DCAA94FAB50A2B3C135898E199C757882F9AF2A9ADC54219` |
| [pico2w-switch-r1.uf2](../pico2w/firmware/pico2w-switch-r1.uf2) | `E96CE86612F9AA40E7EB75192932237D9864474D286A282BA74CBCFC91F88243` |
| [pico2w-switch-r1-package.zip](../pico2w/releases/pico2w-switch-r1-package.zip) | `6AC6A95C049387A5F72E5F6B36EA23FC2516F5D91B14E8FFCF24089C30553E06` |

哈希只验证文件是否与当前交付一致，不证明硬件功能已经实测。

## 回退与历史资料

- Windows 固件 R1、R2、R3、R4、R4.1、R4.2、R5：[firmware](../pico2w/firmware)；对应[完整包](../pico2w/releases)。
- Pico Control V0.1、V0.2、V0.3：[host/releases](../pico2w/host/releases)。
- 版本变化：[CHANGELOG](../pico2w/CHANGELOG.md)；实测与模拟范围：[VALIDATION](../pico2w/VALIDATION.md)。
- CH582F/CH592 与 ESP32 旧方案：见 [receiver](../receiver/README.md) 和 [switchpro](../switchpro/README.md)。

按 BOOTSEL 刷回所需 UF2。正常刷入本项目 UF2 保留配对和 R5 配置；运行中长按 BOOTSEL 会清除配对。历史版本的功能和已知问题请查看对应记录。
