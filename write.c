/*
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pforemski@asn.pl>
 * All rights reserved
 */

#define _GNU_SOURCE

#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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

bool writehttp_get(struct req *req)
{
	int code = 200;
	char *msg = "OK";
	const char *type = "application/octet-stream";
	char date[128];
	struct stat ss;
	char *ms, *ext, buf[BUFSIZ];
	int fd = 0, r;
	time_t now;
	struct tm mod_client;
	struct tm mod_server;

	/* check file */
	if (stat(req->uripath, &ss) == -1)
		return errsys("stat()");

	/* date/time stuff */
	now = time(NULL);
	gmtime_r(&ss.st_mtime, &mod_server);

	/* dont open the file if client is up to date */
	if ((ms = thash_get(req->hh, "If-Modified-Since")) &&
		strptime(ms, RFC_DATETIME, &mod_client) != NULL &&
		mktime(&mod_client) >= mktime(&mod_server)) {
		code = 304;
		msg = "Not Modified";
	} else {
		fd = open(req->uripath, O_RDONLY, O_NOATIME);

		if (fd == -1)
			return errsys("open()");
	}

	/* say hello */
	printf("HTTP/1.1 %u %s\n", code, msg);
	printf("Server: rpcd\n");
	printf("Connection: %s\n", req->last ? "Close" : "Keep-alive");

	strftime(date, sizeof date, RFC_DATETIME, gmtime(&now));
	printf("Date: %s\n", date);

	if (code == 200) {
		strftime(date, sizeof date, RFC_DATETIME, &mod_server);
		printf("Last-Modified: %s\n", date);

		/* XXX: expire in 1h */
		now += 3600;
		strftime(date, sizeof date, RFC_DATETIME, gmtime(&now));
		printf("Expires: %s\n", date);

		/* MIME stuff */
		if ((ext = strrchr(req->uripath, '.')))
			type = asn_ext2mime(ext + 1);

		printf("Content-Type: %s\n", type);
		printf("Content-Length: %u\n", (unsigned int) ss.st_size);
	}

	/* headers end */
	printf("\n");
	fflush(stdout);

	if (code == 200) {
		/* send file */
		while ((r = read(fd, buf, sizeof buf)) > 0)
			write(1, buf, r);

		close(fd);
	}

	return true;
}

void writehttp(struct req *req)
{
	int code = 200;
	char *msg = "OK", *txt = "", *header = "";
	const char *type = "application/json-rpc";
	time_t now;
	char date[128];

	/* get current time */
	now = time(NULL);
	strftime(date, sizeof date, RFC_DATETIME, gmtime(&now));

	if (!ut_ok(req->reply)) switch (ut_errcode(req->reply)) {
		case JSON_RPC_ACCESS_DENIED:
			if (R.htpasswd)
				header = mmatic_printf(req->mm, "WWW-Authenticate: Basic realm=\"%s\"\n", R.name);

			if (R.htpasswd && !req->claim_user) {
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
			if (writehttp_get(req)) {
				return;
			} else {
				code = 500;
				msg = "Internal Server Error";
			}
			break;

		default:
			break;
	}

	txt = common(req);

printtxt:
	printf(
		"HTTP/1.1 %d %s\n"
		"Server: rpcd\n"
		"Date: %s\n"
		"Connection: %s\n"
		"%s"
		"Content-Type: %s\n"
		"Content-Length: %d\n"
		"\n"
		"%s\n",
		code, msg, date,
		(req->last ? "Close" : "Keep-alive"),
		header, type, (int) strlen(txt) + 1,
		txt);

	fflush(stdout);
}
