/* SPDX-FileCopyrightText: 2019 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2019-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OPTIX

#  include "device/cuda/device_impl.h"
#  include "device/optix/util.h"  // IWYU pragma: keep
#  include "kernel/osl/globals.h"

#  include "util/task.h"

CCL_NAMESPACE_BEGIN

class BVHOptiX;
struct KernelParamsOptiX;

/* List of OptiX program groups. */
enum {
  /* Ray generation */
  PG_RGEN_INTERSECT_CLOSEST,
  PG_RGEN_INTERSECT_SHADOW,
  PG_RGEN_INTERSECT_SUBSURFACE,
  PG_RGEN_INTERSECT_VOLUME_STACK,
  PG_RGEN_INTERSECT_DEDICATED_LIGHT,
  PG_RGEN_SHADE_BACKGROUND,
  PG_RGEN_SHADE_LIGHT,
  PG_RGEN_SHADE_SURFACE,
  PG_RGEN_SHADE_SURFACE_RAYTRACE,
  PG_RGEN_SHADE_SURFACE_MNEE,
  PG_RGEN_SHADE_VOLUME,
  PG_RGEN_SHADE_VOLUME_RAY_MARCHING,
  PG_RGEN_SHADE_SHADOW,
  PG_RGEN_SHADE_DEDICATED_LIGHT,
  PG_RGEN_EVAL_DISPLACE,
  PG_RGEN_EVAL_BACKGROUND,
  PG_RGEN_EVAL_CURVE_SHADOW_TRANSPARENCY,
  PG_RGEN_INIT_FROM_CAMERA,
  PG_RGEN_EVAL_VOLUME_DENSITY,

  /* Miss */
  PG_MISS,

  /* Hit */
  PG_HITD, /* Default hit group. */
  PG_HITS, /* __SHADOW_RECORD_ALL__ hit group. */
  PG_HITL, /* __BVH_LOCAL__ hit group (only used for triangles). */
  PG_HITV, /* __VOLUME__ hit group. */
  PG_HITD_MOTION,
  PG_HITS_MOTION,
  PG_HITL_MOTION,
  PG_HITV_MOTION,
  PG_HITD_CURVE_LINEAR,
  PG_HITS_CURVE_LINEAR,
  PG_HITV_CURVE_LINEAR,
  PG_HITL_CURVE_LINEAR,
  PG_HITD_CURVE_LINEAR_MOTION,
  PG_HITS_CURVE_LINEAR_MOTION,
  PG_HITV_CURVE_LINEAR_MOTION,
  PG_HITL_CURVE_LINEAR_MOTION,
  PG_HITD_CURVE_RIBBON,
  PG_HITS_CURVE_RIBBON,
  PG_HITV_CURVE_RIBBON,
  PG_HITL_CURVE_RIBBON,
  PG_HITD_POINTCLOUD,
  PG_HITS_POINTCLOUD,
  PG_HITV_POINTCLOUD,
  PG_HITL_POINTCLOUD,

  /* Callable */
  PG_CALL_SVM_AO,
  PG_CALL_SVM_BEVEL,

  NUM_PROGRAM_GROUPS
};

static const int MISS_PROGRAM_GROUP_OFFSET = PG_MISS;
static const int NUM_MISS_PROGRAM_GROUPS = 1;
static const int HIT_PROGAM_GROUP_OFFSET = PG_HITD;
static const int NUM_HIT_PROGRAM_GROUPS = 24;
static const int CALLABLE_PROGRAM_GROUPS_BASE = PG_CALL_SVM_AO;
static const int NUM_CALLABLE_PROGRAM_GROUPS = 2;

/* List of OptiX pipelines. */
enum { PIP_SHADE, PIP_INTERSECT, NUM_PIPELINES };

/* A single shader binding table entry. */
struct SbtRecord {
  char header[OPTIX_SBT_RECORD_HEADER_SIZE];
};

class OptiXDevice : public CUDADevice {
 public:
  OptixDeviceContext context = nullptr;

  OptixModule optix_module = nullptr; /* All necessary OptiX kernels are in one module. */
  OptixModule builtin_modules[4] = {};
  OptixPipeline pipelines[NUM_PIPELINES] = {};
  OptixProgramGroup groups[NUM_PROGRAM_GROUPS] = {};
  OptixPipelineCompileOptions pipeline_options = {};

#  ifdef WITH_OSL
  OSLGlobals osl_globals;
  vector<OptixModule> osl_modules;
  vector<OptixProgramGroup> osl_groups;
  OptixModule osl_camera_module = nullptr;
  device_vector<uint8_t> osl_colorsystem;
#  endif

  device_vector<SbtRecord> sbt_data;
  device_only_memory<KernelParamsOptiX> launch_params;

 private:
  OptixTraversableHandle tlas_handle = 0;
  vector<unique_ptr<device_only_memory<char>>> delayed_free_bvh_memory;
  thread_mutex delayed_free_bvh_mutex;

 public:
  OptiXDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler, bool headless);
  ~OptiXDevice() override;

  BVHLayoutMask get_bvh_layout_mask(uint /*kernel_features*/) const override;

  string compile_kernel_get_common_cflags(const uint kernel_features);

  void create_optix_module(TaskPool &pool,
                           OptixModuleCompileOptions &module_options,
                           string &ptx_data,
                           OptixModule &module,
                           OptixResult &failure_reason);

  bool load_kernels(const uint kernel_features) override;

  bool load_osl_kernels() override;

  bool build_optix_bvh(BVHOptiX *bvh,
                       OptixBuildOperation operation,
                       const OptixBuildInput &build_input,
                       uint16_t num_motion_steps);

  void build_bvh(BVH *bvh, Progress &progress, bool refit) override;

  void release_bvh(BVH *bvh) override;
  void free_bvh_memory_delayed();

  void const_copy_to(const char *name, void *host, const size_t size) override;

  void update_launch_params(const size_t offset, void *data, const size_t data_size);

  unique_ptr<DeviceQueue> gpu_queue_create() override;

  OSLGlobals *get_cpu_osl_memory() override;
};

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */
