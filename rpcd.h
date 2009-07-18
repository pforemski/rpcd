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

#define RPCD_VER "0.1"
#define RPCD_DEFAULT_PIDFILE "/var/run/rpcd.pid"

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
	tlist *checks;           /** list of struct mod*: modules of which check() to issue on each request */
} R;

#define CFG(name) (asn_fcget(R.fc, (name)))

/** A JSON-RPC request representation */
struct req {
	const char *id;          /** optional ID, if present */
	const char *method;      /** called procedure */
	tlist *params;           /** list of char *: positional parameters - ie. without keys */
	thash *args;             /** char *key -> char *val: named arguments (for JSON-RPC 1.1WD+) */

	thash *env;              /** request environment, initially cloned from R.env */
};

/** A JSON-RPC reply */
struct rep {
	const struct req *req;   /** reference on request */

	enum reptype {
		_T_NONE,     /* for internal use - meaning no reply */
		T_BOOL,      /* bool */
		T_STRING,    /* char*  */
		T_INT,       /* int    */
		T_DOUBLE,    /* double */
		T_LIST,      /* tlist* */
		T_HASH,      /* thash* */
		T_ERROR      /* http://groups.google.com/group/json-rpc/web/json-rpc-1-2-proposal#error-object */
	} type;

	union repdata {
		bool        as_bool;
		const char *as_char;
		int         as_int;
		double      as_double;
		tlist      *as_tlist;
		thash      *as_thash;
		struct err { int code; const char *message; const char *data; } *as_err;
	} data;
};

void rep_set_bool(struct rep *rep, bool arg);
void rep_set_string(struct rep *rep, const char *arg);
void rep_set_int(struct rep *rep, int arg);
void rep_set_double(struct rep *rep, double arg);
void rep_set_tlist(struct rep *rep, tlist *arg);
void rep_set_thash(struct rep *rep, thash *arg);
void rep_set_error(struct rep *rep, int code, const char *msg, const char *data);

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
	const char *path;        /** module file path */
	enum modtype { C, JS, SH } type; /** implemented in? */

	thash *rrules;           /** regexp rules to check only for this command */

	/** Per-module custom firewall
	 * @retval true   continue to handler
	 * @retval false  stop request, show rep.data.as_err or access violation error */
	bool (*check)(const struct req *req, struct rep *rep);

	/** RPC handler
	 * @note may be null, meaning "just check the request"
	 * @retval true   send reply to user, set reply to "true" if empty
	 * @retval false  ignore rep, send internal server error message */
	bool (*handle)(const struct req *req, struct rep *rep);
};

#endif
