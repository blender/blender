/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

#ifdef __VOLUME__

/* Volume Stack
 *
 * This is an array of object/shared ID's that the current segment of the path
 * is inside of. */

template<const bool shadow, typename IntegratorGenericState>
ccl_device_forceinline VolumeStack volume_stack_read(const IntegratorGenericState state,
                                                     const int i)
{
  if constexpr (shadow) {
    return integrator_state_read_shadow_volume_stack(state, i);
  }
  else {
    return integrator_state_read_volume_stack(state, i);
  }

#  ifdef __KERNEL_GPU__
  /* Silence false positive warning with some GPU compilers. */
  VolumeStack stack = {};
  return stack;
#  endif
}

template<const bool shadow, typename IntegratorGenericState>
ccl_device_forceinline void volume_stack_write(IntegratorGenericState state,
                                               const int i,
                                               const VolumeStack entry)
{
  if constexpr (shadow) {
    integrator_state_write_shadow_volume_stack(state, i, entry);
  }
  else {
    integrator_state_write_volume_stack(state, i, entry);
  }
}

template<const bool shadow, typename IntegratorGenericState>
ccl_device void volume_stack_enter_exit(KernelGlobals kg,
                                        IntegratorGenericState state,
                                        const ccl_private ShaderData *sd)
{
#  ifdef __KERNEL_USE_DATA_CONSTANTS__
  /* If we're using data constants, this fetch disappears.
   * On Apple GPUs, scenes without volumetric features can render 1 or 2% faster by dead-stripping
   * this function. */
  if (!(kernel_data.kernel_features & KERNEL_FEATURE_VOLUME)) {
    return;
  }
#  endif

  /* todo: we should have some way for objects to indicate if they want the
   * world shader to work inside them. excluding it by default is problematic
   * because non-volume objects can't be assumed to be closed manifolds */
  if (!(sd->flag & SD_HAS_VOLUME)) {
    return;
  }

  if (sd->flag & SD_BACKFACING) {
    /* Exit volume object: remove from stack. */
    for (int i = 0;; i++) {
      VolumeStack entry = volume_stack_read<shadow>(state, i);
      if (entry.shader == SHADER_NONE) {
        break;
      }

      if (entry.object == sd->object && entry.shader == sd->shader) {
        /* Shift back next stack entries. */
        do {
          entry = volume_stack_read<shadow>(state, i + 1);
          volume_stack_write<shadow>(state, i, entry);
          i++;
        } while (entry.shader != SHADER_NONE);

        return;
      }
    }
  }
  else {
    /* Enter volume object: add to stack. */
    uint i;
    for (i = 0;; i++) {
      const VolumeStack entry = volume_stack_read<shadow>(state, i);
      if (entry.shader == SHADER_NONE) {
        break;
      }

      /* Already in the stack? then we have nothing to do. */
      if (entry.object == sd->object && entry.shader == sd->shader) {
        return;
      }
    }

    /* If we exceed the stack limit, ignore. */
    if (i >= kernel_data.volume_stack_size - 1) {
      return;
    }

    /* Add to the end of the stack. */
    const VolumeStack new_entry = {sd->object, sd->shader};
    const VolumeStack empty_entry = {OBJECT_NONE, SHADER_NONE};
    volume_stack_write<shadow>(state, i, new_entry);
    volume_stack_write<shadow>(state, i + 1, empty_entry);
  }
}

/* Clean stack after the last bounce.
 *
 * It is expected that all volumes are closed manifolds, so at the time when ray
 * hits nothing (for example, it is a last bounce which goes to environment) the
 * only expected volume in the stack is the world's one. All the rest volume
 * entries should have been exited already.
 *
 * This isn't always true because of ray intersection precision issues, which
 * could lead us to an infinite non-world volume in the stack, causing render
 * artifacts.
 *
 * Use this function after the last bounce to get rid of all volumes apart from
 * the world's one after the last bounce to avoid render artifacts.
 */
ccl_device_inline void volume_stack_clean(KernelGlobals kg, IntegratorState state)
{
  if (kernel_data.background.volume_shader != SHADER_NONE) {
    /* Keep the world's volume in stack. */
    INTEGRATOR_STATE_ARRAY_WRITE(state, volume_stack, 1, shader) = SHADER_NONE;
  }
  else {
    INTEGRATOR_STATE_ARRAY_WRITE(state, volume_stack, 0, shader) = SHADER_NONE;
  }
}

