/*
 * RATTLE configuration handler
 * Copyright (c) 2012, Jamael Seun
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <libconfig.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rattle/args.h>
#include <rattle/def.h>
#include <rattle/conf.h>
#include <rattle/log.h>

#include "args.h"

#ifndef CONF_FILEPATH
#define CONF_FILEPATH	"/etc/rattle/rattd.conf"
#endif

/* initial size of a value list */
#define CONF_LSTTABSIZ		2

/* maximum size of a declaration path */
#define CONF_SETTPATHMAX	128

/* arguments section identifier */
#define CONF_ARGSSEC_ID		'C'

/* config defaults */
static char const l_default_conf_file[] = CONF_FILEPATH;

/* config arguments */
static char const *l_args_conf_file = l_default_conf_file;
static int get_args_conf_file(char const *path)
{
	if (strlen(path) >= PATH_MAX) {
		error("%s: path is too long", path);
		return FAIL;
	}

	l_args_conf_file = path;

	return OK;
}

static ratt_args_t l_args[] = {
	{ 'f', "filepath", "use specified configuration file",
	    NULL, get_args_conf_file, 0 },
	{ 0 }
};

/* config root */
static struct config_t l_cfg;

static int check_num(int type, int num, int unsign)
{
	unsigned int u_num = num;

	switch(type) {
	case RATTCONFDTNUM8:
		if (unsign && ((u_num > UCHAR_MAX) || (num < 0))) {
			error("`%i' out of bound (%u to %u).",
		 	    num, 0, UCHAR_MAX);
			return FAIL;
		} else if (!unsign && ((num > SCHAR_MAX)
		    || (num < SCHAR_MIN))) {
			error("`%i' out of bound (%i to %i).",
		 	    num, SCHAR_MIN, SCHAR_MAX);
			return FAIL;
		}
		break;
	case RATTCONFDTNUM16:
		if (unsign && ((u_num > USHRT_MAX) || (num < 0))) {
			error("`%i' out of bound (%u to %u).",
		 	    num, 0, USHRT_MAX);
			return FAIL;
		} else if (!unsign && ((num > SHRT_MAX)
		    || (num < SHRT_MIN))) {
			error("`%i' out of bound (%i to %i).",
		 	    num, SHRT_MIN, SHRT_MAX);
			return FAIL;
		}
		break;
	case RATTCONFDTNUM32:
		if (unsign && (num < 0)) {
			error("`%i' out of bound (%u to %u).",
		 	    num, 0, UINT_MAX);
			return FAIL;
		} else if (!unsign && ((num > INT_MAX)
		    || (num < INT_MIN))) {
			error("`%i' out of bound (%i to %i).",
		 	    num, INT_MIN, INT_MAX);
			return FAIL;
		}
		break;
	case RATTCONFDTSTR:
		/* empty */
		break;
	default:
		debug("invalid value type `%i'", type);
		return FAIL;
	}
	return OK;
}

static int unset_value(void *value, int type)
{
	char **str = value;

	switch (type) {
	case RATTCONFDTSTR:
		if (*str) {
			debug("freeing string at %p", *str);
			free(*str);
			*str = NULL;
		}
		break;
	case RATTCONFDTNUM8:
	case RATTCONFDTNUM16:
	case RATTCONFDTNUM32:
		/* empty */
		break;
	default:
		debug("invalid value type `%i'", type);
		return FAIL;
	}

	return OK;
}

