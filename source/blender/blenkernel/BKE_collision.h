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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_COLLISION_H__
#define __BKE_COLLISION_H__

/** \file BKE_collision.h
 *  \ingroup bke
 *  \author Daniel Genrich
 */

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

/* types */
#include "BKE_collision.h"
#include "DNA_cloth_types.h"

#include "BLI_kdopbvh.h"

struct CollisionModifierData;
struct Collection;
struct MFace;
struct MVert;
struct Object;
struct Scene;
struct Depsgraph;
struct MVertTri;

////////////////////////////////////////
// used for collisions in collision.c
////////////////////////////////////////

/* COLLISION FLAGS */
typedef enum {
	COLLISION_IN_FUTURE =       (1 << 1),
#ifdef WITH_ELTOPO
	COLLISION_USE_COLLFACE =    (1 << 2),
	COLLISION_IS_EDGES =        (1 << 3),
#endif
	COLLISION_INACTIVE =        (1 << 4),
} COLLISION_FLAGS;


////////////////////////////////////////
// used for collisions in collision.c
////////////////////////////////////////
/* used for collisions in collision.c */
typedef struct CollPair {
	unsigned int face1; // cloth face
	unsigned int face2; // object face
	float distance;
	float normal[3];
	float vector[3]; // unnormalized collision vector: p2-p1
	float pa[3], pb[3]; // collision point p1 on face1, p2 on face2
	int flag;
	float time; // collision time, from 0 up to 1

	/* mesh-mesh collision */
#ifdef WITH_ELTOPO /*either ap* or bp* can be set, but not both*/
	float bary[3];
	int ap1, ap2, ap3, collp, bp1, bp2, bp3;
	int collface;
#else
	int ap1, ap2, ap3, bp1, bp2, bp3;
#endif
	int pointsb[4];
}
CollPair;

/* used for collisions in collision.c */
typedef struct EdgeCollPair {
	unsigned int p11, p12, p21, p22;
	float normal[3];
	float vector[3];
	float time;
	int lastsign;
	float pa[3], pb[3]; // collision point p1 on face1, p2 on face2
}
EdgeCollPair;

/* used for collisions in collision.c */
typedef struct FaceCollPair {
	unsigned int p11, p12, p13, p21;
	float normal[3];
	float vector[3];
	float time;
	int lastsign;
	float pa[3], pb[3]; // collision point p1 on face1, p2 on face2
}
FaceCollPair;

////////////////////////////////////////



/////////////////////////////////////////////////
// forward declarations
/////////////////////////////////////////////////

/////////////////////////////////////////////////
// used in modifier.c from collision.c
/////////////////////////////////////////////////

BVHTree *bvhtree_build_from_mvert(
        const struct MVert *mvert,
        const struct MVertTri *tri, int tri_num,
        float epsilon);
void bvhtree_update_from_mvert(
        BVHTree *bvhtree,
        const struct MVert *mvert, const struct MVert *mvert_moving,
        const struct MVertTri *tri, int tri_num,
        bool moving);

/////////////////////////////////////////////////

// move Collision modifier object inter-frame with step = [0,1]
// defined in collisions.c
void collision_move_object(struct CollisionModifierData *collmd, float step, float prevstep);

void collision_get_collider_velocity(float vel_old[3], float vel_new[3], struct CollisionModifierData *collmd, struct CollPair *collpair);


/* Collision relations for dependency graph build. */

typedef struct CollisionRelation {
	struct CollisionRelation *next, *prev;
	struct Object *ob;
} CollisionRelation;

struct ListBase *BKE_collision_relations_create(
        struct Depsgraph *depsgraph,
        struct Collection *collection,
        unsigned int modifier_type);
void BKE_collision_relations_free(struct ListBase *relations);

/* Collision object lists for physics simulation evaluation. */

struct Object **BKE_collision_objects_create(
        struct Depsgraph *depsgraph,
        struct Object *self,
        struct Collection *collection,
        unsigned int *numcollobj,
        unsigned int modifier_type);
void BKE_collision_objects_free(struct Object **objects);

typedef struct ColliderCache {
	struct ColliderCache *next, *prev;
	struct Object *ob;
	struct CollisionModifierData *collmd;
} ColliderCache;

struct ListBase *BKE_collider_cache_create(
        struct Depsgraph *scene,
        struct Object *self,
        struct Collection *collection);
void BKE_collider_cache_free(struct ListBase **colliders);

/////////////////////////////////////////////////



/////////////////////////////////////////////////

#endif
