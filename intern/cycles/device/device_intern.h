/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __DEVICE_INTERN_H__
#define __DEVICE_INTERN_H__

CCL_NAMESPACE_BEGIN

class Device;

Device *device_cpu_create(DeviceInfo& info, int threads);
Device *device_opencl_create(DeviceInfo& info, bool background);
Device *device_cuda_create(DeviceInfo& info, bool background);
Device *device_network_create(DeviceInfo& info, const char *address);
Device *device_multi_create(DeviceInfo& info, bool background);

void device_cpu_info(vector<DeviceInfo>& devices);
void device_opencl_info(vector<DeviceInfo>& devices);
void device_cuda_info(vector<DeviceInfo>& devices);
void device_network_info(vector<DeviceInfo>& devices);
void device_multi_info(vector<DeviceInfo>& devices);

CCL_NAMESPACE_END

#endif /* __DEVICE_INTERN_H__ */

