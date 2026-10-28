    .section .text.start
    .globl _start
_start:
    la sp, _stack_top

    # 从ROM复制.data初始值到RAM。
    la t0, _data_load
    la t1, _data_start
    la t2, _data_end
1:
    bgeu t1, t2, 2f
    lbu t3, 0(t0)
    sb t3, 0(t1)
    addi t0, t0, 1
    addi t1, t1, 1
    j 1b
2:
    la t1, _bss_start
    la t2, _bss_end
3:
    bgeu t1, t2, 4f
    sb zero, 0(t1)
    addi t1, t1, 1
    j 3b
4:
    call main
    # 统一的对比约定：a1保留main返回的计算结果，a0表示执行环境退出状态。
    mv a1, a0
    lui t0, 0x30000
    sw a1, 8(t0)
    li a0, 0
    ebreak
5:
    j 5b
