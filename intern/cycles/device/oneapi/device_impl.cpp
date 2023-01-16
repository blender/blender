/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Intel Corporation */

#ifdef WITH_ONEAPI

#  include "device/oneapi/device_impl.h"

#  include "util/debug.h"
#  include "util/log.h"

#  include "kernel/device/oneapi/globals.h"

CCL_NAMESPACE_BEGIN

static void queue_error_cb(const char *message, void *user_ptr)
{
  if (user_ptr) {
    *reinterpret_cast<std::string *>(user_ptr) = message;
  }
}

OneapiDevice::OneapiDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler)
    : Device(info, stats, profiler),
      device_queue_(nullptr),
      texture_info_(this, "texture_info", MEM_GLOBAL),
      kg_memory_(nullptr),
      kg_memory_device_(nullptr),
      kg_memory_size_(0)
{
  need_texture_info_ = false;

  oneapi_set_error_cb(queue_error_cb, &oneapi_error_string_);

  bool is_finished_ok = create_queue(device_queue_, info.num);
  if (is_finished_ok == false) {
    set_error("oneAPI queue initialization error: got runtime exception \"" +
              oneapi_error_string_ + "\"");
  }
  else {
    VLOG_DEBUG << "oneAPI queue has been successfully created for the device \""
               << info.description << "\"";
    assert(device_queue_);
  }

  size_t globals_segment_size;
  is_finished_ok = kernel_globals_size(globals_segment_size);
  if (is_finished_ok == false) {
    set_error("oneAPI constant memory initialization got runtime exception \"" +
              oneapi_error_string_ + "\"");
  }
  else {
    VLOG_DEBUG << "Successfully created global/constant memory segment (kernel globals object)";
  }

  kg_memory_ = usm_aligned_alloc_host(device_queue_, globals_segment_size, 16);
  usm_memset(device_queue_, kg_memory_, 0, globals_segment_size);

  kg_memory_device_ = usm_alloc_device(device_queue_, globals_segment_size);

  kg_memory_size_ = globals_segment_size;

  max_memory_on_device_ = get_memcapacity();
}

OneapiDevice::~OneapiDevice()
{
  texture_info_.free();
  usm_free(device_queue_, kg_memory_);
  usm_free(device_queue_, kg_memory_device_);

  for (ConstMemMap::iterator mt = const_mem_map_.begin(); mt != const_mem_map_.end(); mt++)
    delete mt->second;

  if (device_queue_)
    free_queue(device_queue_);
}

bool OneapiDevice::check_peer_access(Device * /*peer_device*/)
{
  return false;
}

BVHLayoutMask OneapiDevice::get_bvh_layout_mask() const
{
  return BVH_LAYOUT_BVH2;
}

bool OneapiDevice::load_kernels(const uint requested_features)
{
  assert(device_queue_);

  bool is_finished_ok = oneapi_run_test_kernel(device_queue_);
  if (is_finished_ok == false) {
    set_error("oneAPI test kernel execution: got a runtime exception \"" + oneapi_error_string_ +
              "\"");
    return false;
  }
  else {
    VLOG_INFO << "Test kernel has been executed successfully for \"" << info.description << "\"";
    assert(device_queue_);
  }

  is_finished_ok = oneapi_load_kernels(device_queue_, (const unsigned int)requested_features);
  if (is_finished_ok == false) {
    set_error("oneAPI kernels loading: got a runtime exception \"" + oneapi_error_string_ + "\"");
  }
  else {
    VLOG_INFO << "Kernels loading (compilation) has been done for \"" << info.description << "\"";
  }

  return is_finished_ok;
}

void OneapiDevice::load_texture_info()
{
  if (need_texture_info_) {
    need_texture_info_ = false;
    texture_info_.copy_to_device();
  }
}

