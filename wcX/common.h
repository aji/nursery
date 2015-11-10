/* common things */

#ifndef __INC_COMMON_H__
#define __INC_COMMON_H__

#define DEFAULT   0
#define DEBUG     1
#define INFO      2
#define WARNING   3
#define ERROR     4
#define FATAL     5

extern void Log(unsigned level, const char *fmt, ...);

extern void Abort(const char *msg, ...);

#endif
