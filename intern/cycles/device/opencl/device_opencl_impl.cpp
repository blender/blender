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

#ifdef WITH_OPENCL

#  include "device/opencl/device_opencl.h"

#  include "kernel/kernel_types.h"
#  include "kernel/split/kernel_split_data_types.h"

#  include "util/util_algorithm.h"
#  include "util/util_debug.h"
#  include "util/util_foreach.h"
#  include "util/util_logging.h"
#  include "util/util_md5.h"
#  include "util/util_path.h"
#  include "util/util_time.h"

CCL_NAMESPACE_BEGIN

struct texture_slot_t {
  texture_slot_t(const string &name, int slot) : name(name), slot(slot)
  {
  }
  string name;
  int slot;
};

static const string NON_SPLIT_KERNELS =
    "denoising "
    "base "
    "background "
    "displace ";

static const string SPLIT_BUNDLE_KERNELS =
    "data_init "
    "path_init "
    "state_buffer_size "
    "scene_intersect "
    "queue_enqueue "
    "shader_setup "
    "shader_sort "
    "enqueue_inactive "
    "next_iteration_setup "
    "indirect_subsurface "
    "buffer_update "
    "adaptive_stopping "
    "adaptive_filter_x "
    "adaptive_filter_y "
    "adaptive_adjust_samples";

const string OpenCLDevice::get_opencl_program_name(const string &kernel_name)
{
  if (NON_SPLIT_KERNELS.find(kernel_name) != std::string::npos) {
    return kernel_name;
  }
  else if (SPLIT_BUNDLE_KERNELS.find(kernel_name) != std::string::npos) {
    return "split_bundle";
  }
  else {
    return "split_" + kernel_name;
  }
}

const string OpenCLDevice::get_opencl_program_filename(const string &kernel_name)
{
  if (kernel_name == "denoising") {
    return "filter.cl";
  }
  else if (SPLIT_BUNDLE_KERNELS.find(kernel_name) != std::string::npos) {
    return "kernel_split_bundle.cl";
  }
  else {
    return "kernel_" + kernel_name + ".cl";
  }
}

/* Enable features that we always want to compile to reduce recompilation events */
void OpenCLDevice::enable_default_features(DeviceRequestedFeatures &features)
{
  features.use_transparent = true;
  features.use_shadow_tricks = true;
  features.use_principled = true;
  features.use_denoising = true;

  if (!background) {
    features.max_nodes_group = NODE_GROUP_LEVEL_MAX;
    features.nodes_features = NODE_FEATURE_ALL;
    features.use_hair = true;
    features.use_subsurface = true;
    features.use_camera_motion = false;
    features.use_object_motion = false;
  }
}

string OpenCLDevice::get_build_options(const DeviceRequestedFeatures &requested_features,
                                       const string &opencl_program_name,
                                       bool preview_kernel)
{
  /* first check for non-split kernel programs */
  if (opencl_program_name == "base" || opencl_program_name == "denoising") {
    return "";
  }
  else if (opencl_program_name == "bake") {
    /* Note: get_build_options for bake is only requested when baking is enabled.
     * displace and background are always requested.
     * `__SPLIT_KERNEL__` must not be present in the compile directives for bake */
    DeviceRequestedFeatures features(requested_features);
    enable_default_features(features);
    features.use_denoising = false;
    features.use_object_motion = false;
    features.use_camera_motion = false;
    features.use_hair = true;
    features.use_subsurface = true;
    features.max_nodes_group = NODE_GROUP_LEVEL_MAX;
    features.nodes_features = NODE_FEATURE_ALL;
    features.use_integrator_branched = false;
    return features.get_build_options();
  }
  else if (opencl_program_name == "displace") {
    /* As displacement does not use any nodes from the Shading group (eg BSDF).
     * We disable all features that are related to shading. */
    DeviceRequestedFeatures features(requested_features);
    enable_default_features(features);
    features.use_denoising = false;
    features.use_object_motion = false;
    features.use_camera_motion = false;
    features.use_baking = false;
    features.use_transparent = false;
    features.use_shadow_tricks = false;
    features.use_subsurface = false;
    features.use_volume = false;
    features.nodes_features &= ~NODE_FEATURE_VOLUME;
    features.use_denoising = false;
    features.use_principled = false;
    features.use_integrator_branched = false;
    return features.get_build_options();
  }
  else if (opencl_program_name == "background") {
    /* Background uses Background shading
     * It is save to disable shadow features, subsurface and volumetric. */
    DeviceRequestedFeatures features(requested_features);
    enable_default_features(features);
    features.use_baking = false;
    features.use_object_motion = false;
    features.use_camera_motion = false;
    features.use_transparent = false;
    features.use_shadow_tricks = false;
    features.use_denoising = false;
    /* NOTE: currently possible to use surface nodes like `Hair Info`, `Bump` node.
     * Perhaps we should remove them in UI as it does not make any sense when
     * rendering background. */
    features.nodes_features &= ~NODE_FEATURE_VOLUME;
    features.use_subsurface = false;
    features.use_volume = false;
    features.use_shader_raytrace = false;
    features.use_patch_evaluation = false;
    features.use_integrator_branched = false;
    return features.get_build_options();
  }

  string build_options = "-D__SPLIT_KERNEL__ ";
  /* Set compute device build option. */
  cl_device_type device_type;
  OpenCLInfo::get_device_type(this->cdDevice, &device_type, &this->ciErr);
  assert(this->ciErr == CL_SUCCESS);
  if (device_type == CL_DEVICE_TYPE_GPU) {
    build_options += "-D__COMPUTE_DEVICE_GPU__ ";
  }

  DeviceRequestedFeatures nofeatures;
  enable_default_features(nofeatures);

  /* Add program specific optimized compile directives */
  if (preview_kernel) {
    DeviceRequestedFeatures preview_features;
    preview_features.use_hair = true;
    build_options += "-D__KERNEL_AO_PREVIEW__ ";
    build_options += preview_features.get_build_options();
  }
  else if (opencl_program_name == "split_do_volume" && !requested_features.use_volume) {
    build_options += nofeatures.get_build_options();
  }
  else {
    DeviceRequestedFeatures features(requested_features);
    enable_default_features(features);

    /* Always turn off baking at this point. Baking is only useful when building the bake kernel.
     * this also makes sure that the kernels that are build during baking can be reused
     * when not doing any baking. */
    features.use_baking = false;

    /* Do not vary on shaders when program doesn't do any shading.
     * We have bundled them in a single program. */
    if (opencl_program_name == "split_bundle") {
      features.max_nodes_group = 0;
      features.nodes_features = 0;
      features.use_shader_raytrace = false;
    }

    /* No specific settings, just add the regular ones */
    build_options += features.get_build_options();
  }

  return build_options;
}

OpenCLDevice::OpenCLSplitPrograms::OpenCLSplitPrograms(OpenCLDevice *device_)
{
  device = device_;
}

OpenCLDevice::OpenCLSplitPrograms::~OpenCLSplitPrograms()
{
  program_split.release();
  program_lamp_emission.release();
  program_do_volume.release();
  program_indirect_background.release();
  program_shader_eval.release();
  program_holdout_emission_blurring_pathtermination_ao.release();
  program_subsurface_scatter.release();
  program_direct_lighting.release();
  program_shadow_blocked_ao.release();
  program_shadow_blocked_dl.release();
}

