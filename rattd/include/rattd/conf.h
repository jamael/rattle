#ifndef RATTD_CONF_H
#define RATTD_CONF_H

#include <stddef.h>
#include <stdint.h>

enum RATTCONFDT {	/* data type */
	RATTCONFDTSTR,	/* string */
	RATTCONFDTNUM8,	/* int8 */
	RATTCONFDTNUM16,/* int16 */
	RATTCONFDTNUM32,/* int32 */
};

#define RATTCONFFLLST	0x1	/* Allow multiple value */
#define RATTCONFFLREQ	0x2	/* Config declaration is mandatory */
#define RATTCONFFLUNS	0x4	/* Value is unsigned */

#define RATTCONFLSTSIZ	10	/* Fixed-size for defval list table */

typedef struct conf_decl conf_decl_t;
struct conf_decl {
	char const *path;		/* absolute path of declaration */
	char const *desc;		/* description */

	union {
		char const *str;	/* string */
		uint32_t const num;	/* numeric */
		union {
			/* string */
			char const *str[RATTCONFLSTSIZ];
			/* numeric */
			uint32_t const num[RATTCONFLSTSIZ];
		} lst;	/* fixed length array */
	} defval;	/* default value */
	size_t defval_lstcnt;	/* count of elements in list */

	union {
		char **str;		/* string pointer */
		long long *num;		/* numeric pointer */
		union {
			/* string */
			char ***str;
			/* 8bit numeric */
			int8_t **num8;
			/* 8bit unsigned numeric */
			uint8_t **num8u;
			/* 16bit numeric */
			int16_t **num16;
			/* 16bit unsigned numeric */
			uint16_t **num16u;
			/* 32bit numeric */
			int32_t **num32;
			/* 32bit unsigned numeric */
			uint32_t **num32u;
		} lst;	/* variable length array */
	} val;		/* config value */
	size_t val_lstcnt;	/* count of elements in list */

	enum RATTCONFDT datatype;	/* type of data */
	unsigned int flags;		/* optional flags */
};

#endif /* RATTD_CONF_H */
