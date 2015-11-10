import traceback

class AnsiState(object):
    def __init__(self):
        self.bd = False
        self.em = False
        self.ul = False
        self.fg = None
        self.bg = None
        self.saved = 0

    def set_mode(self, cmds):
        # change locally so we can 'roll back' any changes

        if len(cmds) == 0:
            cmds = '0'

        bd = self.bd
        em = self.em
        ul = self.ul
        fg = self.fg
        bg = self.bg

        for cmd in cmds.split(';'):
            i = int(cmd)
            if i == 0:
                bd = False
                em = False
                ul = False
                fg = None
                bg = None
            if i == 1:
                bd = True
            if i == 3:
                em = True
            if i == 4:
                ul = True
            if i == 21:
                bd = False
            if i == 22:
                bd = False
            if i == 23:
                em = False
            if i == 24:
                ul = False
            if 30 <= i and i <= 37:
                fg = i - 30
            if i == 39:
                fg = None
            if 40 <= i and i <= 47:
                bg = i - 30
            if i == 49:
                bg = None

        self.bd = bd
        self.em = em
        self.ul = ul
        self.fg = fg
        self.bg = bg

_ansi_bad = AnsiState()
_ansi_bad.bd = True
_ansi_bad.fg = 7
_ansi_bad.bg = 0

class Character(object):
    def __init__(self, sym, ansi):
        self.sym = sym
        self.bd = ansi.bd
        self.em = ansi.em
        self.ul = ansi.ul
        self.fg = ansi.fg
        self.bg = ansi.bg

    @classmethod
    def blank(cls):
        return cls(' ', AnsiState())

class CharacterList(object):
    def __init__(self):
        self.chars = []
        self.column = 0

    def putchar(self, c):
        while not len(self.chars) > self.column:
            self.chars.append(Character.blank())
        self.chars[self.column] = c
        self.column += 1

ENTITY_MAP = {
    '<': '&lt;',
    '>': '&gt;',
    '&': '&amp;',
    }

FG_PALETTE = [ '#000', '#c00', '#0c0', '#cc0', '#00c', '#c0c', '#0cc', '#ccc' ]
BG_PALETTE = [ '#444', '#f44', '#4f4', '#ff4', '#44f', '#f4f', '#4ff', '#fff' ]

def htmlify_characters(chars):
    s = ''

    bdp = False
    emp = False
    ulp = False
    fgp = None
    bgp = None

    span = False
    was_space = True

    for c in chars.chars:
        if bdp!=c.bd or emp!=c.em or ulp!=c.ul or fgp!=c.fg or bgp!=c.bg:
            if span:
                s += '</span>'
            css = []
            if c.bd:
                css.append('font-weight:bold')
            if c.em:
                css.append('font-style:italic')
            if c.ul:
                css.append('text-decoration:underline')
            if c.fg:
                css.append('color:{}'.format(FG_PALETTE[c.fg]))
            if c.bg:
                css.append('background-color:{}'.format(BG_PALETTE[c.fg]))
            s += '<span style="{}">'.format(';'.join(css))
            span = True

        if c.sym == ' ' and was_space:
            s += '&nbsp;'
        elif c.sym in ENTITY_MAP:
            s += ENTITY_MAP[c.sym]
        elif ord(c.sym) >= 0x80:
            s += '&#{};'.format(ord(c.sym))
        else:
            s += c.sym

        was_space = c.sym == ' '

        bdp = c.bd
        emp = c.em
        ulp = c.ul
        fgp = c.fg
        bgp = c.bg

    if span:
        s += '</span>'

    return '<span style="font-family:monospace">{}</span>'.format(s)

def htmlify_ansi(s, ansi):
    chars = CharacterList()

    try:
        tmp = s.decode('utf-8')
        s = tmp
    except UnicodeDecodeError:
        pass

    def try_ansi(s, i):
        if i+1 >= len(s):
            return None
        if s[i+1] != '[':
            return i + 2
        i += 2
        j = i
        while j < len(s) and s[j] in '0123456789;?':
            j = j + 1
        if j >= len(s):
            return None

        arg = s[i:j]
        iarg = '0' if len(arg) == 0 else arg

        if s[j] == 'm':
            try:
                ansi.set_mode(s[i:j])
            except:
                traceback.print_exc()
                return None
        elif s[j] == 'C':
            chars.column += int(iarg)
        elif s[j] == 'D':
            chars.column -= int(iarg)
            if chars.column <= 0:
                chars.column = 0
        elif s[j] == 'K' or s[j] == 'J':
            if iarg == '0':
                chars.chars = chars.chars[:chars.column]
            if iarg == '1':
                for i in range(chars.column):
                    if i >= len(chars.chars):
                        break
                    chars.chars[i] = Character.blank()
            if iarg == '2':
                chars.chars = []
        elif s[j] == 's':
            ansi.saved = chars.column
        elif s[j] == 'u':
            chars.column = ansi.saved

        return j + 1

    i = 0
    while i < len(s):
        if s[i] == '\r':
            chars.column = 0
            i += 1
            continue

        if s[i] == '\x0f':
            # seems to mean 'clear modes'
            ansi.set_mode('0')
            i += 1
            continue

        if s[i] == '\x1b':
            j = try_ansi(s, i)
            if j is not None:
                i = j
                continue

        if ord(s[i]) < 0x20:
            for c in '<{:02x}>'.format(ord(s[i])):
                chars.putchar(Character(c, _ansi_bad))
        else:
            chars.putchar(Character(s[i], ansi))
        i += 1

    return htmlify_characters(chars)

def htmlify_log_line(line_number, s, ansi):
    fields = s.split(' ', 1)
    if len(fields) == 0:
        return ''
    if len(fields) == 1:
        return fields[0]
    return '''
        <div class="line" id="L{line}">
            <span class="date"><a href="#L{line}"><span
            class="linenr">{line:6d}</span>: {date}</a></span>
            <a name="L{line}">{text}</a>
        </div>
    '''.format(
        line = line_number + 1,
        date = fields[0],
        text = htmlify_ansi(' '.join(fields[1:]), ansi),
        )

def htmlify_log(s):
    ansi = AnsiState()
    lines = enumerate(s.split('\n'))
    return '\n'.join(htmlify_log_line(i, ln, ansi) for i, ln in lines)
