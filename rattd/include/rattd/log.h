#ifndef RATTD_LOG_H
#define RATTD_LOG_H

#include <stdio.h>
#include <string.h>

enum RATTLOGLEVEL {
	RATTLOGERR = 0,	/* error */
	RATTLOGWAR,	/* warning */
	RATTLOGNOT,	/* notice */
#ifdef DEBUG
	RATTLOGDBG,	/* debug */
	RATTLOGTRA,	/* trace */
#endif
	RATTLOGMAX	/* count; must be last */
};

static const char *rattlog_level_name[RATTLOGMAX] = {
	/* exact same order as in RATTLOGLEVEL */
	"error", "warning", "notice",
#ifdef DEBUG
	"debug", "trace",
#endif
};

static inline int rattlog_name_to_level(const char *name)
{
	int i;
	for (i = RATTLOGERR; i < RATTLOGMAX; ++i)
		if (strcmp(name, rattlog_level_name[i]) == 0)
			return i;

	return RATTLOGMAX;	/* RATTLOGMAX if none found */
}

static inline const char *rattlog_level_to_name(int level)
{
	if (level >= RATTLOGERR && level < RATTLOGMAX)
		return rattlog_level_name[level];
	return "unknown";
}

extern void log_msg(int, const char *, ...);

#define notice(fmt, args...) log_msg(RATTLOGNOT, fmt "\n" , ## args)
#define warning(fmt, args...) log_msg(RATTLOGWAR, fmt "\n" , ## args)
#define error(fmt, args...) log_msg(RATTLOGERR, fmt "\n" , ## args)

#ifdef DEBUG
#define debug(fmt, args...) \
    log_msg(RATTLOGDBG, "<%s:%i> " fmt "\n", __FILE__, __LINE__ , ## args)
#define RATTLOG_TRACE() log_msg(RATTLOGTRA, "entering %s\n", __func__)
#else
#define debug(fmt, args...) while (0) { /* empty */ }
#define RATTLOG_TRACE()
#endif

#endif /* RATTD_LOG_H */
