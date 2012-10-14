#ifndef RATTLE_LOG_H
#define RATTLE_LOG_H

#include <stdio.h>
#include <string.h>

#define RATTLOG	"logger"	/* name of this module parent */
#define RATTLOG_VERSION_MAJOR 0	/* major version */
#define RATTLOG_VERSION_MINOR 1 /* minor version */

#define RATTLOG_MSGSIZMAX 256	/* includes trailing NULL byte */

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

extern void rattlog_msg(int, const char *, ...);

#define notice(fmt, args...) rattlog_msg(RATTLOGNOT, fmt "\n" , ## args)
#define warning(fmt, args...) rattlog_msg(RATTLOGWAR, fmt "\n" , ## args)
#define error(fmt, args...) rattlog_msg(RATTLOGERR, fmt "\n" , ## args)

#ifdef DEBUG
#define debug(fmt, args...) rattlog_msg(RATTLOGDBG, \
    "<%s:%s:%i> " fmt "\n", __func__, __FILE__, __LINE__ , ## args)
#define RATTLOG_TRACE() rattlog_msg(RATTLOGTRA, \
    "<%s:%i> entering %s\n", __FILE__, __LINE__, __func__)
#else
#define debug(fmt, args...) while (0) { /* empty */ }
#define RATTLOG_TRACE()
#endif

#define RATTLOG_CALLBACK_VERSION 1
typedef struct {
	void (*on_msg)(int, const char *);
} rattlog_callback_t;

#endif /* RATTLE_LOG_H */
