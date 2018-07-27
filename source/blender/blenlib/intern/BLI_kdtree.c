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
 * Contributor(s): Janne Karhu
 *                 Brecht Van Lommel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/BLI_kdtree.c
 *  \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_kdtree.h"
#include "BLI_utildefines.h"
#include "BLI_strict_flags.h"

typedef struct KDTreeNode_head {
	uint left, right;
	float co[3];
	int index;
} KDTreeNode_head;

typedef struct KDTreeNode {
	uint left, right;
	float co[3];
	int index;
	uint d;  /* range is only (0-2) */
} KDTreeNode;

struct KDTree {
	KDTreeNode *nodes;
	uint totnode;
	uint root;
#ifdef DEBUG
	bool is_balanced;  /* ensure we call balance first */
	uint maxsize;   /* max size of the tree */
#endif
};

#define KD_STACK_INIT 100      /* initial size for array (on the stack) */
#define KD_NEAR_ALLOC_INC 100  /* alloc increment for collecting nearest */
#define KD_FOUND_ALLOC_INC 50  /* alloc increment for collecting nearest */

#define KD_NODE_UNSET ((uint)-1)

/**
 * Creates or free a kdtree
 */
KDTree *BLI_kdtree_new(uint maxsize)
{
	KDTree *tree;

	tree = MEM_mallocN(sizeof(KDTree), "KDTree");
	tree->nodes = MEM_mallocN(sizeof(KDTreeNode) * maxsize, "KDTreeNode");
	tree->totnode = 0;
	tree->root = KD_NODE_UNSET;

#ifdef DEBUG
	tree->is_balanced = false;
	tree->maxsize = maxsize;
#endif

	return tree;
}

void BLI_kdtree_free(KDTree *tree)
{
	if (tree) {
		MEM_freeN(tree->nodes);
		MEM_freeN(tree);
	}
}

/**
 * Construction: first insert points, then call balance. Normal is optional.
 */
void BLI_kdtree_insert(KDTree *tree, int index, const float co[3])
{
	KDTreeNode *node = &tree->nodes[tree->totnode++];

#ifdef DEBUG
	BLI_assert(tree->totnode <= tree->maxsize);
#endif

	/* note, array isn't calloc'd,
	 * need to initialize all struct members */

	node->left = node->right = KD_NODE_UNSET;
	copy_v3_v3(node->co, co);
	node->index = index;
	node->d = 0;

#ifdef DEBUG
	tree->is_balanced = false;
#endif
}

static uint kdtree_balance(KDTreeNode *nodes, uint totnode, uint axis, const uint ofs)
{
	KDTreeNode *node;
	float co;
	uint left, right, median, i, j;

	if (totnode <= 0)
		return KD_NODE_UNSET;
	else if (totnode == 1)
		return 0 + ofs;

	/* quicksort style sorting around median */
	left = 0;
	right = totnode - 1;
	median = totnode / 2;

	while (right > left) {
		co = nodes[right].co[axis];
		i = left - 1;
		j = right;

		while (1) {
			while (nodes[++i].co[axis] < co) ;
			while (nodes[--j].co[axis] > co && j > left) ;

			if (i >= j)
				break;

			SWAP(KDTreeNode_head, *(KDTreeNode_head *)&nodes[i], *(KDTreeNode_head *)&nodes[j]);
		}

		SWAP(KDTreeNode_head, *(KDTreeNode_head *)&nodes[i], *(KDTreeNode_head *)&nodes[right]);
		if (i >= median)
			right = i - 1;
		if (i <= median)
			left = i + 1;
	}

	/* set node and sort subnodes */
	node = &nodes[median];
	node->d = axis;
	axis = (axis + 1) % 3;
	node->left = kdtree_balance(nodes, median, axis, ofs);
	node->right = kdtree_balance(nodes + median + 1, (totnode - (median + 1)), axis, (median + 1) + ofs);

	return median + ofs;
}

