/*
 * rpcd - a JSON-RPC bridge
 *
 * Copyright (C) 2009 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 *
 * All rights reserved
 */

#ifndef _RPCD_H_
#define _RPCD_H_

#include <libasn/lib.h>
#include "standard.h"

#define RPCD_VER "0.1"
#define RPCD_DEFAULT_PIDFILE "/var/run/rpcd.pid"

struct rpcd;
struct req;
struct rep;
struct rrule;
struct mod;

/** Global data root */
struct rpcd {
	const char *startdir;    /** starting $PWD */
	const char *pidfile;     /** path to store PID in */
	bool daemonize;          /** start in background? */

	const char *fcdir;       /** Flatconf datatree to read config from */
	thash *fc;               /** Flatconf dump */

	thash *modules;          /** char *command -> struct mod *module */
	thash *env;              /** global environment skeleton */
	thash *rrules;           /** char*s -> struct rrule*: the global regexp firewall */
//	tlist *checks;           /** TODO: list of struct mod*: modules of which check() to issue on each request */
} R;

#define CFG(name) (asn_fcget(R.fc, (name)))

/** A JSON-RPC request representation */
struct req {
	const char *id;          /** optional ID, if present */
	const char *method;      /** called procedure */
	tlist *params;           /** list of char *: positional parameters - ie. without keys */
	thash *args;             /** char *key -> char *val: named arguments (for JSON-RPC 1.1WD+) */

	struct mod *mod;         /** handler module */
	mmatic *mm;              /** mmatic that will be flushed after handling */
	thash *env;              /** request environment, initially cloned from R.env */
};

/** A JSON-RPC reply */
struct rep {
	const struct req *req;   /** reference on request */

	enum json_reptype type;  /** reply object type */
	union repdata {
		bool        as_bool;
		const char *as_string;
		int         as_int;
		double      as_double;
		tlist      *as_tlist;
		thash      *as_thash;
		struct err { int code; const char *msg; const char *data; } *as_err;
	} data;
};

/** Regexp firewall rule */
struct rrule {
	bool cgi;                /** match CGI requests */
	bool cli;                /** match CLI requests */
	bool lib;                /** match direct (library) requests */
	tlist *users;            /** if not NULL - match reqs from these users only */

	const char *var;         /** name of variable to check */
	tlist *regexp;           /** list of char*s: regular expressions that the value must satisfy */
	tlist *nregexp;          /** same as regexp, but negative (match = fail) */
};

/** Module representation */
struct mod {
	enum modtype { C, JS, SH } type; /** implemented in? */
	const char *path;        /** full path to module */

	thash *rrules;           /** regexp rules to check only for this command */

	/** Per-module custom firewall
	 * @retval true   continue to handler
	 * @retval false  stop request, show rep.data.as_err or access violation error */
	bool (*check)(const struct req *req, struct rep *rep);

	/** RPC handler
	 * @note may be null, meaning "just check the request"
	 * @retval true   send reply to user, set reply to "true" if empty
	 * @retval false  stop request, show rep.data.as_err or generic internal error */
	bool (*handle)(const struct req *req, struct rep *rep);
};

#include "rep.h"
#include "read.h"
#include "sh.h"

/** Temporary mmatic flushed after each query */
extern mmatic *mmtmp;
#define pbt(...) (mmatic_printf(mmtmp, __VA_ARGS__))

/** Pointer at function reading new request */
extern enum readstatus (*readreq)(struct req *req, mmatic *mm);

#endif
