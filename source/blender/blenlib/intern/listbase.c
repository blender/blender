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
 * 
 */

/** \file blender/blenlib/intern/listbase.c
 *  \ingroup bli
 *
 * Manipulations on ListBase structs
 */

#include <string.h>
#include <stdlib.h>


#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_listbase.h"

#include "BLI_strict_flags.h"

/* implementation */

/**
 * moves the entire contents of \a src onto the end of \a dst.
 */
void BLI_movelisttolist(ListBase *dst, ListBase *src)
{
	if (src->first == NULL) return;

	if (dst->first == NULL) {
		dst->first = src->first;
		dst->last = src->last;
	}
	else {
		((Link *)dst->last)->next = src->first;
		((Link *)src->first)->prev = dst->last;
		dst->last = src->last;
	}
	src->first = src->last = NULL;
}

/**
 * Prepends \a vlink (assumed to begin with a Link) onto listbase.
 */
void BLI_addhead(ListBase *listbase, void *vlink)
{
	Link *link = vlink;

	if (link == NULL) return;

	link->next = listbase->first;
	link->prev = NULL;

	if (listbase->first) ((Link *)listbase->first)->prev = link;
	if (listbase->last == NULL) listbase->last = link;
	listbase->first = link;
}


/**
 * Appends \a vlink (assumed to begin with a Link) onto listbase.
 */
void BLI_addtail(ListBase *listbase, void *vlink)
{
	Link *link = vlink;

	if (link == NULL) return;

	link->next = NULL;
	link->prev = listbase->last;

	if (listbase->last) ((Link *)listbase->last)->next = link;
	if (listbase->first == NULL) listbase->first = link;
	listbase->last = link;
}


/**
 * Removes \a vlink from \a listbase. Assumes it is linked into there!
 */
void BLI_remlink(ListBase *listbase, void *vlink)
{
	Link *link = vlink;

	if (link == NULL) return;

	if (link->next) link->next->prev = link->prev;
	if (link->prev) link->prev->next = link->next;

	if (listbase->last == link) listbase->last = link->prev;
	if (listbase->first == link) listbase->first = link->next;
}

/**
 * Checks that \a vlink is linked into listbase, removing it from there if so.
 */
bool BLI_remlink_safe(ListBase *listbase, void *vlink)
{
	if (BLI_findindex(listbase, vlink) != -1) {
		BLI_remlink(listbase, vlink);
		return true;
	}
	else {
		return false;
	}
}

/**
 * Swaps \a vlinka and \a vlinkb in the list. Assumes they are both already in the list!
 */
void BLI_listbase_swaplinks(ListBase *listbase, void *vlinka, void *vlinkb)
{
	Link *linka = vlinka;
	Link *linkb = vlinkb;

	if (!linka || !linkb)
		return;

	if (linkb->next == linka) {
		SWAP(Link *, linka, linkb);
	}

	if (linka->next == linkb) {
		linka->next = linkb->next;
		linkb->prev = linka->prev;
		linka->prev = linkb;
		linkb->next = linka;
	}
	else {  /* Non-contiguous items, we can safely swap. */
		SWAP(Link *, linka->prev, linkb->prev);
		SWAP(Link *, linka->next, linkb->next);
	}

	/* Update neighbors of linka and linkb. */
	if (linka->prev) linka->prev->next = linka;
	if (linka->next) linka->next->prev = linka;
	if (linkb->prev) linkb->prev->next = linkb;
	if (linkb->next) linkb->next->prev = linkb;

	if (listbase->last == linka) listbase->last = linkb;
	else if (listbase->last == linkb) listbase->last = linka;
	if (listbase->first == linka) listbase->first = linkb;
	else if (listbase->first == linkb) listbase->first = linka;
}

/**
 * Removes the head from \a listbase and returns it.
 */
void *BLI_pophead(ListBase *listbase)
{
	Link *link;
	if ((link = listbase->first)) {
		BLI_remlink(listbase, link);
	}
	return link;
}


/**
 * Removes the tail from \a listbase and returns it.
 */
void *BLI_poptail(ListBase *listbase)
{
	Link *link;
	if ((link = listbase->last)) {
		BLI_remlink(listbase, link);
	}
	return link;
}

/**
 * Removes \a vlink from listbase and disposes of it. Assumes it is linked into there!
 */
void BLI_freelinkN(ListBase *listbase, void *vlink)
{
	Link *link = vlink;

	if (link == NULL) return;

	BLI_remlink(listbase, link);
	MEM_freeN(link);
}

/**
 * Assigns all #Link.prev pointers from #Link.next
 */
