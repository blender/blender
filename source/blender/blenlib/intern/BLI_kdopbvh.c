/**
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich, Andre Pinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "math.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_kdopbvh.h"
#include "BLI_math.h"

#ifdef _OPENMP
#include <omp.h>
#endif



#define MAX_TREETYPE 32
#define DEFAULT_FIND_NEAREST_HEAP_SIZE 1024

typedef struct BVHNode
{
	struct BVHNode **children;
	struct BVHNode *parent; // some user defined traversed need that
	struct BVHNode *skip[2];
	float *bv;		// Bounding volume of all nodes, max 13 axis
	int index;		// face, edge, vertex index
	char totnode;	// how many nodes are used, used for speedup
	char main_axis;	// Axis used to split this node
} BVHNode;

struct BVHTree
{
	BVHNode **nodes;
	BVHNode *nodearray; /* pre-alloc branch nodes */
	BVHNode **nodechild;	// pre-alloc childs for nodes
	float	*nodebv;		// pre-alloc bounding-volumes for nodes
	float 	epsilon; /* epslion is used for inflation of the k-dop	   */
	int 	totleaf; // leafs
	int 	totbranch;
	char 	tree_type; // type of tree (4 => quadtree)
	char 	axis; // kdop type (6 => OBB, 7 => AABB, ...)
	char 	start_axis, stop_axis; // KDOP_AXES array indices according to axis
};

typedef struct BVHOverlapData 
{  
	BVHTree *tree1, *tree2; 
	BVHTreeOverlap *overlap; 
	int i, max_overlap; /* i is number of overlaps */
	int start_axis, stop_axis;
} BVHOverlapData;

typedef struct BVHNearestData
{
	BVHTree *tree;
	const float	*co;
	BVHTree_NearestPointCallback callback;
	void	*userdata;
	float proj[13];			//coordinates projection over axis
	BVHTreeNearest nearest;

} BVHNearestData;

typedef struct BVHRayCastData
{
	BVHTree *tree;

	BVHTree_RayCastCallback callback;
	void	*userdata;


	BVHTreeRay    ray;
	float ray_dot_axis[13];
	float idot_axis[13];
	int index[6];

	BVHTreeRayHit hit;
} BVHRayCastData;
////////////////////////////////////////m


////////////////////////////////////////////////////////////////////////
// Bounding Volume Hierarchy Definition
// 
// Notes: From OBB until 26-DOP --> all bounding volumes possible, just choose type below
// Notes: You have to choose the type at compile time ITM
// Notes: You can choose the tree type --> binary, quad, octree, choose below
////////////////////////////////////////////////////////////////////////

static float KDOP_AXES[13][3] =
{ {1.0, 0, 0}, {0, 1.0, 0}, {0, 0, 1.0}, {1.0, 1.0, 1.0}, {1.0, -1.0, 1.0}, {1.0, 1.0, -1.0},
{1.0, -1.0, -1.0}, {1.0, 1.0, 0}, {1.0, 0, 1.0}, {0, 1.0, 1.0}, {1.0, -1.0, 0}, {1.0, 0, -1.0},
{0, 1.0, -1.0}
};

/*
 * Generic push and pop heap
 */
#define PUSH_HEAP_BODY(HEAP_TYPE,PRIORITY,heap,heap_size)	\
{													\
	HEAP_TYPE element = heap[heap_size-1];			\
	int child = heap_size-1;						\
	while(child != 0)								\
	{												\
		int parent = (child-1) / 2;					\
		if(PRIORITY(element, heap[parent]))			\
		{											\
			heap[child] = heap[parent];				\
			child = parent;							\
		}											\
		else break;									\
	}												\
	heap[child] = element;							\
}

#define POP_HEAP_BODY(HEAP_TYPE, PRIORITY,heap,heap_size)	\
{													\
	HEAP_TYPE element = heap[heap_size-1];			\
	int parent = 0;									\
	while(parent < (heap_size-1)/2 )				\
	{												\
		int child2 = (parent+1)*2;					\
		if(PRIORITY(heap[child2-1], heap[child2]))	\
			--child2;								\
													\
		if(PRIORITY(element, heap[child2]))			\
			break;									\
													\
		heap[parent] = heap[child2];				\
		parent = child2;							\
	}												\
	heap[parent] = element;							\
}

