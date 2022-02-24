/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include <stdio.h>

#include "device/device.h"

#include "util/args.h"
#include "util/foreach.h"
#include "util/log.h"
#include "util/path.h"
#include "util/stats.h"
#include "util/string.h"
#include "util/task.h"

using namespace ccl;

int main(int argc, const char **argv)
{
  util_logging_init(argv[0]);
  path_init();

  /* device types */
  string devicelist = "";
  string devicename = "cpu";
  bool list = false, debug = false;
  int threads = 0, verbosity = 1;

  vector<DeviceType> &types = Device::available_types();

  foreach (DeviceType type, types) {
    if (devicelist != "")
      devicelist += ", ";

    devicelist += Device::string_from_type(type);
  }

  /* parse options */
  ArgParse ap;

  ap.options("Usage: cycles_server [options]",
             "--device %s",
             &devicename,
             ("Devices to use: " + devicelist).c_str(),
             "--list-devices",
             &list,
             "List information about all available devices",
             "--threads %d",
             &threads,
             "Number of threads to use for CPU device",
#ifdef WITH_CYCLES_LOGGING
             "--debug",
             &debug,
             "Enable debug logging",
             "--verbose %d",
             &verbosity,
             "Set verbosity of the logger",
#endif
             NULL);

  if (ap.parse(argc, argv) < 0) {
    fprintf(stderr, "%s\n", ap.geterror().c_str());
    ap.usage();
    exit(EXIT_FAILURE);
  }

  if (debug) {
    util_logging_start();
    util_logging_verbosity_set(verbosity);
  }

  if (list) {
    vector<DeviceInfo> &devices = Device::available_devices();

    printf("Devices:\n");

    foreach (DeviceInfo &info, devices) {
      printf("    %s%s\n", info.description.c_str(), (info.display_device) ? " (display)" : "");
    }

    exit(EXIT_SUCCESS);
  }

  /* find matching device */
  DeviceType device_type = Device::type_from_string(devicename.c_str());
  vector<DeviceInfo> &devices = Device::available_devices();
  DeviceInfo device_info;

  foreach (DeviceInfo &device, devices) {
    if (device_type == device.type) {
      device_info = device;
      break;
    }
  }

  TaskScheduler::init(threads);

  while (1) {
    Stats stats;
    Device *device = Device::create(device_info, stats, true);
    printf("Cycles Server with device: %s\n", device->info.description.c_str());
    device->server_run();
    delete device;
  }

  TaskScheduler::exit();

  return 0;
}
