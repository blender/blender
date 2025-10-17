/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_task.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_bvhutils.hh"
#include "BKE_deform.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph.hh"

#include "MEM_guardedalloc.h"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

struct SDefAdjacency {
  SDefAdjacency *next;
  uint index;
};

struct SDefAdjacencyArray {
  SDefAdjacency *first;
  uint num; /* Careful, this is twice the number of faces (avoids an extra loop) */
};

/**
 * Polygons per edge (only 2, any more will exit calculation).
 */
struct SDefEdgePolys {
  uint polys[2], num;
};

struct SDefBindCalcData {
  blender::bke::BVHTreeFromMesh *treeData;
  const SDefAdjacencyArray *vert_edges;
  const SDefEdgePolys *edge_polys;
  SDefVert *bind_verts;
  blender::Span<blender::int2> edges;
  blender::OffsetIndices<int> polys;
  blender::Span<int> corner_verts;
  blender::Span<int> corner_edges;
  blender::Span<blender::int3> corner_tris;
  blender::Span<int> tri_faces;

  /** Coordinates to bind to, transformed into local space (compatible with `vertexCos`). */
  float (*targetCos)[3];
  /** Coordinates to bind (reference to the modifiers input argument). */
  const float (*vertexCos)[3];
  float imat[4][4];
  float falloff;
  int success;
  /** Vertex group lookup data. */
  const MDeformVert *dvert;
  int defgrp_index;
  bool invert_vgroup;
  bool sparse_bind;
};

/**
 * This represents the relationship between a point (a source coordinate)
 * and the face-corner it's being bound to (from the target mesh).
 *
 * \note Some of these values could be de-duplicated however these are only
 * needed once when running bind, so optimizing this structure isn't a priority.
 */
struct SDefBindPoly {
  /** Coordinates copied directly from the modifiers input. */
  float (*coords)[3];
  /** Coordinates projected into 2D space using `normal`. */
  float (*coords_v2)[2];
  /** The point being queried projected into 2D space using `normal`. */
  float point_v2[2];
  float weight_angular;
  float weight_dist_proj;
  float weight_dist;
  float weight;
  /**
   * Distances from the centroid to edges flanking the corner vertex, used to penalize
   * small or long and narrow faces in favor of bigger and more square ones.
   */
  float scales[2];
  /**
   * Distance weight from the corner vertex to the chord line, used to penalize
   * cases with the three consecutive vertices being nearly in line.
   */
  float scale_mid;
  /** Center of `coords` */
  float centroid[3];
  /** Center of `coords_v2` */
  float centroid_v2[2];
  /**
   * The calculated normal of coords (could be shared between faces).
   */
  float normal[3];
  /**
   * Vectors pointing from the centroid to the midpoints of the two edges
   * flanking the corner vertex.
   */
  float cent_edgemid_vecs_v2[2][2];
  /** Angle between the cent_edgemid_vecs_v2 vectors. */
  float edgemid_angle;
  /**
   * Angles between the centroid-to-point and cent_edgemid_vecs_v2 vectors.
   * Positive values measured towards the corner; clamped non-negative.
   */
  float point_edgemid_angles[2];
  /** Angles between the centroid-to-corner and cent_edgemid_vecs_v2 vectors. */
  float corner_edgemid_angles[2];
  /**
   * Weight of the bind mode based on the corner and two adjacent vertices,
   * versus the one based on the centroid and the dominant edge.
   */
  float dominant_angle_weight;
  /** Index of the input face. */
  uint index;
  /** Number of vertices in this face. */
  uint verts_num;
  /**
   * This polygons loop-start.
   * \note that we could look this up from the face.
   */
  uint loopstart;
  uint edge_inds[2];
  uint edge_vert_inds[2];
  /** The index of this corner in the face (starting at zero). */
  uint corner_ind;
  uint dominant_edge;
  /** When true `point_v2` is inside `coords_v2`. */
  bool inside;
};

struct SDefBindWeightData {
  SDefBindPoly *bind_polys;
  uint faces_num;
  uint binds_num;
};

struct SDefDeformData {
  const SDefVert *bind_verts;
  float (*targetCos)[3];
  float (*vertexCos)[3];
  const MDeformVert *dvert;
  int defgrp_index;
  bool invert_vgroup;
  float strength;
};

/* Bind result values */
enum {
  MOD_SDEF_BIND_RESULT_SUCCESS = 1,
  MOD_SDEF_BIND_RESULT_GENERIC_ERR = 0,
  MOD_SDEF_BIND_RESULT_MEM_ERR = -1,
  MOD_SDEF_BIND_RESULT_NONMANY_ERR = -2,
  MOD_SDEF_BIND_RESULT_CONCAVE_ERR = -3,
  MOD_SDEF_BIND_RESULT_OVERLAP_ERR = -4,
};

/* Infinite weight flags */
enum {
  MOD_SDEF_INFINITE_WEIGHT_ANGULAR = (1 << 0),
  MOD_SDEF_INFINITE_WEIGHT_DIST_PROJ = (1 << 1),
  MOD_SDEF_INFINITE_WEIGHT_DIST = (1 << 2),
};

