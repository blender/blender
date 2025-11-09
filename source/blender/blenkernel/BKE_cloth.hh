/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_math_vector_types.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_set.hh"

#include <cfloat>

struct BVHTree;
struct ClothVertex;
struct ClothModifierData;
struct CollisionModifierData;
struct Implicit_Data;
struct Depsgraph;
struct LinkNode;
struct Mesh;
struct Object;
struct Scene;

#define DO_INLINE MALWAYS_INLINE

/* Goal defines. */
#define SOFTGOALSNAP 0.999f

/* This is approximately the smallest number that can be
 * represented by a float, given its precision. */
#define ALMOST_ZERO FLT_EPSILON

/* Bits to or into the #ClothVertex.flags. */
enum eClothVertexFlag {
  CLOTH_VERT_FLAG_PINNED = (1 << 0),
  CLOTH_VERT_FLAG_NOSELFCOLL = (1 << 1), /* vertex NOT used for self collisions */
  CLOTH_VERT_FLAG_NOOBJCOLL = (1 << 2),  /* vertex NOT used for object collisions */
};

struct ClothHairData {
  float loc[3];
  float rot[3][3];
  float rest_target[3]; /* rest target direction for each segment */
  float radius;
  float bending_stiffness;
};

struct ClothSolverResult {
  int status;

  int max_iterations, min_iterations;
  float avg_iterations;
  float max_error, min_error, avg_error;
};

/**
 * This structure describes a cloth object against which the
 * simulation can run.
 *
 * The m and n members of this structure represent the assumed
 * rectangular ordered grid for which the original paper is written.
 * At some point they need to disappear and we need to determine our
 * own connectivity of the mesh based on the actual edges in the mesh.
 */
struct Cloth {
  ClothVertex *verts;            /* The vertices that represent this cloth. */
  LinkNode *springs;             /* The springs connecting the mesh. */
  unsigned int numsprings;       /* The count of springs. */
  unsigned int mvert_num;        /* The number of verts == m * n. */
  unsigned int primitive_num;    /* Number of triangles for cloth and edges for hair. */
  unsigned char old_solver_type; /* unused, only 1 solver here */
  unsigned char pad2;
  short pad3;
  BVHTree *bvhtree;     /* collision tree for this cloth object */
  BVHTree *bvhselftree; /* Collision tree for this cloth object (may be same as BVH-tree). */
  blender::int3 *vert_tris;
  Implicit_Data *implicit;                    /* our implicit solver connects to this pointer */
  blender::Set<blender::OrderedEdge> edgeset; /* Used for self-collisions. */
  int last_frame;
  float initial_mesh_volume;     /* Initial volume of the mesh. Used for pressure */
  float average_acceleration[3]; /* Moving average of overall acceleration. */
  const blender::int2 *edges;    /* Used for hair collisions. */
  blender::Set<blender::OrderedEdge> sew_edge_graph; /* Sewing edges. */
};

/**
 * The definition of a cloth vertex.
 */
struct ClothVertex {
  int flags;                  /* General flags per vertex. */
  float v[3];                 /* The velocity of the point. */
  float xconst[3];            /* constrained position         */
  float x[3];                 /* The current position of this vertex. */
  float xold[3];              /* The previous position of this vertex. */
  float tx[3];                /* temporary position */
  float txold[3];             /* temporary old position */
  float tv[3];                /* temporary "velocity", mostly used as tv = tx-txold */
  float mass;                 /* mass / weight of the vertex      */
  float goal;                 /* goal, from SB            */
  float impulse[3];           /* used in collision.cc */
  float xrest[3];             /* rest position of the vertex */
  float dcvel[3];             /* delta velocities to be applied by collision response */
  unsigned int impulse_count; /* same as above */
  float avg_spring_len;       /* average length of connected springs */
  float struct_stiff;
  float bend_stiff;
  float shear_stiff;
  int spring_count;      /* how many springs attached? */
  float shrink_factor;   /* how much to shrink this cloth */
  float internal_stiff;  /* internal spring stiffness scaling */
  float pressure_factor; /* how much pressure should affect this vertex */
};

/**
 * The definition of a spring.
 */
struct ClothSpring {
  int ij;              /* `Pij` from the paper, one end of the spring. */
  int kl;              /* `Pkl` from the paper, one end of the spring. */
  int mn;              /* For hair springs: third vertex index; For bending springs: edge index. */
  int *pa;             /* Array of vert indices for poly a (for bending springs). */
  int *pb;             /* Array of vert indices for poly b (for bending springs). */
  int la;              /* Length of `*pa`. */
  int lb;              /* Length of `*pb`. */
  float restlen;       /* The original length of the spring. */
  float restang;       /* The original angle of the bending springs. */
  int type;            /* Types defined in BKE_cloth.hh ("springType"). */
  int flags;           /* Defined in BKE_cloth.hh, e.g. deactivated due to tearing. */
  float lin_stiffness; /* Linear stiffness factor from the vertex groups. */
  float ang_stiffness; /* Angular stiffness factor from the vertex groups. */
  float editrestlen;

