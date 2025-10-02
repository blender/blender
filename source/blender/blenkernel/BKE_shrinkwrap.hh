/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

/* Shrinkwrap stuff */
#include "BKE_bvhutils.hh"

#include "BKE_context.hh"
#include "BLI_array.hh"
#include "BLI_bit_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_virtual_array.hh"

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

struct Depsgraph;
struct BVHTree;
struct MDeformVert;
struct Mesh;
struct ModifierEvalContext;
struct Object;
struct ShrinkwrapGpencilModifierData;
struct ShrinkwrapModifierData;
struct SpaceTransform;

/* Information about boundary edges in the mesh. */
struct ShrinkwrapBoundaryVertData {
  /* Average direction of edges that meet here. */
  float direction[3];

  /* Closest vector to direction that is orthogonal to vertex normal. */
  float normal_plane[3];
};

class ShrinkwrapBoundaryData {
 public:
  /* Returns true if there is boundary information. If there is no boundary information, then the
   * mesh from which this data is created from has no boundaries. */
  bool has_boundary() const
  {
    return !edge_is_boundary.is_empty();
  }

  /* True if the edge belongs to exactly one face. */
  blender::BitVector<> edge_is_boundary;
  /* True if the triangle has any boundary edges. */
  blender::BitVector<> tri_has_boundary;

  /* Mapping from vertex index to boundary vertex index, or -1.
   * Used for compact storage of data about boundary vertices. */
  blender::Array<int> vert_boundary_id;

  /* Direction data about boundary vertices. */
  blender::Array<ShrinkwrapBoundaryVertData> boundary_verts;
};

namespace blender::bke::shrinkwrap {

const ShrinkwrapBoundaryData &boundary_cache_ensure(const Mesh &mesh);

}  // namespace blender::bke::shrinkwrap

/* Information about a mesh and BVH tree. */
struct ShrinkwrapTreeData {
  Mesh *mesh;

  const BVHTree *bvh;
  blender::bke::BVHTreeFromMesh treeData;

  blender::OffsetIndices<int> faces;
  blender::Span<blender::int2> edges;
  blender::Span<int> corner_edges;

  blender::Span<blender::float3> face_normals;
  blender::Span<blender::float3> vert_normals;
  blender::Span<blender::float3> corner_normals;
  blender::VArraySpan<bool> sharp_faces;
  const ShrinkwrapBoundaryData *boundary;
};

/**
 * Checks if the modifier needs target normals with these settings.
 */
bool BKE_shrinkwrap_needs_normals(int shrinkType, int shrinkMode);

/**
 * Initializes the mesh data structure from the given mesh and settings.
 */
bool BKE_shrinkwrap_init_tree(
    ShrinkwrapTreeData *data, Mesh *mesh, int shrinkType, int shrinkMode, bool force_normals);

/**
 * Frees the tree data if necessary.
 */
void BKE_shrinkwrap_free_tree(ShrinkwrapTreeData *data);

/**
 * Main shrink-wrap function (implementation of the shrink-wrap modifier).
 */
void shrinkwrapModifier_deform(ShrinkwrapModifierData *smd,
                               const ModifierEvalContext *ctx,
                               Scene *scene,
                               Object *ob,
                               Mesh *mesh,
                               const MDeformVert *dvert,
                               int defgrp_index,
                               float (*vertexCos)[3],
                               int numVerts);

struct ShrinkwrapParams {
  /** Shrink target. */
  Object *target = nullptr;
  /** Additional shrink target. */
  Object *aux_target = nullptr;
  /* Use inverse vertex group weights. */
  bool invert_vertex_weights = false;
  /** Distance offset to keep from mesh/projection point. */
  float keep_distance = 0.05f;
  /** Shrink type projection. */
  short shrink_type = 0 /*MOD_SHRINKWRAP_NEAREST_SURFACE*/;
  /** Shrink options. */
  char shrink_options = 0 /*MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR*/;
  /** Shrink to surface mode. */
  char shrink_mode = 0 /*MOD_SHRINKWRAP_ON_SURFACE*/;
  /** Limit the projection ray cast. */
  float projection_limit = 0.0f;
  /** Axis to project over. */
  char projection_axis = 0 /*MOD_SHRINKWRAP_PROJECT_OVER_NORMAL*/;
  /**
   * If using projection over vertex normal this controls the level of subsurface that must be
   * done before getting the vertex coordinates and normal.
   */
  char subsurf_levels = 0;
};

void shrinkwrapParams_deform(const ShrinkwrapParams &params,
                             Object &object,
                             ShrinkwrapTreeData &tree,
                             blender::Span<MDeformVert> dvert,
                             int defgrp_index,
                             blender::MutableSpan<blender::float3> positions);

/**
 * Used in `editmesh_mask_extract.cc` to shrink-wrap the extracted mesh to the sculpt.
 */
void BKE_shrinkwrap_mesh_nearest_surface_deform(Depsgraph *depsgraph,
                                                Scene *scene,
                                                Object *ob_source,
                                                Object *ob_target);

/**
 * Used in `object_remesh.cc` to preserve the details and volume in the voxel remesher.
 */
void BKE_shrinkwrap_remesh_target_project(Mesh *src_me, Mesh *target_me, Object *ob_target);

/**
 * This function ray-cast a single vertex and updates the hit if the "hit" is considered valid.
 *
 * \param options: Opts control whether an hit is valid or not.
 * Supported options are:
 * - #MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE (front faces hits are ignored)
 * - #MOD_SHRINKWRAP_CULL_TARGET_BACKFACE (back faces hits are ignored)
 *
 * \param transf: Take into consideration the space_transform, that is:
 * if `transf` was configured with `SPACE_TRANSFORM_SETUP(&transf, ob1, ob2)`
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
                                   const SpaceTransform *transf,
                                   ShrinkwrapTreeData *tree,
                                   BVHTreeRayHit *hit);

/**
 * Maps the point to the nearest surface, either by simple nearest, or by target normal projection.
 */
void BKE_shrinkwrap_find_nearest_surface(ShrinkwrapTreeData *tree,
                                         BVHTreeNearest *nearest,
                                         float co[3],
                                         int type);

/**
 * Compute a smooth normal of the target (if applicable) at the hit location.
 *
 * \param tree: information about the mesh.
 * \param transform: transform from the hit coordinate space to the object space; may be null.
 * \param r_no: output in hit coordinate space; may be shared with inputs.
 */
void BKE_shrinkwrap_compute_smooth_normal(const ShrinkwrapTreeData *tree,
                                          const SpaceTransform *transform,
                                          int tri_idx,
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
void BKE_shrinkwrap_snap_point_to_surface(const ShrinkwrapTreeData *tree,
                                          const SpaceTransform *transform,
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
