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


typedef struct KDTreeNode {
	struct KDTreeNode *left, *right;
	float co[3], nor[3];
	int index;
	unsigned int d;  /* range is only (0-2) */
} KDTreeNode;

struct KDTree {
	KDTreeNode *nodes;
	unsigned int totnode;
	KDTreeNode *root;
#ifdef DEBUG
	bool is_balanced;  /* ensure we call balance first */
#endif
};

#define KD_STACK_INIT 100      /* initial size for array (on the stack) */
#define KD_NEAR_ALLOC_INC 100  /* alloc increment for collecting nearest */
#define KD_FOUND_ALLOC_INC 50  /* alloc increment for collecting nearest */

/**
 * Creates or free a kdtree
 */
KDTree *BLI_kdtree_new(unsigned int maxsize)
{
	KDTree *tree;

	tree = MEM_mallocN(sizeof(KDTree), "KDTree");
	tree->nodes = MEM_mallocN(sizeof(KDTreeNode) * maxsize, "KDTreeNode");
	tree->totnode = 0;
	tree->root = NULL;

#ifdef DEBUG
	tree->is_balanced = false;
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
void BLI_kdtree_insert(KDTree *tree, int index, const float co[3], const float nor[3])
{
	KDTreeNode *node = &tree->nodes[tree->totnode++];

	/* note, array isn't calloc'd,
	 * need to initialize all struct members */

	node->left = node->right = NULL;
	copy_v3_v3(node->co, co);
	if (nor)
		copy_v3_v3(node->nor, nor);
	else
		zero_v3(node->nor);

	node->index = index;
	node->d = 0;

#ifdef DEBUG
	tree->is_balanced = false;
#endif
}

static KDTreeNode *kdtree_balance(KDTreeNode *nodes, unsigned int totnode, unsigned int axis)
{
	KDTreeNode *node;
	float co;
	unsigned int left, right, median, i, j;

	if (totnode <= 0)
		return NULL;
	else if (totnode == 1)
		return nodes;
	
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

			SWAP(KDTreeNode, nodes[i], nodes[j]);
		}

		SWAP(KDTreeNode, nodes[i], nodes[right]);
		if (i >= median)
			right = i - 1;
		if (i <= median)
			left = i + 1;
	}

	/* set node and sort subnodes */
	node = &nodes[median];
	node->d = axis;
	node->left = kdtree_balance(nodes, median, (axis + 1) % 3);
	node->right = kdtree_balance(nodes + median + 1, (totnode - (median + 1)), (axis + 1) % 3);

	return node;
}

void BLI_kdtree_balance(KDTree *tree)
{
	tree->root = kdtree_balance(tree->nodes, tree->totnode, 0);

#ifdef DEBUG
	tree->is_balanced = true;
#endif
}

static float squared_distance(const float v2[3], const float v1[3], const float UNUSED(n1[3]), const float n2[3])
{
	float d[3], dist;

	d[0] = v2[0] - v1[0];
	d[1] = v2[1] - v1[1];
	d[2] = v2[2] - v1[2];

	dist = dot_v3v3(d, d);

	//if (n1 && n2 && (dot_v3v3(n1, n2) < 0.0f))

	/* can someone explain why this is done?*/
	if (n2 && (dot_v3v3(d, n2) < 0.0f)) {
		dist *= 10.0f;
	}

	return dist;
}

static KDTreeNode **realloc_nodes(KDTreeNode **stack, unsigned int *totstack, const bool is_alloc)
{
	KDTreeNode **stack_new = MEM_mallocN((*totstack + KD_NEAR_ALLOC_INC) * sizeof(KDTreeNode *), "KDTree.treestack");
	memcpy(stack_new, stack, *totstack * sizeof(KDTreeNode *));
	// memset(stack_new + *totstack, 0, sizeof(KDTreeNode *) * KD_NEAR_ALLOC_INC);
	if (is_alloc)
		MEM_freeN(stack);
	*totstack += KD_NEAR_ALLOC_INC;
	return stack_new;
}

/**
 * Find nearest returns index, and -1 if no node is found.
 */
