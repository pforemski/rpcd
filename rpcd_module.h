/*
 * rpcd - a JSON-RPC bridge
 *
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pforemski@asn.pl>
 *
 * All rights reserved
 */

#ifndef _RPCD_MODULE_H_
#define _RPCD_MODULE_H_

#include <libasn/lib.h>
#include "rpcd.h"
#include "standard.h"

/***************************************************************************************************/

struct rpcd {
	ut *cfg;                           /** configuration: * */
	thash *svcs;                       /** char (service name) => struct svc: available services */
	struct svc *defsvc;                /** default service */
};

struct svc {
	struct rpcd *rpcd;                 /** way up */
	const char *name;                  /** service name */
	ut *cfg;                           /** configuration: svc.* */
	ut *prv;                           /** service internal data hash */

	thash *dirs;                       /** char (dir basename) => struct dir: directories in this service */
	struct dir *defdir;                /** default directory */
};

struct dir {
	struct svc *svc;                   /** way up */
	const char *name;                  /** directory basename */
	ut *cfg;                           /** configuration: svc.dir.* */
	ut *prv;                           /** directory internal data hash */

	const char *path;                  /** full directory path */
	thash *modules;                    /** char (module name) => struct mod: modules implementing procedures */
	struct mod *common;                /** the common module */
};

struct mod {
	struct dir *dir;                   /** way up */
	const char *name;                  /** procedure name */
	ut *cfg;                           /** configuration: svc.dir.mod */
	ut *prv;                           /** module internal data hash */

	const char *path;                  /** full path to module file (XXX: != dir->name/name - includes extension) */
	enum modtype { C, JS, SH } type;   /** implemented in? */
	struct api *api;                   /** implementation API */
	struct fw *fw;                     /** array of firewall rules, ended by NULL */
};

struct req {
	struct mod *mod;                   /** way up */
	ut *prv;                           /** request internal data hash */

	const char *service;               /** called service */
	const char *method;                /** called method */
	const char *id;                    /** optional ID, if present */
	ut *params;                        /** the "params" argument */
	ut *reply;                         /** reply, may be NULL */

	const char *user;                  /** if not null, points at authenticated user */
	const char *pass;                  /** if not null, holds password of authed user */
	bool last;                         /** if true, exit after handling this request */

	/* HTTP handling */
	struct req_http {
		thash *headers;                /** if not NULL, holds HTTP headers */
		const char *uripath;           /** full filesystem path to requested doc */
		const char *user;              /** requester claims to be this user */
		const char *pass;              /** and gives us this password to verify him */
		bool needauth;                 /** if true, require authentication if available */
	} http;
};

struct api {
	uint32_t tag;                      /** for sanity checks */
#define RPCD_TAG 0x13370003

	/** Module initialization
	 * @param   mod   module instance, feel free to use mod->prv */
	bool (*init)(struct mod *mod);

	/** Module deinitialization
	 * @param   mod   module instance, feel free to use mod->prv */
	bool (*deinit)(struct mod *mod);

	/** RPC handler
	 * @param  req    user request, feel free to use req->prv
	 * @retval true   send reply to user, set reply to "true" if empty
	 * @retval false  stop request, show error in rep or a generic error */
	bool (*handle)(struct req *req);

	void *prv;                         /** for implementation-dependent use */
};

struct fw {
	const char *name;                  /** parameter name */
	bool required;                     /** if true, fail if parameter not found */
	enum ut_type type;                 /** parameter type
	                                    * @note its important, because properly done conversion will ensure
	                                    *       economic memory usage */
	const char *regexp;                /** regexp to run against ut_char(param) */
};

/***************************************************************************************************/

/** Set error in req->reply
 * @param req       request to update req->reply to new ut_err in
 * @param code      error code
 * @param msg       error message; if code is from standard.h, will be set automatically if null
 * @param data      additional error description; will be set to "cfile#cline" if null
 * @param cfile     source code file that error originated in
 * @param cline     source code line that originated the error
 * @return always returns false; nice usage: if (!condition) return error(req, ...); */
bool rpcd_error(struct req *req, int code, const char *msg, const char *data,
	const char *cfile, unsigned int cline);

/** Wrapper around error() which automatically retrieves cfile and cline */
#define err(code, msg, data) rpcd_error(req, (code), (msg), (data), __FILE__, __LINE__)

/** Wrapper around err() which generates just an error code */
#define errcode(code) err((code), NULL,  NULL)

/** Wrapper around err() which generates just an error message, with error code of JSON_RPC_ERROR (-32092) */
#define errmsg(msg) err(JSON_RPC_ERROR, (msg),  NULL)

/** Wrapper around err() which generates an error out of errno global variable
 * Useful for reporting of errors in system calls.
 * @param ctx       textual context - eg. function name that failed (required) */
#define errsys(ctx) err(JSON_RPC_ERROR, strerror(errno), \
                        mmatic_printf(req, "%s: errno %d in %s#%u", (ctx), errno, __FILE__, __LINE__))

#endif
