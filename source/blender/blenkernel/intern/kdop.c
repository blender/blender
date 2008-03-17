/*  kdop.c      
* 
*
* ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version. The Blender
* Foundation also sells licenses for use in proprietary software under
* the Blender License.  See http://www.blender.org/BL/ for information
* about this.
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
* The Original Code is Copyright (C) Blender Foundation
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): none yet.
*
* ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "MEM_guardedalloc.h"

#include "BKE_cloth.h"

#include "DNA_cloth_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_modifier.h"
#include "BKE_utildefines.h"

#ifdef _OPENMP
#include <omp.h>
#endif


////////////////////////////////////////////////////////////////////////
// Additional fastened appending function
// It uses the link to the last inserted node as start value 
// for searching the end of the list
// NEW: in compare to the original function, this one returns
// the reference to the last inserted node 
////////////////////////////////////////////////////////////////////////
LinkNode *BLI_linklist_append_fast(LinkNode **listp, void *ptr) {
	LinkNode *nlink= MEM_mallocN(sizeof(*nlink), "nlink");
	LinkNode *node = *listp;

	nlink->link = ptr;
	nlink->next = NULL;

	if(node == NULL){
		*listp = nlink;
	} else {
		while(node->next != NULL){
			node = node->next;   
		}
		node->next = nlink;
	}
	return nlink;
}



////////////////////////////////////////////////////////////////////////
// Bounding Volume Hierarchy Definition
// 
// Notes: From OBB until 26-DOP --> all bounding volumes possible, just choose type below
// Notes: You have to choose the type at compile time ITM
// Notes: You can choose the tree type --> binary, quad, octree, choose below
////////////////////////////////////////////////////////////////////////

static float KDOP_AXES[13][3] =
{ {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}, {1, -1, 1}, {1, 1, -1},
{1, -1, -1}, {1, 1, 0}, {1, 0, 1}, {0, 1, 1}, {1, -1, 0}, {1, 0, -1},
{0, 1, -1}
};

///////////// choose bounding volume here! /////////////

#define KDOP_26



#ifdef KDOP_26
#define KDOP_END 13
#define KDOP_START 0
#endif

#ifdef KDOP_18
#define KDOP_END 13
#define KDOP_START 7
#endif

#ifdef KDOP_14
#define KDOP_END 7
#define KDOP_START 0
#endif

// this is basicly some AABB
#ifdef KDOP_8
#define KDOP_END 4
#define KDOP_START 0
#endif

// this is basicly some OBB
#ifdef KDOP_6
#define KDOP_END 3
#define KDOP_START 0
#endif

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
DO_INLINE void bvh_exchange(CollisionTree **a, int i, int j)
{
	CollisionTree *t=a[i];
	a[i]=a[j];
	a[j]=t;
}
DO_INLINE int floor_lg(int a)
{
	return (int)(floor(log(a)/log(2)));
}

/*
* Insertion sort algorithm
*/
static void bvh_insertionsort(CollisionTree **a, int lo, int hi, int axis)
{
	int i,j;
	CollisionTree *t;
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

static int bvh_partition(CollisionTree **a, int lo, int hi, CollisionTree * x, int axis)
{
	int i=lo, j=hi;
	while (1)
	{
		while ((a[i])->bv[axis] < x->bv[axis]) i++;
		j=j-1;
		while (x->bv[axis] < (a[j])->bv[axis]) j=j-1;
		if(!(i < j))
			return i;
		bvh_exchange(a, i,j);
		i++;
	}
}

/*
* Heapsort algorithm
*/
static void bvh_downheap(CollisionTree **a, int i, int n, int lo, int axis)
{
	CollisionTree * d = a[lo+i-1];
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

static void bvh_heapsort(CollisionTree **a, int lo, int hi, int axis)
{
	int n = hi-lo, i;
	for (i=n/2; i>=1; i=i-1)
	{
		bvh_downheap(a, i,n,lo, axis);
	}
	for (i=n; i>1; i=i-1)
	{
		bvh_exchange(a, lo,lo+i-1);
		bvh_downheap(a, 1,i-1,lo, axis);
	}
}

static CollisionTree *bvh_medianof3(CollisionTree **a, int lo, int mid, int hi, int axis) // returns Sortable
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
static void bvh_introsort_loop (CollisionTree **a, int lo, int hi, int depth_limit, int axis)
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

DO_INLINE void bvh_sort(CollisionTree **a0, int begin, int end, int axis)
{
	if (begin < end)
	{
		CollisionTree **a=a0;
		bvh_introsort_loop(a, begin, end, 2*floor_lg(end-begin), axis);
		bvh_insertionsort(a, begin, end, axis);
	}
}
DO_INLINE void bvh_sort_along_axis(CollisionTree **face_list, int start, int end, int axis)
{
	bvh_sort(face_list, start, end, axis);
}
////////////////////////////////////////////////////////////////////////////////////////////////
void bvh_free(BVH * bvh)
{
	LinkNode *search = NULL;
	CollisionTree *tree = NULL;

	if (bvh) 
	{

		search = bvh->tree;

		while(search)
		{
			LinkNode *next= search->next;
			tree = search->link;

			MEM_freeN(tree);

			search = next;
		}

		BLI_linklist_free(bvh->tree,NULL); 
		bvh->tree = NULL;
		
		if(bvh->current_x)
			MEM_freeN(bvh->current_x);
		if(bvh->current_xold)
			MEM_freeN(bvh->current_xold);

		MEM_freeN(bvh);
		bvh = NULL;
	}
}

// only supports x,y,z axis in the moment
// but we should use a plain and simple function here for speed sake
DO_INLINE int bvh_largest_axis(float *bv)
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

// depends on the fact that the BVH's for each face is already build
DO_INLINE void bvh_calc_DOP_hull_from_faces(BVH * bvh, CollisionTree **tri, int numfaces, float *bv)
{
	float newmin,newmax;
	int i, j;
	for (j = 0; j < numfaces; j++)
	{
		// for all Axes.
		for (i = KDOP_START; i < KDOP_END; i++)
		{
			newmin = (tri [j])->bv[(2 * i)];	
			if ((newmin < bv[(2 * i)]) || (j == 0))
			{
				bv[(2 * i)] = newmin;
			}

			newmax = (tri [j])->bv[(2 * i) + 1];
			if ((newmax > bv[(2 * i) + 1]) || (j == 0))
			{
				bv[(2 * i) + 1] = newmax;
			}
		}
	}
}

DO_INLINE void bvh_calc_DOP_hull_static(BVH * bvh, CollisionTree **tri, int numfaces, float *bv, CollisionTree *tree)
{
	MFace *tempMFace = bvh->mfaces;
	float *tempBV = bv;
	float newminmax;
	int i, j, k;
	
	for (j = 0; j < numfaces; j++)
	{
		tempMFace = bvh->mfaces + (tri [j])->tri_index;
		// 3 or 4 vertices per face.
		for (k = 0; k < 4; k++)
		{
			int temp = 0;  
			// If this is a triangle.
			if (k == 3 && !tempMFace->v4)
				continue;
			// TODO: other name for "temp" this gets all vertices of a face
			if (k == 0)
				temp = tempMFace->v1;
			else if (k == 1)
				temp = tempMFace->v2;
			else if (k == 2)
				temp = tempMFace->v3;
			else if (k == 3)
				temp = tempMFace->v4;
			// for all Axes.
			for (i = KDOP_START; i < KDOP_END; i++)
			{				
				newminmax = INPR(bvh->current_xold[temp].co, KDOP_AXES[i]);
				if ((newminmax < tempBV[(2 * i)]) || (k == 0 && j == 0))
					tempBV[(2 * i)] = newminmax;
				if ((newminmax > tempBV[(2 * i) + 1])|| (k == 0 && j == 0))
					tempBV[(2 * i) + 1] = newminmax;
			}
		}
		
		/* calculate normal of this face */
		/* (code copied from cdderivedmesh.c) */
		if(tempMFace->v4)
			CalcNormFloat4(bvh->current_xold[tempMFace->v1].co, bvh->current_xold[tempMFace->v2].co,
				       bvh->current_xold[tempMFace->v3].co, bvh->current_xold[tempMFace->v4].co, tree->normal);
		else
			CalcNormFloat(bvh->current_xold[tempMFace->v1].co, bvh->current_xold[tempMFace->v2].co,
				      bvh->current_xold[tempMFace->v3].co, tree->normal);
		
		tree->alpha = 0;
		
		
	}
}

DO_INLINE void bvh_calc_DOP_hull_moving(BVH * bvh, CollisionTree **tri, int numfaces, float *bv, CollisionTree *tree)
{
	MFace *tempMFace = bvh->mfaces;
	float *tempBV = bv;
	float newminmax;
	int i, j, k;
	
	/* TODO: calculate normals */
	
	for (j = 0; j < numfaces; j++)
	{
		tempMFace = bvh->mfaces + (tri [j])->tri_index;
		// 3 or 4 vertices per face.
		for (k = 0; k < 4; k++)
		{
			int temp = 0;  
			// If this is a triangle.
			if (k == 3 && !tempMFace->v4)
				continue;
			// TODO: other name for "temp" this gets all vertices of a face
			if (k == 0)
				temp = tempMFace->v1;
			else if (k == 1)
				temp = tempMFace->v2;
			else if (k == 2)
				temp = tempMFace->v3;
			else if (k == 3)
				temp = tempMFace->v4;
			// for all Axes.
			for (i = KDOP_START; i < KDOP_END; i++)
			{				
				newminmax = INPR(bvh->current_xold[temp].co, KDOP_AXES[i]);
				if ((newminmax < tempBV[(2 * i)]) || (k == 0 && j == 0))
					tempBV[(2 * i)] = newminmax;
				if ((newminmax > tempBV[(2 * i) + 1])|| (k == 0 && j == 0))
					tempBV[(2 * i) + 1] = newminmax;
				
				newminmax = INPR(bvh->current_x[temp].co, KDOP_AXES[i]);
				if ((newminmax < tempBV[(2 * i)]) || (k == 0 && j == 0))
					tempBV[(2 * i)] = newminmax;
				if ((newminmax > tempBV[(2 * i) + 1])|| (k == 0 && j == 0))
					tempBV[(2 * i) + 1] = newminmax;
			}
		}
	}
}

static void bvh_div_env_node(BVH *bvh, CollisionTree *tree, CollisionTree **face_list, unsigned int start, unsigned int end, int lastaxis, LinkNode *nlink)
{
	int		i = 0;
	CollisionTree *newtree = NULL;
	int laxis = 0, max_nodes=4;
	unsigned int tstart, tend;
	LinkNode *nlink1 = nlink;
	LinkNode *tnlink;
	tree->traversed = 0;	
	// Determine which axis to split along
	laxis = bvh_largest_axis(tree->bv);

	// Sort along longest axis
	if(laxis!=lastaxis)
		bvh_sort_along_axis(face_list, start, end, laxis);
	
	// maximum is 4 since we have a quad tree
	max_nodes = MIN2((end-start + 1 ),4);

	for (i = 0; i < max_nodes; i++)
	{
		tree->count_nodes++;

		if(end-start+1 > 4)
		{
			int quarter = ((float)((float)(end - start + 1) / 4.0f));
			tstart = start + i * quarter;
			tend = tstart + quarter - 1;

			// be sure that we get all faces
			if(i==3)
			{
				tend = end;
			}
		}
		else
		{
			tend = tstart = start + i;
		}

		// Build tree until 4 node left.
		if ((tend-tstart + 1 ) > 1) 
		{
			newtree = (CollisionTree *)MEM_callocN(sizeof(CollisionTree), "CollisionTree");
			tnlink = BLI_linklist_append_fast(&nlink1->next, newtree);

			newtree->nodes[0] = newtree->nodes[1] = newtree->nodes[2] = newtree->nodes[3] = NULL;
			newtree->count_nodes = 0;
			newtree->parent = tree;
			newtree->isleaf = 0;

			tree->nodes[i] = newtree;

			nlink1 = tnlink;

			bvh_calc_DOP_hull_from_faces(bvh, &face_list[tstart], tend-tstart + 1, tree->nodes[i]->bv);

			bvh_div_env_node(bvh, tree->nodes[i], face_list, tstart, tend, laxis, nlink1);
		}
		else // ok, we have 1 left for this node
		{
			CollisionTree *tnode = face_list[tstart];
			tree->nodes[i] = tnode;
			tree->nodes[i]->parent = tree;
		}
	}
	return;
}

/* function cannot be directly called - needs alloced bvh */
void bvh_build (BVH *bvh)
{
	unsigned int i = 0, j = 0, k = 0;
	CollisionTree **face_list=NULL;
	CollisionTree *tree=NULL;
	LinkNode *nlink = NULL;
	
	bvh->flags = 0;
	bvh->leaf_tree = NULL;
	bvh->leaf_root = NULL;
	bvh->tree = NULL;
	
	if(!bvh->current_x)
	{
		bvh_free(bvh);
		return;
	}
	
	bvh->current_xold = MEM_dupallocN(bvh->current_x);
	
	tree = (CollisionTree *)MEM_callocN(sizeof(CollisionTree), "CollisionTree");
	
	if (tree == NULL) 
	{
		printf("bvh_build: Out of memory for nodes.\n");
		bvh_free(bvh);
		return;
	}
	
	BLI_linklist_append(&bvh->tree, tree);

	nlink = bvh->tree;

	bvh->root = bvh->tree->link;
	bvh->root->isleaf = 0;
	bvh->root->parent = NULL;
	bvh->root->nodes[0] = bvh->root->nodes[1] = bvh->root->nodes[1] = bvh->root->nodes[3] = NULL;

	if(bvh->numfaces<=1)
	{
		bvh->root->tri_index = 0;	// Why that? --> only one face there 
		bvh->root->isleaf = 1;
		bvh->root->traversed = 0;
		bvh->root->count_nodes = 0;
		bvh->leaf_root = bvh->root;
		bvh->leaf_tree = bvh->root;
		bvh->root->nextLeaf = NULL;
		bvh->root->prevLeaf = NULL;
	}
	else
	{
		// create face boxes		
		face_list = MEM_callocN (bvh->numfaces * sizeof (CollisionTree *), "CollisionTree");
		if (face_list == NULL) 
		{
			printf("bvh_build: Out of memory for face_list.\n");
			bvh_free(bvh);
			return;
		}

		// create face boxes
		for(i = 0, k = 0; i < bvh->numfaces; i++)
		{
			LinkNode *tnlink;

			tree = (CollisionTree *)MEM_callocN(sizeof(CollisionTree), "CollisionTree");
			// TODO: check succesfull alloc

			tnlink = BLI_linklist_append_fast(&nlink->next, tree);

			face_list[i] = tree;
			tree->tri_index = i;
			tree->isleaf = 1;
			tree->nextLeaf = NULL;
			tree->prevLeaf = bvh->leaf_tree;
			tree->parent = NULL;
			tree->count_nodes = 0;

			if(i==0)
			{
				bvh->leaf_tree = bvh->leaf_root = tree;
			}
			else
			{
				bvh->leaf_tree->nextLeaf = tree;
				bvh->leaf_tree = bvh->leaf_tree->nextLeaf;
			}		

			tree->nodes[0] = tree->nodes[1] = tree->nodes[2] = tree->nodes[3] = NULL;		

			bvh_calc_DOP_hull_static(bvh, &face_list[i], 1, tree->bv, tree);

			// inflate the bv with some epsilon
			for (j = KDOP_START; j < KDOP_END; j++)
			{
				tree->bv[(2 * j)] -= bvh->epsilon; // minimum 
				tree->bv[(2 * j) + 1] += bvh->epsilon;	// maximum 
			}
			
			nlink = tnlink;
		}
		
		// build root bvh
		bvh_calc_DOP_hull_from_faces(bvh, face_list, bvh->numfaces, bvh->root->bv);

		// This is the traversal function. 
		bvh_div_env_node(bvh, bvh->root, face_list, 0, bvh->numfaces-1, 0, nlink);
		if (face_list)
			MEM_freeN(face_list);
		
	}
	
}

// bvh_overlap - is it possbile for 2 bv's to collide ?
DO_INLINE int bvh_overlap(float *bv1, float *bv2)
{
	int i = 0;
	for (i = KDOP_START; i < KDOP_END; i++)
	{
		// Minimum test.
		if (bv1[(2 * i)] > bv2[(2 * i) + 1]) 
		{
			return 0;
		}
		// Maxiumum test.
		if (bv2[(2 * i)] > bv1[(2 * i) + 1]) 
		{
			return 0;
		}
	}
	
	return 1;
}

// bvh_overlap_self - is it possbile for 2 bv's to selfcollide ?
DO_INLINE int bvh_overlap_self(CollisionTree * tree1, CollisionTree * tree2)
{
	// printf("overlap: %f, q: %f\n", (saacos(INPR(tree1->normal, tree2->normal)) / 2.0)+MAX2(tree1->alpha, tree2->alpha), saacos(INPR(tree1->normal, tree2->normal)));
	
	if((saacos(INPR(tree1->normal, tree2->normal)) / 2.0)+MAX2(tree1->alpha, tree2->alpha) > M_PI)
	{
		return 1;
	}
	else
		return 0;
}

/**
 * bvh_traverse - traverse two bvh trees looking for potential collisions.
 *
 * max collisions are n*n collisions --> every triangle collide with
 * every other triangle that doesn't require any realloc, but uses
 * much memory
 */
int bvh_traverse ( ModifierData * md1, ModifierData * md2, CollisionTree * tree1, CollisionTree * tree2, float step, CM_COLLISION_RESPONSE collision_response, int selfcollision)
{
	int i = 0, ret=0, overlap = 0;
	
	/*
	// Shouldn't be possible
	if(!tree1 || !tree2)
	{
	printf("Error: no tree there\n");
	return 0;
}
	*/	
	
	if(selfcollision)
		overlap = bvh_overlap_self(tree1, tree2);
	else
		overlap = bvh_overlap(tree1->bv, tree2->bv);
	
	if (overlap) 
	{		
		// Check if this node in the first tree is a leaf
		if (tree1->isleaf) 
		{
			// Check if this node in the second tree a leaf
			if (tree2->isleaf) 
			{
				// Provide the collision response.
				
				if(collision_response)
					collision_response (md1, md2, tree1, tree2);
				return 1;
			}
			else 
			{
				// Process the quad tree.
				for (i = 0; i < 4; i++)
				{
					// Only traverse nodes that exist.
					if (tree2->nodes[i] && bvh_traverse (md1, md2, tree1, tree2->nodes[i], step, collision_response, selfcollision))
						ret = 1;
				}
			}
		}
		else 
		{
			// Process the quad tree.
			for (i = 0; i < 4; i++)
			{
				// Only traverse nodes that exist.
				if (tree1->nodes [i] && bvh_traverse (md1, md2, tree1->nodes[i], tree2, step, collision_response, selfcollision))
					ret = 1;
			}
		}
	}

	return ret;
}
// bottom up update of bvh tree:
// join the 4 children here
void bvh_join(CollisionTree *tree)
{
	int	i = 0, j = 0;
	float max = 0;
	
	if (!tree)
		return;
	
	for (i = 0; i < 4; i++)
	{
		if (tree->nodes[i]) 
		{
			for (j = KDOP_START; j < KDOP_END; j++)
			{
				// update minimum 
				if ((tree->nodes[i]->bv[(2 * j)] < tree->bv[(2 * j)]) || (i == 0)) 
				{
					tree->bv[(2 * j)] = tree->nodes[i]->bv[(2 * j)];
				}
				// update maximum 
				if ((tree->nodes[i]->bv[(2 * j) + 1] > tree->bv[(2 * j) + 1])|| (i == 0))
				{
					tree->bv[(2 * j) + 1] = tree->nodes[i]->bv[(2 * j) + 1];
				}
			}
			
			/* for selfcollisions */
			if(!i)
			{
				tree->alpha = tree->nodes[i]->alpha;
				VECCOPY(tree->normal, tree->nodes[i]->normal);
			}
			else
			{
				tree->alpha += saacos(INPR(tree->normal, tree->nodes[i]->normal)) / 2.0;
				VECADD(tree->normal, tree->normal, tree->nodes[i]->normal);
				VecMulf(tree->normal, 0.5);
				max = MAX2(max, tree->nodes[i]->alpha);
			}
			
		}
		else
			break;
	}
	
	tree->alpha += max;
}

// update static bvh
/* you have to update the bvh position before calling this function */
void bvh_update(BVH * bvh, int moving)
{
	CollisionTree *leaf, *parent;
	int traversecheck = 1;	// if this is zero we don't go further 
	unsigned int j = 0;
	
	for (leaf = bvh->leaf_root; leaf; leaf = leaf->nextLeaf)
	{
		traversecheck = 1;
		if ((leaf->parent) && (leaf->parent->traversed == leaf->parent->count_nodes)) 
		{			
			leaf->parent->traversed = 0;
		}
		if(!moving)
			bvh_calc_DOP_hull_static(bvh, &leaf, 1, leaf->bv, leaf);
		else
			bvh_calc_DOP_hull_moving(bvh, &leaf, 1, leaf->bv, leaf);

		// inflate the bv with some epsilon
		for (j = KDOP_START; j < KDOP_END; j++)
		{			
			leaf->bv[(2 * j)] -= bvh->epsilon; // minimum 
			leaf->bv[(2 * j) + 1] += bvh->epsilon;	// maximum 
		}

		for (parent = leaf->parent; parent; parent = parent->parent)
		{			
			if (traversecheck) 
			{
				parent->traversed++;	// we tried to go up in hierarchy 
				if (parent->traversed < parent->count_nodes) 
				{
					traversecheck = 0;

					if (parent->parent) 
					{
						if (parent->parent->traversed == parent->parent->count_nodes) 
						{
							parent->parent->traversed = 0;
						}
					}
					break;	// we do not need to check further 
				}
				else 
				{
					bvh_join(parent);
				}
			}

		}
	}	
}

