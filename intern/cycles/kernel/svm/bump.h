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

  /* This is a loop to work around a Metal compiler issue, this way we can have
   * a single find_attribute and primitive_surface_attribute call. */
  AttributeStandard std[2] = {ATTR_STD_POSITION_UNDISPLACED, ATTR_STD_NORMAL_UNDISPLACED};
  for (int i = 0; i < 2; i++) {
    const AttributeDescriptor desc = find_attribute(kg, sd, std[i]);
    if (desc.offset == ATTR_STD_NOT_FOUND) {
      continue;
    }

    dual3 attr = primitive_surface_attribute<float3>(kg, sd, desc, true, true);

    if (std[i] == ATTR_STD_NORMAL_UNDISPLACED) {
      /* Set normal as if undisplaced.
       * Note this does not need to be restored, because the bump evaluation will
       * write to sd->N. */
      float3 N = safe_normalize(attr.val);
      object_normal_transform(kg, sd, &N);
      sd->N = (sd->flag & SD_BACKFACING) ? -N : N;
    }
    else {
      /* Set position as if undisplaced. */
      object_position_transform(kg, sd, &attr);

      sd->P = attr.val;
      sd->dP = differential_make_compact(attr);

      /* Save the full differential, the compact form isn't enough for svm_node_set_bump. */
      stack_store_float3(stack, offset + 4, attr.dx);
      stack_store_float3(stack, offset + 7, attr.dy);
    }
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
