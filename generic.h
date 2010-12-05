/*
 * generic handling procedures, off-loading module programming
 *
 * Copyright (C) 2009-2010 Pawel Foremski <pawel@foremski.pl>
 *
 * Licensed under GPLv3
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
