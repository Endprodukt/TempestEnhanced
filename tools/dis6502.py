#!/usr/bin/env python3
"""dis6502.py - disassemble the Tempest program ROMs (MAME set tempest, revision 3).

  python -I tools/dis6502.py ROMDIR [START [END]]     addresses in hex

ROMDIR holds the files of tempest.zip unpacked (its top level).
Prints a plain listing; data is disassembled as code, read with care.
"""
import os, sys

M = {}
def op(code, name, mode): M[code] = (name, mode)
T = """
00 BRK imp|01 ORA izx|05 ORA zp|06 ASL zp|08 PHP imp|09 ORA imm|0A ASL acc|0D ORA abs|0E ASL abs
10 BPL rel|11 ORA izy|15 ORA zpx|16 ASL zpx|18 CLC imp|19 ORA aby|1D ORA abx|1E ASL abx
20 JSR abs|21 AND izx|24 BIT zp|25 AND zp|26 ROL zp|28 PLP imp|29 AND imm|2A ROL acc|2C BIT abs|2D AND abs|2E ROL abs
30 BMI rel|31 AND izy|35 AND zpx|36 ROL zpx|38 SEC imp|39 AND aby|3D AND abx|3E ROL abx
40 RTI imp|41 EOR izx|45 EOR zp|46 LSR zp|48 PHA imp|49 EOR imm|4A LSR acc|4C JMP abs|4D EOR abs|4E LSR abs
50 BVC rel|51 EOR izy|55 EOR zpx|56 LSR zpx|58 CLI imp|59 EOR aby|5D EOR abx|5E LSR abx
60 RTS imp|61 ADC izx|65 ADC zp|66 ROR zp|68 PLA imp|69 ADC imm|6A ROR acc|6C JMP ind|6D ADC abs|6E ROR abs
70 BVS rel|71 ADC izy|75 ADC zpx|76 ROR zpx|78 SEI imp|79 ADC aby|7D ADC abx|7E ROR abx
81 STA izx|84 STY zp|85 STA zp|86 STX zp|88 DEY imp|8A TXA imp|8C STY abs|8D STA abs|8E STX abs
90 BCC rel|91 STA izy|94 STY zpx|95 STA zpx|96 STX zpy|98 TYA imp|99 STA aby|9A TXS imp|9D STA abx
A0 LDY imm|A1 LDA izx|A2 LDX imm|A4 LDY zp|A5 LDA zp|A6 LDX zp|A8 TAY imp|A9 LDA imm|AA TAX imp|AC LDY abs|AD LDA abs|AE LDX abs
B0 BCS rel|B1 LDA izy|B4 LDY zpx|B5 LDA zpx|B6 LDX zpy|B8 CLV imp|B9 LDA aby|BA TSX imp|BC LDY abx|BD LDA abx|BE LDX aby
C0 CPY imm|C1 CMP izx|C4 CPY zp|C5 CMP zp|C6 DEC zp|C8 INY imp|C9 CMP imm|CA DEX imp|CC CPY abs|CD CMP abs|CE DEC abs
D0 BNE rel|D1 CMP izy|D5 CMP zpx|D6 DEC zpx|D8 CLD imp|D9 CMP aby|DD CMP abx|DE DEC abx
E0 CPX imm|E1 SBC izx|E4 CPX zp|E5 SBC zp|E6 INC zp|E8 INX imp|E9 SBC imm|EA NOP imp|EC CPX abs|ED SBC abs|EE INC abs
F0 BEQ rel|F1 SBC izy|F5 SBC zpx|F6 INC zpx|F8 SED imp|F9 SBC aby|FD SBC abx|FE INC abx
"""
for line in T.strip().split("\n"):
    for e in line.split("|"):
        c, n, m = e.split()
        op(int(c, 16), n, m)
LEN = dict(imp=1, acc=1, imm=2, zp=2, zpx=2, zpy=2, izx=2, izy=2, rel=2, abs=3, abx=3, aby=3, ind=3)

FILES = [("136002-133.d1", 0x9000), ("136002-134.f1", 0xA000), ("136002-235.j1", 0xB000),
         ("136002-136.lm1", 0xC000), ("136002-237.p1", 0xD000), ("136002-237.p1", 0xF000),
         ("136002-138.np3", 0x3000)]

def load(romdir):
    mem = bytearray(0x10004)
    for name, at in FILES:
        d = open(os.path.join(romdir, name), "rb").read()
        mem[at:at + len(d)] = d
    return mem

def dis(mem, pc):
    b = mem[pc]
    if b not in M:
        return 1, ".BYTE %02X" % b
    n, m = M[b]
    l = LEN[m]
    a = mem[pc + 1] if l > 1 else 0
    w = a | (mem[pc + 2] << 8) if l > 2 else a
    s = {"imp": "", "acc": "A", "imm": "#%02X" % a, "zp": "%02X" % a, "zpx": "%02X,X" % a, "zpy": "%02X,Y" % a,
         "izx": "(%02X,X)" % a, "izy": "(%02X),Y" % a, "abs": "%04X" % w, "abx": "%04X,X" % w,
         "aby": "%04X,Y" % w, "ind": "(%04X)" % w,
         "rel": "%04X" % ((pc + 2 + (a - 256 if a > 127 else a)) & 0xffff)}[m]
    return l, (n + " " + s).strip()

if __name__ == "__main__":
    mem = load(sys.argv[1])
    start = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x9000
    end = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0xdfff
    pc = start
    while pc <= end:
        l, t = dis(mem, pc)
        print("%04X  %-9s %s" % (pc, " ".join("%02X" % mem[pc + i] for i in range(l)), t))
        pc += l
