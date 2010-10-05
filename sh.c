/*
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pforemski@asn.pl>
 * All rights reserved
 */

#include <signal.h>
#include "common.h"

static char escbuf[4096];

static const char *escape(const char *what)
{
	int i, j;

	for (i = 0, j = 0; what[i] && j < sizeof escbuf - 2; i++) {
		if (what[i] == '\'' || what[i] == '\\' || what[i] == '"')
			continue;

		escbuf[j++] = what[i];
	}

	escbuf[j] = '\0';
	return escbuf;
}

static bool sh_init(struct mod *mod)
{
	signal(SIGPIPE, SIG_IGN);
	return true;
}

static bool sh_handle(struct req *req)
{
	thash *env = NULL, *qh = NULL;
	tlist *list = NULL;
	ut *v;
	char *k;
	xstr *args = NULL;

	switch (ut_type(req->params)) {
		case T_LIST:
			args = xstr_create("", req);

			list = ut_tlist(req->params);
			TLIST_ITER_LOOP(list, v) {
				xstr_append_char(args, '\'');
				xstr_append(args, escape(ut_char(v)));
				xstr_append_char(args, '\'');
				xstr_append_char(args, ' ');
			}
			break;

		case T_HASH:
			env = thash_create_strkey(NULL, req);

			qh = ut_thash(req->params);
			THASH_ITER_LOOP(qh, k, v) {
				thash_set(env,
					asn_replace("/[^a-zA-Z0-9_]/", "_", k, req), ut_char(v));
			}
			break;

		default:
			return errcode(JSON_RPC_INVALID_INPUT);
	}

	/* run the handler */
	int rc;
	xstr *out = xstr_create("", req);
	xstr *err = xstr_create("", req);

	rc = asn_cmd2(req->mod->path, xstr_string(args), env, NULL, out, err);

	if (rc != 0)
		return err(rc, xstr_string(out), xstr_string(err));

	req->reply = ut_new_thash(rfc822_parse(xstr_string(out), req), req);
	return true;
}

struct api sh_api = {
	.tag    = RPCD_TAG,
	.init   = sh_init,
	.handle = sh_handle,
};
