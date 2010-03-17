/*
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#include "common.h"

bool sh_init(struct mod *mod)
{
	dbg(1, "%s: initialized\n", mod->name);
	return true;
}

bool sh_check(struct req *req, mmatic *mm)
{
	return true; // TODO?
}

// FIXME
bool sh_handle(struct req *req, mmatic *mm)
{
	thash *env = NULL, *qh = NULL;
	tlist *args = NULL, *qp = NULL;
	ut *v;
	char *k, out[BUFSIZ] = {0}, err[BUFSIZ] = {0};
	int rc;

	switch (ut_type(req->query)) {
		case T_LIST: // FIXME: security?
			qp = ut_tlist(req->query);
			args = MMTLIST_CREATE(NULL);

			TLIST_ITER_LOOP(qp, v)
				tlist_push(args, ut_char(v));
			break;

		case T_HASH:
			qh = ut_thash(req->query);
			env = MMTHASH_CREATE_STR(NULL);

			THASH_ITER_LOOP(qh, k, v)
				thash_set(env, asn_replace("/[^a-zA-Z0-9_]/", "_", k, mm), ut_char(v));
			break;

		default:
			return errcode(JSON_RPC_INVALID_INPUT);
	}

	/* run the handler */
	rc = asn_cmd(req->mod->path, /* TODO:args */ NULL, env, NULL, 0, out, BUFSIZ, err, BUFSIZ);

	if (rc != 0) {
		return err(rc, out, err);
	}
	else {
		req->rep = ut_new_thash(rfc822_parse(out, req->mm), req->mm);
		return true;
	}
}

struct api sh_api = {
	.magic  = RPCD_MAGIC,
	.init   = sh_init,
	.deinit = NULL,
	.check  = sh_check,
	.handle = sh_handle,
};
