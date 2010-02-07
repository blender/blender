/* util.c
 *
 * various string, file, list operations.
 *
 *
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
 */

#include <string.h>
#include <stdlib.h>


#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_listbase.h"


/* implementation */

/* Ripped this from blender.c */
void addlisttolist(ListBase *list1, ListBase *list2)
{
	if (list2->first==0) return;

	if (list1->first==0) {
		list1->first= list2->first;
		list1->last= list2->last;
	}
	else {
		((Link *)list1->last)->next= list2->first;
		((Link *)list2->first)->prev= list1->last;
		list1->last= list2->last;
	}
	list2->first= list2->last= 0;
}

void BLI_addhead(ListBase *listbase, void *vlink)
{
	Link *link= vlink;

	if (link == NULL) return;
	if (listbase == NULL) return;

	link->next = listbase->first;
	link->prev = NULL;

	if (listbase->first) ((Link *)listbase->first)->prev = link;
	if (listbase->last == NULL) listbase->last = link;
	listbase->first = link;
}


void BLI_addtail(ListBase *listbase, void *vlink)
{
	Link *link= vlink;

	if (link == NULL) return;
	if (listbase == NULL) return;

	link->next = NULL;
	link->prev = listbase->last;

	if (listbase->last) ((Link *)listbase->last)->next = link;
	if (listbase->first == 0) listbase->first = link;
	listbase->last = link;
}


void BLI_remlink(ListBase *listbase, void *vlink)
{
	Link *link= vlink;

	if (link == NULL) return;
	if (listbase == NULL) return;

	if (link->next) link->next->prev = link->prev;
	if (link->prev) link->prev->next = link->next;

	if (listbase->last == link) listbase->last = link->prev;
	if (listbase->first == link) listbase->first = link->next;
}


void BLI_freelinkN(ListBase *listbase, void *vlink)
{
	Link *link= vlink;

	if (link == NULL) return;
	if (listbase == NULL) return;

	BLI_remlink(listbase,link);
	MEM_freeN(link);
}


void BLI_insertlink(ListBase *listbase, void *vprevlink, void *vnewlink)
{
	Link *prevlink= vprevlink;
	Link *newlink= vnewlink;

	/* newlink comes after prevlink */
	if (newlink == NULL) return;
	if (listbase == NULL) return;
	
	/* empty list */
	if (listbase->first == NULL) { 
		
		listbase->first= newlink;
		listbase->last= newlink;
		return;
	}
	
	/* insert before first element */
	if (prevlink == NULL) {	
		newlink->next= listbase->first;
		newlink->prev= 0;
		newlink->next->prev= newlink;
		listbase->first= newlink;
		return;
	}

	/* at end of list */
	if (listbase->last== prevlink) 
		listbase->last = newlink;

	newlink->next= prevlink->next;
	prevlink->next= newlink;
	if (newlink->next) newlink->next->prev= newlink;
	newlink->prev= prevlink;
}

/* This uses insertion sort, so NOT ok for large list */
void BLI_sortlist(ListBase *listbase, int (*cmp)(void *, void *))
{
	Link *current = NULL;
	Link *previous = NULL;
	Link *next = NULL;
	
	if (cmp == NULL) return;
	if (listbase == NULL) return;

	if (listbase->first != listbase->last)
	{
		for( previous = listbase->first, current = previous->next; current; current = next )
		{
			next = current->next;
			previous = current->prev;
			
			BLI_remlink(listbase, current);
			
			while(previous && cmp(previous, current) == 1)
			{
				previous = previous->prev;
			}
			
			BLI_insertlinkafter(listbase, previous, current);
		}
	}
}

void BLI_insertlinkafter(ListBase *listbase, void *vprevlink, void *vnewlink)
{
	Link *prevlink= vprevlink;
	Link *newlink= vnewlink;

	/* newlink before nextlink */
	if (newlink == NULL) return;
	if (listbase == NULL) return;

	/* empty list */
	if (listbase->first == NULL) { 
		listbase->first= newlink;
		listbase->last= newlink;
		return;
	}
	
	/* insert at head of list */
	if (prevlink == NULL) {	
		newlink->prev = NULL;
		newlink->next = listbase->first;
		((Link *)listbase->first)->prev = newlink;
		listbase->first = newlink;
		return;
	}

	/* at end of list */
	if (listbase->last == prevlink) 
		listbase->last = newlink;

	newlink->next = prevlink->next;
	newlink->prev = prevlink;
	prevlink->next = newlink;
	if (newlink->next) newlink->next->prev = newlink;
}

