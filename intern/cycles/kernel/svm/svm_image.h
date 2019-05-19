/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

ccl_device float4 svm_image_texture(KernelGlobals *kg, int id, float x, float y, uint flags)
{
  float4 r = kernel_tex_image_interp(kg, id, x, y);
  const float alpha = r.w;

  if ((flags & NODE_IMAGE_ALPHA_UNASSOCIATE) && alpha != 1.0f && alpha != 0.0f) {
    r /= alpha;
    const int texture_type = kernel_tex_type(id);
    if (texture_type == IMAGE_DATA_TYPE_BYTE4 || texture_type == IMAGE_DATA_TYPE_BYTE) {
      r = min(r, make_float4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    r.w = alpha;
  }

  if (flags & NODE_IMAGE_COMPRESS_AS_SRGB) {
    r = color_srgb_to_linear_v4(r);
  }

  return r;
}

/* Remap coordnate from 0..1 box to -1..-1 */
ccl_device_inline float3 texco_remap_square(float3 co)
{
  return (co - make_float3(0.5f, 0.5f, 0.5f)) * 2.0f;
}

ccl_device void svm_node_tex_image(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
  uint id = node.y;
  uint co_offset, out_offset, alpha_offset, flags;

  decode_node_uchar4(node.z, &co_offset, &out_offset, &alpha_offset, &flags);

  float3 co = stack_load_float3(stack, co_offset);
  float2 tex_co;
  if (node.w == NODE_IMAGE_PROJ_SPHERE) {
    co = texco_remap_square(co);
    tex_co = map_to_sphere(co);
  }
  else if (node.w == NODE_IMAGE_PROJ_TUBE) {
    co = texco_remap_square(co);
    tex_co = map_to_tube(co);
  }
  else {
    tex_co = make_float2(co.x, co.y);
  }
  float4 f = svm_image_texture(kg, id, tex_co.x, tex_co.y, flags);

  if (stack_valid(out_offset))
    stack_store_float3(stack, out_offset, make_float3(f.x, f.y, f.z));
  if (stack_valid(alpha_offset))
    stack_store_float(stack, alpha_offset, f.w);
}

ccl_device void svm_node_tex_image_box(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
  /* get object space normal */
  float3 N = sd->N;

  N = sd->N;
  object_inverse_normal_transform(kg, sd, &N);

  /* project from direction vector to barycentric coordinates in triangles */
  float3 signed_N = N;

  N.x = fabsf(N.x);
  N.y = fabsf(N.y);
  N.z = fabsf(N.z);

  N /= (N.x + N.y + N.z);

  /* basic idea is to think of this as a triangle, each corner representing
   * one of the 3 faces of the cube. in the corners we have single textures,
   * in between we blend between two textures, and in the middle we a blend
   * between three textures.
   *
   * the Nxyz values are the barycentric coordinates in an equilateral
   * triangle, which in case of blending, in the middle has a smaller
   * equilateral triangle where 3 textures blend. this divides things into
   * 7 zones, with an if() test for each zone */

  float3 weight = make_float3(0.0f, 0.0f, 0.0f);
  float blend = __int_as_float(node.w);
  float limit = 0.5f * (1.0f + blend);

  /* first test for corners with single texture */
  if (N.x > limit * (N.x + N.y) && N.x > limit * (N.x + N.z)) {
    weight.x = 1.0f;
  }
  else if (N.y > limit * (N.x + N.y) && N.y > limit * (N.y + N.z)) {
    weight.y = 1.0f;
  }
  else if (N.z > limit * (N.x + N.z) && N.z > limit * (N.y + N.z)) {
    weight.z = 1.0f;
  }
  else if (blend > 0.0f) {
    /* in case of blending, test for mixes between two textures */
    if (N.z < (1.0f - limit) * (N.y + N.x)) {
      weight.x = N.x / (N.x + N.y);
      weight.x = saturate((weight.x - 0.5f * (1.0f - blend)) / blend);
      weight.y = 1.0f - weight.x;
    }
    else if (N.x < (1.0f - limit) * (N.y + N.z)) {
      weight.y = N.y / (N.y + N.z);
      weight.y = saturate((weight.y - 0.5f * (1.0f - blend)) / blend);
      weight.z = 1.0f - weight.y;
    }
    else if (N.y < (1.0f - limit) * (N.x + N.z)) {
      weight.x = N.x / (N.x + N.z);
      weight.x = saturate((weight.x - 0.5f * (1.0f - blend)) / blend);
      weight.z = 1.0f - weight.x;
    }
    else {
      /* last case, we have a mix between three */
      weight.x = ((2.0f - limit) * N.x + (limit - 1.0f)) / (2.0f * limit - 1.0f);
      weight.y = ((2.0f - limit) * N.y + (limit - 1.0f)) / (2.0f * limit - 1.0f);
      weight.z = ((2.0f - limit) * N.z + (limit - 1.0f)) / (2.0f * limit - 1.0f);
    }
  }
  else {
    /* Desperate mode, no valid choice anyway, fallback to one side.*/
    weight.x = 1.0f;
  }

  /* now fetch textures */
  uint co_offset, out_offset, alpha_offset, flags;
  decode_node_uchar4(node.z, &co_offset, &out_offset, &alpha_offset, &flags);

  float3 co = stack_load_float3(stack, co_offset);
  uint id = node.y;

  float4 f = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

  /* Map so that no textures are flipped, rotation is somewhat arbitrary. */
  if (weight.x > 0.0f) {
    float2 uv = make_float2((signed_N.x < 0.0f) ? 1.0f - co.y : co.y, co.z);
    f += weight.x * svm_image_texture(kg, id, uv.x, uv.y, flags);
  }
  if (weight.y > 0.0f) {
    float2 uv = make_float2((signed_N.y > 0.0f) ? 1.0f - co.x : co.x, co.z);
    f += weight.y * svm_image_texture(kg, id, uv.x, uv.y, flags);
  }
  if (weight.z > 0.0f) {
    float2 uv = make_float2((signed_N.z > 0.0f) ? 1.0f - co.y : co.y, co.x);
    f += weight.z * svm_image_texture(kg, id, uv.x, uv.y, flags);
  }

  if (stack_valid(out_offset))
    stack_store_float3(stack, out_offset, make_float3(f.x, f.y, f.z));
  if (stack_valid(alpha_offset))
    stack_store_float(stack, alpha_offset, f.w);
}

ccl_device void svm_node_tex_environment(KernelGlobals *kg,
                                         ShaderData *sd,
                                         float *stack,
                                         uint4 node)
{
  uint id = node.y;
  uint co_offset, out_offset, alpha_offset, flags;
  uint projection = node.w;

  decode_node_uchar4(node.z, &co_offset, &out_offset, &alpha_offset, &flags);

  float3 co = stack_load_float3(stack, co_offset);
  float2 uv;

  co = safe_normalize(co);

  if (projection == 0)
    uv = direction_to_equirectangular(co);
  else
    uv = direction_to_mirrorball(co);

  float4 f = svm_image_texture(kg, id, uv.x, uv.y, flags);

  if (stack_valid(out_offset))
    stack_store_float3(stack, out_offset, make_float3(f.x, f.y, f.z));
  if (stack_valid(alpha_offset))
    stack_store_float(stack, alpha_offset, f.w);
}

CCL_NAMESPACE_END
