# R5 配置协议 v1

USB 2E8A:10C8，XUSB 接口 0 不变。HID 接口 1，Usage Page FF00 / Usage 1。Report ID 1 是原 P2W 电量，保持只读；ID 2 是配置 Feature SET，ID 3 是配置 Feature GET。全部 HID 报告含 ID 为 32 字节，沿用系统 HID 驱动。bcdDevice 0500，刷机后重新插入 USB。上位机通过 INFO 握手判断配置支持。

## 命令与回复

以下偏移不含 Report ID，共 31 字节，小端整数。每次 SET ID 2 后 GET ID 3；回复必须匹配操作、token、offset 和 CRC。

| 偏移 | 字段 |
|---|---|
| 0–2 | ASCII P2C |
| 3 | 版本 1 |
| 4 | 操作码 |
| 5–6 | 非零 uint16 token；BEGIN/PUT/APPLY/CANCEL 共用事务 token |
| 7 | 配置偏移 |
| 8 | 数据长度，最多 19 |
| 9 | 命令保留 0；回复状态 |
| 10–28 | 数据；空余补 0 |
| 29–30 | CRC16-CCITT-FALSE，前 29 字节，初值 FFFF、多项式 1021 |

操作：INFO=1，READ=2，BEGIN=3，PUT=4，APPLY=5，SAVE=6，DEFAULTS=7，CANCEL=8。

状态：OK=0，BUSY=1，BAD_PACKET=2，RANGE=3，INVALID=4，IO=5，UNSUPPORTED=6，NO_TRANSACTION=7，TOKEN=8，NEEDS_IDLE=9。

INFO 回复长度10：数据0=schema1，1=能力位1映射/2死区与反转/4宏/8Flash/16马达缩放（31），2=配置长度128，3=最多16步，4–7=Flash代次uint32，8=暂存事务，9=保存中。

READ 按 offset/length 返回当前 RAM 配置片段，拼接后验证配置 CRC。跨请求被其他客户端改动时 CRC/回读核对可发现不一致。

BEGIN 创建128字节事务；另一 token 已占用时 BUSY。PUT 任意有效范围，设置覆盖标记。APPLY 仅在128字节全部到齐、CRC与语义有效时整体替换 RAM，取消宏并发送一次中性输入。5秒无事务活动自动过期。最后一条相同命令重复提交返回原回复，防止丢 ACK 后重复 APPLY/SAVE。

SAVE 数据长度必须4，数据为回读配置 offset124 的 CRC32。当前 RAM 不匹配返回 INVALID；有暂存事务返回 BUSY；否则异步 BUSY。平台检查初始化完成、无手柄、无配对/设备槽、无清配对流程，不满足返回 NEEDS_IDLE。合法保存关闭 incoming/扫描/HCI，HCI OFF 后写 Flash 并验证，回报 OK和代次；失败/关闭超时回报 IO。关闭无线后的成功或失败均在750ms后整机重启；等待期间不接受其他配置命令，保持 SAVE 回复。Bluepad32 初始化序号不会随 HCI OFF 自动复位，禁止原地 HCI ON，必须经整机启动恢复。

DEFAULTS 只将默认配置应用 RAM，不持久化；上位机默认按钮只改本地编辑区。CANCEL 取消指定事务。

## 配置内容（128 字节）

| 偏移 | 字段 |
|---|---|
| 0–3 | ASCII PCF1 |
| 4 | schema 1 |
| 5 | 反转位 LX=1、LY=2、RX=4、RY=8 |
| 6 / 7 | 左 / 右各轴死区百分比，0–50 |
| 8 / 9 | 强 / 弱马达缩放百分比，0–100 |
| 10 | 宏物理触发键索引，255关闭 |
| 11 | 使用步骤数，0–16 |
| 12–26 | 15个物理键到目标键的索引，255禁用 |
| 27 | 保留0 |
| 28–123 | 16×6字节：buttons uint16、duration_ms uint16、LT uint8、RT uint8 |
| 124–127 | CRC32 IEEE 前124字节，反射多项式 EDB88320，初始/最终异或 FFFFFFFF |

按键索引：0上/1下/2左/3右/4Start/5Back/6LS/7RS/8LB/9RB/10Guide/11A/12B/13X/14Y。对应 XUSB 按键位：0001/0002/0004/0008/0010/0020/0040/0080/0100/0200/0400/1000/2000/4000/8000；0800不可用。未使用步骤必须全零。

宏触发为映射前物理上升沿；有步骤时占用触发键。宏按键输出在映射后合并，LT/RT取物理与宏较大值，摇杆保持实时输入。持续按住不循环，执行中按下不重启。总时长≤30000ms，每步20–5000ms。宏按时间推进，不依赖新手柄事件；断线、USB休眠、新配置取消。默认身份映射、死区/反转0、马达100%、无宏，保持 R4.2 原输入行为。

## Flash 日志

Pico 2 W 4MB Flash：配置A=offset3FB000，B=3FC000，各4096字节；Bluetooth两银行仍为3FD000/3FE000；3FF000保留给RP2350 erratum。链接与 UF2 检查限制应用低于103FB000。不得用全片擦除或包含配置/配对区的数据块升级。

每银行第一页256字节：PCJ1（0–3）、代次uint32（4–7）、配置128（8–135）、CRC32前136字节（136–139）、DONE（140–143），其余FF。启动选校验有效且代次更新的一页，均无效则默认。保存擦写另一银行，回读有效后切换；中断保留前一有效配置。相同已持久化内容跳过擦写。提交页来自 SRAM，通过 SDK `flash_safe_execute` 和 RAM callback 写入，应用单 core0，CORE1_SAFE=1。配置保存不擦配对区，长按 BOOTSEL 清配对不会清配置。
