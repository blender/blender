/*
 * Copyright (c) 2016, Campbell Barton.
 *
 * Licensed under the Apache License, Version 2.0 (the "Apache License")
 * with the following modification; you may not use this file except in
 * compliance with the Apache License and the following modification to it:
 * Section 6. Trademarks. is deleted and replaced with:
 *
 * 6. Trademarks. This License does not grant permission to use the trade
 *   names, trademarks, service marks, or product names of the Licensor
 *   and its affiliates, except as required to comply with Section 4(c) of
 *   the License and to reproduce the content of the NOTICE file.
 *
 * You may obtain a copy of the Apache License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Apache License with the above modification is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the Apache License for the specific
 * language governing permissions and limitations under the Apache License.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <assert.h>

#include "range_tree.h"

typedef unsigned int uint;

/* Use binary-tree for lookups, else fallback to full search */
#define USE_BTREE
/* Use memory pool for nodes, else do individual allocations */
#define USE_TPOOL

/* Node representing a range in the RangeTreeUInt. */
typedef struct Node {
	struct Node *next, *prev;

	/* range (inclusive) */
	uint min, max;

#ifdef USE_BTREE
	/* Left leaning red-black tree, for reference implementation see:
	 * https://gitlab.com/ideasman42/btree-mini-py */
	struct Node *left, *right;
	/* RED/BLACK */
	bool color;
#endif
} Node;

#ifdef USE_TPOOL
/* rt_pool_* pool allocator */
#define TPOOL_IMPL_PREFIX  rt_node
#define TPOOL_ALLOC_TYPE   Node
#define TPOOL_STRUCT       ElemPool_Node
#include "generic_alloc_impl.h"
#undef TPOOL_IMPL_PREFIX
#undef TPOOL_ALLOC_TYPE
#undef TPOOL_STRUCT
#endif  /* USE_TPOOL */

typedef struct LinkedList {
	Node *first, *last;
} LinkedList;

typedef struct RangeTreeUInt {
	uint range[2];
	LinkedList list;
#ifdef USE_BTREE
	Node *root;
#endif
#ifdef USE_TPOOL
	struct ElemPool_Node epool;
#endif
} RangeTreeUInt;

/* ------------------------------------------------------------------------- */
/* List API */

static void list_push_front(LinkedList *list, Node *node)
{
	if (list->first != NULL) {
		node->next = list->first;
		node->next->prev = node;
		node->prev = NULL;
	}
	else {
		list->last = node;
	}
	list->first = node;
}

static void list_push_back(LinkedList *list, Node *node)
{
	if (list->first != NULL) {
		node->prev = list->last;
		node->prev->next = node;
		node->next = NULL;
	}
	else {
		list->first = node;
	}
	list->last = node;
}

static void list_push_after(LinkedList *list, Node *node_prev, Node *node_new)
{
	/* node_new before node_next */

	/* empty list */
	if (list->first == NULL) {
		list->first = node_new;
		list->last = node_new;
		return;
	}

	/* insert at head of list */
	if (node_prev == NULL) {
		node_new->prev = NULL;
		node_new->next = list->first;
		node_new->next->prev = node_new;
		list->first = node_new;
		return;
	}

	/* at end of list */
	if (list->last == node_prev) {
		list->last = node_new;
	}

	node_new->next = node_prev->next;
	node_new->prev = node_prev;
	node_prev->next = node_new;
	if (node_new->next) {
		node_new->next->prev = node_new;
	}
}

static void list_push_before(LinkedList *list, Node *node_next, Node *node_new)
{
	/* node_new before node_next */

	/* empty list */
	if (list->first == NULL) {
		list->first = node_new;
		list->last = node_new;
		return;
	}

	/* insert at end of list */
	if (node_next == NULL) {
		node_new->prev = list->last;
		node_new->next = NULL;
		list->last->next = node_new;
		list->last = node_new;
		return;
	}

	/* at beginning of list */
	if (list->first == node_next) {
		list->first = node_new;
	}

	node_new->next = node_next;
	node_new->prev = node_next->prev;
	node_next->prev = node_new;
	if (node_new->prev) {
		node_new->prev->next = node_new;
	}
}

static void list_remove(LinkedList *list, Node *node)
{
	if (node->next != NULL) {
		node->next->prev = node->prev;
	}
	if (node->prev != NULL) {
		node->prev->next = node->next;
	}

	if (list->last == node) {
		list->last = node->prev;
	}
	if (list->first == node) {
		list->first = node->next;
	}
}

