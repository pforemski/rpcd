/*
 * Copyright (C) 2009 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#include "rpcd.h"

enum readstatus readcli(struct req *req)
{
	char buf[BUFSIZ];
	xstr *input = xstr_create("", req->mm);
	thash *qh;

	while (fgets(buf, sizeof(buf), stdin)) {
		if (!buf[0] || buf[0] == '\n') break;
		xstr_append(input, buf);
	}

	/* eof? */
	if (xstr_length(input) == 0)
		return EXIT;

	req->query = rfc822_parse(xstr_string(input), req->mm);
	qh = ut_thash(req->query);

	// TODO: check "jsonrpc"?
	if (thash_get(qh, "method"))
		req->method = ut_char(thash_get(qh, "method"));
	else if (thash_get(qh, "id"))
		req->id = ut_char(thash_get(qh, "id"));

	return OK;
}
