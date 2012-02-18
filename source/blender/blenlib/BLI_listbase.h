/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_LISTBASE_H__
#define __BLI_LISTBASE_H__

/** \file BLI_listbase.h
 *  \ingroup bli
 */

#include "DNA_listBase.h"
//struct ListBase;
//struct LinkData;

#ifdef __cplusplus
extern "C" {
#endif

void BLI_insertlink(struct ListBase *listbase, void *vprevlink, void *vnewlink);
void *BLI_findlink(const struct ListBase *listbase, int number);
int BLI_findindex(const struct ListBase *listbase, void *vlink);
int BLI_findstringindex(const struct ListBase *listbase, const char *id, const int offset);

/* find forwards */
void *BLI_findstring(const struct ListBase *listbase, const char *id, const int offset);
void *BLI_findstring_ptr(const struct ListBase *listbase, const char *id, const int offset);

/* find backwards */
void *BLI_rfindstring(const struct ListBase *listbase, const char *id, const int offset);
void *BLI_rfindstring_ptr(const struct ListBase *listbase, const char *id, const int offset);

void BLI_freelistN(struct ListBase *listbase);
void BLI_addtail(struct ListBase *listbase, void *vlink);
void BLI_remlink(struct ListBase *listbase, void *vlink);
int BLI_remlink_safe(struct ListBase *listbase, void *vlink);

void BLI_addhead(struct ListBase *listbase, void *vlink);
void BLI_insertlinkbefore(struct ListBase *listbase, void *vnextlink, void *vnewlink);
void BLI_insertlinkafter(struct ListBase *listbase, void *vprevlink, void *vnewlink);
void BLI_sortlist(struct ListBase *listbase, int (*cmp)(void *, void *));
void BLI_freelist(struct ListBase *listbase);
int BLI_countlist(const struct ListBase *listbase);
void BLI_freelinkN(struct ListBase *listbase, void *vlink);

void BLI_movelisttolist(struct ListBase *dst, struct ListBase *src);
void BLI_duplicatelist(struct ListBase *dst, const struct ListBase *src);

/* create a generic list node containing link to provided data */
struct LinkData *BLI_genericNodeN(void *data);

#ifdef __cplusplus
}
#endif

#endif