static int set_value(void *dst, void const * const src, int type, int flags)
{
	void *tail = NULL;
	char **str = dst;
	int retval;

	if (!dst || !src) {
		debug("either src or dst pointer is NULL");
		return FAIL;
	}

	/*
	 * We check for numeric boundaries first
	 * -- two reasons I see:
	 *
	 * 1. Saves the work of a _get_tail_next() if a list is requested
	 *    and the numeric value is rejected soon after.
	 *
	 * 2. This makes the code easier to read than multiple
	 *    check_num() in the switch()-case below.
	 */
	if ((type == RATTCONFDTNUM8)
	    || (type == RATTCONFDTNUM16)
	    || (type == RATTCONFDTNUM32)) {
		retval = check_num(type, *((int *) src),
		    (flags & RATTCONFFLUNS));
		if (retval != OK) {
			debug("check_num() failed");
			return FAIL;
		}
	}

	if (flags & RATTCONFFLLST) {
		retval = ratt_table_get_tail_next(dst, &tail);
		if (retval != OK) {
			debug("ratt_table_get_tail_next() failed");
			return FAIL;
		}
		str = dst = tail;
	}

	switch (type) {
	case RATTCONFDTSTR:
		*str = strdup((char *) src);
		if (!(*str)) {
			error("memory allocation failed");
			debug("strdup() failed");
			return FAIL;
		}
		break;
	case RATTCONFDTNUM8:
		memcpy(dst, src, sizeof(int8_t));
		break;
	case RATTCONFDTNUM16:
		memcpy(dst, src, sizeof(int16_t));
		break;
	case RATTCONFDTNUM32:
		memcpy(dst, src, sizeof(int32_t));
		break;
	default:
		debug("invalid value type `%i'", type);
		return FAIL;
	}

	return OK;
}

static int list_destroy(ratt_table_t *table, int type)
{
	void *value = NULL;
	int retval;

	RATT_TABLE_FOREACH(table, value)
	{
		unset_value(value, type);
	}

	retval = ratt_table_destroy(table);
	if (retval != OK) {
		debug("ratt_table_destroy() failed");
		return FAIL;
	}

	return OK;
}

static int list_create(ratt_table_t *table, size_t cnt, int type)
{
	size_t size = 0;
	int retval;

	if (!cnt)
		cnt = CONF_LSTTABSIZ;

	switch (type) {
	case RATTCONFDTSTR:
		size = sizeof(char *);
		break;
	case RATTCONFDTNUM8:
		size = sizeof(int8_t);
		break;
	case RATTCONFDTNUM16:
		size = sizeof(int16_t);
		break;
	case RATTCONFDTNUM32:
		size = sizeof(int32_t);
		break;
	default:
		debug("invalid value type `%i'", type);
		return FAIL;
	}

	retval = ratt_table_create(table, cnt, size, 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	}

	return OK;
}

static int decl_use_default_value(ratt_conf_t *decl)
{
	const char **defval = NULL;
	int retval, num;

	if (decl->flags & RATTCONFFLLST) {
		retval = list_create(decl->value, 0, decl->type);
		if (retval != OK) {
			debug("list_create() failed");
			return FAIL;
		}
	}

	for (defval = decl->defval; defval && (*defval != '\0'); defval++) {
		switch (decl->type) {
		case RATTCONFDTNUM8:
		case RATTCONFDTNUM16:
		case RATTCONFDTNUM32:
			/* convert string to integer */
			num = atoi(*defval);
			retval = set_value(decl->value,
			    &num, decl->type, decl->flags);
			break;
		default:
			retval = set_value(decl->value,
			    *defval, decl->type, decl->flags);
			break;
		}

		if (retval != OK) {
			debug("set_value() failed");
			return FAIL;
		}
	}

	return OK;
}

