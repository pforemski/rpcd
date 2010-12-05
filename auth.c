/*
 * Copyright (C) 2009-2010 Pawel Foremski <pawel@foremski.pl>
 * Licensed under GPLv3
 */

#include "common.h"

/** Cache of internal users: username -> struct user * */
static thash *authdb = NULL;

static void authdb_init(struct req *req)
{
	struct mod *mod = req->mod;
	char *str;

	str = asn_readfile(O.http.htpasswd, req);
	if (str) {
		authdb = rfc822_parse(str, mod);

		if (authdb) {
			dbg(3, "db initialized\n");
			thash_dump(5, authdb);
			return;
		}
	}

	authdb = thash_create_strkey(NULL, req);
}

/** Authenticate user info in req->http and update user/pass in req
 * @retval false authentication failed */
bool auth_http(struct req *req)
{
	const char *pass;

	if (!O.http.htpasswd)
		return false;

	if (!authdb)
		authdb_init(req);

	if (!req->http.user || !req->http.user[0] || !req->http.pass)
		return false;

	pass = thash_get(authdb, req->http.user);
	if (!pass || !streq(req->http.pass, pass))
		return false;

	req->user = mmatic_strdup(req->http.user, req);
	req->pass = mmatic_strdup(req->http.pass, req);
	dbg(3, "user %s authenticated\n", req->user);

	return true;
}
