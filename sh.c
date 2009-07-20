/*
 * Copyright (C) 2009 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#include "rpcd.h"

bool sh_check(const struct req *req, struct rep *rep)
{
	return true; // TODO?
}

bool sh_handle(const struct req *req, struct rep *rep)
{
	char *k, *v, *params, out[BUFSIZ] = {0}, err[BUFSIZ] = {0};
	int rc;
	mmatic *mm = req->mm;
	thash *env = MMTHASH_CREATE_STR(NULL);
	thash *outhash = MMTHASH_CREATE_STR(NULL);

	THASH_ITER_LOOP(req->args, k, v)
		thash_set(env, asn_replace("/[^a-zA-Z0-9_]/", "_", k, mm), v);

	// FIXME: security?
	params = tlist_stringify(req->params, " ", mm);

	/* run the handler */
	rc = asn_cmd(req->mod->path, params, env, NULL, 0, out, BUFSIZ, err, BUFSIZ);

	if (rc != 0) {
		dbg(0, "%s: %s\n", req->method, err);

		rep_set_error(rep, rc, out, err);
		return false;
	}

	/* parse out as RFC822 */
	if (!thash_parse_rfc822(outhash, mmstrdup(out))) {
		dbg(0, "%s: output parse error\n", req->method);

		rep_set_error(rep, JSON_RPC_OUT_PARSE_ERROR, NULL, NULL);
		return false;
	}

	/* success */
	rep_set_thash(rep, outhash);
	return true;
}
