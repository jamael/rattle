#ifndef RATTLE_MODULE_H
#define RATTLE_MODULE_H

#include <stddef.h>

#include <rattle/args.h>
#include <rattle/conf.h>
#include <rattle/table.h>

/* maximum size of a module name */
#define RATTMODNAMSIZ	128

/* module flags */
#define RATTMODFLPAR	0x1	/* module is a parent */

/* parent flags */
#define RATTMODPARFLONE	0x1	/* only one module can attach */

typedef struct ratt_module_entry ratt_module_entry_t;
typedef struct ratt_module_parent ratt_module_parent_t;

struct ratt_module_parent {
	char const * const name;	/* parent name */
	unsigned int const ver_major;	/* major version */
	unsigned int const ver_minor;	/* minor version */
	char const * const conf_label;	/* name of config section */
	ratt_table_t *hook_table;	/* parent hook table */
	size_t const hook_size;		/* parent hook size */
	unsigned int flags;		/* parent flags */

	/* attach callback */
	int (*attach)(ratt_module_entry_t const *);
	/* detach callback */
	int (*detach)(ratt_module_entry_t const *);
};

struct ratt_module_entry {
	void *handle;			/* module handle */

	char const * const name;	/* module name */
	char const * const desc;	/* module description */
	char const * const version;	/* module version */
	unsigned int flags;		/* module flags */

	ratt_args_t *args;		/* arguments table */
	ratt_conf_t *config;		/* config table */

	/* attach callback */
	void *(*attach)(ratt_module_parent_t const *);
	/* detach callback */
	void (*detach)(void);
	/* constructor callback */
	int (*constructor)(void);
	/* destructor callack */
	void (*destructor)(void);

	ratt_module_parent_t const *parinfo;	/* parent information */
	size_t const hook_size;			/* module hook size */
};

extern int ratt_module_register(ratt_module_entry_t const *);
extern int ratt_module_unregister(ratt_module_entry_t const *);
extern int ratt_module_attach(char const *, char const *);

#endif /* RATTLE_MODULE_H */