void BLI_kdtree_balance(KDTree *tree)
{
	tree->root = kdtree_balance(tree->nodes, tree->totnode, 0, 0);

#ifdef DEBUG
	tree->is_balanced = true;
#endif
}

static float squared_distance(const float v2[3], const float v1[3], const float n2[3])
{
	float d[3], dist;

	d[0] = v2[0] - v1[0];
	d[1] = v2[1] - v1[1];
	d[2] = v2[2] - v1[2];

	dist = len_squared_v3(d);

	/* can someone explain why this is done?*/
	if (n2 && (dot_v3v3(d, n2) < 0.0f)) {
		dist *= 10.0f;
	}

	return dist;
}

static uint *realloc_nodes(uint *stack, uint *totstack, const bool is_alloc)
{
	uint *stack_new = MEM_mallocN((*totstack + KD_NEAR_ALLOC_INC) * sizeof(uint), "KDTree.treestack");
	memcpy(stack_new, stack, *totstack * sizeof(uint));
	// memset(stack_new + *totstack, 0, sizeof(uint) * KD_NEAR_ALLOC_INC);
	if (is_alloc)
		MEM_freeN(stack);
	*totstack += KD_NEAR_ALLOC_INC;
	return stack_new;
}

/**
 * Find nearest returns index, and -1 if no node is found.
 */
int BLI_kdtree_find_nearest(
        const KDTree *tree, const float co[3],
        KDTreeNearest *r_nearest)
{
	const KDTreeNode *nodes = tree->nodes;
	const KDTreeNode *root, *min_node;
	uint *stack, defaultstack[KD_STACK_INIT];
	float min_dist, cur_dist;
	uint totstack, cur = 0;

#ifdef DEBUG
	BLI_assert(tree->is_balanced == true);
#endif

	if (UNLIKELY(tree->root == KD_NODE_UNSET))
		return -1;

	stack = defaultstack;
	totstack = KD_STACK_INIT;

	root = &nodes[tree->root];
	min_node = root;
	min_dist = len_squared_v3v3(root->co, co);

	if (co[root->d] < root->co[root->d]) {
		if (root->right != KD_NODE_UNSET)
			stack[cur++] = root->right;
		if (root->left != KD_NODE_UNSET)
			stack[cur++] = root->left;
	}
	else {
		if (root->left != KD_NODE_UNSET)
			stack[cur++] = root->left;
		if (root->right != KD_NODE_UNSET)
			stack[cur++] = root->right;
	}

	while (cur--) {
		const KDTreeNode *node = &nodes[stack[cur]];

		cur_dist = node->co[node->d] - co[node->d];

		if (cur_dist < 0.0f) {
			cur_dist = -cur_dist * cur_dist;

			if (-cur_dist < min_dist) {
				cur_dist = len_squared_v3v3(node->co, co);
				if (cur_dist < min_dist) {
					min_dist = cur_dist;
					min_node = node;
				}
				if (node->left != KD_NODE_UNSET)
					stack[cur++] = node->left;
			}
			if (node->right != KD_NODE_UNSET)
				stack[cur++] = node->right;
		}
		else {
			cur_dist = cur_dist * cur_dist;

			if (cur_dist < min_dist) {
				cur_dist = len_squared_v3v3(node->co, co);
				if (cur_dist < min_dist) {
					min_dist = cur_dist;
					min_node = node;
				}
				if (node->right != KD_NODE_UNSET)
					stack[cur++] = node->right;
			}
			if (node->left != KD_NODE_UNSET)
				stack[cur++] = node->left;
		}
		if (UNLIKELY(cur + 3 > totstack)) {
			stack = realloc_nodes(stack, &totstack, defaultstack != stack);
		}
	}

	if (r_nearest) {
		r_nearest->index = min_node->index;
		r_nearest->dist = sqrtf(min_dist);
		copy_v3_v3(r_nearest->co, min_node->co);
	}

	if (stack != defaultstack)
		MEM_freeN(stack);

	return min_node->index;
}


/**
 * A version of #BLI_kdtree_find_nearest which runs a callback
 * to filter out values.
 *
 * \param filter_cb: Filter find results,
 * Return codes: (1: accept, 0: skip, -1: immediate exit).
 */
int BLI_kdtree_find_nearest_cb(
        const KDTree *tree, const float co[3],
        int (*filter_cb)(void *user_data, int index, const float co[3], float dist_sq), void *user_data,
        KDTreeNearest *r_nearest)
{
	const KDTreeNode *nodes = tree->nodes;
	const KDTreeNode *min_node = NULL;

	uint *stack, defaultstack[KD_STACK_INIT];
	float min_dist = FLT_MAX, cur_dist;
	uint totstack, cur = 0;

#ifdef DEBUG
	BLI_assert(tree->is_balanced == true);
#endif

	if (UNLIKELY(tree->root == KD_NODE_UNSET))
		return -1;

	stack = defaultstack;
	totstack = KD_STACK_INIT;

#define NODE_TEST_NEAREST(node) \
{ \
	const float dist_sq = len_squared_v3v3((node)->co, co); \
	if (dist_sq < min_dist) { \
		const int result = filter_cb(user_data, (node)->index, (node)->co, dist_sq); \
		if (result == 1) { \
			min_dist = dist_sq; \
			min_node = node; \
		} \
		else if (result == 0) { \
			/* pass */ \
		} \
		else { \
			BLI_assert(result == -1); \
			goto finally; \
		} \
	} \
} ((void)0)

	stack[cur++] = tree->root;

	while (cur--) {
		const KDTreeNode *node = &nodes[stack[cur]];

		cur_dist = node->co[node->d] - co[node->d];

		if (cur_dist < 0.0f) {
			cur_dist = -cur_dist * cur_dist;

			if (-cur_dist < min_dist) {
				NODE_TEST_NEAREST(node);

				if (node->left != KD_NODE_UNSET)
					stack[cur++] = node->left;
			}
			if (node->right != KD_NODE_UNSET)
				stack[cur++] = node->right;
		}
		else {
			cur_dist = cur_dist * cur_dist;

			if (cur_dist < min_dist) {
				NODE_TEST_NEAREST(node);

				if (node->right != KD_NODE_UNSET)
					stack[cur++] = node->right;
			}
			if (node->left != KD_NODE_UNSET)
				stack[cur++] = node->left;
		}
		if (UNLIKELY(cur + 3 > totstack)) {
			stack = realloc_nodes(stack, &totstack, defaultstack != stack);
		}
	}

#undef NODE_TEST_NEAREST


finally:
	if (stack != defaultstack)
		MEM_freeN(stack);

	if (min_node) {
		if (r_nearest) {
			r_nearest->index = min_node->index;
			r_nearest->dist = sqrtf(min_dist);
			copy_v3_v3(r_nearest->co, min_node->co);
		}

		return min_node->index;
	}
	else {
		return -1;
	}
}

static void add_nearest(KDTreeNearest *ptn, uint *found, uint n, int index,
                        float dist, const float *co)
{
	uint i;

	if (*found < n) (*found)++;

	for (i = *found - 1; i > 0; i--) {
		if (dist >= ptn[i - 1].dist)
			break;
		else
			ptn[i] = ptn[i - 1];
	}

	ptn[i].index = index;
	ptn[i].dist = dist;
	copy_v3_v3(ptn[i].co, co);
}

/**
 * Find n nearest returns number of points found, with results in nearest.
 * Normal is optional, but if given will limit results to points in normal direction from co.
 *
 * \param r_nearest  An array of nearest, sized at least \a n.
 */