static void listbase_double_from_single(Link *iter, ListBase *listbase)
{
	Link *prev = NULL;
	listbase->first = iter;
	do {
		iter->prev = prev;
		prev = iter;
	} while ((iter = iter->next));
	listbase->last = prev;
}

#define SORT_IMPL_LINKTYPE Link

/* regular call */
#define SORT_IMPL_FUNC listbase_sort_fn
#include "list_sort_impl.h"
#undef SORT_IMPL_FUNC

/* reentrant call */
#define SORT_IMPL_USE_THUNK
#define SORT_IMPL_FUNC listbase_sort_fn_r
#include "list_sort_impl.h"
#undef SORT_IMPL_FUNC
#undef SORT_IMPL_USE_THUNK

#undef SORT_IMPL_LINKTYPE

/**
 * Sorts the elements of listbase into the order defined by cmp
 * (which should return 1 if its first arg should come after its second arg).
 * This uses insertion sort, so NOT ok for large list.
 */
void BLI_listbase_sort(ListBase *listbase, int (*cmp)(const void *, const void *))
{
	if (listbase->first != listbase->last) {
		Link *head = listbase->first;
		head = listbase_sort_fn(head, cmp);
		listbase_double_from_single(head, listbase);
	}
}

void BLI_listbase_sort_r(ListBase *listbase, int (*cmp)(void *, const void *, const void *), void *thunk)
{
	if (listbase->first != listbase->last) {
		Link *head = listbase->first;
		head = listbase_sort_fn_r(head, cmp, thunk);
		listbase_double_from_single(head, listbase);
	}
}

/**
 * Inserts \a vnewlink immediately following \a vprevlink in \a listbase.
 * Or, if \a vprevlink is NULL, puts \a vnewlink at the front of the list.
 */
void BLI_insertlinkafter(ListBase *listbase, void *vprevlink, void *vnewlink)
{
	Link *prevlink = vprevlink;
	Link *newlink = vnewlink;

	/* newlink before nextlink */
	if (newlink == NULL) return;

	/* empty list */
	if (listbase->first == NULL) {
		listbase->first = newlink;
		listbase->last = newlink;
		return;
	}
	
	/* insert at head of list */
	if (prevlink == NULL) {
		newlink->prev = NULL;
		newlink->next = listbase->first;
		newlink->next->prev = newlink;
		listbase->first = newlink;
		return;
	}

	/* at end of list */
	if (listbase->last == prevlink) {
		listbase->last = newlink;
	}

	newlink->next = prevlink->next;
	newlink->prev = prevlink;
	prevlink->next = newlink;
	if (newlink->next) {
		newlink->next->prev = newlink;
	}
}

/**
 * Inserts \a vnewlink immediately preceding \a vnextlink in listbase.
 * Or, if \a vnextlink is NULL, puts \a vnewlink at the end of the list.
 */
void BLI_insertlinkbefore(ListBase *listbase, void *vnextlink, void *vnewlink)
{
	Link *nextlink = vnextlink;
	Link *newlink = vnewlink;

	/* newlink before nextlink */
	if (newlink == NULL) return;

	/* empty list */
	if (listbase->first == NULL) {
		listbase->first = newlink;
		listbase->last = newlink;
		return;
	}
	
	/* insert at end of list */
	if (nextlink == NULL) {
		newlink->prev = listbase->last;
		newlink->next = NULL;
		((Link *)listbase->last)->next = newlink;
		listbase->last = newlink;
		return;
	}

	/* at beginning of list */
	if (listbase->first == nextlink) {
		listbase->first = newlink;
	}

	newlink->next = nextlink;
	newlink->prev = nextlink->prev;
	nextlink->prev = newlink;
	if (newlink->prev) {
		newlink->prev->next = newlink;
	}
}


/**
 * Insert a link in place of another, without changing it's position in the list.
 *
 * Puts `vnewlink` in the position of `vreplacelink`, removing `vreplacelink`.
 * - `vreplacelink` *must* be in the list.
 * - `vnewlink` *must not* be in the list.
 */
void BLI_insertlinkreplace(ListBase *listbase, void *vreplacelink, void *vnewlink)
{
	Link *l_old = vreplacelink;
	Link *l_new = vnewlink;

	/* update adjacent links */
	if (l_old->next != NULL) {
		l_old->next->prev = l_new;
	}
	if (l_old->prev != NULL) {
		l_old->prev->next = l_new;
	}

	/* set direct links */
	l_new->next = l_old->next;
	l_new->prev = l_old->prev;

	 /* update list */
	if (listbase->first == l_old) {
		listbase->first = l_new;
	}
	if (listbase->last == l_old) {
		listbase->last = l_new;
	}
}

