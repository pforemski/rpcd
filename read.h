/*
 * Copyright (C) 2009-2010 Pawel Foremski <pawel@foremski.pl>
 * Licensed under GPLv3
 */

#ifndef _READ_H_
#define _READ_H_

/** Read req->args from stdin in JSON-RPC format */
bool readjson(struct req *req);

/** Read req->args from stdin in rfc822 format */
bool read822(struct req *req);

/** Read req->args from stdin in HTTP POST application/json-rpc format
 * @note http://groups.google.com/group/json-rpc/web/json-rpc-over-http */
bool readhttp(struct req *req);

#endif