int BLI_kdtree_find_nearest_n__normal(
        const KDTree *tree, const float co[3], const float nor[3],
        KDTreeNearest r_nearest[],
        uint n)
{
	const KDTreeNode *nodes = tree->nodes;
	const KDTreeNode *root;
	uint *stack, defaultstack[KD_STACK_INIT];
	float cur_dist;
	uint totstack, cur = 0;
	uint i, found = 0;

#ifdef DEBUG
	BLI_assert(tree->is_balanced == true);
#endif

	if (UNLIKELY((tree->root == KD_NODE_UNSET) || n == 0))
		return 0;

	stack = defaultstack;
	totstack = KD_STACK_INIT;

	root = &nodes[tree->root];

	cur_dist = squared_distance(root->co, co, nor);
	add_nearest(r_nearest, &found, n, root->index, cur_dist, root->co);

	if (co[root->d] < root->co[root->d]) {
		if (root->right != KD_NODE_UNSET)
			stack[cur++] = root->right;
		if (root->left != KD_NODE_UNSET)
			stack[cur++] = root->left;
	}
	else {
		if (root->left != KD_NODE_UNSET)
			stack[cur++] = root->left;
		if (root->right != KD_NODE_UNSET)
			stack[cur++] = root->right;
	}

	while (cur--) {
		const KDTreeNode *node = &nodes[stack[cur]];

		cur_dist = node->co[node->d] - co[node->d];

		if (cur_dist < 0.0f) {
			cur_dist = -cur_dist * cur_dist;

			if (found < n || -cur_dist < r_nearest[found - 1].dist) {
				cur_dist = squared_distance(node->co, co, nor);

				if (found < n || cur_dist < r_nearest[found - 1].dist)
					add_nearest(r_nearest, &found, n, node->index, cur_dist, node->co);

				if (node->left != KD_NODE_UNSET)
					stack[cur++] = node->left;
			}
			if (node->right != KD_NODE_UNSET)
				stack[cur++] = node->right;
		}
		else {
			cur_dist = cur_dist * cur_dist;

			if (found < n || cur_dist < r_nearest[found - 1].dist) {
				cur_dist = squared_distance(node->co, co, nor);
				if (found < n || cur_dist < r_nearest[found - 1].dist)
					add_nearest(r_nearest, &found, n, node->index, cur_dist, node->co);

				if (node->right != KD_NODE_UNSET)
					stack[cur++] = node->right;
			}
			if (node->left != KD_NODE_UNSET)
				stack[cur++] = node->left;
		}
		if (UNLIKELY(cur + 3 > totstack)) {
			stack = realloc_nodes(stack, &totstack, defaultstack != stack);
		}
	}

	for (i = 0; i < found; i++)
		r_nearest[i].dist = sqrtf(r_nearest[i].dist);

	if (stack != defaultstack)
		MEM_freeN(stack);

	return (int)found;
}

static int range_compare(const void *a, const void *b)
{
	const KDTreeNearest *kda = a;
	const KDTreeNearest *kdb = b;

	if (kda->dist < kdb->dist)
		return -1;
	else if (kda->dist > kdb->dist)
		return 1;
	else
		return 0;
}
static void add_in_range(
        KDTreeNearest **r_foundstack,
        uint   *r_foundstack_tot_alloc,
        uint      found,
        const int index, const float dist, const float *co)
{
	KDTreeNearest *to;

	if (UNLIKELY(found >= *r_foundstack_tot_alloc)) {
		*r_foundstack = MEM_reallocN_id(
		        *r_foundstack,
		        (*r_foundstack_tot_alloc += KD_FOUND_ALLOC_INC) * sizeof(KDTreeNode),
		        __func__);
	}

	to = (*r_foundstack) + found;

	to->index = index;
	to->dist = sqrtf(dist);
	copy_v3_v3(to->co, co);
}

/**
 * Range search returns number of points found, with results in nearest
 * Normal is optional, but if given will limit results to points in normal direction from co.
 * Remember to free nearest after use!
 */
