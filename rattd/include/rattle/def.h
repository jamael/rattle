#ifndef RATTLE_DEF_H
#define RATTLE_DEF_H

#include <limits.h>
#include <stddef.h>

#define FAIL	0
#define OK	1

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef SIZE_MAX
#define SIZE_MAX (~((size_t) 0))
#endif

#endif /* RATTLE_DEF_H */
