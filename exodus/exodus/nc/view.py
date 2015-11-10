import sys, os
import string
import curses
import itertools

import exodus.rom
import exodus.data

from exodus.data import grouper

class InputModel(object):
    def on_input(self, ch):
        pass

    def draw_input(self, w, y, x):
        pass

class PromptInput(InputModel):
    def __init__(self, prompt, endfunc, cplfunc, *p, **kw):
        super(PromptInput, self).__init__(*p, **kw)
        self.prompt = prompt
        self.pfunc = endfunc
        self.tab = cplfunc
        self.s = ['', '']
        self.clipboard = ''

    def on_input(self, ch):
        c = chr(ch)

        if ch == 1 or ch == curses.KEY_HOME: # ^A
            self.s[1] = self.s[0] + self.s[1]
            self.s[0] = ''

        elif ch == 2 or ch == curses.KEY_LEFT: # ^B
            if len(self.s[0]) > 0:
                self.s[1] = self.s[0][-1] + self.s[1]
                self.s[0] = self.s[0][:-1]

        elif ch == 4: # ^D
            if len(self.s[1]) > 0:
                self.s[1] = self.s[1][1:]

        elif ch == 5 or ch == curses.KEY_END: # ^E
            self.s[0] = self.s[0] + self.s[1]
            self.s[1] = ''

        elif ch == 6 or ch == curses.KEY_RIGHT: # ^F
            if len(self.s[1]) > 0:
                self.s[0] = self.s[0] + self.s[1][0]
                self.s[1] = self.s[1][1:]

        elif ch == 8 or ch == 127: # ^H, delete
            if len(self.s[0]) > 0:
                self.s[0] = self.s[0][:-1]

        elif ch == 9: # ^I, tab
            if len(self.s[0]) > 0 and self.tab is not None:
                s = self.s[0].split(' ')
                cpl = self.tab(s[-1])
                if len(cpl) == 1:
                    s[-1] = cpl[0]
                elif len(cpl) > 1:
                    s[-1] = os.path.commonprefix(cpl)
                self.s[0] = ' '.join(s)
            else:
                self.s[0] += '\t'

        elif ch == 10 or ch == 13 or ch == curses.KEY_ENTER: # ^J, ^M, enter
            self.finish_input()

        elif ch == 11: # ^K
            self.clipboard = self.s[1]
            self.s[1] = ''

        elif ch == 20: # ^T
            if len(self.s[0]) > 0 and len(self.s[1]) > 0:
                a, b = self.s[0][:-1], self.s[0][-1:]
                c, d = self.s[1][:1], self.s[1][1:]
                self.s[0] = a + c + b
                self.s[1] = d

        elif ch == 21: # ^U
            self.clipboard = self.s[0]
            self.s[0] = ''

        elif ch == 23: # ^W
            if len(self.s[0]) > 0:
                s = self.s[0].split(' ')
                x = 0
                for i, t in enumerate(s):
                    if len(t) > 0:
                        x = i
                self.s[0] = ' '.join(s[:x]+[''])
                self.clipboard = ' '.join(s[x:])

        elif ch == 25: # ^Y
            self.s[0] += self.clipboard

        elif c in string.printable:
            self.s[0] = self.s[0] + c

        else:
            # bell?
            pass

    def finish_input(self):
        if self.pfunc is not None:
            self.pfunc(self.s[0] + self.s[1])

    def draw_input(self, w, y, x):
        w.move(y, x)
        w.clrtoeol()
        w.addstr(self.prompt + self.s[0])
        y, x = w.getyx()
        w.addstr(self.s[1])
        w.move(y, x)

class DataView(object):
    def __init__(self, data, win):
        self.d = data
        self.w = win

        self.show_hex_dump = False

        self.last_addr = None
        self.d_lines = [ ]
        self.w_lines = [ ]

    def fresh_lines(self, addr, nw=None):
        if nw is None:
            y, x = self.w.getmaxyx()
            nw = y

        self.w_lines = []

        while len(self.w_lines) < nw:
            addr = self._append_w_lines(addr)

        self.last_addr = addr

    def _append_w_lines(self, addr):
        ln = self.d.line(addr)

        notes = ln[3]

        for i in range(notes.get('blanks', 0)):
            self.w_lines.append(('','','','',''))

        if 'comment_big' in notes:
            self.w_lines.append(('','','','# '+notes['comment_big'],''))
        if 'label_big' in notes:
            self.w_lines.append(('','',notes['label_big']+':','',''))

        label = ''
        comment = ''
        if 'label' in notes:
            label = notes['label'] + ':'
        if 'comment' in notes:
            comment = '# ' + notes['comment']

        hexdump = (''.join(x) for x in
                    grouper(('{:02x}{:02x} '.format(a, b) for a, b in
                        grouper(ln[2], 2, 0)), 3, ''))
        if not self.show_hex_dump:
            hexdump = ['']

        lines = itertools.zip_longest(
            # address
            ['{:06x}:'.format(ln[1])],

            hexdump,
            [self.d.line_format(*ln)],

            fillvalue='')

        for x in lines:
            self.w_lines.append(x)

        return ln[1] + len(ln[2])

    def render_code(self, ln):
        tp, a, data, *ex = ln

        if tp == exodus.data.WT_STRING:
            return exodus.data.as_ascii(data)

        elif tp == exodus.data.WT_INSN:
            return exodus.data.as_insn(data)

        # otherwise, it's just data

        return exodus.data.as_data(data)

    def draw(self):
        h, w = self.w.getmaxyx()
        r = 0
        for x in self.w_lines[:h]:
            self.w.move(r, 0)
            self.w.addstr('{:7} '.format(x[0]))
            if self.show_hex_dump:
                self.w.addstr('{:15} '.format(x[1]))
            self.w.addstr(x[2])
            r += 1

class MainInput(InputModel):
    def __init__(self, mainview):
        self.m = mainview

    def on_input(self, ch):
        c = chr(ch)
        if c == 'q':
            self.m.main.stop()
        elif c == 'x':
            self.m.dv.show_hex_dump = not self.m.dv.show_hex_dump

class MainView(object):
    def __init__(self, main, win):
        self.main = main
        self.win = win
        self.inp = []
        self.push_input(MainInput(self))

        self.rom = exodus.rom.open('/home/alex/game/genesis/sonic1.bin')
        self.data = exodus.data.Data(self.rom)

        self.dv = DataView(self.data, self.win)
        self.dv.fresh_lines(0x100)

        self.cursor = 0x100

    def on_input(self, ch):
        if len(self.inp) > 0:
            self.inp[-1].on_input(ch)

    def push_input(self, inp):
        self.inp.append(inp)

    def pop_input(self):
        if len(self.inp) > 0:
            self.inp.pop()

    def draw(self):
        self.win.erase()
        self.dv.draw()
        if len(self.inp) > 0:
            self.inp[-1].draw_input(self.win, 0, 0)