void OneapiDevice::generic_alloc(device_memory &mem)
{
  size_t memory_size = mem.memory_size();

  /* TODO(@nsirgien): In future, if scene doesn't fit into device memory, then
   * we can use USM host memory.
   * Because of the expected performance impact, implementation of this has had a low priority
   * and is not implemented yet. */

  assert(device_queue_);
  /* NOTE(@nsirgien): There are three types of Unified Shared Memory (USM) in oneAPI: host, device
   * and shared. For new project it maybe more beneficial to use USM shared memory, because it
   * provides automatic migration mechanism in order to allow to use the same pointer on host and
   * on device, without need to worry about explicit memory transfer operations. But for
   * Blender/Cycles this type of memory is not very suitable in current application architecture,
   * because Cycles already uses two different pointer for host activity and device activity, and
   * also has to perform all needed memory transfer operations. So, USM device memory
   * type has been used for oneAPI device in order to better fit in Cycles architecture. */
  void *device_pointer = nullptr;
  if (mem.memory_size() + stats.mem_used < max_memory_on_device_)
    device_pointer = usm_alloc_device(device_queue_, memory_size);
  if (device_pointer == nullptr) {
    set_error("oneAPI kernel - device memory allocation error for " +
              string_human_readable_size(mem.memory_size()) +
              ", possibly caused by lack of available memory space on the device: " +
              string_human_readable_size(stats.mem_used) + " of " +
              string_human_readable_size(max_memory_on_device_) + " is already allocated");
  }

  mem.device_pointer = reinterpret_cast<ccl::device_ptr>(device_pointer);
  mem.device_size = memory_size;

  stats.mem_alloc(memory_size);
}

void OneapiDevice::generic_copy_to(device_memory &mem)
{
  if (!mem.device_pointer) {
    return;
  }
  size_t memory_size = mem.memory_size();

  /* Copy operation from host shouldn't be requested if there is no memory allocated on host. */
  assert(mem.host_pointer);
  assert(device_queue_);
  usm_memcpy(device_queue_, (void *)mem.device_pointer, (void *)mem.host_pointer, memory_size);
}

/* TODO: Make sycl::queue part of OneapiQueue and avoid using pointers to sycl::queue. */
SyclQueue *OneapiDevice::sycl_queue()
{
  return device_queue_;
}

string OneapiDevice::oneapi_error_message()
{
  return string(oneapi_error_string_);
}

void *OneapiDevice::kernel_globals_device_pointer()
{
  return kg_memory_device_;
}

void OneapiDevice::generic_free(device_memory &mem)
{
  if (!mem.device_pointer) {
    return;
  }

  stats.mem_free(mem.device_size);
  mem.device_size = 0;

  assert(device_queue_);
  usm_free(device_queue_, (void *)mem.device_pointer);
  mem.device_pointer = 0;
}

void OneapiDevice::mem_alloc(device_memory &mem)
{
  if (mem.type == MEM_TEXTURE) {
    assert(!"mem_alloc not supported for textures.");
  }
  else if (mem.type == MEM_GLOBAL) {
    assert(!"mem_alloc not supported for global memory.");
  }
  else {
    if (mem.name) {
      VLOG_DEBUG << "OneapiDevice::mem_alloc: \"" << mem.name << "\", "
                 << string_human_readable_number(mem.memory_size()) << " bytes. ("
                 << string_human_readable_size(mem.memory_size()) << ")";
    }
    generic_alloc(mem);
  }
}

void OneapiDevice::mem_copy_to(device_memory &mem)
{
  if (mem.name) {
    VLOG_DEBUG << "OneapiDevice::mem_copy_to: \"" << mem.name << "\", "
               << string_human_readable_number(mem.memory_size()) << " bytes. ("
               << string_human_readable_size(mem.memory_size()) << ")";
  }

  if (mem.type == MEM_GLOBAL) {
    global_free(mem);
    global_alloc(mem);
  }
  else if (mem.type == MEM_TEXTURE) {
    tex_free((device_texture &)mem);
    tex_alloc((device_texture &)mem);
  }
  else {
    if (!mem.device_pointer)
      mem_alloc(mem);

    generic_copy_to(mem);
  }
}

