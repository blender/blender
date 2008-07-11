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

#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_kdopbvh.h"
#include "BLI_arithb.h"

#ifdef _OPENMP
#include <omp.h>
#endif

typedef struct BVHNode
{
	struct BVHNode **children; // max 8 children
	struct BVHNode *parent; // needed for bottom - top update
	float *bv; // Bounding volume of all nodes, max 13 axis
	int index; /* face, edge, vertex index */
	char totnode; // how many nodes are used, used for speedup
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
////////////////////////////////////////


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

BVHTree *BLI_bvhtree_new(int maxsize, float epsilon, char tree_type, char axis)
{
	BVHTree *tree;
	int numbranches=0, i;
	
	// only support up to octree
	if(tree_type > 8)
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


		// calculate max number of branches, our bvh kdop is "almost perfect"
		for(i = 1; i <= (int)ceil((float)((float)log(maxsize)/(float)log(tree_type))); i++)
			numbranches += (pow(tree_type, i) / tree_type);
		
		tree->nodes = (BVHNode **)MEM_callocN(sizeof(BVHNode *)*(numbranches+maxsize + tree_type), "BVHNodes");
		
		if(!tree->nodes)
		{
			MEM_freeN(tree);
			return NULL;
		}
		
		tree->nodebv = (float*)MEM_callocN(sizeof(float)* axis * (numbranches+maxsize + tree_type), "BVHNodeBV");
		if(!tree->nodebv)
		{
			MEM_freeN(tree->nodes);
			MEM_freeN(tree);
		}

		tree->nodechild = (BVHNode**)MEM_callocN(sizeof(BVHNode*) * tree_type * (numbranches+maxsize + tree_type), "BVHNodeBV");
		if(!tree->nodechild)
		{
			MEM_freeN(tree->nodebv);
			MEM_freeN(tree->nodes);
			MEM_freeN(tree);
		}

		tree->nodearray = (BVHNode *)MEM_callocN(sizeof(BVHNode)*(numbranches+maxsize + tree_type), "BVHNodeArray");
		
		if(!tree->nodearray)
		{
			MEM_freeN(tree->nodechild);
			MEM_freeN(tree->nodebv);
			MEM_freeN(tree->nodes);
			MEM_freeN(tree);
			return NULL;
		}

		//link the dynamic bv and child links
		for(i=0; i< numbranches+maxsize + tree_type; i++)
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
	int i, k;
	
	// don't init boudings for the moving case
	if(!moving)
	{
		for (i = tree->start_axis; i < tree->stop_axis; i++)
		{
			node->bv[2*i] = FLT_MAX;
			node->bv[2*i + 1] = -FLT_MAX;
		}
	}
	
	for(k = 0; k < numpoints; k++)
	{
		// for all Axes.
		for (i = tree->start_axis; i < tree->stop_axis; i++)
		{
			newminmax = INPR(&co[k * 3], KDOP_AXES[i]);
			if (newminmax < node->bv[2 * i])
				node->bv[2 * i] = newminmax;
			if (newminmax > node->bv[(2 * i) + 1])
				node->bv[(2 * i) + 1] = newminmax;
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
	BVHNode *node= NULL;
	int i;
	
	// insert should only possible as long as tree->totbranch is 0
	if(tree->totbranch > 0)
		return 0;
	
	if(tree->totleaf+1 >= MEM_allocN_len(tree->nodes))
		return 0;
	
	// TODO check if have enough nodes in array
	
	node = tree->nodes[tree->totleaf] = &(tree->nodearray[tree->totleaf]);
	tree->totleaf++;
	
	create_kdop_hull(tree, node, co, numpoints, 0);
	
	// inflate the bv with some epsilon
	for (i = tree->start_axis; i < tree->stop_axis; i++)
	{
		node->bv[(2 * i)] -= tree->epsilon; // minimum 
		node->bv[(2 * i) + 1] += tree->epsilon; // maximum 
	}

	node->index= index;
	
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

static void bvh_div_nodes(BVHTree *tree, BVHNode *node, int start, int end, char lastaxis)
{
	char laxis;
	int i, tend;
	BVHNode *tnode;
	int slice = (end-start+tree->tree_type-1)/tree->tree_type;	//division rounded up
	
	// Determine which axis to split along
	laxis = get_largest_axis(node->bv);
	
	// split nodes along longest axis
	for (i=0; start < end; start += slice, i++) //i counts the current child
	{	
		tend = start + slice;
		
		if(tend > end) tend = end;
		
		if(tend-start == 1)	// ok, we have 1 left for this node
		{
			node->children[i] = tree->nodes[start];
			node->children[i]->parent = node;
		}
		else
		{
			tnode = node->children[i] = tree->nodes[tree->totleaf  + tree->totbranch] = &(tree->nodearray[tree->totbranch + tree->totleaf]);
			tree->totbranch++;
			tnode->parent = node;
			
			if(tend != end)
				partition_nth_element(tree->nodes, start, end, tend, laxis);
			refit_kdop_hull(tree, tnode, start, tend);
			bvh_div_nodes(tree, tnode, start, tend, laxis);
		}
		node->totnode++;
	}
	
	return;
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
	
void BLI_bvhtree_balance(BVHTree *tree)
{
	BVHNode *node;
	
	if(tree->totleaf == 0)
		return;
	
	// create root node
	node = tree->nodes[tree->totleaf] = &(tree->nodearray[tree->totleaf]);
	tree->totbranch++;
	
	// refit root bvh node
	refit_kdop_hull(tree, tree->nodes[tree->totleaf], 0, tree->totleaf);
	// create + balance tree
	bvh_div_nodes(tree, tree->nodes[tree->totleaf], 0, tree->totleaf, 0);
	
	// verify_tree(tree);
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
