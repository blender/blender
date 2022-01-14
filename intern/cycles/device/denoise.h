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

#pragma once

#include "device/memory.h"
#include "graph/node.h"
#include "session/buffers.h"

CCL_NAMESPACE_BEGIN

enum DenoiserType {
  DENOISER_OPTIX = 2,
  DENOISER_OPENIMAGEDENOISE = 4,
  DENOISER_NUM,

  DENOISER_NONE = 0,
  DENOISER_ALL = ~0,
};

/* COnstruct human-readable string which denotes the denoiser type. */
const char *denoiserTypeToHumanReadable(DenoiserType type);

typedef int DenoiserTypeMask;

enum DenoiserPrefilter {
  /* Best quality of the result without extra processing time, but requires guiding passes to be
   * noise-free. */
  DENOISER_PREFILTER_NONE = 1,

  /* Denoise color and guiding passes together.
   * Improves quality when guiding passes are noisy using least amount of extra processing time. */
  DENOISER_PREFILTER_FAST = 2,

  /* Prefilter noisy guiding passes before denoising color.
   * Improves quality when guiding passes are noisy using extra processing time. */
  DENOISER_PREFILTER_ACCURATE = 3,

  DENOISER_PREFILTER_NUM,
};

/* NOTE: Is not a real scene node. Using Node API for ease of (de)serialization.
 * The default values here do not really matter as they are always initialized from the
 * Integrator node. */
class DenoiseParams : public Node {
 public:
  NODE_DECLARE

  /* Apply denoiser to image. */
  bool use = false;

  /* Denoiser type. */
  DenoiserType type = DENOISER_OPENIMAGEDENOISE;

  /* Viewport start sample. */
  int start_sample = 0;

  /* Auxiliary passes. */
  bool use_pass_albedo = true;
  bool use_pass_normal = true;

  /* Configure the denoiser to use motion vectors, previous image and a temporally stable model. */
  bool temporally_stable = false;

  DenoiserPrefilter prefilter = DENOISER_PREFILTER_FAST;

  static const NodeEnum *get_type_enum();
  static const NodeEnum *get_prefilter_enum();

  DenoiseParams();

  bool modified(const DenoiseParams &other) const
  {
    return !(use == other.use && type == other.type && start_sample == other.start_sample &&
             use_pass_albedo == other.use_pass_albedo &&
             use_pass_normal == other.use_pass_normal &&
             temporally_stable == other.temporally_stable && prefilter == other.prefilter);
  }
};

/* All the parameters needed to perform buffer denoising on a device.
 * Is not really a task in its canonical terms (as in, is not an asynchronous running task). Is
 * more like a wrapper for all the arguments and parameters needed to perform denoising. Is a
 * single place where they are all listed, so that it's not required to modify all device methods
 * when these parameters do change. */
class DeviceDenoiseTask {
 public:
  DenoiseParams params;

  int num_samples;

  RenderBuffers *render_buffers;
  BufferParams buffer_params;

  /* Allow to do in-place modification of the input passes (scaling them down i.e.). This will
   * lower the memory footprint of the denoiser but will make input passes "invalid" (from path
   * tracer) point of view. */
  bool allow_inplace_modification;
};

CCL_NAMESPACE_END
