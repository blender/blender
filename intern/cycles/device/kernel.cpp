/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "device/kernel.h"

#ifndef __KERNEL_ONEAPI__
#  include "util/log.h"
#endif

CCL_NAMESPACE_BEGIN

bool device_kernel_has_shading(DeviceKernel kernel)
{
  return (kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_DEDICATED_LIGHT ||
          kernel == DEVICE_KERNEL_SHADER_EVAL_DISPLACE ||
          kernel == DEVICE_KERNEL_SHADER_EVAL_BACKGROUND ||
          kernel == DEVICE_KERNEL_SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY);
}

bool device_kernel_has_intersection(DeviceKernel kernel)
{
  return (kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST ||
          kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW ||
          kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE ||
          kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK ||
          kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_DEDICATED_LIGHT ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE);
}

const char *device_kernel_as_string(DeviceKernel kernel)
{
  switch (kernel) {
    /* Integrator. */
    case DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA:
      return "integrator_init_from_camera";
    case DEVICE_KERNEL_INTEGRATOR_INIT_FROM_BAKE:
      return "integrator_init_from_bake";
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST:
      return "integrator_intersect_closest";
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW:
      return "integrator_intersect_shadow";
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE:
      return "integrator_intersect_subsurface";
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK:
      return "integrator_intersect_volume_stack";
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_DEDICATED_LIGHT:
      return "integrator_intersect_dedicated_light";
    case DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND:
      return "integrator_shade_background";
    case DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT:
      return "integrator_shade_light";
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW:
      return "integrator_shade_shadow";
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE:
      return "integrator_shade_surface";
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE:
      return "integrator_shade_surface_raytrace";
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE:
      return "integrator_shade_surface_mnee";
    case DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME:
      return "integrator_shade_volume";
    case DEVICE_KERNEL_INTEGRATOR_SHADE_DEDICATED_LIGHT:
      return "integrator_shade_dedicated_light";
    case DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL:
      return "integrator_megakernel";
    case DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY:
      return "integrator_queued_paths_array";
    case DEVICE_KERNEL_INTEGRATOR_QUEUED_SHADOW_PATHS_ARRAY:
      return "integrator_queued_shadow_paths_array";
    case DEVICE_KERNEL_INTEGRATOR_ACTIVE_PATHS_ARRAY:
      return "integrator_active_paths_array";
    case DEVICE_KERNEL_INTEGRATOR_TERMINATED_PATHS_ARRAY:
      return "integrator_terminated_paths_array";
    case DEVICE_KERNEL_INTEGRATOR_SORTED_PATHS_ARRAY:
      return "integrator_sorted_paths_array";
    case DEVICE_KERNEL_INTEGRATOR_SORT_BUCKET_PASS:
      return "integrator_sort_bucket_pass";
    case DEVICE_KERNEL_INTEGRATOR_SORT_WRITE_PASS:
      return "integrator_sort_write_pass";
    case DEVICE_KERNEL_INTEGRATOR_COMPACT_PATHS_ARRAY:
      return "integrator_compact_paths_array";
    case DEVICE_KERNEL_INTEGRATOR_COMPACT_STATES:
      return "integrator_compact_states";
    case DEVICE_KERNEL_INTEGRATOR_TERMINATED_SHADOW_PATHS_ARRAY:
      return "integrator_terminated_shadow_paths_array";
    case DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_PATHS_ARRAY:
      return "integrator_compact_shadow_paths_array";
    case DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_STATES:
      return "integrator_compact_shadow_states";
    case DEVICE_KERNEL_INTEGRATOR_RESET:
      return "integrator_reset";
    case DEVICE_KERNEL_INTEGRATOR_SHADOW_CATCHER_COUNT_POSSIBLE_SPLITS:
      return "integrator_shadow_catcher_count_possible_splits";

    /* Shader evaluation. */
    case DEVICE_KERNEL_SHADER_EVAL_DISPLACE:
      return "shader_eval_displace";
    case DEVICE_KERNEL_SHADER_EVAL_BACKGROUND:
      return "shader_eval_background";
    case DEVICE_KERNEL_SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY:
      return "shader_eval_curve_shadow_transparency";

      /* Film. */

#define FILM_CONVERT_KERNEL_AS_STRING(variant, variant_lowercase) \
  case DEVICE_KERNEL_FILM_CONVERT_##variant: \
    return "film_convert_" #variant_lowercase; \
  case DEVICE_KERNEL_FILM_CONVERT_##variant##_HALF_RGBA: \
    return "film_convert_" #variant_lowercase "_half_rgba";

      FILM_CONVERT_KERNEL_AS_STRING(DEPTH, depth)
      FILM_CONVERT_KERNEL_AS_STRING(MIST, mist)
      FILM_CONVERT_KERNEL_AS_STRING(SAMPLE_COUNT, sample_count)
      FILM_CONVERT_KERNEL_AS_STRING(FLOAT, float)
      FILM_CONVERT_KERNEL_AS_STRING(LIGHT_PATH, light_path)
      FILM_CONVERT_KERNEL_AS_STRING(FLOAT3, float3)
      FILM_CONVERT_KERNEL_AS_STRING(MOTION, motion)
      FILM_CONVERT_KERNEL_AS_STRING(CRYPTOMATTE, cryptomatte)
      FILM_CONVERT_KERNEL_AS_STRING(SHADOW_CATCHER, shadow_catcher)
      FILM_CONVERT_KERNEL_AS_STRING(SHADOW_CATCHER_MATTE_WITH_SHADOW,
                                    shadow_catcher_matte_with_shadow)
      FILM_CONVERT_KERNEL_AS_STRING(COMBINED, combined)
      FILM_CONVERT_KERNEL_AS_STRING(FLOAT4, float4)

#undef FILM_CONVERT_KERNEL_AS_STRING

    /* Adaptive sampling. */
    case DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_CHECK:
      return "adaptive_sampling_convergence_check";
    case DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_FILTER_X:
      return "adaptive_sampling_filter_x";
    case DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_FILTER_Y:
      return "adaptive_sampling_filter_y";

    /* Denoising. */
    case DEVICE_KERNEL_FILTER_GUIDING_PREPROCESS:
      return "filter_guiding_preprocess";
    case DEVICE_KERNEL_FILTER_GUIDING_SET_FAKE_ALBEDO:
      return "filter_guiding_set_fake_albedo";
    case DEVICE_KERNEL_FILTER_COLOR_PREPROCESS:
      return "filter_color_preprocess";
    case DEVICE_KERNEL_FILTER_COLOR_POSTPROCESS:
      return "filter_color_postprocess";

    /* Cryptomatte. */
    case DEVICE_KERNEL_CRYPTOMATTE_POSTPROCESS:
      return "cryptomatte_postprocess";

    /* Generic */
    case DEVICE_KERNEL_PREFIX_SUM:
      return "prefix_sum";

    case DEVICE_KERNEL_NUM:
      break;
  };
#ifndef __KERNEL_ONEAPI__
  LOG(FATAL) << "Unhandled kernel " << static_cast<int>(kernel) << ", should never happen.";
#endif
  return "UNKNOWN";
}

#ifndef __KERNEL_ONEAPI__
std::ostream &operator<<(std::ostream &os, DeviceKernel kernel)
{
  os << device_kernel_as_string(kernel);
  return os;
}

string device_kernel_mask_as_string(DeviceKernelMask mask)
{
  string str;

  for (uint64_t i = 0; i < sizeof(DeviceKernelMask) * 8; i++) {
    if (mask & (uint64_t(1) << i)) {
      if (!str.empty()) {
        str += " ";
      }
      str += device_kernel_as_string((DeviceKernel)i);
    }
  }

  return str;
}
#endif

CCL_NAMESPACE_END
