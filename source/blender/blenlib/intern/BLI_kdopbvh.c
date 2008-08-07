/**
 *
 * $Id$
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
#include "BLI_arithb.h"

#ifdef _OPENMP
#include <omp.h>
#endif

typedef struct BVHNode
{
	struct BVHNode **children;	// max 8 children
	struct BVHNode *parent; // needed for bottom - top update
	float *bv;		// Bounding volume of all nodes, max 13 axis
	int index;		// face, edge, vertex index
	char totnode;	// how many nodes are used, used for speedup
	char traversed;  // how many nodes already traversed until this level?
	char main_axis;
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
} BVHOverlapData;

typedef struct BVHNearestData
{
	BVHTree *tree;
	float	*co;
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
void sort_along_axis(BVHTree *tree, int start, int end, int axis)
{
	sort(tree->nodes, start, end, axis);
}

//after a call to this function you can expect one of:
//      every node to left of a[n] are smaller or equal to it
//      every node to the right of a[n] are greater or equal to it
int partition_nth_element(BVHNode **a, int _begin, int _end, int n, int axis){
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

// calculate max number of branches
int needed_branches(int tree_type, int leafs)
{
#if 1
	//Worst case scenary  ( return max(0, leafs-tree_type)+1 )
	if(leafs <= tree_type)
		return 1;
	else
		return leafs-tree_type+1;

#else
	//If our bvh kdop is "almost perfect"
	//TODO i dont trust the float arithmetic in here (and I am not sure this formula is according to our splitting method)
	int i, numbranches = 0;
	for(i = 1; i <= (int)ceil((float)((float)log(leafs)/(float)log(tree_type))); i++)
		numbranches += (pow(tree_type, i) / tree_type);

	return numbranches;
#endif
}
		

BVHTree *BLI_bvhtree_new(int maxsize, float epsilon, char tree_type, char axis)
{
	BVHTree *tree;
	int numnodes, i;
	
	// theres not support for trees below binary-trees :P
	if(tree_type < 2)
		return NULL;

	tree = (BVHTree *)MEM_callocN(sizeof(BVHTree), "BVHTree");
	
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
		numnodes = maxsize + needed_branches(tree_type, maxsize) + tree_type;

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

static void bvh_div_nodes(BVHTree *tree, BVHNode *node, int start, int end, int free_node_index)
{
	int i;

	const char laxis = get_largest_axis(node->bv); 	//determine longest axis to split along
	const int  slice = (end-start)/tree->tree_type;	//division rounded down
	const int  rest  = (end-start)%tree->tree_type;	//remainder of division
	
	assert( node->totnode == 0 );

	node->main_axis = laxis/2;
	
	// split nodes along longest axis
	for (i=0; start < end; node->totnode = ++i) //i counts the current child
	{	
		int tend = start + slice + (i < rest ? 1 : 0);
		
		assert( tend <= end);
		
		if(tend-start == 1)	// ok, we have 1 left for this node
		{
			node->children[i] = tree->nodes[start];
			node->children[i]->parent = node;
		}
		else
		{
			BVHNode *tnode = node->children[i] = tree->nodes[free_node_index] = &(tree->nodearray[free_node_index]);
			tnode->parent = node;
			
			if(tend != end)
				partition_nth_element(tree->nodes, start, end, tend, laxis);

			refit_kdop_hull(tree, tnode, start, tend);

			bvh_div_nodes(tree, tnode, start, tend, free_node_index+1);
			free_node_index += needed_branches(tree->tree_type, tend-start);
		}
		start = tend;
	}
	
	return;
}

static void omp_bvh_div_nodes(BVHTree *tree, BVHNode *node, int start, int end, int free_node_index)
{
	int i;

	const char laxis = get_largest_axis(node->bv); 	//determine longest axis to split along
	const int  slice = (end-start)/tree->tree_type;	//division rounded down
	const int  rest  = (end-start)%tree->tree_type;	//remainder of division

	int omp_data_start[tree->tree_type];
	int omp_data_end  [tree->tree_type];
	int omp_data_index[tree->tree_type];
	
	assert( node->totnode == 0 );

	node->main_axis = laxis/2;	

	// split nodes along longest axis
	for (i=0; start < end; node->totnode = ++i) //i counts the current child
	{	
		//Split the rest from left to right (TODO: this doenst makes an optimal tree)
		int tend = start + slice + (i < rest ? 1 : 0);
		
		assert( tend <= end);
		
		//save data for later OMP
		omp_data_start[i] = start;
		omp_data_end  [i] = tend;
		omp_data_index[i] = free_node_index;

		if(tend-start == 1)
		{
			node->children[i] = tree->nodes[start];
			node->children[i]->parent = node;
		}
		else
		{
			node->children[i] = tree->nodes[free_node_index] = &(tree->nodearray[free_node_index]);
			node->children[i]->parent = node;

			if(tend != end)
				partition_nth_element(tree->nodes, start, end, tend, laxis);

			free_node_index += needed_branches(tree->tree_type, tend-start);
		}

		start = tend;
	}

#pragma omp parallel for private(i) schedule(static)
	for( i = 0; i < node->totnode; i++)
	{
		if(omp_data_end[i]-omp_data_start[i] > 1)
		{
			BVHNode *tnode = node->children[i];
			refit_kdop_hull(tree, tnode, omp_data_start[i], omp_data_end[i]);
			bvh_div_nodes  (tree, tnode, omp_data_start[i], omp_data_end[i], omp_data_index[i]+1);
		}
	}
	
	return;
}


static void print_tree(BVHTree *tree, BVHNode *node, int depth)
{
	int i;
	for(i=0; i<depth; i++) printf(" ");
	printf(" - %d (%d): ", node->index, node - tree->nodearray);
	for(i=2*tree->start_axis; i<2*tree->stop_axis; i++)
		printf("%.3f ", node->bv[i]);
	printf("\n");

	for(i=0; i<tree->tree_type; i++)
		if(node->children[i])
			print_tree(tree, node->children[i], depth+1);
}

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

//Helper data and structures to build generalized implicit trees
//This code can be easily reduced
typedef struct BVHBuildHelper
{
	int tree_type; //
	int totleafs; //

	int leafs_per_child  [32]; //Min number of leafs that are archievable from a node at depth N
	int branches_on_level[32]; //Number of nodes at depth N (tree_type^N)

	int remain_leafs; //Number of leafs that are placed on the level that is not 100% filled

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

//WARNING: Beautiful/tricky code starts here :P
//Generalized implicit trees
static void non_recursive_bvh_div_nodes(BVHTree *tree)
{
	int i;

	const int tree_type   = tree->tree_type;
	const int tree_offset = 2 - tree->tree_type; //this value is 0 (on binary trees) and negative on the others
	const int num_leafs   = tree->totleaf;
	const int num_branches= MAX2(1, (num_leafs + tree_type - 3) / (tree_type-1) );

	BVHNode*  branches_array = tree->nodearray + tree->totleaf - 1; // This code uses 1 index arrays
	BVHNode** leafs_array    = tree->nodes;

	BVHBuildHelper data;
	int depth  = 0;

	build_implicit_tree_helper(tree, &data);

	//YAY this could be 1 loop.. but had to split in 2 to remove OMP dependencies
	for(i=1; i <= num_branches; i = i*tree_type + tree_offset)
	{
		const int first_of_next_level = i*tree_type + tree_offset;
		const int  end_j = MIN2(first_of_next_level, num_branches + 1);	//index of last branch on this level
		int j;

		depth++;

#pragma omp parallel for private(j) schedule(static)
		for(j = i; j < end_j; j++)
		{
			int k;
			const int parent_level_index= j-i;
			BVHNode* parent = branches_array + j;
			char split_axis;

			int parent_leafs_begin = implicit_leafs_index(&data, depth, parent_level_index);
			int parent_leafs_end   = implicit_leafs_index(&data, depth, parent_level_index+1);

			//split_axis = (depth*2 % 6); //use this instead of the 2 following lines for XYZ splitting

			refit_kdop_hull(tree, parent, parent_leafs_begin, parent_leafs_end);
			split_axis = get_largest_axis(parent->bv);

			parent->main_axis = split_axis / 2;

			for(k = 0; k < tree_type; k++)
			{
				int child_index = j * tree_type + tree_offset + k;
				int child_level_index = child_index - first_of_next_level; //child level index

				int child_leafs_begin = implicit_leafs_index(&data, depth+1, child_level_index);
				int child_leafs_end   = implicit_leafs_index(&data, depth+1, child_level_index+1);

				assert( k != 0 || child_leafs_begin == parent_leafs_begin);

				if(child_leafs_end - child_leafs_begin > 1)
				{
					parent->children[k] = branches_array + child_index;
					parent->children[k]->parent = parent;

/*
					printf("Add child %d (%d) to branch %d\n",
					branches_array  + child_index - tree->nodearray,
					branches_array[ child_index ].index,
					parent - tree->nodearray
						);
*/

					partition_nth_element(leafs_array, child_leafs_begin, parent_leafs_end, child_leafs_end, split_axis);
				}
				else if(child_leafs_end - child_leafs_begin == 1)
				{
/*
					printf("Add child %d (%d) to branch %d\n",
					leafs_array[ child_leafs_begin ] - tree->nodearray,
					leafs_array[ child_leafs_begin ]->index,
					parent - tree->nodearray
						);
*/
					parent->children[k] = leafs_array[ child_leafs_begin ];
					parent->children[k]->parent = parent;
				}
				else
				{
					parent->children[k] = NULL;
					break;
				}
				parent->totnode = k+1;
			}
		}
	}


	for(i = 0; i<num_branches; i++)
		tree->nodes[tree->totleaf + i] = branches_array + 1 + i;

	tree->totbranch = num_branches;

