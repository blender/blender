/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* TODO(sergey): Think of making it more generic volume-type attribute
 * sampler.
 */
template<uint node_feature_mask>
ccl_device_noinline int svm_node_tex_voxel(
    KernelGlobals kg, ccl_private ShaderData *sd, ccl_private float *stack, uint4 node, int offset)
{
  uint co_offset, density_out_offset, color_out_offset, space;
  svm_unpack_node_uchar4(node.z, &co_offset, &density_out_offset, &color_out_offset, &space);

  float4 r = zero_float4();

#ifdef __VOLUME__
  IF_KERNEL_NODES_FEATURE(VOLUME)
  {
    int id = node.y;
    float3 co = stack_load_float3(stack, co_offset);
    if (space == NODE_TEX_VOXEL_SPACE_OBJECT) {
      co = volume_normalized_position(kg, sd, co);
    }
    else {
      kernel_assert(space == NODE_TEX_VOXEL_SPACE_WORLD);
      Transform tfm;
      tfm.x = read_node_float(kg, &offset);
      tfm.y = read_node_float(kg, &offset);
      tfm.z = read_node_float(kg, &offset);
      co = transform_point(&tfm, co);
    }

    r = kernel_tex_image_interp_3d(kg, id, co, INTERPOLATION_NONE);
  }
  else
#endif /* __VOLUME__ */
      if (space != NODE_TEX_VOXEL_SPACE_OBJECT)
  {
    read_node_float(kg, &offset);
    read_node_float(kg, &offset);
    read_node_float(kg, &offset);
  }

  if (stack_valid(density_out_offset)) {
    stack_store_float(stack, density_out_offset, r.w);
  }
  if (stack_valid(color_out_offset)) {
    stack_store_float3(stack, color_out_offset, make_float3(r.x, r.y, r.z));
  }
  return offset;
}

CCL_NAMESPACE_END
