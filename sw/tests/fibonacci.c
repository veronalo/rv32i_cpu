/* F(0)=0, F(1)=1, F(12)=144。测试循环和连续的数据依赖。 */
static volatile unsigned int index = 12;
static volatile unsigned int result;
__attribute__((noinline)) static unsigned int fib(unsigned int n) {
    unsigned int a = 0, b = 1;
    while (n--) {
        unsigned int next = a + b;
        a = b;
        b = next;
    }
    return a;
}
int main(void) {
    result = fib(index);
    return (int)result;
}
