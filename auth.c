/*
 * Copyright (C) 2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#include "common.h"

/** Cache of internal users: username -> struct user * */
static thash *internaldb = NULL;

static void internaldb_init(void)
{
	struct user *user;
	tlist *groups; struct fcel *groupel;
	tlist *users; struct fcel *userel;

	internaldb = MMTHASH_CREATE_STR(NULL);

	users = asn_fcparselist(CFG("users/list.order"), mmtmp);
	TLIST_ITER_LOOP (users, userel) {
		if (userel->enabled == false)
			continue;

		user = mmzalloc(sizeof(*user));
		user->name = mmstrdup(userel->elname);
		user->pass = CFG((const char *) pbt("users/%d/password", userel->elid));
		user->groups = MMTLIST_CREATE(NULL);

		groups = asn_fcparselist(CFG(pbt("users/%d/groups/list.order", userel->elid)), mmtmp);
		TLIST_ITER_LOOP (groups, groupel) {
			tlist_push(user->groups, mmstrdup(groupel->elname));
		}

		thash_set(internaldb, user->name, user);
	}

	dbg(3, "db initialized\n");
}

/** Authenticate user info in req->claim_* and return matching user on success
 * @retval NULL   authentication failed */
struct user *authinternal(struct req *req)
{
	struct user *user;

	if (!internaldb)
		internaldb_init();

	if (!req->claim_user || !req->claim_user[0] || !req->claim_pass)
		return NULL;

	dbg(3, "user '%s', pass '%s'\n", req->claim_user, req->claim_pass);

	user = thash_get(internaldb, req->claim_user);
	if (!user || !streq(req->claim_pass, user->pass))
		return NULL;

	dbg(5, "user %s authenticated\n", user->name);

	return user;
}

/*************************************************************/

struct user *authsystem(struct req *req)
{
	dbg (0, "TODO :)\n");
	return NULL;
}
