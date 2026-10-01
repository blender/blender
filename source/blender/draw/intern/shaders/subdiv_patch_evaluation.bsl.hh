/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "gpu_shader_compat.hh"

#include "draw_subdiv_shader_shared.hh"

/* `osd_patch_defines.glsl` must be included before `osd_patch_basis.glsl` */
#include "osd_patch_defines.glsl" /* IWYU pragma: export */

#include "osd_patch_basis.glsl"

#include "draw_subdiv_defines.hh"
#include "subdiv_common.bsl.hh"

namespace subdiv {

/* ------------------------------------------------------------------------------
 * Patch Coordinate lookup. Return an #OsdPatchCoord for the given patch_index and UVs.
 * This code is a port of the #OpenSubdiv PatchMap lookup code.
 */

PatchHandle bogus_patch_handle()
{
  PatchHandle ret;
  ret.array_index = -1;
  ret.vertex_index = -1;
  ret.patch_index = -1;
  return ret;
}

int transformUVToQuadQuadrant(float median, float &u, float &v)
{
  int uHalf = (u >= median) ? 1 : 0;
  if (uHalf != 0) {
    u -= median;
  }
  int vHalf = (v >= median) ? 1 : 0;
  if (vHalf != 0) {
    v -= median;
  }
  return (vHalf << 1) | uHalf;
}

int transformUVToTriQuadrant(float median, float &u, float &v, bool &rotated)
{

  if (!rotated) {
    if (u >= median) {
      u -= median;
      return 1;
    }
    if (v >= median) {
      v -= median;
      return 2;
    }
    if ((u + v) >= median) {
      rotated = true;
      return 3;
    }
    return 0;
  }
  else {
    if (u < median) {
      v -= median;
      return 1;
    }
    if (v < median) {
      u -= median;
      return 2;
    }
    u -= median;
    v -= median;
    if ((u + v) < median) {
      rotated = false;
      return 3;
    }
    return 0;
  }
}

OsdPatchCoord bogus_patch_coord(int face_index, float u, float v)
{
  OsdPatchCoord coord;
  coord.arrayIndex = 0;
  coord.patchIndex = face_index;
  coord.vertIndex = 0;
  coord.s = u;
  coord.t = v;
  return coord;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Patch evaluation
 * \{ */

struct PatchEvalConsts {
  [[compilation_constant]] bool do_orcos;
  /* WORKAROUND: Needed define for OSD. */
  [[compilation_constant]] bool use_1st_derivatives;
};

struct PatchEval {
  [[resource_table]] PatchEvalConsts consts;
  [[resource_table]] SubdivResources srt;

  [[storage(PATCH_EVALUATION_SOURCE_VERTEX_BUFFER_BUF_SLOT,
            read)]] const float (&srcVertexBuffer)[];

  [[storage(PATCH_EVALUATION_INPUT_PATCH_HANDLES_BUF_SLOT,
            read)]] const PatchHandle (&input_patch_handles)[];

  [[storage(PATCH_EVALUATION_QUAD_NODES_BUF_SLOT, read)]] const QuadNode (&quad_nodes)[];

  [[storage(PATCH_EVALUATION_PATCH_COORDS_BUF_SLOT,
            read)]] const BlenderPatchCoord (&patch_coords)[];
  [[storage(PATCH_EVALUATION_PATCH_ARRAY_BUFFER_BUF_SLOT,
            read)]] const OsdPatchArray (&patchArrayBuffer)[];
  [[storage(PATCH_EVALUATION_PATCH_INDEX_BUFFER_BUF_SLOT, read)]] const int (&patchIndexBuffer)[];
  [[storage(PATCH_EVALUATION_PATCH_PARAM_BUFFER_BUF_SLOT,
            read)]] const OsdPatchParam (&patchParamBuffer)[];

  [[storage(PATCH_EVALUATION_SOURCE_EXTRA_VERTEX_BUFFER_BUF_SLOT,
            read)]] [[condition(do_orcos)]] const float (&srcExtraVertexBuffer)[];

  float2 read_vec2(int index) const
  {
    float2 result;
    result.x = srcVertexBuffer[index * 2];
    result.y = srcVertexBuffer[index * 2 + 1];
    return result;
  }

  float3 read_vec3(int index) const
  {
    float3 result;
    result.x = srcVertexBuffer[index * 3];
    result.y = srcVertexBuffer[index * 3 + 1];
    result.z = srcVertexBuffer[index * 3 + 2];
    return result;
  }

  float3 read_vec3_extra(int index) const
  {
    float3 result;
    result.x = srcExtraVertexBuffer[index * 3];
    result.y = srcExtraVertexBuffer[index * 3 + 1];
    result.z = srcExtraVertexBuffer[index * 3 + 2];
    return result;
  }

  OsdPatchArray GetPatchArray(int arrayIndex) const
  {
    return patchArrayBuffer[arrayIndex];
  }

  OsdPatchParam GetPatchParam(int patchIndex) const
  {
    return patchParamBuffer[patchIndex];
  }

  PatchHandle find_patch(int face_index, float u, float v) const
  {
    if (face_index < srt.shader_data.min_patch_face || face_index > srt.shader_data.max_patch_face)
    {
      return bogus_patch_handle();
    }

    QuadNode node = quad_nodes[face_index - srt.shader_data.min_patch_face];

    if (!is_set(node.child[0])) {
      return bogus_patch_handle();
    }

    float median = 0.5f;
    bool tri_rotated = false;

    for (int depth = 0; depth <= srt.shader_data.max_depth; ++depth, median *= 0.5f) {
      int quadrant = srt.shader_data.patches_are_triangular ?
                         transformUVToTriQuadrant(median, u, v, tri_rotated) :
                         transformUVToQuadQuadrant(median, u, v);

      if (is_leaf(node.child[quadrant])) {
        return input_patch_handles[get_index(node.child[quadrant])];
      }

      node = quad_nodes[get_index(node.child[quadrant])];
    }
    return bogus_patch_handle();
  }

  OsdPatchCoord GetPatchCoord(int face_index, float u, float v) const
  {
    PatchHandle patch_handle = find_patch(face_index, u, v);

    if (patch_handle.array_index == -1) {
      return bogus_patch_coord(face_index, u, v);
    }

    OsdPatchCoord coord;
    coord.arrayIndex = patch_handle.array_index;
    coord.patchIndex = patch_handle.patch_index;
    coord.vertIndex = patch_handle.vertex_index;
    coord.s = u;
    coord.t = v;
    return coord;
  }

  /* ------------------------------------------------------------------------------
   * Patch evaluation. Note that the 1st and 2nd derivatives are always computed, although we
   * only return and use the 1st derivatives if adaptive patches are used. This could
   * perhaps be optimized.
   */

  void evaluate_patches_limits(int patch_index, float u, float v, float2 &dst) const
  {
    OsdPatchCoord coord = GetPatchCoord(patch_index, u, v);
    OsdPatchArray array = GetPatchArray(coord.arrayIndex);
    OsdPatchParam param = GetPatchParam(coord.patchIndex);

    int patchType = OsdPatchParamIsRegular(param) ? array.regDesc : array.desc;

    float wP[20], wDu[20], wDv[20], wDuu[20], wDuv[20], wDvv[20];
    int nPoints = OsdEvaluatePatchBasis(
        patchType, param, coord.s, coord.t, wP, wDu, wDv, wDuu, wDuv, wDvv);

    int indexBase = array.indexBase + array.stride * (coord.patchIndex - array.primitiveIdBase);

    for (int cv = 0; cv < nPoints; ++cv) {
      int index = patchIndexBuffer[indexBase + cv];
      float2 src_fvar = read_vec2(srt.shader_data.src_offset + index);
      dst += src_fvar * wP[cv];
    }
  }

  void evaluate_patches_limits(
      int patch_index, float u, float v, float3 &dst, float3 &du, float3 &dv) const
  {
    OsdPatchCoord coord = GetPatchCoord(patch_index, u, v);
    OsdPatchArray array = GetPatchArray(coord.arrayIndex);
    OsdPatchParam param = GetPatchParam(coord.patchIndex);

    int patchType = OsdPatchParamIsRegular(param) ? array.regDesc : array.desc;

    float wP[20], wDu[20], wDv[20], wDuu[20], wDuv[20], wDvv[20];
    int nPoints = OsdEvaluatePatchBasis(
        patchType, param, coord.s, coord.t, wP, wDu, wDv, wDuu, wDuv, wDvv);

    int indexBase = array.indexBase + array.stride * (coord.patchIndex - array.primitiveIdBase);

    for (int cv = 0; cv < nPoints; ++cv) {
      int index = patchIndexBuffer[indexBase + cv];
      float3 src_vertex = read_vec3(index);

      dst += src_vertex * wP[cv];
      du += src_vertex * wDu[cv];
      dv += src_vertex * wDv[cv];
    }
  }

  /* Evaluate the patches limits from the extra source vertex buffer. */
  void evaluate_patches_limits_extra(int patch_index, float u, float v, float3 &dst) const
  {
    if (consts.do_orcos) [[static_branch]] {
      OsdPatchCoord coord = GetPatchCoord(patch_index, u, v);
      OsdPatchArray array = GetPatchArray(coord.arrayIndex);
      OsdPatchParam param = GetPatchParam(coord.patchIndex);

      int patchType = OsdPatchParamIsRegular(param) ? array.regDesc : array.desc;

      float wP[20], wDu[20], wDv[20], wDuu[20], wDuv[20], wDvv[20];
      int nPoints = OsdEvaluatePatchBasis(
          patchType, param, coord.s, coord.t, wP, wDu, wDv, wDuu, wDuv, wDvv);

      int indexBase = array.indexBase + array.stride * (coord.patchIndex - array.primitiveIdBase);

      for (int cv = 0; cv < nPoints; ++cv) {
        int index = patchIndexBuffer[indexBase + cv];
        float3 src_vertex = read_vec3_extra(index);

        dst += src_vertex * wP[cv];
      }
    }
  }
};

/** \} */

/* ------------------------------------------------------------------------------
 * Face Varying (UVs).
 */

struct FvarEval {
  [[storage(PATCH_EVALUATION_OUTPUT_FVAR_BUF_SLOT, write)]] packed_float2 (&fvar_buf)[];
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void patch_eval_fvar([[resource_table]] const SubdivResources &srt,
                     [[resource_table]] const PatchEval &eval,
                     [[resource_table]] FvarEval &res,
                     [[global_invocation_id]] const uint3 global_id,
                     [[num_work_groups]] const uint3 num_work_groups)
{
  /* We execute for each quad. */
  uint quad_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (quad_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  for (uint loop_index = start_loop_index; loop_index < start_loop_index + 4; loop_index++) {
    float2 fvar = float2(0.0f);

    BlenderPatchCoord patch_co = eval.patch_coords[loop_index];
    float2 uv = decode_uv(patch_co.encoded_uv);

    eval.evaluate_patches_limits(patch_co.patch_index, uv.x, uv.y, fvar);
    res.fvar_buf[srt.shader_data.dst_offset + loop_index] = fvar;
  }
}

/** \} */

/* ------------------------------------------------------------------------------
 * Face Dots.
 */

struct FdotsConsts {
  [[compilation_constant]] bool do_normals;
};

struct FdotsEval {
  [[resource_table]] FdotsConsts consts;
  [[resource_table]] SubdivResources srt;

  [[storage(PATCH_EVALUATION_EXTRA_COARSE_FACE_DATA_BUF_SLOT,
            read)]] const uint (&extra_coarse_face_data)[];

  [[storage(PATCH_EVALUATION_OUTPUT_FDOTS_VERTEX_BUFFER_BUF_SLOT, write)]] FDotVert (&verts_buf)[];
  [[storage(PATCH_EVALUATION_OUTPUT_INDICES_BUF_SLOT, write)]] uint (&indices_buf)[];

  [[storage(PATCH_EVALUATION_OUTPUT_NORMALS_BUF_SLOT,
            write)]] [[condition(do_normals)]] FDotNor (&nors_buf)[];

  bool is_face_selected(uint coarse_quad_index)
  {
    return (extra_coarse_face_data[coarse_quad_index] & srt.shader_data.coarse_face_select_mask) !=
           0;
  }

  bool is_face_active(uint coarse_quad_index)
  {
    return (extra_coarse_face_data[coarse_quad_index] & srt.shader_data.coarse_face_active_mask) !=
           0;
  }

  bool is_face_hidden(uint coarse_quad_index)
  {
    return (extra_coarse_face_data[coarse_quad_index] & srt.shader_data.coarse_face_hidden_mask) !=
           0;
  }

  float get_face_flag(uint coarse_quad_index)
  {
    if (is_face_active(coarse_quad_index)) {
      return -1.0f;
    }

    if (is_face_selected(coarse_quad_index)) {
      return 1.0f;
    }

    return 0.0f;
  }
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void patch_eval_fdots([[resource_table]] const SubdivResources &srt,
                      [[resource_table]] const PatchEval &eval,
                      [[resource_table]] FdotsEval &res,
                      [[global_invocation_id]] const uint3 global_id,
                      [[num_work_groups]] const uint3 num_work_groups)
{
  /* We execute for each coarse quad. */
  uint coarse_quad_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (coarse_quad_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  BlenderPatchCoord patch_co = eval.patch_coords[coarse_quad_index];
  float2 uv = decode_uv(patch_co.encoded_uv);

  float3 pos = float3(0.0f);
  float3 du = float3(0.0f);
  float3 dv = float3(0.0f);
  eval.evaluate_patches_limits(patch_co.patch_index, uv.x, uv.y, pos, du, dv);
  float3 nor = normalize(cross(du, dv));

  FDotVert vert;
  vert.x = pos.x;
  vert.y = pos.y;
  vert.z = pos.z;

  FDotNor fnor;
  fnor.x = nor.x;
  fnor.y = nor.y;
  fnor.z = nor.z;
  fnor.flag = res.get_face_flag(coarse_quad_index);

  res.verts_buf[coarse_quad_index] = vert;
  if (res.consts.do_normals) [[static_branch]] {
    res.nors_buf[coarse_quad_index] = fnor;
  }

  if (srt.shader_data.use_hide && res.is_face_hidden(coarse_quad_index)) {
    res.indices_buf[coarse_quad_index] = 0xffffffff;
  }
  else {
    res.indices_buf[coarse_quad_index] = coarse_quad_index;
  }
}

/** \} */

/* ------------------------------------------------------------------------------
 * Vertices.
 */

struct VertsEval {
  [[resource_table]] PatchEvalConsts constants;

  [[storage(PATCH_EVALUATION_OUTPUT_POS_BUF_SLOT, write)]] Position (&pos_buf)[];
  [[storage(PATCH_EVALUATION_OUTPUT_ORCOS_BUF_SLOT,
            write)]] [[condition(do_orcos)]] float4 (&orco_buf)[];
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void patch_eval_verts([[resource_table]] const SubdivResources &srt,
                      [[resource_table]] const PatchEval &eval,
                      [[resource_table]] VertsEval &res,
                      [[global_invocation_id]] const uint3 global_id,
                      [[num_work_groups]] const uint3 num_work_groups)
{
  /* We execute for each quad. */
  uint quad_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (quad_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  for (uint loop_index = start_loop_index; loop_index < start_loop_index + 4; loop_index++) {
    float3 pos = float3(0.0f);
    float3 du = float3(0.0f);
    float3 dv = float3(0.0f);

    BlenderPatchCoord patch_co = eval.patch_coords[loop_index];
    float2 uv = decode_uv(patch_co.encoded_uv);

    eval.evaluate_patches_limits(patch_co.patch_index, uv.x, uv.y, pos, du, dv);

    Position position;
    position.x = pos.x;
    position.y = pos.y;
    position.z = pos.z;
    res.pos_buf[loop_index] = position;

    if (eval.consts.do_orcos) [[static_branch]] {
      pos = float3(0.0f);
      eval.evaluate_patches_limits_extra(patch_co.patch_index, uv.x, uv.y, pos);

      /* Set w = 0.0f to indicate that this is not a generic attribute.
       * See comments in `extract_mesh_vbo_orco.cc`. */
      float4 orco_data = float4(pos, 0.0f);
      res.orco_buf[loop_index] = orco_data;
    }
  }
}

/** \} */

PipelineCompute patch_evaluation_fvar(patch_eval_fvar,
                                      PatchEvalConsts{
                                          .do_orcos = false,
                                          .use_1st_derivatives = true,
                                      });
PipelineCompute patch_evaluation_fdots(patch_eval_fdots,
                                       PatchEvalConsts{
                                           .do_orcos = false,
                                           .use_1st_derivatives = true,
                                       },
                                       FdotsConsts{
                                           .do_normals = false,
                                       });
PipelineCompute patch_evaluation_fdots_normals(patch_eval_fdots,
                                               PatchEvalConsts{
                                                   .do_orcos = false,
                                                   .use_1st_derivatives = true,
                                               },
                                               FdotsConsts{
                                                   .do_normals = true,
                                               });
PipelineCompute patch_evaluation_verts(patch_eval_verts,
                                       PatchEvalConsts{
                                           .do_orcos = false,
                                           .use_1st_derivatives = true,
                                       });
PipelineCompute patch_evaluation_verts_orcos(patch_eval_verts,
                                             PatchEvalConsts{
                                                 .do_orcos = true,
                                                 .use_1st_derivatives = true,
                                             });

}  // namespace subdiv
