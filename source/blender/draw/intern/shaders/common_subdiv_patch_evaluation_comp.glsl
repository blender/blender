/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* To be compiled with common_subdiv_lib.glsl */

/* Source buffer. */
layout(std430, binding = 0) buffer src_buffer
{
  float srcVertexBuffer[];
};

/* #DRWPatchMap */
layout(std430, binding = 1) readonly buffer inputPatchHandles
{
  PatchHandle input_patch_handles[];
};

layout(std430, binding = 2) readonly buffer inputQuadNodes
{
  QuadNode quad_nodes[];
};

layout(std430, binding = 3) readonly buffer inputPatchCoords
{
  BlenderPatchCoord patch_coords[];
};

layout(std430, binding = 4) readonly buffer inputVertOrigIndices
{
  int input_vert_origindex[];
};

/* Patch buffers. */
layout(std430, binding = 5) buffer patchArray_buffer
{
  OsdPatchArray patchArrayBuffer[];
};

layout(std430, binding = 6) buffer patchIndex_buffer
{
  int patchIndexBuffer[];
};

layout(std430, binding = 7) buffer patchParam_buffer
{
  OsdPatchParam patchParamBuffer[];
};

  /* Output buffer(s). */

#if defined(FVAR_EVALUATION)
layout(std430, binding = 8) writeonly buffer outputFVarData
{
  vec2 output_fvar[];
};
#elif defined(FDOTS_EVALUATION)
/* For face dots, we build the position, normals, and index buffers in one go. */

/* vec3 is padded to vec4, but the format used for face-dots does not have any padding. */
struct FDotVert {
  float x, y, z;
};

/* Same here, do not use vec3. */
struct FDotNor {
  float x, y, z;
  float flag;
};

layout(std430, binding = 8) writeonly buffer outputVertices
{
  FDotVert output_verts[];
};

#  ifdef FDOTS_NORMALS
layout(std430, binding = 9) writeonly buffer outputNormals
{
  FDotNor output_nors[];
};
#  endif

layout(std430, binding = 10) writeonly buffer outputFdotsIndices
{
  uint output_indices[];
};

layout(std430, binding = 11) readonly buffer extraCoarseFaceData
{
  uint extra_coarse_face_data[];
};
#else
layout(std430, binding = 8) readonly buffer inputFlagsBuffer
{
  int flags_buffer[]; /*char*/
};
float get_flag(int vertex)
{
  int char4 = flags_buffer[vertex / 4];
  int flag = (char4 >> ((vertex % 4) * 8)) & 0xFF;
  if (flag >= 128) {
    flag = -128 + (flag - 128);
  }

  return float(flag);
}
layout(std430, binding = 9) writeonly buffer outputVertexData
{
  PosNorLoop output_verts[];
};
#  if defined(ORCO_EVALUATION)
layout(std430, binding = 10) buffer src_extra_buffer
{
  float srcExtraVertexBuffer[];
};
layout(std430, binding = 11) writeonly buffer outputOrcoData
{
  vec4 output_orcos[];
};
#  endif
#endif

vec2 read_vec2(int index)
{
  vec2 result;
  result.x = srcVertexBuffer[index * 2];
  result.y = srcVertexBuffer[index * 2 + 1];
  return result;
}

vec3 read_vec3(int index)
{
  vec3 result;
  result.x = srcVertexBuffer[index * 3];
  result.y = srcVertexBuffer[index * 3 + 1];
  result.z = srcVertexBuffer[index * 3 + 2];
  return result;
}

