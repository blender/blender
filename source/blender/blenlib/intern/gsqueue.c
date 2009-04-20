/**
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
 */
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_gsqueue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _GSQueueElem GSQueueElem;
struct _GSQueueElem {
	GSQueueElem *next;
};

struct _GSQueue {
	GSQueueElem	*head;
	GSQueueElem	*tail;
	int			elem_size;
};

GSQueue *BLI_gsqueue_new(int elem_size)
{
	GSQueue *gq= MEM_mallocN(sizeof(*gq), "gqueue_new");
	gq->head= gq->tail= NULL;
	gq->elem_size= elem_size;
	
	return gq;
}

int BLI_gsqueue_is_empty(GSQueue *gq)
{
	return (gq->head==NULL);
}

void BLI_gsqueue_peek(GSQueue *gq, void *item_r)
{
	memcpy(item_r, &gq->head[1], gq->elem_size);
}
void BLI_gsqueue_pop(GSQueue *gq, void *item_r)
{
	GSQueueElem *elem= gq->head;
	if (elem==gq->tail) {
		gq->head= gq->tail= NULL;
	} else {
		gq->head= gq->head->next;
	}
	
	if (item_r) memcpy(item_r, &elem[1], gq->elem_size);
	MEM_freeN(elem);
}
void BLI_gsqueue_push(GSQueue *gq, void *item)
{
	GSQueueElem *elem;
	
	/* compare: prevent events added double in row */
	if (!BLI_gsqueue_is_empty(gq)) {
		if(0==memcmp(item, &gq->head[1], gq->elem_size))
			return;
	}
	elem= MEM_mallocN(sizeof(*elem)+gq->elem_size, "gqueue_push");
	memcpy(&elem[1], item, gq->elem_size);
	elem->next= NULL;
	
	if (BLI_gsqueue_is_empty(gq)) {
		gq->tail= gq->head= elem;
	} else {
		gq->tail= gq->tail->next= elem;
	}
}
void BLI_gsqueue_pushback(GSQueue *gq, void *item)
{
	GSQueueElem *elem= MEM_mallocN(sizeof(*elem)+gq->elem_size, "gqueue_push");
	memcpy(&elem[1], item, gq->elem_size);
	elem->next= gq->head;

	if (BLI_gsqueue_is_empty(gq)) {
		gq->head= gq->tail= elem;
	} else {
		gq->head= elem;
	}
}

void BLI_gsqueue_free(GSQueue *gq)
{
	while (gq->head) {
		BLI_gsqueue_pop(gq, NULL);
	}
	MEM_freeN(gq);
}


