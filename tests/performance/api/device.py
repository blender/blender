# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import platform
import subprocess


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


def get_gpu_device(args: None) -> list:
    # Get the list of available Cycles GPU devices.
    import bpy

    prefs = bpy.context.preferences
    if 'cycles' not in prefs.addons.keys():
        return []
    cprefs = prefs.addons['cycles'].preferences

    result = []

    for device_type, _, _, _ in cprefs.get_device_types(bpy.context):
        cprefs.compute_device_type = device_type
        devices = cprefs.get_devices_for_type(device_type)
        index = 0
        for device in devices:
            if device.type == device_type:
                result.append({'type': device.type, 'name': device.name, 'index': index})
                if device.type in {"HIP", "METAL", "ONEAPI"}:
                    result.append({'type': f"{device.type}-RT", 'name': device.name, 'index': index})
                if device.type in {"OPTIX"}:
                    result.append({'type': f"{device.type}-OSL", 'name': device.name, 'index': index})
                index += 1

    # Get GPU backends
    # TODO: Add support for Vulkan device selection even when backend isn't active.
    # TODO: Cannot retrieve actual GPU name as the gpu module isn't initialized when run in background mode
    #   !152683 adds support to use gpu module in background mode.
    original_gpu_backend = prefs.system.gpu_backend
    for gpu_backend in prefs.system.bl_rna.properties['gpu_backend'].enum_items:
        try:
            prefs.system.gpu_backend = gpu_backend.identifier
        except TypeError:
            # GPU backend isn't available.
            pass
        else:
            result.append({'type': gpu_backend.identifier, 'name': gpu_backend.name})
    prefs.system.gpu_backend = original_gpu_backend

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

        self.devices = [TestDevice('CPU', 'CPU', get_cpu_name(), operating_system),
                        TestDevice('CPU-OSL', 'CPU-OSL', get_cpu_name(), operating_system)]
        self.has_gpus = need_gpus

        if need_gpus and env.blender_executable:
            gpu_devices, _ = env.run_in_blender(get_gpu_device, {})
            for gpu_device in gpu_devices:
                device_type = gpu_device['type']
                device_name = gpu_device['name']
                device_id = device_type
                if 'index' in gpu_device:
                    device_id += "_" + str(gpu_device['index'])
                self.devices.append(TestDevice(device_type, device_id, device_name, operating_system))

    def cpu_device(self) -> TestDevice:
        return self.devices[0]
