/*
 * Copyright (C) 2009 ASN Sp. z o.o.
 * Author: Pawel Foremski <pjf@asn.pl>
 * All rights reserved
 */

#ifndef _REP_H_
#define _REP_H_

void rep_set_bool(struct rep *rep, bool arg);
void rep_set_string(struct rep *rep, const char *arg);
void rep_set_int(struct rep *rep, int arg);
void rep_set_double(struct rep *rep, double arg);
void rep_set_tlist(struct rep *rep, tlist *arg);
void rep_set_thash(struct rep *rep, thash *arg);
void rep_set_error(struct rep *rep, int code, const char *msg, const char *data);

/** Convert struct rep to string representation */
char *rep_tostring(struct rep *rep);

#endif