void OneapiDevice::mem_copy_from(device_memory &mem, size_t y, size_t w, size_t h, size_t elem)
{
  if (mem.type == MEM_TEXTURE || mem.type == MEM_GLOBAL) {
    assert(!"mem_copy_from not supported for textures.");
  }
  else if (mem.host_pointer) {
    const size_t size = (w > 0 || h > 0 || elem > 0) ? (elem * w * h) : mem.memory_size();
    const size_t offset = elem * y * w;

    if (mem.name) {
      VLOG_DEBUG << "OneapiDevice::mem_copy_from: \"" << mem.name << "\" object of "
                 << string_human_readable_number(mem.memory_size()) << " bytes. ("
                 << string_human_readable_size(mem.memory_size()) << ") from offset " << offset
                 << " data " << size << " bytes";
    }

    assert(device_queue_);

    assert(size != 0);
    if (mem.device_pointer) {
      char *shifted_host = reinterpret_cast<char *>(mem.host_pointer) + offset;
      char *shifted_device = reinterpret_cast<char *>(mem.device_pointer) + offset;
      bool is_finished_ok = usm_memcpy(device_queue_, shifted_host, shifted_device, size);
      if (is_finished_ok == false) {
        set_error("oneAPI memory operation error: got runtime exception \"" +
                  oneapi_error_string_ + "\"");
      }
    }
  }
}

void OneapiDevice::mem_zero(device_memory &mem)
{
  if (mem.name) {
    VLOG_DEBUG << "OneapiDevice::mem_zero: \"" << mem.name << "\", "
               << string_human_readable_number(mem.memory_size()) << " bytes. ("
               << string_human_readable_size(mem.memory_size()) << ")\n";
  }

  if (!mem.device_pointer) {
    mem_alloc(mem);
  }
  if (!mem.device_pointer) {
    return;
  }

  assert(device_queue_);
  bool is_finished_ok = usm_memset(
      device_queue_, (void *)mem.device_pointer, 0, mem.memory_size());
  if (is_finished_ok == false) {
    set_error("oneAPI memory operation error: got runtime exception \"" + oneapi_error_string_ +
              "\"");
  }
}

void OneapiDevice::mem_free(device_memory &mem)
{
  if (mem.name) {
    VLOG_DEBUG << "OneapiDevice::mem_free: \"" << mem.name << "\", "
               << string_human_readable_number(mem.device_size) << " bytes. ("
               << string_human_readable_size(mem.device_size) << ")\n";
  }

  if (mem.type == MEM_GLOBAL) {
    global_free(mem);
  }
  else if (mem.type == MEM_TEXTURE) {
    tex_free((device_texture &)mem);
  }
  else {
    generic_free(mem);
  }
}

device_ptr OneapiDevice::mem_alloc_sub_ptr(device_memory &mem, size_t offset, size_t /*size*/)
{
  return reinterpret_cast<device_ptr>(reinterpret_cast<char *>(mem.device_pointer) +
                                      mem.memory_elements_size(offset));
}

void OneapiDevice::const_copy_to(const char *name, void *host, size_t size)
{
  assert(name);

  VLOG_DEBUG << "OneapiDevice::const_copy_to \"" << name << "\" object "
             << string_human_readable_number(size) << " bytes. ("
             << string_human_readable_size(size) << ")";

  ConstMemMap::iterator i = const_mem_map_.find(name);
  device_vector<uchar> *data;

  if (i == const_mem_map_.end()) {
    data = new device_vector<uchar>(this, name, MEM_READ_ONLY);
    data->alloc(size);
    const_mem_map_.insert(ConstMemMap::value_type(name, data));
  }
  else {
    data = i->second;
  }

  assert(data->memory_size() <= size);
  memcpy(data->data(), host, size);
  data->copy_to_device();

  set_global_memory(device_queue_, kg_memory_, name, (void *)data->device_pointer);

  usm_memcpy(device_queue_, kg_memory_device_, kg_memory_, kg_memory_size_);
}

