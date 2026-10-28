/* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 * SPDX-License-Identifier: MulanPSL-2.0
 * NEMU course adapter. License: http://license.coscl.org.cn/MulanPSL2 */
#include <isa.h>
#include "local-include/reg.h"

const char *regs[] = {
  "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};
void isa_reg_display(void) {
  printf("--- NEMU registers (hex / signed decimal) ---\n");
  printf("PC = 0x%08x\n", cpu.pc);
  for (int i = 0; i < 32; ++i)
    printf("x%02d = 0x%08x  %d  (%s)\n", i, cpu.gpr[i], (int32_t)cpu.gpr[i], regs[i]);
  printf("RESULT a1/x11 = %d (0x%08x)\n", (int32_t)cpu.gpr[11], cpu.gpr[11]);
}
word_t isa_reg_str2val(const char *s, bool *success) {
  if (*s == '$') ++s;
  *success = true;
  if (strcmp(s, "pc") == 0) return cpu.pc;
  for (int i = 0; i < 32; ++i) {
    char xname[8];
    snprintf(xname, sizeof(xname), "x%d", i);
    if (strcmp(s, regs[i]) == 0 || strcmp(s, xname) == 0) return cpu.gpr[i];
  }
  *success = false;
  return 0;
}
