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
#include <dlfcn.h>
#include <libasn/lib.h>
#include "common.h"

/** Parse config file
 * @retval NULL   failed */
static ut *read_config(struct rpcd *rpcd, const char *config_file)
{
	FILE *fp;
	xstr *xs;
	char buf[BUFSIZ], *str;
	json *js;
	ut *cfg;

	if (config_file == NULL) {
		config_file = RPCD_DEFAULT_CONFIGFILE;
	}

	if (config_file[0] == '/' || (config_file[0] == '.' && config_file[1] == '/')) {
		/* read file contents, ignoring empty lines and comments */
		fp = fopen(config_file, "r");
		if (!fp) {
			dbg(1, "%s: fopen() failed: %s\n", config_file, strerror(errno));
			return NULL;
		}

		xs = xstr_create("{", rpcd);
		while (fgets(buf, sizeof buf, fp)) {
			str = asn_trim(buf);
			if (!str || !str[0] || str[0] == '#') continue;
			xstr_append(xs, str);
		}
		xstr_append_char(xs, '}');
		fclose(fp);

		config_file = xstr_string(xs);
	} else if (config_file[0]) {
		config_file = mmatic_printf(rpcd, "{ rpcd = { %s } }", config_file);
	}

	if (config_file && config_file[0]) {
		/* parse config file as loose JSON */
		js = json_create(rpcd);
		json_setopt(js, JSON_LOOSE, 1);

		cfg = json_parse(js, config_file);
		if (!ut_ok(cfg)) {
			dbg(1, "Parsing config file failed: %s\n", ut_err(cfg));
			return NULL;
		}
	} else {
		/* start with empty config */
		cfg = ut_new_thash(NULL, rpcd);
	}

	return cfg;
}

/** Load given module
 *
 * @param dir       directory containing module file
 * @param filename  name of module file (relative to dir)
 * @return          pointer to struct mod
 * @retval NULL     loading failed */
static struct mod *load_module(struct dir *dir, const char *filename, bool *skipflag)
{
	struct mod *mod;
	char *ext;

	*skipflag = 0;

	mod = mmatic_zalloc(sizeof *mod, dir);
	mod->dir  = dir;
	mod->name = asn_replace("/\\.[a-z]+$/", "", filename, mod);
	mod->prv  = ut_new_thash(NULL, mod);
	mod->path = mmatic_printf(mod, "%s/%s", dir->path, filename);

	if (asn_isfile(mod->path) < 0)
		goto skip;

	ext = asn_replace("/.*(\\.[a-z]+)$/", "\\1", filename, mod);
	if (streq(ext, ".sh")) {
		if (!asn_isexecutable(mod->path)) {
			dbg(1, "%s: exec bit not set - skipping\n", mod->path);
			goto skip;
		}

		mod->type = SH;
		mod->api = &sh_api;
	} else if (streq(ext, ".so")) {
		mod->type = C;

		void *so = dlopen(mod->path, RTLD_LAZY | RTLD_GLOBAL);
		if (!so) {
			dbg(0, "%s failed: %s\n", mod->name, dlerror());
			return NULL;
		}

		mod->api = dlsym(so, mmatic_printf(mod, "%s_api", mod->name));
		if (!mod->api) {
			dbg(1, "%s: warning - no API found\n", mod->name);
			mod->api = &generic_api;
		}

		mod->fw = dlsym(so, mmatic_printf(mod, "%s_fw", mod->name));
	} else if (streq(ext, ".js")) {
		dbg(1, "%s: JS not supported yet\n", mod->path);
		goto skip;
	} else goto skip;

	if (mod->api->tag != RPCD_TAG) {
		dbg(0, "%s failed: invalid API magic\n", mod->path);
		goto skip;
	}

	if (!mod->api->init)
		mod->api->init = generic_init;

	if (!mod->api->deinit)
		mod->api->deinit = generic_deinit;

	if (!mod->api->handle)
		mod->api->handle = generic_handle;

	asnsert(mod->api);
	dbg(1, "loaded %s\n", mod->path);

	return mod;

skip:
	mmatic_freeptr(mod);
	*skipflag = 1;
	return NULL;
}

/** Load given directory
 * @retval NULL   failed */
static struct dir *load_dir(struct svc *svc, const char *dirpath, ut *dircfg)
{
	struct dir *dir;
	struct mod *mod;
	const char *modname;
	ut *modcfg;
	thash *t;
	char *filename;
	tlist *ls;
	bool skip;

	dir = mmatic_zalloc(sizeof *dir, svc);
	dir->svc = svc;
	dir->name = asn_basename(dirpath);
	dir->cfg = uth_get(dircfg, "*"); /* TODO: merge svc.* */
	dir->prv = ut_new_thash(NULL, dir);
	dir->path = dirpath; /* TODO: abspath */
	dir->modules = thash_create_strkey(NULL, dir);

