#ifndef RATTLE_LOG_H
#define RATTLE_LOG_H

#include <stdio.h>

#define notice(fmt, args...) fprintf(stdout, "notice: " fmt "\n" , ## args)
#define warning(fmt, args...) fprintf(stdout, "warning: " fmt "\n" , ## args)
#define error(fmt, args...) fprintf(stderr, "error: " fmt "\n" , ## args)

#ifdef DEBUG
#define debug(fmt, args...) \
    fprintf(stdout, "debug: <%s:%i> " fmt "\n", __FILE__, __LINE__ , ## args)
#else
#define debug(fmt, args...) while (0) { /* empty */ }
#endif

#endif /* RATTLE_LOG_H */
