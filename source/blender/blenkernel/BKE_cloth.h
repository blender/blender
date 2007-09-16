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
#ifndef BKE_CLOTH_H
#define BKE_CLOTH_H

#include "BKE_DerivedMesh.h"
#include "DNA_customdata_types.h"
#include "BKE_customdata.h"
#include "DNA_meshdata_types.h"

struct Object;
struct Cloth;
struct MFace;
struct DerivedMesh;

// this is needed for inlining behaviour
#ifndef _WIN32
	#define LINUX 
	#define DO_INLINE inline
#else
	#define DO_INLINE
#endif


/* goal defines */
#define SOFTGOALSNAP  0.999f 

/* This is approximately the smallest number that can be
* represented by a float, given its precision. */
#define ALMOST_ZERO		0.0000001

/* Bits to or into the ClothVertex.flags. */
#define CVERT_FLAG_PINNED	1
#define CVERT_FLAG_COLLISION	2


// some macro enhancements for vector treatment
#define VECADDADD(v1,v2,v3) 	{*(v1)+= *(v2) + *(v3); *(v1+1)+= *(v2+1) + *(v3+1); *(v1+2)+= *(v2+2) + *(v3+2);}
#define VECSUBADD(v1,v2,v3) 	{*(v1)-= *(v2) + *(v3); *(v1+1)-= *(v2+1) + *(v3+1); *(v1+2)-= *(v2+2) + *(v3+2);}
#define VECADDSUB(v1,v2,v3) 	{*(v1)+= *(v2) - *(v3); *(v1+1)+= *(v2+1) - *(v3+1); *(v1+2)+= *(v2+2) - *(v3+2);}
#define VECSUBADDSS(v1,v2,aS,v3,bS) 	{*(v1)-= *(v2)*aS + *(v3)*bS; *(v1+1)-= *(v2+1)*aS + *(v3+1)*bS; *(v1+2)-= *(v2+2)*aS + *(v3+2)*bS;}
#define VECADDSUBSS(v1,v2,aS,v3,bS) 	{*(v1)+= *(v2)*aS - *(v3)*bS; *(v1+1)+= *(v2+1)*aS - *(v3+1)*bS; *(v1+2)+= *(v2+2)*aS - *(v3+2)*bS;}
#define VECADDSS(v1,v2,aS,v3,bS) 	{*(v1)= *(v2)*aS + *(v3)*bS; *(v1+1)= *(v2+1)*aS + *(v3+1)*bS; *(v1+2)= *(v2+2)*aS + *(v3+2)*bS;}
#define VECADDS(v1,v2,v3,bS) 	{*(v1)= *(v2) + *(v3)*bS; *(v1+1)= *(v2+1) + *(v3+1)*bS; *(v1+2)= *(v2+2) + *(v3+2)*bS;}
#define VECSUBMUL(v1,v2,aS) 	{*(v1)-= *(v2) * aS; *(v1+1)-= *(v2+1) * aS; *(v1+2)-= *(v2+2) * aS;}
#define VECSUBS(v1,v2,v3,bS) 	{*(v1)= *(v2) - *(v3)*bS; *(v1+1)= *(v2+1) - *(v3+1)*bS; *(v1+2)= *(v2+2) - *(v3+2)*bS;}
#define VECSUBSB(v1,v2, v3,bS) 	{*(v1)= (*(v2)- *(v3))*bS; *(v1+1)= (*(v2+1) - *(v3+1))*bS; *(v1+2)= (*(v2+2) - *(v3+2))*bS;}
#define VECMULS(v1,aS) 	{*(v1)*= aS; *(v1+1)*= aS; *(v1+2)*= *aS;}
#define VECADDMUL(v1,v2,aS) 	{*(v1)+= *(v2) * aS; *(v1+1)+= *(v2+1) * aS; *(v1+2)+= *(v2+2) * aS;}

/* SIMULATION FLAGS: goal flags,.. */
/* These are the bits used in SimSettings.flags. */
typedef enum 
{
	CSIMSETT_FLAG_RESET = (1 << 1),		// The CM object requires a reinitializaiton.
	CSIMSETT_FLAG_COLLOBJ = (1 << 2), 	// object is only collision object, no cloth simulation is done
	CSIMSETT_FLAG_GOAL = (1 << 3), 		// we have goals enabled
	CSIMSETT_FLAG_CCACHE_FREE_ALL = (1 << 4),  // delete all from cache
	CSIMSETT_FLAG_CCACHE_FREE_PART = (1 << 5), // delete some part of cache
	CSIMSETT_FLAG_TEARING_ENABLED = (1 << 6), // true if tearing is enabled
} CSIMSETT_FLAGS;

/* Spring types as defined in the paper.*/
typedef enum 
{
	STRUCTURAL = 0,
	SHEAR,
	BENDING,
} springType;

/* SPRING FLAGS */
typedef enum 
{
	CSPRING_FLAG_DEACTIVATE = (1 << 1),
} CSPRINGS_FLAGS;

// needed for buttons_object.c
void cloth_cache_free(ClothModifierData *clmd, float time);
void cloth_free_modifier (ClothModifierData *clmd);

// needed for cloth.c
void implicit_set_positions (ClothModifierData *clmd);

