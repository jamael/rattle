#ifndef RATTLE_LOG_H
#define RATTLE_LOG_H

#include <stdio.h>
#include <string.h>

#define RATT_LOG		"log"	/* name of this module parent */
#define RATT_LOG_VER_MAJOR	0	/* major version */
#define RATT_LOG_VER_MINOR	1 	/* minor version */

#define RATTLOGMSGSIZ		256	/* includes trailing NULL byte */
#define RATTLOGLVLSIZ		10	/* ditto. */

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

static const char *ratt_log_level_to_name[RATTLOGMAX] = {
	/* exact same order as in RATTLOGLEVEL */
	"error", "warning", "notice",
#ifdef DEBUG
	"debug", "trace",
#endif
};

static inline int ratt_log_level(const char *name)
{
	int i;
	for (i = RATTLOGERR; i < RATTLOGMAX; ++i)
		if (strcmp(name, ratt_log_level_to_name[i]) == 0)
			return i;

	return RATTLOGMAX;	/* RATTLOGMAX if none found */
}

static inline const char *ratt_log_level_name(int level)
{
	if (level >= RATTLOGERR && level < RATTLOGMAX)
		return ratt_log_level_to_name[level];
	return "unknown";
}

extern void ratt_log_msg(int, const char *, ...);

#define notice(fmt, args...) ratt_log_msg(RATTLOGNOT, fmt "\n" , ## args)
#define warning(fmt, args...) ratt_log_msg(RATTLOGWAR, fmt "\n" , ## args)
#define error(fmt, args...) ratt_log_msg(RATTLOGERR, fmt "\n" , ## args)

#ifdef DEBUG
#define debug(fmt, args...) ratt_log_msg(RATTLOGDBG, \
    "<%s:%s:%i> " fmt "\n", __func__, __FILE__, __LINE__ , ## args)
#define RATTLOG_TRACE() ratt_log_msg(RATTLOGTRA, \
    "<%s:%i> entering %s\n", __FILE__, __LINE__, __func__)
#else
#define debug(fmt, args...) while (0) { /* empty */ }
#define RATTLOG_TRACE()
#endif

typedef struct {
	void (*on_msg)(int, const char *);
} ratt_log_hook_t;

#endif /* RATTLE_LOG_H */
