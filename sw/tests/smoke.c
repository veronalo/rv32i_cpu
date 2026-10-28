/* 裸机集成测试：.data复制、.bss清零、ROM常量、栈、函数、循环和窄访存。 */
typedef unsigned int u32;
static volatile int values[] = {9, -3, 7, 0, 5, -8, 2, 1};
static const unsigned char message[] = {0x01, 0x7f, 0x80, 0xff};
static volatile u32 zero_area[8];
static volatile signed char bytes[4];
static volatile short halves[2];

__attribute__((noinline)) static u32 fib(u32 n) {
    u32 a = 0, b = 1;
    while (n--) {
        u32 next = a + b;
        a = b;
        b = next;
    }
    return a;
}

int main(void) {
    const volatile unsigned char *rom_message = message;
    for (int i = 0; i < 8; ++i)
        if (zero_area[i] != 0) return 1;
    if (rom_message[2] != 0x80 || rom_message[3] != 0xff) return 2;

    for (int i = 0; i < 8; ++i)
        for (int j = i + 1; j < 8; ++j)
            if (values[j] < values[i]) {
                int tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
    int sum = 0;
    for (int i = 0; i < 8; ++i) {
        if (i && values[i] < values[i - 1]) return 3;
        sum += values[i];
    }
    if (sum != 13 || values[0] != -8 || values[7] != 9) return 4;
    if (fib(12) != 144) return 5;
    bytes[0] = -128; bytes[1] = 127; bytes[2] = -1; bytes[3] = 42;
    halves[0] = -32768; halves[1] = 32767;
    if (bytes[0] != -128 || bytes[1] != 127 || bytes[2] != -1 || bytes[3] != 42) return 6;
    if (halves[0] != -32768 || halves[1] != 32767) return 7;
    zero_area[0] = fib(10);
    return zero_area[0] == 55 ? 0 : 8;
}
