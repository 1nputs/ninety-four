"""Hooked oracle: the GPGX core with the oracle shim (build/nhl_oracle.so).

Adds breakpoints, per-PC coverage and function capture to the plain libretro host.
"""
import ctypes as C
import json
import os
import struct
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'oracle'))
from retro import Core  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
ORACLE_SO = os.path.join(ROOT, 'build', 'nhl_oracle.so')
NREGS = 18
REGNAMES = [f'd{i}' for i in range(8)] + [f'a{i}' for i in range(8)] + ['pc', 'sr']
CAP_MAGIC = 0x43434C48


def load_symbols():
    """name -> ROM address, preferring the vasm build's complete table."""
    for p in (os.path.join(ROOT, 'build', 'asm', 'symbols.json'), os.path.join(ROOT, 'out', 'symbols.json')):
        if os.path.exists(p):
            return json.load(open(p))
    return {}


class Oracle(Core):
    def __init__(self, so_path=ORACLE_SO, **kw):
        super().__init__(so_path, **kw)
        lib = self.lib
        lib.oracle_install.restype = C.c_int
        lib.oracle_bp_set.argtypes = [C.c_uint32, C.c_int]
        lib.oracle_bp_hits.argtypes = [C.c_uint32]; lib.oracle_bp_hits.restype = C.c_uint32
        lib.oracle_bp_captured.argtypes = [C.c_uint32]; lib.oracle_bp_captured.restype = C.c_uint32
        lib.oracle_set_frame.argtypes = [C.c_uint32]
        lib.oracle_fn_set.argtypes = [C.c_uint32, C.c_int]
        lib.oracle_fn_set_many.argtypes = [C.POINTER(C.c_uint32), C.c_int]
        lib.oracle_set_irq_pcs.argtypes = [C.POINTER(C.c_uint32), C.c_int]
        lib.oracle_capture_open.argtypes = [C.c_char_p, C.c_uint32, C.c_uint32]; lib.oracle_capture_open.restype = C.c_int
        lib.oracle_capture_spread.argtypes = [C.c_int]
        lib.oracle_capture_close.restype = C.c_uint32
        lib.oracle_capture_records.restype = C.c_uint32
        lib.oracle_coverage_enable.argtypes = [C.c_int]
        lib.oracle_coverage_get.argtypes = [C.c_void_p]
        lib.oracle_ram_get.argtypes = [C.c_void_p]
        lib.oracle_regs_get.argtypes = [C.c_void_p]
        lib.oracle_install()
        self.frame_no = 0
        self.symbols = load_symbols()

    def load(self, rom_path):
        av = super().load(rom_path)
        rom = open(rom_path, 'rb').read(0x100)
        # exception vectors the game installs: level 4 (hblank) at $70, level 6 (vblank) at $78
        irqs = [struct.unpack('>I', rom[0x70:0x74])[0], struct.unpack('>I', rom[0x78:0x7C])[0]]
        arr = (C.c_uint32 * len(irqs))(*irqs)
        self.lib.oracle_set_irq_pcs(arr, len(irqs))
        self.irq_pcs = irqs
        return av

    def addr(self, name_or_addr):
        if isinstance(name_or_addr, int):
            return name_or_addr
        s = name_or_addr
        if s in self.symbols:
            return self.symbols[s]
        return int(s, 16)

    def bp(self, name_or_addr, on=True):
        self.lib.oracle_bp_set(self.addr(name_or_addr), 1 if on else 0)

    def fn_set_all(self, names=None):
        """Log every routine (default: all code routines known to the call graph) as a nested call."""
        if names is None:
            cg_path = os.path.join(ROOT, 'out', 'callgraph.json')
            cg = json.load(open(cg_path)) if os.path.exists(cg_path) else {}
            names = [n for n, v in cg.items() if v.get('insns', 0) > 0] or list(self.symbols)
        pcs = [self.symbols[n] for n in names if n in self.symbols and self.symbols[n] < 0x100000]
        arr = (C.c_uint32 * len(pcs))(*pcs)
        self.lib.oracle_fn_set_many(arr, len(pcs))
        return len(pcs)

    def hits(self, name_or_addr):
        return self.lib.oracle_bp_hits(self.addr(name_or_addr))

    def captured(self, name_or_addr):
        return self.lib.oracle_bp_captured(self.addr(name_or_addr))

    def capture_spread(self, on=True):
        self.lib.oracle_capture_spread(1 if on else 0)

    def capture_open(self, path, max_per_bp=32, stride=1):
        if not self.lib.oracle_capture_open(path.encode(), max_per_bp, stride):
            raise RuntimeError('capture_open failed')

    def capture_close(self):
        return self.lib.oracle_capture_close()

    def run(self, frames=1):
        for _ in range(frames):
            self.lib.oracle_set_frame(self.frame_no)
            self.lib.retro_run()
            self.frame_no += 1

    def coverage(self, on=True):
        self.lib.oracle_coverage_enable(1 if on else 0)

    def coverage_get(self):
        buf = (C.c_uint32 * 0x100000)()
        self.lib.oracle_coverage_get(buf)
        return buf

    def regs(self):
        buf = (C.c_uint32 * NREGS)()
        self.lib.oracle_regs_get(buf)
        return dict(zip(REGNAMES, buf))


