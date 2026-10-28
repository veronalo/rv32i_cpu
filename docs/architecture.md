# CPU 与 SoC 架构

## 使用入口

- 顶层：`rtl/nvboard_top.v` → `rtl/soc/soc_top.v` → `rtl/soc/core/rv32i_core.v`
- 检查核心：`make core-test`、`make lint`
- 运行与调试：[测试说明](testing.md)
- 显示与鼠标寄存器：[图形外设接口](game-graphics.md)

CPU Core 不包含 ROM、RAM 或外设；SoC 负责存储器和 MMIO 译码，板级顶层连接 NVBoard 引脚。CPU 时钟由仿真驱动或 testbench 提供。

## 数据通路

```text
PC → 指令 ROM → 译码器 → GPR / 立即数 → ALU → LSU / 数据存储器
 ↑                                      │             │
 └──────── 下一 PC 选择                  └── 写回选择 → GPR
              ↑
          分支比较器
```

核心采用单周期组合数据通路，PC、GPR 在上升沿更新，x0 恒为 0。控制信号定义在 `rv32i_defs.vh`。

| 模块 | 职责 |
| --- | --- |
| `pc` | 保存当前 PC |
| `opcode_decoder` | 检查编码并生成控制信号 |
| `imm_gen` | 提取并符号扩展立即数 |
| `gpr` | 32×32 寄存器堆，双读单写 |
| `alu` | 加减、比较、移位与逻辑运算 |
| `branch_comp` | 判断条件分支 |
| `load_store_unit` | 窄访存、扩展、写掩码与对齐检查 |
| `rv32i_core` | 连接模块，选择下一 PC 和写回值 |

下一 PC 默认为 PC+4；JAL 为 PC+立即数，JALR 为 `(rs1+立即数)&~1`，条件分支成立时为 PC+立即数。写回来源为 ALU、访存结果或 PC+4。跳转目标必须 4 字节对齐，JALR 仅清除 bit0；不跳转的分支不检查潜在目标。

## 存储器接口

取指和数据读均要求本周期组合返回，不提供通用 ready/valid 握手。

| 信号 | 约定 |
| --- | --- |
| `imem_addr/rdata` | 指令地址与 32 位指令 |
| `dmem_addr` | 原始字节地址 |
| `dmem_rdata` | 地址向下对齐到 4 字节后的完整字 |
| `dmem_wdata/wstrb` | 按字节通道对齐的数据与 4 位写掩码 |
| `dmem_ren` | 真实读请求；`|dmem_wstrb` 为写请求 |

例如向地址 `0x101` 写入字节 `0xab`，总线输出 `wdata=0x0000ab00`、`wstrb=0010`。半字要求 2 字节对齐，字要求 4 字节对齐，字节访问不限。

`WITH_CLOCK_ENABLE=1` 时，`clock_enable=0` 保持 PC/GPR、不提交指令且不写存储器，但保留读请求。SoC 用它等待图形提交寄存器的 pending 清零，VGA 仍继续运行；独立核心默认关闭此选项。

## 地址与裸机启动

| 地址 | 内容 |
| --- | --- |
| `0x00000000–0x003fffff` | 4 MiB ROM，指令端和数据端可读 |
| `0x10000000–0x103fffff` | 4 MiB RAM |
| `0x20000000` 起 | 图形 MMIO，布局随游戏后端选择 |
| `0x30000000` | 按键位图 |
| `0x30000008` | 写入测试结果 |
| `0x3000000c` | VGA 帧计数 |
| `0x30000010/14/18` | 软件写入的游戏帧号与状态 |
| `0x30000020/24` | PvZ 鼠标点击数据与确认 |

按键 bit0..8 依次为 A、B、Select、Start、Up、Down、Left、Right、Reset。W 同时映射 Up/A；空格只映射 A。

`sw/start.S` 设置 sp=`0x10400000`，复制 `.data`、清零 `.bss` 并调用 main。返回后将结果保存至 a1 和测试 MMIO，将 a0 置 0 后执行 EBREAK。ROM 镜像为小端字节 HEX，由 `$readmemh` 加载。

未映射地址读 0、写入忽略。ROM 通过两个相同镜像实例提供指令/数据读口。修改容量时需同步 RTL、`sw/link.ld` 和 `sim/nemu/` 的内存配置。

## 停机与提交

| `halt_reason` | 含义 |
| --- | --- |
| 0 | 运行中 |
| 1 / 2 | EBREAK / ECALL |
| 3 | 非法指令 |
| 4 / 5 | 指令 / 数据地址未对齐 |

停机后 PC 保持在结束或错误指令处，直到 reset；结束或错误指令不写 GPR/RAM。这是项目调试约定，不是标准异常处理机制。

`commit_*` 在上升沿后表示本次处理的指令：

- `commit_valid`：正常指令及 ECALL/EBREAK 为 1；fault、等待和停机后为 0。
- `commit_pc/inst`：执行前的 PC 和指令。
- `commit_rd_wen/rd/rd_data`：实际写回信息，写 x0 时 wen=0。
- valid=0 时不得将其余字段视为新提交。

## 实现边界

覆盖 RV32I 40 条基础指令；FENCE 为 NOP，ECALL/EBREAK 为停机。无 M/C 扩展、CSR、特权态、Cache、流水线或操作系统。

当前存储器为组合读模型。实际 FPGA 同步 BRAM 需要等待控制和存储结构适配，不能直接替换；`make synth` 只综合 CPU Core，不代表整个 SoC 已完成上板验证。
