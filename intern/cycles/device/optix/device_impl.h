/*
 * Copyright 2019, NVIDIA Corporation.
 * Copyright 2019, Blender Foundation.
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

#ifdef WITH_OPTIX

#  include "device/cuda/device_impl.h"
#  include "device/optix/queue.h"
#  include "device/optix/util.h"
#  include "kernel/types.h"
#  include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

class BVHOptiX;
struct KernelParamsOptiX;

/* List of OptiX program groups. */
enum {
  PG_RGEN_INTERSECT_CLOSEST,
  PG_RGEN_INTERSECT_SHADOW,
  PG_RGEN_INTERSECT_SUBSURFACE,
  PG_RGEN_INTERSECT_VOLUME_STACK,
  PG_RGEN_SHADE_SURFACE_RAYTRACE,
  PG_MISS,
  PG_HITD, /* Default hit group. */
  PG_HITS, /* __SHADOW_RECORD_ALL__ hit group. */
  PG_HITL, /* __BVH_LOCAL__ hit group (only used for triangles). */
  PG_HITV, /* __VOLUME__ hit group. */
  PG_HITD_MOTION,
  PG_HITS_MOTION,
  PG_HITD_POINTCLOUD,
  PG_HITS_POINTCLOUD,
  PG_CALL_SVM_AO,
  PG_CALL_SVM_BEVEL,
  NUM_PROGRAM_GROUPS
};

static const int MISS_PROGRAM_GROUP_OFFSET = PG_MISS;
static const int NUM_MIS_PROGRAM_GROUPS = 1;
static const int HIT_PROGAM_GROUP_OFFSET = PG_HITD;
static const int NUM_HIT_PROGRAM_GROUPS = 8;
static const int CALLABLE_PROGRAM_GROUPS_BASE = PG_CALL_SVM_AO;
static const int NUM_CALLABLE_PROGRAM_GROUPS = 2;

/* List of OptiX pipelines. */
enum { PIP_SHADE_RAYTRACE, PIP_INTERSECT, NUM_PIPELINES };

/* A single shader binding table entry. */
struct SbtRecord {
  char header[OPTIX_SBT_RECORD_HEADER_SIZE];
};

class OptiXDevice : public CUDADevice {
 public:
  OptixDeviceContext context = NULL;

  OptixModule optix_module = NULL; /* All necessary OptiX kernels are in one module. */
  OptixModule builtin_modules[2] = {};
  OptixPipeline pipelines[NUM_PIPELINES] = {};

  bool motion_blur = false;
  device_vector<SbtRecord> sbt_data;
  device_only_memory<KernelParamsOptiX> launch_params;
  OptixTraversableHandle tlas_handle = 0;

  vector<unique_ptr<device_only_memory<char>>> delayed_free_bvh_memory;
  thread_mutex delayed_free_bvh_mutex;

  class Denoiser {
   public:
    explicit Denoiser(OptiXDevice *device);

    OptiXDevice *device;
    OptiXDeviceQueue queue;

    OptixDenoiser optix_denoiser = nullptr;

    /* Configuration size, as provided to `optixDenoiserSetup`.
     * If the `optixDenoiserSetup()` was never used on the current `optix_denoiser` the
     * `is_configured` will be false. */
    bool is_configured = false;
    int2 configured_size = make_int2(0, 0);

    /* OptiX denoiser state and scratch buffers, stored in a single memory buffer.
     * The memory layout goes as following: [denoiser state][scratch buffer]. */
    device_only_memory<unsigned char> state;
    OptixDenoiserSizes sizes = {};

    bool use_pass_albedo = false;
    bool use_pass_normal = false;
    bool use_pass_flow = false;
  };
  Denoiser denoiser_;

 public:
  OptiXDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler);
  ~OptiXDevice();

 private:
  BVHLayoutMask get_bvh_layout_mask() const override;

  string compile_kernel_get_common_cflags(const uint kernel_features) override;

  bool load_kernels(const uint kernel_features) override;

  bool build_optix_bvh(BVHOptiX *bvh,
                       OptixBuildOperation operation,
                       const OptixBuildInput &build_input,
                       uint16_t num_motion_steps);

  void build_bvh(BVH *bvh, Progress &progress, bool refit) override;

  void release_optix_bvh(BVH *bvh) override;
  void free_bvh_memory_delayed();

  void const_copy_to(const char *name, void *host, size_t size) override;

  void update_launch_params(size_t offset, void *data, size_t data_size);

  virtual unique_ptr<DeviceQueue> gpu_queue_create() override;

  /* --------------------------------------------------------------------
   * Denoising.
   */

  class DenoiseContext;
  class DenoisePass;

  virtual bool denoise_buffer(const DeviceDenoiseTask &task) override;
  virtual DeviceQueue *get_denoise_queue() override;

  /* Read guiding passes from the render buffers, preprocess them in a way which is expected by
   * OptiX and store in the guiding passes memory within the given context.
   *
   * Pre=-processing of the guiding passes is to only happen once per context lifetime. DO not
   * preprocess them for every pass which is being denoised. */
  bool denoise_filter_guiding_preprocess(DenoiseContext &context);

  /* Set fake albedo pixels in the albedo guiding pass storage.
   * After this point only passes which do not need albedo for denoising can be processed. */
  bool denoise_filter_guiding_set_fake_albedo(DenoiseContext &context);

  void denoise_pass(DenoiseContext &context, PassType pass_type);

  /* Read input color pass from the render buffer into the memory which corresponds to the noisy
   * input within the given context. Pixels are scaled to the number of samples, but are not
   * preprocessed yet. */
  void denoise_color_read(DenoiseContext &context, const DenoisePass &pass);

  /* Run corresponding filter kernels, preparing data for the denoiser or copying data from the
   * denoiser result to the render buffer. */
  bool denoise_filter_color_preprocess(DenoiseContext &context, const DenoisePass &pass);
  bool denoise_filter_color_postprocess(DenoiseContext &context, const DenoisePass &pass);

  /* Make sure the OptiX denoiser is created and configured. */
  bool denoise_ensure(DenoiseContext &context);

  /* Create OptiX denoiser descriptor if needed.
   * Will do nothing if the current OptiX descriptor is usable for the given parameters.
   * If the OptiX denoiser descriptor did re-allocate here it is left unconfigured. */
  bool denoise_create_if_needed(DenoiseContext &context);

  /* Configure existing OptiX denoiser descriptor for the use for the given task. */
  bool denoise_configure_if_needed(DenoiseContext &context);

  /* Run configured denoiser. */
  bool denoise_run(DenoiseContext &context, const DenoisePass &pass);
};

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */
