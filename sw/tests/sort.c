/* 排序后为{-3,-1,0,5,7,12}，验证顺序和总和后返回最大值12。 */
static volatile int values[] = {7, -3, 12, 0, 5, -1};
static volatile int result;
int main(void) {
    for (int i = 0; i < 6; ++i)
        for (int j = i + 1; j < 6; ++j)
            if (values[i] > values[j]) {
                int tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
    int sum = 0;
    for (int i = 0; i < 6; ++i) {
        if (i && values[i - 1] > values[i]) return -1;
        sum += values[i];
    }
    if (sum != 20 || values[0] != -3) return -2;
    result = values[5];
    return result;
}
