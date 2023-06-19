/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Bump Eval Nodes */

ccl_device_noinline void svm_node_enter_bump_eval(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  uint offset)
{
  /* save state */
  stack_store_float3(stack, offset + 0, sd->P);
  stack_store_float(stack, offset + 3, sd->dP);

  /* set state as if undisplaced */
  const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_POSITION_UNDISPLACED);

  if (desc.offset != ATTR_STD_NOT_FOUND) {
    differential3 dP;
    float3 P = primitive_surface_attribute_float3(kg, sd, desc, &dP.dx, &dP.dy);

    object_position_transform(kg, sd, &P);
    object_dir_transform(kg, sd, &dP.dx);
    object_dir_transform(kg, sd, &dP.dy);

    sd->P = P;
    sd->dP = differential_make_compact(dP);
  }
}

ccl_device_noinline void svm_node_leave_bump_eval(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  uint offset)
{
  /* restore state */
  sd->P = stack_load_float3(stack, offset + 0);
  sd->dP = stack_load_float(stack, offset + 3);
}

CCL_NAMESPACE_END
