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

/** Scan dir for modules
 * @note stores reference to dir
 * TODO: handle overrides (maybe via @1) */
void scan_module_dir(const char *dir)
{
	struct mod *mod;
	char *ext, *filename;
	tlist *ls;

	dbg(5, "scanning %s\n", dir);

	ls = asn_ls(dir, mmtmp);
	TLIST_ITER_LOOP(ls, filename) {
		mod = mmzalloc(sizeof(*mod));
		mod->dir  = dir;
		mod->name = asn_replace("/\\.[a-z]+$/", "", filename, mm);
		mod->path = asn_abspath(pbt("%s/%s", mod->dir, filename), mm);
		mod->rrules = MMTHASH_CREATE_STR(NULL); /* TODO: free f-n? */

		ext = asn_replace("/.*(\\.[a-z]+)$/", "\\1", filename, mmtmp);
		if (streq(ext, ".sh") && asn_isexecutable(mod->path)) {
			mod->type = SH;

			mod->init = sh_init;
			mod->check = sh_check;
			mod->handle = sh_handle;
		} else if (streq(ext, ".so")) {
			mod->type = C;

			/* TODO */
			mod->init = NULL;
			mod->check = mod->handle = NULL;
		} else if (streq(ext, ".js")) {
			mod->type = JS;

			/* TODO */
			mod->init = NULL;
			mod->check = mod->handle = NULL;
		} else {
			/* TODO: free */
			continue;
		}

		if (streq(mod->name, "global"))
			thash_set(R.globals, mod->dir, mod); /* overrides: @1 */
		else
			thash_set(R.modules, mod->name, mod); /* overrides: @1 */

		dbg(5, "added '%s': %s\n", mod->name, mod->path);
	}
}

int main(int argc, char *argv[])
{
	char _path[PATH_MAX];
	char *v, *k;
	struct req *req;
	struct mod *mod, *global;
	json *js;

	getcwd(_path, sizeof(_path));

	/* init some vars */
	mm = mmatic_create();
	mmtmp = mmatic_create();
	R.startdir = mmstrdup(_path);
	R.modules  = MMTHASH_CREATE_STR(NULL); /* TODO: free f-n? @1 */
	R.globals  = MMTHASH_CREATE_STR(NULL); /* TODO: free f-n? @1 */
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
	{
		tlist *dirs = asn_fcparselist(CFG("dir/list.order"), mm);
		struct fcel *dir;

		TLIST_ITER_LOOP(dirs, dir) {
			if (dir->enabled)
				scan_module_dir(dir->elname);
		}
	}

	/* initialize global modules */
	THASH_ITER_LOOP(R.globals, k, mod) {
		if (mod->init)
			mod->init(mod->name);
	}

	/* initialize modules */
	THASH_ITER_LOOP(R.modules, k, mod) {
		if (mod->init)
			mod->init(mod->name);
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
		req = mmatic_zalloc(sizeof(*req), mmtmp);
		req->mm = mmtmp;
		req->env = thash_clone(R.env, mmtmp);

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

		/* find global module */
		global = thash_get(R.globals, req->mod->dir);

		/* run global check() */
		if (global && global->check && !global->check(req)) {
			if (!req->rep) errcode(JSON_RPC_INVALID_INPUT);
			goto reply;
		}

		/* check arguments */
		if (req->mod->check && !req->mod->check(req)) {
			if (!req->rep) errcode(JSON_RPC_INVALID_INPUT);
			goto reply;
		}

		/* handle */
		if (req->mod->handle && !req->mod->handle(req)) {
			if (!req->rep) errcode(JSON_RPC_INTERNAL_ERROR);
			goto reply;
		}

		/* run global handle() */
		if (global && global->handle && !global->handle(req)) {
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
