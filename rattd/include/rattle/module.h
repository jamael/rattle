#ifndef RATTLE_MODULE_H
#define RATTLE_MODULE_H

#include <rattle/args.h>
#include <rattle/conf.h>

/* maximum size of a module name */
#define RATTMODNAMSIZ	128

/* module flags */
#define RATTMODFLPAR	0x1	/* module is a parent */

typedef struct module_entry ratt_module_entry_t;
typedef struct {
	char const * const name;	/* parent name */
	unsigned int const ver_major;	/* major version */
	unsigned int const ver_minor;	/* minor version */

	/* attach callback */
	int (*attach)(ratt_module_entry_t const *);
} ratt_module_parent_t;

struct module_entry {
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
};

extern int ratt_module_register(ratt_module_entry_t const *);
extern int ratt_module_unregister(ratt_module_entry_t const *);
extern int ratt_module_attach_from_config(char const *, ratt_conf_list_t *);

#endif /* RATTLE_MODULE_H */
