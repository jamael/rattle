#ifndef RATTLE_PROC_H
#define RATTLE_PROC_H

#include <signal.h>
#include <stdint.h>

#define RATTPROC	"proc"	/* name of this module parent */
#define RATTPROC_VERSION_MAJOR 0	/* major version */
#define RATTPROC_VERSION_MINOR 1	/* minor version */

#define RATTPROCFLNTS	0x1	/* not thread-safe */

#define RATTPROC_CALLBACK_VERSION 1
typedef struct {
	int (*on_start)();
	int (*on_stop)();
	void (*on_interrupt)(int, siginfo_t const *, void *);
	void (*on_unregister)(void (*)(void *));
	int (*on_register)(void (*)(void *), uint32_t, void *);
} rattproc_callback_t;

void rattproc_unregister(void (*)(void *));
int rattproc_register(void (*)(void *), uint32_t, void *);

#endif /* RATTLE_PROC_H */
