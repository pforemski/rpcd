/*
 * Copyright (C) 2009 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#ifndef _READ_H_
#define _READ_H_

enum readstatus { OK, SKIP, EXIT };

/** Read req->args from stdin in rfc822 format */
enum readstatus readcli(struct req *req, mmatic *mm);

#endif