int BLI_kdtree_range_search__normal(
        const KDTree *tree, const float co[3], const float nor[3],
        KDTreeNearest **r_nearest, float range)
{
	const KDTreeNode *nodes = tree->nodes;
	uint *stack, defaultstack[KD_STACK_INIT];
	KDTreeNearest *foundstack = NULL;
	float range_sq = range * range, dist_sq;
	uint totstack, cur = 0, found = 0, totfoundstack = 0;

#ifdef DEBUG
	BLI_assert(tree->is_balanced == true);
#endif

	if (UNLIKELY(tree->root == KD_NODE_UNSET))
		return 0;

	stack = defaultstack;
	totstack = KD_STACK_INIT;

	stack[cur++] = tree->root;

	while (cur--) {
		const KDTreeNode *node = &nodes[stack[cur]];

		if (co[node->d] + range < node->co[node->d]) {
			if (node->left != KD_NODE_UNSET)
				stack[cur++] = node->left;
		}
		else if (co[node->d] - range > node->co[node->d]) {
			if (node->right != KD_NODE_UNSET)
				stack[cur++] = node->right;
		}
		else {
			dist_sq = squared_distance(node->co, co, nor);
			if (dist_sq <= range_sq) {
				add_in_range(&foundstack, &totfoundstack, found++, node->index, dist_sq, node->co);
			}

			if (node->left != KD_NODE_UNSET)
				stack[cur++] = node->left;
			if (node->right != KD_NODE_UNSET)
				stack[cur++] = node->right;
		}

		if (UNLIKELY(cur + 3 > totstack)) {
			stack = realloc_nodes(stack, &totstack, defaultstack != stack);
		}
	}

	if (stack != defaultstack)
		MEM_freeN(stack);

	if (found)
		qsort(foundstack, found, sizeof(KDTreeNearest), range_compare);

	*r_nearest = foundstack;

	return (int)found;
}

/**
 * A version of #BLI_kdtree_range_search which runs a callback
 * instead of allocating an array.
 *
 * \param search_cb: Called for every node found in \a range, false return value performs an early exit.
 *
 * \note the order of calls isn't sorted based on distance.
 */
void BLI_kdtree_range_search_cb(
        const KDTree *tree, const float co[3], float range,
        bool (*search_cb)(void *user_data, int index, const float co[3], float dist_sq), void *user_data)
{
	const KDTreeNode *nodes = tree->nodes;

	uint *stack, defaultstack[KD_STACK_INIT];
	float range_sq = range * range, dist_sq;
	uint totstack, cur = 0;

#ifdef DEBUG
	BLI_assert(tree->is_balanced == true);
#endif

	if (UNLIKELY(tree->root == KD_NODE_UNSET))
		return;

	stack = defaultstack;
	totstack = KD_STACK_INIT;

	stack[cur++] = tree->root;

	while (cur--) {
		const KDTreeNode *node = &nodes[stack[cur]];

		if (co[node->d] + range < node->co[node->d]) {
			if (node->left != KD_NODE_UNSET)
				stack[cur++] = node->left;
		}
		else if (co[node->d] - range > node->co[node->d]) {
			if (node->right != KD_NODE_UNSET)
				stack[cur++] = node->right;
		}
		else {
			dist_sq = len_squared_v3v3(node->co, co);
			if (dist_sq <= range_sq) {
				if (search_cb(user_data, node->index, node->co, dist_sq) == false) {
					goto finally;
				}
			}

			if (node->left != KD_NODE_UNSET)
				stack[cur++] = node->left;
			if (node->right != KD_NODE_UNSET)
				stack[cur++] = node->right;
		}

		if (UNLIKELY(cur + 3 > totstack)) {
			stack = realloc_nodes(stack, &totstack, defaultstack != stack);
		}
	}

finally:
	if (stack != defaultstack)
		MEM_freeN(stack);
}

/**
 * Use when we want to loop over nodes ordered by index.
 * Requires indices to be aligned with nodes.
 */
