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
 * Support for linked lists.
 */

/** \file blender/blenlib/intern/BLI_linklist.c
 *  \ingroup bli
 */


#include "MEM_guardedalloc.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"

int BLI_linklist_length(LinkNode *list)
{
	if (0) {
		return list?(1+BLI_linklist_length(list->next)):0;
	} else {
		int len;

		for (len=0; list; list= list->next)
			len++;
	
		return len;
	}
}

int BLI_linklist_index(LinkNode *list, void *ptr)
{
	int index;
	
	for (index = 0; list; list= list->next, index++)
		if (list->link == ptr)
			return index;
	
	return -1;
}

LinkNode *BLI_linklist_find(LinkNode *list, int index)
{
	int i;
	
	for (i = 0; list; list= list->next, i++)
		if (i == index)
			return list;

	return NULL;
}

void BLI_linklist_reverse(LinkNode **listp)
{
	LinkNode *rhead= NULL, *cur= *listp;
	
	while (cur) {
		LinkNode *next= cur->next;
		
		cur->next= rhead;
		rhead= cur;
		
		cur= next;
	}
	
	*listp= rhead;
}

void BLI_linklist_prepend(LinkNode **listp, void *ptr)
{
	LinkNode *nlink= MEM_mallocN(sizeof(*nlink), "nlink");
	nlink->link= ptr;
	
	nlink->next= *listp;
	*listp= nlink;
}

void BLI_linklist_append(LinkNode **listp, void *ptr)
{
	LinkNode *nlink= MEM_mallocN(sizeof(*nlink), "nlink");
	LinkNode *node = *listp;
	
	nlink->link = ptr;
	nlink->next = NULL;
	
	if (node == NULL) {
		*listp = nlink;
	}
	else {
		while (node->next != NULL) {
			node = node->next;   
		}
		node->next = nlink;
	}
}

void BLI_linklist_prepend_arena(LinkNode **listp, void *ptr, MemArena *ma)
{
	LinkNode *nlink= BLI_memarena_alloc(ma, sizeof(*nlink));
	nlink->link= ptr;
	
	nlink->next= *listp;
	*listp= nlink;
}

void BLI_linklist_insert_after(LinkNode **listp, void *ptr)
{
	LinkNode *nlink= MEM_mallocN(sizeof(*nlink), "nlink");
	LinkNode *node = *listp;

	nlink->link = ptr;

	if(node) {
		nlink->next = node->next;
		node->next = nlink;
	}
	else {
		nlink->next = NULL;
		*listp = nlink;
	}
}

void BLI_linklist_free(LinkNode *list, LinkNodeFreeFP freefunc)
{
	while (list) {
		LinkNode *next= list->next;
		
		if (freefunc)
			freefunc(list->link);
		MEM_freeN(list);
		
		list= next;
	}
}

void BLI_linklist_apply(LinkNode *list, LinkNodeApplyFP applyfunc, void *userdata)
{
	for (; list; list= list->next)
		applyfunc(list->link, userdata);
}
