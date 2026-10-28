# 俄罗斯方块

## 运行与操作

在 `project/rv32i_cpu` 执行：

```bash
make nvboard PROGRAM=tetris     # RV32I + NVBoard
make tetris-native              # 电脑直接运行，不仿真 CPU
```

| 按键 | 功能 |
| --- | --- |
| Enter | 开始、暂停/继续、失败后重开 |
| A/D 或左右方向键 | 移动，长按自动重复 |
| W / 上方向键、Z | 顺时针、逆时针旋转 |
| S / 下方向键 | 加速下落 |
| 空格 | 直接落底 |
| R | 重新开始 |

使用 W 旋转后先松开，再按空格：共享键盘中 W 带 UP/JUMP 别名，游戏对旋转优先处理。上方向键无此别名。鼠标不参与游戏；按键失效时点击 VGA 离开 UART 焦点。关闭窗口或 Ctrl+C 退出，原生版还支持 Esc。

## 规则

- 10×20 棋盘，七种方块按七袋洗牌，提供下一块和空心落点预览；重开使用固定种子。
- 单次消除 1/2/3/4 行得 100/300/500/800 分，乘当前等级；每累计 10 行升级并加速。
- 软降每格 1 分，直接落底每格 2 分。一级约每秒自动下降一格，目标 30 次逻辑更新/秒。
- 使用简单墙边/地板旋转修正；无完整 SRS、锁定延迟、暂存、连击、音频或存档。

## 开发入口

| 文件 | 用途 |
| --- | --- |
| `game.cpp/h` | 棋盘、旋转、消行、计分及画面布局 |
| `main.cpp` | 裸机入口、按键读取与画面提交 |
| `art.h` | 自绘字模和调色板，无外部素材依赖 |
| `native.cpp/render.h` | 原生预览和测试参考 |
| `demo.h` | 固定测试按键，不读取实时游戏状态 |

逻辑由 RV32I CPU 执行，`tile_video.v` 将 40×30 格字符/色块生成 640×480 VGA 画面；驱动不执行游戏逻辑。寄存器编码见[图形外设接口](../../docs/game-graphics.md)。

调试遥测：`0x30000014` 为完整游戏状态哈希；`0x30000018` 为模式 bits31:24、锁定块数 bits23:12、消行数 bits11:0。模式 0/1/2/3 对应标题、游戏、暂停、失败。

`make tetris-rom` 只交叉编译，`make nvboard-build PROGRAM=tetris` 只构建仿真器。产物分别位于 `build/tetris-rv32i/`、`build/tetris-native/`、`build/tetris-nvboard/`，可由 `make clean` 清理。

## 测试

| 命令 | 范围 |
| --- | --- |
| `make tetris-unit` | 七袋、碰撞、旋转、计分、四行消除、重开及压力测试 |
| `make tetris-lint` | Tetris 配置的板级 RTL 检查 |
| `make tetris-test` | 600 帧 CPU/原生状态哈希与完整 RGB 对照，包含消行 |
| `make tetris-ps2-test` | 同上，经过 SDL→NVBoard→PS/2→RTL |

测试不打开桌面窗口，输出在 `build/tetris-tests/`，可用 `TETRIS_FRAMES` 调整帧数。实际游戏速度受仿真吞吐限制，不保证所有主机稳定 30 fps。
