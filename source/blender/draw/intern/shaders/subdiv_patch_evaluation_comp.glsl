/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "subdiv_lib.glsl"

COMPUTE_SHADER_CREATE_INFO(subdiv_patch_evaluation_fdots_normals)

float2 read_vec2(int index)
{
  float2 result;
  result.x = srcVertexBuffer[index * 2];
  result.y = srcVertexBuffer[index * 2 + 1];
  return result;
}

float3 read_vec3(int index)
{
  float3 result;
  result.x = srcVertexBuffer[index * 3];
  result.y = srcVertexBuffer[index * 3 + 1];
  result.z = srcVertexBuffer[index * 3 + 2];
  return result;
}

#if defined(ORCO_EVALUATION)
float3 read_vec3_extra(int index)
{
  float3 result;
  result.x = srcExtraVertexBuffer[index * 3];
  result.y = srcExtraVertexBuffer[index * 3 + 1];
  result.z = srcExtraVertexBuffer[index * 3 + 2];
  return result;
}
#endif

OsdPatchArray GetPatchArray(int arrayIndex)
{
  return patchArrayBuffer[arrayIndex];
}

OsdPatchParam GetPatchParam(int patchIndex)
{
  return patchParamBuffer[patchIndex];
}

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

int transformUVToQuadQuadrant(float median, inout float u, inout float v)
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

int transformUVToTriQuadrant(float median, inout float u, inout float v, inout bool rotated)
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

PatchHandle find_patch(int face_index, float u, float v)
{
  if (face_index < shader_data.min_patch_face || face_index > shader_data.max_patch_face) {
    return bogus_patch_handle();
  }

  QuadNode node = quad_nodes[face_index - shader_data.min_patch_face];

  if (!is_set(node.child[0])) {
    return bogus_patch_handle();
  }

  float median = 0.5f;
  bool tri_rotated = false;

  for (int depth = 0; depth <= shader_data.max_depth; ++depth, median *= 0.5f) {
    int quadrant = shader_data.patches_are_triangular ?
                       transformUVToTriQuadrant(median, u, v, tri_rotated) :
                       transformUVToQuadQuadrant(median, u, v);

    if (is_leaf(node.child[quadrant])) {
      return input_patch_handles[get_index(node.child[quadrant])];
    }

    node = quad_nodes[get_index(node.child[quadrant])];
  }
  return bogus_patch_handle();
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

OsdPatchCoord GetPatchCoord(int face_index, float u, float v)
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

#if defined(FVAR_EVALUATION)
void evaluate_patches_limits(int patch_index, float u, float v, inout float2 dst)
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
    float2 src_fvar = read_vec2(shader_data.src_offset + index);
    dst += src_fvar * wP[cv];
  }
}
#else
void evaluate_patches_limits(
    int patch_index, float u, float v, inout float3 dst, inout float3 du, inout float3 dv)
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

#  if defined(ORCO_EVALUATION)
/* Evaluate the patches limits from the extra source vertex buffer. */
void evaluate_patches_limits_extra(int patch_index, float u, float v, inout float3 dst)
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
    float3 src_vertex = read_vec3_extra(index);

    dst += src_vertex * wP[cv];
  }
}
#  endif
#endif

/* ------------------------------------------------------------------------------
 * Entry point.
 */

#if defined(FVAR_EVALUATION)
void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= shader_data.total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  for (uint loop_index = start_loop_index; loop_index < start_loop_index + 4; loop_index++) {
    float2 fvar = float2(0.0f);

    BlenderPatchCoord patch_co = patch_coords[loop_index];
    float2 uv = decode_uv(patch_co.encoded_uv);

    evaluate_patches_limits(patch_co.patch_index, uv.x, uv.y, fvar);
    output_fvar[shader_data.dst_offset + loop_index] = fvar;
  }
}
#elif defined(FDOTS_EVALUATION)
bool is_face_selected(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & shader_data.coarse_face_select_mask) != 0;
}

bool is_face_active(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & shader_data.coarse_face_active_mask) != 0;
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

bool is_face_hidden(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & shader_data.coarse_face_hidden_mask) != 0;
}

void main()
{
  /* We execute for each coarse quad. */
  uint coarse_quad_index = get_global_invocation_index();
  if (coarse_quad_index >= shader_data.total_dispatch_size) {
    return;
  }

  BlenderPatchCoord patch_co = patch_coords[coarse_quad_index];
  float2 uv = decode_uv(patch_co.encoded_uv);

  float3 pos = float3(0.0f);
  float3 du = float3(0.0f);
  float3 dv = float3(0.0f);
  evaluate_patches_limits(patch_co.patch_index, uv.x, uv.y, pos, du, dv);
  float3 nor = normalize(cross(du, dv));

  FDotVert vert;
  vert.x = pos.x;
  vert.y = pos.y;
  vert.z = pos.z;

  FDotNor fnor;
  fnor.x = nor.x;
  fnor.y = nor.y;
  fnor.z = nor.z;
  fnor.flag = get_face_flag(coarse_quad_index);

  output_verts[coarse_quad_index] = vert;
#  ifdef FDOTS_NORMALS
  output_nors[coarse_quad_index] = fnor;
#  endif

  if (shader_data.use_hide && is_face_hidden(coarse_quad_index)) {
    output_indices[coarse_quad_index] = 0xffffffff;
  }
  else {
    output_indices[coarse_quad_index] = coarse_quad_index;
  }
}
#else
void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= shader_data.total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  for (uint loop_index = start_loop_index; loop_index < start_loop_index + 4; loop_index++) {
    float3 pos = float3(0.0f);
    float3 du = float3(0.0f);
    float3 dv = float3(0.0f);

    BlenderPatchCoord patch_co = patch_coords[loop_index];
    float2 uv = decode_uv(patch_co.encoded_uv);

    evaluate_patches_limits(patch_co.patch_index, uv.x, uv.y, pos, du, dv);

    Position position;
    position.x = pos.x;
    position.y = pos.y;
    position.z = pos.z;
    positions[loop_index] = position;

#  if defined(ORCO_EVALUATION)
    pos = float3(0.0f);
    evaluate_patches_limits_extra(patch_co.patch_index, uv.x, uv.y, pos);

    /* Set w = 0.0f to indicate that this is not a generic attribute.
     * See comments in `extract_mesh_vbo_orco.cc`. */
    float4 orco_data = float4(pos, 0.0f);
    output_orcos[loop_index] = orco_data;
#  endif
  }
}
#endif
