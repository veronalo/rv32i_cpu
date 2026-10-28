/* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 * SPDX-License-Identifier: MulanPSL-2.0
 * Minimal NEMU course debugger. License: http://license.coscl.org.cn/MulanPSL2 */
#include <isa.h>
#include <cpu/cpu.h>
#include <memory/paddr.h>
#include <readline/readline.h>
#include <readline/history.h>

static bool batch;
void sdb_set_batch_mode(void) { batch = true; }
void init_sdb(void) {}
void sdb_mainloop(void) {
  if (batch) { cpu_exec(1000000); return; }
  for (;;) {
    char *line = readline("(nemu) ");
    if (!line) return;
    if (*line) add_history(line);
    char *cmd = strtok(line, " \t");
    if (!cmd) { free(line); continue; }
    if (strcmp(cmd, "q") == 0) { free(line); return; }
    else if (strcmp(cmd, "c") == 0) cpu_exec(1000000);
    else if (strcmp(cmd, "si") == 0) {
      char *n = strtok(NULL, " \t");
      cpu_exec(n ? strtoul(n, NULL, 0) : 1);
    } else if (strcmp(cmd, "info") == 0) {
      char *what = strtok(NULL, " \t");
      if (what && strcmp(what, "r") == 0) isa_reg_display();
      else puts("Usage: info r");
    } else if (strcmp(cmd, "x") == 0) {
      char *count = strtok(NULL, " \t"), *address = strtok(NULL, " \t");
      if (!count || !address) puts("Usage: x N ADDRESS (N 32-bit words)");
      else {
        unsigned n = strtoul(count, NULL, 0);
        paddr_t addr = strtoul(address, NULL, 0);
        for (unsigned i = 0; i < n && i < 1024; ++i)
          printf("0x%08x: 0x%08x\n", addr + i * 4, paddr_read(addr + i * 4, 4));
      }
    } else puts("Commands: c | si [N] | info r | x N ADDRESS | q");
    free(line);
  }
}
