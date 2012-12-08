#ifndef RATTLE_DEBUG_H
#define RATTLE_DEBUG_H

#include <rattle/def.h>
#include <rattle/log.h>	/* for debug() - fixme */

#define RATT_DEBUG_NAME		"debug"
#define RATT_DEBUG_VER_MAJOR	0
#define RATT_DEBUG_VER_MINOR	1
#define RATT_DEBUG_VER_PATCH	0

#ifdef DEBUG
#define debug_oops(x) debug("oops! (%s == NULL)", #x)
static inline void this_will_crash(char *oops)
{
	(*oops)++;
}
#define OOPS(x)						\
	do {						\
		if ((x) == NULL) {			\
			debug_oops(x);			\
			this_will_crash((char *) (x));	\
		}					\
	} while (0)
#else
#define OOPS(x)
#endif /* DEBUG */

typedef struct {
	int (*on_fork)(unsigned int);
} ratt_debug_hook_v1_t;

typedef union ratt_debug_hook {
	ratt_debug_hook_v1_t v1;
} ratt_debug_hook_t;

#define RATT_DEBUG_HOOK_VERSION 1
#define RATT_DEBUG_HOOK_SIZE sizeof(ratt_debug_hook_t)

#endif /* RATTLE_DEBUG_H */
