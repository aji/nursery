#ifndef __INC_ROM_H__
#define __INC_ROM_H__

#include <stdint.h>

#define ROM_N_WORDS 0x200000

#define ROM_WORD_DATA  0x0001
#define ROM_WORD_CODE  0x0002

extern uint16_t rom_data[ROM_N_WORDS];
extern uint16_t rom_info[ROM_N_WORDS];
extern size_t rom_words;

extern void rom_reset(void);
extern int rom_load(const char *path);

#endif
