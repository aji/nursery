#!/usr/bin/python

from distutils.core import setup, Extension

import sys, os
from lib import m68k_gen

dec_inf = 'lib/m68k-instructions.txt'
dec_out = 'lib/m68k-decode'
print('generating m68k decoder...')
try:
    isa = m68k_gen.ISA()
    isa.parse_file(dec_inf)
    gen = m68k_gen.CDECGEN(dec_out + '.c', dec_out + '.h')
    gen.emit_isa(isa)
    gen.close()
except m68k_gen.ABORTCOMPILE:
    print('generating m68k decoder failed')
    sys.exit(1)

setup(name='exodus', version='1.0',
      description='68000 disassembly tool',
      packages=['exodus', 'exodus.gui', 'exodus.nc'],
      ext_modules=[
        Extension('exodus.codec',
          sources=['exodus/codec.c', 'lib/m68k-decode.c'],
          depends=['lib/m68k-decode.h'],
          include_dirs=['.'],
        ),
        Extension('exodus.rom',
          sources=['exodus/rom.c'],
          include_dirs=['.'],
        ),
      ])
