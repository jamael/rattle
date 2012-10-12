#ifndef RATTLE_MODULE_H
#define RATTLE_MODULE_H

#include <rattle/conf.h>
#include <rattle/log.h>

enum RATTMODPT {	/* module path */
	RATTMODPTLOG = 0,	/* logger */
	RATTMODPTMAX		/* count, must be last */
};

typedef struct {
	const char * const name;	/* module name */
	const char * const desc;	/* module description */
	const char * const version;	/* module version */

	conf_decl_t *conftable;		/* config table */

	const struct {
		enum RATTMODPT path;
		union {
			/* logger hooks */
			rattlog_hook_t * const log;
		} u;
	} hook[];	/* NULL-terminated hook table */
} rattmod_entry_t;

typedef struct {
	int const path;
	int const path_version;
} module_path_t;

#endif /* RATTLE_MODULE_H */
