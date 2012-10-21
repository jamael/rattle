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

	rattconf_decl_t *conftable;	/* config table */
	void *callbacks;		/* module callbacks */

	unsigned int flags;		/* module flags */
} rattmod_entry_t;

/**
 * \fn static inline int
 * rattmod_attach_from_config(char const *parname, rattconf_list_t *modules)
 * \brief attach modules from config setting
 *
 * Helper to attach modules found in a config list setting.
 *
 * \param parname	parent name to attach modules to
 * \param modules	list of modules name in a config setting
 */
static inline int
rattmod_attach_from_config(char const *parname, rattconf_list_t *modules)
{
	RATTLOG_TRACE();
	char **name = NULL;
	int retval;

	RATTCONF_LIST_FOREACH(modules, name)
	{
		retval = module_attach(parname, *name);
		if (retval != OK) {
			debug("module_attach() failed");
			return FAIL;
		}
	}

	return OK;
}

extern int rattmod_register(rattmod_entry_t const *);
extern int rattmod_unregister(rattmod_entry_t const *);

#endif /* RATTLE_MODULE_H */
