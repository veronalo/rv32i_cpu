/* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 * SPDX-License-Identifier: MulanPSL-2.0
 * NEMU course memory adapter. License: http://license.coscl.org.cn/MulanPSL2 */
#include <memory/paddr.h>
#include <isa.h>

// 与课程SoC一致：4MiB ROM + 4MiB RAM；不用连续数组填地址空洞。
// CONFIG_MSIZE配置ROM大小，也供monitor检查镜像容量。
#define COURSE_RAM_BYTES (4u * 1024u * 1024u)
static uint8_t rom[CONFIG_MSIZE], ram[COURSE_RAM_BYTES];
static uint32_t status;
uint8_t *guest_to_host(paddr_t addr) {
  if (addr < sizeof(rom)) return rom + addr;
  if (addr >= 0x10000000u && addr - 0x10000000u < sizeof(ram))
    return ram + addr - 0x10000000u;
  panic("No backing memory at " FMT_PADDR, addr);
}
paddr_t host_to_guest(uint8_t *ptr) {
  uintptr_t p = (uintptr_t)ptr, r = (uintptr_t)rom, d = (uintptr_t)ram;
  if (p >= r && p - r < sizeof(rom)) return (paddr_t)(p - r);
  if (p >= d && p - d < sizeof(ram)) return 0x10000000u + (paddr_t)(p - d);
  panic("Pointer is outside course memory");
}
void init_mem(void) {
  memset(rom, 0, sizeof(rom));
  memset(ram, 0, sizeof(ram));
  status = 0;
  Log("Course map: ROM=0x00000000..0x003fffff RAM=0x10000000..0x103fffff (4MiB each)");
}
static uint8_t *map_access(paddr_t addr, int len) {
  Assert(len == 1 || len == 2 || len == 4, "Unsupported access width");
  Assert((addr & (len - 1)) == 0, "Misaligned data address " FMT_PADDR, addr);
  if (addr <= sizeof(rom) - len) return rom + addr;
  if (addr >= 0x10000000u && addr - 0x10000000u <= sizeof(ram) - len)
    return ram + addr - 0x10000000u;
  return NULL;
}
word_t paddr_read(paddr_t addr, int len) {
  uint8_t *p = map_access(addr, len);
  uint32_t value = 0;
  if (p) for (int i = 0; i < len; ++i) value |= (uint32_t)p[i] << (8 * i);
  // 与当前SoC一致：未映射/测试状态寄存器读取0。
  return value;
}
void paddr_write(paddr_t addr, int len, word_t data) {
  uint8_t *p = map_access(addr, len);
  if (addr == 0x30000008u && len == 4) {
    status = data;
    printf("MMIO result = %d (0x%08x)\n", (int32_t)status, status);
  } else if (p && addr >= 0x10000000u) {
    for (int i = 0; i < len; ++i) p[i] = data >> (8 * i);
  }
  // ROM和未映射地址写入忽略，与SoC一致。
}