  /* angular bending spring target and derivatives */
  float target[3];
};

/* Some macro enhancements for vector treatment. */
#define VECSUBADDSS(v1, v2, aS, v3, bS) \
  { \
    *(v1) -= *(v2) * aS + *(v3) * bS; \
    *(v1 + 1) -= *(v2 + 1) * aS + *(v3 + 1) * bS; \
    *(v1 + 2) -= *(v2 + 2) * aS + *(v3 + 2) * bS; \
  } \
  ((void)0)
#define VECADDSS(v1, v2, aS, v3, bS) \
  { \
    *(v1) = *(v2) * aS + *(v3) * bS; \
    *(v1 + 1) = *(v2 + 1) * aS + *(v3 + 1) * bS; \
    *(v1 + 2) = *(v2 + 2) * aS + *(v3 + 2) * bS; \
  } \
  ((void)0)
#define VECADDS(v1, v2, v3, bS) \
  { \
    *(v1) = *(v2) + *(v3) * bS; \
    *(v1 + 1) = *(v2 + 1) + *(v3 + 1) * bS; \
    *(v1 + 2) = *(v2 + 2) + *(v3 + 2) * bS; \
  } \
  ((void)0)
#define VECSUBMUL(v1, v2, aS) \
  { \
    *(v1) -= *(v2) * aS; \
    *(v1 + 1) -= *(v2 + 1) * aS; \
    *(v1 + 2) -= *(v2 + 2) * aS; \
  } \
  ((void)0)
#define VECSUBS(v1, v2, v3, bS) \
  { \
    *(v1) = *(v2) - *(v3) * bS; \
    *(v1 + 1) = *(v2 + 1) - *(v3 + 1) * bS; \
    *(v1 + 2) = *(v2 + 2) - *(v3 + 2) * bS; \
  } \
  ((void)0)
#define VECADDMUL(v1, v2, aS) \
  { \
    *(v1) += *(v2) * aS; \
    *(v1 + 1) += *(v2 + 1) * aS; \
    *(v1 + 2) += *(v2 + 2) * aS; \
  } \
  ((void)0)

/* Spring types as defined in the paper. */
enum CLOTH_SPRING_TYPES {
  CLOTH_SPRING_TYPE_STRUCTURAL = (1 << 1),
  CLOTH_SPRING_TYPE_SHEAR = (1 << 2),
  CLOTH_SPRING_TYPE_BENDING = (1 << 3),
  CLOTH_SPRING_TYPE_GOAL = (1 << 4),
  CLOTH_SPRING_TYPE_SEWING = (1 << 5),
  CLOTH_SPRING_TYPE_BENDING_HAIR = (1 << 6),
  CLOTH_SPRING_TYPE_INTERNAL = (1 << 7),
};

/* SPRING FLAGS */
enum CLOTH_SPRINGS_FLAGS {
  CLOTH_SPRING_FLAG_DEACTIVATE = (1 << 1),
  CLOTH_SPRING_FLAG_NEEDED = (1 << 2), /* Springs has values to be applied. */
};

/* -------------------------------------------------------------------- */
/* collision.cc */

struct CollPair;

struct ColliderContacts {
  Object *ob;
  CollisionModifierData *collmd;

  CollPair *collisions;
  int totcollisions;
};

/* needed for implicit.c */
int cloth_bvh_collision(
    Depsgraph *depsgraph, Object *ob, ClothModifierData *clmd, float step, float dt);

/* -------------------------------------------------------------------- */
/* cloth.cc */

/* Needed for modifier.cc */
/** Frees all. */
void cloth_free_modifier_extern(ClothModifierData *clmd);
/** Frees all. */
void cloth_free_modifier(ClothModifierData *clmd);
void clothModifier_do(ClothModifierData *clmd,
                      Depsgraph *depsgraph,
                      Scene *scene,
                      Object *ob,
                      const Mesh *mesh,
                      float (*vertexCos)[3]);

int cloth_uses_vgroup(ClothModifierData *clmd);

/* Needed for collision.cc */
void bvhtree_update_from_cloth(ClothModifierData *clmd, bool moving, bool self);

/* Needed for button_object.c */
void cloth_clear_cache(Object *ob, ClothModifierData *clmd, float framenr);

void cloth_parallel_transport_hair_frame(float mat[3][3],
                                         const float dir_old[3],
                                         const float dir_new[3]);
