/*
 * Copyright (C) 2009-2010 Pawel Foremski <pawel@foremski.pl>
 * Licensed under GPLv3
 */

#ifndef _WRITE_H_
#define _WRITE_H_

void writejson(struct req *req);
void write822(struct req *req);
void writehttp(struct req *req);

#endif
