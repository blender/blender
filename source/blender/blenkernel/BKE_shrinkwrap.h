/* SPDX-FileCopyrightText: Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

/* Shrinkwrap stuff */
#include "BKE_bvhutils.h"
#include "BLI_bitmap.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Shrinkwrap is composed by a set of functions and options that define the type of shrink.
 *
 * 3 modes are available:
 * - Nearest vertex.
 * - Nearest surface.
 * - Normal projection.
 *
 * #ShrinkwrapCalcData encapsulates all needed data for shrink-wrap functions.
 * (So that you don't have to pass an enormous amount of arguments to functions)
 */

struct BVHTree;
struct MDeformVert;
struct Mesh;
struct ModifierEvalContext;
struct Object;
struct ShrinkwrapGpencilModifierData;
struct ShrinkwrapModifierData;
struct SpaceTransform;

/* Information about boundary edges in the mesh. */
typedef struct ShrinkwrapBoundaryVertData {
  /* Average direction of edges that meet here. */
  float direction[3];

  /* Closest vector to direction that is orthogonal to vertex normal. */
  float normal_plane[3];
} ShrinkwrapBoundaryVertData;

typedef struct ShrinkwrapBoundaryData {
  /* True if the edge belongs to exactly one face. */
  const BLI_bitmap *edge_is_boundary;
  /* True if the looptri has any boundary edges. */
  const BLI_bitmap *looptri_has_boundary;

  /* Mapping from vertex index to boundary vertex index, or -1.
   * Used for compact storage of data about boundary vertices. */
  const int *vert_boundary_id;
  unsigned int num_boundary_verts;

  /* Direction data about boundary vertices. */
  const ShrinkwrapBoundaryVertData *boundary_verts;
} ShrinkwrapBoundaryData;

/**
 * Free boundary data for target project.
 */
void BKE_shrinkwrap_boundary_data_free(ShrinkwrapBoundaryData *data);
void BKE_shrinkwrap_compute_boundary_data(struct Mesh *mesh);

/* Information about a mesh and BVH tree. */
typedef struct ShrinkwrapTreeData {
  Mesh *mesh;

  BVHTree *bvh;
  BVHTreeFromMesh treeData;

  const int *face_offsets;
  const float (*vert_normals)[3];
  const int *corner_edges;
  const float (*face_normals)[3];
  const bool *sharp_faces;
  const float (*clnors)[3];
  ShrinkwrapBoundaryData *boundary;
} ShrinkwrapTreeData;

/**
 * Checks if the modifier needs target normals with these settings.
 */
bool BKE_shrinkwrap_needs_normals(int shrinkType, int shrinkMode);

/**
 * Initializes the mesh data structure from the given mesh and settings.
 */
bool BKE_shrinkwrap_init_tree(struct ShrinkwrapTreeData *data,
                              Mesh *mesh,
                              int shrinkType,
                              int shrinkMode,
                              bool force_normals);

/**
 * Frees the tree data if necessary.
 */
void BKE_shrinkwrap_free_tree(struct ShrinkwrapTreeData *data);

/**
 * Main shrink-wrap function (implementation of the shrink-wrap modifier).
 */
void shrinkwrapModifier_deform(struct ShrinkwrapModifierData *smd,
                               const struct ModifierEvalContext *ctx,
                               struct Scene *scene,
                               struct Object *ob,
                               struct Mesh *mesh,
                               const struct MDeformVert *dvert,
                               int defgrp_index,
                               float (*vertexCos)[3],
                               int numVerts);
/* Implementation of the Shrinkwrap Grease Pencil modifier. */
void shrinkwrapGpencilModifier_deform(struct ShrinkwrapGpencilModifierData *mmd,
                                      struct Object *ob,
                                      struct MDeformVert *dvert,
                                      int defgrp_index,
                                      float (*vertexCos)[3],
                                      int numVerts);

/**
 * Used in `editmesh_mask_extract.cc` to shrink-wrap the extracted mesh to the sculpt.
 */
void BKE_shrinkwrap_mesh_nearest_surface_deform(struct bContext *C,
                                                struct Object *ob_source,
                                                struct Object *ob_target);

/**
 * Used in `object_remesh.cc` to preserve the details and volume in the voxel remesher.
 */
void BKE_shrinkwrap_remesh_target_project(struct Mesh *src_me,
                                          struct Mesh *target_me,
                                          struct Object *ob_target);

/**
 * This function ray-cast a single vertex and updates the hit if the "hit" is considered valid.
 *
 * \param options: Opts control whether an hit is valid or not.
 * Supported options are:
 * - #MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE (front faces hits are ignored)
 * - #MOD_SHRINKWRAP_CULL_TARGET_BACKFACE (back faces hits are ignored)
 *
 * \param transf: Take into consideration the space_transform, that is:
 * if `transf` was configured with `SPACE_TRANSFORM_SETUP( &transf,  ob1, ob2)`
 * then the input (vert, dir, #BVHTreeRayHit) must be defined in ob1 coordinates space
 * and the #BVHTree must be built in ob2 coordinate space.
 * Thus it provides an easy way to cast the same ray across several trees
 * (where each tree was built on its own coords space).
 *
 * \return true if "hit" was updated.
 */
bool BKE_shrinkwrap_project_normal(char options,
                                   const float vert[3],
                                   const float dir[3],
                                   float ray_radius,
                                   const struct SpaceTransform *transf,
                                   struct ShrinkwrapTreeData *tree,
                                   BVHTreeRayHit *hit);

/**
 * Maps the point to the nearest surface, either by simple nearest, or by target normal projection.
 */
void BKE_shrinkwrap_find_nearest_surface(struct ShrinkwrapTreeData *tree,
                                         struct BVHTreeNearest *nearest,
                                         float co[3],
                                         int type);

/**
 * Compute a smooth normal of the target (if applicable) at the hit location.
 *
 * \param tree: information about the mesh.
 * \param transform: transform from the hit coordinate space to the object space; may be null.
 * \param r_no: output in hit coordinate space; may be shared with inputs.
 */
void BKE_shrinkwrap_compute_smooth_normal(const struct ShrinkwrapTreeData *tree,
                                          const struct SpaceTransform *transform,
                                          int looptri_idx,
                                          const float hit_co[3],
                                          const float hit_no[3],
                                          float r_no[3]);

/**
 * Apply the shrink to surface modes to the given original coordinates and nearest point.
 *
 * \param tree: mesh data for smooth normals.
 * \param transform: transform from the hit coordinate space to the object space; may be null.
 * \param r_point_co: may be the same memory location as `point_co`, `hit_co`, or `hit_no`.
 */
void BKE_shrinkwrap_snap_point_to_surface(const struct ShrinkwrapTreeData *tree,
                                          const struct SpaceTransform *transform,
                                          int mode,
                                          int hit_idx,
                                          const float hit_co[3],
                                          const float hit_no[3],
                                          float goal_dist,
                                          const float point_co[3],
                                          float r_point_co[3]);

/*
 * NULL initializes to local data
 */
#define NULL_ShrinkwrapCalcData \
  { \
    NULL, \
  }
#define NULL_BVHTreeFromMesh \
  { \
    NULL, \
  }
#define NULL_BVHTreeRayHit \
  { \
    NULL, \
  }
#define NULL_BVHTreeNearest \
  { \
    0, \
  }

#ifdef __cplusplus
}
#endif
