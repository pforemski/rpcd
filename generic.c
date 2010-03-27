/*
 * generic handling procedures, off-loading module programming
 *
 * Copyright (C) 2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 *
 * All rights reserved
 */

#include "common.h"

bool generic_init(struct mod *mod)
{
	dbg(1, "%s: initialized\n", mod->path);
	return true;
}

bool generic_deinit(struct mod *mod)
{
	return true;
}

bool generic_check(struct req *req, mmatic *mm)
{
	return true;
}

bool generic_handle(struct req *req, mmatic *mm)
{
	return true;
}

/** Run parameter validation against simple "firewall"
 *
 * @note we dont get req->mod->fw automatically because we want to be more flexible
 */
bool generic_fw(struct req *req, struct fw *fw)
{
	if (!fw)
		return true;

	if (!ut_is_thash(req->query))
		return err(JSON_RPC_INVALID_PARAMS, "Expected parameters in a hash object", NULL);

	ut *param;
	for (; fw->name; fw++) {
		dbg(5, "%s: checking\n", fw->name);

		param = uth_get(req->query, fw->name);
		if (!param) {
			if (fw->required)
				return err(JSON_RPC_INVALID_PARAMS, "Parameter required", fw->name);
			else
				continue;
		}

		if (ut_type(param) != fw->type) {
			dbg(5, "%s: converting\n", fw->name);
			switch (fw->type) {
				case T_PTR:    uth_set_ptr(req->query,    fw->name, ut_ptr(param));    break;
				case T_BOOL:   uth_set_bool(req->query,   fw->name, ut_bool(param));   break;
				case T_INT:    uth_set_int(req->query,    fw->name, ut_int(param));    break;
				case T_DOUBLE: uth_set_double(req->query, fw->name, ut_double(param)); break;
				case T_STRING: uth_set_xstr(req->query,   fw->name, ut_xstr(param));   break;
				case T_LIST:   uth_set_tlist(req->query , fw->name, ut_tlist(param));  break;
				case T_HASH:   uth_set_thash(req->query,  fw->name, ut_thash(param));  break;
				case T_NULL:
				case T_ERR:
					dbg(0, "%s: %s_fw: %s: invalid type\n", req->mod->path, req->mod->name, fw->name);
					return errcode(JSON_RPC_INTERNAL_ERROR);
					break;
			}

			/* get converted value */
			param = uth_get(req->query, fw->name);
			asnsert(param);
		}

		if (fw->regexp && fw->regexp[0]) {
			dbg(5, "%s: checking regexp\n", fw->name);

			if (!asn_match(fw->regexp, ut_char(param)))
				return err(JSON_RPC_INVALID_PARAMS, "Invalid value", fw->name);
		}
	}

	return true;
}

/* for Vim autocompletion:
 * vim: path=.,/usr/include,/usr/local/include,~/local/include
 */
