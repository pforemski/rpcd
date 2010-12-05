/*
 * rpcd - a JSON-RPC server
 *
 * Copyright (C) 2009-2010 Pawel Foremski <pawel@foremski.pl>
 * Licensed under GPLv3
 */

#define _GNU_SOURCE 1

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <libpjf/lib.h>
#include "common.h"

__USE_LIBASN

/** SIGTERM/INT handler */
static void finish() { unlink(O.pidfile); exit(0); }

/** Prints usage help screen */
static void help(void)
{
	printf("Usage: rpcd [OPTIONS] <CONFIG FILE>|<DIRECTORY>\n");
	printf("\n");
	printf("  A JSON-RPC server.\n");
	printf("\n");
	printf("Options:\n");
	printf("  --json                 read/write in JSON-RPC (default)\n");
	printf("  --rfc822               read/write in RFC822\n");
	printf("\n");
	printf("  --http                 read/write in JSON-RPC over HTTP\n");
	printf("  --htpasswd=<file>      require HTTP authentication from file (plain passwords)\n");
	printf("  --htdocs=<dir>         serve static HTTP docs from given dir\n");
	printf("\n");
	printf("  --daemonize,-d <name>  daemonize, log to syslog with given <name>\n");
	printf("  --pidfile=<path>       where to write daemon PID to [%s]\n", RPCD_DEFAULT_PIDFILE);
	printf("  --verbose              be verbose (alias for --debug=5)\n");
	printf("  --debug=<num>          set debugging level\n");
	printf("\n");
	printf("  --help,-h              show this usage help screen\n");
	printf("  --version,-v           show version and copying information\n");
	return;
}

/** Prints version and copying information. */
static void version(void)
{
	printf("rpcd %s\n", RPCD_VER);
	printf("Copyright (C) 2009-2010 Pawel Foremski <pawel@foremski.pl>\n");
	printf("Licensed under GNU GPL v3.\n");
}

/** Parses arguments and loads modules
 * @retval 0     error, main() should exit (eg. wrong arg. given)
 * @retval 1     ok
 * @retval 2     ok, but main() should exit (eg. on --version or --help) */
static int parse_argv(int argc, char *argv[])
{
	int i, c;

	static char *short_opts = "hvd";
	static struct option long_opts[] = {
		/* name, has_arg, NULL, short_ch */
		{ "verbose",    0, NULL,  1  },
		{ "debug",      1, NULL,  2  },
		{ "help",       0, NULL,  3  },
		{ "version",    0, NULL,  4  },
		{ "daemonize",  0, NULL,  5  },
		{ "pidfile",    1, NULL,  6  },
		{ "rfc822",     0, NULL,  7  },
		{ "http",       0, NULL,  8  },
		{ "json",       0, NULL,  9  },
		{ "name",       1, NULL, 10  },
		{ "htpasswd",   1, NULL, 11  },
		{ "htdocs",     1, NULL, 12  },
		{ 0, 0, 0, 0 }
	};

	/* set defaults */
	memset(&O, 0, sizeof O);

	O.pidfile = RPCD_DEFAULT_PIDFILE;
	O.name = "rpcd";
	O.mode = RPCD_JSON;
	O.read = readjson;
	O.write = writejson;

	for (;;) {
		c = getopt_long(argc, argv, short_opts, long_opts, &i);
		if (c == -1) break; /* end of options */

		switch (c) {
			case  1 : debug = 5; break;
			case  2 : debug = atoi(optarg); break;
			case 'h':
			case  3 : help(); return 2;
			case 'v':
			case  4 : version(); return 2;
			case 'd':
			case  5 : O.daemonize = 1; break;
			case  6 : O.pidfile = optarg; break;
			case  7 :
				O.mode = RPCD_RFC;
				O.read = read822;
				O.write = write822;
				break;
			case  8 :
				O.mode = RPCD_HTTP;
				O.read = readhttp;
				O.write = writehttp;
				break;
			case  9 :
				O.mode = RPCD_JSON;
				O.read = readjson;
				O.write = writejson;
				break;
			case 10 : O.name = optarg; break;
			case 11 : O.http.htpasswd = optarg; break;
			case 12 : O.http.htdocs = optarg; break;
			default: help(); return 0;
		}
	}

	if (argc - optind > 0) {
		O.config_file = argv[optind];
	} else {
		help();
		return 0;
	}

	return 1;
}

/** Pass request to librpcd
 * @retval true    request went through modules
 * @retval false   request handled internally - eg. error or HTTP GET */
bool handle(struct rpcd *rpcd, struct req *req)
{
	/* HTTP authentication */
	if (req->http.needauth && O.http.htpasswd) {
		auth_http(req);
		if (!req->user)
			return errcode(JSON_RPC_ACCESS_DENIED);
	}

	if (!ut_ok(req->reply))
		return false;

	/*
	 * Handle RPC call
	 */
	dbg(8, "params: %s\n", ut_char(req->params));
	rpcd_handle(rpcd, req);
	return true;
}

int main(int argc, char *argv[])
{
	struct rpcd *rpcd;
	struct req *req = NULL;

	signal(SIGTERM, finish);
	signal(SIGINT,  finish);

	switch (parse_argv(argc, argv)) {
		case 0: return 1;
		case 2: return 0;
	}

	if (asn_isdir(O.config_file) == 1) {
		rpcd = rpcd_init(asn_malloc_printf("\"%s\" = {}", O.config_file), true);
	} else {
		rpcd = rpcd_init(O.config_file, false);
	}

	if (!rpcd) {
		fprintf(stderr, "Initialization failed. Try --debug option to get more info.\n");
		return 2;
	}

	if (O.daemonize)
		asn_daemonize(O.name, O.pidfile);

	do {
		/* flush temp mem */
		if (req)
			mmatic_free(req);

		/* prepare request struct */
		req = mmatic_zalloc(sizeof *req, mmatic_create());
		req->prv = ut_new_thash(NULL, req);
		req->reply = ut_new_thash(NULL, req);

		/* handle it */
		O.read(req);
		handle(rpcd, req);
		O.write(req);
	} while (req->last == false);

	return 0;
}

/* for Vim autocompletion:
 * vim: path=.,/usr/include,/usr/local/include,~/local/include
 */
