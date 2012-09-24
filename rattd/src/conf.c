/*
 * RATTD configuration handler
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


#include <libconfig.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rattd/def.h>
#include <rattd/conf.h>
#include <rattd/log.h>

static struct config_t l_cfg;

static inline int decl_strdup(conf_decl_t *decl, const char *str)
{
	*(decl->val.str) = strdup(str);
	if (*(decl->val.str))
		return OK;
	error("memory allocation failed");
	return FAIL;
}

static inline int decl_strdup_elem(conf_decl_t *decl, const char *str, int i)
{
	(*(decl->val.lst.str))[i] = strdup(str);
	if ((*(decl->val.lst.str))[i])
		return OK;
	error("memory allocation failed");
	return FAIL;
}

static inline int decl_check_num(conf_decl_t *decl, long long num)
{
	switch(decl->datatype) {
	case RATTCONFDTNUM8:
		if ((decl->flags & RATTCONFFLUNS)
		    && ((num > UCHAR_MAX) || (num < 0))) {
			error("`%lli' out of bound (%u to %u).",
		 	    num, 0, UCHAR_MAX);
			return FAIL;
		} else if (!(decl->flags & RATTCONFFLUNS)
		    && ((num > SCHAR_MAX) || (num < SCHAR_MIN))) {
			error("`%lli' out of bound (%i to %i).",
		 	    num, SCHAR_MIN, SCHAR_MAX);
			return FAIL;
		}
		break;
	case RATTCONFDTNUM16:
		if ((decl->flags & RATTCONFFLUNS)
		    && ((num > USHRT_MAX) || (num < 0))) {
			error("`%lli' out of bound (%u to %u).",
		 	    num, 0, USHRT_MAX);
			return FAIL;
		} else if (!(decl->flags & RATTCONFFLUNS)
		    && ((num > SHRT_MAX) || (num < SHRT_MIN))) {
			error("`%lli' out of bound (%i to %i).",
		 	    num, SHRT_MIN, SHRT_MAX);
			return FAIL;
		}
		break;
	case RATTCONFDTNUM32:
		if ((decl->flags & RATTCONFFLUNS)
		    && ((num > UINT_MAX) || (num < 0))) {
			error("`%lli' out of bound (%u to %u).",
		 	    num, 0, UINT_MAX);
			return FAIL;
		} else if (!(decl->flags & RATTCONFFLUNS)
		    && ((num > INT_MAX) || (num < INT_MIN))) {
			error("`%lli' out of bound (%i to %i).",
		 	    num, INT_MIN, INT_MAX);
			return FAIL;
		}
		break;
	case RATTCONFDTSTR:
		/* empty */
		break;
	default:
		debug("unknown datatype value of %i", decl->datatype);
		return FAIL;
	}
	return OK;
}

static void decl_release_list(conf_decl_t *decl)
{
	int i;

	for (i = 0; i < decl->lstcnt; ++i) {
		switch (decl->datatype) {
		case RATTCONFDTSTR:
			debug("releasing index %i for `%s'", i, decl->path);
			if ((*(decl->val.lst.str))[i])
				free((*(decl->val.lst.str))[i]);
			break;
		case RATTCONFDTNUM8:
		case RATTCONFDTNUM16:
		case RATTCONFDTNUM32:
			/* empty */
			break;
		default:
			debug("unknown datatype value of %i", decl->datatype);
			break;
		}
	}
	debug("releasing memory chunck for `%s'", decl->path);
	free(*(decl->val.lst.str));
}

static int decl_alloc_list(conf_decl_t *decl, size_t len)
{
	size_t size = 0;
	debug("allocating list of %i element(s) for `%s'", len, decl->path);

	switch (decl->datatype) {
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
		debug("unknown datatype value of %i", decl->datatype);
		return FAIL;
	}

	*(decl->val.lst.str) = calloc(len, size);
	if (!(*(decl->val.lst.str))) {
		error("memory allocation failed");
		return FAIL;
	}

	decl->lstcnt = len;
	return OK;
}

