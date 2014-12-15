#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <stdlib.h>

#define __log__(fmt, args...) fprintf(stderr, "%s:%d " fmt "\n", __FILE__, __LINE__, ##args)

#define warn(fmt, args...) __log__("Warning: %s:%d " fmt, __FILE__, __LINE__, ##args)
#define fatal(fmt, args...) do { __log__("Fatal: " fmt, ##args); abort(); } while (0)

#endif

