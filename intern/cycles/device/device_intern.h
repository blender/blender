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
 * limitations under the License
 */

#ifndef __DEVICE_INTERN_H__
#define __DEVICE_INTERN_H__

CCL_NAMESPACE_BEGIN

class Device;

Device *device_cpu_create(DeviceInfo& info, Stats &stats, bool background);
bool device_opencl_init(void);
Device *device_opencl_create(DeviceInfo& info, Stats &stats, bool background);
bool device_cuda_init(void);
Device *device_cuda_create(DeviceInfo& info, Stats &stats, bool background);
Device *device_network_create(DeviceInfo& info, Stats &stats, const char *address);
Device *device_multi_create(DeviceInfo& info, Stats &stats, bool background);

void device_cpu_info(vector<DeviceInfo>& devices);
void device_opencl_info(vector<DeviceInfo>& devices);
void device_cuda_info(vector<DeviceInfo>& devices);
void device_network_info(vector<DeviceInfo>& devices);
void device_multi_info(vector<DeviceInfo>& devices);

CCL_NAMESPACE_END

#endif /* __DEVICE_INTERN_H__ */

