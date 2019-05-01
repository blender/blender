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

#ifndef __BLI_LISTBASE_H__
#define __BLI_LISTBASE_H__

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"
#include "DNA_listBase.h"
// struct ListBase;
// struct LinkData;

#ifdef __cplusplus
extern "C" {
#endif

int BLI_findindex(const struct ListBase *listbase, const void *vlink) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
int BLI_findstringindex(const struct ListBase *listbase,
                        const char *id,
                        const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/* find forwards */
void *BLI_findlink(const struct ListBase *listbase, int number) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
void *BLI_findstring(const struct ListBase *listbase,
                     const char *id,
                     const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_findstring_ptr(const struct ListBase *listbase,
                         const char *id,
                         const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_findptr(const struct ListBase *listbase,
                  const void *ptr,
                  const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_listbase_bytes_find(const ListBase *listbase,
                              const void *bytes,
                              const size_t bytes_size,
                              const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);

/* find backwards */
void *BLI_rfindlink(const struct ListBase *listbase, int number) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
void *BLI_rfindstring(const struct ListBase *listbase,
                      const char *id,
                      const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_rfindstring_ptr(const struct ListBase *listbase,
                          const char *id,
                          const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_rfindptr(const struct ListBase *listbase,
                   const void *ptr,
                   const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_listbase_bytes_rfind(const ListBase *listbase,
                               const void *bytes,
                               const size_t bytes_size,
                               const int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);

void BLI_freelistN(struct ListBase *listbase) ATTR_NONNULL(1);
void BLI_addtail(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
void BLI_remlink(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
bool BLI_remlink_safe(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
void *BLI_pophead(ListBase *listbase) ATTR_NONNULL(1);
void *BLI_poptail(ListBase *listbase) ATTR_NONNULL(1);

void BLI_addhead(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
void BLI_insertlinkbefore(struct ListBase *listbase, void *vnextlink, void *vnewlink)
    ATTR_NONNULL(1);
void BLI_insertlinkafter(struct ListBase *listbase, void *vprevlink, void *vnewlink)
    ATTR_NONNULL(1);
void BLI_insertlinkreplace(ListBase *listbase, void *v_l_src, void *v_l_dst) ATTR_NONNULL(1, 2, 3);
void BLI_listbase_sort(struct ListBase *listbase, int (*cmp)(const void *, const void *))
    ATTR_NONNULL(1, 2);
void BLI_listbase_sort_r(ListBase *listbase,
                         int (*cmp)(void *, const void *, const void *),
                         void *thunk) ATTR_NONNULL(1, 2);
bool BLI_listbase_link_move(ListBase *listbase, void *vlink, int step) ATTR_NONNULL();
void BLI_freelist(struct ListBase *listbase) ATTR_NONNULL(1);
int BLI_listbase_count_at_most(const struct ListBase *listbase,
                               const int count_max) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLI_listbase_count(const struct ListBase *listbase) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void BLI_freelinkN(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);

void BLI_listbase_swaplinks(struct ListBase *listbase, void *vlinka, void *vlinkb)
    ATTR_NONNULL(1, 2);
void BLI_listbases_swaplinks(struct ListBase *listbasea,
                             struct ListBase *listbaseb,
                             void *vlinka,
                             void *vlinkb) ATTR_NONNULL(2, 3);

void BLI_movelisttolist(struct ListBase *dst, struct ListBase *src) ATTR_NONNULL(1, 2);
void BLI_movelisttolist_reverse(struct ListBase *dst, struct ListBase *src) ATTR_NONNULL(1, 2);
void BLI_duplicatelist(struct ListBase *dst, const struct ListBase *src) ATTR_NONNULL(1, 2);
void BLI_listbase_reverse(struct ListBase *lb) ATTR_NONNULL(1);
void BLI_listbase_rotate_first(struct ListBase *lb, void *vlink) ATTR_NONNULL(1, 2);
void BLI_listbase_rotate_last(struct ListBase *lb, void *vlink) ATTR_NONNULL(1, 2);

/**
 * Utility functions to avoid first/last references inline all over.
 */
BLI_INLINE bool BLI_listbase_is_single(const struct ListBase *lb)
{
  return (lb->first && lb->first == lb->last);
}
BLI_INLINE bool BLI_listbase_is_empty(const struct ListBase *lb)
{
  return (lb->first == (void *)0);
}
BLI_INLINE void BLI_listbase_clear(struct ListBase *lb)
{
  lb->first = lb->last = (void *)0;
}

/* create a generic list node containing link to provided data */
struct LinkData *BLI_genericNodeN(void *data);

/**
 * Does a full loop on the list, with any value acting as first
 * (handy for cycling items)
 *
 * \code{.c}
 *
 * LISTBASE_CIRCULAR_FORWARD_BEGIN(listbase, item, item_init)
 * {
 *     ...operate on marker...
 * }
 * LISTBASE_CIRCULAR_FORWARD_END (listbase, item, item_init);
 *
 * \endcode
 */
#define LISTBASE_CIRCULAR_FORWARD_BEGIN(lb, lb_iter, lb_init) \
  if ((lb)->first && (lb_init || (lb_init = (lb)->first))) { \
    lb_iter = lb_init; \
    do {
#define LISTBASE_CIRCULAR_FORWARD_END(lb, lb_iter, lb_init) \
  } \
  while ((lb_iter = (lb_iter)->next ? (lb_iter)->next : (lb)->first), (lb_iter != lb_init)) \
    ; \
  } \
  ((void)0)

#define LISTBASE_CIRCULAR_BACKWARD_BEGIN(lb, lb_iter, lb_init) \
  if ((lb)->last && (lb_init || (lb_init = (lb)->last))) { \
    lb_iter = lb_init; \
    do {
#define LISTBASE_CIRCULAR_BACKWARD_END(lb, lb_iter, lb_init) \
  } \
  while ((lb_iter = (lb_iter)->prev ? (lb_iter)->prev : (lb)->last), (lb_iter != lb_init)) \
    ; \
  } \
  ((void)0)

#define LISTBASE_FOREACH(type, var, list) \
  for (type var = (type)((list)->first); var != NULL; var = (type)(((Link *)(var))->next))

/** A verion of #LISTBASE_FOREACH that supports removing the item we're looping over. */
#define LISTBASE_FOREACH_MUTABLE(type, var, list) \
  for (type var = (type)((list)->first), *var##_iter_next; \
       ((var != NULL) ? ((void)(var##_iter_next = (type)(((Link *)(var))->next)), 1) : 0); \
       var = var##_iter_next)

#ifdef __cplusplus
}
#endif

#endif /* __BLI_LISTBASE_H__ */
