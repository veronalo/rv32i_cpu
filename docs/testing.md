# 测试与调试

以下命令均在 `project/rv32i_cpu` 执行，环境依赖见[项目 README](../README.md)。

## 运行裸机程序

```bash
make c-test PROGRAM=sum
make compare PROGRAM=sum
```

`c-test` 交叉编译后在 CPU RTL 中运行，输出 GPR、PC 和结果；`compare` 用同一 ELF 生成的镜像分别运行 RTL 与课程适配版 NEMU，核对最终 32 个 GPR、PC 及预期返回值。它不是逐指令在线 DiffTest，也不比较全部 RAM。

| PROGRAM | 作用 | 预期 a1/x11 |
| --- | --- | --- |
| `smoke` | 启动、调用、循环及窄访存自检 | 0 |
| `sum` | 1+2+…+10 | 55 |
| `fibonacci` | F(12) | 144 |
| `gcd` | gcd(48,18) | 6 |
| `sort` | 升序排序并返回最大值 | 12 |
| `memory` | 跨 64 KiB ROM 读取及 RAM 高地址访存 | 42 |

main 返回值保存在 **a1/x11**，a0/x10 被启动代码置 0，作为 NEMU 退出状态。例如：

```text
RESULT a1/x11 = 55 (0x00000037), MMIO = 55, expected = 55
```

`HIT GOOD TRAP` 仅表示正常停机，计算结果仍需检查 a1。RTL 的 GPR 打印由 `sim/system/soc_tb.sv` 实现，不增加硬件端口。

## NEMU

```bash
make compare PROGRAM=sum NEMU_SRC=/path/to/nemu
make nemu-run PROGRAM=sum
make nemu-debug PROGRAM=sum
```

`NEMU_SRC` 默认指向本机一生一芯工作区；需要主机 C/C++ 工具链和 `libreadline-dev`。适配脚本读取原始教学源码，在 `build/nemu/` 生成独立副本，不修改原工作区。修改课程实现应编辑 `sim/nemu/`，不要编辑生成副本。

调试器支持：

| 命令 | 作用 |
| --- | --- |
| `si 5` | 执行 5 条指令 |
| `c` | 继续执行，单次上限 100 万条 |
| `info r` | 查看寄存器 |
| `x 6 0x10000000` | 查看 6 个 32 位内存字 |
| `q` | 退出 |

课程 NEMU 适配了完整基础指令、寄存器显示和本项目内存/停机约定，不是未经修改的官方参考实现，也不支持游戏的图形/输入 MMIO。

## 添加程序与查看产物

新增 `sw/tests/mytest.c`，通过 main 返回需要观察的结果：

```bash
make c-test PROGRAM=mytest EXPECTED=210
make compare PROGRAM=mytest EXPECTED=210
```

`EXPECTED` 仅用于检查，不改变程序输入。该目标不链接 C 标准库；变量乘除法可能需要额外软件运行库。

ELF、HEX、BIN 和反汇编位于 `build/mario-rv32i/`；对比输出位于 `build/results/<PROGRAM>.rtl.txt` 与 `<PROGRAM>.nemu.txt`。用 `riscv64-linux-gnu-nm -n build/mario-rv32i/mytest.elf` 查询变量地址。

编译后用 `riscv64-linux-gnu-size build/*-rv32i/*.elf` 查看 ROM 与静态 RAM 占用；该统计不包含图形外设存储和运行时栈。

## 测试目标

| 命令 | 范围 |
| --- | --- |
| `make test` | 核心指令、存储器边界、smoke |
| `make core-test` | PC、写回、访存、等待/停机及 40 条指令覆盖 |
| `make memory-test` | ROM/RAM 首末字、高地址、字节掩码与越界 |
| `make examples` | sum、fibonacci、gcd、sort、smoke |
| `make lint` / `make board-lint` | 核心 / 默认板级 RTL 检查 |
| `make synth` | CPU Core 综合，依赖 Yosys |

游戏测试与按键说明：[Mario](mario.md)、[PvZ](../sw/PvZ-simplized/README.md)、[Tetris](../sw/tetris/README.md)。游戏持续运行，使用关闭窗口或 Ctrl+C 退出，不会自动返回 main。

## 波形与清理

```bash
make c-test PROGRAM=sum
vvp build/soc_tb +rom=build/mario-rv32i/sum.hex +expected=55 +trace
gtkwave build/soc.vcd
```

观察 `dut.u_core` 的 PC、指令和 commit 信号；GTKWave 需另行安装。

`make clean` 清理 `build/`、游戏生成素材和 Python 缓存，保留源素材及已有 Mario 原生配置。之后会重新编译，包括 NEMU 副本与 Mario PGO 训练。