static int decl_defval_list(conf_decl_t *decl)
{
	return OK;
}

static int decl_parse_list(conf_decl_t *decl, config_setting_t *sett)
{
	config_setting_t *elem = NULL;
	size_t len = 0;
	const char *str = NULL;
	long long num = 0;
	int i, retval;

	len = config_setting_length(sett);
	if (len <= 0) {
		debug("config_setting_length() invalid length");
		return FAIL;
	}

	/* first alloc the table */
	decl_alloc_list(decl, len);

	for (i = 0; i < len; ++i) {

		elem = config_setting_get_elem(sett, i);
		if (!elem) {
			debug("config_setting_get_elem() failed");
			return FAIL;
		}

		switch (decl->datatype) {
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
			} else {
				retval = decl_strdup_elem(decl, str, i);
				if (retval != OK) {
					debug("decl_strdup_elem() failed");
					return FAIL;
				}
			}
			break;
		case RATTCONFDTNUM8:
			if (config_setting_type(elem) != CONFIG_TYPE_INT) {
				error("`%s' type mismatch; should be numeric",
				    config_setting_name(sett));
				return FAIL;
			}
			num = config_setting_get_int(elem);
			retval = decl_check_num(decl, num);
			if (retval != OK) {
				debug("decl_check_num() failed");
				return FAIL;
			}
			if (decl->flags & RATTCONFFLUNS)
				(*(decl->val.lst.num8u))[i] = (uint8_t) num;
			else
				(*(decl->val.lst.num8))[i] = (int8_t) num;
			break;
		case RATTCONFDTNUM16:
			if (config_setting_type(elem) != CONFIG_TYPE_INT) {
				error("`%s' type mismatch; should be numeric",
				    config_setting_name(sett));
				return FAIL;
			}
			num = config_setting_get_int(elem);
			retval = decl_check_num(decl, num);
			if (retval != OK) {
				debug("decl_check_num() failed");
				return FAIL;
			}
			if (decl->flags & RATTCONFFLUNS)
				(*(decl->val.lst.num16u))[i] = (uint16_t) num;
			else
				(*(decl->val.lst.num16))[i] = (int16_t) num;
			break;
		case RATTCONFDTNUM32:
			if (config_setting_type(elem) != CONFIG_TYPE_INT) {
				error("`%s' type mismatch; should be numeric",
				    config_setting_name(sett));
				return FAIL;
			}
			num = config_setting_get_int(elem);
			retval = decl_check_num(decl, num);
			if (retval != OK) {
				debug("decl_check_num() failed");
				return FAIL;
			}
			if (decl->flags & RATTCONFFLUNS)
				(*(decl->val.lst.num32u))[i] = (uint32_t) num;
			else
				(*(decl->val.lst.num32))[i] = (int32_t) num;
			break;
		default:
			debug("unknown datatype value of %i", decl->datatype);
			return FAIL;
		}
	}
	return OK;
}

static int decl_parse(conf_decl_t *decl, config_setting_t *sett)
{
	const char *str = NULL;
	long long num = 0;
	int retval;

	switch (decl->datatype) {
	case RATTCONFDTSTR:
		str = config_setting_get_string(sett);
		if (!str) {
			error("incompatible value for `%s', line %i",
			    config_setting_name(sett),
			    config_setting_source_line(sett));
			return FAIL;
		} else if (decl->flags & RATTCONFFLLST) {
			/* store in a table even if alone */
			if (decl_alloc_list(decl, 1) != OK) {
				debug("decl_alloc_list() failed");
				return FAIL;
			}
			retval = decl_strdup_elem(decl, str, 0);
		} else 
			retval = decl_strdup(decl, str);

		if (retval != OK) {
			debug("decl_strdup_elem() or decl_strdup() failed");
			return FAIL;
		}
		break;
	case RATTCONFDTNUM8:
	case RATTCONFDTNUM16:
	case RATTCONFDTNUM32:
		num = config_setting_get_int(sett);
		retval = decl_check_num(decl, num);
		if (retval != OK) {
			debug("decl_check_num() failed");
			return FAIL;
		}
		if (config_setting_type(sett) != CONFIG_TYPE_INT) {
			error("incompatible value for `%s', line %i",
			    config_setting_name(sett),
			    config_setting_source_line(sett));
			return FAIL;
		} else if (decl->flags & RATTCONFFLLST) {
			/* store in a table even if alone */
			if (decl_alloc_list(decl, 1) != OK) {
				debug("decl_alloc_list() failed");
				return FAIL;
			}
			(*(decl->val.lst.num32u))[0] = (uint32_t) num;
		} else
			*(decl->val.num) = num;
		break;
	default:
		debug("unknown datatype value of %i", decl->datatype);
		return FAIL;

	}
	return OK;
}

