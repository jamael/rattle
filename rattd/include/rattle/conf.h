#ifndef RATTLE_CONF_H
#define RATTLE_CONF_H

#include <rattle/table.h>

enum RATTCONFDT {		/* type of value */
	RATTCONFDTSTR,		/* string */
	RATTCONFDTNUM8,		/* int8 */
	RATTCONFDTNUM16,	/* int16 */
	RATTCONFDTNUM32,	/* int32 */
};

#define RATTCONFFLLST	0x1	/* allow multiple value */
#define RATTCONFFLREQ	0x2	/* declaration is mandatory */
#define RATTCONFFLUNS	0x4	/* value is unsigned */

typedef struct {
	char const * const path;	/* path of declaration */
	char const * const desc;	/* description */
	char const **defval;		/* default value */
	void *value;			/* config value */
	enum RATTCONFDT type;		/* type of value */
	unsigned int flags;		/* optional flags */
} rattconf_decl_t;

typedef rattconf_decl_t conf_decl_t;

#define RATTCONF_DEFVAL(defval, def...) \
    char const *(defval)[] = { def, '\0' }
#define RATTCONF_LIST_INIT(tab) RATT_TABLE_INIT((tab))
#define RATTCONF_LIST_FOREACH(tab, x) RATT_TABLE_FOREACH((tab), (x))

#endif /* RATTLE_CONF_H */
