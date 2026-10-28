// CPU级测试：独立的软件执行模型逐条核对PC、写回和访存总线。
// 所有指令由测试自身编码，不依赖交叉编译器是否恰好生成某条指令。
#include "Vrv32i_core.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static std::string hex(uint32_t n) {
    std::ostringstream s;
    s << "0x" << std::hex << std::setw(8) << std::setfill('0') << n;
    return s.str();
}
static int32_t sx(uint32_t n, unsigned bits) {
    uint32_t sign = 1u << (bits - 1);
    return static_cast<int32_t>((n ^ sign) - sign);
}
static uint32_t arithmetic_right(uint32_t n, unsigned sh) {
    sh &= 31;
    if (!sh) return n;
    return (n >> sh) | ((n & 0x80000000u) ? (~0u << (32 - sh)) : 0u);
}
static uint32_t enc_r(unsigned f7, unsigned f3, unsigned rd, unsigned a, unsigned b) {
    return (f7 << 25) | (b << 20) | (a << 15) | (f3 << 12) | (rd << 7) | 0x33;
}
static uint32_t enc_i(unsigned op, unsigned f3, unsigned rd, unsigned a, int imm) {
    return ((uint32_t(imm) & 0xfff) << 20) | (a << 15) | (f3 << 12) | (rd << 7) | op;
}
static uint32_t enc_s(unsigned f3, unsigned a, unsigned b, int imm) {
    uint32_t u = uint32_t(imm) & 0xfff;
    return ((u >> 5) << 25) | (b << 20) | (a << 15) | (f3 << 12) | ((u & 31) << 7) | 0x23;
}
static uint32_t enc_b(unsigned f3, unsigned a, unsigned b, int imm) {
    uint32_t u = uint32_t(imm) & 0x1fff;
    return (((u >> 12) & 1) << 31) | (((u >> 5) & 63) << 25) | (b << 20) |
           (a << 15) | (f3 << 12) | (((u >> 1) & 15) << 8) | (((u >> 11) & 1) << 7) | 0x63;
}
static uint32_t enc_j(unsigned rd, int imm) {
    uint32_t u = uint32_t(imm) & 0x1fffff;
    return (((u >> 20) & 1) << 31) | (((u >> 1) & 1023) << 21) |
           (((u >> 11) & 1) << 20) | (((u >> 12) & 255) << 12) | (rd << 7) | 0x6f;
}

enum Id { LUI, AUIPC, JAL, JALR, BEQ, BNE, BLT, BGE, BLTU, BGEU,
          LB, LH, LW, LBU, LHU, SB, SH, SW,
          ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI,
          ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND,
          FENCE, ECALL, EBREAK, ID_COUNT };
static const char* names[] = {
    "LUI", "AUIPC", "JAL", "JALR", "BEQ", "BNE", "BLT", "BGE", "BLTU", "BGEU",
    "LB", "LH", "LW", "LBU", "LHU", "SB", "SH", "SW",
    "ADDI", "SLTI", "SLTIU", "XORI", "ORI", "ANDI", "SLLI", "SRLI", "SRAI",
    "ADD", "SUB", "SLL", "SLT", "SLTU", "XOR", "SRL", "SRA", "OR", "AND",
    "FENCE", "ECALL", "EBREAK"
};

struct Effect {
    uint32_t next_pc = 0, result = 0, address = 0;
    unsigned rd = 0, width = 0;
    bool write_rd = false, load = false, store = false;
    uint32_t store_value = 0;
    unsigned stop = 0;
    int id = -1;
};

class Test {
    Vrv32i_core dut;
    VerilatedVcdC trace;
    bool tracing;
    uint64_t time = 0;
    std::array<uint8_t, 4096> bus_memory{}, ref_memory{};
    std::array<uint32_t, 32> regs{};
    std::array<unsigned, ID_COUNT> coverage{};
    uint32_t pc = 0;
    unsigned stopped = 0;
    uint64_t steps = 0;

