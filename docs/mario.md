# 超级马里奥

## 运行与操作

在 `project/rv32i_cpu` 执行：

```bash
make nvboard PROGRAM=mario     # RV32I + NVBoard
make mario-native             # 电脑直接运行，不仿真 CPU
```

首次 NVBoard 构建会进行 PGO 训练；`make nvboard-base` 可运行未训练的版本，`make nvboard-build PROGRAM=mario` 仅构建普通仿真器。

| 按键 | 操作 |
| --- | --- |
| A/D 或左右方向键 | 移动 |
| W / 空格 / X | 跳跃 |
| Z | 加速、发射火球 |
| Enter | 开始、确认、暂停/继续 |
| Tab / Backspace | Select |
| R | 重置 |

NVBoard 通过关闭窗口或 Ctrl+C 退出；原生版还支持 Esc。按键失效时，点击 VGA 画面离开 UART 输入框。

## 素材准备

已有 `sw/mario/assets/smb-litenes.chr` 时可直接构建。缺少缓存时，从本地 C 数组或对应 iNES 文件导入一次：

```bash
make mario-assets MARIO_ROM_C=/path/to/litenes/src/mario-rom.c
# 或：
make mario-assets MARIO_ROM=/path/to/smb.nes
```

`make mario-assets` 从缓存重新生成素材；`make mario` 只交叉编译游戏。原生入口自动更新 `graphics.bin`，保留已有 `build/mario-native/smbc.conf`。

来源及授权见[素材说明](../sw/mario/assets/CREDITS.md)，移植结构见[PORTING.md](../sw/mario/PORTING.md)。

## 速度与显示

RV32I 版每次提交画面前推进两个原始逻辑 tick，目标为 30 fps 显示、60 tick/s；每组共用一次按键采样。原生版按 60 Hz 运行。受伤闪烁采用两逻辑帧亮、两逻辑帧灭，避免抽帧后人物一直隐藏。

默认按游戏提交数限速到 30 fps，而非按 VGA 扫描数。主机吞吐不足时游戏仍会变慢，调整模拟时间刻度不能加速 RTL 求值。关闭限速仅用于测速：

```bash
make nvboard PROGRAM=mario NVBOARD_ARGS='--fps 0'
```

逻辑画面为 256×240，放大到 512×480，居中显示在 640×480 VGA 中。显存和寄存器布局见[图形外设接口](game-graphics.md)。

调试遥测：`0x30000014` 从低到高四个字节为 mode、playerX、playerY、page；`0x30000018` 的低三个字节为 scrollX、world、level。

## 构建与输入配置

NVBoard 需要已构建的外部库，路径可覆盖：

```bash
make nvboard PROGRAM=mario NVBOARD_HOME=/path/to/nvboard
```

默认 SDL 显示后端为 software，可通过 `NVBOARD_RENDERER=auto` 或 `gpu` 切换；它只影响窗口显示，不把 CPU/RTL 仿真交给 GPU。桌面运行需要可用的显示环境。

项目输入适配层仅在 UART 获得焦点时启用文本输入，并过滤游戏按键的系统自动重复事件；长按/松键由 RTL 保持，外部 NVBoard 源码不修改。

| 目录 | 产物 |
| --- | --- |
| `build/mario-rv32i/` | ELF、HEX、BIN、反汇编 |
| `build/mario-native/` | 原生程序、图形容器与配置 |
| `build/nvboard/`、`build/nvboard-pgo/` | 普通 / PGO 仿真器 |

## 测试

| 命令 | 范围 |
| --- | --- |
| `make mario-test` | 300 帧 CPU 与原生参考状态对照，可设 `GAME_FRAMES` |
| `make nvboard-test` | 330 帧 SDL→PS/2→RTL 输入链路 |
| `make peripheral-test` | PS/2、调色板及 VGA 逐像素对照 |
| `make mario-art-test` | 字形、角色姿态与连续可见性 |
| `make mario-blink-test` | 受伤闪烁和 PPU/OAM 提交相位 |
| `make controls-test` | 原生/RTL 键位、组合键及别名松键 |

自动演示可用 `NVBOARD_ARGS='--ps2-demo'`；追加 `--frames 330 --screenshot build/demo.ppm` 可限时截图。`--demo` 使用测试按键口，`--ps2-demo` 经过真实 PS/2 接收链路。

运行时游戏逻辑由 RV32I CPU 执行，图块和精灵由 RTL 合成；主机参考程序只用于测试。RV32I 版不提供音频、操作系统或完整 C++ 标准库；测试不等于完整通关或实际 FPGA 上板验证。