void OpenCLDevice::OpenCLSplitPrograms::load_kernels(
    vector<OpenCLProgram *> &programs,
    const DeviceRequestedFeatures &requested_features,
    bool is_preview)
{
  if (!requested_features.use_baking) {
#  define ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(kernel_name) \
    program_split.add_kernel(ustring("path_trace_" #kernel_name));
#  define ADD_SPLIT_KERNEL_PROGRAM(kernel_name) \
    const string program_name_##kernel_name = "split_" #kernel_name; \
    program_##kernel_name = OpenCLDevice::OpenCLProgram( \
        device, \
        program_name_##kernel_name, \
        "kernel_" #kernel_name ".cl", \
        device->get_build_options(requested_features, program_name_##kernel_name, is_preview)); \
    program_##kernel_name.add_kernel(ustring("path_trace_" #kernel_name)); \
    programs.push_back(&program_##kernel_name);

    /* Ordered with most complex kernels first, to reduce overall compile time. */
    ADD_SPLIT_KERNEL_PROGRAM(subsurface_scatter);
    ADD_SPLIT_KERNEL_PROGRAM(direct_lighting);
    ADD_SPLIT_KERNEL_PROGRAM(indirect_background);
    if (requested_features.use_volume || is_preview) {
      ADD_SPLIT_KERNEL_PROGRAM(do_volume);
    }
    ADD_SPLIT_KERNEL_PROGRAM(shader_eval);
    ADD_SPLIT_KERNEL_PROGRAM(lamp_emission);
    ADD_SPLIT_KERNEL_PROGRAM(holdout_emission_blurring_pathtermination_ao);
    ADD_SPLIT_KERNEL_PROGRAM(shadow_blocked_dl);
    ADD_SPLIT_KERNEL_PROGRAM(shadow_blocked_ao);

    /* Quick kernels bundled in a single program to reduce overhead of starting
     * Blender processes. */
    program_split = OpenCLDevice::OpenCLProgram(
        device,
        "split_bundle",
        "kernel_split_bundle.cl",
        device->get_build_options(requested_features, "split_bundle", is_preview));

    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(data_init);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(state_buffer_size);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(path_init);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(scene_intersect);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(queue_enqueue);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(shader_setup);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(shader_sort);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(enqueue_inactive);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(next_iteration_setup);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(indirect_subsurface);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(buffer_update);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(adaptive_stopping);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(adaptive_filter_x);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(adaptive_filter_y);
    ADD_SPLIT_KERNEL_BUNDLE_PROGRAM(adaptive_adjust_samples);
    programs.push_back(&program_split);

#  undef ADD_SPLIT_KERNEL_PROGRAM
#  undef ADD_SPLIT_KERNEL_BUNDLE_PROGRAM
  }
}

namespace {

/* Copy dummy KernelGlobals related to OpenCL from kernel_globals.h to
 * fetch its size.
 */
typedef struct KernelGlobalsDummy {
  ccl_constant KernelData *data;
  ccl_global char *buffers[8];

#  define KERNEL_TEX(type, name) TextureInfo name;
#  include "kernel/kernel_textures.h"
#  undef KERNEL_TEX
  SplitData split_data;
  SplitParams split_param_data;
} KernelGlobalsDummy;

}  // namespace

struct CachedSplitMemory {
  int id;
  device_memory *split_data;
  device_memory *ray_state;
  device_memory *queue_index;
  device_memory *use_queues_flag;
  device_memory *work_pools;
  device_ptr *buffer;
};

class OpenCLSplitKernelFunction : public SplitKernelFunction {
 public:
  OpenCLDevice *device;
  OpenCLDevice::OpenCLProgram program;
  CachedSplitMemory &cached_memory;
  int cached_id;

  OpenCLSplitKernelFunction(OpenCLDevice *device, CachedSplitMemory &cached_memory)
      : device(device), cached_memory(cached_memory), cached_id(cached_memory.id - 1)
  {
  }

  ~OpenCLSplitKernelFunction()
  {
    program.release();
  }

  virtual bool enqueue(const KernelDimensions &dim, device_memory &kg, device_memory &data)
  {
    if (cached_id != cached_memory.id) {
      cl_uint start_arg_index = device->kernel_set_args(
          program(), 0, kg, data, *cached_memory.split_data, *cached_memory.ray_state);

      device->set_kernel_arg_buffers(program(), &start_arg_index);

      start_arg_index += device->kernel_set_args(program(),
                                                 start_arg_index,
                                                 *cached_memory.queue_index,
                                                 *cached_memory.use_queues_flag,
                                                 *cached_memory.work_pools,
                                                 *cached_memory.buffer);

      cached_id = cached_memory.id;
    }

    device->ciErr = clEnqueueNDRangeKernel(device->cqCommandQueue,
                                           program(),
                                           2,
                                           NULL,
                                           dim.global_size,
                                           dim.local_size,
                                           0,
                                           NULL,
                                           NULL);

    device->opencl_assert_err(device->ciErr, "clEnqueueNDRangeKernel");

    if (device->ciErr != CL_SUCCESS) {
      string message = string_printf("OpenCL error: %s in clEnqueueNDRangeKernel()",
                                     clewErrorString(device->ciErr));
      device->opencl_error(message);
      return false;
    }

    return true;
  }
};

class OpenCLSplitKernel : public DeviceSplitKernel {
  OpenCLDevice *device;
  CachedSplitMemory cached_memory;

 public:
  explicit OpenCLSplitKernel(OpenCLDevice *device) : DeviceSplitKernel(device), device(device)
  {
  }

  virtual SplitKernelFunction *get_split_kernel_function(
      const string &kernel_name, const DeviceRequestedFeatures &requested_features)
  {
    OpenCLSplitKernelFunction *kernel = new OpenCLSplitKernelFunction(device, cached_memory);

    const string program_name = device->get_opencl_program_name(kernel_name);
    kernel->program = OpenCLDevice::OpenCLProgram(
        device,
        program_name,
        device->get_opencl_program_filename(kernel_name),
        device->get_build_options(requested_features, program_name, device->use_preview_kernels));

    kernel->program.add_kernel(ustring("path_trace_" + kernel_name));
    kernel->program.load();

    if (!kernel->program.is_loaded()) {
      delete kernel;
      return NULL;
    }

    return kernel;
  }

  virtual uint64_t state_buffer_size(device_memory &kg, device_memory &data, size_t num_threads)
  {
    device_vector<uint64_t> size_buffer(device, "size_buffer", MEM_READ_WRITE);
    size_buffer.alloc(1);
    size_buffer.zero_to_device();

    uint threads = num_threads;
    OpenCLDevice::OpenCLSplitPrograms *programs = device->get_split_programs();
    cl_kernel kernel_state_buffer_size = programs->program_split(
        ustring("path_trace_state_buffer_size"));
    device->kernel_set_args(kernel_state_buffer_size, 0, kg, data, threads, size_buffer);

    size_t global_size = 64;
    device->ciErr = clEnqueueNDRangeKernel(device->cqCommandQueue,
                                           kernel_state_buffer_size,
                                           1,
                                           NULL,
                                           &global_size,
                                           NULL,
                                           0,
                                           NULL,
                                           NULL);

    device->opencl_assert_err(device->ciErr, "clEnqueueNDRangeKernel");

    size_buffer.copy_from_device(0, 1, 1);
    size_t size = size_buffer[0];
    size_buffer.free();

    if (device->ciErr != CL_SUCCESS) {
      string message = string_printf("OpenCL error: %s in clEnqueueNDRangeKernel()",
                                     clewErrorString(device->ciErr));
      device->opencl_error(message);
      return 0;
    }

    return size;
  }

  virtual bool enqueue_split_kernel_data_init(const KernelDimensions &dim,
                                              RenderTile &rtile,
                                              int num_global_elements,
                                              device_memory &kernel_globals,
                                              device_memory &kernel_data,
                                              device_memory &split_data,
                                              device_memory &ray_state,
                                              device_memory &queue_index,
                                              device_memory &use_queues_flag,
                                              device_memory &work_pool_wgs)
  {
    cl_int dQueue_size = dim.global_size[0] * dim.global_size[1];

    /* Set the range of samples to be processed for every ray in
     * path-regeneration logic.
     */
    cl_int start_sample = rtile.start_sample;
    cl_int end_sample = rtile.start_sample + rtile.num_samples;

    OpenCLDevice::OpenCLSplitPrograms *programs = device->get_split_programs();
    cl_kernel kernel_data_init = programs->program_split(ustring("path_trace_data_init"));

    cl_uint start_arg_index = device->kernel_set_args(kernel_data_init,
                                                      0,
                                                      kernel_globals,
                                                      kernel_data,
                                                      split_data,
                                                      num_global_elements,
                                                      ray_state);

    device->set_kernel_arg_buffers(kernel_data_init, &start_arg_index);

    start_arg_index += device->kernel_set_args(kernel_data_init,
                                               start_arg_index,
                                               start_sample,
                                               end_sample,
                                               rtile.x,
                                               rtile.y,
                                               rtile.w,
                                               rtile.h,
                                               rtile.offset,
                                               rtile.stride,
                                               queue_index,
                                               dQueue_size,
                                               use_queues_flag,
                                               work_pool_wgs,
                                               rtile.num_samples,
                                               rtile.buffer);

    /* Enqueue ckPathTraceKernel_data_init kernel. */
    device->ciErr = clEnqueueNDRangeKernel(device->cqCommandQueue,
                                           kernel_data_init,
                                           2,
                                           NULL,
                                           dim.global_size,
                                           dim.local_size,
                                           0,
                                           NULL,
                                           NULL);

    device->opencl_assert_err(device->ciErr, "clEnqueueNDRangeKernel");

    if (device->ciErr != CL_SUCCESS) {
      string message = string_printf("OpenCL error: %s in clEnqueueNDRangeKernel()",
                                     clewErrorString(device->ciErr));
      device->opencl_error(message);
      return false;
    }

    cached_memory.split_data = &split_data;
    cached_memory.ray_state = &ray_state;
    cached_memory.queue_index = &queue_index;
    cached_memory.use_queues_flag = &use_queues_flag;
    cached_memory.work_pools = &work_pool_wgs;
    cached_memory.buffer = &rtile.buffer;
    cached_memory.id++;

    return true;
  }

  virtual int2 split_kernel_local_size()
  {
    return make_int2(64, 1);
  }

  virtual int2 split_kernel_global_size(device_memory &kg,
                                        device_memory &data,
                                        DeviceTask & /*task*/)
  {
    cl_device_type type = OpenCLInfo::get_device_type(device->cdDevice);
    /* Use small global size on CPU devices as it seems to be much faster. */
    if (type == CL_DEVICE_TYPE_CPU) {
      VLOG(1) << "Global size: (64, 64).";
      return make_int2(64, 64);
    }

    cl_ulong max_buffer_size;
    clGetDeviceInfo(
        device->cdDevice, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &max_buffer_size, NULL);

    if (DebugFlags().opencl.mem_limit) {
      max_buffer_size = min(max_buffer_size,
                            cl_ulong(DebugFlags().opencl.mem_limit - device->stats.mem_used));
    }

    VLOG(1) << "Maximum device allocation size: " << string_human_readable_number(max_buffer_size)
            << " bytes. (" << string_human_readable_size(max_buffer_size) << ").";

    /* Limit to 2gb, as we shouldn't need more than that and some devices may support much more. */
    max_buffer_size = min(max_buffer_size / 2, (cl_ulong)2l * 1024 * 1024 * 1024);

    size_t num_elements = max_elements_for_max_buffer_size(kg, data, max_buffer_size);
    int2 global_size = make_int2(max(round_down((int)sqrt(num_elements), 64), 64),
                                 (int)sqrt(num_elements));
    VLOG(1) << "Global size: " << global_size << ".";
    return global_size;
  }
};

bool OpenCLDevice::opencl_error(cl_int err)
{
  if (err != CL_SUCCESS) {
    string message = string_printf("OpenCL error (%d): %s", err, clewErrorString(err));
    if (error_msg == "")
      error_msg = message;
    fprintf(stderr, "%s\n", message.c_str());
    return true;
  }

  return false;
}

void OpenCLDevice::opencl_error(const string &message)
{
  if (error_msg == "")
    error_msg = message;
  fprintf(stderr, "%s\n", message.c_str());
}

void OpenCLDevice::opencl_assert_err(cl_int err, const char *where)
{
  if (err != CL_SUCCESS) {
    string message = string_printf(
        "OpenCL error (%d): %s in %s", err, clewErrorString(err), where);
    if (error_msg == "")
      error_msg = message;
    fprintf(stderr, "%s\n", message.c_str());
#  ifndef NDEBUG
    abort();
#  endif
  }
}

OpenCLDevice::OpenCLDevice(DeviceInfo &info, Stats &stats, Profiler &profiler, bool background)
    : Device(info, stats, profiler, background),
      load_kernel_num_compiling(0),
      kernel_programs(this),
      preview_programs(this),
      memory_manager(this),
      texture_info(this, "__texture_info", MEM_GLOBAL)
{
  cpPlatform = NULL;
  cdDevice = NULL;
  cxContext = NULL;
  cqCommandQueue = NULL;
  device_initialized = false;
  textures_need_update = true;
  use_preview_kernels = !background;

  vector<OpenCLPlatformDevice> usable_devices;
  OpenCLInfo::get_usable_devices(&usable_devices);
  if (usable_devices.size() == 0) {
    opencl_error("OpenCL: no devices found.");
    return;
  }
  assert(info.num < usable_devices.size());
  OpenCLPlatformDevice &platform_device = usable_devices[info.num];
  device_num = info.num;
  cpPlatform = platform_device.platform_id;
  cdDevice = platform_device.device_id;
  platform_name = platform_device.platform_name;
  device_name = platform_device.device_name;
  VLOG(2) << "Creating new Cycles device for OpenCL platform " << platform_name << ", device "
          << device_name << ".";

  {
    /* try to use cached context */
    thread_scoped_lock cache_locker;
    cxContext = OpenCLCache::get_context(cpPlatform, cdDevice, cache_locker);

    if (cxContext == NULL) {
      /* create context properties array to specify platform */
      const cl_context_properties context_props[] = {
          CL_CONTEXT_PLATFORM, (cl_context_properties)cpPlatform, 0, 0};

      /* create context */
      cxContext = clCreateContext(
          context_props, 1, &cdDevice, context_notify_callback, cdDevice, &ciErr);

      if (opencl_error(ciErr)) {
        opencl_error("OpenCL: clCreateContext failed");
        return;
      }

      /* cache it */
      OpenCLCache::store_context(cpPlatform, cdDevice, cxContext, cache_locker);
    }
  }

  cqCommandQueue = clCreateCommandQueue(cxContext, cdDevice, 0, &ciErr);
  if (opencl_error(ciErr)) {
    opencl_error("OpenCL: Error creating command queue");
    return;
  }

  /* Allocate this right away so that texture_info
   * is placed at offset 0 in the device memory buffers. */
  texture_info.resize(1);
  memory_manager.alloc("texture_info", texture_info);

  device_initialized = true;

  split_kernel = new OpenCLSplitKernel(this);
  if (use_preview_kernels) {
    load_preview_kernels();
  }
}

OpenCLDevice::~OpenCLDevice()
{
  task_pool.cancel();
  load_required_kernel_task_pool.cancel();
  load_kernel_task_pool.cancel();

  memory_manager.free();

  ConstMemMap::iterator mt;
  for (mt = const_mem_map.begin(); mt != const_mem_map.end(); mt++) {
    delete mt->second;
  }

  base_program.release();
  bake_program.release();
  displace_program.release();
  background_program.release();
  denoising_program.release();

  if (cqCommandQueue)
    clReleaseCommandQueue(cqCommandQueue);
  if (cxContext)
    clReleaseContext(cxContext);

  delete split_kernel;
}

void CL_CALLBACK OpenCLDevice::context_notify_callback(const char *err_info,
                                                       const void * /*private_info*/,
                                                       size_t /*cb*/,
                                                       void *user_data)
{
  string device_name = OpenCLInfo::get_device_name((cl_device_id)user_data);
  fprintf(stderr, "OpenCL error (%s): %s\n", device_name.c_str(), err_info);
}

bool OpenCLDevice::opencl_version_check()
{
  string error;
  if (!OpenCLInfo::platform_version_check(cpPlatform, &error)) {
    opencl_error(error);
    return false;
  }
  if (!OpenCLInfo::device_version_check(cdDevice, &error)) {
    opencl_error(error);
    return false;
  }
  return true;
}

string OpenCLDevice::device_md5_hash(string kernel_custom_build_options)
{
  MD5Hash md5;
  char version[256], driver[256], name[256], vendor[256];

  clGetPlatformInfo(cpPlatform, CL_PLATFORM_VENDOR, sizeof(vendor), &vendor, NULL);
  clGetDeviceInfo(cdDevice, CL_DEVICE_VERSION, sizeof(version), &version, NULL);
  clGetDeviceInfo(cdDevice, CL_DEVICE_NAME, sizeof(name), &name, NULL);
  clGetDeviceInfo(cdDevice, CL_DRIVER_VERSION, sizeof(driver), &driver, NULL);

  md5.append((uint8_t *)vendor, strlen(vendor));
  md5.append((uint8_t *)version, strlen(version));
  md5.append((uint8_t *)name, strlen(name));
  md5.append((uint8_t *)driver, strlen(driver));

  string options = kernel_build_options();
  options += kernel_custom_build_options;
  md5.append((uint8_t *)options.c_str(), options.size());

  return md5.get_hex();
}

bool OpenCLDevice::load_kernels(const DeviceRequestedFeatures &requested_features)
{
  VLOG(2) << "Loading kernels for platform " << platform_name << ", device " << device_name << ".";
  /* Verify if device was initialized. */
  if (!device_initialized) {
    fprintf(stderr, "OpenCL: failed to initialize device.\n");
    return false;
  }

  /* Verify we have right opencl version. */
  if (!opencl_version_check())
    return false;

  load_required_kernels(requested_features);

  vector<OpenCLProgram *> programs;
  kernel_programs.load_kernels(programs, requested_features, false);

  if (!requested_features.use_baking && requested_features.use_denoising) {
    denoising_program = OpenCLProgram(
        this, "denoising", "filter.cl", get_build_options(requested_features, "denoising"));
    denoising_program.add_kernel(ustring("filter_divide_shadow"));
    denoising_program.add_kernel(ustring("filter_get_feature"));
    denoising_program.add_kernel(ustring("filter_write_feature"));
    denoising_program.add_kernel(ustring("filter_detect_outliers"));
    denoising_program.add_kernel(ustring("filter_combine_halves"));
    denoising_program.add_kernel(ustring("filter_construct_transform"));
    denoising_program.add_kernel(ustring("filter_nlm_calc_difference"));
    denoising_program.add_kernel(ustring("filter_nlm_blur"));
    denoising_program.add_kernel(ustring("filter_nlm_calc_weight"));
    denoising_program.add_kernel(ustring("filter_nlm_update_output"));
    denoising_program.add_kernel(ustring("filter_nlm_normalize"));
    denoising_program.add_kernel(ustring("filter_nlm_construct_gramian"));
    denoising_program.add_kernel(ustring("filter_finalize"));
    programs.push_back(&denoising_program);
  }

  load_required_kernel_task_pool.wait_work();

  /* Parallel compilation of Cycles kernels, this launches multiple
   * processes to workaround OpenCL frameworks serializing the calls
   * internally within a single process. */
  foreach (OpenCLProgram *program, programs) {
    if (!program->load()) {
      load_kernel_num_compiling++;
      load_kernel_task_pool.push([&] {
        program->compile();
        load_kernel_num_compiling--;
      });
    }
  }
  return true;
}

void OpenCLDevice::load_required_kernels(const DeviceRequestedFeatures &requested_features)
{
  vector<OpenCLProgram *> programs;
  base_program = OpenCLProgram(
      this, "base", "kernel_base.cl", get_build_options(requested_features, "base"));
  base_program.add_kernel(ustring("convert_to_byte"));
  base_program.add_kernel(ustring("convert_to_half_float"));
  base_program.add_kernel(ustring("zero_buffer"));
  programs.push_back(&base_program);

  if (requested_features.use_true_displacement) {
    displace_program = OpenCLProgram(
        this, "displace", "kernel_displace.cl", get_build_options(requested_features, "displace"));
    displace_program.add_kernel(ustring("displace"));
    programs.push_back(&displace_program);
  }

  if (requested_features.use_background_light) {
    background_program = OpenCLProgram(this,
                                       "background",
                                       "kernel_background.cl",
                                       get_build_options(requested_features, "background"));
    background_program.add_kernel(ustring("background"));
    programs.push_back(&background_program);
  }

  if (requested_features.use_baking) {
    bake_program = OpenCLProgram(
        this, "bake", "kernel_bake.cl", get_build_options(requested_features, "bake"));
    bake_program.add_kernel(ustring("bake"));
    programs.push_back(&bake_program);
  }

  foreach (OpenCLProgram *program, programs) {
    if (!program->load()) {
      load_required_kernel_task_pool.push(function_bind(&OpenCLProgram::compile, program));
    }
  }
}

void OpenCLDevice::load_preview_kernels()
{
  DeviceRequestedFeatures no_features;
  vector<OpenCLProgram *> programs;
  preview_programs.load_kernels(programs, no_features, true);

  foreach (OpenCLProgram *program, programs) {
    if (!program->load()) {
      load_required_kernel_task_pool.push(function_bind(&OpenCLProgram::compile, program));
    }
  }
}

bool OpenCLDevice::wait_for_availability(const DeviceRequestedFeatures &requested_features)
{
  if (background) {
    load_kernel_task_pool.wait_work();
    use_preview_kernels = false;
  }
  else {
    /* We use a device setting to determine to load preview kernels or not
     * Better to check on device level than per kernel as mixing preview and
     * non-preview kernels does not work due to different data types */
    if (use_preview_kernels) {
      use_preview_kernels = load_kernel_num_compiling.load() > 0;
    }
  }
  return split_kernel->load_kernels(requested_features);
}

OpenCLDevice::OpenCLSplitPrograms *OpenCLDevice::get_split_programs()
{
  return use_preview_kernels ? &preview_programs : &kernel_programs;
}

DeviceKernelStatus OpenCLDevice::get_active_kernel_switch_state()
{
  /* Do not switch kernels for background renderings
   * We do foreground rendering but use the preview kernels
   * Check for the optimized kernels
   *
   * This works also the other way around, where we are using
   * optimized kernels but new ones are being compiled due
   * to other features that are needed */
  if (background) {
    /* The if-statements below would find the same result,
     * But as the `finished` method uses a mutex we added
     * this as an early exit */
    return DEVICE_KERNEL_USING_FEATURE_KERNEL;
  }

  bool other_kernels_finished = load_kernel_num_compiling.load() == 0;
  if (use_preview_kernels) {
    if (other_kernels_finished) {
      return DEVICE_KERNEL_FEATURE_KERNEL_AVAILABLE;
    }
    else {
      return DEVICE_KERNEL_WAITING_FOR_FEATURE_KERNEL;
    }
  }
  else {
    if (other_kernels_finished) {
      return DEVICE_KERNEL_USING_FEATURE_KERNEL;
    }
    else {
      return DEVICE_KERNEL_FEATURE_KERNEL_INVALID;
    }
  }
}

void OpenCLDevice::mem_alloc(device_memory &mem)
{
  if (mem.name) {
    VLOG(1) << "Buffer allocate: " << mem.name << ", "
            << string_human_readable_number(mem.memory_size()) << " bytes. ("
            << string_human_readable_size(mem.memory_size()) << ")";
  }

  size_t size = mem.memory_size();

  /* check there is enough memory available for the allocation */
  cl_ulong max_alloc_size = 0;
  clGetDeviceInfo(cdDevice, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &max_alloc_size, NULL);

  if (DebugFlags().opencl.mem_limit) {
    max_alloc_size = min(max_alloc_size, cl_ulong(DebugFlags().opencl.mem_limit - stats.mem_used));
  }

  if (size > max_alloc_size) {
    string error = "Scene too complex to fit in available memory.";
    if (mem.name != NULL) {
      error += string_printf(" (allocating buffer %s failed.)", mem.name);
    }
    set_error(error);

    return;
  }

  cl_mem_flags mem_flag;
  void *mem_ptr = NULL;

  if (mem.type == MEM_READ_ONLY || mem.type == MEM_TEXTURE || mem.type == MEM_GLOBAL)
    mem_flag = CL_MEM_READ_ONLY;
  else
    mem_flag = CL_MEM_READ_WRITE;

  /* Zero-size allocation might be invoked by render, but not really
   * supported by OpenCL. Using NULL as device pointer also doesn't really
   * work for some reason, so for the time being we'll use special case
   * will null_mem buffer.
   */
  if (size != 0) {
    mem.device_pointer = (device_ptr)clCreateBuffer(cxContext, mem_flag, size, mem_ptr, &ciErr);
    opencl_assert_err(ciErr, "clCreateBuffer");
  }
  else {
    mem.device_pointer = 0;
  }

  stats.mem_alloc(size);
  mem.device_size = size;
}

void OpenCLDevice::mem_copy_to(device_memory &mem)
{
  if (mem.type == MEM_GLOBAL) {
    global_free(mem);
    global_alloc(mem);
  }
  else if (mem.type == MEM_TEXTURE) {
    tex_free((device_texture &)mem);
    tex_alloc((device_texture &)mem);
  }
  else {
    if (!mem.device_pointer) {
      mem_alloc(mem);
    }

    /* this is blocking */
    size_t size = mem.memory_size();
    if (size != 0) {
      opencl_assert(clEnqueueWriteBuffer(cqCommandQueue,
                                         CL_MEM_PTR(mem.device_pointer),
                                         CL_TRUE,
                                         0,
                                         size,
                                         mem.host_pointer,
                                         0,
                                         NULL,
                                         NULL));
    }
  }
}

void OpenCLDevice::mem_copy_from(device_memory &mem, int y, int w, int h, int elem)
{
  size_t offset = elem * y * w;
  size_t size = elem * w * h;
  assert(size != 0);
  opencl_assert(clEnqueueReadBuffer(cqCommandQueue,
                                    CL_MEM_PTR(mem.device_pointer),
                                    CL_TRUE,
                                    offset,
                                    size,
                                    (uchar *)mem.host_pointer + offset,
                                    0,
                                    NULL,
                                    NULL));
}

void OpenCLDevice::mem_zero_kernel(device_ptr mem, size_t size)
{
  base_program.wait_for_availability();
  cl_kernel ckZeroBuffer = base_program(ustring("zero_buffer"));

  size_t global_size[] = {1024, 1024};
  size_t num_threads = global_size[0] * global_size[1];

  cl_mem d_buffer = CL_MEM_PTR(mem);
  cl_ulong d_offset = 0;
  cl_ulong d_size = 0;

  while (d_offset < size) {
    d_size = std::min<cl_ulong>(num_threads * sizeof(float4), size - d_offset);

    kernel_set_args(ckZeroBuffer, 0, d_buffer, d_size, d_offset);

    ciErr = clEnqueueNDRangeKernel(
        cqCommandQueue, ckZeroBuffer, 2, NULL, global_size, NULL, 0, NULL, NULL);
    opencl_assert_err(ciErr, "clEnqueueNDRangeKernel");

    d_offset += d_size;
  }
}

void OpenCLDevice::mem_zero(device_memory &mem)
{
  if (!mem.device_pointer) {
    mem_alloc(mem);
  }

  if (mem.device_pointer) {
    if (base_program.is_loaded()) {
      mem_zero_kernel(mem.device_pointer, mem.memory_size());
    }

    if (mem.host_pointer) {
      memset(mem.host_pointer, 0, mem.memory_size());
    }

    if (!base_program.is_loaded()) {
      void *zero = mem.host_pointer;

      if (!mem.host_pointer) {
        zero = util_aligned_malloc(mem.memory_size(), 16);
        memset(zero, 0, mem.memory_size());
      }

      opencl_assert(clEnqueueWriteBuffer(cqCommandQueue,
                                         CL_MEM_PTR(mem.device_pointer),
                                         CL_TRUE,
                                         0,
                                         mem.memory_size(),
                                         zero,
                                         0,
                                         NULL,
                                         NULL));

      if (!mem.host_pointer) {
        util_aligned_free(zero);
      }
    }
  }
}

void OpenCLDevice::mem_free(device_memory &mem)
{
  if (mem.type == MEM_GLOBAL) {
    global_free(mem);
  }
  else if (mem.type == MEM_TEXTURE) {
    tex_free((device_texture &)mem);
  }
  else {
    if (mem.device_pointer) {
      if (mem.device_pointer != 0) {
        opencl_assert(clReleaseMemObject(CL_MEM_PTR(mem.device_pointer)));
      }
      mem.device_pointer = 0;

      stats.mem_free(mem.device_size);
      mem.device_size = 0;
    }
  }
}

int OpenCLDevice::mem_sub_ptr_alignment()
{
  return OpenCLInfo::mem_sub_ptr_alignment(cdDevice);
}

device_ptr OpenCLDevice::mem_alloc_sub_ptr(device_memory &mem, int offset, int size)
{
  cl_mem_flags mem_flag;
  if (mem.type == MEM_READ_ONLY || mem.type == MEM_TEXTURE || mem.type == MEM_GLOBAL)
    mem_flag = CL_MEM_READ_ONLY;
  else
    mem_flag = CL_MEM_READ_WRITE;

  cl_buffer_region info;
  info.origin = mem.memory_elements_size(offset);
  info.size = mem.memory_elements_size(size);

  device_ptr sub_buf = (device_ptr)clCreateSubBuffer(
      CL_MEM_PTR(mem.device_pointer), mem_flag, CL_BUFFER_CREATE_TYPE_REGION, &info, &ciErr);
  opencl_assert_err(ciErr, "clCreateSubBuffer");
  return sub_buf;
}

void OpenCLDevice::mem_free_sub_ptr(device_ptr device_pointer)
{
  if (device_pointer != 0) {
    opencl_assert(clReleaseMemObject(CL_MEM_PTR(device_pointer)));
  }
}

void OpenCLDevice::const_copy_to(const char *name, void *host, size_t size)
{
  ConstMemMap::iterator i = const_mem_map.find(name);
  device_vector<uchar> *data;

  if (i == const_mem_map.end()) {
    data = new device_vector<uchar>(this, name, MEM_READ_ONLY);
    data->alloc(size);
    const_mem_map.insert(ConstMemMap::value_type(name, data));
  }
  else {
    data = i->second;
  }

  memcpy(data->data(), host, size);
  data->copy_to_device();
}

void OpenCLDevice::global_alloc(device_memory &mem)
{
  VLOG(1) << "Global memory allocate: " << mem.name << ", "
          << string_human_readable_number(mem.memory_size()) << " bytes. ("
          << string_human_readable_size(mem.memory_size()) << ")";

  memory_manager.alloc(mem.name, mem);
  /* Set the pointer to non-null to keep code that inspects its value from thinking its
   * unallocated. */
  mem.device_pointer = 1;
  textures[mem.name] = &mem;
  textures_need_update = true;
}

void OpenCLDevice::global_free(device_memory &mem)
{
  if (mem.device_pointer) {
    mem.device_pointer = 0;

    if (memory_manager.free(mem)) {
      textures_need_update = true;
    }

    foreach (TexturesMap::value_type &value, textures) {
      if (value.second == &mem) {
        textures.erase(value.first);
        break;
      }
    }
  }
}

void OpenCLDevice::tex_alloc(device_texture &mem)
{
  VLOG(1) << "Texture allocate: " << mem.name << ", "
          << string_human_readable_number(mem.memory_size()) << " bytes. ("
          << string_human_readable_size(mem.memory_size()) << ")";

  memory_manager.alloc(mem.name, mem);
  /* Set the pointer to non-null to keep code that inspects its value from thinking its
   * unallocated. */
  mem.device_pointer = 1;
  textures[mem.name] = &mem;
  textures_need_update = true;
}

void OpenCLDevice::tex_free(device_texture &mem)
{
  global_free(mem);
}

size_t OpenCLDevice::global_size_round_up(int group_size, int global_size)
{
  int r = global_size % group_size;
  return global_size + ((r == 0) ? 0 : group_size - r);
}

void OpenCLDevice::enqueue_kernel(
    cl_kernel kernel, size_t w, size_t h, bool x_workgroups, size_t max_workgroup_size)
{
  size_t workgroup_size, max_work_items[3];

  clGetKernelWorkGroupInfo(
      kernel, cdDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &workgroup_size, NULL);
  clGetDeviceInfo(
      cdDevice, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t) * 3, max_work_items, NULL);

  if (max_workgroup_size > 0 && workgroup_size > max_workgroup_size) {
    workgroup_size = max_workgroup_size;
  }

  /* Try to divide evenly over 2 dimensions. */
  size_t local_size[2];
  if (x_workgroups) {
    local_size[0] = workgroup_size;
    local_size[1] = 1;
  }
  else {
    size_t sqrt_workgroup_size = max((size_t)sqrt((double)workgroup_size), 1);
    local_size[0] = local_size[1] = sqrt_workgroup_size;
  }

  /* Some implementations have max size 1 on 2nd dimension. */
  if (local_size[1] > max_work_items[1]) {
    local_size[0] = workgroup_size / max_work_items[1];
    local_size[1] = max_work_items[1];
  }

  size_t global_size[2] = {global_size_round_up(local_size[0], w),
                           global_size_round_up(local_size[1], h)};

  /* Vertical size of 1 is coming from bake/shade kernels where we should
   * not round anything up because otherwise we'll either be doing too
   * much work per pixel (if we don't check global ID on Y axis) or will
   * be checking for global ID to always have Y of 0.
   */
  if (h == 1) {
    global_size[h] = 1;
  }

  /* run kernel */
  opencl_assert(
      clEnqueueNDRangeKernel(cqCommandQueue, kernel, 2, NULL, global_size, NULL, 0, NULL, NULL));
  opencl_assert(clFlush(cqCommandQueue));
}

void OpenCLDevice::set_kernel_arg_mem(cl_kernel kernel, cl_uint *narg, const char *name)
{
  cl_mem ptr;

  MemMap::iterator i = mem_map.find(name);
  if (i != mem_map.end()) {
    ptr = CL_MEM_PTR(i->second);
  }
  else {
    ptr = 0;
  }

  opencl_assert(clSetKernelArg(kernel, (*narg)++, sizeof(ptr), (void *)&ptr));
}

void OpenCLDevice::set_kernel_arg_buffers(cl_kernel kernel, cl_uint *narg)
{
  flush_texture_buffers();

  memory_manager.set_kernel_arg_buffers(kernel, narg);
}

void OpenCLDevice::flush_texture_buffers()
{
  if (!textures_need_update) {
    return;
  }
  textures_need_update = false;

  /* Setup slots for textures. */
  int num_slots = 0;

  vector<texture_slot_t> texture_slots;

#  define KERNEL_TEX(type, name) \
    if (textures.find(#name) != textures.end()) { \
      texture_slots.push_back(texture_slot_t(#name, num_slots)); \
    } \
    num_slots++;
#  include "kernel/kernel_textures.h"

  int num_data_slots = num_slots;

  foreach (TexturesMap::value_type &tex, textures) {
    string name = tex.first;
    device_memory *mem = tex.second;

    if (mem->type == MEM_TEXTURE) {
      const uint id = ((device_texture *)mem)->slot;
      texture_slots.push_back(texture_slot_t(name, num_data_slots + id));
      num_slots = max(num_slots, num_data_slots + id + 1);
    }
  }

  /* Realloc texture descriptors buffer. */
  memory_manager.free(texture_info);
  texture_info.resize(num_slots);
  memory_manager.alloc("texture_info", texture_info);

  /* Fill in descriptors */
  foreach (texture_slot_t &slot, texture_slots) {
    device_memory *mem = textures[slot.name];
    TextureInfo &info = texture_info[slot.slot];

    MemoryManager::BufferDescriptor desc = memory_manager.get_descriptor(slot.name);

    if (mem->type == MEM_TEXTURE) {
      info = ((device_texture *)mem)->info;
    }
    else {
      memset(&info, 0, sizeof(TextureInfo));
    }

    info.data = desc.offset;
    info.cl_buffer = desc.device_buffer;
  }

  /* Force write of descriptors. */
  memory_manager.free(texture_info);
  memory_manager.alloc("texture_info", texture_info);
}

void OpenCLDevice::thread_run(DeviceTask &task)
{
  flush_texture_buffers();

  if (task.type == DeviceTask::RENDER) {
    RenderTile tile;
    DenoisingTask denoising(this, task);

    /* Allocate buffer for kernel globals */
    device_only_memory<KernelGlobalsDummy> kgbuffer(this, "kernel_globals");
    kgbuffer.alloc_to_device(1);

    /* Keep rendering tiles until done. */
    while (task.acquire_tile(this, tile, task.tile_types)) {
      if (tile.task == RenderTile::PATH_TRACE) {
        assert(tile.task == RenderTile::PATH_TRACE);
        scoped_timer timer(&tile.buffers->render_time);

        split_kernel->path_trace(task, tile, kgbuffer, *const_mem_map["__data"]);

        /* Complete kernel execution before release tile. */
        /* This helps in multi-device render;
         * The device that reaches the critical-section function
         * release_tile waits (stalling other devices from entering
         * release_tile) for all kernels to complete. If device1 (a
         * slow-render device) reaches release_tile first then it would
         * stall device2 (a fast-render device) from proceeding to render
         * next tile.
         */
        clFinish(cqCommandQueue);
      }
      else if (tile.task == RenderTile::BAKE) {
        bake(task, tile);
      }
      else if (tile.task == RenderTile::DENOISE) {
        tile.sample = tile.start_sample + tile.num_samples;
        denoise(tile, denoising);
        task.update_progress(&tile, tile.w * tile.h);
      }

      task.release_tile(tile);
    }

    kgbuffer.free();
  }
  else if (task.type == DeviceTask::SHADER) {
    shader(task);
  }
  else if (task.type == DeviceTask::FILM_CONVERT) {
    film_convert(task, task.buffer, task.rgba_byte, task.rgba_half);
  }
  else if (task.type == DeviceTask::DENOISE_BUFFER) {
    RenderTile tile;
    tile.x = task.x;
    tile.y = task.y;
    tile.w = task.w;
    tile.h = task.h;
    tile.buffer = task.buffer;
    tile.sample = task.sample + task.num_samples;
    tile.num_samples = task.num_samples;
    tile.start_sample = task.sample;
    tile.offset = task.offset;
    tile.stride = task.stride;
    tile.buffers = task.buffers;

    DenoisingTask denoising(this, task);
    denoise(tile, denoising);
    task.update_progress(&tile, tile.w * tile.h);
  }
}

void OpenCLDevice::film_convert(DeviceTask &task,
                                device_ptr buffer,
                                device_ptr rgba_byte,
                                device_ptr rgba_half)
{
  /* cast arguments to cl types */
  cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
  cl_mem d_rgba = (rgba_byte) ? CL_MEM_PTR(rgba_byte) : CL_MEM_PTR(rgba_half);
  cl_mem d_buffer = CL_MEM_PTR(buffer);
  cl_int d_x = task.x;
  cl_int d_y = task.y;
  cl_int d_w = task.w;
  cl_int d_h = task.h;
  cl_float d_sample_scale = 1.0f / (task.sample + 1);
  cl_int d_offset = task.offset;
  cl_int d_stride = task.stride;

  cl_kernel ckFilmConvertKernel = (rgba_byte) ? base_program(ustring("convert_to_byte")) :
                                                base_program(ustring("convert_to_half_float"));

  cl_uint start_arg_index = kernel_set_args(ckFilmConvertKernel, 0, d_data, d_rgba, d_buffer);

  set_kernel_arg_buffers(ckFilmConvertKernel, &start_arg_index);

  start_arg_index += kernel_set_args(ckFilmConvertKernel,
                                     start_arg_index,
                                     d_sample_scale,
                                     d_x,
                                     d_y,
                                     d_w,
                                     d_h,
                                     d_offset,
                                     d_stride);

  enqueue_kernel(ckFilmConvertKernel, d_w, d_h);
}

bool OpenCLDevice::denoising_non_local_means(device_ptr image_ptr,
                                             device_ptr guide_ptr,
                                             device_ptr variance_ptr,
                                             device_ptr out_ptr,
                                             DenoisingTask *task)
{
  int stride = task->buffer.stride;
  int w = task->buffer.width;
  int h = task->buffer.h;
  int r = task->nlm_state.r;
  int f = task->nlm_state.f;
  float a = task->nlm_state.a;
  float k_2 = task->nlm_state.k_2;

  int pass_stride = task->buffer.pass_stride;
  int num_shifts = (2 * r + 1) * (2 * r + 1);
  int channel_offset = task->nlm_state.is_color ? task->buffer.pass_stride : 0;

  device_sub_ptr difference(task->buffer.temporary_mem, 0, pass_stride * num_shifts);
  device_sub_ptr blurDifference(
      task->buffer.temporary_mem, pass_stride * num_shifts, pass_stride * num_shifts);
  device_sub_ptr weightAccum(
      task->buffer.temporary_mem, 2 * pass_stride * num_shifts, pass_stride);
  cl_mem weightAccum_mem = CL_MEM_PTR(*weightAccum);
  cl_mem difference_mem = CL_MEM_PTR(*difference);
  cl_mem blurDifference_mem = CL_MEM_PTR(*blurDifference);

  cl_mem image_mem = CL_MEM_PTR(image_ptr);
  cl_mem guide_mem = CL_MEM_PTR(guide_ptr);
  cl_mem variance_mem = CL_MEM_PTR(variance_ptr);
  cl_mem out_mem = CL_MEM_PTR(out_ptr);
  cl_mem scale_mem = NULL;

  mem_zero_kernel(*weightAccum, sizeof(float) * pass_stride);
  mem_zero_kernel(out_ptr, sizeof(float) * pass_stride);

  cl_kernel ckNLMCalcDifference = denoising_program(ustring("filter_nlm_calc_difference"));
  cl_kernel ckNLMBlur = denoising_program(ustring("filter_nlm_blur"));
  cl_kernel ckNLMCalcWeight = denoising_program(ustring("filter_nlm_calc_weight"));
  cl_kernel ckNLMUpdateOutput = denoising_program(ustring("filter_nlm_update_output"));
  cl_kernel ckNLMNormalize = denoising_program(ustring("filter_nlm_normalize"));

  kernel_set_args(ckNLMCalcDifference,
                  0,
                  guide_mem,
                  variance_mem,
                  scale_mem,
                  difference_mem,
                  w,
                  h,
                  stride,
                  pass_stride,
                  r,
                  channel_offset,
                  0,
                  a,
                  k_2);
  kernel_set_args(
      ckNLMBlur, 0, difference_mem, blurDifference_mem, w, h, stride, pass_stride, r, f);
  kernel_set_args(
      ckNLMCalcWeight, 0, blurDifference_mem, difference_mem, w, h, stride, pass_stride, r, f);
  kernel_set_args(ckNLMUpdateOutput,
                  0,
                  blurDifference_mem,
                  image_mem,
                  out_mem,
                  weightAccum_mem,
                  w,
                  h,
                  stride,
                  pass_stride,
                  channel_offset,
                  r,
                  f);

  enqueue_kernel(ckNLMCalcDifference, w * h, num_shifts, true);
  enqueue_kernel(ckNLMBlur, w * h, num_shifts, true);
  enqueue_kernel(ckNLMCalcWeight, w * h, num_shifts, true);
  enqueue_kernel(ckNLMBlur, w * h, num_shifts, true);
  enqueue_kernel(ckNLMUpdateOutput, w * h, num_shifts, true);

  kernel_set_args(ckNLMNormalize, 0, out_mem, weightAccum_mem, w, h, stride);
  enqueue_kernel(ckNLMNormalize, w, h);

  return true;
}

bool OpenCLDevice::denoising_construct_transform(DenoisingTask *task)
{
  cl_mem buffer_mem = CL_MEM_PTR(task->buffer.mem.device_pointer);
  cl_mem transform_mem = CL_MEM_PTR(task->storage.transform.device_pointer);
  cl_mem rank_mem = CL_MEM_PTR(task->storage.rank.device_pointer);
  cl_mem tile_info_mem = CL_MEM_PTR(task->tile_info_mem.device_pointer);

  char use_time = task->buffer.use_time ? 1 : 0;

  cl_kernel ckFilterConstructTransform = denoising_program(ustring("filter_construct_transform"));

  int arg_ofs = kernel_set_args(ckFilterConstructTransform, 0, buffer_mem, tile_info_mem);
  cl_mem buffers[9];
  for (int i = 0; i < 9; i++) {
    buffers[i] = CL_MEM_PTR(task->tile_info->buffers[i]);
    arg_ofs += kernel_set_args(ckFilterConstructTransform, arg_ofs, buffers[i]);
  }
  kernel_set_args(ckFilterConstructTransform,
                  arg_ofs,
                  transform_mem,
                  rank_mem,
                  task->filter_area,
                  task->rect,
                  task->buffer.pass_stride,
                  task->buffer.frame_stride,
                  use_time,
                  task->radius,
                  task->pca_threshold);

  enqueue_kernel(ckFilterConstructTransform, task->storage.w, task->storage.h, 256);

  return true;
}

bool OpenCLDevice::denoising_accumulate(device_ptr color_ptr,
                                        device_ptr color_variance_ptr,
                                        device_ptr scale_ptr,
                                        int frame,
                                        DenoisingTask *task)
{
  cl_mem color_mem = CL_MEM_PTR(color_ptr);
  cl_mem color_variance_mem = CL_MEM_PTR(color_variance_ptr);
  cl_mem scale_mem = CL_MEM_PTR(scale_ptr);

  cl_mem buffer_mem = CL_MEM_PTR(task->buffer.mem.device_pointer);
  cl_mem transform_mem = CL_MEM_PTR(task->storage.transform.device_pointer);
  cl_mem rank_mem = CL_MEM_PTR(task->storage.rank.device_pointer);
  cl_mem XtWX_mem = CL_MEM_PTR(task->storage.XtWX.device_pointer);
  cl_mem XtWY_mem = CL_MEM_PTR(task->storage.XtWY.device_pointer);

  cl_kernel ckNLMCalcDifference = denoising_program(ustring("filter_nlm_calc_difference"));
  cl_kernel ckNLMBlur = denoising_program(ustring("filter_nlm_blur"));
  cl_kernel ckNLMCalcWeight = denoising_program(ustring("filter_nlm_calc_weight"));
  cl_kernel ckNLMConstructGramian = denoising_program(ustring("filter_nlm_construct_gramian"));

  int w = task->reconstruction_state.source_w;
  int h = task->reconstruction_state.source_h;
  int stride = task->buffer.stride;
  int frame_offset = frame * task->buffer.frame_stride;
  int t = task->tile_info->frames[frame];
  char use_time = task->buffer.use_time ? 1 : 0;

  int r = task->radius;
  int pass_stride = task->buffer.pass_stride;
  int num_shifts = (2 * r + 1) * (2 * r + 1);

  device_sub_ptr difference(task->buffer.temporary_mem, 0, pass_stride * num_shifts);
  device_sub_ptr blurDifference(
      task->buffer.temporary_mem, pass_stride * num_shifts, pass_stride * num_shifts);
  cl_mem difference_mem = CL_MEM_PTR(*difference);
  cl_mem blurDifference_mem = CL_MEM_PTR(*blurDifference);

  kernel_set_args(ckNLMCalcDifference,
                  0,
                  color_mem,
                  color_variance_mem,
                  scale_mem,
                  difference_mem,
                  w,
                  h,
                  stride,
                  pass_stride,
                  r,
                  pass_stride,
                  frame_offset,
                  1.0f,
                  task->nlm_k_2);
  kernel_set_args(
      ckNLMBlur, 0, difference_mem, blurDifference_mem, w, h, stride, pass_stride, r, 4);
  kernel_set_args(
      ckNLMCalcWeight, 0, blurDifference_mem, difference_mem, w, h, stride, pass_stride, r, 4);
  kernel_set_args(ckNLMConstructGramian,
                  0,
                  t,
                  blurDifference_mem,
                  buffer_mem,
                  transform_mem,
                  rank_mem,
                  XtWX_mem,
                  XtWY_mem,
                  task->reconstruction_state.filter_window,
                  w,
                  h,
                  stride,
                  pass_stride,
                  r,
                  4,
                  frame_offset,
                  use_time);

  enqueue_kernel(ckNLMCalcDifference, w * h, num_shifts, true);
  enqueue_kernel(ckNLMBlur, w * h, num_shifts, true);
  enqueue_kernel(ckNLMCalcWeight, w * h, num_shifts, true);
  enqueue_kernel(ckNLMBlur, w * h, num_shifts, true);
  enqueue_kernel(ckNLMConstructGramian, w * h, num_shifts, true, 256);

  return true;
}

bool OpenCLDevice::denoising_solve(device_ptr output_ptr, DenoisingTask *task)
{
  cl_kernel ckFinalize = denoising_program(ustring("filter_finalize"));

  cl_mem output_mem = CL_MEM_PTR(output_ptr);
  cl_mem rank_mem = CL_MEM_PTR(task->storage.rank.device_pointer);
  cl_mem XtWX_mem = CL_MEM_PTR(task->storage.XtWX.device_pointer);
  cl_mem XtWY_mem = CL_MEM_PTR(task->storage.XtWY.device_pointer);

  int w = task->reconstruction_state.source_w;
  int h = task->reconstruction_state.source_h;

  kernel_set_args(ckFinalize,
                  0,
                  output_mem,
                  rank_mem,
                  XtWX_mem,
                  XtWY_mem,
                  task->filter_area,
                  task->reconstruction_state.buffer_params,
                  task->render_buffer.samples);
  enqueue_kernel(ckFinalize, w, h);

  return true;
}

bool OpenCLDevice::denoising_combine_halves(device_ptr a_ptr,
                                            device_ptr b_ptr,
                                            device_ptr mean_ptr,
                                            device_ptr variance_ptr,
                                            int r,
                                            int4 rect,
                                            DenoisingTask *task)
{
  cl_mem a_mem = CL_MEM_PTR(a_ptr);
  cl_mem b_mem = CL_MEM_PTR(b_ptr);
  cl_mem mean_mem = CL_MEM_PTR(mean_ptr);
  cl_mem variance_mem = CL_MEM_PTR(variance_ptr);

  cl_kernel ckFilterCombineHalves = denoising_program(ustring("filter_combine_halves"));

  kernel_set_args(ckFilterCombineHalves, 0, mean_mem, variance_mem, a_mem, b_mem, rect, r);
  enqueue_kernel(ckFilterCombineHalves, task->rect.z - task->rect.x, task->rect.w - task->rect.y);

  return true;
}

bool OpenCLDevice::denoising_divide_shadow(device_ptr a_ptr,
                                           device_ptr b_ptr,
                                           device_ptr sample_variance_ptr,
                                           device_ptr sv_variance_ptr,
                                           device_ptr buffer_variance_ptr,
                                           DenoisingTask *task)
{
  cl_mem a_mem = CL_MEM_PTR(a_ptr);
  cl_mem b_mem = CL_MEM_PTR(b_ptr);
  cl_mem sample_variance_mem = CL_MEM_PTR(sample_variance_ptr);
  cl_mem sv_variance_mem = CL_MEM_PTR(sv_variance_ptr);
  cl_mem buffer_variance_mem = CL_MEM_PTR(buffer_variance_ptr);

  cl_mem tile_info_mem = CL_MEM_PTR(task->tile_info_mem.device_pointer);

  cl_kernel ckFilterDivideShadow = denoising_program(ustring("filter_divide_shadow"));

  int arg_ofs = kernel_set_args(
      ckFilterDivideShadow, 0, task->render_buffer.samples, tile_info_mem);
  cl_mem buffers[9];
  for (int i = 0; i < 9; i++) {
    buffers[i] = CL_MEM_PTR(task->tile_info->buffers[i]);
    arg_ofs += kernel_set_args(ckFilterDivideShadow, arg_ofs, buffers[i]);
  }
  kernel_set_args(ckFilterDivideShadow,
                  arg_ofs,
                  a_mem,
                  b_mem,
                  sample_variance_mem,
                  sv_variance_mem,
                  buffer_variance_mem,
                  task->rect,
                  task->render_buffer.pass_stride,
                  task->render_buffer.offset);
  enqueue_kernel(ckFilterDivideShadow, task->rect.z - task->rect.x, task->rect.w - task->rect.y);

  return true;
}

bool OpenCLDevice::denoising_get_feature(int mean_offset,
                                         int variance_offset,
                                         device_ptr mean_ptr,
                                         device_ptr variance_ptr,
                                         float scale,
                                         DenoisingTask *task)
{
  cl_mem mean_mem = CL_MEM_PTR(mean_ptr);
  cl_mem variance_mem = CL_MEM_PTR(variance_ptr);

  cl_mem tile_info_mem = CL_MEM_PTR(task->tile_info_mem.device_pointer);

  cl_kernel ckFilterGetFeature = denoising_program(ustring("filter_get_feature"));

  int arg_ofs = kernel_set_args(ckFilterGetFeature, 0, task->render_buffer.samples, tile_info_mem);
  cl_mem buffers[9];
  for (int i = 0; i < 9; i++) {
    buffers[i] = CL_MEM_PTR(task->tile_info->buffers[i]);
    arg_ofs += kernel_set_args(ckFilterGetFeature, arg_ofs, buffers[i]);
  }
  kernel_set_args(ckFilterGetFeature,
                  arg_ofs,
                  mean_offset,
                  variance_offset,
                  mean_mem,
                  variance_mem,
                  scale,
                  task->rect,
                  task->render_buffer.pass_stride,
                  task->render_buffer.offset);
  enqueue_kernel(ckFilterGetFeature, task->rect.z - task->rect.x, task->rect.w - task->rect.y);

  return true;
}

bool OpenCLDevice::denoising_write_feature(int out_offset,
                                           device_ptr from_ptr,
                                           device_ptr buffer_ptr,
                                           DenoisingTask *task)
{
  cl_mem from_mem = CL_MEM_PTR(from_ptr);
  cl_mem buffer_mem = CL_MEM_PTR(buffer_ptr);

  cl_kernel ckFilterWriteFeature = denoising_program(ustring("filter_write_feature"));

  kernel_set_args(ckFilterWriteFeature,
                  0,
                  task->render_buffer.samples,
                  task->reconstruction_state.buffer_params,
                  task->filter_area,
                  from_mem,
                  buffer_mem,
                  out_offset,
                  task->rect);
  enqueue_kernel(ckFilterWriteFeature, task->filter_area.z, task->filter_area.w);

  return true;
}

bool OpenCLDevice::denoising_detect_outliers(device_ptr image_ptr,
                                             device_ptr variance_ptr,
                                             device_ptr depth_ptr,
                                             device_ptr output_ptr,
                                             DenoisingTask *task)
{
  cl_mem image_mem = CL_MEM_PTR(image_ptr);
  cl_mem variance_mem = CL_MEM_PTR(variance_ptr);
  cl_mem depth_mem = CL_MEM_PTR(depth_ptr);
  cl_mem output_mem = CL_MEM_PTR(output_ptr);

  cl_kernel ckFilterDetectOutliers = denoising_program(ustring("filter_detect_outliers"));

  kernel_set_args(ckFilterDetectOutliers,
                  0,
                  image_mem,
                  variance_mem,
                  depth_mem,
                  output_mem,
                  task->rect,
                  task->buffer.pass_stride);
  enqueue_kernel(ckFilterDetectOutliers, task->rect.z - task->rect.x, task->rect.w - task->rect.y);

  return true;
}

void OpenCLDevice::denoise(RenderTile &rtile, DenoisingTask &denoising)
{
  denoising.functions.construct_transform = function_bind(
      &OpenCLDevice::denoising_construct_transform, this, &denoising);
  denoising.functions.accumulate = function_bind(
      &OpenCLDevice::denoising_accumulate, this, _1, _2, _3, _4, &denoising);
  denoising.functions.solve = function_bind(&OpenCLDevice::denoising_solve, this, _1, &denoising);
  denoising.functions.divide_shadow = function_bind(
      &OpenCLDevice::denoising_divide_shadow, this, _1, _2, _3, _4, _5, &denoising);
  denoising.functions.non_local_means = function_bind(
      &OpenCLDevice::denoising_non_local_means, this, _1, _2, _3, _4, &denoising);
  denoising.functions.combine_halves = function_bind(
      &OpenCLDevice::denoising_combine_halves, this, _1, _2, _3, _4, _5, _6, &denoising);
  denoising.functions.get_feature = function_bind(
      &OpenCLDevice::denoising_get_feature, this, _1, _2, _3, _4, _5, &denoising);
  denoising.functions.write_feature = function_bind(
      &OpenCLDevice::denoising_write_feature, this, _1, _2, _3, &denoising);
  denoising.functions.detect_outliers = function_bind(
      &OpenCLDevice::denoising_detect_outliers, this, _1, _2, _3, _4, &denoising);

  denoising.filter_area = make_int4(rtile.x, rtile.y, rtile.w, rtile.h);
  denoising.render_buffer.samples = rtile.sample;
  denoising.buffer.gpu_temporary_mem = true;

  denoising.run_denoising(&rtile);
}

void OpenCLDevice::shader(DeviceTask &task)
{
  /* cast arguments to cl types */
  cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
  cl_mem d_input = CL_MEM_PTR(task.shader_input);
  cl_mem d_output = CL_MEM_PTR(task.shader_output);
  cl_int d_shader_eval_type = task.shader_eval_type;
  cl_int d_shader_filter = task.shader_filter;
  cl_int d_shader_x = task.shader_x;
  cl_int d_shader_w = task.shader_w;
  cl_int d_offset = task.offset;

  OpenCLDevice::OpenCLProgram *program = &background_program;
  if (task.shader_eval_type == SHADER_EVAL_DISPLACE) {
    program = &displace_program;
  }
  program->wait_for_availability();
  cl_kernel kernel = (*program)();

  cl_uint start_arg_index = kernel_set_args(kernel, 0, d_data, d_input, d_output);

  set_kernel_arg_buffers(kernel, &start_arg_index);

  start_arg_index += kernel_set_args(kernel, start_arg_index, d_shader_eval_type);
  if (task.shader_eval_type >= SHADER_EVAL_BAKE) {
    start_arg_index += kernel_set_args(kernel, start_arg_index, d_shader_filter);
  }
  start_arg_index += kernel_set_args(kernel, start_arg_index, d_shader_x, d_shader_w, d_offset);

  for (int sample = 0; sample < task.num_samples; sample++) {

    if (task.get_cancel())
      break;

    kernel_set_args(kernel, start_arg_index, sample);

    enqueue_kernel(kernel, task.shader_w, 1);

    clFinish(cqCommandQueue);

    task.update_progress(NULL);
  }
}

void OpenCLDevice::bake(DeviceTask &task, RenderTile &rtile)
{
  scoped_timer timer(&rtile.buffers->render_time);

  /* Cast arguments to cl types. */
  cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
  cl_mem d_buffer = CL_MEM_PTR(rtile.buffer);
  cl_int d_x = rtile.x;
  cl_int d_y = rtile.y;
  cl_int d_w = rtile.w;
  cl_int d_h = rtile.h;
  cl_int d_offset = rtile.offset;
  cl_int d_stride = rtile.stride;

  bake_program.wait_for_availability();
  cl_kernel kernel = bake_program();

  cl_uint start_arg_index = kernel_set_args(kernel, 0, d_data, d_buffer);

  set_kernel_arg_buffers(kernel, &start_arg_index);

  start_arg_index += kernel_set_args(
      kernel, start_arg_index, d_x, d_y, d_w, d_h, d_offset, d_stride);

  int start_sample = rtile.start_sample;
  int end_sample = rtile.start_sample + rtile.num_samples;

  for (int sample = start_sample; sample < end_sample; sample++) {
    if (task.get_cancel()) {
      if (task.need_finish_queue == false)
        break;
    }

    kernel_set_args(kernel, start_arg_index, sample);

    enqueue_kernel(kernel, d_w, d_h);

    rtile.sample = sample + 1;

    task.update_progress(&rtile, rtile.w * rtile.h);
  }

  clFinish(cqCommandQueue);
}

string OpenCLDevice::kernel_build_options(const string *debug_src)
{
  string build_options = "-cl-no-signed-zeros -cl-mad-enable ";

  /* Build with OpenCL 2.0 if available, this improves performance
   * with AMD OpenCL drivers on Windows and Linux (legacy drivers).
   * Note that OpenCL selects the highest 1.x version by default,
   * only for 2.0 do we need the explicit compiler flag. */
  int version_major, version_minor;
  if (OpenCLInfo::get_device_version(cdDevice, &version_major, &version_minor)) {
    if (version_major >= 2) {
      /* This appears to trigger a driver bug in Radeon RX cards, so we
       * don't use OpenCL 2.0 for those. */
      string device_name = OpenCLInfo::get_readable_device_name(cdDevice);
      if (!(string_startswith(device_name, "Radeon RX 4") ||
            string_startswith(device_name, "Radeon (TM) RX 4") ||
            string_startswith(device_name, "Radeon RX 5") ||
            string_startswith(device_name, "Radeon (TM) RX 5"))) {
        build_options += "-cl-std=CL2.0 ";
      }
    }
  }

  if (platform_name == "NVIDIA CUDA") {
    build_options +=
        "-D__KERNEL_OPENCL_NVIDIA__ "
        "-cl-nv-maxrregcount=32 "
        "-cl-nv-verbose ";

    uint compute_capability_major, compute_capability_minor;
    clGetDeviceInfo(cdDevice,
                    CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV,
                    sizeof(cl_uint),
                    &compute_capability_major,
                    NULL);
    clGetDeviceInfo(cdDevice,
                    CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV,
                    sizeof(cl_uint),
                    &compute_capability_minor,
                    NULL);

    build_options += string_printf("-D__COMPUTE_CAPABILITY__=%u ",
                                   compute_capability_major * 100 + compute_capability_minor * 10);
  }

  else if (platform_name == "Apple")
    build_options += "-D__KERNEL_OPENCL_APPLE__ ";

  else if (platform_name == "AMD Accelerated Parallel Processing")
    build_options += "-D__KERNEL_OPENCL_AMD__ ";

  else if (platform_name == "Intel(R) OpenCL") {
    build_options += "-D__KERNEL_OPENCL_INTEL_CPU__ ";

    /* Options for gdb source level kernel debugging.
     * this segfaults on linux currently.
     */
    if (OpenCLInfo::use_debug() && debug_src)
      build_options += "-g -s \"" + *debug_src + "\" ";
  }

  if (info.has_half_images) {
    build_options += "-D__KERNEL_CL_KHR_FP16__ ";
  }

  if (OpenCLInfo::use_debug()) {
    build_options += "-D__KERNEL_OPENCL_DEBUG__ ";
  }

#  ifdef WITH_CYCLES_DEBUG
  build_options += "-D__KERNEL_DEBUG__ ";
#  endif

  return build_options;
}

/* TODO(sergey): In the future we can use variadic templates, once
 * C++0x is allowed. Should allow to clean this up a bit.
 */
int OpenCLDevice::kernel_set_args(cl_kernel kernel,
                                  int start_argument_index,
                                  const ArgumentWrapper &arg1,
                                  const ArgumentWrapper &arg2,
                                  const ArgumentWrapper &arg3,
                                  const ArgumentWrapper &arg4,
                                  const ArgumentWrapper &arg5,
                                  const ArgumentWrapper &arg6,
                                  const ArgumentWrapper &arg7,
                                  const ArgumentWrapper &arg8,
                                  const ArgumentWrapper &arg9,
                                  const ArgumentWrapper &arg10,
                                  const ArgumentWrapper &arg11,
                                  const ArgumentWrapper &arg12,
                                  const ArgumentWrapper &arg13,
                                  const ArgumentWrapper &arg14,
                                  const ArgumentWrapper &arg15,
                                  const ArgumentWrapper &arg16,
                                  const ArgumentWrapper &arg17,
                                  const ArgumentWrapper &arg18,
                                  const ArgumentWrapper &arg19,
                                  const ArgumentWrapper &arg20,
                                  const ArgumentWrapper &arg21,
                                  const ArgumentWrapper &arg22,
                                  const ArgumentWrapper &arg23,
                                  const ArgumentWrapper &arg24,
                                  const ArgumentWrapper &arg25,
                                  const ArgumentWrapper &arg26,
                                  const ArgumentWrapper &arg27,
                                  const ArgumentWrapper &arg28,
                                  const ArgumentWrapper &arg29,
                                  const ArgumentWrapper &arg30,
                                  const ArgumentWrapper &arg31,
                                  const ArgumentWrapper &arg32,
                                  const ArgumentWrapper &arg33)
{
  int current_arg_index = 0;
#  define FAKE_VARARG_HANDLE_ARG(arg) \
    do { \
      if (arg.pointer != NULL) { \
        opencl_assert(clSetKernelArg( \
            kernel, start_argument_index + current_arg_index, arg.size, arg.pointer)); \
        ++current_arg_index; \
      } \
      else { \
        return current_arg_index; \
      } \
    } while (false)
  FAKE_VARARG_HANDLE_ARG(arg1);
  FAKE_VARARG_HANDLE_ARG(arg2);
  FAKE_VARARG_HANDLE_ARG(arg3);
  FAKE_VARARG_HANDLE_ARG(arg4);
  FAKE_VARARG_HANDLE_ARG(arg5);
  FAKE_VARARG_HANDLE_ARG(arg6);
  FAKE_VARARG_HANDLE_ARG(arg7);
  FAKE_VARARG_HANDLE_ARG(arg8);
  FAKE_VARARG_HANDLE_ARG(arg9);
  FAKE_VARARG_HANDLE_ARG(arg10);
  FAKE_VARARG_HANDLE_ARG(arg11);
  FAKE_VARARG_HANDLE_ARG(arg12);
  FAKE_VARARG_HANDLE_ARG(arg13);
  FAKE_VARARG_HANDLE_ARG(arg14);
  FAKE_VARARG_HANDLE_ARG(arg15);
  FAKE_VARARG_HANDLE_ARG(arg16);
  FAKE_VARARG_HANDLE_ARG(arg17);
  FAKE_VARARG_HANDLE_ARG(arg18);
  FAKE_VARARG_HANDLE_ARG(arg19);
  FAKE_VARARG_HANDLE_ARG(arg20);
  FAKE_VARARG_HANDLE_ARG(arg21);
  FAKE_VARARG_HANDLE_ARG(arg22);
  FAKE_VARARG_HANDLE_ARG(arg23);
  FAKE_VARARG_HANDLE_ARG(arg24);
  FAKE_VARARG_HANDLE_ARG(arg25);
  FAKE_VARARG_HANDLE_ARG(arg26);
  FAKE_VARARG_HANDLE_ARG(arg27);
  FAKE_VARARG_HANDLE_ARG(arg28);
  FAKE_VARARG_HANDLE_ARG(arg29);
  FAKE_VARARG_HANDLE_ARG(arg30);
  FAKE_VARARG_HANDLE_ARG(arg31);
  FAKE_VARARG_HANDLE_ARG(arg32);
  FAKE_VARARG_HANDLE_ARG(arg33);
#  undef FAKE_VARARG_HANDLE_ARG
  return current_arg_index;
}

void OpenCLDevice::release_kernel_safe(cl_kernel kernel)
{
  if (kernel) {
    clReleaseKernel(kernel);
  }
}

void OpenCLDevice::release_mem_object_safe(cl_mem mem)
{
  if (mem != NULL) {
    clReleaseMemObject(mem);
  }
}

void OpenCLDevice::release_program_safe(cl_program program)
{
  if (program) {
    clReleaseProgram(program);
  }
}

/* ** Those guys are for workign around some compiler-specific bugs ** */

cl_program OpenCLDevice::load_cached_kernel(ustring key, thread_scoped_lock &cache_locker)
{
  return OpenCLCache::get_program(cpPlatform, cdDevice, key, cache_locker);
}

void OpenCLDevice::store_cached_kernel(cl_program program,
                                       ustring key,
                                       thread_scoped_lock &cache_locker)
{
  OpenCLCache::store_program(cpPlatform, cdDevice, program, key, cache_locker);
}

Device *opencl_create_split_device(DeviceInfo &info,
                                   Stats &stats,
                                   Profiler &profiler,
                                   bool background)
{
  return new OpenCLDevice(info, stats, profiler, background);
}

CCL_NAMESPACE_END

#endif
