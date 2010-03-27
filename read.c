/*
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#include "common.h"

static int common(struct req *req, bool leave)
{
	ut *ut;

	if (!ut_ok(req->query)) {
		req->reply = req->query;
		req->query = NULL;
		return false;
	} else if (ut_type(req->query) != T_HASH) {
		return errcode(JSON_RPC_INVALID_REQUEST);
	}

	/* JSON-RPC argument check */
	if ((ut = uth_get(req->query, "method")))
		req->method = ut_char(ut);

	if ((ut = uth_get(req->query, "id")))
		req->id = ut_char(ut);

	/* XXX: dont check jsonrpc=2.0 */

	if (!leave) {
		if ((ut = uth_get(req->query, "params")))
			req->query = ut;
		else
			req->query = uth_set_thash(req->query, "params", NULL); /* create empty hash */
	}

	return true;
}

int readjson(struct req *req)
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

	return common(req, false);
}

int read822(struct req *req)
{
	char buf[BUFSIZ];
	xstr *input = xstr_create("", req->mm);

	while (fgets(buf, sizeof(buf), stdin)) {
		if (!buf[0] || buf[0] == '\n') break;
		xstr_append(input, buf);
	}

	/* eof? */
	if (xstr_length(input) == 0) exit(0);

	dbg(8, "parsing %s\n", xstr_string(input));

	req->query = ut_new_thash(
		rfc822_parse(xstr_string(input), req->mm),
		req->mm);

	return common(req, true);
}

int readhttp(struct req *req)
{
	enum http_type ht;
	char first[256], buf[BUFSIZ], *ct, *cl, *ac, *uri, *auth;
	int len;
	xstr *xs = xstr_create("", req->mm);
	thash *h;
	json *js;

	/* read query */
	if (!fgets(first, sizeof(first), stdin) || first[0] == '\n')
		exit(0); /* eof */

	if (strncmp(first, "POST ", 5) == 0) {
		ht = POST;
		uri = first + 5;
	} else if (strncmp(first, "OPTIONS ", 8) == 0) {
		ht = OPTIONS;
		uri = first + 8;
	} else if (strncmp(first, "GET ", 4) == 0) {
		ht = GET;
		uri = first + 4;
	} else {
		dbg(4, "invalid method: %s\n", first);
		return errmsg("Invalid HTTP method");
	}

	/* read headers */
	while (fgets(buf, sizeof(buf), stdin)) {
		if (!buf[0] || buf[0] == '\n' || buf[0] == '\r') break;
		xstr_append(xs, buf);
	}

	h = rfc822_parse(xstr_string(xs), req->mm);

	/* fetch authentication information ASAP */
	auth = thash_get(h, "Authorization");
	if (auth && strncmp(auth, "Basic ", 6) == 0) {
		xstr *ad = asn_b64dec(auth+6, req->mm);
		char *pass = strchr(xstr_string(ad), ':');

		if (pass) {
			*pass++ = '\0';
			req->claim_user = xstr_string(ad);
			req->claim_pass = pass;
		}
	}

	if (ht == OPTIONS) {
		return errcode(JSON_RPC_HTTP_OPTIONS);
	}

	/* read URI */
	if (ht == GET) {
		char *space = strchr(uri, ' ');
		if (space) *space = '\0';

		if (streq(uri, "/")) {
			uri = "/index.html";
		} else if (!asn_match(":^/[a-zA-Z0-9/]+(|\\.[a-zA-Z0-9]+)$:", uri)) {
			dbg(4, "invalid uri: '%s'\n", uri);
			errcode(JSON_RPC_HTTP_NOT_FOUND);
			return 2;
		}

		req->uripath = mmatic_printf(req->mm, "%s%s", CFG("htdocs"), uri);
		if (asn_isfile(req->uripath) > 0) {
			dbg(4, "GET '%s'\n", req->uripath);
			errcode(JSON_RPC_HTTP_GET);
		} else {
			req->uripath = 0;
			dbg(4, "not found: '%s'\n", uri);
			errcode(JSON_RPC_HTTP_NOT_FOUND);
		}

		return 2;
	}

	/* = POST - ie. normal RPC call = */

	ct = thash_get(h, "Content-Type");
	if (!ct) return errmsg("Content-Type needed");
	if (strncmp(ct, "application/json", 16) != 0)
		return errmsg("Unsupported Content-Type");

	ac = thash_get(h, "Accept");
	if (!ac) return errmsg("Accept needed");
	if (!asn_match(":application/json:", ac))
		return errmsg("Unsupported Accept");

	/* read the query */
	cl = thash_get(h, "Content-Length");
	if (!cl) return errmsg("Content-Length needed");

	len = atoi(cl);
	if (len < 0 || len > BUFSIZ - 1)
		return errmsg("Unsupported Content-Length");

	if (fread(buf, 1, len, stdin) != len)
		return errmsg("Invalid Content-Length");
	else
		buf[len+1] = '\0';

	/* parse the query */
	js = json_create(req->mm);
	req->query = json_parse(js, buf);

	return common(req, false);
}
