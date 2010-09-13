/*
 * rpcd - a JSON-RPC server
 *
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pforemski@asn.pl>
 *
 * All rights reserved
 */

#define _GNU_SOURCE 1

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <libasn/lib.h>
#include "common.h"

__USE_LIBASN

/** Pointer at function reading new request */
int (*readreq)(struct req *req);

/** Pointer at function writing reply */
void (*writerep)(struct req *req);

/****************************************************/

/** SIGTERM/INT handler */
static void finish() { /* FIXME unlink(R.pidfile); */ exit(0); }

/** Prints usage help screen */
static void help(void)
{
	printf("Usage: rpcd [OPTIONS] <CONFIG FILE>\n");
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
	printf("Copyright (C) 2009-2010 ASN Sp. z o.o.\n");
	printf("All rights reserved.\n");
}

/** Parses arguments and loads modules
 * @retval 0     error, main() should exit (eg. wrong arg. given)
 * @retval 1     ok
 * @retval 2     ok, but main() should exit (eg. on --version or --help) */
static int init(int argc, char *argv[])
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
/* FIXME	R.daemonize = false;
	R.pidfile = RPCD_DEFAULT_PIDFILE;
	R.name = NULL;
	R.htpasswd = NULL;*/

	readreq = readjson;
	writerep = writejson;

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
//			case  5 : R.daemonize = 1; break;
//			case  6 : R.pidfile = optarg; break;
			case  7 : readreq = read822;  writerep = write822; break;
			case  8 : readreq = readhttp; writerep = writehttp; break;
			case  9 : readreq = readjson; writerep = writejson; break;
//			case 10 : R.name = optarg; break;
//			case 11 : R.htpasswd = optarg; break;
//			case 12 : R.htdocs = optarg; break;
			default: help(); return 0;
		}
	}

/*	if (argc - optind < 1) {
		R.name = "rpcd";
		scan_dir(".");
	} else {
		while (argc - optind > 0) {
			if (R.name == NULL)
				R.name = pb("rpcd %s", argv[optind]);

			scan_dir(argv[optind++]);
		}
	}*/

	return 1;
}

/******************************************************/

int main(int argc, char *argv[])
{
	struct rpcd *rpcd;
	struct req *req = NULL;
	json *js;

	signal(SIGTERM, finish);
	signal(SIGINT,  finish);

	switch (init(argc, argv)) {
		case 0: return 1;
		case 2: return 0;
	}

	// TODO + error handling
//	rpcd = rpcd_init(R.config_file);
	rpcd = rpcd_init("./rpcd.conf");

/*	if (R.daemonize)
		asn_daemonize(R.name, R.pidfile);*/

	int read_status;

	do {
		/* flush temp mem */
		if (req)
			mmatic_free(req);

		/* prepare request struct */
		req = mmatic_zalloc(sizeof *req, mmatic_create());
		req->prv = ut_new_thash(NULL, req);
		req->reply = ut_new_thash(NULL, req);

		/* prepare parser */
		js = json_create(req);

		/* read the request */
		read_status = readreq(req);
		if (read_status == 0) /* invalid */
			goto reply;

		/* dump it for debugging purposes */
		if (read_status == 1 && req->params) /* all OK */
			dbg(5, "%s\n", ut_char(req->params));

		/* authenticate */
/*		if (R.htpasswd) {
			req->user = auth(req);

			if (!req->user) {
				errcode(JSON_RPC_ACCESS_DENIED);
				goto reply;
			}
		}*/

		/* mostly for HTTP file server */
		if (read_status == 2)
			goto reply;

		req->mod = thash_get(rpcd->defsvc->defdir->modules, req->method);

		rpcd_handle(req);
reply:
		writerep(req);
	} while (req->last == false);

	return 1;
}

/* for Vim autocompletion:
 * vim: path=.,/usr/include,/usr/local/include,~/local/include
 */
