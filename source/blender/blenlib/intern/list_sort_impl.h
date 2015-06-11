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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/list_sort_impl.h
 *  \ingroup bli
 *
 * Common implementation of linked-list a non-recursive mergesort.
 *
 * Originally from Mono's eglib, adapted for portable inclusion.
 * This file is to be directly included in C-source,
 * with defines to control its use.
 *
 * This code requires a typedef named `SORT_IMPL_LINKTYPE` for the list node.
 * It is assumed that the list type is the type of a pointer to a list
 * node, and that the node has a field named 'next' that implements to
 * the linked list.  No additional invariant is maintained
 * (e.g. the `prev` pointer of a doubly-linked list node is _not_ updated).
 * Any invariant would require a post-processing pass to update `prev`.
 *
 * Source file including this must define:
 * - `SORT_IMPL_LINKTYPE`:
 *   Struct type for sorting.
 * - `SORT_IMPL_LINKTYPE_DATA`:
 *   Data pointer or leave undefined to pass the link its self.
 * - `SORT_IMPL_FUNC`:
 *   Function name of the sort function.
 *
 * Optionally:
 * - `SORT_IMPL_USE_THUNK`:
 *   Takes an argument for the sort function (like `qsort_r`).
 */

/* -------------------------------------------------------------------- */
/* Handle External Defines */

/* check we're not building directly */
#if !defined(SORT_IMPL_LINKTYPE) || !defined(SORT_IMPL_FUNC)
#  error "This file can't be compiled directly, include in another source file"
#endif

#define list_node SORT_IMPL_LINKTYPE
#define list_sort_do SORT_IMPL_FUNC

#ifdef SORT_IMPL_LINKTYPE_DATA
#  define SORT_ARG(n) ((n)->SORT_IMPL_LINKTYPE_DATA)
#else
#  define SORT_ARG(n) (n)
#endif

#ifdef SORT_IMPL_USE_THUNK
#  define THUNK_APPEND1(a, thunk) a, thunk
#  define THUNK_PREPEND2(thunk, a, b) thunk, a, b
#else
#  define THUNK_APPEND1(a, thunk) a
#  define THUNK_PREPEND2(thunk, a, b) a, b
#endif

#define _CONCAT_AUX(MACRO_ARG1, MACRO_ARG2) MACRO_ARG1 ## MACRO_ARG2
#define _CONCAT(MACRO_ARG1, MACRO_ARG2) _CONCAT_AUX(MACRO_ARG1, MACRO_ARG2)
#define _SORT_PREFIX(id) _CONCAT(SORT_IMPL_FUNC, _##id)

/* local identifiers */
#define SortInfo		_SORT_PREFIX(SortInfo)
#define CompareFn		_SORT_PREFIX(CompareFn)
#define init_sort_info	_SORT_PREFIX(init_sort_info)
#define merge_lists		_SORT_PREFIX(merge_lists)
#define sweep_up		_SORT_PREFIX(sweep_up)
#define insert_list		_SORT_PREFIX(insert_list)

typedef int (* CompareFn)(
#ifdef SORT_IMPL_USE_THUNK
        void *thunk,
#endif
        const void *,
        const void *);


/* -------------------------------------------------------------------- */
/* MIT license from original source */

/*
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Author:
 *   Raja R Harinath (rharinath@novell.com)
 */

/**
 * The maximum possible depth of the merge tree
 * - `ceiling(log2(maximum number of list nodes))`
 * - `ceiling(log2(maximum possible memory size/size of each list node))`
 * - number of bits in `'size_t' - floor(log2(sizeof(list_node *)))`
 *
 * Also, each list in #SortInfo is at least 2 nodes long:
 * we can reduce the depth by 1.
 */
#define FLOOR_LOG2(x) \
	(((x) >= 2) + ((x) >= 4) + ((x) >= 8) + ((x) >= 16) + ((x) >= 32) + ((x) >= 64) + ((x) >= 128))
#define MAX_RANKS \
	((sizeof(size_t) * 8) - FLOOR_LOG2(sizeof(list_node)) - 1)

struct SortInfo {
	unsigned int min_rank, n_ranks;
	CompareFn func;

#ifdef SORT_IMPL_USE_THUNK
	void *thunk;
#endif

	/**
	 * Invariant: `ranks[i] == NULL || length(ranks[i]) >= 2**(i+1)`.
	 *
	 * ~ 128 bytes on 32bit, ~ 512 bytes on 64bit */
	list_node *ranks[MAX_RANKS];
};

BLI_INLINE void init_sort_info(
        struct SortInfo *si,
        CompareFn func
#ifdef SORT_IMPL_USE_THUNK
        ,
        void *thunk
#endif
        )
{
	si->min_rank = si->n_ranks = 0;
	si->func = func;
	/* we don't need to initialize si->ranks,
	 * since we never lookup past si->n_ranks. */

#ifdef SORT_IMPL_USE_THUNK
	si->thunk = thunk;
#endif
}

