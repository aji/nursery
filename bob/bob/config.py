import logging

L = logging.getLogger('config')

import itertools
import os
import shlex
import sys

if 'STAGE' not in os.environ:
    print('warning: STAGE defaults to "devo"')
    os.environ['STAGE'] = 'devo'

STAGE = os.environ['STAGE']

if STAGE == 'prod':
    os.chdir('/opt/bob/run')

class _Node(object):
    def __init__(self, key, cfg):
        self._key = key
        self._kst = '.'.join(key)
        self._cfg = cfg
        self._ver = 0
        self._val = None

    def set_(self, v):
        self._cfg[self._kst] = str(v)

    def val_(self):
        if self._val is None or self._ver != self._cfg.version():
            self._ver = self._cfg.version()
            self._val = self._cfg[self._kst]
        return self._val

    def __repr__(self):
        return '${{{}={}}}'.format(self._kst, self.val_())

    def __str__(self):
        return str(self.val_())

    def __int__(self):
        v = self.val_()
        try:
            return int(v)
        except ValueError:
            exc = sys.exc_info()
            if v.lower() == 'true':
                return int(1)
            if v.lower() == 'false':
                return int(0)
            raise exc[0], exc[1], exc[2]

    def __long__(self):
        v = self.val_()
        try:
            return long(v)
        except ValueError:
            exc = sys.exc_info()
            if v.lower() == 'true':
                return long(1)
            if v.lower() == 'false':
                return long(0)
            raise exc[0], exc[1], exc[2]

    def __getattr__(self, k):
        return _Node(self._key + (k,), self._cfg)

    def __setattr__(self, k, v):
        if k[0] == '_':
            super(_Node, self).__setattr__(k, v)
        else:
            _Node(self._key + (k,), self._cfg).set_(v)

class _Line(object):
    def __init__(self, k, v):
        self._k = k.split('.')
        self._v = v

    def match(self, k):
        for a, b in itertools.izip_longest(self._k, k):
            if a != '*' and a != b:
                return False
        return True

class _Cfg(object):
    def __init__(self):
        self._lines = []
        self._version = 0

    def version(self):
        self._version

    def add_line(self, ln, line_number='<direct>'):
        s = shlex.split(ln, True)
        if len(s) == 0:
            return
        if len(s) != 3 or s[1] != '=' or '.' not in s[0]:
            L.warn('ignoring bad config file line %s: %s', line_number, repr(s))
            return
        self[s[0]] = s[2]

    def add_file(self, f):
        extra = ''
        line_number = 0
        for ln in f:
            line_number += 1
            if len(ln) == 1: # blank line
                continue
            if ln[-2] == '\\': # continuation escape
                extra += ln[:-2] + '\n'
                continue
            self.add_line(extra + ln, line_number)
            extra = ''
        if extra:
            L.warn('continuation character at end of file')
            self.add_line(extra, line_number)

    def add_path(self, path):
        L.info('reading configuration from %s', path)
        with open(path, 'r') as f:
            self.add_file(f)

    def __getitem__(self, k):
        kpart = [STAGE] + list(k.split('.'))
        for line in self._lines:
            if line.match(kpart):
                L.debug('{} => {}'.format(k, line._v))
                return line._v
        raise KeyError(k)

    def __setitem__(self, k, v):
        L.debug('{} = {}'.format(k, v))
        self._version += 1
        self._lines = [_Line(k, v)] + self._lines

    def __getattr__(self, k):
        return _Node((k,), self)

    def __setattr__(self, k, v):
        if k[0] == '_':
            super(_Cfg, self).__setattr__(k, v)
        else:
            _Node((k,), self).set_(v)

conf = _Cfg()
conf.add_path('config/global.cfg')
conf.add_path('config/secret.cfg')
