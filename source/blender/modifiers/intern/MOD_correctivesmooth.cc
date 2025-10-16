/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 *
 * Method of smoothing deformation, also known as 'delta-mush'.
 */

#include "BLI_math_base.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_deform.hh"
#include "BKE_editmesh.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"
#include "MOD_util.hh"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_query.hh"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_time.h"
#  include "BLI_time_utildefines.h"
#endif

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

static void init_data(ModifierData *md)
{
  CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(csmd, modifier));

  MEMCPY_STRUCT_AFTER(csmd, DNA_struct_default_get(CorrectiveSmoothModifierData), modifier);

  csmd->delta_cache.deltas = nullptr;
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const CorrectiveSmoothModifierData *csmd = (const CorrectiveSmoothModifierData *)md;
  CorrectiveSmoothModifierData *tcsmd = (CorrectiveSmoothModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  blender::implicit_sharing::copy_shared_pointer(csmd->bind_coords,
                                                 csmd->bind_coords_sharing_info,
                                                 &tcsmd->bind_coords,
                                                 &tcsmd->bind_coords_sharing_info);

  tcsmd->delta_cache.deltas = nullptr;
  tcsmd->delta_cache.deltas_num = 0;
}

static void freeBind(CorrectiveSmoothModifierData *csmd)
{
  blender::implicit_sharing::free_shared_data(&csmd->bind_coords, &csmd->bind_coords_sharing_info);
  MEM_SAFE_FREE(csmd->delta_cache.deltas);

  csmd->bind_coords_num = 0;
}