static int decl_use_config_list(ratt_conf_t *decl, config_setting_t *sett)
{
	config_setting_t *elem = NULL;
	const char *str = NULL;
	size_t len = 0;
	int num = 0, i = 0;
	int retval;

	len = config_setting_length(sett);
	if (len <= 0) {
		debug("config_setting_length() invalid length");
		return FAIL;
	}

	retval = list_create(decl->value, len, decl->type);
	if (retval != OK) {
		debug("list_create() failed");
		return FAIL;
	}

	while ((elem = config_setting_get_elem(sett, i++)) != NULL) {
		switch (decl->type) {
		case RATTCONFDTSTR:
			if (config_setting_type(elem) != CONFIG_TYPE_STRING) {
				error("`%s' type mismatch; should be string",
				    config_setting_name(sett));
				return FAIL;
			}

			str = config_setting_get_string(elem);
			if (!str) {
				debug("config_setting_get_string() failed");
				return FAIL;
			}

			retval = set_value(decl->value,
			    str, decl->type, decl->flags);
			break;
		case RATTCONFDTNUM8:
		case RATTCONFDTNUM16:
		case RATTCONFDTNUM32:
			if (config_setting_type(elem) != CONFIG_TYPE_INT) {
				error("`%s' type mismatch; should be numeric",
				    config_setting_name(sett));
				return FAIL;
			}

			num = config_setting_get_int(elem);
			retval = set_value(decl->value,
			    &num, decl->type, decl->flags);
			break;
		default:
			debug("invalid value type `%i'", decl->type);
			return FAIL;
		}
	}

	if (--i < len) {
		debug("config_setting_get_elem() failed prematurely");
		debug("reporting %u elem, only got %u", len, i);
		return FAIL;
	}

	return OK;
}

static int decl_use_config_value(ratt_conf_t *decl, config_setting_t *sett)
{
	char const *str = NULL;
	int num, retval;

	if (decl->flags & RATTCONFFLLST) {
		if (config_setting_is_array(sett)) {
			retval = decl_use_config_list(decl, sett);
			if (retval != OK) {
				debug("decl_use_config_list() failed");
				return FAIL;
			}

			return OK;
		}
		
		/*
		 * declaration requires a list; but setting is not an array.
		 * we allocate a table of one element to satisfy both.
		 */
		retval = list_create(decl->value, 1, decl->type);
		if (retval != OK) {
			debug("list_create() failed");
			return FAIL;
		}
	}

	switch (decl->type) {
	case RATTCONFDTSTR:
		if (config_setting_type(sett) != CONFIG_TYPE_STRING) {
			error("`%s' type mismatch; should be string",
			    config_setting_name(sett));
			return FAIL;
		}
		str = config_setting_get_string(sett);
		if (!str) {
			debug("config_setting_get_string() failed");
			return FAIL;
		}

		retval = set_value(decl->value, str, decl->type, decl->flags);
		if (retval != OK) {
			debug("set_value() failed");
			return FAIL;
		}
		break;
	case RATTCONFDTNUM8:
	case RATTCONFDTNUM16:
	case RATTCONFDTNUM32:
		if (config_setting_type(sett) != CONFIG_TYPE_INT) {
			error("`%s' type mismatch; should be numeric",
			    config_setting_name(sett));
			return FAIL;
		}
		num = config_setting_get_int(sett);
		retval = set_value(decl->value, &num, decl->type, decl->flags);
		if (retval != OK) {
			debug("set_value() failed");
			return FAIL;
		}
		break;
	default:
		debug("invalid value type `%i'", decl->type);
		return FAIL;

	}
	
	return OK;
}

static inline void decl_release(ratt_conf_t *decl)
{
	if (decl && (decl->flags & RATTCONFFLLST)) {
		debug("releasing list for %s", decl->path);
		list_destroy(decl->value, decl->type);
	} else if (decl) {
		debug("releasing value for %s", decl->path);
		unset_value(decl->value, decl->type);
	} else
		debug("decl is NULL");
}

void conf_close(void)
{
	RATTLOG_TRACE();
	config_destroy(&l_cfg);
}

int conf_open(const char *file)
{
	RATTLOG_TRACE();
	config_error_t err = CONFIG_ERR_NONE;

	if (config_read_file(&l_cfg, file) != CONFIG_TRUE) {
		debug("config_read_file() failed");
		err = config_error_type(&l_cfg);
	}

	switch (err) {
	case CONFIG_ERR_FILE_IO:
		error("%s: %s", file, strerror(errno));
		return FAIL;
	case CONFIG_ERR_PARSE:
		error("%s: %s at line %i", file, config_error_text(&l_cfg),
		    config_error_line(&l_cfg));
		return FAIL;
	default:
	case CONFIG_ERR_NONE:
		/* success */
		break;
	}

	return OK;
}

