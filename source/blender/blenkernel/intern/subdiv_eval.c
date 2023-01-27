/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv_eval.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_bitmap.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_subdiv.h"

#include "MEM_guardedalloc.h"

#include "opensubdiv_evaluator_capi.h"
#include "opensubdiv_topology_refiner_capi.h"

/* --------------------------------------------------------------------
 * Helper functions.
 */

static eOpenSubdivEvaluator opensubdiv_evalutor_from_subdiv_evaluator_type(
    eSubdivEvaluatorType evaluator_type)
{
  switch (evaluator_type) {
    case SUBDIV_EVALUATOR_TYPE_CPU: {
      return OPENSUBDIV_EVALUATOR_CPU;
    }
    case SUBDIV_EVALUATOR_TYPE_GPU: {
      return OPENSUBDIV_EVALUATOR_GPU;
    }
  }
  BLI_assert_msg(0, "Unknown evaluator type");
  return OPENSUBDIV_EVALUATOR_CPU;
}

/* --------------------------------------------------------------------
 * Main subdivision evaluation.
 */

bool BKE_subdiv_eval_begin(Subdiv *subdiv,
                           eSubdivEvaluatorType evaluator_type,
                           OpenSubdiv_EvaluatorCache *evaluator_cache,
                           const OpenSubdiv_EvaluatorSettings *settings)
{
  BKE_subdiv_stats_reset(&subdiv->stats, SUBDIV_STATS_EVALUATOR_CREATE);
  if (subdiv->topology_refiner == NULL) {
    /* Happens on input mesh with just loose geometry,
     * or when OpenSubdiv is disabled */
    return false;
  }
  if (subdiv->evaluator == NULL) {
    eOpenSubdivEvaluator opensubdiv_evaluator_type =
        opensubdiv_evalutor_from_subdiv_evaluator_type(evaluator_type);
    BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_EVALUATOR_CREATE);
    subdiv->evaluator = openSubdiv_createEvaluatorFromTopologyRefiner(
        subdiv->topology_refiner, opensubdiv_evaluator_type, evaluator_cache);
    BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_EVALUATOR_CREATE);
    if (subdiv->evaluator == NULL) {
      return false;
    }
  }
  else {
    /* TODO(sergey): Check for topology change. */
  }
  subdiv->evaluator->setSettings(subdiv->evaluator, settings);
  BKE_subdiv_eval_init_displacement(subdiv);
  return true;
}

static void set_coarse_positions(Subdiv *subdiv,
                                 const Mesh *mesh,
                                 const float (*coarse_vertex_cos)[3])
{
  const float(*positions)[3] = BKE_mesh_vert_positions(mesh);
  const MPoly *mpoly = BKE_mesh_polys(mesh);
  const MLoop *mloop = BKE_mesh_loops(mesh);
  /* Mark vertices which needs new coordinates. */
  /* TODO(sergey): This is annoying to calculate this on every update,
   * maybe it's better to cache this mapping. Or make it possible to have
   * OpenSubdiv's vertices match mesh ones? */
  BLI_bitmap *vertex_used_map = BLI_BITMAP_NEW(mesh->totvert, "vert used map");
  for (int poly_index = 0; poly_index < mesh->totpoly; poly_index++) {
    const MPoly *poly = &mpoly[poly_index];
    for (int corner = 0; corner < poly->totloop; corner++) {
      const MLoop *loop = &mloop[poly->loopstart + corner];
      BLI_BITMAP_ENABLE(vertex_used_map, loop->v);
    }
  }
  /* Use a temporary buffer so we do not upload vertices one at a time to the GPU. */
  float(*buffer)[3] = MEM_mallocN(sizeof(float[3]) * mesh->totvert, "subdiv tmp coarse positions");
  int manifold_vertex_count = 0;
  for (int vertex_index = 0, manifold_vertex_index = 0; vertex_index < mesh->totvert;
       vertex_index++) {
    if (!BLI_BITMAP_TEST_BOOL(vertex_used_map, vertex_index)) {
      continue;
    }
    const float *vertex_co;
    if (coarse_vertex_cos != NULL) {
      vertex_co = coarse_vertex_cos[vertex_index];
    }
    else {
      vertex_co = positions[vertex_index];
    }
    copy_v3_v3(&buffer[manifold_vertex_index][0], vertex_co);
    manifold_vertex_index++;
    manifold_vertex_count++;
  }
  subdiv->evaluator->setCoarsePositions(
      subdiv->evaluator, &buffer[0][0], 0, manifold_vertex_count);
  MEM_freeN(vertex_used_map);
  MEM_freeN(buffer);
}

/* Context which is used to fill face varying data in parallel. */
typedef struct FaceVaryingDataFromUVContext {
  OpenSubdiv_TopologyRefiner *topology_refiner;
  const Mesh *mesh;
  const MPoly *polys;
  const float (*mloopuv)[2];
  float (*buffer)[2];
  int layer_index;
} FaceVaryingDataFromUVContext;