static void init_data(ModifierData *md)
{
  SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(SurfaceDeformModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;

  /* Ask for vertex groups if we need them. */
  if (smd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

class BindVertsImplicitSharing : public blender::ImplicitSharingInfo {
 public:
  SDefVert *verts;
  int bind_verts_num;

  BindVertsImplicitSharing(SDefVert *data, int bind_verts_num)
      : verts(data), bind_verts_num(bind_verts_num)
  {
  }

 private:
  void delete_self_with_data() override
  {
    for (int i = 0; i < this->bind_verts_num; i++) {
      if (this->verts[i].binds) {
        for (int j = 0; j < this->verts[i].binds_num; j++) {
          MEM_SAFE_FREE(this->verts[i].binds[j].vert_inds);
          MEM_SAFE_FREE(this->verts[i].binds[j].vert_weights);
        }
        MEM_freeN(this->verts[i].binds);
      }
    }
    MEM_freeN(verts);
    MEM_delete(this);
  }
};

static void free_data(ModifierData *md)
{
  SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;
  blender::implicit_sharing::free_shared_data(&smd->verts, &smd->verts_sharing_info);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const SurfaceDeformModifierData *smd = (const SurfaceDeformModifierData *)md;
  SurfaceDeformModifierData *tsmd = (SurfaceDeformModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  blender::implicit_sharing::copy_shared_pointer(
      smd->verts, smd->verts_sharing_info, &tsmd->verts, &tsmd->verts_sharing_info);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;

  walk(user_data, ob, (ID **)&smd->target, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;
  if (smd->target != nullptr) {
    DEG_add_object_relation(
        ctx->node, smd->target, DEG_OB_COMP_GEOMETRY, "Surface Deform Modifier");
  }
}

static void freeAdjacencyMap(SDefAdjacencyArray *const vert_edges,
                             SDefAdjacency *const adj_ref,
                             SDefEdgePolys *const edge_polys)
{
  MEM_freeN(edge_polys);

  MEM_freeN(adj_ref);

  MEM_freeN(vert_edges);
}

static int buildAdjacencyMap(const blender::OffsetIndices<int> polys,
                             const blender::Span<blender::int2> edges,
                             const blender::Span<int> corner_edges,
                             SDefAdjacencyArray *const vert_edges,
                             SDefAdjacency *adj,
                             SDefEdgePolys *const edge_polys)
{
  /* Find polygons adjacent to edges. */
  for (const int i : polys.index_range()) {
    for (const int edge_i : corner_edges.slice(polys[i])) {
      if (edge_polys[edge_i].num == 0) {
        edge_polys[edge_i].polys[0] = i;
        edge_polys[edge_i].polys[1] = -1;
        edge_polys[edge_i].num++;
      }
      else if (edge_polys[edge_i].num == 1) {
        edge_polys[edge_i].polys[1] = i;
        edge_polys[edge_i].num++;
      }
      else {
        return MOD_SDEF_BIND_RESULT_NONMANY_ERR;
      }
    }
  }

  /* Find edges adjacent to vertices */
  for (const int i : edges.index_range()) {
    const blender::int2 &edge = edges[i];
    adj->next = vert_edges[edge[0]].first;
    adj->index = i;
    vert_edges[edge[0]].first = adj;
    vert_edges[edge[0]].num += edge_polys[i].num;
    adj++;

    adj->next = vert_edges[edge[1]].first;
    adj->index = i;
    vert_edges[edge[1]].first = adj;
    vert_edges[edge[1]].num += edge_polys[i].num;
    adj++;
  }

  return MOD_SDEF_BIND_RESULT_SUCCESS;
}

BLI_INLINE void sortPolyVertsEdge(uint *indices,
                                  const int *const corner_verts,
                                  const int *const corner_edges,
                                  const uint edge,
                                  const uint num)
{
  bool found = false;

  for (int i = 0; i < num; i++) {
    if (corner_edges[i] == edge) {
      found = true;
    }
    if (found) {
      *indices = corner_verts[i];
      indices++;
    }
  }

  /* Fill in remaining vertex indices that occur before the edge */
  for (int i = 0; corner_edges[i] != edge; i++) {
    *indices = corner_verts[i];
    indices++;
  }
}

BLI_INLINE void sortPolyVertsTri(uint *indices,
                                 const int *const corner_verts,
                                 const uint loopstart,
                                 const uint num)
{
  for (int i = loopstart; i < num; i++) {
    *indices = corner_verts[i];
    indices++;
  }

  for (int i = 0; i < loopstart; i++) {
    *indices = corner_verts[i];
    indices++;
  }
}

BLI_INLINE uint nearestVert(SDefBindCalcData *const data, const float point_co[3])
{
  BVHTreeNearest nearest{};
  nearest.dist_sq = FLT_MAX;
  nearest.index = -1;

  float t_point[3];
  float max_dist = FLT_MAX;
  float dist;
  uint index = 0;

  mul_v3_m4v3(t_point, data->imat, point_co);

  BLI_bvhtree_find_nearest(
      data->treeData->tree, t_point, &nearest, data->treeData->nearest_callback, data->treeData);

  const blender::IndexRange face = data->polys[data->tri_faces[nearest.index]];

  for (int i = 0; i < face.size(); i++) {
    const int edge_i = data->corner_edges[face.start() + i];
    const blender::int2 &edge = data->edges[edge_i];
    dist = dist_squared_to_line_segment_v3(
        point_co, data->targetCos[edge[0]], data->targetCos[edge[1]]);

    if (dist < max_dist) {
      max_dist = dist;
      index = edge_i;
    }
  }

  const blender::int2 &edge = data->edges[index];
  if (len_squared_v3v3(point_co, data->targetCos[edge[0]]) <
      len_squared_v3v3(point_co, data->targetCos[edge[1]]))
  {
    return edge[0];
  }

  return edge[1];
}

BLI_INLINE int isPolyValid(const float coords[][2], const uint nr)
{
  float prev_co[2], prev_prev_co[2];
  float curr_vec[2], prev_vec[2];

  if (!is_poly_convex_v2(coords, nr)) {
    return MOD_SDEF_BIND_RESULT_CONCAVE_ERR;
  }

  copy_v2_v2(prev_prev_co, coords[nr - 2]);
  copy_v2_v2(prev_co, coords[nr - 1]);
  sub_v2_v2v2(prev_vec, prev_co, coords[nr - 2]);
  normalize_v2(prev_vec);

  for (int i = 0; i < nr; i++) {
    sub_v2_v2v2(curr_vec, coords[i], prev_co);

    /* Check overlap between directly adjacent vertices. */
    const float curr_len = normalize_v2(curr_vec);
    if (curr_len < FLT_EPSILON) {
      return MOD_SDEF_BIND_RESULT_OVERLAP_ERR;
    }

    /* Check overlap between vertices skipping one. */
    if (len_squared_v2v2(prev_prev_co, coords[i]) < FLT_EPSILON * FLT_EPSILON) {
      return MOD_SDEF_BIND_RESULT_OVERLAP_ERR;
    }

    /* Check for adjacent parallel edges. */
    if (1.0f - dot_v2v2(prev_vec, curr_vec) < FLT_EPSILON) {
      return MOD_SDEF_BIND_RESULT_CONCAVE_ERR;
    }

    copy_v2_v2(prev_prev_co, prev_co);
    copy_v2_v2(prev_co, coords[i]);
    copy_v2_v2(prev_vec, curr_vec);
  }

  return MOD_SDEF_BIND_RESULT_SUCCESS;
}

static void freeBindData(SDefBindWeightData *const bwdata)
{
  SDefBindPoly *bpoly = bwdata->bind_polys;

  if (bwdata->bind_polys) {
    for (int i = 0; i < bwdata->faces_num; bpoly++, i++) {
      MEM_SAFE_FREE(bpoly->coords);
      MEM_SAFE_FREE(bpoly->coords_v2);
    }

    MEM_freeN(bwdata->bind_polys);
  }

  MEM_freeN(bwdata);
}

BLI_INLINE float computeAngularWeight(const float point_angle, const float edgemid_angle)
{
  return sinf(min_ff(point_angle / edgemid_angle, 1) * M_PI_2);
}

BLI_INLINE SDefBindWeightData *computeBindWeights(SDefBindCalcData *const data,
                                                  const float point_co[3])
{
  const uint nearest = nearestVert(data, point_co);
  const SDefAdjacency *const vert_edges = data->vert_edges[nearest].first;
  const SDefEdgePolys *const edge_polys = data->edge_polys;

  const SDefAdjacency *vedge;

  SDefBindWeightData *bwdata;
  SDefBindPoly *bpoly;

  const float world[3] = {0.0f, 0.0f, 1.0f};
  float avg_point_dist = 0.0f;
  float tot_weight = 0.0f;
  int inf_weight_flags = 0;

  bwdata = MEM_callocN<SDefBindWeightData>("SDefBindWeightData");
  if (bwdata == nullptr) {
    data->success = MOD_SDEF_BIND_RESULT_MEM_ERR;
    return nullptr;
  }

  bwdata->faces_num = data->vert_edges[nearest].num / 2;

  bpoly = MEM_calloc_arrayN<SDefBindPoly>(bwdata->faces_num, "SDefBindPoly");
  if (bpoly == nullptr) {
    freeBindData(bwdata);
    data->success = MOD_SDEF_BIND_RESULT_MEM_ERR;
    return nullptr;
  }

  bwdata->bind_polys = bpoly;

  /* Loop over all adjacent edges,
   * and build the #SDefBindPoly data for each face adjacent to those. */
  for (vedge = vert_edges; vedge; vedge = vedge->next) {
    uint edge_ind = vedge->index;

    for (int i = 0; i < edge_polys[edge_ind].num; i++) {
      {
        bpoly = bwdata->bind_polys;

        for (int j = 0; j < bwdata->faces_num; bpoly++, j++) {
          /* If coords isn't allocated, we have reached the first uninitialized `bpoly`. */
          if ((bpoly->index == edge_polys[edge_ind].polys[i]) || (!bpoly->coords)) {
            break;
          }
        }
      }

      /* Check if face was already created by another edge or still has to be initialized */
      if (!bpoly->coords) {
        float angle;
        float axis[3];
        float tmp_vec_v2[2];
        int is_poly_valid;

        bpoly->index = edge_polys[edge_ind].polys[i];
        bpoly->coords = nullptr;
        bpoly->coords_v2 = nullptr;

        /* Copy face data */
        const blender::IndexRange face = data->polys[bpoly->index];

        bpoly->verts_num = face.size();
        bpoly->loopstart = face.start();

        bpoly->coords = MEM_malloc_arrayN<float[3]>(size_t(face.size()), "SDefBindPolyCoords");
        if (bpoly->coords == nullptr) {
          freeBindData(bwdata);
          data->success = MOD_SDEF_BIND_RESULT_MEM_ERR;
          return nullptr;
        }

        bpoly->coords_v2 = MEM_malloc_arrayN<float[2]>(size_t(face.size()),
                                                       "SDefBindPolyCoords_v2");
        if (bpoly->coords_v2 == nullptr) {
          freeBindData(bwdata);
          data->success = MOD_SDEF_BIND_RESULT_MEM_ERR;
          return nullptr;
        }

        for (int j = 0; j < face.size(); j++) {
          const int vert_i = data->corner_verts[face.start() + j];
          const int edge_i = data->corner_edges[face.start() + j];
          copy_v3_v3(bpoly->coords[j], data->targetCos[vert_i]);

          /* Find corner and edge indices within face loop array */
          if (vert_i == nearest) {
            bpoly->corner_ind = j;
            bpoly->edge_vert_inds[0] = (j == 0) ? (face.size() - 1) : (j - 1);
            bpoly->edge_vert_inds[1] = (j == face.size() - 1) ? (0) : (j + 1);

            bpoly->edge_inds[0] = data->corner_edges[face.start() + bpoly->edge_vert_inds[0]];
            bpoly->edge_inds[1] = edge_i;
          }
        }

        /* Compute polygons parametric data. */
        mid_v3_v3_array(bpoly->centroid, bpoly->coords, face.size());
        normal_poly_v3(bpoly->normal, bpoly->coords, face.size());

        /* Compute face skew angle and axis */
        angle = angle_normalized_v3v3(bpoly->normal, world);

        cross_v3_v3v3(axis, bpoly->normal, world);
        normalize_v3(axis);

        /* Map coords onto 2d normal plane. */
        map_to_plane_axis_angle_v2_v3v3fl(bpoly->point_v2, point_co, axis, angle);

        zero_v2(bpoly->centroid_v2);
        for (int j = 0; j < face.size(); j++) {
          map_to_plane_axis_angle_v2_v3v3fl(bpoly->coords_v2[j], bpoly->coords[j], axis, angle);
          madd_v2_v2fl(bpoly->centroid_v2, bpoly->coords_v2[j], 1.0f / face.size());
        }

        is_poly_valid = isPolyValid(bpoly->coords_v2, face.size());

        if (is_poly_valid != MOD_SDEF_BIND_RESULT_SUCCESS) {
          freeBindData(bwdata);
          data->success = is_poly_valid;
          return nullptr;
        }

        bpoly->inside = isect_point_poly_v2(bpoly->point_v2, bpoly->coords_v2, face.size());

        /* Initialize weight components */
        bpoly->weight_angular = 1.0f;
        bpoly->weight_dist_proj = len_v2v2(bpoly->centroid_v2, bpoly->point_v2);
        bpoly->weight_dist = len_v3v3(bpoly->centroid, point_co);

        avg_point_dist += bpoly->weight_dist;

        /* Common vertex coordinates. */
        const float *const vert0_v2 = bpoly->coords_v2[bpoly->edge_vert_inds[0]];
        const float *const vert1_v2 = bpoly->coords_v2[bpoly->edge_vert_inds[1]];
        const float *const corner_v2 = bpoly->coords_v2[bpoly->corner_ind];

        /* Compute centroid to mid-edge vectors */
        mid_v2_v2v2(bpoly->cent_edgemid_vecs_v2[0], vert0_v2, corner_v2);
        mid_v2_v2v2(bpoly->cent_edgemid_vecs_v2[1], vert1_v2, corner_v2);

        sub_v2_v2(bpoly->cent_edgemid_vecs_v2[0], bpoly->centroid_v2);
        sub_v2_v2(bpoly->cent_edgemid_vecs_v2[1], bpoly->centroid_v2);

        normalize_v2(bpoly->cent_edgemid_vecs_v2[0]);
        normalize_v2(bpoly->cent_edgemid_vecs_v2[1]);

        /* Compute face scales with respect to the two edges. */
        bpoly->scales[0] = dist_to_line_v2(bpoly->centroid_v2, vert0_v2, corner_v2);
        bpoly->scales[1] = dist_to_line_v2(bpoly->centroid_v2, vert1_v2, corner_v2);

        /* Compute the angle between the edge mid vectors. */
        bpoly->edgemid_angle = angle_normalized_v2v2(bpoly->cent_edgemid_vecs_v2[0],
                                                     bpoly->cent_edgemid_vecs_v2[1]);

        /* Compute the angles between the corner and the edge mid vectors. The angles
         * are computed signed in order to correctly clamp point_edgemid_angles later. */
        float corner_angles[2];

        sub_v2_v2v2(tmp_vec_v2, corner_v2, bpoly->centroid_v2);
        normalize_v2(tmp_vec_v2);

        corner_angles[0] = angle_signed_v2v2(tmp_vec_v2, bpoly->cent_edgemid_vecs_v2[0]);
        corner_angles[1] = angle_signed_v2v2(tmp_vec_v2, bpoly->cent_edgemid_vecs_v2[1]);

        bpoly->corner_edgemid_angles[0] = fabsf(corner_angles[0]);
        bpoly->corner_edgemid_angles[1] = fabsf(corner_angles[1]);

        /* Verify that the computed values are valid (the face isn't somehow
         * degenerate despite having passed isPolyValid). */
        if (bpoly->scales[0] < FLT_EPSILON || bpoly->scales[1] < FLT_EPSILON ||
            bpoly->edgemid_angle < FLT_EPSILON || bpoly->corner_edgemid_angles[0] < FLT_EPSILON ||
            bpoly->corner_edgemid_angles[1] < FLT_EPSILON)
        {
          freeBindData(bwdata);
          data->success = MOD_SDEF_BIND_RESULT_GENERIC_ERR;
          return nullptr;
        }

        /* Check for infinite weights, and compute angular data otherwise. */
        if (bpoly->weight_dist < FLT_EPSILON) {
          inf_weight_flags |= MOD_SDEF_INFINITE_WEIGHT_DIST_PROJ;
          inf_weight_flags |= MOD_SDEF_INFINITE_WEIGHT_DIST;
        }
        else if (bpoly->weight_dist_proj < FLT_EPSILON) {
          inf_weight_flags |= MOD_SDEF_INFINITE_WEIGHT_DIST_PROJ;
        }
        else {
          /* Compute angles between the point and the edge mid vectors. */
          float cent_point_vec[2], point_angles[2];

          sub_v2_v2v2(cent_point_vec, bpoly->point_v2, bpoly->centroid_v2);
          normalize_v2(cent_point_vec);

          point_angles[0] = angle_signed_v2v2(cent_point_vec, bpoly->cent_edgemid_vecs_v2[0]) *
                            signf(corner_angles[0]);
          point_angles[1] = angle_signed_v2v2(cent_point_vec, bpoly->cent_edgemid_vecs_v2[1]) *
                            signf(corner_angles[1]);

          if (point_angles[0] <= 0 && point_angles[1] <= 0) {
            /* If the point is outside the corner formed by the edge mid vectors,
             * choose to clamp the closest side and flip the other. */
            if (point_angles[0] < point_angles[1]) {
              point_angles[0] = bpoly->edgemid_angle - point_angles[1];
            }
            else {
              point_angles[1] = bpoly->edgemid_angle - point_angles[0];
            }
          }

          bpoly->point_edgemid_angles[0] = max_ff(0, point_angles[0]);
          bpoly->point_edgemid_angles[1] = max_ff(0, point_angles[1]);

          /* Compute the distance scale for the corner. The base value is the orthogonal
           * distance from the corner to the chord, scaled by `sqrt(2)` to preserve the old
           * values in case of a square grid. This doesn't use the centroid because the
           * corner_triS method only uses these three vertices. */
          bpoly->scale_mid = area_tri_v2(vert0_v2, corner_v2, vert1_v2) /
                             len_v2v2(vert0_v2, vert1_v2) * sqrtf(2);

          if (bpoly->inside) {
            /* When inside, interpolate to centroid-based scale close to the center. */
            float min_dist = min_ff(bpoly->scales[0], bpoly->scales[1]);

            bpoly->scale_mid = interpf(bpoly->scale_mid,
                                       (bpoly->scales[0] + bpoly->scales[1]) / 2,
                                       min_ff(bpoly->weight_dist_proj / min_dist, 1));
          }

          /* Verify that the additional computed values are valid. */
          if (bpoly->scale_mid < FLT_EPSILON ||
              bpoly->point_edgemid_angles[0] + bpoly->point_edgemid_angles[1] < FLT_EPSILON)
          {
            freeBindData(bwdata);
            data->success = MOD_SDEF_BIND_RESULT_GENERIC_ERR;
            return nullptr;
          }
        }
      }
    }
  }

  avg_point_dist /= bwdata->faces_num;

  /* If weights 1 and 2 are not infinite, loop over all adjacent edges again,
   * and build adjacency dependent angle data (depends on all polygons having been computed) */
  if (!inf_weight_flags) {
    for (vedge = vert_edges; vedge; vedge = vedge->next) {
      SDefBindPoly *bpolys[2];
      const SDefEdgePolys *epolys;
      float ang_weights[2];
      uint edge_ind = vedge->index;
      uint edge_on_poly[2];

      epolys = &edge_polys[edge_ind];

      /* Find bind polys corresponding to the edge's adjacent polys */
      bpoly = bwdata->bind_polys;

      for (int i = 0, j = 0; (i < bwdata->faces_num) && (j < epolys->num); bpoly++, i++) {
        if (ELEM(bpoly->index, epolys->polys[0], epolys->polys[1])) {
          bpolys[j] = bpoly;

          if (bpoly->edge_inds[0] == edge_ind) {
            edge_on_poly[j] = 0;
          }
          else {
            edge_on_poly[j] = 1;
          }

          j++;
        }
      }

      /* Compute angular weight component */
      if (epolys->num == 1) {
        ang_weights[0] = computeAngularWeight(bpolys[0]->point_edgemid_angles[edge_on_poly[0]],
                                              bpolys[0]->edgemid_angle);
        bpolys[0]->weight_angular *= ang_weights[0] * ang_weights[0];
      }
      else if (epolys->num == 2) {
        ang_weights[0] = computeAngularWeight(bpolys[0]->point_edgemid_angles[edge_on_poly[0]],
                                              bpolys[0]->edgemid_angle);
        ang_weights[1] = computeAngularWeight(bpolys[1]->point_edgemid_angles[edge_on_poly[1]],
                                              bpolys[1]->edgemid_angle);

        bpolys[0]->weight_angular *= ang_weights[0] * ang_weights[1];
        bpolys[1]->weight_angular *= ang_weights[0] * ang_weights[1];
      }
    }
  }

  /* Compute scaling and falloff:
   * - Scale all weights if no infinite weight is found.
   * - Scale only un-projected weight if projected weight is infinite.
   * - Scale none if both are infinite. */
  if (!inf_weight_flags) {
    bpoly = bwdata->bind_polys;

    for (int i = 0; i < bwdata->faces_num; bpoly++, i++) {
      float corner_angle_weights[2];
      float scale_weight, sqr, inv_sqr;

      corner_angle_weights[0] = bpoly->point_edgemid_angles[0] / bpoly->corner_edgemid_angles[0];
      corner_angle_weights[1] = bpoly->point_edgemid_angles[1] / bpoly->corner_edgemid_angles[1];

      if (isnan(corner_angle_weights[0]) || isnan(corner_angle_weights[1])) {
        freeBindData(bwdata);
        data->success = MOD_SDEF_BIND_RESULT_GENERIC_ERR;
        return nullptr;
      }

      /* Find which edge the point is closer to */
      if (corner_angle_weights[0] < corner_angle_weights[1]) {
        bpoly->dominant_edge = 0;
        bpoly->dominant_angle_weight = corner_angle_weights[0];
      }
      else {
        bpoly->dominant_edge = 1;
        bpoly->dominant_angle_weight = corner_angle_weights[1];
      }

      /* Check for invalid weights just in case computations fail. */
      if (bpoly->dominant_angle_weight < 0 || bpoly->dominant_angle_weight > 1) {
        freeBindData(bwdata);
        data->success = MOD_SDEF_BIND_RESULT_GENERIC_ERR;
        return nullptr;
      }

      bpoly->dominant_angle_weight = sinf(bpoly->dominant_angle_weight * M_PI_2);

      /* Compute quadratic angular scale interpolation weight */
      {
        const float edge_angle_a = bpoly->point_edgemid_angles[bpoly->dominant_edge];
        const float edge_angle_b = bpoly->point_edgemid_angles[!bpoly->dominant_edge];
        /* Clamp so skinny faces with near zero `edgemid_angle`
         * won't cause numeric problems. see #81988. */
        scale_weight = edge_angle_a / max_ff(edge_angle_a, bpoly->edgemid_angle);
        scale_weight /= scale_weight + (edge_angle_b / max_ff(edge_angle_b, bpoly->edgemid_angle));
      }

      sqr = scale_weight * scale_weight;
      inv_sqr = 1.0f - scale_weight;
      inv_sqr *= inv_sqr;
      scale_weight = sqr / (sqr + inv_sqr);

      BLI_assert(scale_weight >= 0 && scale_weight <= 1);

      /* Compute interpolated scale (no longer need the individual scales,
       * so simply storing the result over the scale in index zero) */
      bpoly->scales[0] = interpf(bpoly->scale_mid,
                                 interpf(bpoly->scales[!bpoly->dominant_edge],
                                         bpoly->scales[bpoly->dominant_edge],
                                         scale_weight),
                                 bpoly->dominant_angle_weight);

      /* Scale the point distance weights, and introduce falloff */
      bpoly->weight_dist_proj /= bpoly->scales[0];
      bpoly->weight_dist_proj = powf(bpoly->weight_dist_proj, data->falloff);

      bpoly->weight_dist /= avg_point_dist;
      bpoly->weight_dist = powf(bpoly->weight_dist, data->falloff);

      /* Re-check for infinite weights, now that all scalings and interpolations are computed */
      if (bpoly->weight_dist < FLT_EPSILON) {
        inf_weight_flags |= MOD_SDEF_INFINITE_WEIGHT_DIST_PROJ;
        inf_weight_flags |= MOD_SDEF_INFINITE_WEIGHT_DIST;
      }
      else if (bpoly->weight_dist_proj < FLT_EPSILON) {
        inf_weight_flags |= MOD_SDEF_INFINITE_WEIGHT_DIST_PROJ;
      }
      else if (bpoly->weight_angular < FLT_EPSILON) {
        inf_weight_flags |= MOD_SDEF_INFINITE_WEIGHT_ANGULAR;
      }
    }
  }
  else if (!(inf_weight_flags & MOD_SDEF_INFINITE_WEIGHT_DIST)) {
    bpoly = bwdata->bind_polys;

    for (int i = 0; i < bwdata->faces_num; bpoly++, i++) {
      /* Scale the point distance weight by average point distance, and introduce falloff */
      bpoly->weight_dist /= avg_point_dist;
      bpoly->weight_dist = powf(bpoly->weight_dist, data->falloff);

      /* Re-check for infinite weights, now that all scalings and interpolations are computed */
      if (bpoly->weight_dist < FLT_EPSILON) {
        inf_weight_flags |= MOD_SDEF_INFINITE_WEIGHT_DIST;
      }
    }
  }

  /* Final loop, to compute actual weights */
  bpoly = bwdata->bind_polys;

  for (int i = 0; i < bwdata->faces_num; bpoly++, i++) {
    /* Weight computation from components */
    if (inf_weight_flags & MOD_SDEF_INFINITE_WEIGHT_DIST) {
      bpoly->weight = bpoly->weight_dist < FLT_EPSILON ? 1.0f : 0.0f;
    }
    else if (inf_weight_flags & MOD_SDEF_INFINITE_WEIGHT_DIST_PROJ) {
      bpoly->weight = bpoly->weight_dist_proj < FLT_EPSILON ? 1.0f / bpoly->weight_dist : 0.0f;
    }
    else if (inf_weight_flags & MOD_SDEF_INFINITE_WEIGHT_ANGULAR) {
      bpoly->weight = bpoly->weight_angular < FLT_EPSILON ?
                          1.0f / bpoly->weight_dist_proj / bpoly->weight_dist :
                          0.0f;
    }
    else {
      bpoly->weight = 1.0f / bpoly->weight_angular / bpoly->weight_dist_proj / bpoly->weight_dist;
    }

    /* Apply after other kinds of scaling so the faces corner angle is always
     * scaled in a uniform way, preventing heavily sub-divided triangle fans
     * from having a lop-sided influence on the weighting, see #81988. */
    bpoly->weight *= bpoly->edgemid_angle / M_PI;

    tot_weight += bpoly->weight;
  }

  bpoly = bwdata->bind_polys;

  for (int i = 0; i < bwdata->faces_num; bpoly++, i++) {
    bpoly->weight /= tot_weight;

    /* Evaluate if this face is relevant to bind */
    /* Even though the weights should add up to 1.0,
     * the losses of weights smaller than epsilon here
     * should be negligible... */
    if (bpoly->weight >= FLT_EPSILON) {
      if (bpoly->inside) {
        bwdata->binds_num += 1;
      }
      else {
        if (bpoly->dominant_angle_weight < FLT_EPSILON ||
            1.0f - bpoly->dominant_angle_weight < FLT_EPSILON)
        {
          bwdata->binds_num += 1;
        }
        else {
          bwdata->binds_num += 2;
        }
      }
    }
  }

  return bwdata;
}

BLI_INLINE float computeNormalDisplacement(const float point_co[3],
                                           const float point_co_proj[3],
                                           const float normal[3])
{
  float disp_vec[3];
  float normal_dist;

  sub_v3_v3v3(disp_vec, point_co, point_co_proj);
  normal_dist = len_v3(disp_vec);

  if (dot_v3v3(disp_vec, normal) < 0) {
    normal_dist *= -1;
  }

  return normal_dist;
}

static void bindVert(void *__restrict userdata,
                     const int index,
                     const TaskParallelTLS *__restrict /*tls*/)
{
  SDefBindCalcData *const data = (SDefBindCalcData *)userdata;
  float point_co[3];
  float point_co_proj[3];

  SDefBindWeightData *bwdata;
  SDefVert *sdvert = data->bind_verts + index;
  SDefBindPoly *bpoly;
  SDefBind *sdbind;

  sdvert->vertex_idx = index;

  if (data->success != MOD_SDEF_BIND_RESULT_SUCCESS) {
    sdvert->binds = nullptr;
    sdvert->binds_num = 0;
    return;
  }

  if (data->sparse_bind) {
    float weight = 0.0f;

    if (data->dvert && data->defgrp_index != -1) {
      weight = BKE_defvert_find_weight(&data->dvert[index], data->defgrp_index);
    }

    if (data->invert_vgroup) {
      weight = 1.0f - weight;
    }

    if (weight <= 0) {
      sdvert->binds = nullptr;
      sdvert->binds_num = 0;
      return;
    }
  }

  copy_v3_v3(point_co, data->vertexCos[index]);
  bwdata = computeBindWeights(data, point_co);

  if (bwdata == nullptr) {
    sdvert->binds = nullptr;
    sdvert->binds_num = 0;
    return;
  }

  sdvert->binds = MEM_calloc_arrayN<SDefBind>(bwdata->binds_num, "SDefVertBindData");
  if (sdvert->binds == nullptr) {
    data->success = MOD_SDEF_BIND_RESULT_MEM_ERR;
    sdvert->binds_num = 0;
    return;
  }

  sdvert->binds_num = bwdata->binds_num;

  sdbind = sdvert->binds;

  bpoly = bwdata->bind_polys;

  for (int i = 0; i < bwdata->binds_num; bpoly++) {
    if (bpoly->weight >= FLT_EPSILON) {
      if (bpoly->inside) {
        sdbind->influence = bpoly->weight;
        sdbind->verts_num = bpoly->verts_num;

        sdbind->mode = MOD_SDEF_MODE_NGONS;
        sdbind->vert_weights = MEM_malloc_arrayN<float>(size_t(bpoly->verts_num),
                                                        "SDefNgonVertWeights");
        if (sdbind->vert_weights == nullptr) {
          data->success = MOD_SDEF_BIND_RESULT_MEM_ERR;
          return;
        }

        sdbind->vert_inds = MEM_malloc_arrayN<uint>(size_t(bpoly->verts_num), "SDefNgonVertInds");
        if (sdbind->vert_inds == nullptr) {
          data->success = MOD_SDEF_BIND_RESULT_MEM_ERR;
          return;
        }

        interp_weights_poly_v2(
            sdbind->vert_weights, bpoly->coords_v2, bpoly->verts_num, bpoly->point_v2);

        /* Re-project vert based on weights and original face verts,
         * to reintroduce face non-planarity */
        zero_v3(point_co_proj);
        for (int j = 0; j < bpoly->verts_num; j++) {
          const int vert_i = data->corner_verts[bpoly->loopstart + j];
          madd_v3_v3fl(point_co_proj, bpoly->coords[j], sdbind->vert_weights[j]);
          sdbind->vert_inds[j] = vert_i;
        }

        sdbind->normal_dist = computeNormalDisplacement(point_co, point_co_proj, bpoly->normal);

        sdbind++;
        i++;
      }
      else {
        float tmp_vec[3];
        float cent[3], norm[3];
        float v1[3], v2[3], v3[3];

        if (1.0f - bpoly->dominant_angle_weight >= FLT_EPSILON) {
          sdbind->influence = bpoly->weight * (1.0f - bpoly->dominant_angle_weight);
          sdbind->verts_num = bpoly->verts_num;

          sdbind->mode = MOD_SDEF_MODE_CENTROID;
          sdbind->vert_weights = MEM_malloc_arrayN<float>(3, "SDefCentVertWeights");
          if (sdbind->vert_weights == nullptr) {
            data->success = MOD_SDEF_BIND_RESULT_MEM_ERR;
            return;
          }

          sdbind->vert_inds = MEM_malloc_arrayN<uint>(size_t(bpoly->verts_num),
                                                      "SDefCentVertInds");
          if (sdbind->vert_inds == nullptr) {
            data->success = MOD_SDEF_BIND_RESULT_MEM_ERR;
            return;
          }

          sortPolyVertsEdge(sdbind->vert_inds,
                            &data->corner_verts[bpoly->loopstart],
                            &data->corner_edges[bpoly->loopstart],
                            bpoly->edge_inds[bpoly->dominant_edge],
                            bpoly->verts_num);

          copy_v3_v3(v1, data->targetCos[sdbind->vert_inds[0]]);
          copy_v3_v3(v2, data->targetCos[sdbind->vert_inds[1]]);
          copy_v3_v3(v3, bpoly->centroid);

          mid_v3_v3v3v3(cent, v1, v2, v3);
          normal_tri_v3(norm, v1, v2, v3);

          add_v3_v3v3(tmp_vec, point_co, bpoly->normal);

          /* We are sure the line is not parallel to the plane.
           * Checking return value just to avoid warning... */
          if (!isect_line_plane_v3(point_co_proj, point_co, tmp_vec, cent, norm)) {
            BLI_assert(false);
          }

          interp_weights_tri_v3(sdbind->vert_weights, v1, v2, v3, point_co_proj);

          sdbind->normal_dist = computeNormalDisplacement(point_co, point_co_proj, bpoly->normal);

          sdbind++;
          i++;
        }

        if (bpoly->dominant_angle_weight >= FLT_EPSILON) {
          sdbind->influence = bpoly->weight * bpoly->dominant_angle_weight;
          sdbind->verts_num = bpoly->verts_num;

          sdbind->mode = MOD_SDEF_MODE_CORNER_TRIS;
          sdbind->vert_weights = MEM_malloc_arrayN<float>(3, "SDefTriVertWeights");
          if (sdbind->vert_weights == nullptr) {
            data->success = MOD_SDEF_BIND_RESULT_MEM_ERR;
            return;
          }

          sdbind->vert_inds = MEM_malloc_arrayN<uint>(size_t(bpoly->verts_num), "SDefTriVertInds");
          if (sdbind->vert_inds == nullptr) {
            data->success = MOD_SDEF_BIND_RESULT_MEM_ERR;
            return;
          }

          sortPolyVertsTri(sdbind->vert_inds,
                           &data->corner_verts[bpoly->loopstart],
                           bpoly->edge_vert_inds[0],
                           bpoly->verts_num);

          copy_v3_v3(v1, data->targetCos[sdbind->vert_inds[0]]);
          copy_v3_v3(v2, data->targetCos[sdbind->vert_inds[1]]);
          copy_v3_v3(v3, data->targetCos[sdbind->vert_inds[2]]);

          mid_v3_v3v3v3(cent, v1, v2, v3);
          normal_tri_v3(norm, v1, v2, v3);

          add_v3_v3v3(tmp_vec, point_co, bpoly->normal);

          /* We are sure the line is not parallel to the plane.
           * Checking return value just to avoid warning... */
          if (!isect_line_plane_v3(point_co_proj, point_co, tmp_vec, cent, norm)) {
            BLI_assert(false);
          }

          interp_weights_tri_v3(sdbind->vert_weights, v1, v2, v3, point_co_proj);

          sdbind->normal_dist = computeNormalDisplacement(point_co, point_co_proj, bpoly->normal);

          sdbind++;
          i++;
        }
      }
    }
  }

  freeBindData(bwdata);
}

/* Remove vertices without bind data from the bind array. */
static void compactSparseBinds(SurfaceDeformModifierData *smd)
{
  smd->bind_verts_num = 0;

  for (uint i = 0; i < smd->mesh_verts_num; i++) {
    if (smd->verts[i].binds_num > 0) {
      smd->bind_verts_num++;
    }
  }

  SDefVert *new_verts = MEM_malloc_arrayN<SDefVert>(size_t(smd->bind_verts_num), __func__);

  /* Move data to new_verts. */
  BLI_assert(smd->verts_sharing_info->is_mutable());
  int dst_index = 0;
  for (uint i = 0; i < smd->mesh_verts_num; i++) {
    if (smd->verts[i].binds_num > 0) {
      new_verts[dst_index++] = smd->verts[i];
      smd->verts[i] = {};
    }
  }

  smd->verts_sharing_info->remove_user_and_delete_if_last();
  smd->verts = new_verts;
  smd->verts_sharing_info = MEM_new<BindVertsImplicitSharing>(
      __func__, smd->verts, smd->bind_verts_num);
}

static bool surfacedeformBind(Object *ob,
                              SurfaceDeformModifierData *smd_orig,
                              SurfaceDeformModifierData *smd_eval,
                              float (*vertexCos)[3],
                              uint verts_num,
                              uint target_faces_num,
                              uint target_verts_num,
                              Mesh *target,
                              Mesh *mesh)
{
  using namespace blender;
  const blender::Span<blender::float3> positions = target->vert_positions();
  const blender::Span<blender::int2> edges = target->edges();
  const blender::OffsetIndices polys = target->faces();
  const blender::Span<int> corner_verts = target->corner_verts();
  const blender::Span<int> corner_edges = target->corner_edges();
  uint tedges_num = target->edges_num;
  int adj_result;

  if (target->faces_num == 0) {
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "Target has no faces");
    return false;
  }

  SDefAdjacencyArray *vert_edges = MEM_calloc_arrayN<SDefAdjacencyArray>(target_verts_num,
                                                                         "SDefVertEdgeMap");
  if (vert_edges == nullptr) {
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "Out of memory");
    return false;
  }

  SDefAdjacency *adj_array = MEM_malloc_arrayN<SDefAdjacency>(2 * size_t(tedges_num),
                                                              "SDefVertEdge");
  if (adj_array == nullptr) {
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "Out of memory");
    MEM_freeN(vert_edges);
    return false;
  }

  SDefEdgePolys *edge_polys = MEM_calloc_arrayN<SDefEdgePolys>(tedges_num, "SDefEdgeFaceMap");
  if (edge_polys == nullptr) {
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "Out of memory");
    MEM_freeN(vert_edges);
    MEM_freeN(adj_array);
    return false;
  }

  smd_orig->verts = MEM_calloc_arrayN<SDefVert>(size_t(verts_num), "SDefBindVerts");
  if (smd_orig->verts == nullptr) {
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "Out of memory");
    freeAdjacencyMap(vert_edges, adj_array, edge_polys);
    return false;
  }
  smd_orig->verts_sharing_info = MEM_new<BindVertsImplicitSharing>(
      __func__, smd_orig->verts, verts_num);

  blender::bke::BVHTreeFromMesh treeData = target->bvh_corner_tris();
  if (treeData.tree == nullptr) {
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "Out of memory");
    freeAdjacencyMap(vert_edges, adj_array, edge_polys);
    implicit_sharing::free_shared_data(&smd_orig->verts, &smd_orig->verts_sharing_info);
    return false;
  }

  adj_result = buildAdjacencyMap(polys, edges, corner_edges, vert_edges, adj_array, edge_polys);

  if (adj_result == MOD_SDEF_BIND_RESULT_NONMANY_ERR) {
    BKE_modifier_set_error(
        ob, (ModifierData *)smd_eval, "Target has edges with more than two polygons");
    freeAdjacencyMap(vert_edges, adj_array, edge_polys);
    implicit_sharing::free_shared_data(&smd_orig->verts, &smd_orig->verts_sharing_info);
    return false;
  }

  smd_orig->mesh_verts_num = verts_num;
  smd_orig->target_verts_num = target_verts_num;
  smd_orig->target_polys_num = target_faces_num;

  int defgrp_index;
  const MDeformVert *dvert;
  MOD_get_vgroup(ob, mesh, smd_orig->defgrp_name, &dvert, &defgrp_index);
  const bool invert_vgroup = (smd_orig->flags & MOD_SDEF_INVERT_VGROUP) != 0;
  const bool sparse_bind = (smd_orig->flags & MOD_SDEF_SPARSE_BIND) != 0;

  SDefBindCalcData data{};
  data.treeData = &treeData;
  data.vert_edges = vert_edges;
  data.edge_polys = edge_polys;
  data.polys = polys;
  data.edges = edges;
  data.corner_verts = corner_verts;
  data.corner_edges = corner_edges;
  data.corner_tris = target->corner_tris();
  data.tri_faces = target->corner_tri_faces();
  data.targetCos = MEM_malloc_arrayN<float[3]>(size_t(target_verts_num),
                                               "SDefTargetBindVertArray");
  data.bind_verts = smd_orig->verts;
  data.vertexCos = vertexCos;
  data.falloff = smd_orig->falloff;
  data.success = MOD_SDEF_BIND_RESULT_SUCCESS;
  data.dvert = dvert;
  data.defgrp_index = defgrp_index;
  data.invert_vgroup = invert_vgroup;
  data.sparse_bind = sparse_bind;

  if (data.targetCos == nullptr) {
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "Out of memory");
    free_data((ModifierData *)smd_orig);
    return false;
  }

  invert_m4_m4(data.imat, smd_orig->mat);

  for (int i = 0; i < target_verts_num; i++) {
    mul_v3_m4v3(data.targetCos[i], smd_orig->mat, positions[i]);
  }

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (verts_num > 10000);
  BLI_task_parallel_range(0, verts_num, &data, bindVert, &settings);

  MEM_freeN(data.targetCos);

  if (sparse_bind) {
    compactSparseBinds(smd_orig);
  }
  else {
    smd_orig->bind_verts_num = verts_num;
  }

  if (data.success == MOD_SDEF_BIND_RESULT_MEM_ERR) {
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "Out of memory");
    free_data((ModifierData *)smd_orig);
  }
  else if (data.success == MOD_SDEF_BIND_RESULT_NONMANY_ERR) {
    BKE_modifier_set_error(
        ob, (ModifierData *)smd_eval, "Target has edges with more than two polygons");
    free_data((ModifierData *)smd_orig);
  }
  else if (data.success == MOD_SDEF_BIND_RESULT_CONCAVE_ERR) {
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "Target contains concave polygons");
    free_data((ModifierData *)smd_orig);
  }
  else if (data.success == MOD_SDEF_BIND_RESULT_OVERLAP_ERR) {
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "Target contains overlapping vertices");
    free_data((ModifierData *)smd_orig);
  }
  else if (data.success == MOD_SDEF_BIND_RESULT_GENERIC_ERR) {
    /* I know this message is vague, but I could not think of a way
     * to explain this with a reasonably sized message.
     * Though it shouldn't really matter all that much,
     * because this is very unlikely to occur */
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "Target contains invalid polygons");
    free_data((ModifierData *)smd_orig);
  }
  else if (smd_orig->bind_verts_num == 0 || !smd_orig->verts) {
    data.success = MOD_SDEF_BIND_RESULT_GENERIC_ERR;
    BKE_modifier_set_error(ob, (ModifierData *)smd_eval, "No vertices were bound");
    free_data((ModifierData *)smd_orig);
  }

  freeAdjacencyMap(vert_edges, adj_array, edge_polys);

  return data.success == 1;
}

