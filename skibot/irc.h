#ifndef __INC_IRC_H__
#define __INC_IRC_H__

#include <stdio.h>

extern int irc_start(const char *opt);
extern int irc_save(FILE *save);
extern int irc_restore(FILE *save);

#endif