void OneapiDevice::global_alloc(device_memory &mem)
{
  assert(mem.name);

  size_t size = mem.memory_size();
  VLOG_DEBUG << "OneapiDevice::global_alloc \"" << mem.name << "\" object "
             << string_human_readable_number(size) << " bytes. ("
             << string_human_readable_size(size) << ")";

  generic_alloc(mem);
  generic_copy_to(mem);

  set_global_memory(device_queue_, kg_memory_, mem.name, (void *)mem.device_pointer);

  usm_memcpy(device_queue_, kg_memory_device_, kg_memory_, kg_memory_size_);
}

void OneapiDevice::global_free(device_memory &mem)
{
  if (mem.device_pointer) {
    generic_free(mem);
  }
}

void OneapiDevice::tex_alloc(device_texture &mem)
{
  generic_alloc(mem);
  generic_copy_to(mem);

  /* Resize if needed. Also, in case of resize - allocate in advance for future allocs. */
  const uint slot = mem.slot;
  if (slot >= texture_info_.size()) {
    texture_info_.resize(slot + 128);
  }

  texture_info_[slot] = mem.info;
  need_texture_info_ = true;

  texture_info_[slot].data = (uint64_t)mem.device_pointer;
}

void OneapiDevice::tex_free(device_texture &mem)
{
  /* There is no texture memory in SYCL. */
  if (mem.device_pointer) {
    generic_free(mem);
  }
}

unique_ptr<DeviceQueue> OneapiDevice::gpu_queue_create()
{
  return make_unique<OneapiDeviceQueue>(this);
}

bool OneapiDevice::should_use_graphics_interop()
{
  /* NOTE(@nsirgien): oneAPI doesn't yet support direct writing into graphics API objects, so
   * return false. */
  return false;
}

void *OneapiDevice::usm_aligned_alloc_host(size_t memory_size, size_t alignment)
{
  assert(device_queue_);
  return usm_aligned_alloc_host(device_queue_, memory_size, alignment);
}

void OneapiDevice::usm_free(void *usm_ptr)
{
  assert(device_queue_);
  return usm_free(device_queue_, usm_ptr);
}

void OneapiDevice::check_usm(SyclQueue *queue_, const void *usm_ptr, bool allow_host = false)
{
#  ifdef _DEBUG
  sycl::queue *queue = reinterpret_cast<sycl::queue *>(queue_);
  sycl::info::device_type device_type =
      queue->get_device().get_info<sycl::info::device::device_type>();
  sycl::usm::alloc usm_type = get_pointer_type(usm_ptr, queue->get_context());
  (void)usm_type;
#    ifndef WITH_ONEAPI_SYCL_HOST_TASK
  const sycl::usm::alloc main_memory_type = sycl::usm::alloc::device;
#    else
  const sycl::usm::alloc main_memory_type = sycl::usm::alloc::host;
#    endif
  assert(usm_type == main_memory_type ||
         (usm_type == sycl::usm::alloc::host &&
          (allow_host || device_type == sycl::info::device_type::cpu)) ||
         usm_type == sycl::usm::alloc::unknown);
#  else
  /* Silence warning about unused arguments. */
  (void)queue_;
  (void)usm_ptr;
  (void)allow_host;
#  endif
}

bool OneapiDevice::create_queue(SyclQueue *&external_queue, int device_index)
{
  bool finished_correct = true;
  try {
    std::vector<sycl::device> devices = OneapiDevice::available_devices();
    if (device_index < 0 || device_index >= devices.size()) {
      return false;
    }
    sycl::queue *created_queue = new sycl::queue(devices[device_index],
                                                 sycl::property::queue::in_order());
    external_queue = reinterpret_cast<SyclQueue *>(created_queue);
  }
  catch (sycl::exception const &e) {
    finished_correct = false;
    oneapi_error_string_ = e.what();
  }
  return finished_correct;
}

void OneapiDevice::free_queue(SyclQueue *queue_)
{
  assert(queue_);
  sycl::queue *queue = reinterpret_cast<sycl::queue *>(queue_);
  delete queue;
}