#if defined(ORCO_EVALUATION)
vec3 read_vec3_extra(int index)
{
  vec3 result;
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
 * Patch Coordinate lookup. Return an OsdPatchCoord for the given patch_index and uvs.
 * This code is a port of the OpenSubdiv PatchMap lookup code.
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
  if (uHalf != 0)
    u -= median;

  int vHalf = (v >= median) ? 1 : 0;
  if (vHalf != 0)
    v -= median;

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
  if (face_index < min_patch_face || face_index > max_patch_face) {
    return bogus_patch_handle();
  }

  QuadNode node = quad_nodes[face_index - min_patch_face];

  if (!is_set(node.child[0])) {
    return bogus_patch_handle();
  }

  float median = 0.5;
  bool tri_rotated = false;

  for (int depth = 0; depth <= max_depth; ++depth, median *= 0.5) {
    int quadrant = (patches_are_triangular != 0) ?
                       transformUVToTriQuadrant(median, u, v, tri_rotated) :
                       transformUVToQuadQuadrant(median, u, v);

    if (is_leaf(node.child[quadrant])) {
      return input_patch_handles[get_index(node.child[quadrant])];
    }

    node = quad_nodes[get_index(node.child[quadrant])];
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
void evaluate_patches_limits(int patch_index, float u, float v, inout vec2 dst)
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
    vec2 src_fvar = read_vec2(src_offset + index);
    dst += src_fvar * wP[cv];
  }
}
#else
void evaluate_patches_limits(
    int patch_index, float u, float v, inout vec3 dst, inout vec3 du, inout vec3 dv)
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
    vec3 src_vertex = read_vec3(index);

    dst += src_vertex * wP[cv];
    du += src_vertex * wDu[cv];
    dv += src_vertex * wDv[cv];
  }
}

#  if defined(ORCO_EVALUATION)
/* Evaluate the patches limits from the extra source vertex buffer. */
void evaluate_patches_limits_extra(int patch_index, float u, float v, inout vec3 dst)
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
    vec3 src_vertex = read_vec3_extra(index);

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
  if (quad_index >= total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  for (uint loop_index = start_loop_index; loop_index < start_loop_index + 4; loop_index++) {
    vec2 fvar = vec2(0.0);

    BlenderPatchCoord patch_co = patch_coords[loop_index];
    vec2 uv = decode_uv(patch_co.encoded_uv);

    evaluate_patches_limits(patch_co.patch_index, uv.x, uv.y, fvar);
    output_fvar[dst_offset + loop_index] = fvar;
  }
}
#elif defined(FDOTS_EVALUATION)
bool is_face_selected(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & coarse_face_select_mask) != 0;
}

bool is_face_active(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & coarse_face_active_mask) != 0;
}

float get_face_flag(uint coarse_quad_index)
{
  if (is_face_active(coarse_quad_index)) {
    return -1.0;
  }

  if (is_face_selected(coarse_quad_index)) {
    return 1.0;
  }

  return 0.0;
}

bool is_face_hidden(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & coarse_face_hidden_mask) != 0;
}

void main()
{
  /* We execute for each coarse quad. */
  uint coarse_quad_index = get_global_invocation_index();
  if (coarse_quad_index >= total_dispatch_size) {
    return;
  }

  BlenderPatchCoord patch_co = patch_coords[coarse_quad_index];
  vec2 uv = decode_uv(patch_co.encoded_uv);

  vec3 pos = vec3(0.0);
  vec3 du = vec3(0.0);
  vec3 dv = vec3(0.0);
  evaluate_patches_limits(patch_co.patch_index, uv.x, uv.y, pos, du, dv);
  vec3 nor = normalize(cross(du, dv));

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

  if (use_hide && is_face_hidden(coarse_quad_index)) {
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
  if (quad_index >= total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  for (uint loop_index = start_loop_index; loop_index < start_loop_index + 4; loop_index++) {
    vec3 pos = vec3(0.0);
    vec3 du = vec3(0.0);
    vec3 dv = vec3(0.0);

    BlenderPatchCoord patch_co = patch_coords[loop_index];
    vec2 uv = decode_uv(patch_co.encoded_uv);

    evaluate_patches_limits(patch_co.patch_index, uv.x, uv.y, pos, du, dv);

    /* This will be computed later. */
    vec3 nor = vec3(0.0);

    int origindex = input_vert_origindex[loop_index];
    float flag = 0.0;
    if (origindex == -1) {
      flag = -1.0;
    }
    else {
      flag = get_flag(origindex);
    }

    PosNorLoop vertex_data;
    set_vertex_pos(vertex_data, pos);
    set_vertex_nor(vertex_data, nor, flag);
    output_verts[loop_index] = vertex_data;

#  if defined(ORCO_EVALUATION)
    pos = vec3(0.0);
    evaluate_patches_limits_extra(patch_co.patch_index, uv.x, uv.y, pos);

    /* Set w = 0.0 to indicate that this is not a generic attribute.
     * See comments in `extract_mesh_vbo_orco.cc`. */
    vec4 orco_data = vec4(pos, 0.0);
    output_orcos[loop_index] = orco_data;
#  endif
  }
}
#endif
