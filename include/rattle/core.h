#ifndef RATTLE_CORE_H
#define RATTLE_CORE_H

#include <rattle/table.h>

typedef struct ratt_core ratt_core_t;
typedef struct ratt_core_hook ratt_core_hook_t;
#include <rattle/module.h>

#define RATT_CORE_VERSION	1

/* core states */
typedef enum ratt_core_state {
	RATTCORSTBOOT = 0,	/* booting */
	RATTCORSTINIT,		/* initialized */
	RATTCORSTSTAR,		/* started */
	RATTCORSTSTOP,		/* stopped */
	RATTCORSTFINI,		/* finished */
} ratt_module_core_state_t;

/* core initialization data */
typedef struct ratt_core_init {
	enum ratt_core_state_t state;	/* core state */
	ratt_table_t *hook_table;	/* core hook table */
	void *udata;			/* core user data */
} ratt_core_init_t;

/* core information */
struct ratt_core {
	char const * const name;	/* name */
	unsigned int const ver_major;	/* version major number */
	unsigned int const ver_minor;	/* version minor number */
	unsigned int flags;		/* flags */

	char const * const conf_section;	/* config section */
	char const * const args_section;	/* arguments section */

	size_t const hook_size;		/* maximum size of hook info */

	/* attach hook callback */
	int (*attach_hook)(ratt_core_hook_t const *);
	/* detach hook callback */
	int (*detach_hook)(ratt_core_hook_t const *);
	/* start callback */
	int (*start)(ratt_core_init_t *);
	/* stop callback */
	int (*stop)(ratt_core_init_t *);
	/* init callback */
	int (*init)(ratt_core_init_t *);
	/* fini callback */
	int (*fini)(ratt_core_init_t *);
};

/* core hook information */
struct ratt_core_hook {
	ratt_module_t const *module;	/* module hooked */
	unsigned int version;		/* hook version */
	void *hook;			/* hook pointer */
};

#endif /* RATTLE_CORE_H */