static void set_face_varying_data_from_uv_task(void *__restrict userdata,
                                               const int face_index,
                                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  FaceVaryingDataFromUVContext *ctx = userdata;
  OpenSubdiv_TopologyRefiner *topology_refiner = ctx->topology_refiner;
  const int layer_index = ctx->layer_index;
  const MPoly *mpoly = &ctx->polys[face_index];
  const float(*mluv)[2] = &ctx->mloopuv[mpoly->loopstart];

  /* TODO(sergey): OpenSubdiv's C-API converter can change winding of
   * loops of a face, need to watch for that, to prevent wrong UVs assigned.
   */
  const int num_face_vertices = topology_refiner->getNumFaceVertices(topology_refiner, face_index);
  const int *uv_indices = topology_refiner->getFaceFVarValueIndices(
      topology_refiner, face_index, layer_index);
  for (int vertex_index = 0; vertex_index < num_face_vertices; vertex_index++, mluv++) {
    copy_v2_v2(ctx->buffer[uv_indices[vertex_index]], *mluv);
  }
}

static void set_face_varying_data_from_uv(Subdiv *subdiv,
                                          const Mesh *mesh,
                                          const float (*mloopuv)[2],
                                          const int layer_index)
{
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;
  const int num_faces = topology_refiner->getNumFaces(topology_refiner);
  const float(*mluv)[2] = mloopuv;

  const int num_fvar_values = topology_refiner->getNumFVarValues(topology_refiner, layer_index);
  /* Use a temporary buffer so we do not upload UVs one at a time to the GPU. */
  float(*buffer)[2] = MEM_mallocN(sizeof(float[2]) * num_fvar_values, "temp UV storage");

  FaceVaryingDataFromUVContext ctx;
  ctx.topology_refiner = topology_refiner;
  ctx.layer_index = layer_index;
  ctx.mloopuv = mluv;
  ctx.mesh = mesh;
  ctx.polys = BKE_mesh_polys(mesh);
  ctx.buffer = buffer;

  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  parallel_range_settings.min_iter_per_thread = 1;

  BLI_task_parallel_range(
      0, num_faces, &ctx, set_face_varying_data_from_uv_task, &parallel_range_settings);

  evaluator->setFaceVaryingData(evaluator, layer_index, &buffer[0][0], 0, num_fvar_values);

  MEM_freeN(buffer);
}

static void set_vertex_data_from_orco(Subdiv *subdiv, const Mesh *mesh)
{
  const float(*orco)[3] = CustomData_get_layer(&mesh->vdata, CD_ORCO);
  const float(*cloth_orco)[3] = CustomData_get_layer(&mesh->vdata, CD_CLOTH_ORCO);

  if (orco || cloth_orco) {
    OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
    OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;
    const int num_verts = topology_refiner->getNumVertices(topology_refiner);

    if (orco && cloth_orco) {
      /* Set one by one if have both. */
      for (int i = 0; i < num_verts; i++) {
        float data[6];
        copy_v3_v3(data, orco[i]);
        copy_v3_v3(data + 3, cloth_orco[i]);
        evaluator->setVertexData(evaluator, data, i, 1);
      }
    }
    else {
      /* Faster single call if we have either. */
      if (orco) {
        evaluator->setVertexData(evaluator, orco[0], 0, num_verts);
      }
      else if (cloth_orco) {
        evaluator->setVertexData(evaluator, cloth_orco[0], 0, num_verts);
      }
    }
  }
}

static void get_mesh_evaluator_settings(OpenSubdiv_EvaluatorSettings *settings, const Mesh *mesh)
{
  settings->num_vertex_data = (CustomData_has_layer(&mesh->vdata, CD_ORCO) ? 3 : 0) +
                              (CustomData_has_layer(&mesh->vdata, CD_CLOTH_ORCO) ? 3 : 0);
}

bool BKE_subdiv_eval_begin_from_mesh(Subdiv *subdiv,
                                     const Mesh *mesh,
                                     const float (*coarse_vertex_cos)[3],
                                     eSubdivEvaluatorType evaluator_type,
                                     OpenSubdiv_EvaluatorCache *evaluator_cache)
{
  OpenSubdiv_EvaluatorSettings settings = {0};
  get_mesh_evaluator_settings(&settings, mesh);
  if (!BKE_subdiv_eval_begin(subdiv, evaluator_type, evaluator_cache, &settings)) {
    return false;
  }
  return BKE_subdiv_eval_refine_from_mesh(subdiv, mesh, coarse_vertex_cos);
}

bool BKE_subdiv_eval_refine_from_mesh(Subdiv *subdiv,
                                      const Mesh *mesh,
                                      const float (*coarse_vertex_cos)[3])
{
  if (subdiv->evaluator == NULL) {
    /* NOTE: This situation is supposed to be handled by begin(). */
    BLI_assert_msg(0, "Is not supposed to happen");
    return false;
  }
  /* Set coordinates of base mesh vertices. */
  set_coarse_positions(subdiv, mesh, coarse_vertex_cos);
  /* Set face-varying data to UV maps. */
  const int num_uv_layers = CustomData_number_of_layers(&mesh->ldata, CD_PROP_FLOAT2);
  for (int layer_index = 0; layer_index < num_uv_layers; layer_index++) {
    const float(*mloopuv)[2] = CustomData_get_layer_n(&mesh->ldata, CD_PROP_FLOAT2, layer_index);
    set_face_varying_data_from_uv(subdiv, mesh, mloopuv, layer_index);
  }
  /* Set vertex data to orco. */
  set_vertex_data_from_orco(subdiv, mesh);
  /* Update evaluator to the new coarse geometry. */
  BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_EVALUATOR_REFINE);
  subdiv->evaluator->refine(subdiv->evaluator);
  BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_EVALUATOR_REFINE);
  return true;
}

