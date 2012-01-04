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

#include <stdio.h>

#include "device.h"

#include "util_args.h"
#include "util_foreach.h"
#include "util_path.h"
#include "util_string.h"

using namespace ccl;

int main(int argc, const char **argv)
{
	path_init();

	/* device types */
	string devices = "";
	string devicename = "cpu";
	bool list = false;

	vector<DeviceType>& types = Device::available_types();

	foreach(DeviceType type, types) {
		if(devices != "")
			devices += ", ";

		devices += Device::string_from_type(type);
	}

	/* parse options */
	ArgParse ap;

	ap.options ("Usage: cycles_server [options]",
		"--device %s", &devicename, ("Devices to use: " + devices).c_str(),
		"--list-devices", &list, "List information about all available devices",
		NULL);

	if(ap.parse(argc, argv) < 0) {
		fprintf(stderr, "%s\n", ap.error_message().c_str());
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

	while(1) {
		Device *device = Device::create(device_info);
		printf("Cycles Server with device: %s\n", device->description().c_str());
		device->server_run();
		delete device;
	}

	return 0;
}

