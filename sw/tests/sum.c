/* 1+2+...+10=55。volatile输入让循环确实在CPU上运行，避免常量折叠。 */
static volatile int limit = 10;
static volatile int result;
int main(void) {
    int sum = 0;
    int n = limit;
    for (int i = 1; i <= n; ++i)
        sum += i;
    result = sum;
    return result;
}
