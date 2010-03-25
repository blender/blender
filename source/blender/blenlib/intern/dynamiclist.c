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
 * 
 */

#include "MEM_guardedalloc.h"


#include "BLI_listbase.h"
#include "BLI_dynamiclist.h"

#define PAGE_SIZE 4

/*=====================================================================================*/
/* Methods for access array (realloc) */
/*=====================================================================================*/

/* remove item with index */
static void rem_array_item(struct DynamicArray *da, unsigned int index)
{
	da->items[index]=NULL;
	da->count--;
	if(index==da->last_item_index){
		while((!da->items[da->last_item_index]) && (da->last_item_index>0)){
			da->last_item_index--;
		}
	}
}

/* add array (if needed, then realloc) */
static void add_array_item(struct DynamicArray *da, void *item, unsigned int index)
{
	/* realloc of access array */
	if(da->max_item_index < index){
		unsigned int i, max = da->max_item_index;
		void **nitems;

		do {
			da->max_item_index += PAGE_SIZE;	/* OS can allocate only PAGE_SIZE Bytes */
		} while(da->max_item_index<=index);

		nitems = (void**)MEM_mallocN(sizeof(void*)*(da->max_item_index+1), "dlist access array");
		for(i=0;i<=max;i++)
			nitems[i] = da->items[i];

		/* set rest pointers to the NULL */
		for(i=max+1; i<=da->max_item_index; i++)
			nitems[i]=NULL;

		MEM_freeN(da->items);		/* free old access array */
		da->items = nitems;
	}

	da->items[index] = item;
	da->count++;
	if(index > da->last_item_index) da->last_item_index = index;
}

/* free access array */
static void destroy_array(DynamicArray *da)
{
	da->count=0;
	da->last_item_index=0;
	da->max_item_index=0;
	MEM_freeN(da->items);
	da->items = NULL;
}

/* initialize dynamic array */
static void init_array(DynamicArray *da)
{
	unsigned int i;

	da->count=0;
	da->last_item_index=0;
	da->max_item_index = PAGE_SIZE-1;
	da->items = (void*)MEM_mallocN(sizeof(void*)*(da->max_item_index+1), "dlist access array");
	for(i=0; i<=da->max_item_index; i++) da->items[i]=NULL;
}

/* reinitialize dynamic array */
static void reinit_array(DynamicArray *da)
{
	destroy_array(da);
	init_array(da);
}

/*=====================================================================================*/
/* Methods for two way dynamic list with access array */
/*=====================================================================================*/

/* create new two way dynamic list with access array from two way dynamic list
 * it doesn't copy any items to new array or something like this It is strongly
 * recomended to use BLI_dlist_ methods for adding/removing items from dynamic list
 * unless you can end with inconsistence system !!! */
DynamicList *BLI_dlist_from_listbase(ListBase *lb)
{
	DynamicList *dlist;
	Link *item;
	int i=0, count;
	
	if(!lb) return NULL;
	
	count = BLI_countlist(lb);

	dlist = MEM_mallocN(sizeof(DynamicList), "temp dynamic list");
	/* ListBase stuff */
	dlist->lb.first = lb->first;
	dlist->lb.last = lb->last;
	/* access array stuff */
	dlist->da.count=count;
	dlist->da.max_item_index = count-1;
	dlist->da.last_item_index = count -1;
	dlist->da.items = (void*)MEM_mallocN(sizeof(void*)*count, "temp dlist access array");

	item = (Link*)lb->first;
	while(item){
		dlist->da.items[i] = (void*)item;
		item = item->next;
		i++;
	}

	/* to prevent you of using original ListBase :-) */
	lb->first = lb->last = NULL;

	return dlist;
}

/* take out ListBase from DynamicList and destroy all temporary structures of DynamicList */
ListBase *BLI_listbase_from_dlist(DynamicList *dlist, ListBase *lb)
{
	if(!dlist) return NULL;

	if(!lb) lb = (ListBase*)MEM_mallocN(sizeof(ListBase), "ListBase");
	
	lb->first = dlist->lb.first;
	lb->last = dlist->lb.last;

	/* free all items of access array */
	MEM_freeN(dlist->da.items);
	/* free DynamicList*/
	MEM_freeN(dlist);

	return lb;
}

/* return pointer at item from th dynamic list with access array */
void *BLI_dlist_find_link(DynamicList *dlist, unsigned int index)
{
	if(!dlist || !dlist->da.items) return NULL;

	if((index <= dlist->da.last_item_index) && (index >= 0) && (dlist->da.count>0)){
			  return dlist->da.items[index];
	}
	else {
		return NULL;
	}
}

/* return count of items in the dynamic list with access array */
unsigned int BLI_count_items(DynamicList *dlist)
{
	if(!dlist) return 0;

	return dlist->da.count;
}

/* free item from the dynamic list with access array */
void BLI_dlist_free_item(DynamicList *dlist, unsigned int index)
{
	if(!dlist || !dlist->da.items) return;
	
	if((index <= dlist->da.last_item_index) && (dlist->da.items[index])){
		BLI_freelinkN(&(dlist->lb), dlist->da.items[index]);
		rem_array_item(&(dlist->da), index);
	}
}

/* remove item from the dynamic list with access array */
void BLI_dlist_rem_item(DynamicList *dlist, unsigned int index)
{
	if(!dlist || !dlist->da.items) return;
	
	if((index <= dlist->da.last_item_index) && (dlist->da.items[index])){
		BLI_remlink(&(dlist->lb), dlist->da.items[index]);
		rem_array_item(&(dlist->da), index);
	}
}

/* add item to the dynamic list with access array (index) */
void* BLI_dlist_add_item_index(DynamicList *dlist, void *item, unsigned int index)
{
	if(!dlist || !dlist->da.items) return NULL;

	if((index <= dlist->da.max_item_index) && (dlist->da.items[index])) {
		/* you can't place item at used index */
		return NULL;
	}
	else {
		add_array_item(&(dlist->da), item, index);
		BLI_addtail(&(dlist->lb), item);
		return item;
	}
}

/* destroy dynamic list with access array */
void BLI_dlist_destroy(DynamicList *dlist)
{
	if(!dlist) return;

	BLI_freelistN(&(dlist->lb));
	destroy_array(&(dlist->da));
}

/* initialize dynamic list with access array */
void BLI_dlist_init(DynamicList *dlist)
{
	if(!dlist) return;

	dlist->lb.first = NULL;
	dlist->lb.last = NULL;

	init_array(&(dlist->da));
}

/* reinitialize dynamic list with acces array */
void BLI_dlist_reinit(DynamicList *dlist)
{
	if(!dlist) return;
	
	BLI_freelistN(&(dlist->lb));
	reinit_array(&(dlist->da));
}

/*=====================================================================================*/
