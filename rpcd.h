/*
 * rpcd - a JSON-RPC bridge
 *
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 *
 * All rights reserved
 */

#ifndef _RPCD_H_
#define _RPCD_H_

#include <libasn/lib.h>

#define RPCD_VER "0.1"
#define RPCD_DEFAULT_PIDFILE "/var/run/rpcd.pid"

#define RPCD_GLOBAL_REGEX "/^global\\.[a-z]+$/"

struct rpcd;
struct req;
struct mod;
struct api;

/** Global data root */
struct rpcd {
	const char *pidfile;     /** path to store PID in */
	bool daemonize;          /** start in background? */

	const char *fcdir;       /** Flatconf datatree to read config from */
	thash *fc;               /** Flatconf dump */

	thash *modules;          /** char name -> struct mod */
	thash *globals;          /** char directory -> struct mod: global modules */
	thash *env;              /** char name -> char val: global environment skeleton */
} R;

#define CFG(name) (asn_fcget(R.fc, (name)))

/** A JSON-RPC request representation */
struct req {
	const char *uripath;     /** full path to optional HTTP URI */
	const char *id;          /** optional ID, if present */
	const char *method;      /** called procedure */

	ut *query;               /** the "params" argument */
	ut *rep;                 /** reply, may be NULL */

	struct mod *mod;         /** handler module */
	mmatic *mm;              /** mmatic that will be flushed after handling */
	thash *env;              /** request environment, initially cloned from R.env */
};

/** Module representation */
struct mod {
	char *name;                        /** procedure name */
	char *dir;                         /** directory path */
	char *path;                        /** full path to module file (XXX: != dir/name)*/

	enum modtype { C, JS, SH } type;   /** implemented in? */

	struct api *api;                   /** implementation API */
};

/** Module API */
struct api {
	int magic;                         /** for sanity checks */
#define RPCD_MAGIC 0xDEADBEEF

	/** Module initialization */
	bool (*init)(struct mod *mod);

	/** Module deinitialization */
	bool (*deinit)(struct mod *mod);

	/** Per-module custom firewall
	 * @param  req    user request
	 * @param  mm     req->mm
	 * @retval true   continue to handler
	 * @retval false  stop request, show error in rep or access violation error */
	bool (*check)(struct req *req, mmatic *mm);

	/** RPC handler
	 * @note          this variable may be null, meaning "just check the request"
	 *
	 * @param  req    user request
	 * @param  mm     req->mm
	 * @retval true   send reply to user, set reply to "true" if empty
	 * @retval false  stop request, show error in rep or generic internal error */
	bool (*handle)(struct req *req, mmatic *mm);

	void *prv;                         /** implementation-dependent use */
};

/** Temporary mmatic flushed after each query */
extern mmatic *mmtmp;
#define pbt(...) (mmatic_printf(mmtmp, __VA_ARGS__))

/** Sets error in req->rep
 * @return false */
bool error(struct req *req, int code, const char *msg, const char *data,
	const char *cfile, unsigned int cline);

#define err(code, msg, data) error(req, (code), (msg), (data), __FILE__, __LINE__)
#define errcode(code) err((code),   NULL,  NULL)
#define errmsg(msg)   err(    0,   (msg),  NULL)

#endif
