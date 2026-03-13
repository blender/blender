# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import platform
import subprocess
import logging

logger = logging.getLogger(__name__)


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


def get_gpu_device_cycles(args: None) -> dict:
    # Get the list of available Cycles GPU devices.
    import bpy

    prefs = bpy.context.preferences
    if 'cycles' not in prefs.addons.keys():
        return {'devices': []}
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
    return {'devices': result}


def get_gpu_device_backend(args: dict) -> dict:

    import bpy
    import gpu

    result = []

    prefs = bpy.context.preferences
    gpu_backend = args['gpu_backend'].upper()

    try:
        gpu.init()
    except AttributeError:
        # `gpu.init` has been introduced in March 2026, previous versions are not able to access gpu module and require a fallback.
        original_gpu_backend = prefs.system.gpu_backend
        try:
            prefs.system.gpu_backend = gpu_backend
        except TypeError:
            # GPU backend isn't available.
            pass
        else:
            result.append({'type': gpu_backend, 'name': gpu_backend})
        prefs.system.gpu_backend = original_gpu_backend
    else:
        if gpu.platform.backend_type_get() == args['gpu_backend'].upper():
            result.append({'type': gpu.platform.backend_type_get(), 'name': gpu.platform.renderer_get()})

    return {'devices': result}


def get_gpu_devices(env) -> list:
    """
    Return a list of devices available in default blender executable.
    """
    from api.environment import TestFailure

    result = []

    try:
        cycles_devices, _ = env.run_in_blender(get_gpu_device_cycles, {})
    except TestFailure as failure:
        logger.error("Unable to receive cycles device list", exc_info=failure)
    else:
        result += cycles_devices.get('devices', [])

    for backend in ['vulkan', 'opengl', 'metal']:
        try:
            backend_devices, _ = env.run_in_blender(
                get_gpu_device_backend, {'gpu_backend': backend}, ['--gpu-backend', backend])
        except TestFailure as failure:
            logger.error("Unable to receive device list for '{backend}'", exc_info=failure)
        else:
            result += backend_devices.get('devices', [])

    return result


class TestDevice:
    def __init__(self, device_type: str, device_id: str, name: str, cpu: str, operating_system: str):
        self.type = device_type
        self.id = device_id
        self.name = name
        self.cpu = cpu
        self.operating_system = operating_system


class TestMachine:
    def __init__(self, env, need_gpus: bool):
        operating_system = platform.system()

        device_cpu = get_cpu_name()
        self.devices = [TestDevice('CPU', 'CPU', device_cpu, device_cpu, operating_system),
                        TestDevice('CPU-OSL', 'CPU-OSL', device_cpu, device_cpu, operating_system)]
        self.has_gpus = need_gpus

        if need_gpus and env.blender_executable:
            gpu_devices = get_gpu_devices(env)
            for gpu_device in gpu_devices:
                device_type = gpu_device['type']
                device_name = gpu_device['name']
                device_id = device_type
                if 'index' in gpu_device:
                    device_id += "_" + str(gpu_device['index'])
                self.devices.append(TestDevice(device_type, device_id, device_name, device_cpu, operating_system))

    def cpu_device(self) -> TestDevice:
        return self.devices[0]
