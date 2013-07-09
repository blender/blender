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

#ifndef __BLI_GSQUEUE_H__
#define __BLI_GSQUEUE_H__

/** \file BLI_gsqueue.h
 *  \ingroup bli
 *  \brief A generic structure queue (a queue for fixed length
 *   (generally small) structures.
 */

typedef struct _GSQueue GSQueue;

GSQueue    *BLI_gsqueue_new(int elem_size);
bool        BLI_gsqueue_is_empty(GSQueue *gq);
int         BLI_gsqueue_size(GSQueue *gq);
void        BLI_gsqueue_peek(GSQueue *gq, void *item_r);
void        BLI_gsqueue_pop(GSQueue *gq, void *item_r);
void        BLI_gsqueue_push(GSQueue *gq, void *item);
void        BLI_gsqueue_pushback(GSQueue *gq, void *item);
void        BLI_gsqueue_free(GSQueue *gq);

#endif /* __BLI_GSQUEUE_H__ */

