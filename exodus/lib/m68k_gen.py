#!/usr/bin/python3

import sys, os
import re

# I'm using all upper case for type names because I find CamelCase hard
# to type.

def bits(seq):
    return int(''.join(('1' if x else '0') for x in seq), 2)

class ABORTCOMPILE(Exception):
    pass

#
# INPUT PARSING

class ISAOPERAND(object):
    def __init__(self, p):
        self.p = p

class ISAOPERAND_EA(ISAOPERAND):
    def __init__(self, p, backwards):
        self.p = p
        self.backwards = backwards

class ISAOPERAND_IMM(ISAOPERAND):
    pass

class ISAOPERAND_REG(ISAOPERAND):
    pass

class ISAOPERAND_ADDR(ISAOPERAND):
    pass

class ISAOPERAND_DATA(ISAOPERAND):
    pass

class ISAOPERAND_POST(ISAOPERAND):
    pass

class ISAOPERAND_PRE(ISAOPERAND):
    pass

class ISAOPERAND_AOFFSET(ISAOPERAND):
    pass

class ISAOPCODE(object):
    def __init__(self):
        self.name = 'INVALID'
        self.has_cc = False
        self.has_sz = False

class ISAPATTERN(object):
    def __init__(self):
        pass

class ISA(object):
    def __init__(self):
        self.pats = []
        self.opcodes = {}

    def opname(self, opstr):
        return opstr.replace('C', 'CC').upper().replace('CC', 'cc')

    def parse_file(self, fname):
        lnno = 1
        with open(fname, 'r') as f:
            for line in f:
                self.parse_line((fname, lnno), line)
                lnno += 1

    def parse_error(self, info, msg):
        fname, lnno = info
        print('{}:{}: error: {}'.format(fname, lnno, msg))
        raise ABORTCOMPILE()

    def parse_line(self, info, ln):
        if len(ln) == 0 or ln[0] == ';':
            return
        s = ln.split()
        if len(s) == 0:
            return

        if len(s) < 3 or len(s) > 5:
            self.parse_error(info, 'bad number of fields')

        patstr, nib, opstr, *ops = s

        if len(patstr) != 16:
            self.parse_error(info, 'bit pattern should be 16 chars')
        if len(nib) != 1:
            self.parse_error(info, 'immediate nibble should be 1 char')
        if nib not in '0123456789abcdefABCDEF':
            self.parse_error(info, 'immediate nibble should be a hex digit')

        s = opstr.split('.')
        if len(s) < 1 or len(s) > 2:
            self.parse_error(info, "invalid opcode specifier `%s'" % opstr)

        opname = self.opname(s[0])
        if opname not in self.opcodes:
            op = ISAOPCODE()
            op.name = opname
            if 'cc' in op.name:
                op.has_cc = True
            self.opcodes[op.name] = op
        op = self.opcodes[opname]

        pat = ISAPATTERN()
        self.pats.append(pat)

        pat.pat = patstr
        pat.mask = bits(x in '01' for x in patstr)
        pat.bits = bits(x == '1' for x in patstr)
        pat.size = s[1] if len(s) > 1 else '' # terrible schema
        pat.op = op
        pat.nib = [(int(nib, 16) >> (2*x)) & 3 for x in range(2)]
        pat.info = info
        pat.ops = [self.parse_operand(info, x) for x in ops]

        if len(pat.size) > 0:
            pat.op.has_sz = True

    def parse_operand(self, info, x):
        m = None
        def match(ex):
            nonlocal m
            m = re.match(ex, x)
            return m is not None

        if match('<(!?)([A-Z])>'):
            return ISAOPERAND_EA(m.group(2), m.group(1) == '!')
        if match('#([A-Z]|@[12])'):
            return ISAOPERAND_IMM(m.group(1))
        if match('%(ccr|sr|usp)'):
            return ISAOPERAND_REG(m.group(1))
        if match('%a([A-Z])'):
            return ISAOPERAND_ADDR(m.group(1))
        if match('%d([A-Z])'):
            return ISAOPERAND_DATA(m.group(1))
        if match('\(%a([A-Z])\)+'):
            return ISAOPERAND_POST(m.group(1))
        if match('-\(%a([A-Z])\)'):
            return ISAOPERAND_PRE(m.group(1))
        if match('\(@1,%a([A-Z])\)'):
            return ISAOPERAND_AOFFSET(m.group(1))

        self.parse_error(info, "invalid operand specifier `%s'" % x)

