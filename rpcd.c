/*
 * rpcd - a JSON-RPC server
 *
 * Copyright (C) 2009 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 *
 * All rights reserved
 */

#define _GNU_SOURCE 1

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include <libasn/lib.h>

#include "rpcd.h"

__USE_LIBASN

mmatic *mm;
mmatic *mmtmp;

bool (*readreq)();
void (*writerep)();

/** SIGUSR1 handler which shows mm dump */
static void show_memory() { if (mm) mmatic_summary(mm, 0); }

/** SIGTERM/INT handler */
static void finish() { unlink(R.pidfile); exit(0); }

/** Prints usage help screen */
static void help(void)
{
	printf("Usage: rpcd [OPTIONS] <CONFIG-DIR>\n");
	printf("\n");
	printf("  A JSON-RPC server. <CONFIG-DIR> contains Flatconf datatree describing\n");
	printf("  the application to serve.\n");
	printf("\n");
	printf("Options:\n");
	printf("  --rfc822         read/write in RFC822\n");
	printf("  --http           read/write in JSON-RPC over HTTP\n");
	printf("  --verbose        be verbose\n");
	printf("  --debug=<num>    set debugging level\n");
	printf("  --foreground,-f  dont daemonize, dont syslog\n");
	printf("  --pidfile=<path> where to write daemon PID to [%s]\n", RPCD_DEFAULT_PIDFILE);
	printf("  --help,-h        show this usage help screen\n");
	printf("  --version,-v     show version and copying information\n");
	return;
}

/** Prints version and copying information. */
static void version(void)
{
	printf("rpcd %s\n", RPCD_VER);
	printf("Copyright (C) 2009 ASN Sp. z o.o.\n");
	printf("All rights reserved.\n");
	return;
}

/** Parses Command Line Arguments.
 * @param  argc  argc passed from main()
 * @param  argv  argv passed from main()
 * @retval 0     error, main() should exit (eg. wrong arg. given)
 * @retval 1     ok
 * @retval 2     ok, but main() should exit (eg. on --version or --help)
 * @note   sets  R.fcdir */
static int parse_argv(int argc, char *argv[])
{
	int i, c;

	static char *short_opts = "hvf";
	static struct option long_opts[] = {
		/* name, has_arg, NULL, short_ch */
		{ "verbose",    0, NULL,  1  },
		{ "debug",      1, NULL,  2  },
		{ "help",       0, NULL,  3  },
		{ "version",    0, NULL,  4  },
		{ "foreground", 0, NULL,  5  },
		{ "pidfile",    1, NULL,  6  },
		{ "rfc822",     0, NULL,  7  },
		{ "http",       0, NULL,  8  },
		{ 0, 0, 0, 0 }
	};

	/* set defaults */
	R.daemonize = 1;
	R.pidfile = RPCD_DEFAULT_PIDFILE;
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
			case 'f':
			case  5 : R.daemonize = 0; break;
			case  6 : R.pidfile = optarg; break;
			case  7 : readreq = read822;  writerep = write822; break;
			case  8 : readreq = readhttp; writerep = writehttp; break;
			default: help(); return 0;
		}
	}

	if (argc - optind < 1) { fprintf(stderr, "Not enough arguments\n"); help(); return 0; }

	R.fcdir = argv[optind];
	return 1;
}

bool error(struct req *req, int code, const char *msg, const char *data,
	const char *cfile, unsigned int cline)
{
	enum json_errcode jcode = code;

	/* XXX: after conversion to enum so gcc catches missing ones */
	if (!msg) switch (jcode) {
		case JSON_RPC_PARSE_ERROR:     msg = "Parse error"; break;
		case JSON_RPC_INVALID_REQUEST: msg = "Invalid Request"; break;
		case JSON_RPC_NOT_FOUND:       msg = "Method not found"; break;
		case JSON_RPC_INVALID_PARAMS:  msg = "Invalid params"; break;
		case JSON_RPC_INTERNAL_ERROR:  msg = "Internal error"; break;
		case JSON_RPC_ACCESS_DENIED:   msg = "Access denied"; break;
		case JSON_RPC_OUT_PARSE_ERROR: msg = "Output parse error"; break;
		case JSON_RPC_INVALID_INPUT:   msg = "Invalid input"; break;
		case JSON_RPC_NO_OUTPUT:       msg = "No output"; break;
	}

	if (!data)
		data = mmatic_printf(req->mm, "%s#%d", cfile, cline);

	req->rep = ut_new_err(code, msg, data, req->mm);
	return false;
}