int conf_open(const char *file)
{
	debug("entering %s", __func__);
	if (config_read_file(&l_cfg, file) != CONFIG_TRUE) {
		debug("config_read_file() failed");
		error("%s at line %i", config_error_text(&l_cfg),
		    config_error_line(&l_cfg));
		return FAIL;
	}
	return OK;
}

void conf_table_release(conf_decl_t *conftable)
{
	debug("entering %s", __func__);
	conf_decl_t *decl = NULL;
	int i = 0;

	while ((decl = &conftable[i++]) && decl->path != NULL)
	{
		if (decl->flags & RATTCONFFLLST) {
			decl_release_list(decl);
			continue;
		}
	
		switch (decl->datatype) {
		case RATTCONFDTSTR:
			debug("releasing memory for `%s'", decl->path);
			free(*(decl->val.str));
			break;
		case RATTCONFDTNUM8:
		case RATTCONFDTNUM16:
		case RATTCONFDTNUM32:
			/* empty */
			break;
		default:
			debug("unknown datatype value of %i", decl->datatype);
			continue;
		}
	}
}

int conf_table_parse(conf_decl_t *conftable)
{
	debug("entering %s", __func__);
	conf_decl_t *decl = NULL;
	config_setting_t *sett = NULL;
	int retval, i = 0;

	while ((decl = &conftable[i++]) && decl->path != NULL)
	{
		sett = config_lookup(&l_cfg, decl->path);
		if (!sett && (decl->flags & RATTCONFFLREQ)) {
			notice("`%s' declaration is mandatory", decl->path);
			return FAIL;
		} else if (!sett) {
			/* use default, then. */
			if (decl->flags & RATTCONFFLLST) {
				/* uh oh; a list of default */
				decl_defval_list(decl);
				continue;
			}

			switch (decl->datatype) {
			case RATTCONFDTSTR:
				notice("`%s' not found, using `%s'",
				    decl->path, decl->defval.str);
				retval = decl_strdup(decl, decl->defval.str);
				if (retval != OK) {
					debug("decl_strdup() failed");
					return FAIL;
				}
				break;
			case RATTCONFDTNUM8:
			case RATTCONFDTNUM16:
			case RATTCONFDTNUM32:
				notice("`%s' not found, using `%lli'",
				    decl->path, decl->defval.num);
				*(decl->val.num) = decl->defval.num;
				break;
			}
			continue;
		}

		if ((decl->flags & RATTCONFFLLST)
		    && (config_setting_is_array(sett))) {
			/* declaration allows multiple value */
			retval = decl_parse_list(decl, sett);
		} else
			retval = decl_parse(decl, sett);

		if (retval != OK) {
			debug("decl_parse_list() or decl_parse() failed");
			error("configuration parser failed at line %i",
			    config_setting_source_line(sett));
			return FAIL;
		}
	}
	return OK;
}

int conf_fini(void)
{
	debug("entering %s", __func__);
	config_destroy(&l_cfg);
	return OK;
}

int conf_init(void)
{
	debug("entering %s", __func__);
	config_init(&l_cfg);
	return OK;
}
