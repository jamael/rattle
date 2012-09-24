#ifndef RATTLE_CONF_H
#define RATTLE_CONF_H

enum RATTCONFDT {	/* data type */
	RATTCONFDTSTR,	/* string */
	RATTCONFDTNUM8,	/* int8 */
	RATTCONFDTNUM16,/* int16 */
	RATTCONFDTNUM32,/* int32 */
};

#define RATTCONFFLLST	0x1	/* Allow multiple value */
#define RATTCONFFLREQ	0x2	/* Config declaration is mandatory */
#define RATTCONFFLUNS	0x4	/* Value is unsigned */

#define RATTCONFLSTSIZ	10	/* Fixed-size for defval list table */

typedef struct ratt_conf ratt_conf_t;
struct ratt_conf {
	char const *path;		/* absolute path of declaration */
	char const *desc;		/* description */

	union {
		char const *str;			/* string */
		char const *strlst[RATTCONFLSTSIZ];	/* string list */
		long long const num;			/* numeric */
		long long const *numlst[RATTCONFLSTSIZ];/* numeric list */
	} defval;			/* default value */

	union {
		char **str;		/* string pointer */
		char **strlst;		/* string list pointer */
		long long *num;		/* numeric pointer */
		long long **numlst;	/* numeric list pointer */
	} val;				/* config value */

	enum RATTCONFDT datatype;	/* type of data */
	unsigned int flags;		/* optional flags */
};

#endif /* RATTLE_CONF_H */
