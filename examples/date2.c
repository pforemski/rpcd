#include <time.h>
#include "../rpcd.h"

static bool init(const char *name)
{
	dbg(1, "module date2 initialized\n");
	return true;
}

static bool check(struct req *req, mmatic *mm)
{
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

	req->rep = ut_new_thash(NULL, mm);
	uth_add_char(req->rep, "date", mmstrdup(buf));

	return true;
}

struct api date2_module = {
	.init   = init,
	.deinit = NULL,
	.check  = check,
	.handle = handle
};
