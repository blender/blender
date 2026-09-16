/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/image.h"

#include "kernel/camera/projection.h"

#include "kernel/geom/object.h"

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

#include "util/color.h"
#include "util/types_image.h"

CCL_NAMESPACE_BEGIN

ccl_device float4 svm_image_texture(KernelGlobals kg,
                                    ccl_private ShaderData *sd,
                                    const int id,
                                    const dual2 uv,
                                    const uint flags,
                                    const float4 missing_rgba)
{
  float4 r = kernel_image_interp_with_udim(kg, sd, id, uv, missing_rgba);
  const float alpha = r.w;

  if ((flags & NODE_IMAGE_ALPHA_UNASSOCIATE) && alpha != 1.0f && alpha != 0.0f) {
    r /= alpha;
    r.w = alpha;
  }

  if (flags & NODE_IMAGE_COMPRESS_AS_SRGB) {
    r = color_srgb_to_linear_v4(r);
  }

  return r;
}

/* Remap coordinate from 0..1 box to -1..-1 */
template<class Float3Type> ccl_device_inline Float3Type texco_remap_square(const Float3Type co)
{
  return (co - make_float3(0.5f, 0.5f, 0.5f)) * 2.0f;
}

template<class Float3Type>
ccl_device_inline auto svm_node_tex_image_mapping(const Float3Type co, const uint proj)
{
  if (proj == NODE_IMAGE_PROJ_SPHERE) {
    return map_to_sphere(texco_remap_square(co));
  }
  if (proj == NODE_IMAGE_PROJ_TUBE) {
    return map_to_tube(texco_remap_square(co));
  }

  return make_float2(co);
}

template<class Float3Type>
ccl_device_noinline void svm_node_tex_image(KernelGlobals kg,
                                            ccl_private ShaderData *sd,
                                            ccl_private float *ccl_restrict stack,
                                            const ccl_global SVMNodeTexImage &ccl_restrict node)
{
  const float3 missing = stack_load(stack, node.missing);
  const float missing_alpha = stack_load(stack, node.missing_alpha);
  const Float3Type co = stack_load<Float3Type>(stack, node.co);
  const dual2 tex_co(svm_node_tex_image_mapping(co, node.projection));

  const float4 f = svm_image_texture(
      kg, sd, node.id, tex_co, node.flags, make_float4(missing, missing_alpha));

  if (stack_valid(node.out_offset)) {
    stack_store_float3(stack, node.out_offset, make_float3(f));
  }
  if (stack_valid(node.alpha_offset)) {
    stack_store_float(stack, node.alpha_offset, f.w);
  }
}

template<class Float3Type>
ccl_device_noinline void svm_node_tex_image_box(KernelGlobals kg,
                                                ccl_private ShaderData *sd,
                                                ccl_private float *ccl_restrict stack,
                                                const ccl_global SVMNodeTexImageBox &ccl_restrict
                                                    node)
{
  /* get object space normal */
  float3 N = sd->N;

  object_inverse_normal_transform(kg, sd, &N);

  /* project from direction vector to barycentric coordinates in triangles */
  const float3 signed_N = N;

  N = fabs(N);

  N /= (N.x + N.y + N.z);

  /* basic idea is to think of this as a triangle, each corner representing
   * one of the 3 faces of the cube. in the corners we have single textures,
   * in between we blend between two textures, and in the middle we a blend
   * between three textures.
   *
   * The `Nxyz` values are the barycentric coordinates in an equilateral
   * triangle, which in case of blending, in the middle has a smaller
   * equilateral triangle where 3 textures blend. this divides things into
   * 7 zones, with an `if()` test for each zone. */

  float3 weight = make_float3(0.0f, 0.0f, 0.0f);
  const float blend = node.blend;
  const float limit = 0.5f * (1.0f + blend);

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
      weight.x = saturatef((weight.x - 0.5f * (1.0f - blend)) / blend);
      weight.y = 1.0f - weight.x;
    }
    else if (N.x < (1.0f - limit) * (N.y + N.z)) {
      weight.y = N.y / (N.y + N.z);
      weight.y = saturatef((weight.y - 0.5f * (1.0f - blend)) / blend);
      weight.z = 1.0f - weight.y;
    }
    else if (N.y < (1.0f - limit) * (N.x + N.z)) {
      weight.x = N.x / (N.x + N.z);
      weight.x = saturatef((weight.x - 0.5f * (1.0f - blend)) / blend);
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
    /* Desperate mode, no valid choice anyway, fall back to one side. */
    weight.x = 1.0f;
  }

  /* now fetch textures */
  float4 f = zero_float4();

  const float3 missing = stack_load(stack, node.missing);
  const float missing_alpha = stack_load(stack, node.missing_alpha);
  const dual3 co = dual3(stack_load<Float3Type>(stack, node.co));

  /* Map so that no textures are flipped, rotation is somewhat arbitrary. */
  if (weight.x > 0.0f) {
    const dual2 uv = make_float2((signed_N.x < 0.0f) ? 1.0f - co.y() : co.y(), co.z());
    f += weight.x *
         svm_image_texture(kg, sd, node.id, uv, node.flags, make_float4(missing, missing_alpha));
  }
  if (weight.y > 0.0f) {
    const dual2 uv = make_float2((signed_N.y > 0.0f) ? 1.0f - co.x() : co.x(), co.z());
    f += weight.y *
         svm_image_texture(kg, sd, node.id, uv, node.flags, make_float4(missing, missing_alpha));
  }
  if (weight.z > 0.0f) {
    const dual2 uv = make_float2((signed_N.z > 0.0f) ? 1.0f - co.y() : co.y(), co.x());
    f += weight.z *
         svm_image_texture(kg, sd, node.id, uv, node.flags, make_float4(missing, missing_alpha));
  }

  if (stack_valid(node.out_offset)) {
    stack_store_float3(stack, node.out_offset, make_float3(f.x, f.y, f.z));
  }
  if (stack_valid(node.alpha_offset)) {
    stack_store_float(stack, node.alpha_offset, f.w);
  }
}

template<class Float3Type>
ccl_device_inline auto svm_node_tex_environment_projection(Float3Type co, const uint proj)
{
  co = safe_normalize(co);
  if (proj == 0) {
    return direction_to_equirectangular(co);
  }
  return direction_to_mirrorball(co);
}

template<class Float3Type>
ccl_device_noinline void svm_node_tex_environment(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    ccl_private float *ccl_restrict stack,
    const ccl_global SVMNodeTexEnvironment &ccl_restrict node)
{
  const float3 missing = stack_load(stack, node.missing);
  const float missing_alpha = stack_load(stack, node.missing_alpha);
  const Float3Type co = stack_load<Float3Type>(stack, node.co);
  const dual2 uv(svm_node_tex_environment_projection(co, node.projection));

  const float4 f = svm_image_texture(
      kg, sd, node.id, uv, node.flags, make_float4(missing, missing_alpha));

  if (stack_valid(node.out_offset)) {
    stack_store_float3(stack, node.out_offset, make_float3(f.x, f.y, f.z));
  }
  if (stack_valid(node.alpha_offset)) {
    stack_store_float(stack, node.alpha_offset, f.w);
  }
}

CCL_NAMESPACE_END
