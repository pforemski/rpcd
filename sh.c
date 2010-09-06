/*
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pforemski@asn.pl>
 * All rights reserved
 */

#include "common.h"

bool sh_init(struct mod *mod)
{
	dbg(1, "%s: initialized\n", mod->path);
	return true;
}

bool sh_deinit(struct mod *mod)
{
	dbg(1, "%s: deinitialized\n", mod->path);
	return true;
}

// FIXME
bool sh_handle(struct req *req, mmatic *mm)
{
	thash *env = NULL, *qh = NULL;
	tlist *args = NULL, *qp = NULL;
	ut *v;
	char *k;

	switch (ut_type(req->params)) {
		case T_LIST: // FIXME: security?
			qp = ut_tlist(req->params);
			args = MMTLIST_CREATE(NULL);

			TLIST_ITER_LOOP(qp, v)
				tlist_push(args, ut_char(v));
			break;

		case T_HASH:
			qh = ut_thash(req->params);
			env = MMTHASH_CREATE_STR(NULL);

			THASH_ITER_LOOP(qh, k, v)
				thash_set(env, asn_replace("/[^a-zA-Z0-9_]/", "_", k, mm), ut_char(v));
			break;

		default:
			return errcode(JSON_RPC_INVALID_INPUT);
	}

	/* run the handler */
	int rc;
	xstr *out = MMXSTR_CREATE("");
	xstr *err = MMXSTR_CREATE("");

	rc = asn_cmd2(req->mod->path, /* TODO:args */ NULL, env, NULL, out, err);

	if (rc != 0) {
		return err(rc, xstr_string(out), xstr_string(err));
	} else {
		req->reply = ut_new_thash(rfc822_parse(xstr_string(out), req->mm), req->mm);
		return true;
	}
}

struct api sh_api = {
	.magic  = RPCD_MAGIC,
	.init   = sh_init,
	.deinit = sh_deinit,
	.handle = sh_handle,
};