int ADJUST_MEMORY(void *local_memblock, void **memblock, int new_size, int *max_size, int size_per_item)
{
	int   new_max_size = *max_size * 2;
	void *new_memblock = NULL;

	if(new_size <= *max_size)
		return TRUE;

	if(*memblock == local_memblock)
	{
		new_memblock = malloc( size_per_item * new_max_size );
		memcpy( new_memblock, *memblock, size_per_item * *max_size );
	}
	else
		new_memblock = realloc(*memblock, size_per_item * new_max_size );

	if(new_memblock)
	{
		*memblock = new_memblock;
		*max_size = new_max_size;
		return TRUE;
	}
	else
		return FALSE;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Introsort 
// with permission deriven from the following Java code:
// http://ralphunden.net/content/tutorials/a-guide-to-introsort/
// and he derived it from the SUN STL 
//////////////////////////////////////////////////////////////////////////////////////////////////////
static int size_threshold = 16;
/*
* Common methods for all algorithms
*/
static int floor_lg(int a)
{
	return (int)(floor(log(a)/log(2)));
}

/*
* Insertion sort algorithm
*/
static void bvh_insertionsort(BVHNode **a, int lo, int hi, int axis)
{
	int i,j;
	BVHNode *t;
	for (i=lo; i < hi; i++)
	{
		j=i;
		t = a[i];
		while((j!=lo) && (t->bv[axis] < (a[j-1])->bv[axis]))
		{
			a[j] = a[j-1];
			j--;
		}
		a[j] = t;
	}
}

static int bvh_partition(BVHNode **a, int lo, int hi, BVHNode * x, int axis)
{
	int i=lo, j=hi;
	while (1)
	{
		while ((a[i])->bv[axis] < x->bv[axis]) i++;
		j--;
		while (x->bv[axis] < (a[j])->bv[axis]) j--;
		if(!(i < j))
			return i;
		SWAP( BVHNode* , a[i], a[j]);
		i++;
	}
}

/*
* Heapsort algorithm
*/
static void bvh_downheap(BVHNode **a, int i, int n, int lo, int axis)
{
	BVHNode * d = a[lo+i-1];
	int child;
	while (i<=n/2)
	{
		child = 2*i;
		if ((child < n) && ((a[lo+child-1])->bv[axis] < (a[lo+child])->bv[axis]))
		{
			child++;
		}
		if (!(d->bv[axis] < (a[lo+child-1])->bv[axis])) break;
		a[lo+i-1] = a[lo+child-1];
		i = child;
	}
	a[lo+i-1] = d;
}

static void bvh_heapsort(BVHNode **a, int lo, int hi, int axis)
{
	int n = hi-lo, i;
	for (i=n/2; i>=1; i=i-1)
	{
		bvh_downheap(a, i,n,lo, axis);
	}
	for (i=n; i>1; i=i-1)
	{
		SWAP(BVHNode*, a[lo],a[lo+i-1]);
		bvh_downheap(a, 1,i-1,lo, axis);
	}
}

static BVHNode *bvh_medianof3(BVHNode **a, int lo, int mid, int hi, int axis) // returns Sortable
{
	if ((a[mid])->bv[axis] < (a[lo])->bv[axis])
	{
		if ((a[hi])->bv[axis] < (a[mid])->bv[axis])
			return a[mid];
		else
		{
			if ((a[hi])->bv[axis] < (a[lo])->bv[axis])
				return a[hi];
			else
				return a[lo];
		}
	}
	else
	{
		if ((a[hi])->bv[axis] < (a[mid])->bv[axis])
		{
			if ((a[hi])->bv[axis] < (a[lo])->bv[axis])
				return a[lo];
			else
				return a[hi];
		}
		else
			return a[mid];
	}
}
/*
* Quicksort algorithm modified for Introsort
*/
static void bvh_introsort_loop (BVHNode **a, int lo, int hi, int depth_limit, int axis)
{
	int p;

	while (hi-lo > size_threshold)
	{
		if (depth_limit == 0)
		{
			bvh_heapsort(a, lo, hi, axis);
			return;
		}
		depth_limit=depth_limit-1;
		p=bvh_partition(a, lo, hi, bvh_medianof3(a, lo, lo+((hi-lo)/2)+1, hi-1, axis), axis);
		bvh_introsort_loop(a, p, hi, depth_limit, axis);
		hi=p;
	}
}

static void sort(BVHNode **a0, int begin, int end, int axis)
{
	if (begin < end)
	{
		BVHNode **a=a0;
		bvh_introsort_loop(a, begin, end, 2*floor_lg(end-begin), axis);
		bvh_insertionsort(a, begin, end, axis);
	}
}
static void sort_along_axis(BVHTree *tree, int start, int end, int axis)
{
	sort(tree->nodes, start, end, axis);
}

//after a call to this function you can expect one of:
//      every node to left of a[n] are smaller or equal to it
//      every node to the right of a[n] are greater or equal to it
static int partition_nth_element(BVHNode **a, int _begin, int _end, int n, int axis){
	int begin = _begin, end = _end, cut;
	while(end-begin > 3)
	{
		cut = bvh_partition(a, begin, end, bvh_medianof3(a, begin, (begin+end)/2, end-1, axis), axis );
		if(cut <= n)
			begin = cut;
		else
			end = cut;
	}
	bvh_insertionsort(a, begin, end, axis);

	return n;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
static void build_skip_links(BVHTree *tree, BVHNode *node, BVHNode *left, BVHNode *right)
{
	int i;
	
	node->skip[0] = left;
	node->skip[1] = right;
	
	for (i = 0; i < node->totnode; i++)
	{
		if(i+1 < node->totnode)
			build_skip_links(tree, node->children[i], left, node->children[i+1] );
		else
			build_skip_links(tree, node->children[i], left, right );

		left = node->children[i];
	}
}

/*
 * BVHTree bounding volumes functions
 */
static void create_kdop_hull(BVHTree *tree, BVHNode *node, float *co, int numpoints, int moving)
{
	float newminmax;
	float *bv = node->bv;
	int i, k;
	
	// don't init boudings for the moving case
	if(!moving)
	{
		for (i = tree->start_axis; i < tree->stop_axis; i++)
		{
			bv[2*i] = FLT_MAX;
			bv[2*i + 1] = -FLT_MAX;
		}
	}
	
	for(k = 0; k < numpoints; k++)
	{
		// for all Axes.
		for (i = tree->start_axis; i < tree->stop_axis; i++)
		{
			newminmax = INPR(&co[k * 3], KDOP_AXES[i]);
			if (newminmax < bv[2 * i])
				bv[2 * i] = newminmax;
			if (newminmax > bv[(2 * i) + 1])
				bv[(2 * i) + 1] = newminmax;
		}
	}
}

// depends on the fact that the BVH's for each face is already build
static void refit_kdop_hull(BVHTree *tree, BVHNode *node, int start, int end)
{
	float newmin,newmax;
	int i, j;
	float *bv = node->bv;

	
	for (i = tree->start_axis; i < tree->stop_axis; i++)
	{
		bv[2*i] = FLT_MAX;
		bv[2*i + 1] = -FLT_MAX;
	}

	for (j = start; j < end; j++)
	{
// for all Axes.
		for (i = tree->start_axis; i < tree->stop_axis; i++)
		{
			newmin = tree->nodes[j]->bv[(2 * i)];   
			if ((newmin < bv[(2 * i)]))
				bv[(2 * i)] = newmin;
 
			newmax = tree->nodes[j]->bv[(2 * i) + 1];
			if ((newmax > bv[(2 * i) + 1]))
				bv[(2 * i) + 1] = newmax;
		}
	}

}

// only supports x,y,z axis in the moment
// but we should use a plain and simple function here for speed sake
static char get_largest_axis(float *bv)
{
	float middle_point[3];

	middle_point[0] = (bv[1]) - (bv[0]); // x axis
	middle_point[1] = (bv[3]) - (bv[2]); // y axis
	middle_point[2] = (bv[5]) - (bv[4]); // z axis
	if (middle_point[0] > middle_point[1]) 
	{
		if (middle_point[0] > middle_point[2])
			return 1; // max x axis
		else
			return 5; // max z axis
	}
	else 
	{
		if (middle_point[1] > middle_point[2])
			return 3; // max y axis
		else
			return 5; // max z axis
	}
}

// bottom-up update of bvh node BV
// join the children on the parent BV
static void node_join(BVHTree *tree, BVHNode *node)
{
	int i, j;
	
	for (i = tree->start_axis; i < tree->stop_axis; i++)
	{
		node->bv[2*i] = FLT_MAX;
		node->bv[2*i + 1] = -FLT_MAX;
	}
	
	for (i = 0; i < tree->tree_type; i++)
	{
		if (node->children[i]) 
		{
			for (j = tree->start_axis; j < tree->stop_axis; j++)
			{
				// update minimum 
				if (node->children[i]->bv[(2 * j)] < node->bv[(2 * j)]) 
					node->bv[(2 * j)] = node->children[i]->bv[(2 * j)];
				
				// update maximum 
				if (node->children[i]->bv[(2 * j) + 1] > node->bv[(2 * j) + 1])
					node->bv[(2 * j) + 1] = node->children[i]->bv[(2 * j) + 1];
			}
		}
		else
			break;
	}
}

/*
 * Debug and information functions
 */
#if 0
static void bvhtree_print_tree(BVHTree *tree, BVHNode *node, int depth)
{
	int i;
	for(i=0; i<depth; i++) printf(" ");
	printf(" - %d (%ld): ", node->index, node - tree->nodearray);
	for(i=2*tree->start_axis; i<2*tree->stop_axis; i++)
		printf("%.3f ", node->bv[i]);
	printf("\n");

	for(i=0; i<tree->tree_type; i++)
		if(node->children[i])
			bvhtree_print_tree(tree, node->children[i], depth+1);
}

static void bvhtree_info(BVHTree *tree)
{
	printf("BVHTree info\n");
	printf("tree_type = %d, axis = %d, epsilon = %f\n", tree->tree_type, tree->axis, tree->epsilon);
	printf("nodes = %d, branches = %d, leafs = %d\n", tree->totbranch + tree->totleaf,  tree->totbranch, tree->totleaf);
	printf("Memory per node = %ldbytes\n", sizeof(BVHNode) + sizeof(BVHNode*)*tree->tree_type + sizeof(float)*tree->axis);
	printf("BV memory = %dbytes\n", MEM_allocN_len(tree->nodebv));

	printf("Total memory = %ldbytes\n", sizeof(BVHTree)
		+ MEM_allocN_len(tree->nodes)
		+ MEM_allocN_len(tree->nodearray)
		+ MEM_allocN_len(tree->nodechild)
		+ MEM_allocN_len(tree->nodebv)
		);

//	bvhtree_print_tree(tree, tree->nodes[tree->totleaf], 0);
}
#endif

#if 0


static void verify_tree(BVHTree *tree)
{
	int i, j, check = 0;
	
	// check the pointer list
	for(i = 0; i < tree->totleaf; i++)
	{
		if(tree->nodes[i]->parent == NULL)
			printf("Leaf has no parent: %d\n", i);
		else
		{
			for(j = 0; j < tree->tree_type; j++)
			{
				if(tree->nodes[i]->parent->children[j] == tree->nodes[i])
					check = 1;
			}
			if(!check)
			{
				printf("Parent child relationship doesn't match: %d\n", i);
			}
			check = 0;
		}
	}
	
	// check the leaf list
	for(i = 0; i < tree->totleaf; i++)
	{
		if(tree->nodearray[i].parent == NULL)
			printf("Leaf has no parent: %d\n", i);
		else
		{
			for(j = 0; j < tree->tree_type; j++)
			{
				if(tree->nodearray[i].parent->children[j] == &tree->nodearray[i])
					check = 1;
			}
			if(!check)
			{
				printf("Parent child relationship doesn't match: %d\n", i);
			}
			check = 0;
		}
	}
	
	printf("branches: %d, leafs: %d, total: %d\n", tree->totbranch, tree->totleaf, tree->totbranch + tree->totleaf);
}
#endif

//Helper data and structures to build a min-leaf generalized implicit tree
//This code can be easily reduced (basicly this is only method to calculate pow(k, n) in O(1).. and stuff like that)
typedef struct BVHBuildHelper
{
	int tree_type;				//
	int totleafs;				//

	int leafs_per_child  [32];	//Min number of leafs that are archievable from a node at depth N
	int branches_on_level[32];	//Number of nodes at depth N (tree_type^N)

	int remain_leafs;			//Number of leafs that are placed on the level that is not 100% filled

} BVHBuildHelper;

static void build_implicit_tree_helper(BVHTree *tree, BVHBuildHelper *data)
{
	int depth = 0;
	int remain;
	int nnodes;

	data->totleafs = tree->totleaf;
	data->tree_type= tree->tree_type;

	//Calculate the smallest tree_type^n such that tree_type^n >= num_leafs
	for(
		data->leafs_per_child[0] = 1;
		data->leafs_per_child[0] <  data->totleafs;
		data->leafs_per_child[0] *= data->tree_type
	);

	data->branches_on_level[0] = 1;

	//We could stop the loop first (but I am lazy to find out when)
	for(depth = 1; depth < 32; depth++)
	{
		data->branches_on_level[depth] = data->branches_on_level[depth-1] * data->tree_type;
		data->leafs_per_child  [depth] = data->leafs_per_child  [depth-1] / data->tree_type;
	}

	remain = data->totleafs - data->leafs_per_child[1];
	nnodes = (remain + data->tree_type - 2) / (data->tree_type - 1);
	data->remain_leafs = remain + nnodes;
}

// return the min index of all the leafs archivable with the given branch
static int implicit_leafs_index(BVHBuildHelper *data, int depth, int child_index)
{
	int min_leaf_index = child_index * data->leafs_per_child[depth-1];
	if(min_leaf_index <= data->remain_leafs)
		return min_leaf_index;
	else if(data->leafs_per_child[depth])
		return data->totleafs - (data->branches_on_level[depth-1] - child_index) * data->leafs_per_child[depth];
	else
		return data->remain_leafs;
}

/**
 * Generalized implicit tree build
 *
 * An implicit tree is a tree where its structure is implied, thus there is no need to store child pointers or indexs.
 * Its possible to find the position of the child or the parent with simple maths (multiplication and adittion). This type
 * of tree is for example used on heaps.. where node N has its childs at indexs N*2 and N*2+1.
 *
 * Altought in this case the tree type is general.. and not know until runtime.
 * tree_type stands for the maximum number of childs that a tree node can have.
 * All tree types >= 2 are supported.
 *
 * Advantages of the used trees include:
 *  - No need to store child/parent relations (they are implicit);
 *  - Any node child always has an index greater than the parent;
 *  - Brother nodes are sequencial in memory;
 *
 *
 * Some math relations derived for general implicit trees:
 *
 *   K = tree_type, ( 2 <= K )
 *   ROOT = 1
 *   N child of node A = A * K + (2 - K) + N, (0 <= N < K)
 *
 * Util methods:
 *   TODO...
 *    (looping elements, knowing if its a leaf or not.. etc...)
 */

// This functions returns the number of branches needed to have the requested number of leafs.
static int implicit_needed_branches(int tree_type, int leafs)
{
	return MAX2(1, (leafs + tree_type - 3) / (tree_type-1) );
}

/*
 * This function handles the problem of "sorting" the leafs (along the split_axis).
 *
 * It arranges the elements in the given partitions such that:
 *  - any element in partition N is less or equal to any element in partition N+1.
 *  - if all elements are diferent all partition will get the same subset of elements
 *    as if the array was sorted.
 *
 * partition P is described as the elements in the range ( nth[P] , nth[P+1] ]
 *
 * TODO: This can be optimized a bit by doing a specialized nth_element instead of K nth_elements
 */
static void split_leafs(BVHNode **leafs_array, int *nth, int partitions, int split_axis)
{
	int i;
	for(i=0; i < partitions-1; i++)
	{
		if(nth[i] >= nth[partitions])
			break;

		partition_nth_element(leafs_array, nth[i], nth[partitions], nth[i+1], split_axis);
	}
}

/*
 * This functions builds an optimal implicit tree from the given leafs.
 * Where optimal stands for:
 *  - The resulting tree will have the smallest number of branches;
 *  - At most only one branch will have NULL childs;
 *  - All leafs will be stored at level N or N+1.
 *
 * This function creates an implicit tree on branches_array, the leafs are given on the leafs_array.
 *
 * The tree is built per depth levels. First branchs at depth 1.. then branches at depth 2.. etc..
 * The reason is that we can build level N+1 from level N witouth any data dependencies.. thus it allows
 * to use multithread building.
 *
 * To archieve this is necessary to find how much leafs are accessible from a certain branch, BVHBuildHelper
 * implicit_needed_branches and implicit_leafs_index are auxiliar functions to solve that "optimal-split".
 */
static void non_recursive_bvh_div_nodes(BVHTree *tree, BVHNode *branches_array, BVHNode **leafs_array, int num_leafs)
{
	int i;

	const int tree_type   = tree->tree_type;
	const int tree_offset = 2 - tree->tree_type; //this value is 0 (on binary trees) and negative on the others
	const int num_branches= implicit_needed_branches(tree_type, num_leafs);

	BVHBuildHelper data;
	int depth;
	
	// set parent from root node to NULL
	BVHNode *tmp = branches_array+0;
	tmp->parent = NULL;

	//Most of bvhtree code relies on 1-leaf trees having at least one branch
	//We handle that special case here
	if(num_leafs == 1)
	{
		BVHNode *root = branches_array+0;
		refit_kdop_hull(tree, root, 0, num_leafs);
		root->main_axis = get_largest_axis(root->bv) / 2;
		root->totnode = 1;
		root->children[0] = leafs_array[0];
		root->children[0]->parent = root;
		return;
	}

	branches_array--;	//Implicit trees use 1-based indexs
	
	build_implicit_tree_helper(tree, &data);

	//Loop tree levels (log N) loops
	for(i=1, depth = 1; i <= num_branches; i = i*tree_type + tree_offset, depth++)
	{
		const int first_of_next_level = i*tree_type + tree_offset;
		const int  end_j = MIN2(first_of_next_level, num_branches + 1);	//index of last branch on this level
		int j;

		//Loop all branches on this level
#pragma omp parallel for private(j) schedule(static)
		for(j = i; j < end_j; j++)
		{
			int k;
			const int parent_level_index= j-i;
			BVHNode* parent = branches_array + j;
			int nth_positions[ MAX_TREETYPE + 1];
			char split_axis;

			int parent_leafs_begin = implicit_leafs_index(&data, depth, parent_level_index);
			int parent_leafs_end   = implicit_leafs_index(&data, depth, parent_level_index+1);

			//This calculates the bounding box of this branch
			//and chooses the largest axis as the axis to divide leafs
			refit_kdop_hull(tree, parent, parent_leafs_begin, parent_leafs_end);
			split_axis = get_largest_axis(parent->bv);

			//Save split axis (this can be used on raytracing to speedup the query time)
			parent->main_axis = split_axis / 2;

			//Split the childs along the split_axis, note: its not needed to sort the whole leafs array
			//Only to assure that the elements are partioned on a way that each child takes the elements
			//it would take in case the whole array was sorted.
			//Split_leafs takes care of that "sort" problem.
			nth_positions[        0] = parent_leafs_begin;
			nth_positions[tree_type] = parent_leafs_end;
			for(k = 1; k < tree_type; k++)
			{
				int child_index = j * tree_type + tree_offset + k;
				int child_level_index = child_index - first_of_next_level; //child level index
				nth_positions[k] = implicit_leafs_index(&data, depth+1, child_level_index);
			}

			split_leafs(leafs_array, nth_positions, tree_type, split_axis);


			//Setup children and totnode counters
			//Not really needed but currently most of BVH code relies on having an explicit children structure
			for(k = 0; k < tree_type; k++)
			{
				int child_index = j * tree_type + tree_offset + k;
				int child_level_index = child_index - first_of_next_level; //child level index

				int child_leafs_begin = implicit_leafs_index(&data, depth+1, child_level_index);
				int child_leafs_end   = implicit_leafs_index(&data, depth+1, child_level_index+1);

				if(child_leafs_end - child_leafs_begin > 1)
				{
					parent->children[k] = branches_array + child_index;
					parent->children[k]->parent = parent;
				}
				else if(child_leafs_end - child_leafs_begin == 1)
				{
					parent->children[k] = leafs_array[ child_leafs_begin ];
					parent->children[k]->parent = parent;
				}
				else
					break;

				parent->totnode = k+1;
			}
		}
	}
}


/*
 * BLI_bvhtree api
 */
BVHTree *BLI_bvhtree_new(int maxsize, float epsilon, char tree_type, char axis)
{
	BVHTree *tree;
	int numnodes, i;
	
	// theres not support for trees below binary-trees :P
	if(tree_type < 2)
		return NULL;
	
	if(tree_type > MAX_TREETYPE)
		return NULL;

	tree = (BVHTree *)MEM_callocN(sizeof(BVHTree), "BVHTree");

	//tree epsilon must be >= FLT_EPSILON
	//so that tangent rays can still hit a bounding volume..
	//this bug would show up when casting a ray aligned with a kdop-axis and with an edge of 2 faces
	epsilon = MAX2(FLT_EPSILON, epsilon);
	
	if(tree)
	{
		tree->epsilon = epsilon;
		tree->tree_type = tree_type; 
		tree->axis = axis;
		
		if(axis == 26)
		{
			tree->start_axis = 0;
			tree->stop_axis = 13;
		}
		else if(axis == 18)
		{
			tree->start_axis = 7;
			tree->stop_axis = 13;
		}
		else if(axis == 14)
		{
			tree->start_axis = 0;
			tree->stop_axis = 7;
		}
		else if(axis == 8) // AABB
		{
			tree->start_axis = 0;
			tree->stop_axis = 4;
		}
		else if(axis == 6) // OBB
		{
			tree->start_axis = 0;
			tree->stop_axis = 3;
		}
		else
		{
			MEM_freeN(tree);
			return NULL;
		}


		//Allocate arrays
		numnodes = maxsize + implicit_needed_branches(tree_type, maxsize) + tree_type;

		tree->nodes = (BVHNode **)MEM_callocN(sizeof(BVHNode *)*numnodes, "BVHNodes");
		
		if(!tree->nodes)
		{
			MEM_freeN(tree);
			return NULL;
		}
		
		tree->nodebv = (float*)MEM_callocN(sizeof(float)* axis * numnodes, "BVHNodeBV");
		if(!tree->nodebv)
		{
			MEM_freeN(tree->nodes);
			MEM_freeN(tree);
		}

		tree->nodechild = (BVHNode**)MEM_callocN(sizeof(BVHNode*) * tree_type * numnodes, "BVHNodeBV");
		if(!tree->nodechild)
		{
			MEM_freeN(tree->nodebv);
			MEM_freeN(tree->nodes);
			MEM_freeN(tree);
		}

		tree->nodearray = (BVHNode *)MEM_callocN(sizeof(BVHNode)* numnodes, "BVHNodeArray");
		
		if(!tree->nodearray)
		{
			MEM_freeN(tree->nodechild);
			MEM_freeN(tree->nodebv);
			MEM_freeN(tree->nodes);
			MEM_freeN(tree);
			return NULL;
		}

		//link the dynamic bv and child links
		for(i=0; i< numnodes; i++)
		{
			tree->nodearray[i].bv = tree->nodebv + i * axis;
			tree->nodearray[i].children = tree->nodechild + i * tree_type;
		}
		
	}

	return tree;
}

void BLI_bvhtree_free(BVHTree *tree)
{	
	if(tree)
	{
		MEM_freeN(tree->nodes);
		MEM_freeN(tree->nodearray);
		MEM_freeN(tree->nodebv);
		MEM_freeN(tree->nodechild);
		MEM_freeN(tree);
	}
}

void BLI_bvhtree_balance(BVHTree *tree)
{
	int i;

	BVHNode*  branches_array = tree->nodearray + tree->totleaf;
	BVHNode** leafs_array    = tree->nodes;

	//This function should only be called once (some big bug goes here if its being called more than once per tree)
	assert(tree->totbranch == 0);

	//Build the implicit tree
	non_recursive_bvh_div_nodes(tree, branches_array, leafs_array, tree->totleaf);

	//current code expects the branches to be linked to the nodes array
	//we perform that linkage here
	tree->totbranch = implicit_needed_branches(tree->tree_type, tree->totleaf);
	for(i = 0; i < tree->totbranch; i++)
		tree->nodes[tree->totleaf + i] = branches_array + i;

	build_skip_links(tree, tree->nodes[tree->totleaf], NULL, NULL);
	//bvhtree_info(tree);
}

int BLI_bvhtree_insert(BVHTree *tree, int index, float *co, int numpoints)
{
	int i;
	BVHNode *node = NULL;
	
	// insert should only possible as long as tree->totbranch is 0
	if(tree->totbranch > 0)
		return 0;
	
	if(tree->totleaf+1 >= MEM_allocN_len(tree->nodes)/sizeof(*(tree->nodes)))
		return 0;
	
	// TODO check if have enough nodes in array
	
	node = tree->nodes[tree->totleaf] = &(tree->nodearray[tree->totleaf]);
	tree->totleaf++;
	
	create_kdop_hull(tree, node, co, numpoints, 0);
	node->index= index;
	
	// inflate the bv with some epsilon
	for (i = tree->start_axis; i < tree->stop_axis; i++)
	{
		node->bv[(2 * i)] -= tree->epsilon; // minimum 
		node->bv[(2 * i) + 1] += tree->epsilon; // maximum 
	}

	return 1;
}


// call before BLI_bvhtree_update_tree()
int BLI_bvhtree_update_node(BVHTree *tree, int index, float *co, float *co_moving, int numpoints)
{
	int i;
	BVHNode *node= NULL;
	
	// check if index exists
	if(index > tree->totleaf)
		return 0;
	
	node = tree->nodearray + index;
	
	create_kdop_hull(tree, node, co, numpoints, 0);
	
	if(co_moving)
		create_kdop_hull(tree, node, co_moving, numpoints, 1);
	
	// inflate the bv with some epsilon
	for (i = tree->start_axis; i < tree->stop_axis; i++)
	{
		node->bv[(2 * i)] -= tree->epsilon; // minimum 
		node->bv[(2 * i) + 1] += tree->epsilon; // maximum 
	}

	return 1;
}

// call BLI_bvhtree_update_node() first for every node/point/triangle
void BLI_bvhtree_update_tree(BVHTree *tree)
{
	//Update bottom=>top
	//TRICKY: the way we build the tree all the childs have an index greater than the parent
	//This allows us todo a bottom up update by starting on the biger numbered branch

	BVHNode** root  = tree->nodes + tree->totleaf;
	BVHNode** index = tree->nodes + tree->totleaf + tree->totbranch-1;

	for (; index >= root; index--)
		node_join(tree, *index);
}

float BLI_bvhtree_getepsilon(BVHTree *tree)
{
	return tree->epsilon;
}


/*
 * BLI_bvhtree_overlap
 */
// overlap - is it possbile for 2 bv's to collide ?
static int tree_overlap(BVHNode *node1, BVHNode *node2, int start_axis, int stop_axis)
{
	float *bv1 = node1->bv;
	float *bv2 = node2->bv;

	float *bv1_end = bv1 + (stop_axis<<1);
		
	bv1 += start_axis<<1;
	bv2 += start_axis<<1;
	
	// test all axis if min + max overlap
	for (; bv1 != bv1_end; bv1+=2, bv2+=2)
	{
		if ((*(bv1) > *(bv2 + 1)) || (*(bv2) > *(bv1 + 1))) 
			return 0;
	}
	
	return 1;
}

static void traverse(BVHOverlapData *data, BVHNode *node1, BVHNode *node2)
{
	int j;
	
	if(tree_overlap(node1, node2, data->start_axis, data->stop_axis))
	{
		// check if node1 is a leaf
		if(!node1->totnode)
		{
			// check if node2 is a leaf
			if(!node2->totnode)
			{
				
				if(node1 == node2)
				{
					return;
				}
					
				if(data->i >= data->max_overlap)
				{	
					// try to make alloc'ed memory bigger
					data->overlap = realloc(data->overlap, sizeof(BVHTreeOverlap)*data->max_overlap*2);
					
					if(!data->overlap)
					{
						printf("Out of Memory in traverse\n");
						return;
					}
					data->max_overlap *= 2;
				}
				
				// both leafs, insert overlap!
				data->overlap[data->i].indexA = node1->index;
				data->overlap[data->i].indexB = node2->index;

				data->i++;
			}
			else
			{
				for(j = 0; j < data->tree2->tree_type; j++)
				{
					if(node2->children[j])
						traverse(data, node1, node2->children[j]);
				}
			}
		}
		else
		{
			
			for(j = 0; j < data->tree2->tree_type; j++)
			{
				if(node1->children[j])
					traverse(data, node1->children[j], node2);
			}
		}
	}
	return;
}

BVHTreeOverlap *BLI_bvhtree_overlap(BVHTree *tree1, BVHTree *tree2, int *result)
{
	int j, total = 0;
	BVHTreeOverlap *overlap = NULL, *to = NULL;
	BVHOverlapData **data;
	
	// check for compatibility of both trees (can't compare 14-DOP with 18-DOP)
	if((tree1->axis != tree2->axis) && (tree1->axis == 14 || tree2->axis == 14) && (tree1->axis == 18 || tree2->axis == 18))
		return 0;
	
	// fast check root nodes for collision before doing big splitting + traversal
	if(!tree_overlap(tree1->nodes[tree1->totleaf], tree2->nodes[tree2->totleaf], MIN2(tree1->start_axis, tree2->start_axis), MIN2(tree1->stop_axis, tree2->stop_axis)))
		return 0;

	data = MEM_callocN(sizeof(BVHOverlapData *)* tree1->tree_type, "BVHOverlapData_star");
	
	for(j = 0; j < tree1->tree_type; j++)
	{
		data[j] = (BVHOverlapData *)MEM_callocN(sizeof(BVHOverlapData), "BVHOverlapData");
		
		// init BVHOverlapData
		data[j]->overlap = (BVHTreeOverlap *)malloc(sizeof(BVHTreeOverlap)*MAX2(tree1->totleaf, tree2->totleaf));
		data[j]->tree1 = tree1;
		data[j]->tree2 = tree2;
		data[j]->max_overlap = MAX2(tree1->totleaf, tree2->totleaf);
		data[j]->i = 0;
		data[j]->start_axis = MIN2(tree1->start_axis, tree2->start_axis);
		data[j]->stop_axis  = MIN2(tree1->stop_axis,  tree2->stop_axis );
	}

#pragma omp parallel for private(j) schedule(static)
	for(j = 0; j < MIN2(tree1->tree_type, tree1->nodes[tree1->totleaf]->totnode); j++)
	{
		traverse(data[j], tree1->nodes[tree1->totleaf]->children[j], tree2->nodes[tree2->totleaf]);
	}
	
	for(j = 0; j < tree1->tree_type; j++)
		total += data[j]->i;
	
	to = overlap = (BVHTreeOverlap *)MEM_callocN(sizeof(BVHTreeOverlap)*total, "BVHTreeOverlap");
	
	for(j = 0; j < tree1->tree_type; j++)
	{
		memcpy(to, data[j]->overlap, data[j]->i*sizeof(BVHTreeOverlap));
		to+=data[j]->i;
	}
	
	for(j = 0; j < tree1->tree_type; j++)
	{
		free(data[j]->overlap);
		MEM_freeN(data[j]);
	}
	MEM_freeN(data);
	
	(*result) = total;
	return overlap;
}


/*
 * Nearest neighbour - BLI_bvhtree_find_nearest
 */
static float squared_dist(const float *a, const float *b)
{
	float tmp[3];
	VECSUB(tmp, a, b);
	return INPR(tmp, tmp);
}

//Determines the nearest point of the given node BV. Returns the squared distance to that point.
static float calc_nearest_point(const float *proj, BVHNode *node, float *nearest)
{
	int i;
	const float *bv = node->bv;

	//nearest on AABB hull
	for(i=0; i != 3; i++, bv += 2)
	{
		if(bv[0] > proj[i])
			nearest[i] = bv[0];
		else if(bv[1] < proj[i])
			nearest[i] = bv[1];
		else
			nearest[i] = proj[i]; 
	}

/*
	//nearest on a general hull
	VECCOPY(nearest, data->co);
	for(i = data->tree->start_axis; i != data->tree->stop_axis; i++, bv+=2)
	{
		float proj = INPR( nearest, KDOP_AXES[i]);
		float dl = bv[0] - proj;
		float du = bv[1] - proj;

		if(dl > 0)
		{
			VECADDFAC(nearest, nearest, KDOP_AXES[i], dl);
		}
		else if(du < 0)
		{
			VECADDFAC(nearest, nearest, KDOP_AXES[i], du);
		}
	}
*/
	return squared_dist(proj, nearest);
}


typedef struct NodeDistance
{
	BVHNode *node;
	float dist;

} NodeDistance;

#define NodeDistance_priority(a,b)	( (a).dist < (b).dist )

// TODO: use a priority queue to reduce the number of nodes looked on
static void dfs_find_nearest_dfs(BVHNearestData *data, BVHNode *node)
{
	if(node->totnode == 0)
	{
		if(data->callback)
			data->callback(data->userdata , node->index, data->co, &data->nearest);
		else
		{
			data->nearest.index	= node->index;
			data->nearest.dist	= calc_nearest_point(data->proj, node, data->nearest.co);
		}
	}
	else
	{
		//Better heuristic to pick the closest node to dive on
		int i;
		float nearest[3];

		if(data->proj[ node->main_axis ] <= node->children[0]->bv[node->main_axis*2+1])
		{

			for(i=0; i != node->totnode; i++)
			{
				if( calc_nearest_point(data->proj, node->children[i], nearest) >= data->nearest.dist) continue;
				dfs_find_nearest_dfs(data, node->children[i]);
			}
		}
		else
		{
			for(i=node->totnode-1; i >= 0 ; i--)
			{
				if( calc_nearest_point(data->proj, node->children[i], nearest) >= data->nearest.dist) continue;
				dfs_find_nearest_dfs(data, node->children[i]);
			}
		}
	}
}

static void dfs_find_nearest_begin(BVHNearestData *data, BVHNode *node)
{
	float nearest[3], sdist;
	sdist = calc_nearest_point(data->proj, node, nearest);
	if(sdist >= data->nearest.dist) return;
	dfs_find_nearest_dfs(data, node);
}


#if 0
static void NodeDistance_push_heap(NodeDistance *heap, int heap_size)
PUSH_HEAP_BODY(NodeDistance, NodeDistance_priority, heap, heap_size)

static void NodeDistance_pop_heap(NodeDistance *heap, int heap_size)
POP_HEAP_BODY(NodeDistance, NodeDistance_priority, heap, heap_size)

//NN function that uses an heap.. this functions leads to an optimal number of min-distance
//but for normal tri-faces and BV 6-dop.. a simple dfs with local heuristics (as implemented
//in source/blender/blenkernel/intern/shrinkwrap.c) works faster.
//
//It may make sense to use this function if the callback queries are very slow.. or if its impossible
//to get a nice heuristic
//
//this function uses "malloc/free" instead of the MEM_* because it intends to be openmp safe
static void bfs_find_nearest(BVHNearestData *data, BVHNode *node)
{
	int i;
	NodeDistance default_heap[DEFAULT_FIND_NEAREST_HEAP_SIZE];
	NodeDistance *heap=default_heap, current;
	int heap_size = 0, max_heap_size = sizeof(default_heap)/sizeof(default_heap[0]);
	float nearest[3];

	int callbacks = 0, push_heaps = 0;

	if(node->totnode == 0)
	{
		dfs_find_nearest_dfs(data, node);
		return;
	}

	current.node = node;
	current.dist = calc_nearest_point(data->proj, node, nearest);

	while(current.dist < data->nearest.dist)
	{
//		printf("%f : %f\n", current.dist, data->nearest.dist);
		for(i=0; i< current.node->totnode; i++)
		{
			BVHNode *child = current.node->children[i];
			if(child->totnode == 0)
			{
				callbacks++;
				dfs_find_nearest_dfs(data, child);
			}
			else
			{
				//adjust heap size
				if(heap_size >= max_heap_size
				&& ADJUST_MEMORY(default_heap, (void**)&heap, heap_size+1, &max_heap_size, sizeof(heap[0])) == FALSE)
				{
					printf("WARNING: bvh_find_nearest got out of memory\n");

					if(heap != default_heap)
						free(heap);

					return;
				}

				heap[heap_size].node = current.node->children[i];
				heap[heap_size].dist = calc_nearest_point(data->proj, current.node->children[i], nearest);

				if(heap[heap_size].dist >= data->nearest.dist) continue;
				heap_size++;

				NodeDistance_push_heap(heap, heap_size);
	//			PUSH_HEAP_BODY(NodeDistance, NodeDistance_priority, heap, heap_size);
				push_heaps++;
			}
		}
		
		if(heap_size == 0) break;

		current = heap[0];
		NodeDistance_pop_heap(heap, heap_size);
//		POP_HEAP_BODY(NodeDistance, NodeDistance_priority, heap, heap_size);
		heap_size--;
	}

//	printf("hsize=%d, callbacks=%d, pushs=%d\n", heap_size, callbacks, push_heaps);

	if(heap != default_heap)
		free(heap);
}
#endif


int BLI_bvhtree_find_nearest(BVHTree *tree, const float *co, BVHTreeNearest *nearest, BVHTree_NearestPointCallback callback, void *userdata)
{
	int i;

	BVHNearestData data;
	BVHNode* root = tree->nodes[tree->totleaf];

	//init data to search
	data.tree = tree;
	data.co = co;

	data.callback = callback;
	data.userdata = userdata;

	for(i = data.tree->start_axis; i != data.tree->stop_axis; i++)
	{
		data.proj[i] = INPR(data.co, KDOP_AXES[i]);
	}

	if(nearest)
	{
		memcpy( &data.nearest , nearest, sizeof(*nearest) );
	}
	else
	{
		data.nearest.index = -1;
		data.nearest.dist = FLT_MAX;
	}

	//dfs search
	if(root)
		dfs_find_nearest_begin(&data, root);

	//copy back results
	if(nearest)
	{
		memcpy(nearest, &data.nearest, sizeof(*nearest));
	}

	return data.nearest.index;
}


/*
 * Raycast - BLI_bvhtree_ray_cast
 *
 * raycast is done by performing a DFS on the BVHTree and saving the closest hit
 */


//Determines the distance that the ray must travel to hit the bounding volume of the given node
static float ray_nearest_hit(BVHRayCastData *data, float *bv)
{
	int i;

	float low = 0, upper = data->hit.dist;

	for(i=0; i != 3; i++, bv += 2)
	{
		if(data->ray_dot_axis[i] == 0.0f)
		{
			//axis aligned ray
			if(data->ray.origin[i] < bv[0] - data->ray.radius
			|| data->ray.origin[i] > bv[1] + data->ray.radius)
				return FLT_MAX;
		}
		else
		{
			float ll = (bv[0] - data->ray.radius - data->ray.origin[i]) / data->ray_dot_axis[i];
			float lu = (bv[1] + data->ray.radius - data->ray.origin[i]) / data->ray_dot_axis[i];

			if(data->ray_dot_axis[i] > 0.0f)
			{
				if(ll > low)   low = ll;
				if(lu < upper) upper = lu;
			}
			else
			{
				if(lu > low)   low = lu;
				if(ll < upper) upper = ll;
			}
	
			if(low > upper) return FLT_MAX;
		}
	}
	return low;
}

//Determines the distance that the ray must travel to hit the bounding volume of the given node
//Based on Tactical Optimization of Ray/Box Intersection, by Graham Fyffe
//[http://tog.acm.org/resources/RTNews/html/rtnv21n1.html#art9]
//
//TODO this doens't has data->ray.radius in consideration
static float fast_ray_nearest_hit(const BVHRayCastData *data, const BVHNode *node)
{
	const float *bv = node->bv;
	float dist;
	
	float t1x = (bv[data->index[0]] - data->ray.origin[0]) * data->idot_axis[0];
	float t2x = (bv[data->index[1]] - data->ray.origin[0]) * data->idot_axis[0];
	float t1y = (bv[data->index[2]] - data->ray.origin[1]) * data->idot_axis[1];
	float t2y = (bv[data->index[3]] - data->ray.origin[1]) * data->idot_axis[1];
	float t1z = (bv[data->index[4]] - data->ray.origin[2]) * data->idot_axis[2];
	float t2z = (bv[data->index[5]] - data->ray.origin[2]) * data->idot_axis[2];

	if(t1x > t2y || t2x < t1y || t1x > t2z || t2x < t1z || t1y > t2z || t2y < t1z) return FLT_MAX;
	if(t2x < 0.0 || t2y < 0.0 || t2z < 0.0) return FLT_MAX;
	if(t1x > data->hit.dist || t1y > data->hit.dist || t1z > data->hit.dist) return FLT_MAX;

	dist = t1x;
	if (t1y > dist) dist = t1y;
    if (t1z > dist) dist = t1z;
	return dist;
}

static void dfs_raycast(BVHRayCastData *data, BVHNode *node)
{
	int i;

	//ray-bv is really fast.. and simple tests revealed its worth to test it
	//before calling the ray-primitive functions
	float dist = fast_ray_nearest_hit(data, node);
	if(dist >= data->hit.dist) return;

	if(node->totnode == 0)
	{
		if(data->callback)
			data->callback(data->userdata, node->index, &data->ray, &data->hit);
		else
		{
			data->hit.index	= node->index;
			data->hit.dist  = dist;
			VECADDFAC(data->hit.co, data->ray.origin, data->ray.direction, dist);
		}
	}
	else
	{
		//pick loop direction to dive into the tree (based on ray direction and split axis)
		if(data->ray_dot_axis[ (int)node->main_axis ] > 0.0f)
		{
			for(i=0; i != node->totnode; i++)
			{
				dfs_raycast(data, node->children[i]);
			}
		}
		else
		{
			for(i=node->totnode-1; i >= 0; i--)
			{
				dfs_raycast(data, node->children[i]);
			}
		}
	}
}

static void iterative_raycast(BVHRayCastData *data, BVHNode *node)
{
	while(node)
	{
		float dist = fast_ray_nearest_hit(data, node);
		if(dist >= data->hit.dist)
		{
			node = node->skip[1];
			continue;
		}

		if(node->totnode == 0)
		{
			if(data->callback)
				data->callback(data->userdata, node->index, &data->ray, &data->hit);
			else
			{
				data->hit.index	= node->index;
				data->hit.dist  = dist;
				VECADDFAC(data->hit.co, data->ray.origin, data->ray.direction, dist);
			}
			
			node = node->skip[1];
		}
		else
		{
			node = node->children[0];
		}	
	}
}

int BLI_bvhtree_ray_cast(BVHTree *tree, const float *co, const float *dir, float radius, BVHTreeRayHit *hit, BVHTree_RayCastCallback callback, void *userdata)
{
	int i;
	BVHRayCastData data;
	BVHNode * root = tree->nodes[tree->totleaf];

	data.tree = tree;

	data.callback = callback;
	data.userdata = userdata;

	VECCOPY(data.ray.origin,    co);
	VECCOPY(data.ray.direction, dir);
	data.ray.radius = radius;

	normalize_v3(data.ray.direction);

	for(i=0; i<3; i++)
	{
		data.ray_dot_axis[i] = INPR( data.ray.direction, KDOP_AXES[i]);
		data.idot_axis[i] = 1.0f / data.ray_dot_axis[i];

		if(fabs(data.ray_dot_axis[i]) < FLT_EPSILON)
		{
			data.ray_dot_axis[i] = 0.0;
		}
		data.index[2*i] = data.idot_axis[i] < 0.0 ? 1 : 0;
		data.index[2*i+1] = 1 - data.index[2*i];
		data.index[2*i]	  += 2*i;
		data.index[2*i+1] += 2*i;
	}


	if(hit)
		memcpy( &data.hit, hit, sizeof(*hit) );
	else
	{
		data.hit.index = -1;
		data.hit.dist = FLT_MAX;
	}

	if(root)
	{
		dfs_raycast(&data, root);
//		iterative_raycast(&data, root);
 	}


	if(hit)
		memcpy( hit, &data.hit, sizeof(*hit) );

	return data.hit.index;
}

float BLI_bvhtree_bb_raycast(float *bv, float *light_start, float *light_end, float *pos)
{
	BVHRayCastData data;
	float dist = 0.0;

	data.hit.dist = FLT_MAX;
	
	// get light direction
	data.ray.direction[0] = light_end[0] - light_start[0];
	data.ray.direction[1] = light_end[1] - light_start[1];
	data.ray.direction[2] = light_end[2] - light_start[2];
	
	data.ray.radius = 0.0;
	
	data.ray.origin[0] = light_start[0];
	data.ray.origin[1] = light_start[1];
	data.ray.origin[2] = light_start[2];
	
	normalize_v3(data.ray.direction);
	VECCOPY(data.ray_dot_axis, data.ray.direction);
	
	dist = ray_nearest_hit(&data, bv);
	
	if(dist > 0.0)
	{
		VECADDFAC(pos, light_start, data.ray.direction, dist);
	}
	return dist;
	
}

/*
 * Range Query - as request by broken :P
 *
 * Allocs and fills an array with the indexs of node that are on the given spherical range (center, radius) 
 * Returns the size of the array.
 */
typedef struct RangeQueryData
{
	BVHTree *tree;
	const float *center;
	float radius;			//squared radius

	int hits;

	BVHTree_RangeQuery callback;
	void *userdata;


} RangeQueryData;


static void dfs_range_query(RangeQueryData *data, BVHNode *node)
{
	if(node->totnode == 0)
	{

		//Calculate the node min-coords (if the node was a point then this is the point coordinates)
		float co[3];
		co[0] = node->bv[0];
		co[1] = node->bv[2];
		co[2] = node->bv[4];

	}
	else
	{
		int i;
		for(i=0; i != node->totnode; i++)
		{
			float nearest[3];
			float dist = calc_nearest_point(data->center, node->children[i], nearest);
			if(dist < data->radius)
			{
				//Its a leaf.. call the callback
				if(node->children[i]->totnode == 0)
				{
					data->hits++;
					data->callback( data->userdata, node->children[i]->index, dist );
				}
				else
					dfs_range_query( data, node->children[i] );
			}
		}
	}
}

int BLI_bvhtree_range_query(BVHTree *tree, const float *co, float radius, BVHTree_RangeQuery callback, void *userdata)
{
	BVHNode * root = tree->nodes[tree->totleaf];

	RangeQueryData data;
	data.tree = tree;
	data.center = co;
	data.radius = radius*radius;
	data.hits = 0;

	data.callback = callback;
	data.userdata = userdata;

	if(root != NULL)
	{
		float nearest[3];
		float dist = calc_nearest_point(data.center, root, nearest);
		if(dist < data.radius)
		{
			//Its a leaf.. call the callback
			if(root->totnode == 0)
			{
				data.hits++;
				data.callback( data.userdata, root->index, dist );
			}
			else
				dfs_range_query( &data, root );
		}
	}

	return data.hits;
}
