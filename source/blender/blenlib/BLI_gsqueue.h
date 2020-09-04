/*
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
 */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _GSQueue GSQueue;

GSQueue *BLI_gsqueue_new(const size_t elem_size);
bool BLI_gsqueue_is_empty(const GSQueue *queue);
size_t BLI_gsqueue_len(const GSQueue *queue);
void BLI_gsqueue_pop(GSQueue *queue, void *r_item);
void BLI_gsqueue_push(GSQueue *queue, const void *item);
void BLI_gsqueue_free(GSQueue *queue);

#ifdef __cplusplus
}
#endif
