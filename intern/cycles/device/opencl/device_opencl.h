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

#  include "device/device.h"
#  include "device/device_denoising.h"
#  include "device/device_split_kernel.h"

#  include "util/util_map.h"
#  include "util/util_param.h"
#  include "util/util_string.h"
#  include "util/util_task.h"

#  include "clew.h"

#  include "device/opencl/memory_manager.h"

CCL_NAMESPACE_BEGIN

/* Disable workarounds, seems to be working fine on latest drivers. */
#  define CYCLES_DISABLE_DRIVER_WORKAROUNDS

/* Define CYCLES_DISABLE_DRIVER_WORKAROUNDS to disable workaounds for testing */
#  ifndef CYCLES_DISABLE_DRIVER_WORKAROUNDS
/* Work around AMD driver hangs by ensuring each command is finished before doing anything else. */
#    undef clEnqueueNDRangeKernel
#    define clEnqueueNDRangeKernel(a, b, c, d, e, f, g, h, i) \
      CLEW_GET_FUN(__clewEnqueueNDRangeKernel)(a, b, c, d, e, f, g, h, i); \
      clFinish(a);

#    undef clEnqueueWriteBuffer
#    define clEnqueueWriteBuffer(a, b, c, d, e, f, g, h, i) \
      CLEW_GET_FUN(__clewEnqueueWriteBuffer)(a, b, c, d, e, f, g, h, i); \
      clFinish(a);

#    undef clEnqueueReadBuffer
#    define clEnqueueReadBuffer(a, b, c, d, e, f, g, h, i) \
      CLEW_GET_FUN(__clewEnqueueReadBuffer)(a, b, c, d, e, f, g, h, i); \
      clFinish(a);
#  endif /* CYCLES_DISABLE_DRIVER_WORKAROUNDS */

#  define CL_MEM_PTR(p) ((cl_mem)(uintptr_t)(p))

struct OpenCLPlatformDevice {
  OpenCLPlatformDevice(cl_platform_id platform_id,
                       const string &platform_name,
                       cl_device_id device_id,
                       cl_device_type device_type,
                       const string &device_name,
                       const string &hardware_id,
                       const string &device_extensions)
      : platform_id(platform_id),
        platform_name(platform_name),
        device_id(device_id),
        device_type(device_type),
        device_name(device_name),
        hardware_id(hardware_id),
        device_extensions(device_extensions)
  {
  }
  cl_platform_id platform_id;
  string platform_name;
  cl_device_id device_id;
  cl_device_type device_type;
  string device_name;
  string hardware_id;
  string device_extensions;
};

/* Contains all static OpenCL helper functions. */
class OpenCLInfo {
 public:
  static cl_device_type device_type();
  static bool use_debug();
  static bool device_supported(const string &platform_name, const cl_device_id device_id);
  static bool platform_version_check(cl_platform_id platform, string *error = NULL);
  static bool device_version_check(cl_device_id device, string *error = NULL);
  static bool get_device_version(cl_device_id device,
                                 int *r_major,
                                 int *r_minor,
                                 string *error = NULL);
  static string get_hardware_id(const string &platform_name, cl_device_id device_id);
  static void get_usable_devices(vector<OpenCLPlatformDevice> *usable_devices);

  /* ** Some handy shortcuts to low level cl*GetInfo() functions. ** */

  /* Platform information. */
  static bool get_num_platforms(cl_uint *num_platforms, cl_int *error = NULL);
  static cl_uint get_num_platforms();

  static bool get_platforms(vector<cl_platform_id> *platform_ids, cl_int *error = NULL);
  static vector<cl_platform_id> get_platforms();

  static bool get_platform_name(cl_platform_id platform_id, string *platform_name);
  static string get_platform_name(cl_platform_id platform_id);

  static bool get_num_platform_devices(cl_platform_id platform_id,
                                       cl_device_type device_type,
                                       cl_uint *num_devices,
                                       cl_int *error = NULL);
  static cl_uint get_num_platform_devices(cl_platform_id platform_id, cl_device_type device_type);

  static bool get_platform_devices(cl_platform_id platform_id,
                                   cl_device_type device_type,
                                   vector<cl_device_id> *device_ids,
                                   cl_int *error = NULL);
  static vector<cl_device_id> get_platform_devices(cl_platform_id platform_id,
                                                   cl_device_type device_type);

  /* Device information. */
  static bool get_device_name(cl_device_id device_id, string *device_name, cl_int *error = NULL);

  static string get_device_name(cl_device_id device_id);

