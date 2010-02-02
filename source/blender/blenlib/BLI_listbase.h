/*
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * 
 * $Id$ 
*/

#ifndef BLI_LISTBASE_H
#define BLI_LISTBASE_H

//#include "DNA_listbase.h"
struct ListBase;
struct LinkData;

#ifdef __cplusplus
extern "C" {
#endif

void addlisttolist(struct ListBase *list1, struct ListBase *list2);
void BLI_insertlink(struct ListBase *listbase, void *vprevlink, void *vnewlink);
void *BLI_findlink(struct ListBase *listbase, int number);
int BLI_findindex(struct ListBase *listbase, void *vlink);
void *BLI_findstring(struct ListBase *listbase, const char *id, int offset);
int BLI_findstringindex(struct ListBase *listbase, const char *id, int offset);
void BLI_freelistN(struct ListBase *listbase);
void BLI_addtail(struct ListBase *listbase, void *vlink);
void BLI_remlink(struct ListBase *listbase, void *vlink);

void BLI_addhead(struct ListBase *listbase, void *vlink);
void BLI_insertlinkbefore(struct ListBase *listbase, void *vnextlink, void *vnewlink);
void BLI_insertlinkafter(struct ListBase *listbase, void *vprevlink, void *vnewlink);
void BLI_sortlist(struct ListBase *listbase, int (*cmp)(void *, void *));
void BLI_freelist(struct ListBase *listbase);
int BLI_countlist(struct ListBase *listbase);
void BLI_freelinkN(struct ListBase *listbase, void *vlink);
void BLI_duplicatelist(struct ListBase *list1, const struct ListBase *list2);

/* create a generic list node containing link to provided data */
struct LinkData *BLI_genericNodeN(void *data);

#ifdef __cplusplus
}
#endif

#endif
