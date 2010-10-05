/*
 * generic handling procedures, off-loading module programming
 *
 * Copyright (C) 2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pforemski@asn.pl>
 *
 * All rights reserved
 */

#ifndef _GENERIC_H_
#define _GENERIC_H_

bool generic_init(struct mod *mod);
bool generic_deinit(struct mod *mod);
bool generic_handle(struct req *req);
bool generic_fw(struct req *req, struct fw *fw);

extern struct api generic_api;
extern struct api sh_api;

#endif
