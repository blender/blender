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

#ifndef __BLI_LISTBASE_H__
#define __BLI_LISTBASE_H__

/** \file BLI_listbase.h
 *  \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"
#include "DNA_listBase.h"
//struct ListBase;
//struct LinkData;

#ifdef __cplusplus
extern "C" {
#endif

int BLI_findindex(const struct ListBase *listbase, const void *vlink) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLI_findstringindex(const struct ListBase *listbase, const char *id, const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/* find forwards */
void *BLI_findlink(const struct ListBase *listbase, int number) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_findstring(const struct ListBase *listbase, const char *id, const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_findstring_ptr(const struct ListBase *listbase, const char *id, const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_findptr(const struct ListBase *listbase, const void *ptr, const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/* find backwards */
void *BLI_rfindlink(const struct ListBase *listbase, int number) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_rfindstring(const struct ListBase *listbase, const char *id, const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_rfindstring_ptr(const struct ListBase *listbase, const char *id, const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_rfindptr(const struct ListBase *listbase, const void *ptr, const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

void BLI_freelistN(struct ListBase *listbase) ATTR_NONNULL(1);
void BLI_addtail(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
void BLI_remlink(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
bool BLI_remlink_safe(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
void *BLI_pophead(ListBase *listbase) ATTR_NONNULL(1);
void *BLI_poptail(ListBase *listbase) ATTR_NONNULL(1);

void BLI_addhead(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
void BLI_insertlinkbefore(struct ListBase *listbase, void *vnextlink, void *vnewlink) ATTR_NONNULL(1);
void BLI_insertlinkafter(struct ListBase *listbase, void *vprevlink, void *vnewlink) ATTR_NONNULL(1);
void BLI_sortlist(struct ListBase *listbase, int (*cmp)(const void *, const void *)) ATTR_NONNULL(1, 2);
void BLI_sortlist_r(ListBase *listbase, void *thunk, int (*cmp)(void *, const void *, const void *)) ATTR_NONNULL(1, 3);
void BLI_freelist(struct ListBase *listbase) ATTR_NONNULL(1);
int BLI_countlist(const struct ListBase *listbase) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void BLI_freelinkN(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);

void BLI_movelisttolist(struct ListBase *dst, struct ListBase *src) ATTR_NONNULL(1, 2);
void BLI_duplicatelist(struct ListBase *dst, const struct ListBase *src) ATTR_NONNULL(1, 2);
void BLI_listbase_reverse(struct ListBase *lb) ATTR_NONNULL(1);
void BLI_listbase_rotate_first(struct ListBase *lb, void *vlink) ATTR_NONNULL(1, 2);
void BLI_listbase_rotate_last(struct ListBase *lb, void *vlink) ATTR_NONNULL(1, 2);

/**
 * Utility functions to avoid first/last references inline all over.
 */
BLI_INLINE bool BLI_listbase_is_single(const struct ListBase *lb) { return (lb->first && lb->first == lb->last); }
BLI_INLINE bool BLI_listbase_is_empty(const struct ListBase *lb) { return (lb->first == (void *)0); }
BLI_INLINE void BLI_listbase_clear(struct ListBase *lb) { lb->first = lb->last = (void *)0; }

/* create a generic list node containing link to provided data */
struct LinkData *BLI_genericNodeN(void *data);

/**
 * Does a full loop on the list, with any value acting as first
 * (handy for cycling items)
 *
 * \code{.c}
 *
 * BLI_LISTBASE_CIRCULAR_FORWARD_BEGIN (listbase, item, item_init) {
 *     ...operate on marker...
 * }
 * BLI_LISTBASE_CIRCULAR_FORWARD_END (listbase, item, item_init);
 *
 * \endcode
 */
#define BLI_LISTBASE_CIRCULAR_FORWARD_BEGIN(lb, lb_iter, lb_init) \
if ((lb)->first && (lb_init || (lb_init = (lb)->first))) { \
	lb_iter = lb_init; \
	do {
#define BLI_LISTBASE_CIRCULAR_FORWARD_END(lb, lb_iter, lb_init) \
	} while ((lb_iter  = (lb_iter)->next ? (lb_iter)->next : (lb)->first), \
	         (lb_iter != lb_init)); \
}

#define BLI_LISTBASE_CIRCULAR_BACKWARD_BEGIN(lb, lb_iter, lb_init) \
if ((lb)->last && (lb_init || (lb_init = (lb)->last))) { \
	lb_iter = lb_init; \
	do {
#define BLI_LISTBASE_CIRCULAR_BACKWARD_END(lb, lb_iter, lb_init) \
	} while ((lb_iter  = (lb_iter)->prev ? (lb_iter)->prev : (lb)->last), \
	         (lb_iter != lb_init)); \
}

#ifdef __cplusplus
}
#endif

#endif  /* __BLI_LISTBASE_H__ */