void BKE_subdiv_eval_init_displacement(Subdiv *subdiv)
{
  if (subdiv->displacement_evaluator == NULL) {
    return;
  }
  if (subdiv->displacement_evaluator->initialize == NULL) {
    return;
  }
  subdiv->displacement_evaluator->initialize(subdiv->displacement_evaluator);
}

/* --------------------------------------------------------------------
 * Single point queries.
 */

void BKE_subdiv_eval_limit_point(
    Subdiv *subdiv, const int ptex_face_index, const float u, const float v, float r_P[3])
{
  BKE_subdiv_eval_limit_point_and_derivatives(subdiv, ptex_face_index, u, v, r_P, NULL, NULL);
}

void BKE_subdiv_eval_limit_point_and_derivatives(Subdiv *subdiv,
                                                 const int ptex_face_index,
                                                 const float u,
                                                 const float v,
                                                 float r_P[3],
                                                 float r_dPdu[3],
                                                 float r_dPdv[3])
{
  subdiv->evaluator->evaluateLimit(subdiv->evaluator, ptex_face_index, u, v, r_P, r_dPdu, r_dPdv);

  /* NOTE: In a very rare occasions derivatives are evaluated to zeros or are exactly equal.
   * This happens, for example, in single vertex on Suzannne's nose (where two quads have 2 common
   * edges).
   *
   * This makes tangent space displacement (such as multi-resolution) impossible to be used in
   * those vertices, so those needs to be addressed in one way or another.
   *
   * Simplest thing to do: step inside of the face a little bit, where there is known patch at
   * which there must be proper derivatives. This might break continuity of normals, but is better
   * that giving totally unusable derivatives. */

  if (r_dPdu != NULL && r_dPdv != NULL) {
    if ((is_zero_v3(r_dPdu) || is_zero_v3(r_dPdv)) || equals_v3v3(r_dPdu, r_dPdv)) {
      subdiv->evaluator->evaluateLimit(subdiv->evaluator,
                                       ptex_face_index,
                                       u * 0.999f + 0.0005f,
                                       v * 0.999f + 0.0005f,
                                       r_P,
                                       r_dPdu,
                                       r_dPdv);
    }
  }
}

void BKE_subdiv_eval_limit_point_and_normal(Subdiv *subdiv,
                                            const int ptex_face_index,
                                            const float u,
                                            const float v,
                                            float r_P[3],
                                            float r_N[3])
{
  float dPdu[3], dPdv[3];
  BKE_subdiv_eval_limit_point_and_derivatives(subdiv, ptex_face_index, u, v, r_P, dPdu, dPdv);
  cross_v3_v3v3(r_N, dPdu, dPdv);
  normalize_v3(r_N);
}

void BKE_subdiv_eval_vertex_data(
    Subdiv *subdiv, const int ptex_face_index, const float u, const float v, float r_vertex_data[])
{
  subdiv->evaluator->evaluateVertexData(subdiv->evaluator, ptex_face_index, u, v, r_vertex_data);
}

void BKE_subdiv_eval_face_varying(Subdiv *subdiv,
                                  const int face_varying_channel,
                                  const int ptex_face_index,
                                  const float u,
                                  const float v,
                                  float r_face_varying[2])
{
  subdiv->evaluator->evaluateFaceVarying(
      subdiv->evaluator, face_varying_channel, ptex_face_index, u, v, r_face_varying);
}

void BKE_subdiv_eval_displacement(Subdiv *subdiv,
                                  const int ptex_face_index,
                                  const float u,
                                  const float v,
                                  const float dPdu[3],
                                  const float dPdv[3],
                                  float r_D[3])
{
  if (subdiv->displacement_evaluator == NULL) {
    zero_v3(r_D);
    return;
  }
  subdiv->displacement_evaluator->eval_displacement(
      subdiv->displacement_evaluator, ptex_face_index, u, v, dPdu, dPdv, r_D);
}

void BKE_subdiv_eval_final_point(
    Subdiv *subdiv, const int ptex_face_index, const float u, const float v, float r_P[3])
{
  if (subdiv->displacement_evaluator) {
    float dPdu[3], dPdv[3], D[3];
    BKE_subdiv_eval_limit_point_and_derivatives(subdiv, ptex_face_index, u, v, r_P, dPdu, dPdv);
    BKE_subdiv_eval_displacement(subdiv, ptex_face_index, u, v, dPdu, dPdv, D);
    add_v3_v3(r_P, D);
  }
  else {
    BKE_subdiv_eval_limit_point(subdiv, ptex_face_index, u, v, r_P);
  }
}
