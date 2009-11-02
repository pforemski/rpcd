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

/* TODO: support OPTIONS

pjf@pjflap:/var/www/rpcd$ telnet localhost 80
Trying ::1...
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
OPTIONS / HTTP/1.1
Host: 127.0.0.1:8080
User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.1.4) Gecko/20091028 Ubuntu/9.04 (jaunty) Shiretoko/3.5.4
Accept-Language: en-us,en;q=0.5
Accept-Encoding: gzip,deflate
Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7
Keep-Alive: 300
Connection: keep-alive
Origin: http://localhost
Access-Control-Request-Method: POST
Access-Control-Request-Headers: x-requested-with

HTTP/1.1 200 OK
Date: Mon, 02 Nov 2009 17:08:12 GMT
Server: Apache/2.2.11 (Ubuntu) PHP/5.2.6-3ubuntu4.2 with Suhosin-Patch mod_scgi/1.12
Allow: GET,HEAD,POST,OPTIONS,TRACE
Vary: Accept-Encoding
Content-Encoding: gzip
Content-Length: 20
Keep-Alive: timeout=15, max=100
Connection: Keep-Alive
Content-Type: text/html

etc.
*/

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
