#include <time.h>
#include "../rpcd.h"

static bool init(struct mod *mod)
{
	dbg(1, "module date2 initialized\n");
	return true;
}

static bool handle(struct req *req, mmatic *mm)
{
	time_t t;
	struct tm *tmp;
	char buf[200];

	t = time(NULL);
	tmp = localtime(&t);
	strftime(buf, sizeof(buf), "%F %T", tmp);

	uth_set_char(req->reply, "date", mmstrdup(buf));

	return true;
}

struct api date2_api = {
	.magic  = RPCD_MAGIC,
	.init   = init,
	.handle = handle
};