int main(int argc, char *argv[])
{
	char _path[PATH_MAX];
	char *v, *k, *path, *proc, *ext;
	enum modtype type;
	struct fcel *fcel;
	tlist *tlist, *ls;
	struct mod *mod;
	struct req *req;
	json *js;

	getcwd(_path, sizeof(_path));

	/* init some vars */
	mm = mmatic_create();
	mmtmp = mmatic_create();
	R.startdir = mmstrdup(_path);
	R.modules  = MMTHASH_CREATE_STR(NULL); /* TODO: free f-n? */
	R.env      = MMTHASH_CREATE_STR(NULL); /* TODO: free f-n? */
	R.rrules   = MMTHASH_CREATE_STR(NULL); /* TODO: free f-n? */

	/* setup signal handling */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGUSR1, show_memory);
	signal(SIGTERM, finish);
	signal(SIGINT,  finish);

	/* parse arguments */
	switch (parse_argv(argc, argv)) { case 0: exit(1); case 2: exit(0); }

	/* read Flatconf */
	R.fc = asn_fcdir(R.fcdir, mm);
	thash_dump(5, R.fc);

	/* export Flatconf as bot environment */
	THASH_ITER_LOOP(R.fc, k, v)
		thash_set(R.env, mmprintf("FC_%s", asn_replace("/[^a-zA-Z0-9_]/", "_", k, mm)), v);

	/* scan through fc:/dir */
	tlist = asn_fcparselist(CFG("dir/list.order"), mm);
	TLIST_ITER_LOOP(tlist, fcel) {
		if (!fcel->enabled) continue;

		ls = asn_ls(fcel->elname, mm);
		TLIST_ITER_LOOP(ls, v) {
			proc = asn_replace("/\\.[a-z]+$/", "", v, mmtmp);
			ext  = asn_replace("/.*(\\.[a-z]+)$/", "\\1", v, mmtmp);
			path = asn_abspath(
				mmatic_printf(mmtmp, "%s/%s", fcel->elname, v),
				mmtmp);

			if (streq(ext, ".sh") && asn_isexecutable(path))
				type = SH;
			else if (streq(ext, ".so"))
				type = C;
			else if (streq(ext, ".js"))
				type = JS;
			else
				continue;

			mod = mmalloc(sizeof(*mod));
			mod->type = type;
			mod->path = mmstrdup(path);
			mod->rrules = MMTHASH_CREATE_STR(NULL); /* TODO: free f-n? */

			switch (mod->type) {
				case SH:
					mod->check = sh_check;
					mod->handle = sh_handle;
					break;
				case C:
					/* TODO */
					mod->check = mod->handle = NULL;
					break;
				case JS:
					/* TODO */
					mod->check = mod->handle = NULL;
					break;
			}

			dbg(5, "added '%s': %s\n", proc, mod->path);
			thash_set(R.modules, proc, mod);
		}
	}

	/* TODO: init R.rrules */

	/* TODO: unhash when its needed */
//	if (R.daemonize)
//		asn_daemonize(CFG("name"), R.pidfile);

	do {
		/* flush temp mem */
		mmatic_free(mmtmp);
		mmtmp = mmatic_create();

		/* prepare request struct */
		req = mmatic_alloc(sizeof(*req), mmtmp);
		req->mm = mmtmp;
		req->env = thash_clone(R.env, mmtmp);
		req->id = NULL;
		req->method = NULL;
		req->query = NULL;
		req->rep = NULL;

		/* prepare parser */
		js = json_create(req->mm);

		if (!readreq(req))
			goto reply;
		else
			dbg(5, "%s\n", ut_char(req->query));

		/* find handler */
		if (!req->method ||
		    !(req->mod = thash_get(R.modules, req->method))) {
			errcode(JSON_RPC_NOT_FOUND);
			goto reply;
		}

		/* TODO: check regexps */

		/* check arguments */
		if (mod->check && !mod->check(req)) {
			if (!req->rep) errcode(JSON_RPC_INVALID_INPUT);
			goto reply;
		}

		/* handle */
		if (mod->handle && !mod->handle(req)) {
			if (!req->rep) errcode(JSON_RPC_INTERNAL_ERROR);
			goto reply;
		}

reply:
		if (!req->rep) errcode(JSON_RPC_NO_OUTPUT);

		writerep(req);
	} while (true);

	return 1;
}

/* for Vim autocompletion:
 * vim: path=.,/usr/include,/usr/local/include,~/local/include
 */
