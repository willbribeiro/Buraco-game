import capstone
import sys

path = r"C:\Users\ventoy\Documents\Buraco\BURACO.EXE"
d = open(path, "rb").read()

start = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x70
end = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x240
base = start - 0x70 + 0x100  # file offset -> PSP-relative address

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
md.detail = False
code = d[start:end]
addr = base
for insn in md.disasm(code, addr):
    print(f"{insn.address:04X}: {insn.bytes.hex():<16} {insn.mnemonic} {insn.op_str}")
