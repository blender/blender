/*
 * Copyright 2011-2021 Blender Foundation
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

#pragma once

CCL_NAMESPACE_BEGIN

/* Volume Stack
 *
 * This is an array of object/shared ID's that the current segment of the path
 * is inside of. */

template<typename StackReadOp, typename StackWriteOp>
ccl_device void volume_stack_enter_exit(KernelGlobals kg,
                                        ccl_private const ShaderData *sd,
                                        StackReadOp stack_read,
                                        StackWriteOp stack_write)
{
  /* todo: we should have some way for objects to indicate if they want the
   * world shader to work inside them. excluding it by default is problematic
   * because non-volume objects can't be assumed to be closed manifolds */
  if (!(sd->flag & SD_HAS_VOLUME)) {
    return;
  }

  if (sd->flag & SD_BACKFACING) {
    /* Exit volume object: remove from stack. */
    for (int i = 0;; i++) {
      VolumeStack entry = stack_read(i);
      if (entry.shader == SHADER_NONE) {
        break;
      }

      if (entry.object == sd->object) {
        /* Shift back next stack entries. */
        do {
          entry = stack_read(i + 1);
          stack_write(i, entry);
          i++;
        } while (entry.shader != SHADER_NONE);

        return;
      }
    }
  }
  else {
    /* Enter volume object: add to stack. */
    int i;
    for (i = 0;; i++) {
      VolumeStack entry = stack_read(i);
      if (entry.shader == SHADER_NONE) {
        break;
      }

      /* Already in the stack? then we have nothing to do. */
      if (entry.object == sd->object) {
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
    stack_write(i, new_entry);
    stack_write(i + 1, empty_entry);
  }
}

ccl_device void volume_stack_enter_exit(KernelGlobals kg,
                                        IntegratorState state,
                                        ccl_private const ShaderData *sd)
{
  volume_stack_enter_exit(
      kg,
      sd,
      [=](const int i) { return integrator_state_read_volume_stack(state, i); },
      [=](const int i, const VolumeStack entry) {
        integrator_state_write_volume_stack(state, i, entry);
      });
}

ccl_device void shadow_volume_stack_enter_exit(KernelGlobals kg,
                                               IntegratorState state,
                                               ccl_private const ShaderData *sd)
{
  volume_stack_enter_exit(
      kg,
      sd,
      [=](const int i) { return integrator_state_read_shadow_volume_stack(state, i); },
      [=](const int i, const VolumeStack entry) {
        integrator_state_write_shadow_volume_stack(state, i, entry);
      });
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

template<typename StackReadOp>
ccl_device float volume_stack_step_size(KernelGlobals kg,
                                        IntegratorState state,
                                        StackReadOp stack_read)
{
  float step_size = FLT_MAX;

  for (int i = 0;; i++) {
    VolumeStack entry = stack_read(i);
    if (entry.shader == SHADER_NONE) {
      break;
    }

    int shader_flag = kernel_tex_fetch(__shaders, (entry.shader & SHADER_MASK)).flags;

    bool heterogeneous = false;

    if (shader_flag & SD_HETEROGENEOUS_VOLUME) {
      heterogeneous = true;
    }
    else if (shader_flag & SD_NEED_VOLUME_ATTRIBUTES) {
      /* We want to render world or objects without any volume grids
       * as homogeneous, but can only verify this at run-time since other
       * heterogeneous volume objects may be using the same shader. */
      int object = entry.object;
      if (object != OBJECT_NONE) {
        int object_flag = kernel_tex_fetch(__object_flag, object);
        if (object_flag & SD_OBJECT_HAS_VOLUME_ATTRIBUTES) {
          heterogeneous = true;
        }
      }
    }

    if (heterogeneous) {
      float object_step_size = object_volume_step_size(kg, entry.object);
      object_step_size *= kernel_data.integrator.volume_step_rate;
      step_size = fminf(object_step_size, step_size);
    }
  }

  return step_size;
}

typedef enum VolumeSampleMethod {
  VOLUME_SAMPLE_NONE = 0,
  VOLUME_SAMPLE_DISTANCE = (1 << 0),
  VOLUME_SAMPLE_EQUIANGULAR = (1 << 1),
  VOLUME_SAMPLE_MIS = (VOLUME_SAMPLE_DISTANCE | VOLUME_SAMPLE_EQUIANGULAR),
} VolumeSampleMethod;

ccl_device VolumeSampleMethod volume_stack_sample_method(KernelGlobals kg, IntegratorState state)
{
  VolumeSampleMethod method = VOLUME_SAMPLE_NONE;

  for (int i = 0;; i++) {
    VolumeStack entry = integrator_state_read_volume_stack(state, i);
    if (entry.shader == SHADER_NONE) {
      break;
    }

    int shader_flag = kernel_tex_fetch(__shaders, (entry.shader & SHADER_MASK)).flags;

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

CCL_NAMESPACE_END
