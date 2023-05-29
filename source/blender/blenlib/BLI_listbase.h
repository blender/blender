/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

#pragma once

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

/**
 * Returns the position of \a vlink within \a listbase, numbering from 0, or -1 if not found.
 */
int BLI_findindex(const struct ListBase *listbase, const void *vlink) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
/**
 * Returns the 0-based index of the first element of listbase which contains the specified
 * null-terminated string at the specified offset, or -1 if not found.
 */
int BLI_findstringindex(const struct ListBase *listbase,
                        const char *id,
                        int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/**
 * Return a ListBase representing the entire list the given Link is in.
 */
ListBase BLI_listbase_from_link(struct Link *some_link);

/* Find forwards. */

/**
 * Returns the nth element of \a listbase, numbering from 0.
 */
void *BLI_findlink(const struct ListBase *listbase, int number) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);

/**
 * Returns the nth element after \a link, numbering from 0.
 */
void *BLI_findlinkfrom(struct Link *start, int number) ATTR_WARN_UNUSED_RESULT;

/**
 * Finds the first element of \a listbase which contains the null-terminated
 * string \a id at the specified offset, returning NULL if not found.
 */
void *BLI_findstring(const struct ListBase *listbase,
                     const char *id,
                     int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Finds the first element of \a listbase which contains a pointer to the
 * null-terminated string \a id at the specified offset, returning NULL if not found.
 */
void *BLI_findstring_ptr(const struct ListBase *listbase,
                         const char *id,
                         int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Finds the first element of listbase which contains the specified pointer value
 * at the specified offset, returning NULL if not found.
 */
void *BLI_findptr(const struct ListBase *listbase,
                  const void *ptr,
                  int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Finds the first element of listbase which contains the specified bytes
 * at the specified offset, returning NULL if not found.
 */
void *BLI_listbase_bytes_find(const ListBase *listbase,
                              const void *bytes,
                              size_t bytes_size,
                              int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);
/**
 * Find the first item in the list that matches the given string, or the given index as fallback.
 *
 * \note The string is only used is non-NULL and non-empty.
 *
 * \return The found item, or NULL.
 */
void *BLI_listbase_string_or_index_find(const struct ListBase *listbase,
                                        const char *string,
                                        size_t string_offset,
                                        int index) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/* Find backwards. */

/**
 * Returns the nth-last element of \a listbase, numbering from 0.
 */
void *BLI_rfindlink(const struct ListBase *listbase, int number) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
/**
 * Finds the last element of \a listbase which contains the
 * null-terminated string \a id at the specified offset, returning NULL if not found.
 */
void *BLI_rfindstring(const struct ListBase *listbase,
                      const char *id,
                      int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Finds the last element of \a listbase which contains a pointer to the
 * null-terminated string \a id at the specified offset, returning NULL if not found.
 */
void *BLI_rfindstring_ptr(const struct ListBase *listbase,
                          const char *id,
                          int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Finds the last element of listbase which contains the specified pointer value
 * at the specified offset, returning NULL if not found.
 */
void *BLI_rfindptr(const struct ListBase *listbase,
                   const void *ptr,
                   int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Finds the last element of listbase which contains the specified bytes
 * at the specified offset, returning NULL if not found.
 */
void *BLI_listbase_bytes_rfind(const ListBase *listbase,
                               const void *bytes,
                               size_t bytes_size,
                               int offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2);

/**
 * Removes and disposes of the entire contents of \a listbase using guardedalloc.
 */
void BLI_freelistN(struct ListBase *listbase) ATTR_NONNULL(1);
/**
 * Appends \a vlink (assumed to begin with a Link) onto listbase.
 */
void BLI_addtail(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
/**
 * Removes \a vlink from \a listbase. Assumes it is linked into there!
 *
 * \warning Does _not_ clear the `prev`/`next` pointers of the removed `vlink`.
 */
void BLI_remlink(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
/**
 * Checks that \a vlink is linked into listbase, removing it from there if so.
 */
bool BLI_remlink_safe(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
/**
 * Removes the head from \a listbase and returns it.
 */
void *BLI_pophead(ListBase *listbase) ATTR_NONNULL(1);
/**
 * Removes the tail from \a listbase and returns it.
 */
void *BLI_poptail(ListBase *listbase) ATTR_NONNULL(1);

/**
 * Prepends \a vlink (assumed to begin with a Link) onto listbase.
 */
void BLI_addhead(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);
/**
 * Inserts \a vnewlink immediately preceding \a vnextlink in listbase.
 * Or, if \a vnextlink is NULL, puts \a vnewlink at the end of the list.
 */
void BLI_insertlinkbefore(struct ListBase *listbase, void *vnextlink, void *vnewlink)
    ATTR_NONNULL(1);
/**
 * Inserts \a vnewlink immediately following \a vprevlink in \a listbase.
 * Or, if \a vprevlink is NULL, puts \a vnewlink at the front of the list.
 */
void BLI_insertlinkafter(struct ListBase *listbase, void *vprevlink, void *vnewlink)
    ATTR_NONNULL(1);
/**
 * Insert a link in place of another, without changing its position in the list.
 *
 * Puts `vnewlink` in the position of `vreplacelink`, removing `vreplacelink`.
 * - `vreplacelink` *must* be in the list.
 * - `vnewlink` *must not* be in the list.
 */
void BLI_insertlinkreplace(ListBase *listbase, void *vreplacelink, void *vnewlink)
    ATTR_NONNULL(1, 2, 3);
/**
 * Sorts the elements of listbase into the order defined by cmp
 * (which should return 1 if its first arg should come after its second arg).
 * This uses insertion sort, so NOT ok for large list.
 */
void BLI_listbase_sort(struct ListBase *listbase, int (*cmp)(const void *, const void *))
    ATTR_NONNULL(1, 2);
void BLI_listbase_sort_r(ListBase *listbase,
                         int (*cmp)(void *, const void *, const void *),
                         void *thunk) ATTR_NONNULL(1, 2);
/**
 * Reinsert \a vlink relative to its current position but offset by \a step. Doesn't move
 * item if new position would exceed list (could optionally move to head/tail).
 *
 * \param step: Absolute value defines step size, sign defines direction. E.g pass -1
 *              to move \a vlink before previous, or 1 to move behind next.
 * \return If position of \a vlink has changed.
 */
bool BLI_listbase_link_move(ListBase *listbase, void *vlink, int step) ATTR_NONNULL();
/**
 * Move the link at the index \a from to the position at index \a to.
 *
 * \return If the move was successful.
 */
bool BLI_listbase_move_index(ListBase *listbase, int from, int to) ATTR_NONNULL();
/**
 * Removes and disposes of the entire contents of listbase using direct free(3).
 */
void BLI_freelist(struct ListBase *listbase) ATTR_NONNULL(1);
/**
 * Returns the number of elements in \a listbase, up until (and including count_max)
 *
 * \note Use to avoid redundant looping.
 */
int BLI_listbase_count_at_most(const struct ListBase *listbase,
                               int count_max) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Returns the number of elements in \a listbase.
 */
int BLI_listbase_count(const struct ListBase *listbase) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Removes \a vlink from listbase and disposes of it. Assumes it is linked into there!
 */
void BLI_freelinkN(struct ListBase *listbase, void *vlink) ATTR_NONNULL(1);

/**
 * Swaps \a vlinka and \a vlinkb in the list. Assumes they are both already in the list!
 */
void BLI_listbase_swaplinks(struct ListBase *listbase, void *vlinka, void *vlinkb)
    ATTR_NONNULL(1, 2);
/**
 * Swaps \a vlinka and \a vlinkb from their respective lists.
 * Assumes they are both already in their \a listbasea!
 */
void BLI_listbases_swaplinks(struct ListBase *listbasea,
                             struct ListBase *listbaseb,
                             void *vlinka,
                             void *vlinkb) ATTR_NONNULL(2, 3);

/**
 * Moves the entire contents of \a src onto the end of \a dst.
 */
void BLI_movelisttolist(struct ListBase *dst, struct ListBase *src) ATTR_NONNULL(1, 2);
/**
 * Moves the entire contents of \a src at the beginning of \a dst.
 */
void BLI_movelisttolist_reverse(struct ListBase *dst, struct ListBase *src) ATTR_NONNULL(1, 2);
/**
 * Split `original_listbase` after given `vlink`, putting the remaining of the list into given
 * `split_listbase`.
 *
 * \note If `vlink` is nullptr, it is considered as 'the item before the first item', so the whole
 * list is moved from `original_listbase` to `split_listbase`.
 */
void BLI_listbase_split_after(struct ListBase *original_listbase,
                              struct ListBase *split_listbase,
                              void *vlink) ATTR_NONNULL(1, 2);
/**
 * Sets dst to a duplicate of the entire contents of src. dst may be the same as src.
 */
void BLI_duplicatelist(struct ListBase *dst, const struct ListBase *src) ATTR_NONNULL(1, 2);
void BLI_listbase_reverse(struct ListBase *lb) ATTR_NONNULL(1);
/**
 * \param vlink: Link to make first.
 */
void BLI_listbase_rotate_first(struct ListBase *lb, void *vlink) ATTR_NONNULL(1, 2);
/**
 * \param vlink: Link to make last.
 */
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

/**
 * Validate the integrity of a given ListBase.
 * \return true if everything is OK, false otherwise.
 */
bool BLI_listbase_validate(struct ListBase *lb);

/**
 * Equality check for ListBase.
 *
 * This only shallowly compares the ListBase itself (so the first/last
 * pointers), and does not do any equality checks on the list items.
 */
BLI_INLINE bool BLI_listbase_equal(const struct ListBase *a, const struct ListBase *b)
{
  if (a == NULL) {
    return b == NULL;
  }
  if (b == NULL) {
    return false;
  }
  return a->first == b->first && a->last == b->last;
}

/**
 * Create a generic list node containing link to provided data.
 */
struct LinkData *BLI_genericNodeN(void *data);

/**
 * Does a full loop on the list, with any value acting as first
 * (handy for cycling items)
 *
 * \code{.c}
 *
 * LISTBASE_CIRCULAR_FORWARD_BEGIN(type, listbase, item, item_init)
 * {
 *     ...operate on marker...
 * }
 * LISTBASE_CIRCULAR_FORWARD_END (type, listbase, item, item_init);
 *
 * \endcode
 */
#define LISTBASE_CIRCULAR_FORWARD_BEGIN(type, lb, lb_iter, lb_init) \
  if ((lb)->first && (lb_init || (lb_init = (type)(lb)->first))) { \
    lb_iter = (type)(lb_init); \
    do {
#define LISTBASE_CIRCULAR_FORWARD_END(type, lb, lb_iter, lb_init) \
  } \
  while ((lb_iter = (lb_iter)->next ? (type)(lb_iter)->next : (type)(lb)->first), \
         (lb_iter != lb_init)) \
    ; \
  } \
  ((void)0)

#define LISTBASE_CIRCULAR_BACKWARD_BEGIN(type, lb, lb_iter, lb_init) \
  if ((lb)->last && (lb_init || (lb_init = (type)(lb)->last))) { \
    lb_iter = lb_init; \
    do {
#define LISTBASE_CIRCULAR_BACKWARD_END(type, lb, lb_iter, lb_init) \
  } \
  while ((lb_iter = (lb_iter)->prev ? (lb_iter)->prev : (type)(lb)->last), (lb_iter != lb_init)) \
    ; \
  } \
  ((void)0)

#define LISTBASE_FOREACH(type, var, list) \
  for (type var = (type)((list)->first); var != NULL; var = (type)(((Link *)(var))->next))

/**
 * A version of #LISTBASE_FOREACH that supports incrementing an index variable at every step.
 * Including this in the macro helps prevent mistakes where "continue" mistakenly skips the
 * incrementation.
 */
#define LISTBASE_FOREACH_INDEX(type, var, list, index_var) \
  for (type var = (((void)(index_var = 0)), (type)((list)->first)); var != NULL; \
       var = (type)(((Link *)(var))->next), index_var++)

#define LISTBASE_FOREACH_BACKWARD(type, var, list) \
  for (type var = (type)((list)->last); var != NULL; var = (type)(((Link *)(var))->prev))

/**
 * A version of #LISTBASE_FOREACH that supports removing the item we're looping over.
 */
#define LISTBASE_FOREACH_MUTABLE(type, var, list) \
  for (type var = (type)((list)->first), *var##_iter_next; \
       ((var != NULL) ? ((void)(var##_iter_next = (type)(((Link *)(var))->next)), 1) : 0); \
       var = var##_iter_next)

/**
 * A version of #LISTBASE_FOREACH_BACKWARD that supports removing the item we're looping over.
 */
#define LISTBASE_FOREACH_BACKWARD_MUTABLE(type, var, list) \
  for (type var = (type)((list)->last), *var##_iter_prev; \
       ((var != NULL) ? ((void)(var##_iter_prev = (type)(((Link *)(var))->prev)), 1) : 0); \
       var = var##_iter_prev)

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
BLI_INLINE bool operator==(const ListBase &a, const ListBase &b)
{
  return BLI_listbase_equal(&a, &b);
}
#endif
