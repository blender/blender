/*
 * Copyright 2011-2017 Blender Foundation
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

#include "device/device_memory.h"
#include "device/device.h"

CCL_NAMESPACE_BEGIN

/* Device Memory */

device_memory::device_memory(Device *device, const char *name, MemoryType type)
    : data_type(device_type_traits<uchar>::data_type),
      data_elements(device_type_traits<uchar>::num_elements),
      data_size(0),
      device_size(0),
      data_width(0),
      data_height(0),
      data_depth(0),
      type(type),
      name(name),
      device(device),
      device_pointer(0),
      host_pointer(0),
      shared_pointer(0),
      shared_counter(0)
{
}

device_memory::~device_memory()
{
  assert(shared_pointer == 0);
  assert(shared_counter == 0);
}

void *device_memory::host_alloc(size_t size)
{
  if (!size) {
    return 0;
  }

  void *ptr = util_aligned_malloc(size, MIN_ALIGNMENT_CPU_DATA_TYPES);

  if (ptr) {
    util_guarded_mem_alloc(size);
  }
  else {
    throw std::bad_alloc();
  }

  return ptr;
}

void device_memory::host_free()
{
  if (host_pointer) {
    util_guarded_mem_free(memory_size());
    util_aligned_free((void *)host_pointer);
    host_pointer = 0;
  }
}

void device_memory::device_alloc()
{
  assert(!device_pointer && type != MEM_TEXTURE && type != MEM_GLOBAL);
  device->mem_alloc(*this);
}

void device_memory::device_free()
{
  if (device_pointer) {
    device->mem_free(*this);
  }
}

void device_memory::device_copy_to()
{
  if (host_pointer) {
    device->mem_copy_to(*this);
  }
}

void device_memory::device_copy_from(int y, int w, int h, int elem)
{
  assert(type != MEM_TEXTURE && type != MEM_READ_ONLY && type != MEM_GLOBAL);
  device->mem_copy_from(*this, y, w, h, elem);
}

void device_memory::device_zero()
{
  if (data_size) {
    device->mem_zero(*this);
  }
}

void device_memory::swap_device(Device *new_device,
                                size_t new_device_size,
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

/* Device Sub Ptr */

device_sub_ptr::device_sub_ptr(device_memory &mem, int offset, int size) : device(mem.device)
{
  ptr = device->mem_alloc_sub_ptr(mem, offset, size);
}

device_sub_ptr::~device_sub_ptr()
{
  device->mem_free_sub_ptr(ptr);
}

/* Device Texture */

device_texture::device_texture(Device *device,
                               const char *name,
                               const uint slot,
                               ImageDataType image_data_type,
                               InterpolationType interpolation,
                               ExtensionType extension)
    : device_memory(device, name, MEM_TEXTURE), slot(slot)
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

  memset(&info, 0, sizeof(info));
  info.data_type = image_data_type;
  info.interpolation = interpolation;
  info.extension = extension;
}

device_texture::~device_texture()
{
  device_free();
  host_free();
}

/* Host memory allocation. */
void *device_texture::alloc(const size_t width, const size_t height, const size_t depth)
{
  const size_t new_size = size(width, height, depth);

  if (new_size != data_size) {
    device_free();
    host_free();
    host_pointer = host_alloc(data_elements * datatype_size(data_type) * new_size);
    assert(device_pointer == 0);
  }

  data_size = new_size;
  data_width = width;
  data_height = height;
  data_depth = depth;

  info.width = width;
  info.height = height;
  info.depth = depth;

  return host_pointer;
}

void device_texture::copy_to_device()
{
  device_copy_to();
}

CCL_NAMESPACE_END
