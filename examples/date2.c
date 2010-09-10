#include <time.h>
#include "../rpcd_module.h"

static bool init(struct mod *mod)
{
	dbg(1, "module date2 initialized\n");
	return true;
}

static bool handle(struct req *req)
{
	time_t t;
	struct tm *tmp;
	char buf[200];

	t = time(NULL);
	tmp = localtime(&t);
	strftime(buf, sizeof(buf), "%F %T", tmp);

	uth_set_char(req->reply, "date", mmatic_strdup(buf, req));
	uth_set_char(req->reply, "config variable", uth_char(req->mod->cfg, "variable"));

	return true;
}

struct api date2_api = {
	.tag  = RPCD_TAG,
	.init   = init,
	.handle = handle
};
