/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

ccl_device_noinline void svm_node_scene_time(KernelGlobals kg,
                                             ccl_private float *ccl_restrict stack,
                                             const ccl_global SVMNodeSceneTime &ccl_restrict node)
{
  if (stack_valid(node.seconds_out)) {
    stack_store_float(stack, node.seconds_out, kernel_data.scene_time.time);
  }
  if (stack_valid(node.frame_out)) {
    stack_store_float(stack, node.frame_out, kernel_data.scene_time.frame);
  }
}

CCL_NAMESPACE_END
