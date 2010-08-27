/*
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pforemski@asn.pl>
 * All rights reserved
 */

#include "common.h"

static char *common(struct req *req)
{
	json *js = json_create(req->mm);
	ut *rep = ut_new_thash(NULL, req->mm);

	uth_set_char(rep, "jsonrpc", "2.0");
	if (req->id)
		uth_set_char(rep, "id", req->id);

	uth_set(rep, ut_ok(req->reply) ? "result" : "error", req->reply);

	return json_print(js, rep);
}

void writejson(struct req *req)
{
	char *txt = common(req);

	write(1, txt, strlen(txt));
	write(1, "\n\n", 2);
}

void write822(struct req *req)
{
	char *k;
	ut *v;

	if (ut_type(req->reply) == T_HASH) {
		THASH_ITER_LOOP(ut_thash(req->reply), k, v)
			printf("%s: %s\n", k, ut_char(v));
		write(1, "\n", 1);
	} else {
		printf("result: %s\n\n", ut_char(req->reply));
	}
}

void writehttp(struct req *req)
{
	int code = 200;
	char *msg = "OK";
	char *type = "application/json-rpc";
	char *header = "";
	char *txt;

	if (!ut_ok(req->reply)) switch (ut_errcode(req->reply)) {
		case JSON_RPC_PARSE_ERROR:
		case JSON_RPC_INVALID_INPUT:
		case JSON_RPC_INVALID_REQUEST:
		case JSON_RPC_INVALID_PARAMS:
			code = 400;
			msg = "Bad Request";
			break;
		
		case JSON_RPC_NOT_FOUND:
			code = 404;
			msg = "Not Found";
			break;

		case JSON_RPC_ACCESS_DENIED:
			if (R.auth)
				header = mmatic_printf(req->mm, "WWW-Authenticate: Basic realm=\"%s\"\n", CFG("name"));

			if (R.auth && !req->claim_user) {
				code = 401;
				msg = "Authorization Required";
			} else {
				code = 403;
				msg = "Forbidden";
			}
			break;

		case JSON_RPC_HTTP_NOT_FOUND:
			code = 404;
			msg = "Not Found";
			type = "text/html";
			txt = "<html><body>Document not found</body></html>";
			goto printtxt;

		case JSON_RPC_HTTP_OPTIONS:
			txt = "";
			type = "text/plain";
			header = "Allow: GET,POST,OPTIONS\n";
			goto printtxt;

		case JSON_RPC_HTTP_GET:
			txt = asn_readfile(req->uripath, req->mm);

			if (txt) {
				type = "text/html"; /* FIXME */
				goto printtxt;
			} /* else fall-through */

		default:
			code = 500;
			msg = "Internal Server Error";
			break;
	}

	txt = common(req);

printtxt:
	printf(
		"HTTP/1.1 %d %s\n"
		"Server: rpcd\n"
		"%s"
		"Content-Type: %s\n"
		"Content-Length: %d\n"
		"Connection: %s\n\n"
		"%s\n",
		code, msg, header, type, strlen(txt) + 1,
		(req->last ? "Close" : "Keep-alive"),
		txt);

	fflush(stdout);
}
