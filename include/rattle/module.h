#ifndef RATTLE_MODULE_H
#define RATTLE_MODULE_H

#include <stddef.h>

#include <rattle/args.h>
#include <rattle/conf.h>
#include <rattle/core.h>
#include <rattle/table.h>

#define RATT_MODULE_VERSION	1

/* module name size (maximum) */
#define RATTMODNAMSIZ	32
/* module flags */
#define RATTMODFLCOR	0x1	/* module is a core */

typedef struct ratt_module {
	void *handle;			/* module handle */

	char const * const name;	/* module name */
	char const * const desc;	/* module description */
	char const * const version;	/* module version */
	unsigned int flags;		/* module flags */

	ratt_args_t *args;		/* arguments table */
	ratt_conf_t *config;		/* config table */

	/* attach callback */
	int (*attach)(ratt_core_hook_t *);
	/* detach callback */
	void (*detach)(void);
	/* constructor callback */
	int (*constructor)(void);
	/* destructor callack */
	void (*destructor)(void);

	ratt_core_t const *core;	/* core information */
	size_t const hook_size;		/* module hook size */
} ratt_module_t;

extern int ratt_module_register(ratt_module_t const *, int);
extern int ratt_module_unregister(ratt_module_t const *);
extern int ratt_module_attach(ratt_core_t const *, char const *);

#define RATT_MODULE_INIT(x, e)						\
	int __module_init_ ## x (void *handle)				\
	{								\
		(e)->handle = handle;					\
		return ratt_module_register(e, RATT_MODULE_VERSION);	\
	}

#endif /* RATTLE_MODULE_H */
