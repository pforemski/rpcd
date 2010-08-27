/*
 * Copyright (C) 2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pforemski@asn.pl>
 * All rights reserved
 */

#include "common.h"

/** Cache of internal users: username -> struct user * */
static thash *authdb = NULL;

static void authdb_init(void)
{
	char *str;

	str = asn_readfile(R.htpasswd, mmtmp);
	if (str) {
		authdb = rfc822_parse(str, mm);

		if (authdb) {
			dbg(3, "db initialized\n");
			thash_dump(5, authdb);
			return;
		}
	}

	authdb = MMTHASH_CREATE_STR(NULL);
}

/** Authenticate user info in req->claim_* and return matching user on success
 * @retval NULL   authentication failed */
const char *auth(struct req *req)
{
	const char *pass;

	if (!R.htpasswd)
		return NULL;

	if (!authdb)
		authdb_init();

	if (!req->claim_user || !req->claim_user[0] || !req->claim_pass)
		return NULL;

	pass = thash_get(authdb, req->claim_user);
	if (!pass || !streq(req->claim_pass, pass))
		return NULL;

	dbg(3, "user %s authenticated\n", req->claim_user);

	return req->claim_user;
}
