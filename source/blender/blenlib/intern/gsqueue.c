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

/** \file blender/blenlib/intern/gsqueue.c
 *  \ingroup bli
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_gsqueue.h"
#include "BLI_strict_flags.h"

typedef struct _GSQueueElem GSQueueElem;
struct _GSQueueElem {
	GSQueueElem *next;
};

struct _GSQueue {
	GSQueueElem *head;
	GSQueueElem *tail;
	int elem_size;
};

/**
 * Create a new GSQueue.
 *
 * \param elem_size The size of the structures in the queue.
 * \retval The new queue
 */
GSQueue *BLI_gsqueue_new(int elem_size)
{
	GSQueue *gq = MEM_mallocN(sizeof(*gq), "gqueue_new");
	gq->head = gq->tail = NULL;
	gq->elem_size = elem_size;
	
	return gq;
}

/**
 * Query if the queue is empty
 */
bool BLI_gsqueue_is_empty(GSQueue *gq)
{
	return (gq->head == NULL);
}

/**
 * Query number elements in the queue
 */
int BLI_gsqueue_size(GSQueue *gq)
{ 
	GSQueueElem *elem;
	int size = 0;

	for (elem = gq->head; elem; elem = elem->next)
		size++;
	
	return size;
}

/**
 * Access the item at the head of the queue
 * without removing it.
 *
 * \param item_r A pointer to an appropriately
 * sized structure (the size passed to BLI_gsqueue_new)
 */
void BLI_gsqueue_peek(GSQueue *gq, void *item_r)
{
	memcpy(item_r, &gq->head[1], (size_t)gq->elem_size);
}

/**
 * Access the item at the head of the queue
 * and remove it.
 *
 * \param item_r A pointer to an appropriately
 * sized structure (the size passed to BLI_gsqueue_new).
 * Can be NULL if desired.
 */
void BLI_gsqueue_pop(GSQueue *gq, void *item_r)
{
	GSQueueElem *elem = gq->head;
	if (elem == gq->tail) {
		gq->head = gq->tail = NULL;
	}
	else {
		gq->head = gq->head->next;
	}
	
	if (item_r) memcpy(item_r, &elem[1], (size_t)gq->elem_size);
	MEM_freeN(elem);
}

/**
 * Push an element onto the tail of the queue.
 *
 * \param item A pointer to an appropriately
 * sized structure (the size passed to BLI_gsqueue_new).
 */
void BLI_gsqueue_push(GSQueue *gq, void *item)
{
	GSQueueElem *elem;
	
	/* compare: prevent events added double in row */
	if (!BLI_gsqueue_is_empty(gq)) {
		if (0 == memcmp(item, &gq->head[1], (size_t)gq->elem_size))
			return;
	}
	elem = MEM_mallocN(sizeof(*elem) + (size_t)gq->elem_size, "gqueue_push");
	memcpy(&elem[1], item, (size_t)gq->elem_size);
	elem->next = NULL;
	
	if (BLI_gsqueue_is_empty(gq)) {
		gq->tail = gq->head = elem;
	}
	else {
		gq->tail = gq->tail->next = elem;
	}
}

/**
 * Push an element back onto the head of the queue (so
 * it would be returned from the next call to BLI_gsqueue_pop).
 *
 * \param item A pointer to an appropriately
 * sized structure (the size passed to BLI_gsqueue_new).
 */
void BLI_gsqueue_pushback(GSQueue *gq, void *item)
{
	GSQueueElem *elem = MEM_mallocN(sizeof(*elem) + (size_t)gq->elem_size, "gqueue_push");
	memcpy(&elem[1], item, (size_t)gq->elem_size);
	elem->next = gq->head;

	if (BLI_gsqueue_is_empty(gq)) {
		gq->head = gq->tail = elem;
	}
	else {
		gq->head = elem;
	}
}

/**
 * Free the queue
 */
void BLI_gsqueue_free(GSQueue *gq)
{
	while (gq->head) {
		BLI_gsqueue_pop(gq, NULL);
	}
	MEM_freeN(gq);
}
