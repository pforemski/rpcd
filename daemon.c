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
#include <dlfcn.h>
#include <libasn/lib.h>
#include "common.h"

__USE_LIBASN

mmatic *mm;
mmatic *mmtmp;

/** Pointer at function reading new request */
int (*readreq)(struct req *req);

/** Pointer at function writing reply */
void (*writerep)(struct req *req);

/* prototypes */
static void scan_dir(const char *dir);

/****************************************************/

static void free_mod(void *ptr)
{
	struct mod *mod = (struct mod *) ptr;

	mmfreeptr(mod->name);
	mmfreeptr(mod->dir);
	mmfreeptr(mod->path);
}

/****************************************************/

/** SIGUSR1 handler which shows mm dump */
static void show_memory() { if (mm) mmatic_summary(mm, 0); }

/** SIGTERM/INT handler */
static void finish() { unlink(R.pidfile); exit(0); }

/** Prints usage help screen */
static void help(void)
{
	printf("Usage: rpcd [OPTIONS] [<DIR1>=.] [<DIR2>...]\n");
	printf("\n");
	printf("  A JSON-RPC server. <DIR> contains modules to export\n");
	printf("\n");
	printf("Options:\n");
	printf("  --json            read/write in JSON-RPC (default)\n");
	printf("  --rfc822          read/write in RFC822\n");
	printf("\n");
	printf("  --http            read/write in JSON-RPC over HTTP\n");
	printf("  --htpasswd=<file> require HTTP authentication from file (plain passwords)\n");
	printf("  --htdocs=<dir>    serve static HTTP docs from given dir\n");
	printf("\n");
	printf("  --name=<name>     my name (by default take first rpcd dir)\n");
	printf("  --daemonize,-d    daemonize, log to syslog\n");
	printf("  --pidfile=<path>  where to write daemon PID to [%s]\n", RPCD_DEFAULT_PIDFILE);
	printf("  --verbose         be verbose (alias for --debug=5)\n");
	printf("  --debug=<num>     set debugging level\n");
	printf("\n");
	printf("  --help,-h         show this usage help screen\n");
	printf("  --version,-v      show version and copying information\n");
	return;
}

/** Prints version and copying information. */
static void version(void)
{
	printf("rpcd %s\n", RPCD_VER);
	printf("Copyright (C) 2009-2010 ASN Sp. z o.o.\n");
	printf("All rights reserved.\n");
	return;
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
	R.daemonize = false;
	R.pidfile = RPCD_DEFAULT_PIDFILE;
	R.name = NULL;
	R.htpasswd = NULL;

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
			case  5 : R.daemonize = 1; break;
			case  6 : R.pidfile = optarg; break;
			case  7 : readreq = read822;  writerep = write822; break;
			case  8 : readreq = readhttp; writerep = writehttp; break;
			case  9 : readreq = readjson; writerep = writejson; break;
			case 10 : R.name = optarg; break;
			case 11 : R.htpasswd = optarg; break;
			case 12 : R.htdocs = optarg; break;
			default: help(); return 0;
		}
	}

	if (argc - optind < 1) {
		R.name = "rpcd";
		scan_dir(".");
	} else {
		while (argc - optind > 0) {
			if (R.name == NULL)
				R.name = pb("rpcd %s", argv[optind]);

			scan_dir(argv[optind++]);
		}
	}

	return 1;
}

/******************************************************/

/** Run init() in given modules */
void init_modules(thash *modules)
{
	char *k;
	struct mod *mod;

	THASH_ITER_LOOP(modules, k, mod) {
		if (!mod->api->init(mod)) {
			fprintf(stderr, "Module initialization failed: %s\n", mod->path);
			exit(1);
		}
	}
}


/******************************************************/

static void handle(struct req *req)
{
	struct mod *common;

	/* find handler */
	if (!req->method ||
		!(req->mod = thash_get(R.modules, req->method))) {
		errcode(JSON_RPC_NOT_FOUND);
		goto reply;
	}

	/* find common module */
	common = thash_get(R.commons, req->mod->dir);

	/* check the common firewall */
	if ((common && common->fw && !generic_fw(req, common->fw)) ||
		(req->mod->fw && !generic_fw(req, req->mod->fw))) {
		if (!req->reply) errcode(JSON_RPC_INVALID_INPUT);
		goto reply;
	}

	/* for simpler handle() implementation */
	req->reply = ut_new_thash(NULL, req->mm);

	/* handle - run common/handle, then mod/handle */
	if ((common && !common->api->handle(req, req->mm)) ||
		(!req->mod->api->handle(req, req->mm))) {
		if (ut_ok(req->reply)) errcode(JSON_RPC_ERROR);
		goto reply;
	}

reply:
	if (!req->reply) errcode(JSON_RPC_NO_OUTPUT);
}

struct req *request(const char *method, ut *params)
{
	/* TODO: merge with main */
	struct req *req;

	req = mmatic_zalloc(sizeof(*req), mmtmp);
	req->mm = mmtmp;

	req->method = method;
	req->params = params;

	handle(req);

	return req;
}

int main(int argc, char *argv[])
{
	struct req *req;
	json *js;

	/* init some vars */
	mm = mmatic_create();
	mmtmp = mmatic_create();
	R.modules  = MMTHASH_CREATE_STR(free_mod);
	R.commons  = MMTHASH_CREATE_STR(free_mod);

	/* setup signal handling */
	signal(SIGUSR1, show_memory);
	signal(SIGTERM, finish);
	signal(SIGINT,  finish);

	/**********************/

	/* parse arguments and load modules */
	switch (init(argc, argv)) {
		case 0: return 1;
		case 2: return 0;
	}

	/* initialize common modules */
	init_modules(R.commons);
	init_modules(R.modules);

	if (R.daemonize)
		asn_daemonize(R.name, R.pidfile);

	int read_status;

	do {
		/* flush temp mem */
		mmatic_free(mmtmp);
		mmtmp = mmatic_create();

		/* prepare request struct */
		req = mmatic_zalloc(sizeof(*req), mmtmp);
		req->mm = mmtmp;

		/* prepare parser */
		js = json_create(req->mm);

		/* read the request */
		read_status = readreq(req);
		if (read_status == 0) /* invalid */
			goto reply;

		/* dump it for debugging purposes */
		if (read_status == 1 && req->params) /* all OK */
			dbg(5, "%s\n", ut_char(req->params));

		/* authenticate */
		if (R.htpasswd) {
			req->user = auth(req);

			if (!req->user) {
				errcode(JSON_RPC_ACCESS_DENIED);
				goto reply;
			}
		}

		/* mostly for HTTP file server */
		if (read_status == 2)
			goto reply;

		handle(req);
reply:
		writerep(req);
	} while (req->last == false);

	return 1;
}

/* for Vim autocompletion:
 * vim: path=.,/usr/include,/usr/local/include,~/local/include
 */
