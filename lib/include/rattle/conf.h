#ifndef RATTLE_CONF_H
#define RATTLE_CONF_H

enum RATTCONFDT {	/* data type */
	RATTCONFDTSTR,	/* string */
	RATTCONFDTNUM,	/* number */
	RATTCONFDTANC,	/* anchor */
};

#define RATTCONFFLLST	0x1	/* Allow multiple value */
#define RATTCONFFLREQ	0x2	/* Config declaration is mandatory */

typedef struct ratt_conf ratt_conf_t;
struct ratt_conf {
	ratt_conf_t *parent;

	char const *name;		/* name of declaration */
	char const *desc;		/* description */

	union {
		char const *str;
		int const num;
	} defval;			/* default value */

	union {
		char **str;
		int *num;
	} val;				/* supplied value */

	enum RATTCONFDT datatype;	/* type of data */
	unsigned int flags;		/* optional flags */
};

int ratt_conf_load(char * const);

#endif /* RATTLE_CONF_H */
