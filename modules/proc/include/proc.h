#ifndef RATTLE_PROC_H
#define RATTLE_PROC_H

#include <stdint.h>

#define RATT_PROC_NAME		"proc"	/* name of parent */
#define RATT_PROC_VER_MAJOR	0	/* major version */
#define RATT_PROC_VER_MINOR	1	/* minor version */

#define RATTPROCFLNTS	0x1	/* not thread-safe */ // should be opposite
#define RATTPROCFLSTC	0x2	/* sticky */

typedef struct {
	uint32_t flags;		/* process flags */
	void *lock;		/* process lock */
} ratt_proc_attr_t;

typedef struct {
	int (*on_start)();
	int (*on_stop)();
	void (*on_unregister)(int (*)(void *), ratt_proc_attr_t *, void *);
	int (*on_register)(int (*)(void *), ratt_proc_attr_t *, void *);
} ratt_proc_hook_v0_t;

typedef union ratt_proc_hook {
	ratt_proc_hook_v0_t v0;
} ratt_proc_hook_t;

#define RATT_PROC_HOOK_SIZE sizeof(ratt_proc_hook_t)

void ratt_proc_unregister(int (*)(void *), ratt_proc_attr_t *, void *);
int ratt_proc_register(int (*)(void *), ratt_proc_attr_t *, void *);

#endif /* RATTLE_PROC_H */
