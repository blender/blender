# SPDX-FileCopyrightText: 2021-2023 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

import platform
import subprocess
from typing import List


def get_cpu_name() -> str:
    # Get full CPU name.
    if platform.system() == "Windows":
        return platform.processor()
    elif platform.system() == "Darwin":
        cmd = ['/usr/sbin/sysctl', "-n", "machdep.cpu.brand_string"]
        return subprocess.check_output(cmd).strip().decode('utf-8', 'ignore')
    else:
        with open('/proc/cpuinfo') as f:
            for line in f:
                if line.startswith('model name'):
                    return line.split(':')[1].strip()

    return "Unknown CPU"


def get_gpu_device(args: None) -> List:
    # Get the list of available Cycles GPU devices.
    import bpy

    prefs = bpy.context.preferences
    cprefs = prefs.addons['cycles'].preferences

    result = []

    for device_type, _, _, _ in cprefs.get_device_types(bpy.context):
        cprefs.compute_device_type = device_type
        devices = cprefs.get_devices_for_type(device_type)
        index = 0
        for device in devices:
            if device.type == device_type:
                result.append({'type': device.type, 'name': device.name, 'index': index})
                index += 1

    return result


class TestDevice:
    def __init__(self, device_type: str, device_id: str, name: str, operating_system: str):
        self.type = device_type
        self.id = device_id
        self.name = name
        self.operating_system = operating_system


class TestMachine:
    def __init__(self, env, need_gpus: bool):
        operating_system = platform.system()

        self.devices = [TestDevice('CPU', 'CPU', get_cpu_name(), operating_system)]
        self.has_gpus = need_gpus

        if need_gpus and env.blender_executable:
            gpu_devices, _ = env.run_in_blender(get_gpu_device, {})
            for gpu_device in gpu_devices:
                device_type = gpu_device['type']
                device_name = gpu_device['name']
                device_id = gpu_device['type'] + "_" + str(gpu_device['index'])
                self.devices.append(TestDevice(device_type, device_id, device_name, operating_system))

    def cpu_device(self) -> TestDevice:
        return self.devices[0]
