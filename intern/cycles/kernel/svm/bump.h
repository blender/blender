/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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
  stack_store_float3(stack, offset + 3, sd->dP.dx);
  stack_store_float3(stack, offset + 6, sd->dP.dy);

  /* set state as if undisplaced */
  const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_POSITION_UNDISPLACED);

  if (desc.offset != ATTR_STD_NOT_FOUND) {
    float3 P, dPdx, dPdy;
    P = primitive_surface_attribute_float3(kg, sd, desc, &dPdx, &dPdy);

    object_position_transform(kg, sd, &P);
    object_dir_transform(kg, sd, &dPdx);
    object_dir_transform(kg, sd, &dPdy);

    sd->P = P;
    sd->dP.dx = dPdx;
    sd->dP.dy = dPdy;
  }
}

ccl_device_noinline void svm_node_leave_bump_eval(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  uint offset)
{
  /* restore state */
  sd->P = stack_load_float3(stack, offset + 0);
  sd->dP.dx = stack_load_float3(stack, offset + 3);
  sd->dP.dy = stack_load_float3(stack, offset + 6);
}

CCL_NAMESPACE_END
