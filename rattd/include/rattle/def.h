#ifndef RATTLE_DEF_H
#define RATTLE_DEF_H

#include <limits.h>
#include <stddef.h>

/* function return values */
#define FAIL		0	/* failed */
#define OK		1	/* succeeded */
/* core init return values */
#define STOP		2	/* stop request */
#define NOSTART		3	/* not started */
/* compare return values */
#define MATCH		OK	/* conditions matched */
#define NOMATCH		FAIL	/* conditions did not match */

/* maximum size of a path, including trailing zero byte */
#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

/* maximum value a size_t can hold */
#ifndef SIZE_MAX
#define SIZE_MAX	(~((size_t) 0))
#endif

#endif /* RATTLE_DEF_H */
