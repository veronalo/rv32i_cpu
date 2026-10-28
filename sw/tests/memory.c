/* 扩容回归：读取超出旧64KiB边界的ROM常量，并读写RAM高地址。
 * 本程序单独运行，这些固定RAM测试地址未被其他变量或栈占用。
 */
static const unsigned char rom_probe[0x10004] = {
    [0] = 0x12, [0x10000] = 0x34, [0x10003] = 0x56
};

int main(void) {
    const volatile unsigned char *rom = rom_probe;
    volatile unsigned int *low = (volatile unsigned int *)0x10010000u;
    volatile unsigned int *mid = (volatile unsigned int *)0x10200000u;
    volatile unsigned int *high = (volatile unsigned int *)0x10300000u;
    if (rom[0] != 0x12 || rom[0x10000] != 0x34 || rom[0x10003] != 0x56)
        return -1;
    *low = 0x11223344u;
    *mid = 0xa5a55a5au;
    *high = 0x89abcdefu;
    if (*low != 0x11223344u || *mid != 0xa5a55a5au || *high != 0x89abcdefu)
        return -2;
    // 再测试高地址下的字节和半字通道，确认没有地址截断/别名。
    volatile unsigned char *bytes = (volatile unsigned char *)high;
    volatile unsigned short *halves = (volatile unsigned short *)high;
    bytes[1] = 0x80;
    halves[1] = 0x7654;
    if (*high != 0x765480efu || *low != 0x11223344u || *mid != 0xa5a55a5au)
        return -3;
    return 42;
}