	/* scan directory */
	ls = asn_ls(dir->path, dir);

	/* load the common module first */
	TLIST_ITER_LOOP(ls, filename) {
		if (strncmp(filename, "common.", 7) == 0) {
			tlist_remove(ls);

			mod = load_module(dir, filename, &skip);
			if (mod) {
				thash_set(dir->modules, "common", mod);
				dir->common = mod;
				break; /* first match wins */
			} else if (skip == false) {
				return NULL;
			}
		}
	}

	/* load the rest */
	TLIST_ITER_LOOP(ls, filename) {
		mod = load_module(dir, filename, &skip);
		if (mod) {
			thash_set(dir->modules, mod->name, mod);
		} else if (skip == false) {
			return NULL;
		}
	}

	/* try to bind configs */
	t = ut_thash(dircfg);
	THASH_ITER_LOOP(t, modname, modcfg) {
		mod = thash_get(dir->modules, modname);

		if (mod)
			mod->cfg = modcfg; /* TODO: merge svc.dir.* */
		else
			dbg(1, "%s.%s: could not find matching module for key '%s'\n",
				svc->name, dir->name, modname);
	}

	return dir;
}

/** Load given service
 * @retval NULL  failed */
static struct svc *load_svc(struct rpcd *rpcd, const char *svcname, ut *svccfg)
{
	struct svc *svc;
	struct dir *dir;
	const char *dirpath;
	ut *dircfg;
	thash *t;

	svc = mmatic_zalloc(sizeof *svc, rpcd);
	svc->rpcd = rpcd;
	svc->name = svcname;
	svc->cfg = uth_get(svccfg, "*"); /* TODO: merge * */
	svc->prv = ut_new_thash(NULL, svc);
	svc->dirs = thash_create_strkey(NULL, svc);

	t = ut_thash(svccfg);
	THASH_ITER_LOOP(t, dirpath, dircfg) {
		dir = load_dir(svc, dirpath, dircfg);

		if (dir) {
			thash_set(svc->dirs, asn_basename(dirpath), dir);

			if (!svc->defdir)
				svc->defdir = dir;
		} else return NULL;
	}

	return svc;
}

/***************************************************************************************************/
/***************************************************************************************************/
/***************************************************************************************************/

struct rpcd *rpcd_init(const char *config_file)
{
	struct rpcd *rpcd;
	ut *rootcfg, *svccfg;
	const char *svcname, *dirname, *modname;
	struct svc *svc;
	struct dir *dir;
	struct mod *mod;
	thash *t;

	rpcd = mmatic_zalloc(sizeof *rpcd, mmatic_create());

	/* parse config file */
	rootcfg = read_config(rpcd, config_file);

	if (rootcfg)
		rpcd->cfg = uth_get(rootcfg, "*");
	else
		goto err;

	/* read services */
	rpcd->svcs = thash_create_strkey(NULL, rpcd);

	t = ut_thash(rootcfg);
	THASH_ITER_LOOP(t, svcname, svccfg) {
		svc = load_svc(rpcd, svcname, svccfg);

		if (svc) {
			thash_set(rpcd->svcs, svcname, svc);

			if (!rpcd->defsvc)
				rpcd->defsvc = svc;
		} else goto err;
	}

	/* run init() in each module */
	THASH_ITER_LOOP(rpcd->svcs, svcname, svc) {
		THASH_ITER_LOOP(svc->dirs, dirname, dir) {
			THASH_ITER_LOOP(dir->modules, modname, mod) {
				if (!mod->api->init(mod)) {
					dbg(0, "%s: module initialization failed\n", mod->path);
					goto err;
				}
			}
		}
	}

	return rpcd;
err:
	mmatic_free(rpcd);
	return NULL;
}

void rpcd_deinit(struct rpcd *rpcd)
{
	mmatic_free(rpcd);
}

bool rpcd_dir_load(struct rpcd *rpcd, const char *path)
{
	/* TODO */
	return true;
}

ut *rpcd_request(struct rpcd *rpcd, const char *method, ut *params)
{
	return ut_new_char("aaa kotki dwa", mmatic_create());
}

void rpcd_reqfree(ut *reply)
{
	mmatic_free(reply);
}

/***************************************************************************************************/
/***************************************************************************************************/
/***************************************************************************************************/

/** Generate an error JSON-RPC response based on error code */
bool rpcd_error(struct req *req, int code, const char *msg, const char *data,
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
		case JSON_RPC_ERROR:           msg = "Error"; break;
	}

	if (!data)
		data = mmatic_printf(req, "%s#%d", cfile, cline);

	req->reply = ut_new_err(code, msg, data, req);
	return false;
}

/* for Vim autocompletion:
 * vim: path=.,/usr/include,/usr/local/include,~/local/include
 */
