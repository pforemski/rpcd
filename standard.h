/*
 * Based on http://groups.google.com/group/json-rpc/web/json-rpc-2-0
 *
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#ifndef _STANDARD_H_
#define _STANDARD_H_

#define JSON_RPC_VERSION "2.0"

/* XXX: see also rep.c/rep_set_error() */
enum json_errcode {
	JSON_RPC_PARSE_ERROR     = -32700,
	JSON_RPC_INVALID_REQUEST = -32600,
	JSON_RPC_NOT_FOUND       = -32601,
	JSON_RPC_INVALID_PARAMS  = -32602,
	JSON_RPC_INTERNAL_ERROR  = -32603,

	JSON_RPC_ACCESS_DENIED   = -32099,
	JSON_RPC_OUT_PARSE_ERROR = -32098,
	JSON_RPC_INVALID_INPUT   = -32097,
	JSON_RPC_NO_OUTPUT       = -32096,
	JSON_RPC_HTTP_OPTIONS    = -32095,
	JSON_RPC_HTTP_GET        = -32094,
	JSON_RPC_HTTP_NOT_FOUND  = -32093,
};

enum http_type {
	POST,
	GET,
	OPTIONS
};

#endif