static void list_clear(LinkedList *list)
{
	list->first = NULL;
	list->last = NULL;
}

/* end list API */


/* forward declarations */
static void rt_node_free(RangeTreeUInt *rt, Node *node);


#ifdef USE_BTREE

#ifdef DEBUG
static bool rb_is_balanced_root(const Node *root);
#endif

/* ------------------------------------------------------------------------- */
/* Internal BTree API
 *
 * Left-leaning red-black tree.
 */

/* use minimum, could use max too since nodes never overlap */
#define KEY(n) ((n)->min)

enum {
	RED = 0,
	BLACK = 1,
};


static bool is_red(const Node *node)
{
	return (node && (node->color == RED));
}

static int key_cmp(uint key1, uint key2)
{
	return (key1 == key2) ? 0 : ((key1 < key2) ? -1 : 1);
}

/* removed from the tree */
static void rb_node_invalidate(Node *node)
{
#ifdef DEBUG
	node->left = NULL;
	node->right = NULL;
	node->color = false;
#else
	(void)node;
#endif
}

static void rb_flip_color(Node *node)
{
	node->color ^= 1;
	node->left->color ^= 1;
	node->right->color ^= 1;
}

static Node *rb_rotate_left(Node *left)
{
	/* Make a right-leaning 3-node lean to the left. */
	Node *right = left->right;
	left->right = right->left;
	right->left = left;
	right->color = left->color;
	left->color = RED;
	return right;
}

static Node *rb_rotate_right(Node *right)
{
	/* Make a left-leaning 3-node lean to the right. */
	Node *left = right->left;
	right->left = left->right;
	left->right = right;
	left->color = right->color;
	right->color = RED;
	return left;
}

/* Fixup colors when insert happened */
static Node *rb_fixup_insert(Node *node)
{
	if (is_red(node->right) && !is_red(node->left)) {
		node = rb_rotate_left(node);
	}
	if (is_red(node->left) && is_red(node->left->left)) {
		node = rb_rotate_right(node);
	}

	if (is_red(node->left) && is_red(node->right)) {
		rb_flip_color(node);
	}

	return node;
}

static Node *rb_insert_recursive(Node *node, Node *node_to_insert)
{
	if (node == NULL) {
		return node_to_insert;
	}

	const int cmp = key_cmp(KEY(node_to_insert), KEY(node));
	if (cmp == 0) {
		/* caller ensures no collisions */
		assert(0);
	}
	else if (cmp == -1) {
		node->left = rb_insert_recursive(node->left, node_to_insert);
	}
	else {
		node->right = rb_insert_recursive(node->right, node_to_insert);
	}

	return rb_fixup_insert(node);
}

static Node *rb_insert_root(Node *root, Node *node_to_insert)
{
	root = rb_insert_recursive(root, node_to_insert);
	root->color = BLACK;
	return root;
}

static Node *rb_move_red_to_left(Node *node)
{
	/* Assuming that h is red and both h->left and h->left->left
	 * are black, make h->left or one of its children red.
	 */
	rb_flip_color(node);
	if (node->right && is_red(node->right->left)) {
		node->right = rb_rotate_right(node->right);
		node = rb_rotate_left(node);
		rb_flip_color(node);
	}
	return node;
}

static Node *rb_move_red_to_right(Node *node)
{
	/* Assuming that h is red and both h->right and h->right->left
	 * are black, make h->right or one of its children red.
	 */
	rb_flip_color(node);
	if (node->left && is_red(node->left->left)) {
		node = rb_rotate_right(node);
		rb_flip_color(node);
	}
	return node;
}

/* Fixup colors when remove happened */
static Node *rb_fixup_remove(Node *node)
{
	if (is_red(node->right)) {
		node = rb_rotate_left(node);
	}
	if (is_red(node->left) && is_red(node->left->left)) {
		node = rb_rotate_right(node);
	}
	if (is_red(node->left) && is_red(node->right)) {
		rb_flip_color(node);
	}
	return node;
}

static Node *rb_pop_min_recursive(Node *node, Node **r_node_pop)
{
	if (node == NULL) {
		return NULL;
	}
	if (node->left == NULL) {
		rb_node_invalidate(node);
		*r_node_pop = node;
		return NULL;
	}
	if ((!is_red(node->left)) && (!is_red(node->left->left))) {
		node = rb_move_red_to_left(node);
	}
	node->left = rb_pop_min_recursive(node->left, r_node_pop);
	return rb_fixup_remove(node);
}

