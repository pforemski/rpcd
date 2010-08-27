/*
 * rpcd - a JSON-RPC bridge
 *
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pforemski@asn.pl>
 *
 * All rights reserved
 */

#ifndef _RPCD_H_
#define _RPCD_H_

#include <libasn/lib.h>

#define RPCD_VER "0.2"
#define RPCD_DEFAULT_PIDFILE "/var/run/rpcd.pid"

#define RPCD_COMMON_REGEX "/^common\\.[a-z]+$/"

struct rpcd;
struct req;
struct mod;
struct api;

/** Global data root */
struct rpcd {
	const char *pidfile;     /** path to store PID in */
	const char *name;        /** syslog name */
	bool daemonize;          /** start in background? */

	const char *htpasswd;    /** if not empty, authenticate HTTP queries */
	const char *htdocs;      /** if not empty, serve static GET queries from this dir */

	thash *modules;          /** char name -> struct mod */
	thash *commons;          /** char directory -> struct mod: common modules */
	thash *env;              /** char name -> char val: common environment skeleton */
} R;

/** A simple "parameter firewall" rule */
struct fw {
	const char *name;        /** parameter name */
	bool required;           /** if true, fail if parameter not found */
	enum ut_type type;       /** parameter type
	                           * @note its important, because properly done conversion will ensure
	                           *       economic memory usage */
	const char *regexp;      /** regexp to run against ut_char(param) */
};

/** A JSON-RPC request representation */
struct req {
	const char *uripath;     /** full path to optional HTTP URI */
	const char *id;          /** optional ID, if present */
	const char *method;      /** called procedure */

	ut *query;               /** the "params" argument */
	ut *reply;               /** reply, may be NULL */

	struct mod *mod;         /** handler module */
	mmatic *mm;              /** mmatic that will be flushed after handling */
	thash *env;              /** request environment, initially cloned from R.env */
	void *prv;               /** for use by module */

	const char *claim_user;  /** requester claims to be this user */
	const char *claim_pass;  /** and gives us this password to verify him */
	const char *user;        /** if not null, points at authenticated user */

	bool last;               /** if true, exit after handling this request */
};

/** Module representation */
struct mod {
	char *name;                        /** procedure name */
	char *dir;                         /** directory path */
	char *path;                        /** full path to module file (XXX: != dir/name)*/

	enum modtype { C, JS, SH } type;   /** implemented in? */

	struct api *api;                   /** implementation API */
	struct fw *fw;                     /** array of firewall rules, ended by NULL */
};

/** Module API */
struct api {
	uint32_t magic;                         /** for sanity checks */
#define RPCD_MAGIC 0x13370002

	/** Module initialization */
	bool (*init)(struct mod *mod);

	/** Module deinitialization */
	bool (*deinit)(struct mod *mod);

	/** RPC handler
	 * @note          this variable may be null, meaning "just check the request"
	 *
	 * @param  req    user request
	 * @param  mm     req->mm
	 * @retval true   send reply to user, set reply to "true" if empty
	 * @retval false  stop request, show error in rep or generic error */
	bool (*handle)(struct req *req, mmatic *mm);

	void *prv;                         /** implementation-dependent use */
};

/********************************************************************/

/** Global mmatic flushed on program end */
extern mmatic *mm;
#define pb(...) (mmatic_printf(mm, __VA_ARGS__))

/** Temporary mmatic flushed after each query */
extern mmatic *mmtmp;
#define pbt(...) (mmatic_printf(mmtmp, __VA_ARGS__))

/** Sets error in req->reply
 * @return false */
bool error(struct req *req, int code, const char *msg, const char *data,
	const char *cfile, unsigned int cline);

#define err(code, msg, data) error(req, (code), (msg), (data), __FILE__, __LINE__)
#define errcode(code) err((code),   NULL,  NULL)
#define errmsg(msg)   err(    0,   (msg),  NULL)

#endif
