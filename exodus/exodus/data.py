import exodus.rom
import exodus.codec

import struct
import itertools

def grouper(it, n, fill=None):
    args = [iter(it)] * n
    return itertools.zip_longest(fillvalue=fill, *args)

# word types
WT_DATA    = 0
WT_CONT    = 1
WT_STRING  = 2
WT_INSN    = 3

# valid note types
NOTE_TYPES = [
    'misc',
    'blanks',
    'label',
    'label_big',
    'comment',
    'comment_big',
    ]

class Data(object):
    def __init__(self, rom):
        self.r = rom
        self.notes = exodus.rom.Notes()
        self.starting_meta()

    def word_type(self, addr, v=None):
        addr &= ~0b1
        x = self.r.oob_get(addr)
        if v is not None:
            x = (v & 0b1111) | (x & ~0b1111)
            self.r.oob_set(addr, x)
        return (x & 0b1111)

    def word_is_decimal(self, addr, v=None):
        addr &= ~0b1
        x = self.r.oob_get(addr)
        if v is not None:
            v = int(v)
            x = ((v & 0b1) << 4) | (x & ~0b10000)
            self.r.oob_set(addr, x)
        return (x & 0b10000) > 0

    def note_set(self, addr, n, v):
        addr &= ~0b1
        if n not in NOTE_TYPES:
            raise ValueError(repr(n) + ' is not a valid note type')
        notes = self.notes.get(addr)
        if notes is None:
            notes = {}
            self.notes.add(addr, notes)
        else:
            notes = notes[1]
        notes[n] = v

    def line(self, addr):
        addr &= ~0b1
        while self.word_type(addr) == WT_CONT:
            addr += 0b10
        tp = self.word_type(addr)
        end = addr + 0b10
        while self.word_type(end) == WT_CONT:
            end += 0b10
        notes = self.notes.get(addr)
        if notes is None:
            notes = {}
        else:
            notes = notes[1]
        return tp, addr, self.r.bytes(addr, end - addr), notes

    def next_line_at(self, addr):
        addr &= ~0b1
        addr += 0b10
        while self.word_type(addr) == WT_CONT:
            addr += 0b10
        return addr

    def prev_line_at(self, addr):
        addr &= ~0b1
        if addr == 0:
            return addr
        addr -= 0b10
        while self.word_type(addr) == WT_CONT:
            addr -= 0b10
        return addr

    def line_format(self, tp, addr, by, notes):
        lbl = ''
        cmm = ''
        if 'label' in notes:
            lbl = notes['label'] + ':'
        if 'comment' in notes:
            cmm = '# ' + notes['comment']
        code = '???'
        if tp == WT_DATA:
            code = as_data(by)
        elif tp == WT_STRING:
            code = as_ascii(by)
        elif tp == WT_INSN:
            code = as_insn(by)
        ln = '{:8} {}'.format(lbl, code)
        if cmm:
            ln = '{:50} {}'.format(ln, cmm)
        return ln

    def starting_meta(self):
        # this function WILL be deleted at some point, for the record

        types = [
            (0x100, 0x110, WT_STRING, 'Console name'),
            (0x110, 0x120, WT_STRING, 'Firm name and build date'),
            (0x120, 0x150, WT_STRING, None),
            (0x150, 0x180, WT_STRING, None),
            (0x180, 0x18e, WT_STRING, 'Program type and serial no.'),
            (0x18e, 0x190, WT_DATA,   'Checksum'),
            (0x190, 0x1a0, WT_STRING, 'IO device support'),
            (0x1a0, 0x1a4, WT_DATA,   'Start of the ROM'),
            (0x1a4, 0x1a8, WT_DATA,   'End of the ROM'),
            (0x1a8, 0x1ac, WT_DATA,   'Start of RAM'),
            (0x1ac, 0x1b0, WT_DATA,   'End of RAM'),
            (0x1b0, 0x1b4, WT_STRING, 'Backup RAM ID'),
            (0x1b4, 0x1b8, WT_STRING, 'Start of backup RAM'),
            (0x1b8, 0x1bc, WT_STRING, 'End of backup RAM'),
            (0x1bc, 0x1c8, WT_STRING, 'Modem support'),
            (0x1c8, 0x1cc, WT_DATA,   'Misc Notes'),
            (0x1cc, 0x1d0, WT_DATA,   None),
            (0x1d0, 0x1d4, WT_DATA,   None),
            (0x1d4, 0x1d8, WT_DATA,   None),
            (0x1d8, 0x1dc, WT_DATA,   None),
            (0x1dc, 0x1e0, WT_DATA,   None),
            (0x1e0, 0x1e4, WT_DATA,   None),
            (0x1e4, 0x1e8, WT_DATA,   None),
            (0x1e8, 0x1ec, WT_DATA,   None),
            (0x1ec, 0x1f0, WT_DATA,   None),
            (0x1f0, 0x200, WT_STRING, 'Country codes'),
            (0x200, 0x202, WT_INSN,   None),
            (0x202, 0x204, WT_INSN,   None),
            (0x204, 0x206, WT_INSN,   None),
            (0x206, 0x20c, WT_INSN,   None),
            (0x20c, 0x20e, WT_INSN,   None),
            (0x20e, 0x214, WT_INSN,   None),
            ]

        self.note_set(0x100, 'blanks', 1)
        self.note_set(0x100, 'comment_big', 'ROM metadata')
        self.note_set(0x100, 'label_big', '_rom_header')

        self.note_set(0x180, 'blanks', 1)
        self.note_set(0x1a0, 'blanks', 1)
        self.note_set(0x1b0, 'blanks', 1)
        self.note_set(0x1c8, 'blanks', 1)
        self.note_set(0x1f8, 'blanks', 1)

        self.note_set(0x200, 'blanks', 1)

        for lo, hi, v, comment in types:
            self.word_type(lo, v)
            if comment is not None:
                self.note_set(lo, 'comment', comment)
            for i in range(lo+2, hi, 2):
                self.word_type(i, WT_CONT)

        x = self.r.bytes(0x4, 4)
        (start,) = struct.unpack('>I', x)
        self.note_set(start, 'blanks', 1)
        self.note_set(start, 'label_big', '_start')

def as_data(s):
    if len(s) % 4 == 0:
        return '.long ' + ','.join(
            '0x{:02x}{:02x}{:02x}{:02x}'.format(*x) for x in
            grouper(s, 4, 0))
    elif len(s) % 2 == 0:
        return '.word ' + ','.join(
            '0x{:02x}{:02x}'.format(*x) for x in
            grouper(s, 2, 0))
    else:
        return '.word ' + ','.join(
            '0x{:02x}'.format(x) for x in s)

def as_ascii(s):
    return '.ascii "{}"'.format(repr(s)[2:-1]) # so much hax :'(

def as_insn(s):
    insns = exodus.codec.decode(s)
    if len(insns) != 1 or len(s) != insns[0][3]:
        return as_data(s)
    (opcode, cc), sz, ops, nused = insns[0]
    s = [opcode]
    if cc is not None:
        s.append(cc)
    if sz:
        s.append('.')
        s.append(sz)
    if len(ops) >= 1:
        s.append('\t')
        s.append(exodus.codec.fmtop(ops[0]))
    if len(ops) >= 2:
        s.append(', ')
        s.append(exodus.codec.fmtop(ops[1]))
    return ''.join(s)
