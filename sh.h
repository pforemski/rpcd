/*
 * Copyright (C) 2009 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#ifndef _SH_H_
#define _SH_H_

bool sh_check(const struct req *req, struct rep *rep);
bool sh_handle(const struct req *req, struct rep *rep);

#endif
