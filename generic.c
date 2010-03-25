/*
 * generic handling procedures, off-loading module programming
 *
 * Copyright (C) 2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 *
 * All rights reserved
 */

#include "common.h"

bool generic_init(struct mod *mod)
{
	dbg(1, "module %s initialized\n", mod->name);
	return true;
}

bool generic_deinit(struct mod *mod)
{
	return true;
}

bool generic_check(struct req *req, mmatic *mm)
{
	return true;
}

bool generic_handle(struct req *req, mmatic *mm)
{
	return true;
}

bool fwcheck(struct req *req)
{
	if (!req->fw)
		return true;

	/* TODO: check req->fw */

	return true;
}

/* for Vim autocompletion:
 * vim: path=.,/usr/include,/usr/local/include,~/local/include
 */
