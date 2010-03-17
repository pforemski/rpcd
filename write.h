/*
 * Copyright (C) 2009-2010 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#ifndef _WRITE_H_
#define _WRITE_H_

void writejson(struct req *req);
void write822(struct req *req);
void writehttp(struct req *req);

#endif
