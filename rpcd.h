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

/***************************************************************************************************/

#define RPCD_VER "0.2"
#define RPCD_DEFAULT_CONFIGFILE "/etc/rpcd.conf"
#define RPCD_DEFAULT_PIDFILE "/var/run/rpcd.pid"

/***************************************************************************************************/

struct rpcd;                           /** Global rpcd instance representation */
struct svc;                            /** JSON-RPC "service" */
struct dir;                            /** Directory holding modules */
struct mod;                            /** JSON-RPC "method" */
struct req;                            /** Representation of request, including the reply */
struct api;                            /** Links to functions implementing given module */
struct fw;                             /** Represents one rule in module "parameter firewall" */

/***************************************************************************************************/

/** Initialize rpcd
 * @param config_file  rpcd configuration file:
 *                     if its NULL, RPCD_DEFAULT_CONFIGFILE is used as default
 *                     if the string is empty, rpcd is started with no configuration
 *                     if the string starts with "/" or "./" its interpreted as file path
 *                     otherwise passed string is interpreted as config for service named "rpcd"
 * @retval NULL        something failed - see debug messages for info
 * @return             rpcd handle, which is ready */
struct rpcd *rpcd_init(const char *config_file);

/** Deinitialize rpcd, freeing memory
 * @param rpcd         rpcd handle
 * @note make sure not to use any memory taken from rpcd after calling rpcd_deinit() */
void rpcd_deinit(struct rpcd *rpcd);

/** Load given directory of modules into default service */
bool rpcd_dir_load(struct rpcd *rpcd, const char *path);

/** Make a request
 * @param rpcd         rpcd handle
 * @param method       name of the method to call
 * @param params       unitype object (see libasn) with parameters, may be NULL
 * @return unitype object with reply
 * @note remember to call rpcd_reqfree() after you're done with returned object */
ut *rpcd_request(struct rpcd *rpcd, const char *method, ut *params);

/** Free memory occupied by results of an rpcd request
 * @param reply        object returned by rpcd_request() */
void rpcd_reqfree(ut *reply);

#endif
