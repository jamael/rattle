#ifndef RATTLE_ARGS_H
#define RATTLE_ARGS_H

#define RATTARGSFLARG	0x1	/* argument is mandatory */
#define RATTARGSFLONE	0x2	/* do not tolerate the same option twice */

typedef struct {
	int const option;		/* option identifier */
	char const * const arg;		/* argument name */
	char const * const desc;	/* description */
	int *found;			/* found */
	int (*get)(char const *);	/* get args callback */
	int flags;			/* flags */
} ratt_args_t;

#endif /* RATTLE_ARGS_H */
