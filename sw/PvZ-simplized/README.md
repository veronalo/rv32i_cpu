# 精简植物大战僵尸

## 运行与操作

在 `project/rv32i_cpu` 执行：

```bash
make nvboard PROGRAM=pvz     # RV32I + NVBoard
make pvz-native              # 电脑直接运行同一份精简逻辑
```

点击标题 → START ADVENTURE → 第一关；通关后点 NEXT LEVEL，经过戴夫串场和 Ready/Set/Plant 进入第二关。

| 操作 | 功能 |
| --- | --- |
| 左键点击卡片，再点击草皮 | 种植豌豆射手，消耗 100 阳光；每株需重新选卡 |
| 左键点击阳光 | 收集 25 阳光 |
| 右键或再次点击卡片 | 取消选卡 |
| MENU | 继续、重开当前关或返回主菜单 |
| 戴夫串场左键 / 右键 | 下一页 / 跳过 |

只处理 VGA 游戏画面内的点击。NVBoard 中使用黑边暖黄色 RTL 光标，移出画面恢复系统光标；原生预览使用系统光标。

备用键盘：A/D 选择格子、空格种植、Z 收集阳光、Enter 暂停/继续或跳过串场、R 重置。点击 VGA 可离开 UART 的键盘焦点。关闭窗口或 Ctrl+C 退出。

## 玩法与范围

| 关卡 | 草皮 | 初始阳光 | 僵尸数 |
| --- | --- | --- | --- |
| 1-1 | 单行 | 300 | 6 |
| 1-2 | 三行 | 600 | 9 |

消灭全部僵尸获胜，任意一行被突破即失败。保留豌豆射手、普通僵尸、阳光、标题/菜单和戴夫串场，不包含其他植物、模式、存档或音频。

这是保留部分原作美术、重写整数逻辑的双关版本，不是完整原引擎移植。固定对象池上限为 27 株植物、8 个僵尸、18 个弹丸和 3 个阳光，菜单及串场冻结战斗计时。目标为 30 次逻辑更新/秒，仿真不足时仍会变慢。

## 开发入口

| 路径 | 用途 |
| --- | --- |
| `mini/game.cpp/h` | 关卡、碰撞、界面状态与精灵描述符 |
| `mini/main.cpp` | 裸机入口、资源上传、鼠标 MMIO 与画面提交 |
| `mini/prepare_assets.py` | 离线缩放、骨骼采样、调色板转换 |
| `assets/` | 实际使用的原始素材和字体 |
| `generated/` | 生成的素材头、清单及预览 |
| `mini/native.cpp/render.h` | 原生预览与测试参考，不参与仿真驱动 |

CPU 执行全部游戏逻辑，`pvz_video.v` 合成精灵，`mouse_cursor.v` 叠加光标。320×240 画面放大至 640×480；寄存器、描述符和鼠标协议见[图形外设接口](../../docs/game-graphics.md)。

调试遥测：`0x30000014` 为 Game 状态哈希；`0x30000018` 含击杀数 bits7:0、生成数 bits15:8、模式 bits19:16、第二关标志 bit20、植物数 bits31:24。

`make pvz-assets` 重新生成素材，`make pvz-rom` 只交叉编译，`make nvboard-build PROGRAM=pvz` 只构建仿真器。输出在 `build/pvz-rv32i/`、`build/pvz-nvboard/` 和 `build/pvz-simplized-native/`；`make clean` 不删除 `assets/`。

## 测试

| 命令 | 范围 |
| --- | --- |
| `make pvz-unit` | 种植、战斗、暂停、重开及关卡推进 |
| `make pvz-ui-test` | 标题/菜单/帮助及 SDL 点击链路 |
| `make pvz-video-test` | 精灵合成、裁剪、拥挤场景与扫描线期限 |
| `make pvz-mouse-sdl-test` | 1500 帧单关鼠标输入、状态与 RGB 对照 |
| `make pvz-campaign-sdl-test` | 3200 帧两关与串场的完整输入/状态/RGB 对照 |

测试默认无桌面窗口或使用 SDL dummy。可用 `PVZ_FRAMES` / `PVZ_CAMPAIGN_FRAMES` 调整帧数，但缩短脚本可能无法满足通关检查。`--fps 0` 仅用于吞吐测试，正常游玩保持默认限速。

## 来源

参考代码：[PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable)；美术：[PvZ_Assets](https://github.com/FregD156/PvZ_Assets)。素材提交与使用清单见[assets](assets/README.md)，未确认其具有开放美术授权；本目录 LICENSE/COPYING 不替代原作素材授权。