void BLI_insertlinkbefore(ListBase *listbase, void *vnextlink, void *vnewlink)
{
	Link *nextlink= vnextlink;
	Link *newlink= vnewlink;

	/* newlink before nextlink */
	if (newlink == NULL) return;
	if (listbase == NULL) return;

	/* empty list */
	if (listbase->first == NULL) { 
		listbase->first= newlink;
		listbase->last= newlink;
		return;
	}
	
	/* insert at end of list */
	if (nextlink == NULL) {	
		newlink->prev= listbase->last;
		newlink->next= 0;
		((Link *)listbase->last)->next= newlink;
		listbase->last= newlink;
		return;
	}

	/* at beginning of list */
	if (listbase->first== nextlink) 
		listbase->first = newlink;

	newlink->next= nextlink;
	newlink->prev= nextlink->prev;
	nextlink->prev= newlink;
	if (newlink->prev) newlink->prev->next= newlink;
}


void BLI_freelist(ListBase *listbase)
{
	Link *link, *next;

	if (listbase == NULL) 
		return;
	
	link= listbase->first;
	while (link) {
		next= link->next;
		free(link);
		link= next;
	}
	
	listbase->first= NULL;
	listbase->last= NULL;
}

void BLI_freelistN(ListBase *listbase)
{
	Link *link, *next;

	if (listbase == NULL) return;
	
	link= listbase->first;
	while (link) {
		next= link->next;
		MEM_freeN(link);
		link= next;
	}
	
	listbase->first= NULL;
	listbase->last= NULL;
}


int BLI_countlist(ListBase *listbase)
{
	Link *link;
	int count = 0;
	
	if (listbase) {
		link = listbase->first;
		while (link) {
			count++;
			link= link->next;
		}
	}
	return count;
}

void *BLI_findlink(ListBase *listbase, int number)
{
	Link *link = NULL;

	if (number >= 0) {
		link = listbase->first;
		while (link != NULL && number != 0) {
			number--;
			link = link->next;
		}
	}

	return link;
}

int BLI_findindex(ListBase *listbase, void *vlink)
{
	Link *link= NULL;
	int number= 0;
	
	if (listbase == NULL) return -1;
	if (vlink == NULL) return -1;
	
	link= listbase->first;
	while (link) {
		if (link == vlink)
			return number;
		
		number++;
		link= link->next;
	}
	
	return -1;
}

void *BLI_findstring(ListBase *listbase, const char *id, int offset)
{
	Link *link= NULL;
	const char *id_iter;

	if (listbase == NULL) return NULL;

	link= listbase->first;
	while (link) {
		id_iter= ((const char *)link) + offset;

		if(id[0] == id_iter[0] && strcmp(id, id_iter)==0)
			return link;

		link= link->next;
	}

	return NULL;
}

int BLI_findstringindex(ListBase *listbase, const char *id, int offset)
{
	Link *link= NULL;
	const char *id_iter;
	int i= 0;

	if (listbase == NULL) return -1;

	link= listbase->first;
	while (link) {
		id_iter= ((const char *)link) + offset;

		if(id[0] == id_iter[0] && strcmp(id, id_iter)==0)
			return i;
		i++;
		link= link->next;
	}

	return -1;
}

void BLI_duplicatelist(ListBase *list1, const ListBase *list2)
{
	struct Link *link1, *link2;
	
	/* in this order, to ensure it works if list1 == list2 */
	link2= list2->first;
	list1->first= list1->last= 0;
	
	while(link2) {
		link1= MEM_dupallocN(link2);
		BLI_addtail(list1, link1);
		
		link2= link2->next;
	}	
}

/* create a generic list node containing link to provided data */
LinkData *BLI_genericNodeN (void *data)
{
	LinkData *ld;
	
	if (data == NULL)
		return NULL;
		
	/* create new link, and make it hold the given data */
	ld= MEM_callocN(sizeof(LinkData), "BLI_genericNodeN()");
	ld->data= data;
	
	return ld;
} 