static void free_data(ModifierData *md)
{
  CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;
  freeBind(csmd);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;

  /* ask for vertex groups if we need them */
  if (csmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

/* check individual weights for changes and cache values */
static void mesh_get_weights(const MDeformVert *dvert,
                             const int defgrp_index,
                             const uint verts_num,
                             const bool use_invert_vgroup,
                             float *smooth_weights)
{
  uint i;

  for (i = 0; i < verts_num; i++, dvert++) {
    const float w = BKE_defvert_find_weight(dvert, defgrp_index);

    if (use_invert_vgroup == false) {
      smooth_weights[i] = w;
    }
    else {
      smooth_weights[i] = 1.0f - w;
    }
  }
}

static void mesh_get_boundaries(Mesh *mesh, float *smooth_weights)
{
  const blender::Span<blender::int2> edges = mesh->edges();
  const blender::OffsetIndices faces = mesh->faces();
  const blender::Span<int> corner_edges = mesh->corner_edges();

  /* Flag boundary edges so only boundaries are set to 1. */
  uint8_t *boundaries = MEM_calloc_arrayN<uint8_t>(size_t(edges.size()), __func__);

  for (const int64_t i : faces.index_range()) {
    for (const int edge : corner_edges.slice(faces[i])) {
      uint8_t *e_value = &boundaries[edge];
      *e_value |= uint8_t((*e_value) + 1);
    }
  }

  for (const int64_t i : edges.index_range()) {
    if (boundaries[i] == 1) {
      smooth_weights[edges[i][0]] = 0.0f;
      smooth_weights[edges[i][1]] = 0.0f;
    }
  }

  MEM_freeN(boundaries);
}

/* -------------------------------------------------------------------- */
/* Simple Weighted Smoothing
 *
 * (average of surrounding verts)
 */
static void smooth_iter__simple(CorrectiveSmoothModifierData *csmd,
                                Mesh *mesh,
                                blender::MutableSpan<blender::float3> vertexCos,
                                const float *smooth_weights,
                                uint iterations)
{
  const float lambda = csmd->lambda;
  int i;

  const int edges_num = mesh->edges_num;
  const blender::Span<blender::int2> edges = mesh->edges();

  struct SmoothingData_Simple {
    float delta[3];
  };
  SmoothingData_Simple *smooth_data = MEM_calloc_arrayN<SmoothingData_Simple>(
      size_t(vertexCos.size()), __func__);

  float *vertex_edge_count_div = MEM_calloc_arrayN<float>(size_t(vertexCos.size()), __func__);

  /* calculate as floats to avoid int->float conversion in #smooth_iter */
  for (i = 0; i < edges_num; i++) {
    vertex_edge_count_div[edges[i][0]] += 1.0f;
    vertex_edge_count_div[edges[i][1]] += 1.0f;
  }

  /* a little confusing, but we can include 'lambda' and smoothing weight
   * here to avoid multiplying for every iteration */
  if (smooth_weights == nullptr) {
    for (i = 0; i < vertexCos.size(); i++) {
      vertex_edge_count_div[i] = lambda * (vertex_edge_count_div[i] ?
                                               (1.0f / vertex_edge_count_div[i]) :
                                               1.0f);
    }
  }
  else {
    for (i = 0; i < vertexCos.size(); i++) {
      vertex_edge_count_div[i] = smooth_weights[i] * lambda *
                                 (vertex_edge_count_div[i] ? (1.0f / vertex_edge_count_div[i]) :
                                                             1.0f);
    }
  }

  /* -------------------------------------------------------------------- */
  /* Main Smoothing Loop */

  while (iterations--) {
    for (i = 0; i < edges_num; i++) {
      SmoothingData_Simple *sd_v1;
      SmoothingData_Simple *sd_v2;
      float edge_dir[3];

      sub_v3_v3v3(edge_dir, vertexCos[edges[i][1]], vertexCos[edges[i][0]]);

      sd_v1 = &smooth_data[edges[i][0]];
      sd_v2 = &smooth_data[edges[i][1]];

      add_v3_v3(sd_v1->delta, edge_dir);
      sub_v3_v3(sd_v2->delta, edge_dir);
    }

    for (i = 0; i < vertexCos.size(); i++) {
      SmoothingData_Simple *sd = &smooth_data[i];
      madd_v3_v3fl(vertexCos[i], sd->delta, vertex_edge_count_div[i]);
      /* zero for the next iteration (saves memset on entire array) */
      memset(sd, 0, sizeof(*sd));
    }
  }

  MEM_freeN(vertex_edge_count_div);
  MEM_freeN(smooth_data);
}

/* -------------------------------------------------------------------- */
/* Edge-Length Weighted Smoothing
 */
static void smooth_iter__length_weight(CorrectiveSmoothModifierData *csmd,
                                       Mesh *mesh,
                                       blender::MutableSpan<blender::float3> vertexCos,
                                       const float *smooth_weights,
                                       uint iterations)
{
  const float eps = FLT_EPSILON * 10.0f;
  const uint edges_num = uint(mesh->edges_num);
  /* NOTE: the way this smoothing method works, its approx half as strong as the simple-smooth,
   * and 2.0 rarely spikes, double the value for consistent behavior. */
  const float lambda = csmd->lambda * 2.0f;
  const blender::Span<blender::int2> edges = mesh->edges();
  uint i;

  struct SmoothingData_Weighted {
    float delta[3];
    float edge_length_sum;
  };
  SmoothingData_Weighted *smooth_data = MEM_calloc_arrayN<SmoothingData_Weighted>(
      size_t(vertexCos.size()), __func__);

  /* calculate as floats to avoid int->float conversion in #smooth_iter */
  float *vertex_edge_count = MEM_calloc_arrayN<float>(size_t(vertexCos.size()), __func__);
  for (i = 0; i < edges_num; i++) {
    vertex_edge_count[edges[i][0]] += 1.0f;
    vertex_edge_count[edges[i][1]] += 1.0f;
  }

  /* -------------------------------------------------------------------- */
  /* Main Smoothing Loop */

  while (iterations--) {
    for (i = 0; i < edges_num; i++) {
      SmoothingData_Weighted *sd_v1;
      SmoothingData_Weighted *sd_v2;
      float edge_dir[3];
      float edge_dist;

      sub_v3_v3v3(edge_dir, vertexCos[edges[i][1]], vertexCos[edges[i][0]]);
      edge_dist = len_v3(edge_dir);

      /* weight by distance */
      mul_v3_fl(edge_dir, edge_dist);

      sd_v1 = &smooth_data[edges[i][0]];
      sd_v2 = &smooth_data[edges[i][1]];

      add_v3_v3(sd_v1->delta, edge_dir);
      sub_v3_v3(sd_v2->delta, edge_dir);

      sd_v1->edge_length_sum += edge_dist;
      sd_v2->edge_length_sum += edge_dist;
    }

    if (smooth_weights == nullptr) {
      /* fast-path */
      for (i = 0; i < vertexCos.size(); i++) {
        SmoothingData_Weighted *sd = &smooth_data[i];
        /* Divide by sum of all neighbor distances (weighted) and amount of neighbors,
         * (mean average). */
        const float div = sd->edge_length_sum * vertex_edge_count[i];
        if (div > eps) {
#if 0
          /* first calculate the new location */
          mul_v3_fl(sd->delta, 1.0f / div);
          /* then interpolate */
          madd_v3_v3fl(vertexCos[i], sd->delta, lambda);
#else
          /* do this in one step */
          madd_v3_v3fl(vertexCos[i], sd->delta, lambda / div);
#endif
        }
        /* zero for the next iteration (saves memset on entire array) */
        memset(sd, 0, sizeof(*sd));
      }
    }
    else {
      for (i = 0; i < vertexCos.size(); i++) {
        SmoothingData_Weighted *sd = &smooth_data[i];
        const float div = sd->edge_length_sum * vertex_edge_count[i];
        if (div > eps) {
          const float lambda_w = lambda * smooth_weights[i];
          madd_v3_v3fl(vertexCos[i], sd->delta, lambda_w / div);
        }

        memset(sd, 0, sizeof(*sd));
      }
    }
  }

  MEM_freeN(vertex_edge_count);
  MEM_freeN(smooth_data);
}

static void smooth_iter(CorrectiveSmoothModifierData *csmd,
                        Mesh *mesh,
                        blender::MutableSpan<blender::float3> vertexCos,
                        const float *smooth_weights,
                        uint iterations)
{
  switch (csmd->smooth_type) {
    case MOD_CORRECTIVESMOOTH_SMOOTH_LENGTH_WEIGHT:
      smooth_iter__length_weight(csmd, mesh, vertexCos, smooth_weights, iterations);
      break;

    /* case MOD_CORRECTIVESMOOTH_SMOOTH_SIMPLE: */
    default:
      smooth_iter__simple(csmd, mesh, vertexCos, smooth_weights, iterations);
      break;
  }
}

static void smooth_verts(CorrectiveSmoothModifierData *csmd,
                         Mesh *mesh,
                         const MDeformVert *dvert,
                         const int defgrp_index,
                         blender::MutableSpan<blender::float3> vertexCos)
{
  float *smooth_weights = nullptr;

  if (dvert || (csmd->flag & MOD_CORRECTIVESMOOTH_PIN_BOUNDARY)) {

    smooth_weights = MEM_malloc_arrayN<float>(size_t(vertexCos.size()), __func__);

    if (dvert) {
      mesh_get_weights(dvert,
                       defgrp_index,
                       uint(vertexCos.size()),
                       (csmd->flag & MOD_CORRECTIVESMOOTH_INVERT_VGROUP) != 0,
                       smooth_weights);
    }
    else {
      copy_vn_fl(smooth_weights, int(vertexCos.size()), 1.0f);
    }

    if (csmd->flag & MOD_CORRECTIVESMOOTH_PIN_BOUNDARY) {
      mesh_get_boundaries(mesh, smooth_weights);
    }
  }

  smooth_iter(csmd, mesh, vertexCos, smooth_weights, uint(csmd->repeat));

  if (smooth_weights) {
    MEM_freeN(smooth_weights);
  }
}

/**
 * Calculate an orthogonal 3x3 matrix from 2 edge vectors.
 * \return false if this loop should be ignored (have zero influence).
 */
static bool calc_tangent_loop(const float v_dir_prev[3],
                              const float v_dir_next[3],
                              float r_tspace[3][3])
{
  if (UNLIKELY(compare_v3v3(v_dir_prev, v_dir_next, FLT_EPSILON * 10.0f))) {
    /* As there are no weights, the value doesn't matter just initialize it. */
    unit_m3(r_tspace);
    return false;
  }

  copy_v3_v3(r_tspace[0], v_dir_prev);
  copy_v3_v3(r_tspace[1], v_dir_next);

  cross_v3_v3v3(r_tspace[2], v_dir_prev, v_dir_next);
  normalize_v3(r_tspace[2]);

  /* Make orthogonal using `r_tspace[2]` as a basis.
   *
   * NOTE: while it seems more logical to use `v_dir_prev` & `v_dir_next` as separate X/Y axis
   * (instead of combining them as is done here). It's not necessary as the directions of the
   * axis aren't important as long as the difference between tangent matrices is equivalent.
   * Some computations can be skipped by combining the two directions,
   * using the cross product for the 3rd axes. */
  add_v3_v3(r_tspace[0], r_tspace[1]);
  normalize_v3(r_tspace[0]);
  cross_v3_v3v3(r_tspace[1], r_tspace[2], r_tspace[0]);

  return true;
}

/**
 * \param r_tangent_spaces: Loop aligned array of tangents.
 * \param r_tangent_weights: Loop aligned array of weights (may be nullptr).
 * \param r_tangent_weights_per_vertex: Vertex aligned array, accumulating weights for each loop
 * (may be nullptr).
 */
static void calc_tangent_spaces(const Mesh *mesh,
                                blender::Span<blender::float3> vertexCos,
                                float (*r_tangent_spaces)[3][3],
                                float *r_tangent_weights,
                                float *r_tangent_weights_per_vertex)
{
  const uint mvert_num = uint(mesh->verts_num);
  const blender::OffsetIndices faces = mesh->faces();
  blender::Span<int> corner_verts = mesh->corner_verts();

  if (r_tangent_weights_per_vertex != nullptr) {
    copy_vn_fl(r_tangent_weights_per_vertex, int(mvert_num), 0.0f);
  }

  for (const int64_t i : faces.index_range()) {
    const blender::IndexRange face = faces[i];
    int next_corner = int(face.start());
    int term_corner = next_corner + int(face.size());
    int prev_corner = term_corner - 2;
    int curr_corner = term_corner - 1;

    /* loop directions */
    float v_dir_prev[3], v_dir_next[3];

    /* needed entering the loop */
    sub_v3_v3v3(
        v_dir_prev, vertexCos[corner_verts[prev_corner]], vertexCos[corner_verts[curr_corner]]);
    normalize_v3(v_dir_prev);

    for (; next_corner != term_corner;
         prev_corner = curr_corner, curr_corner = next_corner, next_corner++)
    {
      float (*ts)[3] = r_tangent_spaces[curr_corner];

      /* re-use the previous value */
#if 0
      sub_v3_v3v3(
          v_dir_prev, vertexCos[corner_verts[prev_corner]], vertexCos[corner_verts[curr_corner]]);
      normalize_v3(v_dir_prev);
#endif
      sub_v3_v3v3(
          v_dir_next, vertexCos[corner_verts[curr_corner]], vertexCos[corner_verts[next_corner]]);
      normalize_v3(v_dir_next);

      if (calc_tangent_loop(v_dir_prev, v_dir_next, ts)) {
        if (r_tangent_weights != nullptr) {
          const float weight = fabsf(
              blender::math::safe_acos_approx(dot_v3v3(v_dir_next, v_dir_prev)));
          r_tangent_weights[curr_corner] = weight;
          r_tangent_weights_per_vertex[corner_verts[curr_corner]] += weight;
        }
      }
      else {
        if (r_tangent_weights != nullptr) {
          r_tangent_weights[curr_corner] = 0;
        }
      }

      copy_v3_v3(v_dir_prev, v_dir_next);
    }
  }
}

static void store_cache_settings(CorrectiveSmoothModifierData *csmd)
{
  csmd->delta_cache.lambda = csmd->lambda;
  csmd->delta_cache.repeat = csmd->repeat;
  csmd->delta_cache.flag = csmd->flag;
  csmd->delta_cache.smooth_type = csmd->smooth_type;
  csmd->delta_cache.rest_source = csmd->rest_source;
}

static bool cache_settings_equal(CorrectiveSmoothModifierData *csmd)
{
  return (csmd->delta_cache.lambda == csmd->lambda && csmd->delta_cache.repeat == csmd->repeat &&
          csmd->delta_cache.flag == csmd->flag &&
          csmd->delta_cache.smooth_type == csmd->smooth_type &&
          csmd->delta_cache.rest_source == csmd->rest_source);
}

/**
 * This calculates #CorrectiveSmoothModifierData.delta_cache
 * It's not run on every update (during animation for example).
 */
static void calc_deltas(CorrectiveSmoothModifierData *csmd,
                        Mesh *mesh,
                        const MDeformVert *dvert,
                        const int defgrp_index,
                        const blender::Span<blender::float3> rest_coords)
{
  const blender::Span<int> corner_verts = mesh->corner_verts();

  blender::Array<blender::float3> smooth_vertex_coords(rest_coords);

  uint l_index;

  float (*tangent_spaces)[3][3] = MEM_malloc_arrayN<float[3][3]>(size_t(corner_verts.size()),
                                                                 __func__);

  if (csmd->delta_cache.deltas_num != uint(corner_verts.size())) {
    MEM_SAFE_FREE(csmd->delta_cache.deltas);
  }

  /* allocate deltas if they have not yet been allocated, otherwise we will just write over them */
  if (!csmd->delta_cache.deltas) {
    csmd->delta_cache.deltas_num = uint(corner_verts.size());
    csmd->delta_cache.deltas = MEM_malloc_arrayN<float[3]>(size_t(corner_verts.size()), __func__);
  }

  smooth_verts(csmd, mesh, dvert, defgrp_index, smooth_vertex_coords);

  calc_tangent_spaces(mesh, smooth_vertex_coords, tangent_spaces, nullptr, nullptr);

  copy_vn_fl(&csmd->delta_cache.deltas[0][0], int(corner_verts.size()) * 3, 0.0f);

  for (l_index = 0; l_index < corner_verts.size(); l_index++) {
    const int v_index = corner_verts[l_index];
    float delta[3];
    sub_v3_v3v3(delta, rest_coords[v_index], smooth_vertex_coords[v_index]);

    float imat[3][3];
    if (UNLIKELY(!invert_m3_m3(imat, tangent_spaces[l_index]))) {
      transpose_m3_m3(imat, tangent_spaces[l_index]);
    }
    mul_v3_m3v3(csmd->delta_cache.deltas[l_index], imat, delta);
  }

  MEM_SAFE_FREE(tangent_spaces);
}

static void correctivesmooth_modifier_do(ModifierData *md,
                                         Depsgraph *depsgraph,
                                         Object *ob,
                                         Mesh *mesh,
                                         blender::MutableSpan<blender::float3> vertexCos,
                                         BMEditMesh *em)
{
  using namespace blender;
  CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;

  const bool force_delta_cache_update =
      /* XXX, take care! if mesh data itself changes we need to forcefully recalculate deltas */
      !cache_settings_equal(csmd) ||
      ((csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_ORCO) &&
       (((ID *)ob->data)->recalc & ID_RECALC_ALL));

  blender::Span<int> corner_verts = mesh->corner_verts();

  bool use_only_smooth = (csmd->flag & MOD_CORRECTIVESMOOTH_ONLY_SMOOTH) != 0;
  const MDeformVert *dvert = nullptr;
  int defgrp_index;

  MOD_get_vgroup(ob, mesh, csmd->defgrp_name, &dvert, &defgrp_index);

  /* if rest bind_coords not are defined, set them (only run during bind) */
  if ((csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) &&
      /* signal to recalculate, whoever sets MUST also free bind coords */
      (csmd->bind_coords_num == uint(-1)))
  {
    if (DEG_is_active(depsgraph)) {
      BLI_assert(csmd->bind_coords == nullptr);
      csmd->bind_coords = MEM_malloc_arrayN<float[3]>(size_t(vertexCos.size()), __func__);
      csmd->bind_coords_sharing_info = implicit_sharing::info_for_mem_free(csmd->bind_coords);
      memcpy(csmd->bind_coords, vertexCos.data(), size_t(vertexCos.size_in_bytes()));
      csmd->bind_coords_num = uint(vertexCos.size());
      BLI_assert(csmd->bind_coords != nullptr);

      /* Copy bound data to the original modifier. */
      CorrectiveSmoothModifierData *csmd_orig = (CorrectiveSmoothModifierData *)
          BKE_modifier_get_original(ob, &csmd->modifier);
      implicit_sharing::copy_shared_pointer(csmd->bind_coords,
                                            csmd->bind_coords_sharing_info,
                                            &csmd_orig->bind_coords,
                                            &csmd_orig->bind_coords_sharing_info);

      csmd_orig->bind_coords_num = csmd->bind_coords_num;
    }
    else {
      BKE_modifier_set_error(ob, md, "Attempt to bind from inactive dependency graph");
    }
  }

  if (UNLIKELY(use_only_smooth)) {
    smooth_verts(csmd, mesh, dvert, defgrp_index, vertexCos);
    return;
  }

  if ((csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) &&
      (csmd->bind_coords == nullptr))
  {
    BKE_modifier_set_error(ob, md, "Bind data required");
    goto error;
  }

  /* If the number of verts has changed, the bind is invalid, so we do nothing */
  if (csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) {
    if (csmd->bind_coords_num != vertexCos.size()) {
      BKE_modifier_set_error(ob,
                             md,
                             "Bind vertex count mismatch: %u to %u",
                             csmd->bind_coords_num,
                             uint(vertexCos.size()));
      goto error;
    }
  }
  else {
    /* MOD_CORRECTIVESMOOTH_RESTSOURCE_ORCO */
    if (ob->type != OB_MESH) {
      BKE_modifier_set_error(ob, md, "Object is not a mesh");
      goto error;
    }
    else {
      const int me_numVerts = (em) ? em->bm->totvert : ((Mesh *)ob->data)->verts_num;

      if (me_numVerts != vertexCos.size()) {
        BKE_modifier_set_error(ob,
                               md,
                               "Original vertex count mismatch: %u to %u",
                               uint(me_numVerts),
                               uint(vertexCos.size()));
        goto error;
      }
    }
  }

  /* check to see if our deltas are still valid */
  if (!csmd->delta_cache.deltas || (csmd->delta_cache.deltas_num != corner_verts.size()) ||
      force_delta_cache_update)
  {
    blender::Array<blender::float3> rest_coords_alloc;
    blender::Span<blender::float3> rest_coords;

    store_cache_settings(csmd);

    if (csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) {
      /* caller needs to do sanity check here */
      csmd->bind_coords_num = uint(vertexCos.size());
      rest_coords = {reinterpret_cast<const blender::float3 *>(csmd->bind_coords),
                     csmd->bind_coords_num};
    }
    else {
      if (em) {
        rest_coords_alloc = BKE_editmesh_vert_coords_alloc_orco(em);
        rest_coords = rest_coords_alloc;
      }
      else {
        const Mesh *object_mesh = static_cast<const Mesh *>(ob->data);
        rest_coords = object_mesh->vert_positions();
      }
    }

#ifdef DEBUG_TIME
    TIMEIT_START(corrective_smooth_deltas);
#endif

    calc_deltas(csmd, mesh, dvert, defgrp_index, rest_coords);

#ifdef DEBUG_TIME
    TIMEIT_END(corrective_smooth_deltas);
#endif
  }

  if (csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) {
    /* this could be a check, but at this point it _must_ be valid */
    BLI_assert(csmd->bind_coords_num == vertexCos.size() && csmd->delta_cache.deltas);
  }

#ifdef DEBUG_TIME
  TIMEIT_START(corrective_smooth);
#endif

  /* do the actual delta mush */
  smooth_verts(csmd, mesh, dvert, defgrp_index, vertexCos);

  {

    const float scale = csmd->scale;

    float (*tangent_spaces)[3][3] = MEM_malloc_arrayN<float[3][3]>(size_t(corner_verts.size()),
                                                                   __func__);
    float *tangent_weights = MEM_malloc_arrayN<float>(size_t(corner_verts.size()), __func__);
    float *tangent_weights_per_vertex = MEM_malloc_arrayN<float>(size_t(vertexCos.size()),
                                                                 __func__);

    calc_tangent_spaces(
        mesh, vertexCos, tangent_spaces, tangent_weights, tangent_weights_per_vertex);

    for (const int64_t l_index : corner_verts.index_range()) {
      const int v_index = corner_verts[l_index];
      const float weight = tangent_weights[l_index] / tangent_weights_per_vertex[v_index];
      if (UNLIKELY(!(weight > 0.0f))) {
        /* Catches zero & divide by zero. */
        continue;
      }

      float delta[3];
      mul_v3_m3v3(delta, tangent_spaces[l_index], csmd->delta_cache.deltas[l_index]);
      mul_v3_fl(delta, weight);
      madd_v3_v3fl(vertexCos[v_index], delta, scale);
    }

    MEM_freeN(tangent_spaces);
    MEM_freeN(tangent_weights);
    MEM_freeN(tangent_weights_per_vertex);
  }

#ifdef DEBUG_TIME
  TIMEIT_END(corrective_smooth);
#endif

  return;

  /* when the modifier fails to execute */
error:
  MEM_SAFE_FREE(csmd->delta_cache.deltas);
  csmd->delta_cache.deltas_num = 0;
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  correctivesmooth_modifier_do(md, ctx->depsgraph, ctx->object, mesh, positions, nullptr);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "factor", UI_ITEM_NONE, IFACE_("Factor"), ICON_NONE);
  layout->prop(ptr, "iterations", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "smooth_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  layout->prop(ptr, "use_only_smooth", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "use_pin_boundary", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->prop(ptr, "rest_source", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (RNA_enum_get(ptr, "rest_source") == MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) {
    layout->op("OBJECT_OT_correctivesmooth_bind",
               (RNA_boolean_get(ptr, "is_bind") ? IFACE_("Unbind") : IFACE_("Bind")),
               ICON_NONE);
  }

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_CorrectiveSmooth, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID *id_owner, const ModifierData *md)
{
  CorrectiveSmoothModifierData csmd = *(const CorrectiveSmoothModifierData *)md;
  const bool is_undo = BLO_write_is_undo(writer);

  if (ID_IS_OVERRIDE_LIBRARY(id_owner) && !is_undo) {
    BLI_assert(!ID_IS_LINKED(id_owner));
    const bool is_local = (md->flag & eModifierFlag_OverrideLibrary_Local) != 0;
    if (!is_local) {
      /* Modifier coming from linked data cannot be bound from an override, so we can remove all
       * binding data, can save a significant amount of memory. */
      csmd.bind_coords_num = 0;
      csmd.bind_coords = nullptr;
      csmd.bind_coords_sharing_info = nullptr;
    }
  }

  if (csmd.bind_coords != nullptr) {
    BLO_write_shared(writer,
                     csmd.bind_coords,
                     sizeof(float[3]) * csmd.bind_coords_num,
                     csmd.bind_coords_sharing_info,
                     [&]() {
                       BLO_write_float3_array(
                           writer, csmd.bind_coords_num, (const float *)csmd.bind_coords);
                     });
  }

  BLO_write_struct_at_address(writer, CorrectiveSmoothModifierData, md, &csmd);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;

  if (csmd->bind_coords) {
    csmd->bind_coords_sharing_info = BLO_read_shared(reader, &csmd->bind_coords, [&]() {
      BLO_read_float3_array(reader, int(csmd->bind_coords_num), (float **)&csmd->bind_coords);
      return blender::implicit_sharing::info_for_mem_free(csmd->bind_coords);
    });
  }

  /* runtime only */
  csmd->delta_cache.deltas = nullptr;
  csmd->delta_cache.deltas_num = 0;
}

ModifierTypeInfo modifierType_CorrectiveSmooth = {
    /*idname*/ "CorrectiveSmooth",
    /*name*/ N_("CorrectiveSmooth"),
    /*struct_name*/ "CorrectiveSmoothModifierData",
    /*struct_size*/ sizeof(CorrectiveSmoothModifierData),
    /*srna*/ &RNA_CorrectiveSmoothModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_SMOOTH,

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
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ blend_write,
    /*blend_read*/ blend_read,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
