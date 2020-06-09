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

#include <stdlib.h>
#include <string.h>

#include "device/device_task.h"

#include "render/buffers.h"

#include "util/util_algorithm.h"
#include "util/util_time.h"

CCL_NAMESPACE_BEGIN

/* Device Task */

DeviceTask::DeviceTask(Type type_)
    : type(type_),
      x(0),
      y(0),
      w(0),
      h(0),
      rgba_byte(0),
      rgba_half(0),
      buffer(0),
      sample(0),
      num_samples(1),
      shader_input(0),
      shader_output(0),
      shader_eval_type(0),
      shader_filter(0),
      shader_x(0),
      shader_w(0),
      buffers(nullptr)
{
  last_update_time = time_dt();
}

int DeviceTask::get_subtask_count(int num, int max_size)
{
  if (max_size != 0) {
    int max_size_num;

    if (type == SHADER) {
      max_size_num = (shader_w + max_size - 1) / max_size;
    }
    else {
      max_size = max(1, max_size / w);
      max_size_num = (h + max_size - 1) / max_size;
    }

    num = max(max_size_num, num);
  }

  if (type == SHADER) {
    num = min(shader_w, num);
  }
  else if (type == RENDER) {
  }
  else {
    num = min(h, num);
  }

  return num;
}

void DeviceTask::split(list<DeviceTask> &tasks, int num, int max_size)
{
  num = get_subtask_count(num, max_size);

  if (type == SHADER) {
    for (int i = 0; i < num; i++) {
      int tx = shader_x + (shader_w / num) * i;
      int tw = (i == num - 1) ? shader_w - i * (shader_w / num) : shader_w / num;

      DeviceTask task = *this;

      task.shader_x = tx;
      task.shader_w = tw;

      tasks.push_back(task);
    }
  }
  else if (type == RENDER) {
    for (int i = 0; i < num; i++)
      tasks.push_back(*this);
  }
  else {
    for (int i = 0; i < num; i++) {
      int ty = y + (h / num) * i;
      int th = (i == num - 1) ? h - i * (h / num) : h / num;

      DeviceTask task = *this;

      task.y = ty;
      task.h = th;

      tasks.push_back(task);
    }
  }
}

void DeviceTask::update_progress(RenderTile *rtile, int pixel_samples)
{
  if (type == FILM_CONVERT)
    return;

  if (update_progress_sample) {
    if (pixel_samples == -1) {
      pixel_samples = shader_w;
    }
    update_progress_sample(pixel_samples, rtile ? rtile->sample : 0);
  }

  if (update_tile_sample) {
    double current_time = time_dt();

    if (current_time - last_update_time >= 1.0) {
      update_tile_sample(*rtile);

      last_update_time = current_time;
    }
  }
}

/* Adaptive Sampling */

AdaptiveSampling::AdaptiveSampling() : use(true), adaptive_step(0), min_samples(0)
{
}

/* Render samples in steps that align with the adaptive filtering. */
int AdaptiveSampling::align_static_samples(int samples) const
{
  if (samples > adaptive_step) {
    /* Make multiple of adaptive_step. */
    while (samples % adaptive_step != 0) {
      samples--;
    }
  }
  else if (samples < adaptive_step) {
    /* Make divisor of adaptive_step. */
    while (adaptive_step % samples != 0) {
      samples--;
    }
  }

  return max(samples, 1);
}

/* Render samples in steps that align with the adaptive filtering, with the
 * suggested number of samples dynamically changing. */
int AdaptiveSampling::align_dynamic_samples(int offset, int samples) const
{
  /* Round so that we end up on multiples of adaptive_samples. */
  samples += offset;

  if (samples > adaptive_step) {
    /* Make multiple of adaptive_step. */
    while (samples % adaptive_step != 0) {
      samples--;
    }
  }

  samples -= offset;

  return max(samples, 1);
}

bool AdaptiveSampling::need_filter(int sample) const
{
  if (sample > min_samples) {
    return (sample & (adaptive_step - 1)) == (adaptive_step - 1);
  }
  else {
    return false;
  }
}

CCL_NAMESPACE_END
