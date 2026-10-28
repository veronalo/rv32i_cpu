# Mario 移植接口

运行与按键见[Mario 使用说明](../../docs/mario.md)。本页用于修改移植代码。

## 文件分工

| 路径 | 职责 |
| --- | --- |
| `main.cpp/platform.h` | 裸机循环、按键读取、MMIO 提交 |
| `timing.h` | 每次显示提交推进两个 60 Hz 逻辑 tick |
| `runtime.cpp` | 内存函数、64 KiB 线性分配区、软件整数乘除法 |
| `include/` | 移植所需的最小 C++/Boost 头文件 |
| `make_chr.py` | 导入 8 KiB CHR，保持图块编号与 2bpp 索引 |
| `prepare_native.py` | 打包原生图形容器并保留已有配置 |
| `vendor/SuperMarioBros-C/` | 上游游戏逻辑与本地适配 |

## 第三方源码中的适配

- `MemoryAccess.cpp`：立即数包装对象保存自己的副本，避免悬空指针。
- `SMBEngine.cpp`：确定性初始化；`SMB_NO_AUDIO` 关闭音频依赖，`SMB_RV32I` 使用 CHR 指针。
- `PPU.cpp/hpp`：RV32I 版提交 PPU 状态，原生版保留软件渲染；提供测试状态导出。
- `SMB.cpp`：受伤闪烁改为两帧显示、两帧隐藏，适配 30 fps 提交。
- `Main.cpp/HostControls.hpp`：原生 WASD、跳跃、菜单与组合键处理。

RV32I 构建不使用上游 SDL 主循环、配置解析和 APU；无操作系统或完整 C++ 标准库。游戏驱动 `sim/nvboard_main.cpp` 不执行游戏逻辑；主机参考仅供测试。

## 来源

上游：[SuperMarioBros-C](https://github.com/MitchellSternke/SuperMarioBros-C)，基线提交 `5bb920506a869cdc1fe2ef888a6526a1cf991630`。第三方代码和图形素材的授权分别处理，CHR 来源见[素材说明](assets/CREDITS.md)。

`vendor/SuperMarioBros-C/` 按普通源码随本工程管理，不使用 Git 子模块。
