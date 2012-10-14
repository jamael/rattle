#ifndef RATTLE_MODULE_H
#define RATTLE_MODULE_H

#include <rattle/conf.h>

#define RATTMOD_NAMEMAXSIZ 128

typedef struct {
	void *handle;	/* module handle */

	char const * const name;	/* module name */
	char const * const desc;	/* module description */
	char const * const version;	/* module version */

	char const *parent_name;	/* name of module parent */

	conf_decl_t *conftable;		/* config table */
	void *callbacks;		/* module callbacks */
} rattmod_entry_t;

extern int rattmod_register(rattmod_entry_t const *);

#endif /* RATTLE_MODULE_H */
