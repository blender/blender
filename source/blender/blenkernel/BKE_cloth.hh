/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_math_inline.h"
#include <float.h>

struct ClothModifierData;
struct CollisionModifierData;
struct Depsgraph;
struct GHash;
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
typedef enum eClothVertexFlag {
  CLOTH_VERT_FLAG_PINNED = (1 << 0),
  CLOTH_VERT_FLAG_NOSELFCOLL = (1 << 1), /* vertex NOT used for self collisions */
  CLOTH_VERT_FLAG_NOOBJCOLL = (1 << 2),  /* vertex NOT used for object collisions */
} eClothVertexFlag;

typedef struct ClothHairData {
  float loc[3];
  float rot[3][3];
  float rest_target[3]; /* rest target direction for each segment */
  float radius;
  float bending_stiffness;
} ClothHairData;

typedef struct ClothSolverResult {
  int status;

  int max_iterations, min_iterations;
  float avg_iterations;
  float max_error, min_error, avg_error;
} ClothSolverResult;

/**
 * This structure describes a cloth object against which the
 * simulation can run.
 *
 * The m and n members of this structure represent the assumed
 * rectangular ordered grid for which the original paper is written.
 * At some point they need to disappear and we need to determine out
 * own connectivity of the mesh based on the actual edges in the mesh.
 */
typedef struct Cloth {
  struct ClothVertex *verts;     /* The vertices that represent this cloth. */
  struct LinkNode *springs;      /* The springs connecting the mesh. */
  unsigned int numsprings;       /* The count of springs. */
  unsigned int mvert_num;        /* The number of verts == m * n. */
  unsigned int primitive_num;    /* Number of triangles for cloth and edges for hair. */
  unsigned char old_solver_type; /* unused, only 1 solver here */
  unsigned char pad2;
  short pad3;
  struct BVHTree *bvhtree;     /* collision tree for this cloth object */
  struct BVHTree *bvhselftree; /* collision tree for this cloth object (may be same as bvhtree) */
  struct MVertTri *tri;
  struct Implicit_Data *implicit; /* our implicit solver connects to this pointer */
  struct EdgeSet *edgeset;        /* used for selfcollisions */
  int last_frame;
  float initial_mesh_volume;      /* Initial volume of the mesh. Used for pressure */
  float average_acceleration[3];  /* Moving average of overall acceleration. */
  const struct vec2i *edges;      /* Used for hair collisions. */
  struct EdgeSet *sew_edge_graph; /* Sewing edges represented using a GHash */
} Cloth;

/**
 * The definition of a cloth vertex.
 */
typedef struct ClothVertex {
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
} ClothVertex;

/**
 * The definition of a spring.
 */
typedef struct ClothSpring {
  int ij;              /* `Pij` from the paper, one end of the spring. */
  int kl;              /* `Pkl` from the paper, one end of the spring. */
  int mn;              /* For hair springs: third vertex index; For bending springs: edge index; */
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
} ClothSpring;

/* Some macro enhancements for vector treatment. */
#define VECSUBADDSS(v1, v2, aS, v3, bS) \
  { \
    *(v1) -= *(v2)*aS + *(v3)*bS; \
    *(v1 + 1) -= *(v2 + 1) * aS + *(v3 + 1) * bS; \
    *(v1 + 2) -= *(v2 + 2) * aS + *(v3 + 2) * bS; \
  } \
  ((void)0)
#define VECADDSS(v1, v2, aS, v3, bS) \
  { \
    *(v1) = *(v2)*aS + *(v3)*bS; \
    *(v1 + 1) = *(v2 + 1) * aS + *(v3 + 1) * bS; \
    *(v1 + 2) = *(v2 + 2) * aS + *(v3 + 2) * bS; \
  } \
  ((void)0)
#define VECADDS(v1, v2, v3, bS) \
  { \
    *(v1) = *(v2) + *(v3)*bS; \
    *(v1 + 1) = *(v2 + 1) + *(v3 + 1) * bS; \
    *(v1 + 2) = *(v2 + 2) + *(v3 + 2) * bS; \
  } \
  ((void)0)
#define VECSUBMUL(v1, v2, aS) \
  { \
    *(v1) -= *(v2)*aS; \
    *(v1 + 1) -= *(v2 + 1) * aS; \
    *(v1 + 2) -= *(v2 + 2) * aS; \
  } \
  ((void)0)
#define VECSUBS(v1, v2, v3, bS) \
  { \
    *(v1) = *(v2) - *(v3)*bS; \
    *(v1 + 1) = *(v2 + 1) - *(v3 + 1) * bS; \
    *(v1 + 2) = *(v2 + 2) - *(v3 + 2) * bS; \
  } \
  ((void)0)
#define VECADDMUL(v1, v2, aS) \
  { \
    *(v1) += *(v2)*aS; \
    *(v1 + 1) += *(v2 + 1) * aS; \
    *(v1 + 2) += *(v2 + 2) * aS; \
  } \
  ((void)0)

/* Spring types as defined in the paper. */
typedef enum {
  CLOTH_SPRING_TYPE_STRUCTURAL = (1 << 1),
  CLOTH_SPRING_TYPE_SHEAR = (1 << 2),
  CLOTH_SPRING_TYPE_BENDING = (1 << 3),
  CLOTH_SPRING_TYPE_GOAL = (1 << 4),
  CLOTH_SPRING_TYPE_SEWING = (1 << 5),
  CLOTH_SPRING_TYPE_BENDING_HAIR = (1 << 6),
  CLOTH_SPRING_TYPE_INTERNAL = (1 << 7),
} CLOTH_SPRING_TYPES;

/* SPRING FLAGS */
typedef enum {
  CLOTH_SPRING_FLAG_DEACTIVATE = (1 << 1),
  CLOTH_SPRING_FLAG_NEEDED = (1 << 2), /* Springs has values to be applied. */
} CLOTH_SPRINGS_FLAGS;

/* -------------------------------------------------------------------- */
/* collision.cc */

struct CollPair;

typedef struct ColliderContacts {
  struct Object *ob;
  struct CollisionModifierData *collmd;

  struct CollPair *collisions;
  int totcollisions;
} ColliderContacts;

/* needed for implicit.c */
int cloth_bvh_collision(struct Depsgraph *depsgraph,
                        struct Object *ob,
                        struct ClothModifierData *clmd,
                        float step,
                        float dt);

/* -------------------------------------------------------------------- */
/* cloth.cc */

/* Needed for modifier.cc */
/** Frees all. */
void cloth_free_modifier_extern(struct ClothModifierData *clmd);
/** Frees all. */
void cloth_free_modifier(struct ClothModifierData *clmd);
void clothModifier_do(struct ClothModifierData *clmd,
                      struct Depsgraph *depsgraph,
                      struct Scene *scene,
                      struct Object *ob,
                      struct Mesh *me,
                      float (*vertexCos)[3]);

int cloth_uses_vgroup(struct ClothModifierData *clmd);

/* Needed for collision.cc */
void bvhtree_update_from_cloth(struct ClothModifierData *clmd, bool moving, bool self);

/* Needed for button_object.c */
void cloth_clear_cache(struct Object *ob, struct ClothModifierData *clmd, float framenr);

void cloth_parallel_transport_hair_frame(float mat[3][3],
                                         const float dir_old[3],
                                         const float dir_new[3]);
