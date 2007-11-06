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

#include "BLI_linklist.h"
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

#define CLOTH_MAX_THREAD 2


/* goal defines */
#define SOFTGOALSNAP  0.999f

/* This is approximately the smallest number that can be
* represented by a float, given its precision. */
#define ALMOST_ZERO		0.000001

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
    CLOTH_SIMSETTINGS_FLAG_RESET = ( 1 << 1 ),		// The CM object requires a reinitializaiton.
    CLOTH_SIMSETTINGS_FLAG_COLLOBJ = ( 1 << 2 ), 	// object is only collision object, no cloth simulation is done
    CLOTH_SIMSETTINGS_FLAG_GOAL = ( 1 << 3 ), 		// we have goals enabled
    CLOTH_SIMSETTINGS_FLAG_TEARING = ( 1 << 4 ), // true if tearing is enabled
    CLOTH_SIMSETTINGS_FLAG_CCACHE_PROTECT = ( 1 << 5 ),
    CLOTH_SIMSETTINGS_FLAG_BIG_FORCE = ( 1 << 6 ), // true if we have big spring force for bending
    CLOTH_SIMSETTINGS_FLAG_SLEEP = ( 1 << 7 ), // true if we let the cloth go to sleep
} CLOTH_SIMSETTINGS_FLAGS;

/* SPRING FLAGS */
typedef enum
{
    CLOTH_COLLISIONSETTINGS_FLAG_ENABLED = ( 1 << 1 ),
} CLOTH_COLLISIONSETTINGS_FLAGS;

/* Spring types as defined in the paper.*/
typedef enum
{
    CLOTH_SPRING_TYPE_STRUCTURAL = 0,
    CLOTH_SPRING_TYPE_SHEAR,
    CLOTH_SPRING_TYPE_BENDING,
} CLOTH_SPRING_TYPES;

/* SPRING FLAGS */
typedef enum
{
    CLOTH_SPRING_FLAG_DEACTIVATE = ( 1 << 1 ),
    CLOTH_SPRING_FLAG_NEEDED = ( 1 << 2 ), // springs has values to be applied
} CLOTH_SPRINGS_FLAGS;

/* Bits to or into the ClothVertex.flags. */
#define CVERT_FLAG_PINNED	1
#define CVERT_FLAG_COLLISION	2


// needed for buttons_object.c
void cloth_clear_cache(Object *ob, ClothModifierData *clmd, float framenr);
void cloth_free_modifier ( ClothModifierData *clmd );

// needed for cloth.c
void implicit_set_positions ( ClothModifierData *clmd );

// from cloth.c, needed for modifier.c
DerivedMesh *clothModifier_do(ClothModifierData *clmd,Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc);

// needed in implicit.c
int cloth_bvh_objcollision(ClothModifierData * clmd, float step, float prevstep, float dt);

////////////////////////////////////////////////


/////////////////////////////////////////////////
// cloth.c
////////////////////////////////////////////////
void cloth_free_modifier ( ClothModifierData *clmd );
void cloth_init ( ClothModifierData *clmd );
void cloth_update_normals ( ClothVertex *verts, int nVerts, MFace *face, int totface );
////////////////////////////////////////////////


/* Typedefs for function pointers we need for solvers and collision detection. */
typedef void ( *CM_COLLISION_SELF ) ( ClothModifierData *clmd, int step );
// typedef void ( *CM_COLLISION_OBJ ) ( ClothModifierData *clmd, int step, CM_COLLISION_RESPONSE collision_response );


/* This enum provides the IDs for our solvers. */
// only one available in the moment
typedef enum {
    CM_IMPLICIT = 0,
    CM_VERLET = 1,
} CM_SOLVER_ID;


/* This structure defines how to call the solver.
*/
typedef struct
{
	char		*name;
	CM_SOLVER_ID	id;
	int	( *init ) ( Object *ob, ClothModifierData *clmd );
	int	( *solver ) ( Object *ob, float framenr, ClothModifierData *clmd, ListBase *effectors );
	int	( *free ) ( ClothModifierData *clmd );
}
CM_SOLVER_DEF;


/* new C implicit simulator */
int implicit_init ( Object *ob, ClothModifierData *clmd );
int implicit_free ( ClothModifierData *clmd );
int implicit_solver ( Object *ob, float frame, ClothModifierData *clmd, ListBase *effectors );

/* explicit verlet simulator */
int verlet_init ( Object *ob, ClothModifierData *clmd );
int verlet_free ( ClothModifierData *clmd );
int verlet_solver ( Object *ob, float frame, ClothModifierData *clmd, ListBase *effectors );

/* used for caching in implicit.c */
typedef struct Frame
{
	ClothVertex *verts;
	ClothSpring *springs;
	unsigned int numverts, numsprings;
	float time; /* we need float since we want to support sub-frames */
	float (*x)[3];
	float (*xold)[3];
	float (*v)[3];
	float (*current_xold)[3];
}
Frame;

/* used for collisions in collision.c */
/*
typedef struct CollPair
{
	unsigned int face1; // cloth face
	unsigned int face2; // object face
	double distance; // magnitude of vector
	float normal[3];
	float vector[3]; // unnormalized collision vector: p2-p1
	float pa[3], pb[3]; // collision point p1 on face1, p2 on face2
	int lastsign; // indicates if the distance sign has changed, unused itm
	float time; // collision time, from 0 up to 1
	unsigned int ap1, ap2, ap3, bp1, bp2, bp3, bp4;
	unsigned int pointsb[4];
}
CollPair;
*/

/* used for collisions in collision.c */
typedef struct EdgeCollPair
{
	unsigned int p11, p12, p21, p22;
	float normal[3];
	float vector[3];
	float time;
	int lastsign;
	float pa[3], pb[3]; // collision point p1 on face1, p2 on face2
}
EdgeCollPair;

/* used for collisions in collision.c */
typedef struct FaceCollPair
{
	unsigned int p11, p12, p13, p21;
	float normal[3];
	float vector[3];
	float time;
	int lastsign;
	float pa[3], pb[3]; // collision point p1 on face1, p2 on face2
}
FaceCollPair;

// function definitions from implicit.c
DO_INLINE void mul_fvector_S(float to[3], float from[3], float scalar);

#endif