  static bool get_device_extensions(cl_device_id device_id,
                                    string *device_extensions,
                                    cl_int *error = NULL);

  static string get_device_extensions(cl_device_id device_id);

  static bool get_device_type(cl_device_id device_id,
                              cl_device_type *device_type,
                              cl_int *error = NULL);
  static cl_device_type get_device_type(cl_device_id device_id);

  static bool get_driver_version(cl_device_id device_id,
                                 int *major,
                                 int *minor,
                                 cl_int *error = NULL);

  static int mem_sub_ptr_alignment(cl_device_id device_id);

  /* Get somewhat more readable device name.
   * Main difference is AMD OpenCL here which only gives code name
   * for the regular device name. This will give more sane device
   * name using some extensions.
   */
  static string get_readable_device_name(cl_device_id device_id);
};

/* Thread safe cache for contexts and programs.
 */
class OpenCLCache {
  struct Slot {
    struct ProgramEntry {
      ProgramEntry();
      ProgramEntry(const ProgramEntry &rhs);
      ~ProgramEntry();
      cl_program program;
      thread_mutex *mutex;
    };

    Slot();
    Slot(const Slot &rhs);
    ~Slot();

    thread_mutex *context_mutex;
    cl_context context;
    typedef map<ustring, ProgramEntry> EntryMap;
    EntryMap programs;
  };

  /* key is combination of platform ID and device ID */
  typedef pair<cl_platform_id, cl_device_id> PlatformDevicePair;

  /* map of Slot objects */
  typedef map<PlatformDevicePair, Slot> CacheMap;
  CacheMap cache;

  /* MD5 hash of the kernel source. */
  string kernel_md5;

  thread_mutex cache_lock;
  thread_mutex kernel_md5_lock;

  /* lazy instantiate */
  static OpenCLCache &global_instance();

 public:
  enum ProgramName {
    OCL_DEV_BASE_PROGRAM,
    OCL_DEV_MEGAKERNEL_PROGRAM,
  };

  /* Lookup context in the cache. If this returns NULL, slot_locker
   * will be holding a lock for the cache. slot_locker should refer to a
   * default constructed thread_scoped_lock. */
  static cl_context get_context(cl_platform_id platform,
                                cl_device_id device,
                                thread_scoped_lock &slot_locker);
  /* Same as above. */
  static cl_program get_program(cl_platform_id platform,
                                cl_device_id device,
                                ustring key,
                                thread_scoped_lock &slot_locker);

  /* Store context in the cache. You MUST have tried to get the item before storing to it. */
  static void store_context(cl_platform_id platform,
                            cl_device_id device,
                            cl_context context,
                            thread_scoped_lock &slot_locker);
  /* Same as above. */
  static void store_program(cl_platform_id platform,
                            cl_device_id device,
                            cl_program program,
                            ustring key,
                            thread_scoped_lock &slot_locker);

  static string get_kernel_md5();
};

