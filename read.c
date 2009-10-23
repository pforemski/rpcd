/*
 * Copyright (C) 2009 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#include "rpcd.h"

static bool common(struct req *req)
{
	thash *args;
	ut *ut;

	if (!ut_ok(req->query)) {
		req->rep = req->query;
		req->query = NULL;
		return false;
	}

	/* JSON-RPC argument check */
	args = ut_thash(req->query);

	if ((ut = thash_get(args, "method")))
		req->method = ut_char(ut);

	if ((ut = thash_get(args, "id")))
		req->id = ut_char(ut);

	/* XXX: dont check jsonrpc=2.0 */

	return true;
}

bool readjson(struct req *req)
{
	char buf[BUFSIZ];
	xstr *xs = xstr_create("", req->mm);
	json *js;

	while (fgets(buf, sizeof(buf), stdin)) {
		if (!buf[0] || buf[0] == '\n') break;
		xstr_append(xs, buf);
	}

	/* eof? */
	if (xstr_length(xs) == 0) exit(0);

	js = json_create(req->mm);
	req->query = json_parse(js, xstr_string(xs));

	return common(req);
}

bool read822(struct req *req)
{
	char buf[BUFSIZ];
	xstr *input = xstr_create("", req->mm);

	while (fgets(buf, sizeof(buf), stdin)) {
		if (!buf[0] || buf[0] == '\n') break;
		xstr_append(input, buf);
	}

	/* eof? */
	if (xstr_length(input) == 0) exit(0);

	req->query = ut_new_thash(
		rfc822_parse(xstr_string(input), req->mm),
		req->mm);

	return common(req);
}

bool readhttp(struct req *req)
{
	char buf[BUFSIZ], *ct, *cl, *ac;
	int len;
	xstr *xs = xstr_create("", req->mm);
	thash *h;
	json *js;

	/* read POST /uri HTTP/1.0 */
	if (!fgets(buf, sizeof(buf), stdin))
		exit(0); /* eof */

	if (strncmp(buf, "POST /", 6) != 0)
		return errmsg("Only POST supported");

	/* read headers */
	while (fgets(buf, sizeof(buf), stdin)) {
		if (!buf[0] || buf[0] == '\n' || buf[0] == '\r') break;
		xstr_append(xs, buf);
	}

	if (xstr_length(xs) == 0)
		return errmsg("No HTTP headers");

	h = rfc822_parse(xstr_string(xs), req->mm);

	ct = thash_get(h, "Content-Type");
	if (!ct) return errmsg("Content-Type needed");
	if (!(streq(ct, "application/json-rpc") ||
	      streq(ct, "application/json")     ||
	      streq(ct, "application/jsonrequest")))
		return errmsg("Unsupported Content-Type");

	ac = thash_get(h, "Accept");
	if (!ac) return errmsg("Accept needed");
	if (!(streq(ac, "application/json-rpc") ||
	      streq(ac, "application/json")     ||
	      streq(ac, "application/jsonrequest")))
		return errmsg("Unsupported Accept");

	cl = thash_get(h, "Content-Length");
	if (!cl) return errmsg("Content-Length needed");

	len = atoi(cl);
	if (len < 0 || len > BUFSIZ - 1)
		return errmsg("Unsupported Content-Length");

	if (fread(buf, 1, len, stdin) != len)
		return errmsg("Invalid Content-Length");

	buf[len+1] = '\0';
	js = json_create(req->mm);
	req->query = json_parse(js, buf);

	return common(req);
}