static Node *rb_remove_recursive(Node *node, const Node *node_to_remove)
{
	if (node == NULL) {
		return NULL;
	}
	if (key_cmp(KEY(node_to_remove), KEY(node)) == -1) {
		if (node->left != NULL) {
			if ((!is_red(node->left)) && (!is_red(node->left->left))) {
				node = rb_move_red_to_left(node);
			}
		}
		node->left = rb_remove_recursive(node->left, node_to_remove);
	}
	else {
		if (is_red(node->left)) {
			node = rb_rotate_right(node);
		}
		if ((node == node_to_remove) && (node->right == NULL)) {
			rb_node_invalidate(node);
			return NULL;
		}
		assert(node->right != NULL);
		if ((!is_red(node->right)) && (!is_red(node->right->left))) {
			node = rb_move_red_to_right(node);
		}

		if (node == node_to_remove) {
			/* minor improvement over original method:
			 * no need to double lookup min */
			Node *node_free;  /* will always be set */
			node->right = rb_pop_min_recursive(node->right, &node_free);

			node_free->left = node->left;
			node_free->right = node->right;
			node_free->color = node->color;

			rb_node_invalidate(node);
			node = node_free;
		}
		else {
			node->right = rb_remove_recursive(node->right, node_to_remove);
		}
	}
	return rb_fixup_remove(node);
}

static Node *rb_btree_remove(Node *root, const Node *node_to_remove)
{
	root = rb_remove_recursive(root, node_to_remove);
	if (root != NULL) {
		root->color = BLACK;
	}
	return root;
}

/*
 * Returns the node closest to and including 'key',
 * excluding anything below.
 */
static Node *rb_get_or_upper_recursive(Node *n, const uint key)
{
	if (n == NULL) {
		return NULL;
	}
	const int cmp_upper = key_cmp(KEY(n), key);
	if (cmp_upper == 0) {
		return n;  // exact match
	}
	else if (cmp_upper == 1) {
		assert(KEY(n) >= key);
		Node *n_test = rb_get_or_upper_recursive(n->left, key);
		return n_test ? n_test : n;
	}
	else {  // cmp_upper == -1
		return rb_get_or_upper_recursive(n->right, key);
	}
}

/*
 * Returns the node closest to and including 'key',
 * excluding anything above.
 */
static Node *rb_get_or_lower_recursive(Node *n, const uint key)
{
	if (n == NULL) {
		return NULL;
	}
	const int cmp_lower = key_cmp(KEY(n), key);
	if (cmp_lower == 0) {
		return n;  // exact match
	}
	else if (cmp_lower == -1) {
		assert(KEY(n) <= key);
		Node *n_test = rb_get_or_lower_recursive(n->right, key);
		return n_test ? n_test : n;
	}
	else {  // cmp_lower == 1
		return rb_get_or_lower_recursive(n->left, key);
	}
}

#ifdef DEBUG

static bool rb_is_balanced_recursive(const Node *node, int black)
{
	// Does every path from the root to a leaf have the given number
	// of black links?
	if (node == NULL) {
		return black == 0;
	}
	if (!is_red(node)) {
		black--;
	}
	return rb_is_balanced_recursive(node->left, black) &&
	       rb_is_balanced_recursive(node->right, black);
}

static bool rb_is_balanced_root(const Node *root)
{
	// Do all paths from root to leaf have same number of black edges?
	int black = 0;     // number of black links on path from root to min
	const Node *node = root;
	while (node != NULL) {
		if (!is_red(node)) {
			black++;
		}
		node = node->left;
	}
	return rb_is_balanced_recursive(root, black);
}

#endif  // DEBUG


/* End BTree API */
#endif  // USE_BTREE


/* ------------------------------------------------------------------------- */
/* Internal RangeTreeUInt API */

#ifdef _WIN32
#define inline __inline
#endif

static inline Node *rt_node_alloc(RangeTreeUInt *rt)
{
#ifdef USE_TPOOL
	return rt_node_pool_elem_alloc(&rt->epool);
#else
	(void)rt;
	return malloc(sizeof(Node));
#endif
}

static Node *rt_node_new(RangeTreeUInt *rt, uint min, uint max)
{
	Node *node = rt_node_alloc(rt);

	assert(min <= max);
	node->prev = NULL;
	node->next = NULL;
	node->min = min;
	node->max = max;
#ifdef USE_BTREE
	node->left = NULL;
	node->right = NULL;
#endif
	return node;
}

