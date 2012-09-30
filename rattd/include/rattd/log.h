#ifndef RATTD_LOG_H
#define RATTD_LOG_H

#include <stdio.h>
#include <string.h>

enum LOGLEVEL {
	LOGERR = 0,	/* error */
	LOGWAR,		/* warning */
	LOGNOT,		/* notice */
#ifdef DEBUG
	LOGDBG,		/* debug */
	LOGTRA,		/* trace */
#endif

	LOGMAX		/* count; must be last */
};

static inline int log_name_to_level(const char *name)
{
	int i;
	const char *trsl[LOGMAX] = {
	/* exact same order as in LOGLEVEL */
		"error", "warning", "notice",
#ifdef DEBUG
		"debug", "trace",
#endif
	};

	for (i = 0; i < LOGMAX; ++i)
		if (strcmp(name, trsl[i]) == 0)
			break;

	return i;	/* LOGMAX if none found */
}

void log_msg(int, const char *, ...);

#define notice(fmt, args...) \
    log_msg(LOGNOT, "notice: " fmt "\n" , ## args)
#define warning(fmt, args...) \
    log_msg(LOGWAR, "warning: " fmt "\n" , ## args)
#define error(fmt, args...) \
    log_msg(LOGERR, "error: " fmt "\n" , ## args)

#ifdef DEBUG
#define debug(fmt, args...) \
    log_msg(LOGDBG, "debug: <%s:%i> " fmt "\n", __FILE__, __LINE__ , ## args)
#define LOG_TRACE \
    log_msg(LOGTRA, "trace: entering %s\n", __func__)
#else
#define debug(fmt, args...) while (0) { /* empty */ }
#define LOG_TRACE
#endif

#endif /* RATTD_LOG_H */
