/*
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pforemski@asn.pl>
 * All rights reserved
 */

#ifndef _SH_H_
#define _SH_H_

bool sh_init(struct mod *mod);
bool sh_handle(struct req *req);

extern struct api sh_api;

#endif