/**
 * Reinsert \a vlink relative to its current position but offset by \a step. Doesn't move
 * item if new position would exceed list (could optionally move to head/tail).
 *
 * \param step: Absolute value defines step size, sign defines direction. E.g pass -1
 *              to move \a vlink before previous, or 1 to move behind next.
 * \return If position of \a vlink has changed.
 */
bool BLI_listbase_link_move(ListBase *listbase, void *vlink, int step)
{
	Link *link = vlink;
	Link *hook = link;
	const bool is_up = step < 0;

	if (step == 0) {
		return false;
	}
	BLI_assert(BLI_findindex(listbase, link) != -1);

	/* find link to insert before/after */
	for (int i = 0; i < ABS(step); i++) {
		hook = is_up ? hook->prev : hook->next;
		if (!hook) {
			return false;
		}
	}

	/* reinsert link */
	BLI_remlink(listbase, vlink);
	if (is_up) {
		BLI_insertlinkbefore(listbase, hook, vlink);
	}
	else {
		BLI_insertlinkafter(listbase, hook, vlink);
	}
	return true;
}


/**
 * Removes and disposes of the entire contents of listbase using direct free(3).
 */
void BLI_freelist(ListBase *listbase)
{
	Link *link, *next;
	
	link = listbase->first;
	while (link) {
		next = link->next;
		free(link);
		link = next;
	}

	BLI_listbase_clear(listbase);
}

/**
 * Removes and disposes of the entire contents of \a listbase using guardedalloc.
 */
void BLI_freelistN(ListBase *listbase)
{
	Link *link, *next;
	
	link = listbase->first;
	while (link) {
		next = link->next;
		MEM_freeN(link);
		link = next;
	}

	BLI_listbase_clear(listbase);
}

/**
 * Returns the number of elements in \a listbase, up until (and including count_max)
 *
 * \note Use to avoid redundant looping.
 */
int BLI_listbase_count_ex(const ListBase *listbase, const int count_max)
{
	Link *link;
	int count = 0;

	for (link = listbase->first; link && count != count_max; link = link->next) {
		count++;
	}

	return count;
}

/**
 * Returns the number of elements in \a listbase.
 */
int BLI_listbase_count(const ListBase *listbase)
{
	Link *link;
	int count = 0;

	for (link = listbase->first; link; link = link->next) {
		count++;
	}

	return count;
}

/**
 * Returns the nth element of \a listbase, numbering from 0.
 */
void *BLI_findlink(const ListBase *listbase, int number)
{
	Link *link = NULL;

	if (number >= 0) {
		link = listbase->first;
		while (link != NULL && number != 0) {
			number--;
			link = link->next;
		}
	}

	return link;
}

/**
 * Returns the nth-last element of \a listbase, numbering from 0.
 */
void *BLI_rfindlink(const ListBase *listbase, int number)
{
	Link *link = NULL;

	if (number >= 0) {
		link = listbase->last;
		while (link != NULL && number != 0) {
			number--;
			link = link->prev;
		}
	}

	return link;
}

/**
 * Returns the position of \a vlink within \a listbase, numbering from 0, or -1 if not found.
 */
int BLI_findindex(const ListBase *listbase, const void *vlink)
{
	Link *link = NULL;
	int number = 0;

	if (vlink == NULL) return -1;
	
	link = listbase->first;
	while (link) {
		if (link == vlink)
			return number;
		
		number++;
		link = link->next;
	}
	
	return -1;
}

/**
 * Finds the first element of \a listbase which contains the null-terminated
 * string \a id at the specified offset, returning NULL if not found.
 */
void *BLI_findstring(const ListBase *listbase, const char *id, const int offset)
{
	Link *link = NULL;
	const char *id_iter;

	for (link = listbase->first; link; link = link->next) {
		id_iter = ((const char *)link) + offset;

		if (id[0] == id_iter[0] && STREQ(id, id_iter)) {
			return link;
		}
	}

	return NULL;
}
/* same as above but find reverse */
/**
 * Finds the last element of \a listbase which contains the
 * null-terminated string \a id at the specified offset, returning NULL if not found.
 */
void *BLI_rfindstring(const ListBase *listbase, const char *id, const int offset)
{
	Link *link = NULL;
	const char *id_iter;

	for (link = listbase->last; link; link = link->prev) {
		id_iter = ((const char *)link) + offset;

		if (id[0] == id_iter[0] && STREQ(id, id_iter)) {
			return link;
		}
	}

	return NULL;
}

/**
 * Finds the first element of \a listbase which contains a pointer to the
 * null-terminated string \a id at the specified offset, returning NULL if not found.
 */