void *OneapiDevice::usm_aligned_alloc_host(SyclQueue *queue_, size_t memory_size, size_t alignment)
{
  assert(queue_);
  sycl::queue *queue = reinterpret_cast<sycl::queue *>(queue_);
  return sycl::aligned_alloc_host(alignment, memory_size, *queue);
}

void *OneapiDevice::usm_alloc_device(SyclQueue *queue_, size_t memory_size)
{
  assert(queue_);
  sycl::queue *queue = reinterpret_cast<sycl::queue *>(queue_);
#  ifndef WITH_ONEAPI_SYCL_HOST_TASK
  return sycl::malloc_device(memory_size, *queue);
#  else
  return sycl::malloc_host(memory_size, *queue);
#  endif
}

void OneapiDevice::usm_free(SyclQueue *queue_, void *usm_ptr)
{
  assert(queue_);
  sycl::queue *queue = reinterpret_cast<sycl::queue *>(queue_);
  OneapiDevice::check_usm(queue_, usm_ptr, true);
  sycl::free(usm_ptr, *queue);
}

bool OneapiDevice::usm_memcpy(SyclQueue *queue_, void *dest, void *src, size_t num_bytes)
{
  assert(queue_);
  sycl::queue *queue = reinterpret_cast<sycl::queue *>(queue_);
  OneapiDevice::check_usm(queue_, dest, true);
  OneapiDevice::check_usm(queue_, src, true);
  sycl::event mem_event = queue->memcpy(dest, src, num_bytes);
#  ifdef WITH_CYCLES_DEBUG
  try {
    /* NOTE(@nsirgien) Waiting on memory operation may give more precise error
     * messages. Due to impact on occupancy, it makes sense to enable it only during Cycles debug.
     */
    mem_event.wait_and_throw();
    return true;
  }
  catch (sycl::exception const &e) {
    oneapi_error_string_ = e.what();
    return false;
  }
#  else
  sycl::usm::alloc dest_type = get_pointer_type(dest, queue->get_context());
  sycl::usm::alloc src_type = get_pointer_type(src, queue->get_context());
  bool from_device_to_host = dest_type == sycl::usm::alloc::host &&
                             src_type == sycl::usm::alloc::device;
  bool host_or_device_memop_with_offset = dest_type == sycl::usm::alloc::unknown ||
                                          src_type == sycl::usm::alloc::unknown;
  /* NOTE(@sirgienko) Host-side blocking wait on this operation is mandatory, otherwise the host
   * may not wait until the end of the transfer before using the memory.
   */
  if (from_device_to_host || host_or_device_memop_with_offset)
    mem_event.wait();
  return true;
#  endif
}

bool OneapiDevice::usm_memset(SyclQueue *queue_,
                              void *usm_ptr,
                              unsigned char value,
                              size_t num_bytes)
{
  assert(queue_);
  sycl::queue *queue = reinterpret_cast<sycl::queue *>(queue_);
  OneapiDevice::check_usm(queue_, usm_ptr, true);
  sycl::event mem_event = queue->memset(usm_ptr, value, num_bytes);
#  ifdef WITH_CYCLES_DEBUG
  try {
    /* NOTE(@nsirgien) Waiting on memory operation may give more precise error
     * messages. Due to impact on occupancy, it makes sense to enable it only during Cycles debug.
     */
    mem_event.wait_and_throw();
    return true;
  }
  catch (sycl::exception const &e) {
    oneapi_error_string_ = e.what();
    return false;
  }
#  else
  (void)mem_event;
  return true;
#  endif
}

bool OneapiDevice::queue_synchronize(SyclQueue *queue_)
{
  assert(queue_);
  sycl::queue *queue = reinterpret_cast<sycl::queue *>(queue_);
  try {
    queue->wait_and_throw();
    return true;
  }
  catch (sycl::exception const &e) {
    oneapi_error_string_ = e.what();
    return false;
  }
}

bool OneapiDevice::kernel_globals_size(size_t &kernel_global_size)
{
  kernel_global_size = sizeof(KernelGlobalsGPU);

  return true;
}

