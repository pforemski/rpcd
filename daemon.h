/*
 * rpcd - a JSON-RPC bridge
 *
 * Copyright (C) 2009-2010 Pawel Foremski <pawel@foremski.pl>
 *
 * Licensed under GPLv3
 */

#ifndef _DAEMON_H_
#define _DAEMON_H_

#include <libpjf/lib.h>
#include "standard.h"

struct rpcd_opts {
	const char *config_file;    /** config file */
	bool daemonize;             /** if true, go into background */
	const char *name;           /** syslog name */
	const char *pidfile;        /** daemon pidfile */

	enum rpcd_mode {
		RPCD_JSON = 1,
		RPCD_RFC,
		RPCD_HTTP
	} mode;                     /** mode of operation */

	/** Pointer at function reading new request */
	bool (*read)(struct req *req);

	/** Pointer at function writing reply */
	void (*write)(struct req *req);

	/* HTTP options */
	struct rpcd_http_data {
		const char *htdocs;         /** serve static HTTP files from here */
		const char *htpasswd;       /** HTTP passwd file */
	} http;
} O;

#endif