BLI_INLINE list_node *merge_lists(
        list_node *first, list_node *second,
        CompareFn func
#ifdef SORT_IMPL_USE_THUNK
        ,
        void *thunk
#endif
        )
{
	/* merge the two lists */
	list_node *list = NULL;
	list_node **pos = &list;
	while (first && second) {
		if (func(THUNK_PREPEND2(thunk, SORT_ARG(first), SORT_ARG(second))) > 0) {
			*pos = second;
			second = second->next;
		}
		else {
			*pos = first;
			first = first->next;
		}
		pos = &((*pos)->next);
	}
	*pos = first ? first : second;
	return list;
}

/**
 * Pre-condition:
 * `upto <= si->n_ranks, list == NULL || length(list) == 1`
 */
BLI_INLINE list_node *sweep_up(struct SortInfo *si, list_node *list, unsigned int upto)
{
	unsigned int i;
	for (i = si->min_rank; i < upto; i++) {
		list = merge_lists(si->ranks[i], list, THUNK_APPEND1(si->func, si->thunk));
		si->ranks[i] = NULL;
	}
	return list;
}

/**
 * The 'ranks' array essentially captures the recursion stack of a mergesort.
 * The merge tree is built in a bottom-up manner.  The control loop for
 * updating the 'ranks' array is analogous to incrementing a binary integer,
 * and the `O(n)` time for counting upto n translates to `O(n)` merges when
 * inserting `rank-0` lists.
 * When we plug in the sizes of the lists involved in those merges,
 * we get the `O(n log n)` time for the sort.
 *
 * Inserting higher-ranked lists reduce the height of the merge tree,
 * and also eliminate a lot of redundant comparisons when merging two lists
 * that would've been part of the same run.
 * Adding a `rank-i` list is analogous to incrementing a binary integer by
 * `2**i` in one operation, thus sharing a similar speedup.
 *
 * When inserting higher-ranked lists, we choose to clear out the lower ranks
 * in the interests of keeping the sort stable, but this makes analysis harder.
 * Note that clearing the lower-ranked lists is `O(length(list))--` thus it
 * shouldn't affect the `O(n log n)` behaviour.
 * In other words, inserting one `rank-i` list is equivalent to inserting
 * `2**i` `rank-0` lists, thus even if we do `i` additional merges
 * in the clearing-out (taking at most `2**i` time) we are still fine.
 */

/**
 * Pre-condition:
 * `2**(rank+1) <= length(list) < 2**(rank+2)`
 * (therefore: `length(list) >= 2`)
 */
BLI_INLINE void insert_list(
        struct SortInfo *si,
        list_node *list,
        unsigned int rank)
{
	unsigned int i;

	if (rank > si->n_ranks) {
		if (UNLIKELY(rank > MAX_RANKS)) {
			// printf("Rank '%d' should not exceed " STRINGIFY(MAX_RANKS), rank);
			rank = MAX_RANKS;
		}
		list = merge_lists(sweep_up(si, NULL, si->n_ranks), list, THUNK_APPEND1(si->func, si->thunk));
		for (i = si->n_ranks; i < rank; ++i) {
			si->ranks[i] = NULL;
		}
	}
	else {
		if (rank) {
			list = merge_lists(sweep_up(si, NULL, rank), list, THUNK_APPEND1(si->func, si->thunk));
		}
		for (i = rank; i < si->n_ranks && si->ranks[i]; ++i) {
			list = merge_lists(si->ranks[i], list, THUNK_APPEND1(si->func, si->thunk));
			si->ranks[i] = NULL;
		}
	}

	/* Will _never_ happen: so we can just devolve into quadratic ;-) */
	if (UNLIKELY(i == MAX_RANKS)) {
		i--;
	}

	if (i >= si->n_ranks) {
		si->n_ranks = i + 1;
	}

	si->min_rank = i;
	si->ranks[i] = list;
}

#undef MAX_RANKS
#undef FLOOR_LOG2

/**
 * Main sort function.
 */
BLI_INLINE list_node *list_sort_do(
        list_node *list,
        CompareFn func
#ifdef SORT_IMPL_USE_THUNK
        ,
        void *thunk
#endif
        )
{
	struct SortInfo si;

	init_sort_info(
	        &si,
	        func
#ifdef SORT_IMPL_USE_THUNK
	        ,
	        thunk
#endif
	        );

	while (list && list->next) {
		list_node *next = list->next;
		list_node *tail = next->next;

		if (func(THUNK_PREPEND2(thunk, SORT_ARG(list), SORT_ARG(next))) > 0) {
			next->next = list;
			next = list;
			list = list->next;
		}
		next->next = NULL;

		insert_list(&si, list, 0);

		list = tail;
	}

	return sweep_up(&si, list, si.n_ranks);
}

#undef _CONCAT_AUX
#undef _CONCAT
#undef _SORT_PREFIX

#undef SortInfo
#undef CompareFn
#undef init_sort_info
#undef merge_lists
#undef sweep_up
#undef insert_list

#undef list_node
#undef list_sort_do

#undef THUNK_APPEND1
#undef THUNK_PREPEND2
#undef SORT_ARG
