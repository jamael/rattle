#ifndef RATTLE_MODULE_H
#define RATTLE_MODULE_H

#include <rattle/conf.h>

#define RATTMOD_NAMEMAXSIZ 128

#define RATTMODFLPAR	0x1	/* module is a parent */

typedef struct {
	void *handle;	/* module handle */

	char const * const name;	/* module name */
	char const * const desc;	/* module description */
	char const * const version;	/* module version */

	char const *parent_name;	/* name of module parent */

	conf_decl_t *conftable;		/* config table */
	void *callbacks;		/* module callbacks */

	unsigned int flags;		/* module flags */
} rattmod_entry_t;

extern int rattmod_register(rattmod_entry_t const *);
extern int rattmod_unregister(rattmod_entry_t const *);

#endif /* RATTLE_MODULE_H */
