/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _GSQueue GSQueue;

GSQueue *BLI_gsqueue_new(size_t elem_size);
/**
 * Returns true if the queue is empty, false otherwise.
 */
bool BLI_gsqueue_is_empty(const GSQueue *queue);
size_t BLI_gsqueue_len(const GSQueue *queue);
/**
 * Retrieves and removes the first element from the queue.
 * The value is copies to \a r_item, which must be at least \a elem_size bytes.
 *
 * Does not reduce amount of allocated memory.
 */
void BLI_gsqueue_pop(GSQueue *queue, void *r_item);
/**
 * Copies the source value onto the end of the queue
 *
 * \note This copies #GSQueue.elem_size bytes from \a item,
 * (the pointer itself is not stored).
 *
 * \param item: source data to be copied to the queue.
 */
void BLI_gsqueue_push(GSQueue *queue, const void *item);
/**
 * Free the queue's data and the queue itself.
 */
void BLI_gsqueue_free(GSQueue *queue);

#ifdef __cplusplus
}
#endif
