/*
 * Copyright (C) 2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#ifndef _AUTH_H_
#define _AUTH_H_

struct user *authinternal(struct req *req);
struct user *authsystem(struct req *req);

#endif
