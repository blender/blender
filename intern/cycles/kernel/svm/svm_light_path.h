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

/* Light Path Node */

ccl_device_noinline void svm_node_light_path(INTEGRATOR_STATE_CONST_ARGS,
                                             ccl_private const ShaderData *sd,
                                             ccl_private float *stack,
                                             uint type,
                                             uint out_offset,
                                             int path_flag)
{
  float info = 0.0f;

  switch (type) {
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
      /* Read bounce from difference location depending if this is a shadow
       * path. It's a bit dubious to have integrate state details leak into
       * this function but hard to avoid currently. */
      int bounce = (INTEGRATOR_STATE_IS_NULL)    ? 0 :
                   (path_flag & PATH_RAY_SHADOW) ? INTEGRATOR_STATE(shadow_path, bounce) :
                                                   INTEGRATOR_STATE(path, bounce);

      /* For background, light emission and shadow evaluation we from a
       * surface or volume we are effective one bounce further. */
      if (path_flag & (PATH_RAY_SHADOW | PATH_RAY_EMISSION)) {
        bounce++;
      }

      info = (float)bounce;
      break;
    }
      /* TODO */
    case NODE_LP_ray_transparent: {
      const int bounce = (INTEGRATOR_STATE_IS_NULL) ?
                             0 :
                         (path_flag & PATH_RAY_SHADOW) ?
                             INTEGRATOR_STATE(shadow_path, transparent_bounce) :
                             INTEGRATOR_STATE(path, transparent_bounce);

      info = (float)bounce;
      break;
    }
#if 0
    case NODE_LP_ray_diffuse:
      info = (float)state->diffuse_bounce;
      break;
    case NODE_LP_ray_glossy:
      info = (float)state->glossy_bounce;
      break;
#endif
#if 0
    case NODE_LP_ray_transmission:
      info = (float)state->transmission_bounce;
      break;
#endif
  }

  stack_store_float(stack, out_offset, info);
}

/* Light Falloff Node */

ccl_device_noinline void svm_node_light_falloff(ccl_private ShaderData *sd,
                                                ccl_private float *stack,
                                                uint4 node)
{
  uint strength_offset, out_offset, smooth_offset;

  svm_unpack_node_uchar3(node.z, &strength_offset, &smooth_offset, &out_offset);

  float strength = stack_load_float(stack, strength_offset);
  uint type = node.y;

  switch (type) {
    case NODE_LIGHT_FALLOFF_QUADRATIC:
      break;
    case NODE_LIGHT_FALLOFF_LINEAR:
      strength *= sd->ray_length;
      break;
    case NODE_LIGHT_FALLOFF_CONSTANT:
      strength *= sd->ray_length * sd->ray_length;
      break;
  }

  float smooth = stack_load_float(stack, smooth_offset);

  if (smooth > 0.0f) {
    float squared = sd->ray_length * sd->ray_length;
    /* Distant lamps set the ray length to FLT_MAX, which causes squared to overflow. */
    if (isfinite(squared)) {
      strength *= squared / (smooth + squared);
    }
  }

  stack_store_float(stack, out_offset, strength);
}

CCL_NAMESPACE_END