static void rt_node_free(RangeTreeUInt *rt, Node *node)
{
#ifdef USE_TPOOL
	rt_node_pool_elem_free(&rt->epool, node);
#else
	(void)rt;
	free(node);
#endif
}

#ifdef USE_BTREE
static void rt_btree_insert(RangeTreeUInt *rt, Node *node)
{
	node->color = RED;
	node->left = NULL;
	node->right = NULL;
	rt->root = rb_insert_root(rt->root, node);
}
#endif

static void rt_node_add_back(RangeTreeUInt *rt, Node *node)
{
	list_push_back(&rt->list, node);
#ifdef USE_BTREE
	rt_btree_insert(rt, node);
#endif
}
static void rt_node_add_front(RangeTreeUInt *rt, Node *node)
{
	list_push_front(&rt->list, node);
#ifdef USE_BTREE
	rt_btree_insert(rt, node);
#endif
}
static void rt_node_add_before(RangeTreeUInt *rt, Node *node_next, Node *node)
{
	list_push_before(&rt->list, node_next, node);
#ifdef USE_BTREE
	rt_btree_insert(rt, node);
#endif
}
static void rt_node_add_after(RangeTreeUInt *rt, Node *node_prev, Node *node)
{
	list_push_after(&rt->list, node_prev, node);
#ifdef USE_BTREE
	rt_btree_insert(rt, node);
#endif
}

static void rt_node_remove(RangeTreeUInt *rt, Node *node)
{
	list_remove(&rt->list, node);
#ifdef USE_BTREE
	rt->root = rb_btree_remove(rt->root, node);
#endif
	rt_node_free(rt, node);
}

static Node *rt_find_node_from_value(RangeTreeUInt *rt, const uint value)
{
#ifdef USE_BTREE
	Node *node = rb_get_or_lower_recursive(rt->root, value);
	if (node != NULL) {
		if ((value >= node->min) && (value <= node->max)) {
			return node;
		}
	}
	return NULL;
#else
	for (Node *node = rt->list.first; node; node = node->next) {
		if ((value >= node->min) && (value <= node->max)) {
			return node;
		}
	}
	return NULL;
#endif // USE_BTREE
}

static void rt_find_node_pair_around_value(RangeTreeUInt *rt, const uint value,
                                           Node **r_node_prev, Node **r_node_next)
{
	if (value < rt->list.first->min) {
		*r_node_prev = NULL;
		*r_node_next = rt->list.first;
		return;
	}
	else if (value > rt->list.last->max) {
		*r_node_prev = rt->list.last;
		*r_node_next = NULL;
		return;
	}
	else {
#ifdef USE_BTREE
		Node *node_next = rb_get_or_upper_recursive(rt->root, value);
		if (node_next != NULL) {
			Node *node_prev = node_next->prev;
			if ((node_prev->max < value) && (value < node_next->min)) {
				*r_node_prev = node_prev;
				*r_node_next = node_next;
				return;
			}
		}
#else
		Node *node_prev = rt->list.first;
		Node *node_next;
		while ((node_next = node_prev->next)) {
			if ((node_prev->max < value) && (value < node_next->min)) {
				*r_node_prev = node_prev;
				*r_node_next = node_next;
				return;
			}
			node_prev = node_next;
		}
#endif // USE_BTREE
	}
	*r_node_prev = NULL;
	*r_node_next = NULL;
}


/* ------------------------------------------------------------------------- */
/* Public API */

static RangeTreeUInt *rt_create_empty(uint min, uint max)
{
	RangeTreeUInt *rt = malloc(sizeof(*rt));
	rt->range[0] = min;
	rt->range[1] = max;

	list_clear(&rt->list);

#ifdef USE_BTREE
	rt->root = NULL;
#endif
#ifdef USE_TPOOL
	rt_node_pool_create(&rt->epool, 512);
#endif

	return rt;
}

RangeTreeUInt *range_tree_uint_alloc(uint min, uint max)
{
	RangeTreeUInt *rt = rt_create_empty(min, max);

	Node *node = rt_node_new(rt, min, max);
	rt_node_add_front(rt, node);
	return rt;
}

void range_tree_uint_free(RangeTreeUInt *rt)
{
#ifdef DEBUG
#ifdef USE_BTREE
	assert(rb_is_balanced_root(rt->root));
#endif
#endif

#ifdef USE_TPOOL

	rt_node_pool_destroy(&rt->epool);
#else
	for (Node *node = rt->list.first, *node_next; node; node = node_next) {
		node_next = node->next;
		rt_node_free(rt, node);
	}
#endif

	free(rt);
}

