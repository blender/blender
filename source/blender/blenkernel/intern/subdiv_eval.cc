/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv_eval.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math_vector.h"
#include "BLI_task.h"
#include "BLI_timeit.hh"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_mesh.hh"
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
  if (subdiv->topology_refiner == nullptr) {
    /* Happens on input mesh with just loose geometry,
     * or when OpenSubdiv is disabled */
    return false;
  }
  if (subdiv->evaluator == nullptr) {
    eOpenSubdivEvaluator opensubdiv_evaluator_type =
        opensubdiv_evalutor_from_subdiv_evaluator_type(evaluator_type);
    BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_EVALUATOR_CREATE);
    subdiv->evaluator = openSubdiv_createEvaluatorFromTopologyRefiner(
        subdiv->topology_refiner, opensubdiv_evaluator_type, evaluator_cache);
    BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_EVALUATOR_CREATE);
    if (subdiv->evaluator == nullptr) {
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
                                 const blender::Span<blender::float3> positions,
                                 const blender::bke::LooseVertCache &verts_no_face)
{
  using namespace blender;
  OpenSubdiv_Evaluator *evaluator = subdiv->evaluator;
  if (verts_no_face.count == 0) {
    evaluator->setCoarsePositions(
        evaluator, reinterpret_cast<const float *>(positions.data()), 0, positions.size());
    return;
  }
  Array<float3> used_vert_positions(positions.size() - verts_no_face.count);
  const BitSpan bits = verts_no_face.is_loose_bits;
  int used_vert_count = 0;
  for (const int vert : positions.index_range()) {
    if (bits[vert]) {
      continue;
    }
    used_vert_positions[used_vert_count] = positions[vert];
    used_vert_count++;
  }
  evaluator->setCoarsePositions(evaluator,
                                reinterpret_cast<const float *>(used_vert_positions.data()),
                                0,
                                used_vert_positions.size());
}

/* Context which is used to fill face varying data in parallel. */
struct FaceVaryingDataFromUVContext {
  OpenSubdiv_TopologyRefiner *topology_refiner;
  const Mesh *mesh;
  blender::OffsetIndices<int> polys;
  const float (*mloopuv)[2];
  float (*buffer)[2];
  int layer_index;
};

static void set_face_varying_data_from_uv_task(void *__restrict userdata,
                                               const int face_index,
                                               const TaskParallelTLS *__restrict /*tls*/)
{
  FaceVaryingDataFromUVContext *ctx = static_cast<FaceVaryingDataFromUVContext *>(userdata);
  OpenSubdiv_TopologyRefiner *topology_refiner = ctx->topology_refiner;
  const int layer_index = ctx->layer_index;
  const float(*mluv)[2] = &ctx->mloopuv[ctx->polys[face_index].start()];

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
  float(*buffer)[2] = static_cast<float(*)[2]>(
      MEM_mallocN(sizeof(float[2]) * num_fvar_values, __func__));

  FaceVaryingDataFromUVContext ctx;
  ctx.topology_refiner = topology_refiner;
  ctx.layer_index = layer_index;
  ctx.mloopuv = mluv;
  ctx.mesh = mesh;
  ctx.polys = mesh->polys();
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
  const float(*orco)[3] = static_cast<const float(*)[3]>(
      CustomData_get_layer(&mesh->vdata, CD_ORCO));
  const float(*cloth_orco)[3] = static_cast<const float(*)[3]>(
      CustomData_get_layer(&mesh->vdata, CD_CLOTH_ORCO));

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
  using namespace blender;
  if (subdiv->evaluator == nullptr) {
    /* NOTE: This situation is supposed to be handled by begin(). */
    BLI_assert_msg(0, "Is not supposed to happen");
    return false;
  }
  /* Set coordinates of base mesh vertices. */
  set_coarse_positions(
      subdiv,
      coarse_vertex_cos ?
          Span(reinterpret_cast<const float3 *>(coarse_vertex_cos), mesh->totvert) :
          mesh->vert_positions(),
      mesh->verts_no_face());

  /* Set face-varying data to UV maps. */
  const int num_uv_layers = CustomData_number_of_layers(&mesh->ldata, CD_PROP_FLOAT2);
  for (int layer_index = 0; layer_index < num_uv_layers; layer_index++) {
    const float(*mloopuv)[2] = static_cast<const float(*)[2]>(
        CustomData_get_layer_n(&mesh->ldata, CD_PROP_FLOAT2, layer_index));
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
  if (subdiv->displacement_evaluator == nullptr) {
    return;
  }
  if (subdiv->displacement_evaluator->initialize == nullptr) {
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
  BKE_subdiv_eval_limit_point_and_derivatives(
      subdiv, ptex_face_index, u, v, r_P, nullptr, nullptr);
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

  if (r_dPdu != nullptr && r_dPdv != nullptr) {
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
  if (subdiv->displacement_evaluator == nullptr) {
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