#
# OUTPUT GENERATION

class OUTPUT(object):
    def __init__(self, fname):
        self.f = open(fname, 'w')
        self.depth = 0

    def close(self):
        self.f.close()

    def emit(self, s):
        s = re.sub('^[ \t]*', '', s, flags=re.M)
        for ln in s.split('\n'):
            self.depth -= ln.count('}')
            self.f.write('\t' * self.depth + ln + '\n')
            self.depth += ln.count('{')

class DECGEN(object):
    def emit_isa(self, isa):
        pass

class CDECGEN(DECGEN):
    def __init__(self, c, h):
        self.c = OUTPUT(c)
        self.h = OUTPUT(h)

    def close(self):
        self.c.close()
        self.h.close()

    def warn(self, pat, msg):
        fname, lnno = pat.info
        print('{}:{}: warning: {}'.format(fname, lnno, msg))

    def expr_field(self, pat, n):
        n = n.lower()

        if not n in pat.pat:
            self.warn(pat, "pattern does not contain `%s'; using 0" % n)
            return '0'

        mask = bits(x == n for x in pat.pat)
        last = list(reversed(pat.pat)).index(n)

        if last == 0:
            return '(op & 0x%x)' % mask
        return '((op & 0x%x) >> %d)' % (mask, last)

    def pat_fn_name(self, pat):
        return 'op_' + pat.pat

    def emit_header(self, isa):
        self.h.emit('''
            #ifndef __INC_M68K_DECODE_H__
            #define __INC_M68K_DECODE_H__

            #include <stdint.h>

            typedef enum m68k_operand_mode {
                M68K_M_DATA,      /* %dN */
                M68K_M_ADDR,      /* %aN */
                M68K_M_INDIR,     /* (%aN) */
                M68K_M_POST,      /* (%aN)+ */
                M68K_M_PRE,       /* -(%aN) */
                M68K_M_DISP,      /* (D16,%aN) */
                M68K_M_INDEX,     /* (D8,%aN,%xN) */
                M68K_M_PCINDIR,   /* (D16,%pc) */
                M68K_M_PCINDEX,   /* (D8,%pc,%xN) */
                M68K_M_MEM_W,     /* (D16) */
                M68K_M_MEM_L,     /* (D32) */
                M68K_M_IMM,       /* #D32 */
                M68K_M_SPECIAL,   /* the M68K_R_* #defines */
            } m68k_operand_mode_t;

            #define M68K_R_CCR  0
            #define M68K_R_SR   1
            #define M68K_R_USP  2

            typedef enum m68k_opcode {
                M68K_OP_INVALID,
        ''')
        for op in sorted(isa.opcodes):
            self.h.emit('M68K_OP_' + isa.opcodes[op].name + ',')
        self.h.emit('''
            } m68k_opcode_t;

            typedef struct m68k_opcode_info {
                char *str;
                int has_cc;
                int has_sz;
            } m68k_opcode_info_t;

            extern m68k_opcode_info_t m68k_opcode_info[];

            typedef struct m68k_operand {
                int isz; /* num words to put in d after decode */
                m68k_operand_mode_t mode;
                uint32_t d, rn; /* data, register number */
            } m68k_operand_t;

            typedef struct m68k_insn {
                m68k_opcode_t opcode;
                uint16_t sz, cc;
                int opcount;
                m68k_operand_t op[2];
            } m68k_insn_t;
        ''')

        self.c.emit('''
            #include <stdint.h>
            #include <stdlib.h>
            #include "m68k-decode.h"

            m68k_opcode_info_t m68k_opcode_info[] = {
                { "???", 0, 0 },
        ''')
        for opname in sorted(isa.opcodes):
            op = isa.opcodes[opname]
            name = op.name.replace('cc','').lower()
            has_cc = 1 if op.has_cc else 0
            has_sz = 1 if op.has_sz else 0
            self.c.emit('{"%s", %d, %d},' % (name, has_cc, has_sz))
        self.c.emit('''
            };

            static int decode_ea(m68k_operand_t *op, uint16_t ea)
            {
                op->rn = ea & 07;
                switch ((ea & 070) >> 3) {
                    case 0: op->mode = M68K_M_DATA; return 0;
                    case 1: op->mode = M68K_M_ADDR; return 0;
                    case 2: op->mode = M68K_M_INDIR; return 0;
                    case 3: op->mode = M68K_M_POST; return 0;
                    case 4: op->mode = M68K_M_PRE; return 0;
                    case 5: op->mode = M68K_M_DISP; op->isz = 1; return 0;
                    case 6: op->mode = M68K_M_INDEX; op->isz = 1; return 0;
                    case 7:
                    op->isz = 1;
                    switch (op->rn) {
                        case 0: op->mode = M68K_M_MEM_W; break;
                        case 1: op->mode = M68K_M_MEM_L; op->isz = 2; break;
                        case 2: op->mode = M68K_M_PCINDIR; break;
                        case 3: op->mode = M68K_M_PCINDEX; break;
                        case 4: op->mode = M68K_M_IMM; op->isz = 2; break;
                        default: return -1;
                    }
                    return 0;
                }
                return -1;
            }

            static int decode_swapped_ea(m68k_operand_t *op, uint16_t ea)
            {
                return decode_ea(op, ((ea>>3) & 07) | ((ea&07) << 3));
            }

            static int sz_tbl[] = { 1, 1, 2, -1 };
        ''')

    def emit_patterns(self, isa):
        for pat in isa.pats:
            self.emit_pattern(isa, pat)

    def emit_pattern(self, isa, pat):
        self.c.emit('static int %s(m68k_insn_t *insn, uint16_t op)\n{' %
                    (self.pat_fn_name(pat),))

        if len(pat.ops) > 0:
            self.emit_operand_decode(isa, pat, 0)
        if len(pat.ops) > 1:
            self.emit_operand_decode(isa, pat, 1)

        if pat.size in 'bwl':
            self.c.emit('insn->sz = %s;' % 'bwl'.index(pat.size))
        elif pat.size != '':
            self.c.emit('''
                if ((insn->sz = %s) == 3) {
                    insn->sz = 0;
                    return -1;
                }
            ''' % self.expr_field(pat, pat.size))
        if pat.op.has_cc:
            self.c.emit('insn->cc = %s;' % self.expr_field(pat, 'C'))

        self.c.emit('insn->opcode = M68K_OP_%s;' % pat.op.name)
        self.c.emit('return 0;')

        self.c.emit('}\n')

    def emit_operand_decode(self, isa, pat, n):
        op = pat.ops[n]
        oploc = '(insn->op[%d])' % n

        if type(op) is ISAOPERAND_EA:
            self.c.emit('if (%s(&%s, %s) < 0) return -1;' %
                ('decode_swapped_ea' if op.backwards else 'decode_ea',
                 oploc, self.expr_field(pat, op.p)))
        if type(op) is ISAOPERAND_IMM:
            self.c.emit('%s.mode = M68K_M_IMM;' % oploc)
            worry = False
            if op.p[0] == '@':
                sz = str(pat.nib[int(op.p[1])-1])
                if sz == '3':
                    sz = 'sz_tbl[%s]' % self.expr_field(pat, 'S')
                    worry = True
                self.c.emit('%s.isz = %s;' % (oploc, sz))
            else:
                self.c.emit('%s.d = %s;' % (oploc, self.expr_field(pat, op.p)))
            if worry:
                self.c.emit('''
                    if (%s.isz < 0) {
                        %s.isz = 0;
                        return -1;
                    }''' % (oploc, oploc))
        if type(op) is ISAOPERAND_REG:
            self.c.emit('%s.mode = M68K_M_SPECIAL;' % oploc)
            self.c.emit('%s.rn = M68K_R_%s;' % (oploc, op.p.upper()))
        if type(op) is ISAOPERAND_ADDR:
            self.c.emit('%s.mode = M68K_M_ADDR;' % oploc)
            self.c.emit('%s.rn = %s;' % (oploc, self.expr_field(pat, op.p)))
        if type(op) is ISAOPERAND_DATA:
            self.c.emit('%s.mode = M68K_M_DATA;' % oploc)
            self.c.emit('%s.rn = %s;' % (oploc, self.expr_field(pat, op.p)))
        if type(op) is ISAOPERAND_POST:
            self.c.emit('%s.mode = M68K_M_POST;' % oploc)
            self.c.emit('%s.rn = %s;' % (oploc, self.expr_field(pat, op.p)))
        if type(op) is ISAOPERAND_PRE:
            self.c.emit('%s.mode = M68K_M_PRE;' % oploc)
            self.c.emit('%s.rn = %s;' % (oploc, self.expr_field(pat, op.p)))
        if type(op) is ISAOPERAND_AOFFSET:
            self.c.emit('%s.mode = M68K_M_DISP;' % oploc)
            self.c.emit('%s.rn = %s;' % (oploc, self.expr_field(pat, op.p)))
            self.c.emit('%s.isz = 1;' % oploc)

    def emit_pat_table(self, isa):
        self.c.emit('''
            struct pat_table_row {
                uint16_t mask, bits;
                int opcount;
                int (*fn)(m68k_insn_t*, uint16_t);
            } pat_table[] = {''')
        for pat in isa.pats:
            self.c.emit('{ 0x%04x, 0x%04x, %d, %s }, /* %s */' %
                        (pat.mask, pat.bits, len(pat.ops),
                         self.pat_fn_name(pat), pat.op.name))
        self.c.emit('''{ 0, 0, 0, NULL }
            };\n''')

    def emit_pat_matcher(self, isa):
        self.c.emit('''
            static void decode_op(m68k_insn_t *insn, uint16_t op)
            {
                struct pat_table_row *cur;
                insn->opcode = M68K_OP_INVALID;
                insn->opcount = 0;
                insn->op[0].isz = 0;
                insn->op[1].isz = 0;
                insn->op[0].d = 0;
                insn->op[1].d = 0;
                insn->op[0].rn = 0;
                insn->op[1].rn = 0;
                insn->sz = -1;
                for (cur = pat_table; cur->fn != NULL; cur++) {
                    if ((op & cur->mask) == cur->bits) {
                        if (cur->fn(insn, op) >= 0) {
                            insn->opcount = cur->opcount;
                            break;
                        }
                        insn->sz = -1;
                    }
                }
            }''')

    def emit_decoder(self, isa):
        self.h.emit('extern uint16_t *m68k_decode(m68k_insn_t*, uint16_t*);');
        self.c.emit('''
            static uint16_t *grab_imm(m68k_operand_t *op, uint16_t *mem)
            {
                for (; op->isz > 0; op->isz--, mem++)
                    op->d = (op->d << 16) | *mem;
                return mem;
            }

            uint16_t *m68k_decode(m68k_insn_t *insn, uint16_t *mem)
            {
                decode_op(insn, *mem++);
                if (insn->opcount > 0) mem = grab_imm(&insn->op[0], mem);
                if (insn->opcount > 1) mem = grab_imm(&insn->op[1], mem);
                return mem;
            }''')

    def emit_footer(self, isa):
        self.h.emit('#endif')

    def emit_isa(self, isa):
        self.emit_header(isa)
        self.emit_patterns(isa)
        self.emit_pat_table(isa)
        self.emit_pat_matcher(isa)
        self.emit_decoder(isa)
        self.emit_footer(isa)

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--input', dest='inf',
        help='the input instruction table',
        default='m68k-instructions.txt')
    parser.add_argument('-o', '--output', dest='outf',
        help='the base output filename',
        default='m68k-decode')
    args = parser.parse_args()

    try:
        isa = ISA()
        isa.parse_file(args.inf)
        gen = CDECGEN(args.outf + '.c', args.outf + '.h')
        gen.emit_isa(isa)
        gen.close()
    except ABORTCOMPILE:
        sys.exit(1)
