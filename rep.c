/*
 * Copyright (C) 2009 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#include "rpcd.h"

void rep_set_bool(struct rep *rep, bool arg)
{
	rep->type = T_BOOL;
	rep->data.as_bool = arg;
}

void rep_set_string(struct rep *rep, const char *arg)
{
	rep->type = T_STRING;
	rep->data.as_string = arg;
}

void rep_set_int(struct rep *rep, int arg)
{
	rep->type = T_INT;
	rep->data.as_int = arg;
}

void rep_set_double(struct rep *rep, double arg)
{
	rep->type = T_DOUBLE;
	rep->data.as_double = arg;
}

void rep_set_tlist(struct rep *rep, tlist *arg)
{
	rep->type = T_LIST;
	rep->data.as_tlist = arg;
}

void rep_set_thash(struct rep *rep, thash *arg)
{
	rep->type = T_HASH;
	rep->data.as_thash = arg;
}

void rep_set_error(struct rep *rep, int code, const char *msg, const char *data)
{
	enum json_errcode jcode = code;

	/* XXX: after conversion to enum so gcc catches missing ones */
	if (!msg) switch (jcode) {
		case JSON_RPC_PARSE_ERROR:
			msg = "Parse error"; break;
		case JSON_RPC_INVALID_REQUEST:
			msg = "Invalid Request"; break;
		case JSON_RPC_NOT_FOUND:
			msg = "Method not found"; break;
		case JSON_RPC_INVALID_PARAMS:
			msg = "Invalid params"; break;
		case JSON_RPC_INTERNAL_ERROR:
			msg = "Internal error"; break;
		case JSON_RPC_ACCESS_DENIED:
			msg = "Access denied"; break;
		case JSON_RPC_OUT_PARSE_ERROR:
			msg = "Output parse error"; break;
		case JSON_RPC_INVALID_INPUT:
			msg = "Invalid input"; break;
	}

	rep->type = T_ERROR;
	rep->data.as_err = mmatic_alloc(sizeof(*(rep->data.as_err)), rep->req->mm);
	rep->data.as_err->code = code;
	rep->data.as_err->msg  = msg;
	rep->data.as_err->data = data;
}

// FIXME: wrong! ;)
char *rep_tostring(struct rep *rep)
{
	mmatic *mm = rep->req->mm;
	xstr *ret = MMXSTR_CREATE("");
	char *k, *v;

	if (rep->type == _T_NONE) {
		xstr_set(ret, "# no output :)");
	}
	else if (rep->type == T_STRING) {
		xstr_set(ret, "# string\n");
		xstr_append(ret, rep->data.as_string);
	}
	else if (rep->type == T_HASH) {
		xstr_set(ret, "# hash\n");

		// BUG: assumes string->string
		THASH_ITER_LOOP(rep->data.as_thash, k, v)
			xstr_append(ret, mmprintf("%s: %s\n", k, v));
	}
	else if (rep->type == T_ERROR) {
		xstr_set(ret, mmprintf(
			"# error\n"
			"code: %d\n"
			"msg: %s\n"
			"data: %s\n",
			rep->data.as_err->code,
			asn_trim(rep->data.as_err->msg),
			asn_trim(rep->data.as_err->data)
		));
	}
	else {
		xstr_set(ret, "output not supported yet :)\n");
	}

	return xstr_string(ret);
}
