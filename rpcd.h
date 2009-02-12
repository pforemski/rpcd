#ifndef _FCWEB_H_
#define _FCWEB_H_

/** Global data root */
struct rpcd {
	const char *startdir;    /** starting $PWD */
	const char *pidfile;     /** path to store PID in */
	bool daemonize;          /** start in background? */
	const char *logname;     /** name in syslog */

	thash *modules;          /** char *command -> struct mod *module; TODO: think of always-load-me ones */
	thash *env;              /** global environment skeleton */
	thash *rrules;           /** char*s -> struct rrule: the global regexp firewall */
	tlist *checks;           /** list of bool (*)(const struct *req): checks to issue on each request */
} R;

/** A JSON-RPC request representation */
struct req {
	const char *method;      /** called command */
	tlist *params;           /** list of char *: positional parameters - ie. without keys */
	thash *args;             /** char *key -> char *val: named parameters (for JSON-RPC 1.1WD+) - arguments (sorry for confusing naming :) */
	const char *id;          /** optional ID, if present */

	thash *env;              /** request environment, initially cloned from R.env */
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
	const char *path;        /** module file path */
	enum modtype { C, JS, SH } type; /** implemented in? */

	thash *rrules;           /** regexp rules to check only for this command */

	/** Per-module custom firewall */
	bool (*check)(const struct req *req);

	/** RPC handler */
	bool (*handle)(const struct req *req);
};

#endif