#ifdef USE_BTREE
static Node *rt_copy_recursive(RangeTreeUInt *rt_dst, const Node *node_src)
{
	if (node_src == NULL) {
		return NULL;
	}

	Node *node_dst = rt_node_alloc(rt_dst);

	*node_dst = *node_src;
	node_dst->left = rt_copy_recursive(rt_dst, node_dst->left);
	list_push_back(&rt_dst->list, node_dst);
	node_dst->right = rt_copy_recursive(rt_dst, node_dst->right);

	return node_dst;
}
#endif  // USE_BTREE

RangeTreeUInt *range_tree_uint_copy(const RangeTreeUInt *rt_src)
{
	RangeTreeUInt *rt_dst = rt_create_empty(rt_src->range[0], rt_src->range[1]);
#ifdef USE_BTREE
	rt_dst->root = rt_copy_recursive(rt_dst, rt_src->root);
#else
	for (Node *node_src = rt_src->list.first; node_src; node_src = node_src->next) {
		Node *node_dst = rt_node_alloc(rt_dst);
		*node_dst = *node_src;
		list_push_back(&rt_dst->list, node_dst);
	}
#endif
	return rt_dst;
}

/**
 * Return true if the tree has the value (not taken).
 */
bool range_tree_uint_has(RangeTreeUInt *rt, const uint value)
{
	assert(value >= rt->range[0] && value <= rt->range[1]);
	Node *node = rt_find_node_from_value(rt, value);
	return (node != NULL);
}

static void range_tree_uint_take_impl(RangeTreeUInt *rt, const uint value, Node *node)
{
	assert(node == rt_find_node_from_value(rt, value));
	if (node->min == value) {
		if (node->max != value) {
			node->min += 1;
		}
		else {
			assert(node->min == node->max);
			rt_node_remove(rt, node);
		}
	}
	else if (node->max == value) {
		node->max -= 1;
	}
	else {
		Node *node_next = rt_node_new(rt, value + 1, node->max);
		node->max = value - 1;
		rt_node_add_after(rt, node, node_next);
	}
}

void range_tree_uint_take(RangeTreeUInt *rt, const uint value)
{
	Node *node = rt_find_node_from_value(rt, value);
	assert(node != NULL);
	range_tree_uint_take_impl(rt, value, node);
}

bool range_tree_uint_retake(RangeTreeUInt *rt, const uint value)
{
	Node *node = rt_find_node_from_value(rt, value);
	if (node != NULL) {
		range_tree_uint_take_impl(rt, value, node);
		return true;
	}
	else {
		return false;
	}
}

uint range_tree_uint_take_any(RangeTreeUInt *rt)
{
	Node *node = rt->list.first;
	uint value = node->min;
	if (value == node->max) {
		rt_node_remove(rt, node);
	}
	else {
		node->min += 1;
	}
	return value;
}

void range_tree_uint_release(RangeTreeUInt *rt, const uint value)
{
	bool touch_prev, touch_next;
	Node *node_prev, *node_next;

	if (rt->list.first != NULL) {
		rt_find_node_pair_around_value(rt, value, &node_prev, &node_next);
		/* the value must have been already taken */
		assert(node_prev || node_next);

		/* Cases:
		 * 1) fill the gap between prev & next (two spans into one span).
		 * 2) touching prev, (grow node_prev->max up one).
		 * 3) touching next, (grow node_next->min down one).
		 * 4) touching neither, add a new segment. */
		touch_prev = (node_prev != NULL && node_prev->max + 1 == value);
		touch_next = (node_next != NULL && node_next->min - 1 == value);
	}
	else {
		// we could handle this case (4) inline,
		// since its not a common case - use regular logic.
		node_prev = node_next = NULL;
		touch_prev = false;
		touch_next = false;
	}

	if (touch_prev && touch_next) {  // 1)
		node_prev->max = node_next->max;
		rt_node_remove(rt, node_next);
	}
	else if (touch_prev) {  // 2)
		assert(node_prev->max + 1 == value);
		node_prev->max = value;
	}
	else if (touch_next) {  // 3)
		assert(node_next->min - 1 == value);
		node_next->min = value;
	}
	else {  // 4)
		Node *node_new = rt_node_new(rt, value, value);
		if (node_prev != NULL) {
			rt_node_add_after(rt, node_prev, node_new);
		}
		else if (node_next != NULL) {
			rt_node_add_before(rt, node_next, node_new);
		}
		else {
			assert(rt->list.first == NULL);
			rt_node_add_back(rt, node_new);
		}
	}
}