static void deformVert(void *__restrict userdata,
                       const int index,
                       const TaskParallelTLS *__restrict /*tls*/)
{
  const SDefDeformData *const data = (SDefDeformData *)userdata;
  const SDefBind *sdbind = data->bind_verts[index].binds;
  const int sdbind_num = data->bind_verts[index].binds_num;
  const uint vertex_idx = data->bind_verts[index].vertex_idx;
  float *const vertexCos = data->vertexCos[vertex_idx];
  float norm[3], temp[3], offset[3];

  /* Retrieve the value of the weight vertex group if specified. */
  float weight = 1.0f;

  if (data->dvert && data->defgrp_index != -1) {
    weight = BKE_defvert_find_weight(&data->dvert[vertex_idx], data->defgrp_index);

    if (data->invert_vgroup) {
      weight = 1.0f - weight;
    }
  }

  /* Check if this vertex will be deformed. If it is not deformed we return and avoid
   * unnecessary calculations. */
  if (weight == 0.0f) {
    return;
  }

  zero_v3(offset);

  int max_verts = 0;
  for (int j = 0; j < sdbind_num; j++) {
    max_verts = std::max(max_verts, int(sdbind[j].verts_num));
  }

  /* Allocate a `coords_buffer` that fits all the temp-data. */
  blender::Array<blender::float3, 256> coords_buffer(max_verts);

  for (int j = 0; j < sdbind_num; j++, sdbind++) {
    for (int k = 0; k < sdbind->verts_num; k++) {
      copy_v3_v3(coords_buffer[k], data->targetCos[sdbind->vert_inds[k]]);
    }

    normal_poly_v3(
        norm, reinterpret_cast<const float (*)[3]>(coords_buffer.data()), sdbind->verts_num);
    zero_v3(temp);

    switch (sdbind->mode) {
      /* ---------- corner_tri mode ---------- */
      case MOD_SDEF_MODE_CORNER_TRIS: {
        madd_v3_v3fl(temp, data->targetCos[sdbind->vert_inds[0]], sdbind->vert_weights[0]);
        madd_v3_v3fl(temp, data->targetCos[sdbind->vert_inds[1]], sdbind->vert_weights[1]);
        madd_v3_v3fl(temp, data->targetCos[sdbind->vert_inds[2]], sdbind->vert_weights[2]);
        break;
      }

      /* ---------- ngon mode ---------- */
      case MOD_SDEF_MODE_NGONS: {
        for (int k = 0; k < sdbind->verts_num; k++) {
          madd_v3_v3fl(temp, coords_buffer[k], sdbind->vert_weights[k]);
        }
        break;
      }

      /* ---------- centroid mode ---------- */
      case MOD_SDEF_MODE_CENTROID: {
        float cent[3];
        mid_v3_v3_array(
            cent, reinterpret_cast<const float (*)[3]>(coords_buffer.data()), sdbind->verts_num);

        madd_v3_v3fl(temp, data->targetCos[sdbind->vert_inds[0]], sdbind->vert_weights[0]);
        madd_v3_v3fl(temp, data->targetCos[sdbind->vert_inds[1]], sdbind->vert_weights[1]);
        madd_v3_v3fl(temp, cent, sdbind->vert_weights[2]);
        break;
      }
    }

    /* Apply normal offset (generic for all modes) */
    madd_v3_v3fl(temp, norm, sdbind->normal_dist);

    madd_v3_v3fl(offset, temp, sdbind->influence);
  }
  /* Subtract the vertex coord to get the deformation offset. */
  sub_v3_v3(offset, vertexCos);

  /* Add the offset to start coord multiplied by the strength and weight values. */
  madd_v3_v3fl(vertexCos, offset, data->strength * weight);
}

