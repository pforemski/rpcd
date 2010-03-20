/*
 * rpcd - a JSON-RPC server
 *
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
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
bool (*readreq)(struct req *req);

/** Pointer at function writing reply */
void (*writerep)(struct req *req);

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
	printf("Usage: rpcd [OPTIONS] <CONFIG-DIR>\n");
	printf("\n");
	printf("  A JSON-RPC server. <CONFIG-DIR> contains Flatconf datatree describing\n");
	printf("  the application to serve.\n");
	printf("\n");
	printf("Options:\n");
	printf("  --json           read/write in JSON-RPC (default)\n");
	printf("  --http           read/write in JSON-RPC over HTTP\n");
	printf("  --rfc822         read/write in RFC822\n");
	printf("\n");
	printf("  --foreground,-f  dont daemonize, dont syslog\n");
	printf("  --pidfile=<path> where to write daemon PID to [%s]\n", RPCD_DEFAULT_PIDFILE);
	printf("  --verbose        be verbose (alias for --debug=5)\n");
	printf("  --debug=<num>    set debugging level\n");
	printf("\n");
	printf("  --help,-h        show this usage help screen\n");
	printf("  --version,-v     show version and copying information\n");
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
		{ "json",       0, NULL,  9  },
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
			case  9 : readreq = readjson; writerep = writejson; break;
			default: help(); return 0;
		}
	}

	if (argc - optind < 1) { fprintf(stderr, "Not enough arguments\n"); help(); return 0; }

	R.fcdir = argv[optind];
	return 1;
}

/** Generate an error JSON-RPC response based on error code */
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
		case JSON_RPC_HTTP_GET:
		case JSON_RPC_HTTP_OPTIONS:    msg = "OK"; break;
		case JSON_RPC_HTTP_NOT_FOUND:  msg = "Document not found"; break;
	}

	if (!data)
		data = mmatic_printf(req->mm, "%s#%d", cfile, cline);

	req->rep = ut_new_err(code, msg, data, req->mm);
	return false;
}

/** Load given module and register in given db
 *
 * @param dir       directory containing module file
 * @param filename  name of module file (relative to dir)
 * @return          pointer to struct mod @mm
 * @retval NULL     loading failed
 */
static struct mod *load_module(const char *dir, const char *filename)
{
	struct mod *mod;
	mod = mmatic_zalloc(sizeof(*mod), mm);
	mod->dir  = mmatic_strdup(dir, mm);
	mod->name = asn_replace("/\\.[a-z]+$/", "", filename, mm);
	mod->path = asn_abspath(pbt("%s/%s", mod->dir, filename), mm);

	if (asn_isfile(mod->path) < 0)
		goto skip;

	char *ext = asn_replace("/.*(\\.[a-z]+)$/", "\\1", filename, mmtmp);
	if (streq(ext, ".sh")) {
		if (!asn_isexecutable(mod->path)) {
			dbg(1, "%s: exec bit not set - skipping\n", mod->path);
			goto skip;
		}

		mod->type = SH;
		mod->api = &sh_api;
	} else if (streq(ext, ".so")) {
		mod->type = C;

		void *so = dlopen(mod->path, RTLD_NOW | RTLD_GLOBAL);
		if (!so) {
			dbg(0, "%s failed: %s\n", mod->name, dlerror());
			goto skip;
		}

		mod->api = dlsym(so, pbt("%s_module", mod->name));
		if (!mod->api) {
			dbg(0, "%s failed: %s\n", mod->name, dlerror());
			goto skip;
		}
	} else if (streq(ext, ".js")) {
		dbg(1, "%s: JS not supported yet\n");
		goto skip;
	} else goto skip;

	if (mod->api->magic != RPCD_MAGIC) {
		dbg(0, "loading '%s' failed: invalid API magic\n", mod->path);
		goto skip;
	}

	asnsert(mod->api);
	dbg(1, "loaded %s from %s\n", mod->name, mod->dir);

	return mod;

skip:
	free_mod((void *) mod);
	return NULL;
}

/** Scan given directory for modules and load them */
static void scan_dir(const char *dir)
{
	char *filename;
	struct mod *mod;

	dbg(5, "scanning %s\n", dir);
	tlist *ls = asn_ls(dir, mmtmp);

	/* load globals first */
	TLIST_ITER_LOOP(ls, filename) {
		if (asn_match(RPCD_GLOBAL_REGEX, filename)) {
			tlist_remove(ls);

			mod = load_module(dir, filename);
			if (mod)
				thash_set(R.globals, mod->dir, mod);
		}
	}

	/* load the rest */
	TLIST_ITER_LOOP(ls, filename) {
		mod = load_module(dir, filename);

		if (mod)
			thash_set(R.modules, mod->name, mod);
	}
}

int main(int argc, char *argv[])
{
	char *v, *k;
	struct req *req;
	struct mod *mod, *global;
	json *js;

	/* init some vars */
	mm = mmatic_create();
	mmtmp = mmatic_create();
	R.modules  = MMTHASH_CREATE_STR(free_mod);
	R.globals  = MMTHASH_CREATE_STR(free_mod);
	R.env      = MMTHASH_CREATE_STR(NULL); /* TODO: free f-n? */

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
				scan_dir(dir->elname);
		}
	}

	/* initialize global modules */
	THASH_ITER_LOOP(R.globals, k, mod) {
		if (mod->api->init)
			mod->api->init(mod);
	}

	/* initialize modules */
	THASH_ITER_LOOP(R.modules, k, mod) {
		if (mod->api->init)
			mod->api->init(mod);
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

		/* read the request */
		if (!readreq(req))
			goto reply;

		if (req->query)
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
		if (global && global->api->check && !global->api->check(req, req->mm)) {
			if (!req->rep) errcode(JSON_RPC_INVALID_INPUT);
			goto reply;
		}

		/* check arguments */
		if (req->mod->api->check && !req->mod->api->check(req, req->mm)) {
			if (!req->rep) errcode(JSON_RPC_INVALID_INPUT);
			goto reply;
		}

		/* handle */
		if (req->mod->api->handle && !req->mod->api->handle(req, req->mm)) {
			if (!req->rep) errcode(JSON_RPC_INTERNAL_ERROR);
			goto reply;
		}

		/* run global handle() */
		if (global && global->api->handle && !global->api->handle(req, req->mm)) {
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
