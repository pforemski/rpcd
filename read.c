/*
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pforemski@asn.pl>
 * All rights reserved
 */

#include "common.h"

/** Common part of request parser, usually after JSON representation is made available in req->params
 * @param req     the request
 * @param leave   leave the req->params */
static bool common(struct req *req, bool leave)
{
	ut *ut;

	/* guarantee that req->params is ok */
	if (!ut_ok(req->params)) {
		req->reply = req->params;
		req->params = NULL;
		return false;
	} else if (ut_type(req->params) != T_HASH) {
		return errcode(JSON_RPC_INVALID_REQUEST);
	}

	/* JSON-RPC argument check */
	if ((ut = uth_get(req->params, "service"))) /* used by Qooxdoo */
		req->service = ut_char(ut);

	if ((ut = uth_get(req->params, "method")))
		req->method = ut_char(ut);

	if ((ut = uth_get(req->params, "id")))
		req->id = ut_char(ut);

	/* XXX: dont check jsonrpc=2.0 */

	if (!leave) {
		if ((ut = uth_get(req->params, "params")))
			req->params = ut;
		else
			req->params = uth_set_thash(req->params, "params", NULL); /* create empty hash */

		if (ut_is_tlist(req->params) && thash_get(req->http.headers, "X-Qooxdoo-Response-Type")) {
			tlist *tl = ut_tlist(req->params);
			req->params = tlist_shift(tl);
		}
	}

	return true;
}

static bool readjson_len(struct req *req, int len)
{
	char buf[BUFSIZ];
	xstr *xs = xstr_create("", req);
	json *js;

	if (len < 0) {
		while (fgets(buf, sizeof(buf), stdin)) {
			if (!buf[0] || buf[0] == '\n') break;
			xstr_append(xs, buf);
		}
	} else {
		int r;

		while ((r = fread(buf, 1, MIN(len, sizeof(buf)), stdin))) {
			if (r < 0) {
				dbg(5, "fread() returned %d, len=%d\n", r, len);
				break;
			}

			/* always appends \0 */
			xstr_append_size(xs, buf, r);

			len -= r;
			if (len <= 0)
				break;
		}
	}

	/* eof? */
	if (xstr_length(xs) == 0) exit(0);

	js = json_create(req);
	req->params = json_parse(js, xstr_string(xs));

	return common(req, false);
}

bool readjson(struct req *req)
{
	return readjson_len(req, -1);
}

bool read822(struct req *req)
{
	char buf[BUFSIZ];
	xstr *input = xstr_create("", req);

	while (fgets(buf, sizeof(buf), stdin)) {
		if (!buf[0] || buf[0] == '\n') break;
		xstr_append(input, buf);
	}

	/* eof? */
	if (xstr_length(input) == 0) exit(0);

	dbg(8, "parsing %s\n", xstr_string(input));

	req->params = ut_new_thash(
		rfc822_parse(xstr_string(input), req),
		req);

	return common(req, true);
}

bool readhttp(struct req *req)
{
	enum http_type ht;
	char first[256], buf[BUFSIZ], *ct, *cl, *ac, *uri, *auth;
	int len;
	xstr *xs = xstr_create("", req);

	/* read query */
	if (!fgets(first, sizeof(first), stdin) || first[0] == '\n')
		exit(0); /* eof */

	if (strncmp(first, "POST ", 5) == 0) {
		ht = POST;
		uri = first + 5;
	} else if (strncmp(first, "OPTIONS ", 8) == 0) {
		ht = OPTIONS;
		uri = first + 8;
	} else if (strncmp(first, "GET ", 4) == 0 && O.http.htdocs) { /* @1 */
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

	req->http.headers = rfc822_parse(xstr_string(xs), req);

	/* fetch authentication information ASAP */
	auth = thash_get(req->http.headers, "Authorization");
	if (auth && strncmp(auth, "Basic ", 6) == 0) {
		xstr *ad = asn_b64_dec(auth+6, req);
		char *pass = strchr(xstr_string(ad), ':');

		if (pass) {
			*pass++ = '\0';
			req->http.user = xstr_string(ad);
			req->http.pass = pass;
		}
	}

	const char *cc = thash_get(req->http.headers, "Connection");
	if (cc && (streq(cc, "close") || streq(cc, "Close")))
		req->last = true;

	if (ht == OPTIONS)
		return errcode(JSON_RPC_HTTP_OPTIONS);

	/* handle static query, note that htdocs!=NULL checked @1 */
	if (ht == GET) {
		req->http.needauth = true;

		char *space = strchr(uri, ' ');
		if (space) *space = '\0';

		char *params = strchr(uri, '?');
		if (params) *params = '\0';

		if (streq(uri, "/")) {
			uri = "/index.html";
		} else if (strstr(uri, "..")) {
			dbg(4, "invalid uri: '%s'\n", uri);
			return errcode(JSON_RPC_HTTP_NOT_FOUND);
		}

		req->http.uripath = mmatic_printf(req, "%s%s", O.http.htdocs, uri);
		if (asn_isfile(req->http.uripath) > 0) {
			dbg(4, "GET '%s'\n", req->http.uripath);
			return errcode(JSON_RPC_HTTP_GET);
		}

		dbg(4, "not found: '%s'\n", uri);
		req->http.uripath = NULL;
		return errcode(JSON_RPC_HTTP_NOT_FOUND);
	}

	/* = POST - ie. normal RPC call = */

	ct = thash_get(req->http.headers, "Content-Type");
	if (!ct) return errmsg("Content-Type needed");
	if (strncmp(ct, "application/json", 16) != 0)
		return errmsg("Unsupported Content-Type");

	ac = thash_get(req->http.headers, "Accept");
	if (!ac) return errmsg("Accept needed");
	if (!strstr(ac, "application/json") && !strstr(ac, "*/*"))
		return errmsg("Unsupported Accept");

	/* read the query */
	cl = thash_get(req->http.headers, "Content-Length");
	if (!cl) return errmsg("Content-Length needed");

	len = atoi(cl);
	if (len < 0)
		return errmsg("Unsupported Content-Length");

	return readjson_len(req, len);
}
