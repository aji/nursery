import time
from exodus import rom, data

r = rom.open('/home/alex/game/genesis/sonic1.bin')
d = data.Data(r)