int conf_open_builtin(const char *cfg)
{
	RATTLOG_TRACE();
	config_error_t err = CONFIG_ERR_NONE;

	if (config_read_string(&l_cfg, cfg) != CONFIG_TRUE) {
		debug("config_read_string() failed");
		err = config_error_type(&l_cfg);
	}

	switch (err) {
	case CONFIG_ERR_FILE_IO:
		debug("IO error on a string?");
		return FAIL;
	case CONFIG_ERR_PARSE:
		error("%s at line %i", config_error_text(&l_cfg),
		    config_error_line(&l_cfg));
		return FAIL;
	default:
	case CONFIG_ERR_NONE:
		/* success */
		break;
	}

	return OK;
}

void conf_release_reverse(ratt_conf_t const * const first,
                          ratt_conf_t *current)
{
	RATTLOG_TRACE();
	if (first)
		for (; (current != NULL) && (current >= first); current--)
			decl_release(current);
}

void conf_release(ratt_conf_t *decl)
{
	RATTLOG_TRACE();
	for (; (decl != NULL) && (decl->path != NULL); decl++)
		decl_release(decl);
}

int conf_parse(char const *parent, ratt_conf_t *decl)
{
	RATTLOG_TRACE();
	ratt_conf_t const * const first = decl;
	config_setting_t *sett = NULL;
	char settpath[CONF_SETTPATHMAX] = { '\0' };
	char *declpath = NULL;
	size_t declmaxsiz = CONF_SETTPATHMAX - 1; /* trailing NULL byte */
	size_t parlen = 0;
	int retval;

	if (parent) {
		parlen = strlen(parent);
		if (parlen >= CONF_SETTPATHMAX) {
			debug("parent name is too long `%s'", parent);
			return FAIL;
		} else {
			strncpy(settpath, parent, CONF_SETTPATHMAX);
			declpath = settpath + parlen;
			*declpath = '/';
			declmaxsiz -= parlen;
			declpath++;
		}
	}

	for (; decl && decl->path; decl++)
	{
		if (parent) {
			if (strlen(decl->path) > declmaxsiz) {
				debug("no space left for `%s'", decl->path);
				break;
			}
			strncpy(declpath, decl->path, declmaxsiz);
			sett = config_lookup(&l_cfg, settpath);
		} else
			sett = config_lookup(&l_cfg, decl->path);

		if (!sett && (decl->flags & RATTCONFFLREQ)) {
			error("`%s' declaration is mandatory", decl->path);
			break;
		} else if (!sett) {	/* set to default value */
			retval = decl_use_default_value(decl);
			if (retval != OK) {
				debug("decl_use_default_value() failed");
				break;
			}
			continue;
		} else { /* set to config value */
			retval = decl_use_config_value(decl, sett);
			if (retval != OK) {
				debug("decl_use_config_value() failed");
				error("parser failed at line %i",
				    config_setting_source_line(sett));
				break;
			}
			continue;
		}
	}

	if (retval != OK) {
		conf_release_reverse(first, decl);
		return FAIL;
	}

	return OK;
}

void conf_fini(void *udata)
{
	args_unregister(CONF_ARGSSEC_ID, NULL);
	conf_close();
}

int conf_init(void)
{
	RATTLOG_TRACE();
	int retval;

	retval = args_register(CONF_ARGSSEC_ID, NULL, l_args);
	if (retval != OK) {
		debug("args_register() failed");
		return FAIL;
	}

	config_init(&l_cfg);

	retval = conf_open(l_args_conf_file);
	if (retval != OK) {
		debug("conf_open() failed");
		args_unregister(CONF_ARGSSEC_ID, NULL);
		config_destroy(&l_cfg);
		return FAIL;
	}

	return OK;
}
