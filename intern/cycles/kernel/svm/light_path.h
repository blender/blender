/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Light Path Node */

template<uint node_feature_mask, typename ConstIntegratorGenericState>
ccl_device_noinline void svm_node_light_path(KernelGlobals kg,
                                             ConstIntegratorGenericState state,
                                             const ccl_private ShaderData *sd,
                                             ccl_private float *stack,
                                             const uint type,
                                             const uint out_offset,
                                             const uint32_t path_flag)
{
  float info = 0.0f;

  switch ((NodeLightPath)type) {
    case NODE_LP_camera:
      info = (path_flag & PATH_RAY_CAMERA) ? 1.0f : 0.0f;
      break;
    case NODE_LP_shadow:
      info = (path_flag & PATH_RAY_SHADOW) ? 1.0f : 0.0f;
      break;
    case NODE_LP_diffuse:
      info = (path_flag & PATH_RAY_DIFFUSE) ? 1.0f : 0.0f;
      break;
    case NODE_LP_glossy:
      info = (path_flag & PATH_RAY_GLOSSY) ? 1.0f : 0.0f;
      break;
    case NODE_LP_singular:
      info = (path_flag & PATH_RAY_SINGULAR) ? 1.0f : 0.0f;
      break;
    case NODE_LP_reflection:
      info = (path_flag & PATH_RAY_REFLECT) ? 1.0f : 0.0f;
      break;
    case NODE_LP_transmission:
      info = (path_flag & PATH_RAY_TRANSMIT) ? 1.0f : 0.0f;
      break;
    case NODE_LP_volume_scatter:
      info = (path_flag & PATH_RAY_VOLUME_SCATTER) ? 1.0f : 0.0f;
      break;
    case NODE_LP_backfacing:
      info = (sd->flag & SD_BACKFACING) ? 1.0f : 0.0f;
      break;
    case NODE_LP_ray_length:
      info = sd->ray_length;
      break;
    case NODE_LP_ray_depth: {
      /* Read bounce from different locations depending on if this is a shadow
       * path. It's a bit dubious to have integrate state details leak into
       * this function but hard to avoid currently. */
      IF_KERNEL_NODES_FEATURE(LIGHT_PATH)
      {
        info = (float)integrator_state_bounce(state, path_flag);
      }

      /* For background, light emission and shadow evaluation from a
       * surface or volume we are effectively one bounce further. */
      if (path_flag & (PATH_RAY_SHADOW | PATH_RAY_EMISSION)) {
        info += 1.0f;
      }
      break;
    }
    case NODE_LP_ray_transparent: {
      IF_KERNEL_NODES_FEATURE(LIGHT_PATH)
      {
        info = (float)integrator_state_transparent_bounce(state, path_flag);
      }
      break;
    }
    case NODE_LP_ray_diffuse:
      IF_KERNEL_NODES_FEATURE(LIGHT_PATH)
      {
        info = (float)integrator_state_diffuse_bounce(state, path_flag);
      }
      break;
    case NODE_LP_ray_glossy:
      IF_KERNEL_NODES_FEATURE(LIGHT_PATH)
      {
        info = (float)integrator_state_glossy_bounce(state, path_flag);
      }
      break;
    case NODE_LP_ray_transmission:
      IF_KERNEL_NODES_FEATURE(LIGHT_PATH)
      {
        info = (float)integrator_state_transmission_bounce(state, path_flag);
      }
      break;
  }

  stack_store_float(stack, out_offset, info);
}

/* Light Falloff Node */

ccl_device_noinline void svm_node_light_falloff(ccl_private ShaderData *sd,
                                                ccl_private float *stack,
                                                const uint4 node)
{
  uint strength_offset;
  uint out_offset;
  uint smooth_offset;

  svm_unpack_node_uchar3(node.z, &strength_offset, &smooth_offset, &out_offset);

  float strength = stack_load_float(stack, strength_offset);
  if (sd->ray_length == FLT_MAX) {
    /* Distant lights (which have a ray_length of FLT_MAX) overflow when using most outputs of
     * the light falloff node. So just ignore the node in that case. */
    stack_store_float(stack, out_offset, strength);
    return;
  }

  const uint type = node.y;

  switch ((NodeLightFalloff)type) {
    case NODE_LIGHT_FALLOFF_QUADRATIC:
      break;
    case NODE_LIGHT_FALLOFF_LINEAR:
      strength *= sd->ray_length;
      break;
    case NODE_LIGHT_FALLOFF_CONSTANT:
      strength *= sd->ray_length * sd->ray_length;
      break;
  }

  const float smooth = stack_load_float(stack, smooth_offset);

  if (smooth > 0.0f) {
    const float squared = sd->ray_length * sd->ray_length;
    strength *= squared / (smooth + squared);
  }

  stack_store_float(stack, out_offset, strength);
}

CCL_NAMESPACE_END
