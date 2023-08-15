/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BVHTree;
struct Collection;
struct CollisionModifierData;
struct Depsgraph;
struct MVertTri;
struct Object;

////////////////////////////////////////
// used for collisions in collision.cc
////////////////////////////////////////

/* COLLISION FLAGS */
typedef enum {
  COLLISION_IN_FUTURE = (1 << 1),
#ifdef WITH_ELTOPO
  COLLISION_USE_COLLFACE = (1 << 2),
  COLLISION_IS_EDGES = (1 << 3),
#endif
  COLLISION_INACTIVE = (1 << 4),
} COLLISION_FLAGS;

////////////////////////////////////////
// used for collisions in collision.cc
////////////////////////////////////////
/* used for collisions in collision.cc */
typedef struct CollPair {
  unsigned int face1; /* cloth face */
  unsigned int face2; /* object face */
  float distance;
  float normal[3];
  float vector[3];    /* unnormalized collision vector: p2-p1 */
  float pa[3], pb[3]; /* collision point p1 on face1, p2 on face2 */
  int flag;
  float time; /* collision time, from 0 up to 1 */

  /* mesh-mesh collision */
#ifdef WITH_ELTOPO /* Either ap* or bp* can be set, but not both. */
  float bary[3];
  int ap1, ap2, ap3, collp, bp1, bp2, bp3;
  int collface;
#else
  int ap1, ap2, ap3, bp1, bp2, bp3;
#endif
  /* Barycentric weights of the collision point. */
  float aw1, aw2, aw3, bw1, bw2, bw3;
  int pointsb[4];
} CollPair;

/* used for collisions in collision.cc */
typedef struct EdgeCollPair {
  unsigned int p11, p12, p21, p22;
  float normal[3];
  float vector[3];
  float time;
  int lastsign;
  float pa[3], pb[3]; /* collision point p1 on face1, p2 on face2 */
} EdgeCollPair;

/* used for collisions in collision.cc */
typedef struct FaceCollPair {
  unsigned int p11, p12, p13, p21;
  float normal[3];
  float vector[3];
  float time;
  int lastsign;
  float pa[3], pb[3]; /* collision point p1 on face1, p2 on face2 */
} FaceCollPair;

////////////////////////////////////////

/////////////////////////////////////////////////
// forward declarations
/////////////////////////////////////////////////

/////////////////////////////////////////////////
// used in modifier.cc from collision.cc
/////////////////////////////////////////////////

struct BVHTree *bvhtree_build_from_mvert(const float (*positions)[3],
                                         const struct MVertTri *tri,
                                         int tri_num,
                                         float epsilon);
void bvhtree_update_from_mvert(struct BVHTree *bvhtree,
                               const float (*positions)[3],
                               const float (*positions_moving)[3],
                               const struct MVertTri *tri,
                               int tri_num,
                               bool moving);

/////////////////////////////////////////////////

/**
 * Move Collision modifier object inter-frame with step = [0,1]
 *
 * \param step: is limited from 0 (frame start position) to 1 (frame end position).
 */
void collision_move_object(struct CollisionModifierData *collmd,
                           float step,
                           float prevstep,
                           bool moving_bvh);

void collision_get_collider_velocity(float vel_old[3],
                                     float vel_new[3],
                                     struct CollisionModifierData *collmd,
                                     struct CollPair *collpair);

/* Collision relations for dependency graph build. */

typedef struct CollisionRelation {
  struct CollisionRelation *next, *prev;
  struct Object *ob;
} CollisionRelation;

/**
 * Create list of collision relations in the collection or entire scene.
 * This is used by the depsgraph to build relations, as well as faster
 * lookup of colliders during evaluation.
 */
struct ListBase *BKE_collision_relations_create(struct Depsgraph *depsgraph,
                                                struct Collection *collection,
                                                unsigned int modifier_type);
void BKE_collision_relations_free(struct ListBase *relations);

/* Collision object lists for physics simulation evaluation. */

/**
 * Create effective list of colliders from relations built beforehand.
 * Self will be excluded.
 */
struct Object **BKE_collision_objects_create(struct Depsgraph *depsgraph,
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

/**
 * Create effective list of colliders from relations built beforehand.
 * Self will be excluded.
 */
struct ListBase *BKE_collider_cache_create(struct Depsgraph *depsgraph,
                                           struct Object *self,
                                           struct Collection *collection);
void BKE_collider_cache_free(struct ListBase **colliders);

/////////////////////////////////////////////////

/////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
