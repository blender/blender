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

#include <stdio.h>

#include "device.h"

#include "util_args.h"
#include "util_foreach.h"
#include "util_path.h"
#include "util_stats.h"
#include "util_string.h"
#include "util_task.h"

using namespace ccl;

int main(int argc, const char **argv)
{
	path_init();

	/* device types */
	string devicelist = "";
	string devicename = "cpu";
	bool list = false;
	int threads = 0;

	vector<DeviceType>& types = Device::available_types();

	foreach(DeviceType type, types) {
		if(devicelist != "")
			devicelist += ", ";

		devicelist += Device::string_from_type(type);
	}

	/* parse options */
	ArgParse ap;

	ap.options ("Usage: cycles_server [options]",
		"--device %s", &devicename, ("Devices to use: " + devicelist).c_str(),
		"--list-devices", &list, "List information about all available devices",
		"--threads %d", &threads, "Number of threads to use for CPU device",
		NULL);

	if(ap.parse(argc, argv) < 0) {
		fprintf(stderr, "%s\n", ap.geterror().c_str());
		ap.usage();
		exit(EXIT_FAILURE);
	}
	else if(list) {
		vector<DeviceInfo>& devices = Device::available_devices();

		printf("Devices:\n");

		foreach(DeviceInfo& info, devices) {
			printf("    %s%s\n",
				info.description.c_str(),
				(info.display_device)? " (display)": "");
		}

		exit(EXIT_SUCCESS);
	}

	/* find matching device */
	DeviceType device_type = Device::type_from_string(devicename.c_str());
	vector<DeviceInfo>& devices = Device::available_devices();
	DeviceInfo device_info;

	foreach(DeviceInfo& device, devices) {
		if(device_type == device.type) {
			device_info = device;
			break;
		}
	}

	TaskScheduler::init(threads);

	while(1) {
		Stats stats;
		Device *device = Device::create(device_info, stats, true);
		printf("Cycles Server with device: %s\n", device->info.description.c_str());
		device->server_run();
		delete device;
	}

	TaskScheduler::exit();

	return 0;
}

