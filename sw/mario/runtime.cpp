#include <stddef.h>
#include <stdint.h>

extern "C" void *memset(void *dst, int c, size_t n) {
    auto p = static_cast<unsigned char *>(dst);
    for (size_t i = 0; i < n; ++i) p[i] = static_cast<unsigned char>(c);
    return dst;
}
extern "C" void *memcpy(void *dst, const void *src, size_t n) {
    auto d = static_cast<unsigned char *>(dst);
    auto s = static_cast<const unsigned char *>(src);
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}
extern "C" void *memmove(void *dst, const void *src, size_t n) {
    auto d = static_cast<unsigned char *>(dst);
    auto s = static_cast<const unsigned char *>(src);
    if (d < s) return memcpy(dst, src, n);
    while (n) { --n; d[n] = s[n]; }
    return dst;
}
alignas(16) static uint8_t arena[65536];
static size_t used;
void *operator new(size_t n) {
    n = (n + 15) & ~size_t(15);
    if (n > sizeof(arena) - used) {
        *(volatile uint32_t *)0x30000008u = 0xbad00001u;
        asm volatile("ebreak");
        for (;;) {}
    }
    void *p = arena + used;
    used += n;
    memset(p, 0, n);
    return p;
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *) noexcept {}
void operator delete[](void *) noexcept {}
void operator delete(void *, size_t) noexcept {}
void operator delete[](void *, size_t) noexcept {}
extern "C" void __cxa_pure_virtual() { asm volatile("ebreak"); for (;;) {} }

// RV32I没有M扩展；提供小型整数软件运算，避免误链接64位libgcc。
extern "C" uint32_t __mulsi3(uint32_t a, uint32_t b) {
    uint32_t r = 0;
    while (b) { if (b & 1) r += a; a <<= 1; b >>= 1; }
    return r;
}
static uint32_t divmod(uint32_t a, uint32_t b, uint32_t *rem) {
    uint32_t q = 0, r = 0;
    if (!b) { *rem = a; return ~0u; }
    for (int i = 31; i >= 0; --i) {
        bool carry = (r >> 31) != 0;
        r = (r << 1) | ((a >> i) & 1);
        if (carry || r >= b) { r -= b; q |= 1u << i; }
    }
    *rem = r;
    return q;
}
extern "C" uint32_t __udivsi3(uint32_t a, uint32_t b) { uint32_t r; return divmod(a, b, &r); }
extern "C" uint32_t __umodsi3(uint32_t a, uint32_t b) { uint32_t r; divmod(a, b, &r); return r; }
extern "C" int32_t __divsi3(int32_t a, int32_t b) {
    uint32_t ua = a < 0 ? 0u - uint32_t(a) : uint32_t(a);
    uint32_t ub = b < 0 ? 0u - uint32_t(b) : uint32_t(b);
    uint32_t q = __udivsi3(ua, ub);
    return int32_t((a < 0) != (b < 0) ? 0u - q : q);
}
extern "C" int32_t __modsi3(int32_t a, int32_t b) {
    uint32_t ua = a < 0 ? 0u - uint32_t(a) : uint32_t(a);
    uint32_t ub = b < 0 ? 0u - uint32_t(b) : uint32_t(b);
    uint32_t r = __umodsi3(ua, ub);
    return int32_t(a < 0 ? 0u - r : r);
}