static void surfacedeformModifier_do(ModifierData *md,
                                     const ModifierEvalContext *ctx,
                                     float (*vertexCos)[3],
                                     uint verts_num,
                                     Object *ob,
                                     Mesh *mesh)
{
  SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;
  Mesh *target;
  uint target_verts_num, target_faces_num;

  /* Exit function if bind flag is not set (free bind data if any). */
  if (!(smd->flags & MOD_SDEF_BIND)) {
    if (smd->verts != nullptr) {
      if (!DEG_is_active(ctx->depsgraph)) {
        BKE_modifier_set_error(ob, md, "Attempt to bind from inactive dependency graph");
        return;
      }
      ModifierData *md_orig = BKE_modifier_get_original(ob, md);
      free_data(md_orig);
    }
    return;
  }

  Object *ob_target = smd->target;
  target = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_target);
  if (!target) {
    BKE_modifier_set_error(ob, md, "No valid target mesh");
    return;
  }

  target_verts_num = BKE_mesh_wrapper_vert_len(target);
  target_faces_num = BKE_mesh_wrapper_face_len(target);

  /* If not bound, execute bind. */
  if (smd->verts == nullptr) {
    if (!DEG_is_active(ctx->depsgraph)) {
      BKE_modifier_set_error(ob, md, "Attempt to unbind from inactive dependency graph");
      return;
    }

    SurfaceDeformModifierData *smd_orig = (SurfaceDeformModifierData *)BKE_modifier_get_original(
        ob, md);
    float tmp_mat[4][4];

    invert_m4_m4(tmp_mat, ob->object_to_world().ptr());
    mul_m4_m4m4(smd_orig->mat, tmp_mat, ob_target->object_to_world().ptr());

    /* Avoid converting edit-mesh data, binding is an exception. */
    BKE_mesh_wrapper_ensure_mdata(target);

    if (!surfacedeformBind(ob,
                           smd_orig,
                           smd,
                           vertexCos,
                           verts_num,
                           target_faces_num,
                           target_verts_num,
                           target,
                           mesh))
    {
      smd->flags &= ~MOD_SDEF_BIND;
    }
    /* Early abort, this is binding 'call', no need to perform whole evaluation. */
    return;
  }

  /* Geometry count on the deforming mesh. */
  if (smd->mesh_verts_num != verts_num) {
    BKE_modifier_set_error(
        ob, md, "Vertices changed from %u to %u", smd->mesh_verts_num, verts_num);
    return;
  }

  /* Geometry count on the target mesh. */
  if (smd->target_polys_num != target_faces_num && smd->target_verts_num == 0) {
    /* Change in the number of polygons does not really imply change in the vertex count, but
     * this is how the modifier worked before the vertex count was known. Follow the legacy
     * logic without requirement to re-bind the mesh. */
    BKE_modifier_set_error(
        ob, md, "Target polygons changed from %u to %u", smd->target_polys_num, target_faces_num);
    return;
  }
  if (!ELEM(smd->target_verts_num, 0, target_verts_num)) {
    if (smd->target_verts_num > target_verts_num) {
      /* Number of vertices on the target did reduce. There is no usable recovery from this. */
      BKE_modifier_set_error(ob,
                             md,
                             "Target vertices changed from %u to %u",
                             smd->target_verts_num,
                             target_verts_num);
      return;
    }

    /* Assume the increase in the vertex count means that the "new" vertices in the target mesh are
     * added after the original ones. This covers typical case when target was at the subdivision
     * level 0 and then subdivision was increased (i.e. for the render purposes). */

    BKE_modifier_set_warning(ob,
                             md,
                             "Target vertices changed from %u to %u, continuing anyway",
                             smd->target_verts_num,
                             target_verts_num);

    /* In theory we only need the `smd->verts_num` vertices in the `targetCos` for evaluation, but
     * it is not currently possible to request a subset of coordinates: the API expects that the
     * caller needs coordinates of all vertices and asserts for it. */
  }

  /* Early out if modifier would not affect input at all - still *after* the sanity checks
   * (and potential binding) above. */
  if (smd->strength == 0.0f) {
    return;
  }

  int defgrp_index;
  const MDeformVert *dvert;
  MOD_get_vgroup(ob, mesh, smd->defgrp_name, &dvert, &defgrp_index);
  const bool invert_vgroup = (smd->flags & MOD_SDEF_INVERT_VGROUP) != 0;

  /* Actual vertex location update starts here */
  SDefDeformData data{};
  data.bind_verts = smd->verts;
  data.targetCos = MEM_malloc_arrayN<float[3]>(size_t(target_verts_num), "SDefTargetVertArray");
  data.vertexCos = vertexCos;
  data.dvert = dvert;
  data.defgrp_index = defgrp_index;
  data.invert_vgroup = invert_vgroup;
  data.strength = smd->strength;

  if (data.targetCos != nullptr) {
    BKE_mesh_wrapper_vert_coords_copy_with_mat4(
        target, data.targetCos, target_verts_num, smd->mat);

    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.use_threading = (smd->bind_verts_num > 10000);
    BLI_task_parallel_range(0, smd->bind_verts_num, &data, deformVert, &settings);

    MEM_freeN(data.targetCos);
  }
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  surfacedeformModifier_do(md,
                           ctx,
                           reinterpret_cast<float (*)[3]>(positions.data()),
                           positions.size(),
                           ctx->object,
                           mesh);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the mesh is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return (smd->target == nullptr || smd->target->type != OB_MESH) &&
         !(smd->verts != nullptr && !(smd->flags & MOD_SDEF_BIND));
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA target_ptr = RNA_pointer_get(ptr, "target");

  bool is_bound = RNA_boolean_get(ptr, "is_bound");

  layout->use_property_split_set(true);

  col = &layout->column(false);
  col->active_set(!is_bound);
  col->prop(ptr, "target", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "falloff", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->prop(ptr, "strength", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  col = &layout->column(false);
  col->enabled_set(!is_bound);
  col->active_set(!is_bound && RNA_string_length(ptr, "vertex_group") != 0);
  col->prop(ptr, "use_sparse_bind", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  col = &layout->column(false);
  if (is_bound) {
    col->op("OBJECT_OT_surfacedeform_bind", IFACE_("Unbind"), ICON_NONE);
  }
  else {
    col->active_set(!RNA_pointer_is_null(&target_ptr));
    col->op("OBJECT_OT_surfacedeform_bind", IFACE_("Bind"), ICON_NONE);
  }
  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_SurfaceDeform, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID *id_owner, const ModifierData *md)
{
  SurfaceDeformModifierData smd = *(const SurfaceDeformModifierData *)md;
  const bool is_undo = BLO_write_is_undo(writer);

  if (ID_IS_OVERRIDE_LIBRARY(id_owner) && !is_undo) {
    BLI_assert(!ID_IS_LINKED(id_owner));
    const bool is_local = (md->flag & eModifierFlag_OverrideLibrary_Local) != 0;
    if (!is_local) {
      /* Modifier coming from linked data cannot be bound from an override, so we can remove all
       * binding data, can save a significant amount of memory. */
      smd.bind_verts_num = 0;
      smd.verts = nullptr;
      smd.verts_sharing_info = nullptr;
    }
  }

  if (smd.verts != nullptr) {
    BLO_write_shared(
        writer, smd.verts, sizeof(SDefVert) * smd.bind_verts_num, smd.verts_sharing_info, [&]() {
          SDefVert *bind_verts = smd.verts;
          BLO_write_struct_array(writer, SDefVert, smd.bind_verts_num, bind_verts);

          for (int i = 0; i < smd.bind_verts_num; i++) {
            BLO_write_struct_array(writer, SDefBind, bind_verts[i].binds_num, bind_verts[i].binds);

            if (bind_verts[i].binds) {
              for (int j = 0; j < bind_verts[i].binds_num; j++) {
                BLO_write_uint32_array(
                    writer, bind_verts[i].binds[j].verts_num, bind_verts[i].binds[j].vert_inds);

                if (ELEM(bind_verts[i].binds[j].mode,
                         MOD_SDEF_MODE_CENTROID,
                         MOD_SDEF_MODE_CORNER_TRIS))
                {
                  BLO_write_float3_array(writer, 1, bind_verts[i].binds[j].vert_weights);
                }
                else {
                  BLO_write_float_array(writer,
                                        bind_verts[i].binds[j].verts_num,
                                        bind_verts[i].binds[j].vert_weights);
                }
              }
            }
          }
        });
  }

  BLO_write_struct_at_address(writer, SurfaceDeformModifierData, md, &smd);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;

  if (smd->verts) {
    smd->verts_sharing_info = BLO_read_shared(reader, &smd->verts, [&]() {
      BLO_read_struct_array(reader, SDefVert, smd->bind_verts_num, &smd->verts);
      for (int i = 0; i < smd->bind_verts_num; i++) {
        BLO_read_struct_array(reader, SDefBind, smd->verts[i].binds_num, &smd->verts[i].binds);

        if (smd->verts[i].binds) {
          for (int j = 0; j < smd->verts[i].binds_num; j++) {
            BLO_read_uint32_array(
                reader, smd->verts[i].binds[j].verts_num, &smd->verts[i].binds[j].vert_inds);

            if (ELEM(smd->verts[i].binds[j].mode,
                     MOD_SDEF_MODE_CENTROID,
                     MOD_SDEF_MODE_CORNER_TRIS))
            {
              BLO_read_float3_array(reader, 1, &smd->verts[i].binds[j].vert_weights);
            }
            else {
              BLO_read_float_array(
                  reader, smd->verts[i].binds[j].verts_num, &smd->verts[i].binds[j].vert_weights);
            }
          }
        }
      }
      return MEM_new<BindVertsImplicitSharing>(
          "BindVertsImplicitSharing", smd->verts, smd->bind_verts_num);
    });
  }
}

ModifierTypeInfo modifierType_SurfaceDeform = {
    /*idname*/ "SurfaceDeform",
    /*name*/ N_("SurfaceDeform"),
    /*struct_name*/ "SurfaceDeformModifierData",
    /*struct_size*/ sizeof(SurfaceDeformModifierData),
    /*srna*/ &RNA_SurfaceDeformModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_MESHDEFORM,

    /*copy_data*/ copy_data,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ free_data,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ blend_write,
    /*blend_read*/ blend_read,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
