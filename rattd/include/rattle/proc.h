#ifndef RATTLE_PROC_H
#define RATTLE_PROC_H

#include <signal.h>
#include <stdint.h>

#define RATT_PROC		"proc"	/* name of this module parent */
#define RATT_PROC_VER_MAJOR	0	/* major version */
#define RATT_PROC_VER_MINOR	1	/* minor version */

#define RATTPROCFLNTS	0x1	/* not thread-safe */

typedef struct {
	int (*on_start)();
	int (*on_stop)();
	void (*on_interrupt)(int, siginfo_t const *, void *);
	void (*on_unregister)(int (*)(void *));
	int (*on_register)(int (*)(void *), uint32_t, void *);
} ratt_proc_hook_t;

void ratt_proc_unregister(int (*)(void *));
int ratt_proc_register(int (*)(void *), uint32_t, void *);

#endif /* RATTLE_PROC_H */