//	BLI_bvhtree_update_tree(tree); //Uncoment this for XYZ splitting
}

void BLI_bvhtree_balance(BVHTree *tree)
{
	if(tree->totleaf == 0) return;

	assert(tree->totbranch == 0);
	non_recursive_bvh_div_nodes(tree);

/*
	if(tree->totleaf != 0)
	{
		// create root node
	BVHNode *node = tree->nodes[tree->totleaf] = &(tree->nodearray[tree->totleaf]);
	tree->totbranch++;
	<	
		// refit root bvh node
	refit_kdop_hull(tree, node, 0, tree->totleaf);

		// create + balance tree
	omp_bvh_div_nodes(tree, node, 0, tree->totleaf, tree->totleaf+1);
	tree->totbranch = needed_branches( tree->tree_type, tree->totleaf );
		// verify_tree(tree);
}
*/

}

// overlap - is it possbile for 2 bv's to collide ?
static int tree_overlap(float *bv1, float *bv2, int start_axis, int stop_axis)
{
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
	
	if(tree_overlap(node1->bv, node2->bv, MIN2(data->tree1->start_axis, data->tree2->start_axis), MIN2(data->tree1->stop_axis, data->tree2->stop_axis)))
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
	if((tree1->axis != tree2->axis) && ((tree1->axis == 14) || tree2->axis == 14))
		return 0;
	
	// fast check root nodes for collision before doing big splitting + traversal
	if(!tree_overlap(tree1->nodes[tree1->totleaf]->bv, tree2->nodes[tree2->totleaf]->bv, MIN2(tree1->start_axis, tree2->start_axis), MIN2(tree1->stop_axis, tree2->stop_axis)))
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



// bottom up update of bvh tree:
// join the 4 children here
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

// call before BLI_bvhtree_update_tree()
int BLI_bvhtree_update_node(BVHTree *tree, int index, float *co, float *co_moving, int numpoints)
{
	BVHNode *node= NULL;
	int i = 0;
	
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
	BVHNode *leaf, *parent;
	
	// reset tree traversing flag
	for (leaf = tree->nodearray + tree->totleaf; leaf != tree->nodearray + tree->totleaf + tree->totbranch; leaf++)
		leaf->traversed = 0;
	
	for (leaf = tree->nodearray; leaf != tree->nodearray + tree->totleaf; leaf++)
	{
		for (parent = leaf->parent; parent; parent = parent->parent)
		{
			parent->traversed++;	// we tried to go up in hierarchy 
			if (parent->traversed < parent->totnode) 
				break;	// we do not need to check further 
			else 
				node_join(tree, parent);
		}
	}
}

float BLI_bvhtree_getepsilon(BVHTree *tree)
{
	return tree->epsilon;
}




/*
 * Nearest neighbour
 */
static float squared_dist(const float *a, const float *b)
{
	float tmp[3];
	VECSUB(tmp, a, b);
	return INPR(tmp, tmp);
}

static float calc_nearest_point(BVHNearestData *data, BVHNode *node, float *nearest)
{
	int i;
	const float *bv = node->bv;

	//nearest on AABB hull
	for(i=0; i != 3; i++, bv += 2)
	{
		if(bv[0] > data->proj[i])
			nearest[i] = bv[0];
		else if(bv[1] < data->proj[i])
			nearest[i] = bv[1];
		else
			nearest[i] = data->proj[i];
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
	return squared_dist(data->co, nearest);
}


// TODO: use a priority queue to reduce the number of nodes looked on
static void dfs_find_nearest(BVHNearestData *data, BVHNode *node)
{
	int i;
	float nearest[3], sdist;

	sdist = calc_nearest_point(data, node, nearest);
	if(sdist >= data->nearest.dist) return;

	if(node->totnode == 0)
	{
		if(data->callback)
			data->callback(data->userdata , node->index, data->co, &data->nearest);
		else
		{
			data->nearest.index	= node->index;
			VECCOPY(data->nearest.co, nearest);
			data->nearest.dist	= sdist;
		}
	}
	else
	{
		for(i=0; i != node->totnode; i++)
			dfs_find_nearest(data, node->children[i]);
	}
}

int BLI_bvhtree_find_nearest(BVHTree *tree, const float *co, BVHTreeNearest *nearest, BVHTree_NearestPointCallback callback, void *userdata)
{
	int i;

	BVHNearestData data;

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
	dfs_find_nearest(&data, tree->nodes[tree->totleaf] );

	//copy back results
	if(nearest)
	{
		memcpy(nearest, &data.nearest, sizeof(*nearest));
	}

	return data.nearest.index;
}



/*
 * Ray cast
 */

static float ray_nearest_hit(BVHRayCastData *data, BVHNode *node)
{
	int i;
	const float *bv = node->bv;

	float low = 0, upper = data->hit.dist;

	for(i=0; i != 3; i++, bv += 2)
	{
		if(data->ray_dot_axis[i] == 0.0f)
		{
			//axis aligned ray
			if(data->ray.origin[i] < bv[0]
						|| data->ray.origin[i] > bv[1])
				return FLT_MAX;
		}
		else
		{
			float ll = (bv[0] - data->ray.origin[i]) / data->ray_dot_axis[i];
			float lu = (bv[1] - data->ray.origin[i]) / data->ray_dot_axis[i];

			if(data->ray_dot_axis[i] > 0)
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

static void dfs_raycast(BVHRayCastData *data, BVHNode *node)
{
	int i;

	//ray-bv is really fast.. and simple tests revealed its worth to test it
	//before calling the ray-primitive functions
	float dist = ray_nearest_hit(data, node);
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
		if(data->ray_dot_axis[ node->main_axis ] > 0)
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



int BLI_bvhtree_ray_cast(BVHTree *tree, const float *co, const float *dir, BVHTreeRayHit *hit, BVHTree_RayCastCallback callback, void *userdata)
{
	int i;
	BVHRayCastData data;

	data.tree = tree;

	data.callback = callback;
	data.userdata = userdata;

	VECCOPY(data.ray.origin,    co);
	VECCOPY(data.ray.direction, dir);

	Normalize(data.ray.direction);

	for(i=0; i<3; i++)
	{
		data.ray_dot_axis[i] = INPR( data.ray.direction, KDOP_AXES[i]);

		if(fabs(data.ray_dot_axis[i]) < 1e-7)
			data.ray_dot_axis[i] = 0.0;
	}


	if(hit)
		memcpy( &data.hit, hit, sizeof(*hit) );
	else
	{
		data.hit.index = -1;
		data.hit.dist = FLT_MAX;
	}

	dfs_raycast(&data, tree->nodes[tree->totleaf]);


	if(hit)
		memcpy( hit, &data.hit, sizeof(*hit) );

	return data.hit.index;
}

