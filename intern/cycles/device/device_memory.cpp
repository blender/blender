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

#include "device/device.h"
#include "device/device_memory.h"

CCL_NAMESPACE_BEGIN

/* Device Memory */

device_memory::device_memory(Device *device, const char *name, MemoryType type)
: data_type(device_type_traits<uchar>::data_type),
  data_elements(device_type_traits<uchar>::num_elements),
  data_pointer(0),
  data_size(0),
  device_size(0),
  data_width(0),
  data_height(0),
  data_depth(0),
  type(type),
  name(name),
  interpolation(INTERPOLATION_NONE),
  extension(EXTENSION_REPEAT),
  device(device),
  device_pointer(0)
{
}

device_memory::~device_memory()
{
}

/* Device Sub Ptr */

device_sub_ptr::device_sub_ptr(device_memory& mem, int offset, int size)
: device(mem.device)
{
	ptr = device->mem_alloc_sub_ptr(mem, offset, size);
}

device_sub_ptr::~device_sub_ptr()
{
	device->mem_free_sub_ptr(ptr);
}

CCL_NAMESPACE_END