void OneapiDevice::set_global_memory(SyclQueue *queue_,
                                     void *kernel_globals,
                                     const char *memory_name,
                                     void *memory_device_pointer)
{
  assert(queue_);
  assert(kernel_globals);
  assert(memory_name);
  assert(memory_device_pointer);
  KernelGlobalsGPU *globals = (KernelGlobalsGPU *)kernel_globals;
  OneapiDevice::check_usm(queue_, memory_device_pointer);
  OneapiDevice::check_usm(queue_, kernel_globals, true);

  std::string matched_name(memory_name);

/* This macro will change global ptr of KernelGlobals via name matching. */
#  define KERNEL_DATA_ARRAY(type, name) \
    else if (#name == matched_name) \
    { \
      globals->__##name = (type *)memory_device_pointer; \
      return; \
    }
  if (false) {
  }
  else if ("integrator_state" == matched_name) {
    globals->integrator_state = (IntegratorStateGPU *)memory_device_pointer;
    return;
  }
  KERNEL_DATA_ARRAY(KernelData, data)
#  include "kernel/data_arrays.h"
  else
  {
    std::cerr << "Can't found global/constant memory with name \"" << matched_name << "\"!"
              << std::endl;
    assert(false);
  }
#  undef KERNEL_DATA_ARRAY
}

bool OneapiDevice::enqueue_kernel(KernelContext *kernel_context,
                                  int kernel,
                                  size_t global_size,
                                  void **args)
{
  return oneapi_enqueue_kernel(kernel_context, kernel, global_size, args);
}

/* Compute-runtime (ie. NEO) version is what gets returned by sycl/L0 on Windows
 * since Windows driver 101.3268. */
/* The same min compute-runtime version is currently required across Windows and Linux.
 * For Windows driver 101.3430, compute-runtime version is 23904. */
static const int lowest_supported_driver_version_win = 1013430;
static const int lowest_supported_driver_version_neo = 23904;

int OneapiDevice::parse_driver_build_version(const sycl::device &device)
{
  const std::string &driver_version = device.get_info<sycl::info::device::driver_version>();
  int driver_build_version = 0;

  size_t second_dot_position = driver_version.find('.', driver_version.find('.') + 1);
  if (second_dot_position == std::string::npos) {
    std::cerr << "Unable to parse unknown Intel GPU driver version \"" << driver_version
              << "\" does not match xx.xx.xxxxx (Linux), x.x.xxxx (L0),"
              << " xx.xx.xxx.xxxx (Windows) for device \""
              << device.get_info<sycl::info::device::name>() << "\"." << std::endl;
  }
  else {
    try {
      size_t third_dot_position = driver_version.find('.', second_dot_position + 1);
      if (third_dot_position != std::string::npos) {
        const std::string &third_number_substr = driver_version.substr(
            second_dot_position + 1, third_dot_position - second_dot_position - 1);
        const std::string &forth_number_substr = driver_version.substr(third_dot_position + 1);
        if (third_number_substr.length() == 3 && forth_number_substr.length() == 4)
          driver_build_version = std::stoi(third_number_substr) * 10000 +
                                 std::stoi(forth_number_substr);
      }
      else {
        const std::string &third_number_substr = driver_version.substr(second_dot_position + 1);
        driver_build_version = std::stoi(third_number_substr);
      }
    }
    catch (std::invalid_argument &) {
      std::cerr << "Unable to parse unknown Intel GPU driver version \"" << driver_version
                << "\" does not match xx.xx.xxxxx (Linux), x.x.xxxx (L0),"
                << " xx.xx.xxx.xxxx (Windows) for device \""
                << device.get_info<sycl::info::device::name>() << "\"." << std::endl;
    }
  }

  return driver_build_version;
}

std::vector<sycl::device> OneapiDevice::available_devices()
{
  bool allow_all_devices = false;
  if (getenv("CYCLES_ONEAPI_ALL_DEVICES") != nullptr) {
    allow_all_devices = true;
  }

  const std::vector<sycl::platform> &oneapi_platforms = sycl::platform::get_platforms();

  std::vector<sycl::device> available_devices;
  for (const sycl::platform &platform : oneapi_platforms) {
    /* ignore OpenCL platforms to avoid using the same devices through both Level-Zero and OpenCL.
     */
    if (platform.get_backend() == sycl::backend::opencl) {
      continue;
    }

    const std::vector<sycl::device> &oneapi_devices =
        (allow_all_devices) ? platform.get_devices(sycl::info::device_type::all) :
                              platform.get_devices(sycl::info::device_type::gpu);

    for (const sycl::device &device : oneapi_devices) {
      bool filter_out = false;
      if (!allow_all_devices) {
        /* For now we support all Intel(R) Arc(TM) devices and likely any future GPU,
         * assuming they have either more than 96 Execution Units or not 7 threads per EU.
         * Official support can be broaden to older and smaller GPUs once ready. */
        if (!device.is_gpu() || platform.get_backend() != sycl::backend::ext_oneapi_level_zero) {
          filter_out = true;
        }
        else {
          /* Filtered-out defaults in-case these values aren't available. */
          int number_of_eus = 96;
          int threads_per_eu = 7;
          if (device.has(sycl::aspect::ext_intel_gpu_eu_count)) {
            number_of_eus = device.get_info<sycl::ext::intel::info::device::gpu_eu_count>();
          }
          if (device.has(sycl::aspect::ext_intel_gpu_hw_threads_per_eu)) {
            threads_per_eu =
                device.get_info<sycl::ext::intel::info::device::gpu_hw_threads_per_eu>();
          }
          /* This filters out all Level-Zero supported GPUs from older generation than Arc. */
          if (number_of_eus <= 96 && threads_per_eu == 7) {
            filter_out = true;
          }
          /* if not already filtered out, check driver version. */
          if (!filter_out) {
            int driver_build_version = parse_driver_build_version(device);
            if ((driver_build_version > 100000 &&
                 driver_build_version < lowest_supported_driver_version_win) ||
                driver_build_version < lowest_supported_driver_version_neo) {
              filter_out = true;
            }
          }
        }
      }
      if (!filter_out) {
        available_devices.push_back(device);
      }
    }
  }

  return available_devices;
}

char *OneapiDevice::device_capabilities()
{
  std::stringstream capabilities;

  const std::vector<sycl::device> &oneapi_devices = available_devices();
  for (const sycl::device &device : oneapi_devices) {
#  ifndef WITH_ONEAPI_SYCL_HOST_TASK
    const std::string &name = device.get_info<sycl::info::device::name>();
#  else
    const std::string &name = "SYCL Host Task (Debug)";
#  endif

    capabilities << std::string("\t") << name << "\n";
#  define WRITE_ATTR(attribute_name, attribute_variable) \
    capabilities << "\t\tsycl::info::device::" #attribute_name "\t\t\t" << attribute_variable \
                 << "\n";
#  define GET_NUM_ATTR(attribute) \
    { \
      size_t attribute = (size_t)device.get_info<sycl::info::device ::attribute>(); \
      capabilities << "\t\tsycl::info::device::" #attribute "\t\t\t" << attribute << "\n"; \
    }

    GET_NUM_ATTR(vendor_id)
    GET_NUM_ATTR(max_compute_units)
    GET_NUM_ATTR(max_work_item_dimensions)

    sycl::id<3> max_work_item_sizes =
        device.get_info<sycl::info::device::max_work_item_sizes<3>>();
    WRITE_ATTR("max_work_item_sizes_dim0", ((size_t)max_work_item_sizes.get(0)))
    WRITE_ATTR("max_work_item_sizes_dim1", ((size_t)max_work_item_sizes.get(1)))
    WRITE_ATTR("max_work_item_sizes_dim2", ((size_t)max_work_item_sizes.get(2)))

    GET_NUM_ATTR(max_work_group_size)
    GET_NUM_ATTR(max_num_sub_groups)
    GET_NUM_ATTR(sub_group_independent_forward_progress)

    GET_NUM_ATTR(preferred_vector_width_char)
    GET_NUM_ATTR(preferred_vector_width_short)
    GET_NUM_ATTR(preferred_vector_width_int)
    GET_NUM_ATTR(preferred_vector_width_long)
    GET_NUM_ATTR(preferred_vector_width_float)
    GET_NUM_ATTR(preferred_vector_width_double)
    GET_NUM_ATTR(preferred_vector_width_half)

    GET_NUM_ATTR(native_vector_width_char)
    GET_NUM_ATTR(native_vector_width_short)
    GET_NUM_ATTR(native_vector_width_int)
    GET_NUM_ATTR(native_vector_width_long)
    GET_NUM_ATTR(native_vector_width_float)
    GET_NUM_ATTR(native_vector_width_double)
    GET_NUM_ATTR(native_vector_width_half)

    size_t max_clock_frequency = device.get_info<sycl::info::device::max_clock_frequency>();
    WRITE_ATTR("max_clock_frequency", max_clock_frequency)

    GET_NUM_ATTR(address_bits)
    GET_NUM_ATTR(max_mem_alloc_size)

    /* NOTE(@nsirgien): Implementation doesn't use image support as bindless images aren't
     * supported so we always return false, even if device supports HW texture usage acceleration.
     */
    bool image_support = false;
    WRITE_ATTR("image_support", (size_t)image_support)

    GET_NUM_ATTR(max_parameter_size)
    GET_NUM_ATTR(mem_base_addr_align)
    GET_NUM_ATTR(global_mem_size)
    GET_NUM_ATTR(local_mem_size)
    GET_NUM_ATTR(error_correction_support)
    GET_NUM_ATTR(profiling_timer_resolution)
    GET_NUM_ATTR(is_available)

#  undef GET_NUM_ATTR
#  undef WRITE_ATTR
    capabilities << "\n";
  }

  return ::strdup(capabilities.str().c_str());
}

void OneapiDevice::iterate_devices(OneAPIDeviceIteratorCallback cb, void *user_ptr)
{
  int num = 0;
  std::vector<sycl::device> devices = OneapiDevice::available_devices();
  for (sycl::device &device : devices) {
    const std::string &platform_name =
        device.get_platform().get_info<sycl::info::platform::name>();
#  ifndef WITH_ONEAPI_SYCL_HOST_TASK
    std::string name = device.get_info<sycl::info::device::name>();
#  else
    std::string name = "SYCL Host Task (Debug)";
#  endif
    std::string id = "ONEAPI_" + platform_name + "_" + name;
    if (device.has(sycl::aspect::ext_intel_pci_address)) {
      id.append("_" + device.get_info<sycl::ext::intel::info::device::pci_address>());
    }
    (cb)(id.c_str(), name.c_str(), num, user_ptr);
    num++;
  }
}

size_t OneapiDevice::get_memcapacity()
{
  return reinterpret_cast<sycl::queue *>(device_queue_)
      ->get_device()
      .get_info<sycl::info::device::global_mem_size>();
}

int OneapiDevice::get_num_multiprocessors()
{
  const sycl::device &device = reinterpret_cast<sycl::queue *>(device_queue_)->get_device();
  if (device.has(sycl::aspect::ext_intel_gpu_eu_count)) {
    return device.get_info<sycl::ext::intel::info::device::gpu_eu_count>();
  }
  else
    return 0;
}

int OneapiDevice::get_max_num_threads_per_multiprocessor()
{
  const sycl::device &device = reinterpret_cast<sycl::queue *>(device_queue_)->get_device();
  if (device.has(sycl::aspect::ext_intel_gpu_eu_simd_width) &&
      device.has(sycl::aspect::ext_intel_gpu_hw_threads_per_eu)) {
    return device.get_info<sycl::ext::intel::info::device::gpu_eu_simd_width>() *
           device.get_info<sycl::ext::intel::info::device::gpu_hw_threads_per_eu>();
  }
  else
    return 0;
}

CCL_NAMESPACE_END

#endif
