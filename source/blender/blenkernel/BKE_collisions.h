/**
 * BKE_cloth.h
 *
 * $Id: BKE_cloth.h,v 1.1 2007/08/01 02:07:27 daniel Exp $
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef BKE_COLLISIONS_H
#define BKE_COLLISIONS_H

#include <math.h>
#include <stdlib.h>
#include <string.h>
/* types */
#include "BLI_linklist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_DerivedMesh.h"

// used in kdop.c and collision.c
typedef struct Tree
{
	struct Tree *nodes[4]; // 4 children --> quad-tree
	struct Tree *parent;
	struct Tree *nextLeaf;
	struct Tree *prevLeaf;
	float	bv[26]; // Bounding volume of all nodes / we have 7 axes on a 14-DOP
	unsigned int tri_index; // this saves the index of the face
	int	count_nodes; // how many nodes are used
	int	traversed;  // how many nodes already traversed until this level?
	int	isleaf;
}
Tree;

typedef struct Tree TreeNode;

typedef struct BVH
{
	unsigned int 	numfaces;
	unsigned int 	numverts;
	MVert	 	*xnew; // position of verts at time n 
	MVert	 	*x; // position of verts at time n-1
	MFace 		*mfaces; // just a pointer to the original datastructure
	struct LinkNode *tree;
	TreeNode 	*root; // TODO: saving the root --> is this really needed? YES!
	TreeNode 	*leaf_tree; /* Tail of the leaf linked list.	*/
	TreeNode 	*leaf_root;	/* Head of the leaf linked list.	*/
	float 		epsilon; /* epslion is used for inflation of the k-dop	   */
	int 		flags; /* bvhFlags */
}
BVH;

/* used for collisions in kdop.c and also collision.c*/
typedef struct CollisionPair
{
	unsigned int indexA, indexB;
}
CollisionPair;


/////////////////////////////////////////////////
// forward declarations
/////////////////////////////////////////////////

void bvh_free ( BVH *bvh );
BVH *bvh_build (DerivedMesh *dm, MVert *x, MVert *xold, unsigned int numverts, float epsilon);

int bvh_traverse(Tree *tree1, Tree *tree2, LinkNode *collision_list);
void bvh_update(DerivedMesh *dm, BVH * bvh, int moving);

LinkNode *BLI_linklist_append_fast ( LinkNode **listp, void *ptr );


/////////////////////////////////////////////////

#endif

