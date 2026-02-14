/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/memory.h"
#include "device/device.h"

CCL_NAMESPACE_BEGIN

static const char *name_from_type(ImageDataType type)
{
  switch (type) {
    case IMAGE_DATA_TYPE_FLOAT4:
      return "float4";
    case IMAGE_DATA_TYPE_BYTE4:
      return "byte4";
    case IMAGE_DATA_TYPE_HALF4:
      return "half4";
    case IMAGE_DATA_TYPE_FLOAT:
      return "float";
    case IMAGE_DATA_TYPE_BYTE:
      return "byte";
    case IMAGE_DATA_TYPE_HALF:
      return "half";
    case IMAGE_DATA_TYPE_USHORT4:
      return "ushort4";
    case IMAGE_DATA_TYPE_USHORT:
      return "ushort";
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT:
      return "nanovdb_float";
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT3:
      return "nanovdb_float3";
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT4:
      return "nanovdb_float4";
    case IMAGE_DATA_TYPE_NANOVDB_FPN:
      return "nanovdb_fpn";
    case IMAGE_DATA_TYPE_NANOVDB_FP16:
      return "nanovdb_fp16";
    case IMAGE_DATA_TYPE_NANOVDB_EMPTY:
      return "nanovdb_empty";
    case IMAGE_DATA_NUM_TYPES:
      assert(!"System enumerator type, should never be used");
      return "";
  }
  assert(!"Unhandled image data type");
  return "";
}

/* Device Memory */

device_memory::device_memory(Device *device, const char *name, MemoryType type)
    : data_type(device_type_traits<uchar>::data_type),
      data_elements(device_type_traits<uchar>::num_elements),
      data_size(0),
      device_size(0),
      data_width(0),
      data_height(0),
      type(type),
      device(device),
      device_pointer(0),
      host_pointer(nullptr),
      shared_pointer(nullptr),
      shared_counter(0),
      name_(name),
      original_device_ptr(0),
      original_device_size(0),
      original_device(nullptr),
      need_realloc_(false),
      modified(false)
{
}

device_memory::~device_memory()
{
  assert(shared_pointer == nullptr);
  assert(shared_counter == 0);
}

const char *device_memory::global_name() const
{
  return name_;
}

string device_memory::log_name() const
{
  return (name_) ? name_ : "unknown";
}

void *device_memory::host_alloc(const size_t size)
{
  if (!size) {
    return nullptr;
  }

  void *ptr = device->host_alloc(type, size);

  if (ptr == nullptr) {
    throw std::bad_alloc();
  }

  return ptr;
}

void device_memory::host_and_device_free()
{
  if (host_pointer) {
    if (host_pointer != shared_pointer) {
      device->host_free(type, host_pointer, memory_size());
    }
    host_pointer = nullptr;
  }

  if (device_pointer) {
    device->mem_free(*this);
  }

  data_size = 0;
  data_width = 0;
  data_height = 0;
}

void device_memory::device_alloc()
{
  assert(!device_pointer && type != MEM_IMAGE_TEXTURE && type != MEM_GLOBAL);
  device->mem_alloc(*this);
}

void device_memory::device_copy_to()
{
  if (host_pointer) {
    device->mem_copy_to(*this);
  }
}

void device_memory::device_move_to_host()
{
  if (host_pointer) {
    device->mem_move_to_host(*this);
  }
}

void device_memory::device_copy_from(const size_t y, const size_t w, size_t h, const size_t elem)
{
  assert(type != MEM_IMAGE_TEXTURE && type != MEM_READ_ONLY);
  device->mem_copy_from(*this, y, w, h, elem);
}

void device_memory::device_zero()
{
  if (data_size) {
    device->mem_zero(*this);
  }
}

bool device_memory::device_is_cpu()
{
  return (device->info.type == DEVICE_CPU);
}

void device_memory::swap_device(Device *new_device,
                                const size_t new_device_size,
                                device_ptr new_device_ptr)
{
  original_device = device;
  original_device_size = device_size;
  original_device_ptr = device_pointer;

  device = new_device;
  device_size = new_device_size;
  device_pointer = new_device_ptr;
}

void device_memory::restore_device()
{
  device = original_device;
  device_size = original_device_size;
  device_pointer = original_device_ptr;
}

bool device_memory::is_resident(Device *sub_device) const
{
  return device->is_resident(device_pointer, sub_device);
}

bool device_memory::is_shared(Device *sub_device) const
{
  return device->is_shared(shared_pointer, device_pointer, sub_device);
}

/* Device Sub `ptr`. */

device_sub_ptr::device_sub_ptr(device_memory &mem, const size_t offset, const size_t size)
    : device(mem.device)
{
  ptr = device->mem_alloc_sub_ptr(mem, offset, size);
}

device_sub_ptr::~device_sub_ptr()
{
  device->mem_free_sub_ptr(ptr);
}

/* Device Texture */

device_image::device_image(Device *device,
                           const char *name,
                           const uint slot,
                           ImageDataType image_data_type,
                           InterpolationType interpolation,
                           ExtensionType extension)
    : device_memory(device, name, MEM_IMAGE_TEXTURE), slot(slot)
{
  switch (image_data_type) {
    case IMAGE_DATA_TYPE_FLOAT4:
      data_type = TYPE_FLOAT;
      data_elements = 4;
      break;
    case IMAGE_DATA_TYPE_FLOAT:
      data_type = TYPE_FLOAT;
      data_elements = 1;
      break;
    case IMAGE_DATA_TYPE_BYTE4:
      data_type = TYPE_UCHAR;
      data_elements = 4;
      break;
    case IMAGE_DATA_TYPE_BYTE:
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT:
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT3:
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT4:
    case IMAGE_DATA_TYPE_NANOVDB_FPN:
    case IMAGE_DATA_TYPE_NANOVDB_FP16:
    case IMAGE_DATA_TYPE_NANOVDB_EMPTY:
      data_type = TYPE_UCHAR;
      data_elements = 1;
      break;
    case IMAGE_DATA_TYPE_HALF4:
      data_type = TYPE_HALF;
      data_elements = 4;
      break;
    case IMAGE_DATA_TYPE_HALF:
      data_type = TYPE_HALF;
      data_elements = 1;
      break;
    case IMAGE_DATA_TYPE_USHORT4:
      data_type = TYPE_UINT16;
      data_elements = 4;
      break;
    case IMAGE_DATA_TYPE_USHORT:
      data_type = TYPE_UINT16;
      data_elements = 1;
      break;
    case IMAGE_DATA_NUM_TYPES:
      assert(0);
      return;
  }

  info.data_type = image_data_type;
  info.interpolation = interpolation;
  info.extension = extension;
}

device_image::~device_image()
{
  host_and_device_free();
}

string device_image::log_name() const
{
  const char *name = (name_) ? name_ : "unknown";
  if (type == MEM_IMAGE_TEXTURE) {
    return string_printf("%s_%s_%03u", name, name_from_type(ImageDataType(info.data_type)), slot);
  }
  return name;
}

/* Host memory allocation. */
void *device_image::alloc(const size_t width, const size_t height)
{
  const size_t new_size = size(width, height);

  if (new_size != data_size) {
    host_and_device_free();
    host_pointer = host_alloc(data_elements * datatype_size(data_type) * new_size);
    assert(device_pointer == 0);
  }

  data_size = new_size;
  data_width = width;
  data_height = height;

  info.width = width;
  info.height = height;

  return host_pointer;
}

void device_image::copy_to_device()
{
  device_copy_to();
}

CCL_NAMESPACE_END