IO_READ_FLAG = 0x80000000
IO_WRITE_FLAG = 0x40000000


class Call:
    """A routine entered inside a captured call: registers at entry/return and the RAM it changed."""
    __slots__ = ('pc', 'depth', 'seq', 'regs_in', 'regs_out', 'stack', 'diff')

    def __init__(self, pc, depth, seq, regs_in, regs_out, stack, diff):
        self.pc, self.depth, self.seq, self.regs_in, self.regs_out, self.stack, self.diff = pc, depth, seq, regs_in, regs_out, stack, diff

    @property
    def is_io(self):
        return bool(self.pc & (IO_READ_FLAG | IO_WRITE_FLAG))


class Record:
    """One captured call: registers and RAM at entry and at return, plus the nested call log."""
    __slots__ = ('pc', 'frame', 'flags', 'regs_in', 'regs_out', 'ram_in', 'ram_out', 'calls')

    def __init__(self, blob):
        magic, total, self.pc, self.frame, self.flags, n_calls = struct.unpack_from('<6I', blob, 0)
        assert magic == CAP_MAGIC and total == len(blob), 'bad capture record'
        o = 6 * 4
        self.regs_in = dict(zip(REGNAMES, struct.unpack_from(f'<{NREGS}I', blob, o))); o += NREGS * 4
        self.regs_out = dict(zip(REGNAMES, struct.unpack_from(f'<{NREGS}I', blob, o))); o += NREGS * 4
        self.ram_in = blob[o:o + 0x10000]; o += 0x10000
        self.ram_out = blob[o:o + 0x10000]; o += 0x10000
        self.calls = []
        for _ in range(n_calls):
            pc, depth, seq = struct.unpack_from('<3I', blob, o); o += 12
            depth &= 0x7FFFFFFF
            rin = dict(zip(REGNAMES, struct.unpack_from(f'<{NREGS}I', blob, o))); o += NREGS * 4
            rout = dict(zip(REGNAMES, struct.unpack_from(f'<{NREGS}I', blob, o))); o += NREGS * 4
            stack = blob[o:o + 16]; o += 16
            nd, = struct.unpack_from('<I', blob, o); o += 4
            diff = [struct.unpack_from('<HBB', blob, o + 4 * i) for i in range(nd)]; o += 4 * nd
            self.calls.append(Call(pc, depth, seq, rin, rout, stack, diff))
        assert o == len(blob), 'capture record trailer mismatch'
        self.calls.sort(key=lambda c: c.seq)

    @property
    def interrupted(self):
        return bool(self.flags & 1)

    @property
    def abnormal(self):
        return bool(self.flags & 2)

    def changed(self):
        """Offsets of RAM bytes that differ between entry and return."""
        a, b = self.ram_in, self.ram_out
        return [i for i in range(0x10000) if a[i] != b[i]]


def read_captures(path):
    data = open(path, 'rb').read()
    recs, o = [], 0
    while o < len(data):
        magic, total = struct.unpack_from('<2I', data, o)
        assert magic == CAP_MAGIC, f'bad magic at {o}'
        recs.append(Record(data[o:o + total])); o += total
    return recs