void *BLI_findstring_ptr(const ListBase *listbase, const char *id, const int offset)
{
	Link *link = NULL;
	const char *id_iter;

	for (link = listbase->first; link; link = link->next) {
		/* exact copy of BLI_findstring(), except for this line */
		id_iter = *((const char **)(((const char *)link) + offset));

		if (id[0] == id_iter[0] && STREQ(id, id_iter)) {
			return link;
		}
	}

	return NULL;
}
/* same as above but find reverse */
/**
 * Finds the last element of \a listbase which contains a pointer to the
 * null-terminated string \a id at the specified offset, returning NULL if not found.
 */
void *BLI_rfindstring_ptr(const ListBase *listbase, const char *id, const int offset)
{
	Link *link = NULL;
	const char *id_iter;

	for (link = listbase->last; link; link = link->prev) {
		/* exact copy of BLI_rfindstring(), except for this line */
		id_iter = *((const char **)(((const char *)link) + offset));

		if (id[0] == id_iter[0] && STREQ(id, id_iter)) {
			return link;
		}
	}

	return NULL;
}

/**
 * Finds the first element of listbase which contains the specified pointer value
 * at the specified offset, returning NULL if not found.
 */
void *BLI_findptr(const ListBase *listbase, const void *ptr, const int offset)
{
	Link *link = NULL;
	const void *ptr_iter;

	for (link = listbase->first; link; link = link->next) {
		/* exact copy of BLI_findstring(), except for this line */
		ptr_iter = *((const void **)(((const char *)link) + offset));

		if (ptr == ptr_iter) {
			return link;
		}
	}

	return NULL;
}
/* same as above but find reverse */
/**
 * Finds the last element of listbase which contains the specified pointer value
 * at the specified offset, returning NULL if not found.
 */
void *BLI_rfindptr(const ListBase *listbase, const void *ptr, const int offset)
{
	Link *link = NULL;
	const void *ptr_iter;

	for (link = listbase->last; link; link = link->prev) {
		/* exact copy of BLI_rfindstring(), except for this line */
		ptr_iter = *((const void **)(((const char *)link) + offset));

		if (ptr == ptr_iter) {
			return link;
		}
	}

	return NULL;
}

/**
 * Returns the 0-based index of the first element of listbase which contains the specified
 * null-terminated string at the specified offset, or -1 if not found.
 */
int BLI_findstringindex(const ListBase *listbase, const char *id, const int offset)
{
	Link *link = NULL;
	const char *id_iter;
	int i = 0;

	link = listbase->first;
	while (link) {
		id_iter = ((const char *)link) + offset;

		if (id[0] == id_iter[0] && STREQ(id, id_iter))
			return i;
		i++;
		link = link->next;
	}

	return -1;
}

/**
 * Sets dst to a duplicate of the entire contents of src. dst may be the same as src.
 */
void BLI_duplicatelist(ListBase *dst, const ListBase *src)
{
	struct Link *dst_link, *src_link;

	/* in this order, to ensure it works if dst == src */
	src_link = src->first;
	dst->first = dst->last = NULL;

	while (src_link) {
		dst_link = MEM_dupallocN(src_link);
		BLI_addtail(dst, dst_link);

		src_link = src_link->next;
	}
}

void BLI_listbase_reverse(ListBase *lb)
{
	struct Link *curr = lb->first;
	struct Link *prev = NULL;
	struct Link *next = NULL;
	while (curr) {
		next = curr->next;
		curr->next = prev;
		curr->prev = next;
		prev = curr;
		curr = next;
	}

	/* swap first/last */
	curr = lb->first;
	lb->first = lb->last;
	lb->last = curr;
}

/**
 * \param vlink Link to make first.
 */
void BLI_listbase_rotate_first(ListBase *lb, void *vlink)
{
	/* make circular */
	((Link *)lb->first)->prev = lb->last;
	((Link *)lb->last)->next = lb->first;

	lb->first = vlink;
	lb->last = ((Link *)vlink)->prev;

	((Link *)lb->first)->prev = NULL;
	((Link *)lb->last)->next = NULL;
}

/**
 * \param vlink Link to make last.
 */
void BLI_listbase_rotate_last(ListBase *lb, void *vlink)
{
	/* make circular */
	((Link *)lb->first)->prev = lb->last;
	((Link *)lb->last)->next = lb->first;

	lb->first = ((Link *)vlink)->next;
	lb->last = vlink;

	((Link *)lb->first)->prev = NULL;
	((Link *)lb->last)->next = NULL;
}

/* create a generic list node containing link to provided data */
LinkData *BLI_genericNodeN(void *data)
{
	LinkData *ld;
	
	if (data == NULL)
		return NULL;
		
	/* create new link, and make it hold the given data */
	ld = MEM_callocN(sizeof(LinkData), __func__);
	ld->data = data;
	
	return ld;
} 