/* Check if the volume is homogeneous by checking if the shader flag is set or if volume attributes
 * are needed. */
ccl_device_inline bool volume_is_homogeneous(KernelGlobals kg,
                                             const ccl_private VolumeStack &entry)
{
  const int shader_flag = kernel_data_fetch(shaders, (entry.shader & SHADER_MASK)).flags;

  if (shader_flag & SD_HETEROGENEOUS_VOLUME) {
    return false;
  }

  if (shader_flag & SD_NEED_VOLUME_ATTRIBUTES) {
    const int object = entry.object;
    if (object == kernel_data.background.object_index) {
      /* Volume attributes for world is not supported. */
      return true;
    }

    const int object_flag = kernel_data_fetch(object_flag, object);
    if (object_flag & SD_OBJECT_HAS_VOLUME_ATTRIBUTES) {
      /* If both the shader and the object needs volume attributes, the volume is heterogeneous. */
      return false;
    }
  }

  return true;
}

template<const bool shadow, typename IntegratorGenericState>
ccl_device_inline bool volume_is_homogeneous(KernelGlobals kg, const IntegratorGenericState state)
{
  for (int i = 0;; i++) {
    const VolumeStack entry = volume_stack_read<shadow>(state, i);

    if (entry.shader == SHADER_NONE) {
      return true;
    }

    if (!volume_is_homogeneous(kg, entry)) {
      return false;
    }
  }

  kernel_assert(false);
  return false;
}

template<const bool shadow, typename IntegratorGenericState>
ccl_device float volume_stack_step_size(KernelGlobals kg, const IntegratorGenericState state)
{
  kernel_assert(kernel_data.integrator.volume_ray_marching);

  float step_size = FLT_MAX;

  for (int i = 0;; i++) {
    const VolumeStack entry = volume_stack_read<shadow>(state, i);
    if (entry.shader == SHADER_NONE) {
      break;
    }

    if (!volume_is_homogeneous(kg, entry)) {
      const float object_step_size = kernel_data_fetch(volume_step_size, entry.object);
      step_size = fminf(object_step_size, step_size);
    }
  }

  return step_size;
}

enum VolumeSampleMethod {
  VOLUME_SAMPLE_NONE = 0,
  VOLUME_SAMPLE_DISTANCE = (1 << 0),
  VOLUME_SAMPLE_EQUIANGULAR = (1 << 1),
  VOLUME_SAMPLE_MIS = (VOLUME_SAMPLE_DISTANCE | VOLUME_SAMPLE_EQUIANGULAR),
};

ccl_device VolumeSampleMethod volume_stack_sample_method(KernelGlobals kg, IntegratorState state)
{
  VolumeSampleMethod method = VOLUME_SAMPLE_NONE;

  for (int i = 0;; i++) {
    VolumeStack entry = integrator_state_read_volume_stack(state, i);
    if (entry.shader == SHADER_NONE) {
      break;
    }

    int shader_flag = kernel_data_fetch(shaders, (entry.shader & SHADER_MASK)).flags;

    if (shader_flag & SD_VOLUME_MIS) {
      /* Multiple importance sampling. */
      return VOLUME_SAMPLE_MIS;
    }
    else if (shader_flag & SD_VOLUME_EQUIANGULAR) {
      /* Distance + equiangular sampling -> multiple importance sampling. */
      if (method == VOLUME_SAMPLE_DISTANCE) {
        return VOLUME_SAMPLE_MIS;
      }

      /* Only equiangular sampling. */
      method = VOLUME_SAMPLE_EQUIANGULAR;
    }
    else {
      /* Distance + equiangular sampling -> multiple importance sampling. */
      if (method == VOLUME_SAMPLE_EQUIANGULAR) {
        return VOLUME_SAMPLE_MIS;
      }

      /* Distance sampling only. */
      method = VOLUME_SAMPLE_DISTANCE;
    }
  }

  return method;
}

#endif /* __VOLUME__*/

CCL_NAMESPACE_END
