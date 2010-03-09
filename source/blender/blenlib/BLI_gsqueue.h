/*
 * A generic structure queue (a queue for fixed length
 * (generally small) structures.
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
 */

#ifndef BLI_GSQUEUE_H
#define BLI_GSQUEUE_H

typedef struct _GSQueue GSQueue;

	/**
	 * Create a new GSQueue.
	 * 
	 * @param elem_size The size of the structures in the queue.
	 * @retval The new queue
	 */
GSQueue*	BLI_gsqueue_new		(int elem_size);

	/**
	 * Query if the queue is empty
	 */
int			BLI_gsqueue_is_empty(GSQueue *gq);

	/**
	 * Query number elements in the queue
	 */
int			BLI_gsqueue_size(GSQueue *gq);

	/**
	 * Access the item at the head of the queue
	 * without removing it.
	 * 
	 * @param item_r A pointer to an appropriatly
	 * sized structure (the size passed to BLI_gsqueue_new)
	 */
void		BLI_gsqueue_peek	(GSQueue *gq, void *item_r);

	/**
	 * Access the item at the head of the queue
	 * and remove it.
	 * 
	 * @param item_r A pointer to an appropriatly
	 * sized structure (the size passed to BLI_gsqueue_new).
	 * Can be NULL if desired.
	 */
void		BLI_gsqueue_pop		(GSQueue *gq, void *item_r);

	/**
	 * Push an element onto the tail of the queue.
	 * 
	 * @param item A pointer to an appropriatly
	 * sized structure (the size passed to BLI_gsqueue_new).
	 */
void		BLI_gsqueue_push	(GSQueue *gq, void *item);

	/**
	 * Push an element back onto the head of the queue (so
	 * it would be returned from the next call to BLI_gsqueue_pop).
	 * 
	 * @param item A pointer to an appropriatly
	 * sized structure (the size passed to BLI_gsqueue_new).
	 */
void		BLI_gsqueue_pushback	(GSQueue *gq, void *item);

	/**
	 * Free the queue
	 */
void		BLI_gsqueue_free	(GSQueue *gq);

#endif /* BLI_GSQUEUE_H */

