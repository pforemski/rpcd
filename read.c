/*
 * Copyright (C) 2009 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#include "rpcd.h"

enum readstatus readcli(struct req *req, mmatic *mm)
{
	char buf[BUFSIZ];
	xstr *input = MMXSTR_CREATE("");

	while (fgets(buf, sizeof(buf), stdin)) {
		if (!buf[0] || buf[0] == '\n') break;
		xstr_append(input, buf);
	}

	/* eof? */
	if (xstr_length(input) == 0)
		return EXIT;

	if (!thash_parse_rfc822(req->args, xstr_string(input)))
		return SKIP; /* = parse error */

	// TODO: check "jsonrpc"?
	if (thash_get(req->args, "method"))
		req->method = thash_get(req->args, "method");
	else if (thash_get(req->args, "id"))
		req->id = thash_get(req->args, "id");

	return OK;
}