static uint *kdtree_order(const KDTree *tree)
{
	const KDTreeNode *nodes = tree->nodes;
	uint *order = MEM_mallocN(sizeof(uint) * tree->totnode, __func__);
	for (uint i = 0; i < tree->totnode; i++) {
		order[nodes[i].index] = i;
	}
	return order;
}

/* -------------------------------------------------------------------- */
/** \name BLI_kdtree_calc_duplicates_fast
 * \{ */

struct DeDuplicateParams {
	/* Static */
	const KDTreeNode *nodes;
	float range;
	float range_sq;
	int *duplicates;
	int *duplicates_found;

	/* Per Search */
	float search_co[3];
	int search;
};

static void deduplicate_recursive(const struct DeDuplicateParams *p, uint i)
{
	const KDTreeNode *node = &p->nodes[i];
	if (p->search_co[node->d] + p->range <= node->co[node->d]) {
		if (node->left != KD_NODE_UNSET) {
			deduplicate_recursive(p, node->left);
		}
	}
	else if (p->search_co[node->d] - p->range >= node->co[node->d]) {
		if (node->right != KD_NODE_UNSET) {
			deduplicate_recursive(p, node->right);
		}
	}
	else {
		if ((p->search != node->index) && (p->duplicates[node->index] == -1)) {
			if (compare_len_squared_v3v3(node->co, p->search_co, p->range_sq)) {
				p->duplicates[node->index] = (int)p->search;
				*p->duplicates_found += 1;
			}
		}
		if (node->left != KD_NODE_UNSET) {
			deduplicate_recursive(p, node->left);
		}
		if (node->right != KD_NODE_UNSET) {
			deduplicate_recursive(p, node->right);
		}
	}
}

/**
 * Find duplicate points in \a range.
 * Favors speed over quality since it doesn't find the best target vertex for merging.
 * Nodes are looped over, duplicates are added when found.
 * Nevertheless results are predictable.
 *
 * \param range: Coordinates in this range are candidates to be merged.
 * \param use_index_order: Loop over the coordinates ordered by #KDTreeNode.index
 * At the expense of some performance, this ensures the layout of the tree doesn't influence
 * the iteration order.
 * \param duplicates: An array of int's the length of #KDTree.totnode
 * Values initialized to -1 are candidates to me merged.
 * Setting the index to it's own position in the array prevents it from being touched,
 * although it can still be used as a target.
 * \returns The numebr of merges found (includes any merges already in the \a duplicates array).
 *
 * \note Merging is always a single step (target indices wont be marked for merging).
 */
int BLI_kdtree_calc_duplicates_fast(
        const KDTree *tree, const float range, bool use_index_order,
        int *duplicates)
{
	int found = 0;
	struct DeDuplicateParams p = {
		.nodes = tree->nodes,
		.range = range,
		.range_sq = range * range,
		.duplicates = duplicates,
		.duplicates_found = &found,
	};

	if (use_index_order) {
		uint *order = kdtree_order(tree);
		for (uint i = 0; i < tree->totnode; i++) {
			const uint node_index = order[i];
			const int index = (int)i;
			if (ELEM(duplicates[index], -1, index)) {
				p.search = index;
				copy_v3_v3(p.search_co, tree->nodes[node_index].co);
				int found_prev = found;
				deduplicate_recursive(&p, tree->root);
				if (found != found_prev) {
					/* Prevent chains of doubles. */
					duplicates[index] = index;
				}
			}
		}
		MEM_freeN(order);
	}
	else {
		for (uint i = 0; i < tree->totnode; i++) {
			const uint node_index = i;
			const int index = p.nodes[node_index].index;
			if (ELEM(duplicates[index], -1, index)) {
				p.search = index;
				copy_v3_v3(p.search_co, tree->nodes[node_index].co);
				int found_prev = found;
				deduplicate_recursive(&p, tree->root);
				if (found != found_prev) {
					/* Prevent chains of doubles. */
					duplicates[index] = index;
				}
			}
		}
	}
	return found;
}

/** \} */