// from cloth.c, needed for modifier.c
void clothModifier_do(ClothModifierData *clmd, Object *ob, DerivedMesh *dm, float (*vertexCos)[3], int numverts);

// used in collision.c
typedef struct Tree {
	struct Tree *nodes[4]; // 4 children --> quad-tree
	struct Tree *parent;
	struct Tree *nextLeaf; 
	struct Tree *prevLeaf;
	float	bv[26]; // Bounding volume of all nodes / we have 7 axes on a 14-DOP
	unsigned int tri_index; // this saves the index of the face
	int	count_nodes; // how many nodes are used
	int	traversed;  // how many nodes already traversed until this level?
	int	isleaf;
} Tree;

typedef struct Tree TreeNode;

typedef struct BVH{ 
	unsigned int 	numfaces;
	unsigned int 	numverts;
	ClothVertex 	*verts; // just a pointer to the original datastructure
	MFace 		*mfaces; // just a pointer to the original datastructure
	struct LinkNode *tree;
	TreeNode 	*root; // TODO: saving the root --> is this really needed? YES!
	TreeNode 	*leaf_tree; /* Tail of the leaf linked list.	*/
	TreeNode 	*leaf_root;	/* Head of the leaf linked list.	*/
	float 		epsilon; /* epslion is used for inflation of the k-dop	   */
	int 		flags; /* bvhFlags */		   
} BVH;

typedef void (*CM_COLLISION_RESPONSE) (ClothModifierData *clmd, ClothModifierData *coll_clmd, Tree * tree1, Tree * tree2);


/////////////////////////////////////////////////
// collision.c
////////////////////////////////////////////////

// needed for implicit.c
void bvh_collision_response(ClothModifierData *clmd, ClothModifierData *coll_clmd, Tree * tree1, Tree * tree2);
int cloth_bvh_objcollision(ClothModifierData * clmd, float step, CM_COLLISION_RESPONSE collision_response, float dt);

////////////////////////////////////////////////


/////////////////////////////////////////////////
// kdop.c
////////////////////////////////////////////////

// needed for implicit.c
void bvh_update_static(ClothModifierData * clmd, BVH * bvh);
void bvh_update_moving(ClothModifierData * clmd, BVH * bvh);

// needed for cloth.c
void bvh_free(BVH * bvh);
BVH *bvh_build (ClothModifierData *clmd, float epsilon);

// needed for collision.c
int bvh_traverse(ClothModifierData * clmd, ClothModifierData * coll_clmd, Tree * tree1, Tree * tree2, float step, CM_COLLISION_RESPONSE collision_response);

////////////////////////////////////////////////



/////////////////////////////////////////////////
// cloth.c
////////////////////////////////////////////////
void cloth_free_modifier (ClothModifierData *clmd);
void cloth_init (ClothModifierData *clmd);
void cloth_deform_verts(struct Object *ob, float framenr, float (*vertexCos)[3], int numVerts, void *derivedData, ClothModifierData *clmd);
void cloth_update_normals (ClothVertex *verts, int nVerts, MFace *face, int totface);

////////////////////////////////////////////////


/* Typedefs for function pointers we need for solvers and collision detection. */
typedef void (*CM_COLLISION_SELF) (ClothModifierData *clmd, int step);
typedef void (*CM_COLLISION_OBJ) (ClothModifierData *clmd, int step, CM_COLLISION_RESPONSE collision_response);


/* This enum provides the IDs for our solvers. */
// only one available in the moment
typedef enum {
	CM_IMPLICIT = 0,
} CM_SOLVER_ID;


/* This structure defines how to call the solver.
*/
typedef struct {
	char		*name;
	CM_SOLVER_ID	id;
	int		(*init) (Object *ob, ClothModifierData *clmd);
	int		(*solver) (Object *ob, float framenr, ClothModifierData *clmd, ListBase *effectors,
	CM_COLLISION_SELF self_collision, CM_COLLISION_OBJ obj_collision);
	int		(*free) (ClothModifierData *clmd);
} CM_SOLVER_DEF;


/* new C implicit simulator */
int implicit_init (Object *ob, ClothModifierData *clmd);
int implicit_free (ClothModifierData *clmd);
int implicit_solver (Object *ob, float frame, ClothModifierData *clmd, ListBase *effectors,
						CM_COLLISION_SELF self_collision, CM_COLLISION_OBJ obj_collision);

/* used for caching in implicit.c */
typedef struct Frame
{
	ClothVertex *verts;
	ClothSpring *springs;
	float time; /* we need float since we want to support sub-frames */
} Frame;

/* used for collisions in collision.c */
typedef struct CollPair
{
	unsigned int face1; // cloth face
	unsigned int face2; // object face
	double distance; // magnitude of vector
	float normal[3]; 
	float vector[3]; // unnormalized collision vector: p2-p1
	float p1[3], p2[3]; // collision point p1 on face1, p2 on face2
	int lastsign; // indicates if the distance sign has changed, unused itm
	float time; // collision time, from 0 up to 1
	int quadA, quadB; // indicates the used triangle of the quad: 0 means verts 1,2,3; 1 means verts 4,1,3
} CollPair;


#endif