#  define opencl_device_assert(device, stmt) \
    { \
      cl_int err = stmt; \
\
      if (err != CL_SUCCESS) { \
        string message = string_printf( \
            "OpenCL error: %s in %s (%s:%d)", clewErrorString(err), #stmt, __FILE__, __LINE__); \
        if ((device)->error_message() == "") \
          (device)->set_error(message); \
        fprintf(stderr, "%s\n", message.c_str()); \
      } \
    } \
    (void)0

#  define opencl_assert(stmt) \
    { \
      cl_int err = stmt; \
\
      if (err != CL_SUCCESS) { \
        string message = string_printf( \
            "OpenCL error: %s in %s (%s:%d)", clewErrorString(err), #stmt, __FILE__, __LINE__); \
        if (error_msg == "") \
          error_msg = message; \
        fprintf(stderr, "%s\n", message.c_str()); \
      } \
    } \
    (void)0

class OpenCLDevice : public Device {
 public:
  DedicatedTaskPool task_pool;

  /* Task pool for required kernels (base, AO kernels during foreground rendering) */
  TaskPool load_required_kernel_task_pool;
  /* Task pool for optional kernels (feature kernels during foreground rendering) */
  TaskPool load_kernel_task_pool;
  cl_context cxContext;
  cl_command_queue cqCommandQueue;
  cl_platform_id cpPlatform;
  cl_device_id cdDevice;
  cl_int ciErr;
  int device_num;
  bool use_preview_kernels;

  class OpenCLProgram {
   public:
    OpenCLProgram() : loaded(false), needs_compiling(true), program(NULL), device(NULL)
    {
    }
    OpenCLProgram(OpenCLDevice *device,
                  const string &program_name,
                  const string &kernel_name,
                  const string &kernel_build_options,
                  bool use_stdout = true);
    ~OpenCLProgram();

    void add_kernel(ustring name);

    /* Try to load the program from device cache or disk */
    bool load();
    /* Compile the kernel (first separate, failback to local) */
    void compile();
    /* Create the OpenCL kernels after loading or compiling */
    void create_kernels();

    bool is_loaded() const
    {
      return loaded;
    }
    const string &get_log() const
    {
      return log;
    }
    void report_error();

    /* Wait until this kernel is available to be used
     * It will return true when the kernel is available.
     * It will return false when the kernel is not available
     * or could not be loaded. */
    bool wait_for_availability();

    cl_kernel operator()();
    cl_kernel operator()(ustring name);

    void release();

   private:
    bool build_kernel(const string *debug_src);
    /* Build the program by calling the own process.
     * This is required for multithreaded OpenCL compilation, since most Frameworks serialize
     * build calls internally if they come from the same process.
     * If that is not supported, this function just returns false.
     */
    bool compile_separate(const string &clbin);
    /* Build the program by calling OpenCL directly. */
    bool compile_kernel(const string *debug_src);
    /* Loading and saving the program from/to disk. */
    bool load_binary(const string &clbin, const string *debug_src = NULL);
    bool save_binary(const string &clbin);

    void add_log(const string &msg, bool is_debug);
    void add_error(const string &msg);

    bool loaded;
    bool needs_compiling;

    cl_program program;
    OpenCLDevice *device;

    /* Used for the OpenCLCache key. */
    string program_name;

    string kernel_file, kernel_build_options, device_md5;

    bool use_stdout;
    string log, error_msg;
    string compile_output;

    map<ustring, cl_kernel> kernels;
  };

  /* Container for all types of split programs. */
  class OpenCLSplitPrograms {
   public:
    OpenCLDevice *device;
    OpenCLProgram program_split;
    OpenCLProgram program_lamp_emission;
    OpenCLProgram program_do_volume;
    OpenCLProgram program_indirect_background;
    OpenCLProgram program_shader_eval;
    OpenCLProgram program_holdout_emission_blurring_pathtermination_ao;
    OpenCLProgram program_subsurface_scatter;
    OpenCLProgram program_direct_lighting;
    OpenCLProgram program_shadow_blocked_ao;
    OpenCLProgram program_shadow_blocked_dl;

    OpenCLSplitPrograms(OpenCLDevice *device);
    ~OpenCLSplitPrograms();

    /* Load the kernels and put the created kernels in the given
     * `programs` parameter. */
    void load_kernels(vector<OpenCLProgram *> &programs,
                      const DeviceRequestedFeatures &requested_features,
                      bool is_preview = false);
  };

  DeviceSplitKernel *split_kernel;

  OpenCLProgram base_program;
  OpenCLProgram bake_program;
  OpenCLProgram displace_program;
  OpenCLProgram background_program;
  OpenCLProgram denoising_program;

  OpenCLSplitPrograms kernel_programs;
  OpenCLSplitPrograms preview_programs;

  typedef map<string, device_vector<uchar> *> ConstMemMap;
  typedef map<string, device_ptr> MemMap;

  ConstMemMap const_mem_map;
  MemMap mem_map;

  bool device_initialized;
  string platform_name;
  string device_name;

  bool opencl_error(cl_int err);
  void opencl_error(const string &message);
  void opencl_assert_err(cl_int err, const char *where);

  OpenCLDevice(DeviceInfo &info, Stats &stats, Profiler &profiler, bool background);
  ~OpenCLDevice();

  static void CL_CALLBACK context_notify_callback(const char *err_info,
                                                  const void * /*private_info*/,
                                                  size_t /*cb*/,
                                                  void *user_data);

  bool opencl_version_check();
  OpenCLSplitPrograms *get_split_programs();

  string device_md5_hash(string kernel_custom_build_options = "");
  bool load_kernels(const DeviceRequestedFeatures &requested_features);
  void load_required_kernels(const DeviceRequestedFeatures &requested_features);
  void load_preview_kernels();

  bool wait_for_availability(const DeviceRequestedFeatures &requested_features);
  DeviceKernelStatus get_active_kernel_switch_state();

  /* Get the name of the opencl program for the given kernel */
  const string get_opencl_program_name(const string &kernel_name);
  /* Get the program file name to compile (*.cl) for the given kernel */
  const string get_opencl_program_filename(const string &kernel_name);
  string get_build_options(const DeviceRequestedFeatures &requested_features,
                           const string &opencl_program_name,
                           bool preview_kernel = false);
  /* Enable the default features to reduce recompilation events */
  void enable_default_features(DeviceRequestedFeatures &features);

  void mem_alloc(device_memory &mem);
  void mem_copy_to(device_memory &mem);
  void mem_copy_from(device_memory &mem, int y, int w, int h, int elem);
  void mem_zero(device_memory &mem);
  void mem_free(device_memory &mem);

  int mem_sub_ptr_alignment();

  void const_copy_to(const char *name, void *host, size_t size);
  void global_alloc(device_memory &mem);
  void global_free(device_memory &mem);
  void tex_alloc(device_texture &mem);
  void tex_free(device_texture &mem);

  size_t global_size_round_up(int group_size, int global_size);
  void enqueue_kernel(cl_kernel kernel,
                      size_t w,
                      size_t h,
                      bool x_workgroups = false,
                      size_t max_workgroup_size = -1);
  void set_kernel_arg_mem(cl_kernel kernel, cl_uint *narg, const char *name);
  void set_kernel_arg_buffers(cl_kernel kernel, cl_uint *narg);

  void film_convert(DeviceTask &task,
                    device_ptr buffer,
                    device_ptr rgba_byte,
                    device_ptr rgba_half);
  void shader(DeviceTask &task);
  void update_adaptive(DeviceTask &task, RenderTile &tile, int sample);
  void bake(DeviceTask &task, RenderTile &tile);

  void denoise(RenderTile &tile, DenoisingTask &denoising);

  class OpenCLDeviceTask : public Task {
   public:
    OpenCLDeviceTask(OpenCLDevice *device, DeviceTask &task) : task(task)
    {
      run = function_bind(&OpenCLDevice::thread_run, device, task);
    }

    DeviceTask task;
  };

  int get_split_task_count(DeviceTask & /*task*/)
  {
    return 1;
  }

  void task_add(DeviceTask &task)
  {
    task_pool.push(new OpenCLDeviceTask(this, task));
  }

  void task_wait()
  {
    task_pool.wait();
  }

  void task_cancel()
  {
    task_pool.cancel();
  }

  void thread_run(DeviceTask &task);

  virtual BVHLayoutMask get_bvh_layout_mask() const
  {
    return BVH_LAYOUT_BVH2;
  }

  virtual bool show_samples() const
  {
    return true;
  }

 protected:
  string kernel_build_options(const string *debug_src = NULL);

  void mem_zero_kernel(device_ptr ptr, size_t size);

  bool denoising_non_local_means(device_ptr image_ptr,
                                 device_ptr guide_ptr,
                                 device_ptr variance_ptr,
                                 device_ptr out_ptr,
                                 DenoisingTask *task);
  bool denoising_construct_transform(DenoisingTask *task);
  bool denoising_accumulate(device_ptr color_ptr,
                            device_ptr color_variance_ptr,
                            device_ptr scale_ptr,
                            int frame,
                            DenoisingTask *task);
  bool denoising_solve(device_ptr output_ptr, DenoisingTask *task);
  bool denoising_combine_halves(device_ptr a_ptr,
                                device_ptr b_ptr,
                                device_ptr mean_ptr,
                                device_ptr variance_ptr,
                                int r,
                                int4 rect,
                                DenoisingTask *task);
  bool denoising_divide_shadow(device_ptr a_ptr,
                               device_ptr b_ptr,
                               device_ptr sample_variance_ptr,
                               device_ptr sv_variance_ptr,
                               device_ptr buffer_variance_ptr,
                               DenoisingTask *task);
  bool denoising_get_feature(int mean_offset,
                             int variance_offset,
                             device_ptr mean_ptr,
                             device_ptr variance_ptr,
                             float scale,
                             DenoisingTask *task);
  bool denoising_write_feature(int to_offset,
                               device_ptr from_ptr,
                               device_ptr buffer_ptr,
                               DenoisingTask *task);
  bool denoising_detect_outliers(device_ptr image_ptr,
                                 device_ptr variance_ptr,
                                 device_ptr depth_ptr,
                                 device_ptr output_ptr,
                                 DenoisingTask *task);

  device_ptr mem_alloc_sub_ptr(device_memory &mem, int offset, int size);
  void mem_free_sub_ptr(device_ptr ptr);

  class ArgumentWrapper {
   public:
    ArgumentWrapper() : size(0), pointer(NULL)
    {
    }

    ArgumentWrapper(device_memory &argument)
        : size(sizeof(void *)), pointer((void *)(&argument.device_pointer))
    {
    }

    template<typename T>
    ArgumentWrapper(device_vector<T> &argument)
        : size(sizeof(void *)), pointer((void *)(&argument.device_pointer))
    {
    }

    template<typename T>
    ArgumentWrapper(device_only_memory<T> &argument)
        : size(sizeof(void *)), pointer((void *)(&argument.device_pointer))
    {
    }
    template<typename T> ArgumentWrapper(T &argument) : size(sizeof(argument)), pointer(&argument)
    {
    }

    ArgumentWrapper(int argument) : size(sizeof(int)), int_value(argument), pointer(&int_value)
    {
    }

    ArgumentWrapper(float argument)
        : size(sizeof(float)), float_value(argument), pointer(&float_value)
    {
    }

    size_t size;
    int int_value;
    float float_value;
    void *pointer;
  };

  /* TODO(sergey): In the future we can use variadic templates, once
   * C++0x is allowed. Should allow to clean this up a bit.
   */
  int kernel_set_args(cl_kernel kernel,
                      int start_argument_index,
                      const ArgumentWrapper &arg1 = ArgumentWrapper(),
                      const ArgumentWrapper &arg2 = ArgumentWrapper(),
                      const ArgumentWrapper &arg3 = ArgumentWrapper(),
                      const ArgumentWrapper &arg4 = ArgumentWrapper(),
                      const ArgumentWrapper &arg5 = ArgumentWrapper(),
                      const ArgumentWrapper &arg6 = ArgumentWrapper(),
                      const ArgumentWrapper &arg7 = ArgumentWrapper(),
                      const ArgumentWrapper &arg8 = ArgumentWrapper(),
                      const ArgumentWrapper &arg9 = ArgumentWrapper(),
                      const ArgumentWrapper &arg10 = ArgumentWrapper(),
                      const ArgumentWrapper &arg11 = ArgumentWrapper(),
                      const ArgumentWrapper &arg12 = ArgumentWrapper(),
                      const ArgumentWrapper &arg13 = ArgumentWrapper(),
                      const ArgumentWrapper &arg14 = ArgumentWrapper(),
                      const ArgumentWrapper &arg15 = ArgumentWrapper(),
                      const ArgumentWrapper &arg16 = ArgumentWrapper(),
                      const ArgumentWrapper &arg17 = ArgumentWrapper(),
                      const ArgumentWrapper &arg18 = ArgumentWrapper(),
                      const ArgumentWrapper &arg19 = ArgumentWrapper(),
                      const ArgumentWrapper &arg20 = ArgumentWrapper(),
                      const ArgumentWrapper &arg21 = ArgumentWrapper(),
                      const ArgumentWrapper &arg22 = ArgumentWrapper(),
                      const ArgumentWrapper &arg23 = ArgumentWrapper(),
                      const ArgumentWrapper &arg24 = ArgumentWrapper(),
                      const ArgumentWrapper &arg25 = ArgumentWrapper(),
                      const ArgumentWrapper &arg26 = ArgumentWrapper(),
                      const ArgumentWrapper &arg27 = ArgumentWrapper(),
                      const ArgumentWrapper &arg28 = ArgumentWrapper(),
                      const ArgumentWrapper &arg29 = ArgumentWrapper(),
                      const ArgumentWrapper &arg30 = ArgumentWrapper(),
                      const ArgumentWrapper &arg31 = ArgumentWrapper(),
                      const ArgumentWrapper &arg32 = ArgumentWrapper(),
                      const ArgumentWrapper &arg33 = ArgumentWrapper());

  void release_kernel_safe(cl_kernel kernel);
  void release_mem_object_safe(cl_mem mem);
  void release_program_safe(cl_program program);

  /* ** Those guys are for workign around some compiler-specific bugs ** */

  cl_program load_cached_kernel(ustring key, thread_scoped_lock &cache_locker);

  void store_cached_kernel(cl_program program, ustring key, thread_scoped_lock &cache_locker);

 private:
  MemoryManager memory_manager;
  friend class MemoryManager;

  static_assert_align(TextureInfo, 16);
  device_vector<TextureInfo> texture_info;

  typedef map<string, device_memory *> TexturesMap;
  TexturesMap textures;

  bool textures_need_update;

 protected:
  void flush_texture_buffers();

  friend class OpenCLSplitKernel;
  friend class OpenCLSplitKernelFunction;
};

Device *opencl_create_split_device(DeviceInfo &info,
                                   Stats &stats,
                                   Profiler &profiler,
                                   bool background);

CCL_NAMESPACE_END

#endif
