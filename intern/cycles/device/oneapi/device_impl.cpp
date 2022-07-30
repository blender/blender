/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Intel Corporation */

#ifdef WITH_ONEAPI

#  include "device/oneapi/device_impl.h"

#  include "util/debug.h"
#  include "util/log.h"

#  include "kernel/device/oneapi/kernel.h"

CCL_NAMESPACE_BEGIN

static void queue_error_cb(const char *message, void *user_ptr)
{
  if (user_ptr) {
    *reinterpret_cast<std::string *>(user_ptr) = message;
  }
}

OneapiDevice::OneapiDevice(const DeviceInfo &info,
                           OneAPIDLLInterface &oneapi_dll_object,
                           Stats &stats,
                           Profiler &profiler)
    : Device(info, stats, profiler),
      device_queue_(nullptr),
      texture_info_(this, "texture_info", MEM_GLOBAL),
      kg_memory_(nullptr),
      kg_memory_device_(nullptr),
      kg_memory_size_(0),
      oneapi_dll_(oneapi_dll_object)
{
  need_texture_info_ = false;

  oneapi_dll_.oneapi_set_error_cb(queue_error_cb, &oneapi_error_string_);

  /* OneAPI calls should be initialized on this moment. */
  assert(oneapi_dll_.oneapi_create_queue != nullptr);

  bool is_finished_ok = oneapi_dll_.oneapi_create_queue(device_queue_, info.num);
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
  is_finished_ok = oneapi_dll_.oneapi_kernel_globals_size(device_queue_, globals_segment_size);
  if (is_finished_ok == false) {
    set_error("oneAPI constant memory initialization got runtime exception \"" +
              oneapi_error_string_ + "\"");
  }
  else {
    VLOG_DEBUG << "Successfully created global/constant memory segment (kernel globals object)";
  }

  kg_memory_ = oneapi_dll_.oneapi_usm_aligned_alloc_host(device_queue_, globals_segment_size, 16);
  oneapi_dll_.oneapi_usm_memset(device_queue_, kg_memory_, 0, globals_segment_size);

  kg_memory_device_ = oneapi_dll_.oneapi_usm_alloc_device(device_queue_, globals_segment_size);

  kg_memory_size_ = globals_segment_size;
}

OneapiDevice::~OneapiDevice()
{
  texture_info_.free();
  oneapi_dll_.oneapi_usm_free(device_queue_, kg_memory_);
  oneapi_dll_.oneapi_usm_free(device_queue_, kg_memory_device_);

  for (ConstMemMap::iterator mt = const_mem_map_.begin(); mt != const_mem_map_.end(); mt++)
    delete mt->second;

  if (device_queue_)
    oneapi_dll_.oneapi_free_queue(device_queue_);
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
  /* NOTE(@nsirgien): oneAPI can support compilation of kernel code with certain feature set
   * with specialization constants, but it hasn't been implemented yet. */
  (void)requested_features;

  bool is_finished_ok = oneapi_dll_.oneapi_run_test_kernel(device_queue_);
  if (is_finished_ok == false) {
    set_error("oneAPI kernel load: got runtime exception \"" + oneapi_error_string_ + "\"");
  }
  else {
    VLOG_INFO << "Runtime compilation done for \"" << info.description << "\"";
    assert(device_queue_);
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
  void *device_pointer = oneapi_dll_.oneapi_usm_alloc_device(device_queue_, memory_size);
  if (device_pointer == nullptr) {
    size_t max_memory_on_device = oneapi_dll_.oneapi_get_memcapacity(device_queue_);
    set_error("oneAPI kernel - device memory allocation error for " +
              string_human_readable_size(mem.memory_size()) +
              ", possibly caused by lack of available memory space on the device: " +
              string_human_readable_size(stats.mem_used) + " of " +
              string_human_readable_size(max_memory_on_device) + " is already allocated");
    return;
  }
  assert(device_pointer);

  mem.device_pointer = reinterpret_cast<ccl::device_ptr>(device_pointer);
  mem.device_size = memory_size;

  stats.mem_alloc(memory_size);
}

void OneapiDevice::generic_copy_to(device_memory &mem)
{
  size_t memory_size = mem.memory_size();

  /* Copy operation from host shouldn't be requested if there is no memory allocated on host. */
  assert(mem.host_pointer);
  assert(device_queue_);
  oneapi_dll_.oneapi_usm_memcpy(
      device_queue_, (void *)mem.device_pointer, (void *)mem.host_pointer, memory_size);
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

OneAPIDLLInterface OneapiDevice::oneapi_dll_object()
{
  return oneapi_dll_;
}

void *OneapiDevice::kernel_globals_device_pointer()
{
  return kg_memory_device_;
}

void OneapiDevice::generic_free(device_memory &mem)
{
  assert(mem.device_pointer);
  stats.mem_free(mem.device_size);
  mem.device_size = 0;

  assert(device_queue_);
  oneapi_dll_.oneapi_usm_free(device_queue_, (void *)mem.device_pointer);
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
    assert(mem.device_pointer);
    char *shifted_host = reinterpret_cast<char *>(mem.host_pointer) + offset;
    char *shifted_device = reinterpret_cast<char *>(mem.device_pointer) + offset;
    bool is_finished_ok = oneapi_dll_.oneapi_usm_memcpy(
        device_queue_, shifted_host, shifted_device, size);
    if (is_finished_ok == false) {
      set_error("oneAPI memory operation error: got runtime exception \"" + oneapi_error_string_ +
                "\"");
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
  bool is_finished_ok = oneapi_dll_.oneapi_usm_memset(
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

  oneapi_dll_.oneapi_set_global_memory(
      device_queue_, kg_memory_, name, (void *)data->device_pointer);

  oneapi_dll_.oneapi_usm_memcpy(device_queue_, kg_memory_device_, kg_memory_, kg_memory_size_);
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

  oneapi_dll_.oneapi_set_global_memory(
      device_queue_, kg_memory_, mem.name, (void *)mem.device_pointer);

  oneapi_dll_.oneapi_usm_memcpy(device_queue_, kg_memory_device_, kg_memory_, kg_memory_size_);
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

int OneapiDevice::get_num_multiprocessors()
{
  assert(device_queue_);
  return oneapi_dll_.oneapi_get_num_multiprocessors(device_queue_);
}

int OneapiDevice::get_max_num_threads_per_multiprocessor()
{
  assert(device_queue_);
  return oneapi_dll_.oneapi_get_max_num_threads_per_multiprocessor(device_queue_);
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
  return oneapi_dll_.oneapi_usm_aligned_alloc_host(device_queue_, memory_size, alignment);
}

void OneapiDevice::usm_free(void *usm_ptr)
{
  assert(device_queue_);
  return oneapi_dll_.oneapi_usm_free(device_queue_, usm_ptr);
}

CCL_NAMESPACE_END

#endif
