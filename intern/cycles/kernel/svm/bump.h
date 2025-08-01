/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/geom/attribute.h"
#include "kernel/geom/object.h"
#include "kernel/geom/primitive.h"

#include "kernel/svm/util.h"

#include "kernel/util/differential.h"

CCL_NAMESPACE_BEGIN

/* Bump Eval Nodes */

ccl_device_noinline void svm_node_enter_bump_eval(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  const uint offset)
{
  /* save state */
  stack_store_float3(stack, offset + 0, sd->P);
  stack_store_float(stack, offset + 3, sd->dP);

  /* set state as if undisplaced */
  const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_POSITION_UNDISPLACED);

  if (desc.offset != ATTR_STD_NOT_FOUND) {
    dual3 P = primitive_surface_attribute<float3>(kg, sd, desc, true, true);

    object_position_transform(kg, sd, &P);

    sd->P = P.val;
    sd->dP = differential_make_compact(P);

    /* Save the full differential, the compact form isn't enough for svm_node_set_bump. */
    stack_store_float3(stack, offset + 4, P.dx);
    stack_store_float3(stack, offset + 7, P.dy);
  }
}

ccl_device_noinline void svm_node_leave_bump_eval(ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  const uint offset)
{
  /* restore state */
  sd->P = stack_load_float3(stack, offset + 0);
  sd->dP = stack_load_float(stack, offset + 3);
}

CCL_NAMESPACE_END