    void eval() {
        dut.eval();
        if (tracing) trace.dump(time++);
    }
    void equal(uint32_t actual, uint32_t expected, const std::string& what) {
        if (actual != expected)
            throw std::runtime_error(what + ": got " + hex(actual) + ", expected " + hex(expected));
    }
    Effect reference(uint32_t ins) {
        Effect e;
        e.next_pc = pc + 4;
        unsigned op = ins & 127, f3 = (ins >> 12) & 7, f7 = ins >> 25;
        unsigned aidx = (ins >> 15) & 31, bidx = (ins >> 20) & 31;
        uint32_t a = regs[aidx], b = regs[bidx];
        e.rd = (ins >> 7) & 31;
        int32_t imm = sx(ins >> 20, 12);
        switch (op) {
        case 0x37: e.id = LUI; e.result = ins & 0xfffff000u; e.write_rd = true; break;
        case 0x17: e.id = AUIPC; e.result = pc + (ins & 0xfffff000u); e.write_rd = true; break;
        case 0x6f: {
            uint32_t u = ((ins >> 31) << 20) | (((ins >> 12) & 255) << 12) |
                         (((ins >> 20) & 1) << 11) | (((ins >> 21) & 1023) << 1);
            e.id = JAL; e.result = pc + 4; e.next_pc = pc + sx(u, 21); e.write_rd = true;
            break;
        }
        case 0x67:
            if (f3) { e.stop = 3; break; }
            e.id = JALR; e.result = pc + 4; e.next_pc = (a + imm) & ~1u; e.write_rd = true;
            break;
        case 0x63: {
            uint32_t u = ((ins >> 31) << 12) | (((ins >> 7) & 1) << 11) |
                         (((ins >> 25) & 63) << 5) | (((ins >> 8) & 15) << 1);
            bool take = false;
            switch (f3) {
            case 0: e.id = BEQ;  take = a == b; break;
            case 1: e.id = BNE;  take = a != b; break;
            case 4: e.id = BLT;  take = int32_t(a) < int32_t(b); break;
            case 5: e.id = BGE;  take = int32_t(a) >= int32_t(b); break;
            case 6: e.id = BLTU; take = a < b; break;
            case 7: e.id = BGEU; take = a >= b; break;
            default: e.stop = 3;
            }
            if (take) e.next_pc = pc + sx(u, 13);
            break;
        }
        case 0x03:
        case 0x23: {
            e.load = op == 0x03; e.store = !e.load;
            if (e.store) imm = sx(((ins >> 25) << 5) | ((ins >> 7) & 31), 12);
            e.address = a + imm;
            e.store_value = b;
            switch (f3) {
            case 0: e.width = 1; e.id = e.load ? LB : SB; break;
            case 1: e.width = 2; e.id = e.load ? LH : SH; break;
            case 2: e.width = 4; e.id = e.load ? LW : SW; break;
            case 4: if (e.load) { e.width = 1; e.id = LBU; } else e.stop = 3; break;
            case 5: if (e.load) { e.width = 2; e.id = LHU; } else e.stop = 3; break;
            default: e.stop = 3;
            }
            if (e.stop) break;
            if (e.address & (e.width - 1)) { e.stop = 5; break; }
            if (e.address + e.width > ref_memory.size())
                throw std::runtime_error("test generated out-of-range memory access");
            if (e.load) {
                for (unsigned i = 0; i < e.width; ++i)
                    e.result |= uint32_t(ref_memory[e.address + i]) << (8 * i);
                if (f3 == 0) e.result = sx(e.result, 8);
                if (f3 == 1) e.result = sx(e.result, 16);
                e.write_rd = true;
            }
            break;
        }
        case 0x13:
        case 0x33: {
            bool immediate = op == 0x13;
            uint32_t rhs = immediate ? uint32_t(imm) : b;
            unsigned sh = rhs & 31;
            if (!immediate && f7 != 0 && !(f7 == 32 && (f3 == 0 || f3 == 5))) {
                e.stop = 3; break;
            }
            if (immediate && ((f3 == 1 && f7 != 0) ||
                (f3 == 5 && f7 != 0 && f7 != 32))) { e.stop = 3; break; }
            e.write_rd = true;
            switch (f3) {
            case 0:
                e.id = immediate ? ADDI : (f7 == 32 ? SUB : ADD);
                e.result = (!immediate && f7 == 32) ? a - rhs : a + rhs;
                break;
            case 1: e.id = immediate ? SLLI : SLL; e.result = a << sh; break;
            case 2: e.id = immediate ? SLTI : SLT; e.result = int32_t(a) < int32_t(rhs); break;
            case 3: e.id = immediate ? SLTIU : SLTU; e.result = a < rhs; break;
            case 4: e.id = immediate ? XORI : XOR; e.result = a ^ rhs; break;
            case 5:
                e.id = immediate ? (f7 == 32 ? SRAI : SRLI) : (f7 == 32 ? SRA : SRL);
                e.result = f7 == 32 ? arithmetic_right(a, sh) : a >> sh;
                break;
            case 6: e.id = immediate ? ORI : OR; e.result = a | rhs; break;
            case 7: e.id = immediate ? ANDI : AND; e.result = a & rhs; break;
            }
            break;
        }
        case 0x0f: if (f3 == 0) e.id = FENCE; else e.stop = 3; break;
        case 0x73:
            if (ins == 0x73) { e.id = ECALL; e.stop = 2; }
            else if (ins == 0x100073) { e.id = EBREAK; e.stop = 1; }
            else e.stop = 3;
            break;
        default: e.stop = 3;
        }
        if ((pc & 3) || (!e.stop && (e.next_pc & 3))) e.stop = 4;
        return e;
    }

public:
    explicit Test(bool use_trace): tracing(use_trace) {
        if (tracing) {
            Verilated::traceEverOn(true);
            dut.trace(&trace, 8);
            trace.open("build/core.vcd");
        }
        reset();
    }
    ~Test() { dut.final(); if (tracing) trace.close(); }
    void reset() {
        dut.clock_enable = 1;
        dut.clk = 0; dut.reset = 1; dut.imem_rdata = 0x13; dut.dmem_rdata = 0;
        eval();
        equal(dut.dmem_wstrb, 0, "reset suppresses store");
        dut.clk = 1; eval();
        regs.fill(0); pc = 0; stopped = 0;
        for (unsigned i = 0; i < bus_memory.size(); ++i)
            bus_memory[i] = ref_memory[i] = uint8_t(i ^ (i >> 3));
        equal(dut.halted, 0, "reset clears halt");
        equal(dut.commit_valid, 0, "reset clears commit");
        equal(dut.imem_addr, 0, "reset PC");
        dut.clk = 0; dut.reset = 0; eval();
    }
    void step(uint32_t ins) {
        if (stopped) throw std::runtime_error("test tried to execute after halt");
        const std::string context = "step=" + std::to_string(steps) + " pc=" + hex(pc) + " ins=" + hex(ins) + " ";
        Effect e = reference(ins);
        dut.clk = 0; dut.imem_rdata = ins; eval();
        uint32_t base = dut.dmem_addr & ~3u, read_word = 0;
        if (base <= bus_memory.size() - 4)
            for (unsigned i = 0; i < 4; ++i) read_word |= uint32_t(bus_memory[base + i]) << (8 * i);
        dut.dmem_rdata = read_word; eval();
        // Every seventh instruction spends an extra clock in an enabled wait
        // state. It must neither retire, change PC, halt, nor issue a store.
        if (steps % 7 == 0) {
            dut.clock_enable=0;eval();
            equal(dut.dmem_wstrb,0,context+"wait suppresses store");
            dut.clk=1;eval();
            equal(dut.imem_addr,pc,context+"wait holds PC");
            equal(dut.commit_valid,0,context+"wait has no commit");
            equal(dut.halted,0,context+"wait delays halt");
            dut.clk=0;eval();dut.clock_enable=1;eval();
        }
        equal(dut.imem_addr, pc, context + "PC before edge");
        bool normal = e.stop == 0;
        equal(dut.dmem_ren, normal && e.load, context + "load enable");
        uint32_t expected_mask = normal && e.store ? ((1u << e.width) - 1) << (e.address & 3) : 0;
        equal(dut.dmem_wstrb, expected_mask, context + "store mask");
        if (normal && (e.load || e.store)) equal(dut.dmem_addr, e.address, context + "memory byte address");
        if (normal && e.store) {
            uint32_t shifted = e.store_value << ((e.address & 3) * 8);
            // 未使能的字节通道不参与比较。
            for (unsigned i = 0; i < 4; ++i)
                if (expected_mask & (1u << i)) {
                    equal((dut.dmem_wdata >> (i * 8)) & 255, (shifted >> (i * 8)) & 255, context + "store byte");
                    bus_memory[base + i] = uint8_t(dut.dmem_wdata >> (i * 8));
                }
        }
        uint32_t old_pc = pc;
        dut.clk = 1; eval();
        bool commit = e.stop < 3; // 本运行环境把ECALL/EBREAK报告为结束事件。
        equal(dut.commit_valid, commit, context + "commit valid");
        equal(dut.commit_pc, old_pc, context + "commit PC");
        equal(dut.commit_inst, ins, context + "commit instruction");
        bool write = normal && e.write_rd && e.rd != 0;
        equal(dut.commit_rd_wen, write, context + "register write enable");
        if (write) {
            equal(dut.commit_rd, e.rd, context + "destination register");
            equal(dut.commit_rd_data, e.result, context + "writeback data");
            regs[e.rd] = e.result;
        }
        if (normal && e.store)
            for (unsigned i = 0; i < e.width; ++i)
                ref_memory[e.address + i] = uint8_t(e.store_value >> (i * 8));
        if (normal) pc = e.next_pc;
        stopped = e.stop;
        equal(dut.imem_addr, pc, context + "PC after edge");
        equal(dut.halted, stopped != 0, context + "halted");
        equal(dut.halt_reason, stopped, context + "halt reason");
        if (e.id >= 0 && e.stop < 3) ++coverage[e.id];
        ++steps;
    }
    void li(unsigned rd, uint32_t value) {
        uint32_t upper = (value + 0x800u) & 0xfffff000u;
        step(upper | (rd << 7) | 0x37);
        step(enc_i(0x13, 0, rd, rd, sx(value & 0xfff, 12)));
    }
    void expect_stop(uint32_t ins, unsigned reason) {
        step(ins);
        equal(stopped, reason, "directed halt reason");
        // 停机后即使输入改成store，也必须保持PC、不再写寄存器/存储器。
        for (unsigned i = 0; i < 3; ++i) {
            dut.clk = 0; dut.imem_rdata = enc_s(2, 0, 0, 0); eval();
            equal(dut.dmem_wstrb, 0, "halt suppresses store");
            equal(dut.dmem_ren, 0, "halt suppresses read");
            dut.clk = 1; eval();
            equal(dut.imem_addr, pc, "halt holds PC");
            equal(dut.commit_valid, 0, "halt suppresses commit");
            equal(dut.commit_rd_wen, 0, "halt suppresses register write");
        }
    }
    void directed() {
        // 写x0、读x0和所有31个可写寄存器；验证复位清零和数据依赖。
        for (unsigned i = 1; i < 32; ++i) step(enc_i(0x13, 0, i, i, 0));
        li(0, 0xdeadbeef);
        step(enc_i(0x13, 0, 1, 0, 1));
        const std::array<uint32_t, 10> vals = {
            0, 1, 0xffffffffu, 0x80000000u, 0x7fffffffu, 31, 32, 63, 0xaaaaaaaa, 0x55555555
        };
        const std::array<std::pair<unsigned, unsigned>, 10> r_ops = {{{0,0}, {32,0}, {0,1}, {0,2},
            {0,3}, {0,4}, {0,5}, {32,5}, {0,6}, {0,7}}};
        for (uint32_t a : vals) for (uint32_t b : vals) {
            li(1, a); li(2, b);
            for (auto [f7, f3] : r_ops) step(enc_r(f7, f3, 3, 1, 2));
        }
        for (uint32_t a : vals) {
            li(1, a);
            for (int imm : {-2048, -1, 0, 1, 2047})
                for (unsigned f3 : {0u, 2u, 3u, 4u, 6u, 7u}) step(enc_i(0x13, f3, 3, 1, imm));
            for (int sh : {0, 1, 15, 31}) {
                step(enc_i(0x13, 1, 3, 1, sh));
                step(enc_i(0x13, 5, 3, 1, sh));
                step(enc_i(0x13, 5, 3, 1, 0x400 | sh));
            }
        }
        step(0x80000197); // AUIPC x3,0x80000
        step(enc_j(3, 1048572));
        step(enc_j(0, -1048576));
        li(1, 0x105);
        step(enc_i(0x67, 0, 1, 1, 0)); // JALR rd=rs1，先读旧值，清除目标bit0
        step(enc_i(0x67, 0, 0, 1, -4));
        for (uint32_t a : vals) for (uint32_t b : vals) {
            li(1, a); li(2, b);
            for (unsigned f3 : {0u, 1u, 4u, 5u, 6u, 7u}) {
                step(enc_b(f3, 1, 2, -4096));
                step(enc_b(f3, 1, 2, 4092));
            }
        }
        li(1, 256); li(2, 0x80ff7f01);
        step(enc_s(2, 1, 2, 0));
        for (int lane = 0; lane < 4; ++lane) {
            step(enc_i(0x03, 0, 3, 1, lane));
            step(enc_i(0x03, 4, 3, 1, lane));
            step(enc_s(0, 1, 2, lane));
        }
        for (int lane : {0, 2}) {
            step(enc_i(0x03, 1, 3, 1, lane));
            step(enc_i(0x03, 5, 3, 1, lane));
            step(enc_s(1, 1, 2, lane));
        }
        step(enc_i(0x03, 2, 3, 1, 0));
        step(enc_i(0x03, 2, 0, 1, -4)); // load x0仍然必须发出读取请求
        step(enc_s(2, 1, 2, -4));
        li(1, 2048);
        for (int offset : {-2048, 2047}) {
            step(enc_s(0, 1, 2, offset));
            step(enc_i(0x03, 0, 3, 1, offset));
            step(enc_i(0x03, 4, 3, 1, offset));
        }
        for (int offset : {-2048, 2046}) {
            step(enc_s(1, 1, 2, offset));
            step(enc_i(0x03, 1, 3, 1, offset));
            step(enc_i(0x03, 5, 3, 1, offset));
        }
        step(0x0ff0000f); // FENCE iorw,iorw
        expect_stop(0x00100073, 1);
        reset(); expect_stop(0x00000073, 2);

        for (uint32_t bad : {0u, 0xffffffffu, enc_r(1, 0, 3, 1, 2), // MUL
             enc_r(32, 7, 3, 1, 2), enc_i(0x13, 1, 3, 1, 32),
             enc_i(0x13, 5, 3, 1, 0x20), enc_i(0x03, 3, 3, 1, 0),
             enc_s(4, 1, 2, 0), enc_b(2, 1, 2, 0), enc_i(0x67, 1, 3, 1, 0),
             0x0000100fu, 0x30001073u, 0x001000f3u}) {
            reset(); expect_stop(bad, 3);
        }
        for (unsigned f3 : {1u, 2u, 5u}) {
            reset(); expect_stop(enc_i(0x03, f3, 3, 0, 1), 5);
        }
        for (unsigned f3 : {1u, 2u}) {
            reset(); li(2, 0x12345678); expect_stop(enc_s(f3, 0, 2, 1), 5);
        }
        reset(); expect_stop(enc_j(1, 2), 4);
        reset(); expect_stop(enc_i(0x67, 0, 1, 0, 3), 4);
        reset(); expect_stop(enc_b(0, 0, 0, 2), 4);
        reset(); li(1, 1); step(enc_b(0, 0, 1, 2)); // 不跳转时不检查目标对齐
        step(enc_i(0x67, 0, 0, 0, 1)); // 清除bit0后目标=0，合法
    }
    void random() {
        reset();
        std::mt19937 rng(0x20261026);
        for (unsigned i = 1; i < 32; ++i) li(i, rng());
        for (unsigned i = 0; i < 8000; ++i) {
            unsigned rd = rng() % 32, a = rng() % 32, b = rng() % 32;
            unsigned f3 = rng() % 8;
            switch (rng() % 8) {
            case 0:
                step(enc_r((f3 == 0 || f3 == 5) && (rng() & 1) ? 32 : 0, f3, rd, a, b)); break;
            case 1: {
                int imm = int(rng() % 4096) - 2048;
                if (f3 == 1) imm = rng() % 32;
                if (f3 == 5) imm = (rng() % 32) | ((rng() & 1) ? 0x400 : 0);
                step(enc_i(0x13, f3, rd, a, imm)); break;
            }
            case 2: {
                static const unsigned fs[] = {0, 1, 2, 4, 5};
                f3 = fs[rng() % 5];
                int addr = rng() % 2048;
                if (f3 == 1 || f3 == 5) addr &= ~1;
                if (f3 == 2) addr &= ~3;
                step(enc_i(0x03, f3, rd, 0, addr)); break;
            }
            case 3: {
                f3 = rng() % 3;
                int addr = int(rng() % 2048) & ~((1 << f3) - 1);
                step(enc_s(f3, 0, b, addr)); break;
            }
            case 4: {
                static const unsigned fs[] = {0, 1, 4, 5, 6, 7};
                step(enc_b(fs[rng() % 6], a, b, (int(rng() % 2048) - 1024) * 4)); break;
            }
            case 5: step((rng() & 0xfffff000u) | (rd << 7) | ((rng() & 1) ? 0x37 : 0x17)); break;
            case 6: step(enc_j(rd, (int(rng() % 524288) - 262144) * 4)); break;
            case 7: step(enc_i(0x67, 0, rd, 0, int(rng() % 512) * 4 + (rng() & 1))); break;
            }
        }
        // 将每个寄存器作为源读出，覆盖此前随机写入的状态。
        for (unsigned i = 0; i < 31; ++i) step(enc_i(0x13, 0, 31, i, 0));
        expect_stop(0x00100073, 1);
    }
    void program() {
        reset();
        // 真实按PC取指执行循环：sum(1..10)=55，包含倒向分支和内存签名。
        std::vector<uint32_t> rom = {
            enc_i(0x13, 0, 1, 0, 10), enc_i(0x13, 0, 2, 0, 0),
            enc_r(0, 0, 2, 2, 1), enc_i(0x13, 0, 1, 1, -1),
            enc_b(1, 1, 0, -8), enc_s(2, 0, 2, 0x100), 0x00100073
        };
        unsigned timeout = 100;
        while (!stopped && timeout--) {
            if ((pc >> 2) >= rom.size()) throw std::runtime_error("PC escaped test ROM");
            step(rom[pc >> 2]);
        }
        equal(stopped, 1, "loop terminates");
        equal(bus_memory[0x100], 55, "sum program memory signature");
        equal(regs[2], 55, "sum program result");
    }
    void report() {
        for (unsigned i = 0; i < ID_COUNT; ++i)
            if (coverage[i] == 0) throw std::runtime_error(std::string("uncovered instruction: ") + names[i]);
        std::cout << "PASS: " << steps << " instructions checked, 40/40 RV32I instructions covered\n"
                  << "PASS: boundary operands, byte lanes, taken/untaken branches, faults, reset, halt, loop\n";
    }
};

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    try {
        bool tracing = argc > 1 && std::string(argv[1]) == "--trace";
        Test t(tracing);
        t.directed(); t.random(); t.program(); t.report();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