int BLI_kdtree_find_nearest(KDTree *tree, const float co[3], const float nor[3],
                            KDTreeNearest *r_nearest)
{
	KDTreeNode *root, *node, *min_node;
	KDTreeNode **stack, *defaultstack[KD_STACK_INIT];
	float min_dist, cur_dist;
	unsigned int totstack, cur = 0;

#ifdef DEBUG
	BLI_assert(tree->is_balanced == true);
#endif

	if (!tree->root)
		return -1;

	stack = defaultstack;
	totstack = KD_STACK_INIT;

	root = tree->root;
	min_node = root;
	min_dist = squared_distance(root->co, co, root->nor, nor);

	if (co[root->d] < root->co[root->d]) {
		if (root->right)
			stack[cur++] = root->right;
		if (root->left)
			stack[cur++] = root->left;
	}
	else {
		if (root->left)
			stack[cur++] = root->left;
		if (root->right)
			stack[cur++] = root->right;
	}
	
	while (cur--) {
		node = stack[cur];

		cur_dist = node->co[node->d] - co[node->d];

		if (cur_dist < 0.0f) {
			cur_dist = -cur_dist * cur_dist;

			if (-cur_dist < min_dist) {
				cur_dist = squared_distance(node->co, co, node->nor, nor);
				if (cur_dist < min_dist) {
					min_dist = cur_dist;
					min_node = node;
				}
				if (node->left)
					stack[cur++] = node->left;
			}
			if (node->right)
				stack[cur++] = node->right;
		}
		else {
			cur_dist = cur_dist * cur_dist;

			if (cur_dist < min_dist) {
				cur_dist = squared_distance(node->co, co, node->nor, nor);
				if (cur_dist < min_dist) {
					min_dist = cur_dist;
					min_node = node;
				}
				if (node->right)
					stack[cur++] = node->right;
			}
			if (node->left)
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

static void add_nearest(KDTreeNearest *ptn, unsigned int *found, unsigned int n, int index,
                        float dist, const float *co)
{
	unsigned int i;

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
int BLI_kdtree_find_nearest_n(KDTree *tree, const float co[3], const float nor[3],
                              KDTreeNearest r_nearest[],
                              unsigned int n)
{
	KDTreeNode *root, *node = NULL;
	KDTreeNode **stack, *defaultstack[KD_STACK_INIT];
	float cur_dist;
	unsigned int totstack, cur = 0;
	unsigned int i, found = 0;

#ifdef DEBUG
	BLI_assert(tree->is_balanced == true);
#endif

	if (!tree->root || n == 0)
		return 0;

	stack = defaultstack;
	totstack = KD_STACK_INIT;

	root = tree->root;

	cur_dist = squared_distance(root->co, co, root->nor, nor);
	add_nearest(r_nearest, &found, n, root->index, cur_dist, root->co);
	
	if (co[root->d] < root->co[root->d]) {
		if (root->right)
			stack[cur++] = root->right;
		if (root->left)
			stack[cur++] = root->left;
	}
	else {
		if (root->left)
			stack[cur++] = root->left;
		if (root->right)
			stack[cur++] = root->right;
	}

	while (cur--) {
		node = stack[cur];

		cur_dist = node->co[node->d] - co[node->d];

		if (cur_dist < 0.0f) {
			cur_dist = -cur_dist * cur_dist;

			if (found < n || -cur_dist < r_nearest[found - 1].dist) {
				cur_dist = squared_distance(node->co, co, node->nor, nor);

				if (found < n || cur_dist < r_nearest[found - 1].dist)
					add_nearest(r_nearest, &found, n, node->index, cur_dist, node->co);

				if (node->left)
					stack[cur++] = node->left;
			}
			if (node->right)
				stack[cur++] = node->right;
		}
		else {
			cur_dist = cur_dist * cur_dist;

			if (found < n || cur_dist < r_nearest[found - 1].dist) {
				cur_dist = squared_distance(node->co, co, node->nor, nor);
				if (found < n || cur_dist < r_nearest[found - 1].dist)
					add_nearest(r_nearest, &found, n, node->index, cur_dist, node->co);

				if (node->right)
					stack[cur++] = node->right;
			}
			if (node->left)
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
static void add_in_range(KDTreeNearest **ptn, unsigned int found, unsigned int *totfoundstack, int index, float dist, float *co)
{
	KDTreeNearest *to;

	if (found >= *totfoundstack) {
		KDTreeNearest *temp = MEM_mallocN((*totfoundstack + KD_FOUND_ALLOC_INC) * sizeof(KDTreeNode), "KDTree.treefoundstack");
		memcpy(temp, *ptn, *totfoundstack * sizeof(KDTreeNearest));
		if (*ptn)
			MEM_freeN(*ptn);
		*ptn = temp;
		*totfoundstack += KD_FOUND_ALLOC_INC;
	}

	to = (*ptn) + found;

	to->index = index;
	to->dist = sqrtf(dist);
	copy_v3_v3(to->co, co);
}

/**
 * Range search returns number of points found, with results in nearest
 * Normal is optional, but if given will limit results to points in normal direction from co.
 * Remember to free nearest after use!
 */
int BLI_kdtree_range_search(KDTree *tree, const float co[3], const float nor[3],
                            KDTreeNearest **r_nearest, float range)
{
	KDTreeNode *root, *node = NULL;
	KDTreeNode **stack, *defaultstack[KD_STACK_INIT];
	KDTreeNearest *foundstack = NULL;
	float range2 = range * range, dist2;
	unsigned int totstack, cur = 0, found = 0, totfoundstack = 0;

#ifdef DEBUG
	BLI_assert(tree->is_balanced == true);
#endif

	if (!tree->root)
		return 0;

	stack = defaultstack;
	totstack = KD_STACK_INIT;

	root = tree->root;

	if (co[root->d] + range < root->co[root->d]) {
		if (root->left)
			stack[cur++] = root->left;
	}
	else if (co[root->d] - range > root->co[root->d]) {
		if (root->right)
			stack[cur++] = root->right;
	}
	else {
		dist2 = squared_distance(root->co, co, root->nor, nor);
		if (dist2 <= range2)
			add_in_range(&foundstack, found++, &totfoundstack, root->index, dist2, root->co);

		if (root->left)
			stack[cur++] = root->left;
		if (root->right)
			stack[cur++] = root->right;
	}

	while (cur--) {
		node = stack[cur];

		if (co[node->d] + range < node->co[node->d]) {
			if (node->left)
				stack[cur++] = node->left;
		}
		else if (co[node->d] - range > node->co[node->d]) {
			if (node->right)
				stack[cur++] = node->right;
		}
		else {
			dist2 = squared_distance(node->co, co, node->nor, nor);
			if (dist2 <= range2)
				add_in_range(&foundstack, found++, &totfoundstack, node->index, dist2, node->co);

			if (node->left)
				stack[cur++] = node->left;
			if (node->right)
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
