/*
 * RATTLE module for testing
 *
 * A module needs the following :
 *
 *   Let rattd knows about specific options to the module
 *   Register to a specific rattd subsystem
 */


#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>

#define HELLO_MODULE_NAME	"hello"
#define HELLO_MODULE_VERSION	"0.1.1"

static char *msg = NULL;

static ratt_conf_t hello_module_conf[] = {
	{
		.name = "message",
		.desc = "The message to display next to the hello word",
		.defval.str = "world!",
		.val.str = &msg,
		.datatype = RATTCONFDTSTR,
		.flags = RATTCONFFLREQ,
	},

	{ NULL }
};

static ratt_module_t hello_module_info = {
	.name = HELLO_MODULE_NAME,
	.version = HELLO_MODULE_VERSION,
	.conf = &hello_module_conf,
};

void __attribute__ ((destructor)) fini(void)
{
	int retval;

	retval = ratt_module_detach(&hello_module_info);
	debug("detaching module `%s': %s", hello_module_info.name,
	    (retval == OK) ? "ok" : "fail");
}

void __attribute__ ((constructor)) init(void)
{
	int retval;

	retval = ratt_module_attach(&hello_module_info);
	debug("attaching module `%s': %s", hello_module_info.name,
	    (retval == OK) ? "ok" : "fail");
}
