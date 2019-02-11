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

#include "blender/blender_device.h"
#include "blender/blender_util.h"

CCL_NAMESPACE_BEGIN

int blender_device_threads(BL::Scene& b_scene)
{
	BL::RenderSettings b_r = b_scene.render();

	if(b_r.threads_mode() == BL::RenderSettings::threads_mode_FIXED)
		return b_r.threads();
	else
		return 0;
}

DeviceInfo blender_device_info(BL::Preferences& b_preferences, BL::Scene& b_scene, bool background)
{
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

	/* Default to CPU device. */
	DeviceInfo device = Device::available_devices(DEVICE_MASK_CPU).front();

	if(get_enum(cscene, "device") == 2) {
		/* Find network device. */
		vector<DeviceInfo> devices = Device::available_devices(DEVICE_MASK_NETWORK);
		if(!devices.empty()) {
			device = devices.front();
		}
	}
	else if(get_enum(cscene, "device") == 1) {
		/* Find cycles preferences. */
		PointerRNA cpreferences;

		BL::Preferences::addons_iterator b_addon_iter;
		for(b_preferences.addons.begin(b_addon_iter); b_addon_iter != b_preferences.addons.end(); ++b_addon_iter) {
			if(b_addon_iter->module() == "cycles") {
				cpreferences = b_addon_iter->preferences().ptr;
				break;
			}
		}

		/* Test if we are using GPU devices. */
		enum ComputeDevice {
			COMPUTE_DEVICE_CPU = 0,
			COMPUTE_DEVICE_CUDA = 1,
			COMPUTE_DEVICE_OPENCL = 2,
			COMPUTE_DEVICE_NUM = 3,
		};

		ComputeDevice compute_device = (ComputeDevice)get_enum(cpreferences,
		                                                       "compute_device_type",
		                                                       COMPUTE_DEVICE_NUM,
		                                                       COMPUTE_DEVICE_CPU);

		if(compute_device != COMPUTE_DEVICE_CPU) {
			/* Query GPU devices with matching types. */
			uint mask = DEVICE_MASK_CPU;
			if(compute_device == COMPUTE_DEVICE_CUDA) {
				mask |= DEVICE_MASK_CUDA;
			}
			else if(compute_device == COMPUTE_DEVICE_OPENCL) {
				mask |= DEVICE_MASK_OPENCL;
			}
			vector<DeviceInfo> devices = Device::available_devices(mask);

			/* Match device preferences and available devices. */
			vector<DeviceInfo> used_devices;
			RNA_BEGIN(&cpreferences, device, "devices") {
				if(get_boolean(device, "use")) {
					string id = get_string(device, "id");
					foreach(DeviceInfo& info, devices) {
						if(info.id == id) {
							used_devices.push_back(info);
							break;
						}
					}
				}
			} RNA_END;

			if(!used_devices.empty()) {
				int threads = blender_device_threads(b_scene);
				device = Device::get_multi_device(used_devices,
				                                  threads,
				                                  background);
			}
			/* Else keep using the CPU device that was set before. */
		}
	}

	return device;
}

CCL_NAMESPACE_END
