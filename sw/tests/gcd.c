/* gcd(48,18)=6。用相减法避免除法，适合没有M扩展的RV32I。 */
static volatile unsigned int input_a = 48, input_b = 18;
static volatile unsigned int result;
int main(void) {
    unsigned int a = input_a, b = input_b;
    if (a == 0) return (int)b;
    if (b == 0) return (int)a;
    while (a != b) {
        if (a > b) a -= b;
        else b -= a;
    }
    result = a;
    return (int)result;
}
