/*
 * generic handling procedures, off-loading module programming
 *
 * Copyright (C) 2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 *
 * All rights reserved
 */

#ifndef _GENERIC_H_
#define _GENERIC_H_

bool generic_init(struct mod *mod);
bool generic_deinit(struct mod *mod);
bool generic_check(struct req *req, mmatic *mm);
bool generic_handle(struct req *req, mmatic *mm);

bool generic_fw(struct req *req);

#endif /* _GENERIC_H_ */
