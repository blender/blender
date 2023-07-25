# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

import api


def _run(args):
    import bpy

    device_type = args['device_type']
    device_index = args['device_index']

    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.render.filepath = args['render_filepath']
    scene.render.image_settings.file_format = 'PNG'
    scene.cycles.device = 'CPU' if device_type == 'CPU' else 'GPU'

    if scene.cycles.use_adaptive_sampling:
        # Render samples specified in file, no other way to measure
        # adaptive sampling performance reliably.
        scene.cycles.time_limit = 0.0
    else:
        # Render for fixed amount of time so it's adaptive to the
        # machine and devices.
        scene.cycles.samples = 16384
        scene.cycles.time_limit = 10.0

    if scene.cycles.device == 'GPU':
        # Enable specified GPU in preferences.
        prefs = bpy.context.preferences
        cprefs = prefs.addons['cycles'].preferences
        cprefs.compute_device_type = device_type
        devices = cprefs.get_devices_for_type(device_type)
        for device in devices:
            device.use = False

        index = 0
        for device in devices:
            if device.type == device_type:
                if index == device_index:
                    device.use = True
                    break
                else:
                    index += 1

    # Render
    bpy.ops.render.render(write_still=True)

    return None


class CyclesTest(api.Test):
    def __init__(self, filepath):
        self.filepath = filepath

    def name(self):
        return self.filepath.stem

    def category(self):
        return "cycles"

    def use_device(self):
        return True

    def run(self, env, device_id):
        tokens = device_id.split('_')
        device_type = tokens[0]
        device_index = int(tokens[1]) if len(tokens) > 1 else 0
        args = {'device_type': device_type,
                'device_index': device_index,
                'render_filepath': str(env.log_file.parent / (env.log_file.stem + '.png'))}

        _, lines = env.run_in_blender(_run, args, ['--debug-cycles', '--verbose', '2', self.filepath])

        # Parse render time from output
        prefix_time = "Render time (without synchronization): "
        prefix_memory = "Peak: "
        prefix_time_per_sample = "Average time per sample: "
        time = None
        time_per_sample = None
        memory = None
        for line in lines:
            line = line.strip()
            offset = line.find(prefix_time)
            if offset != -1:
                time = line[offset + len(prefix_time):]
                time = float(time)
            offset = line.find(prefix_time_per_sample)
            if offset != -1:
                time_per_sample = line[offset + len(prefix_time_per_sample):]
                time_per_sample = time_per_sample.split()[0]
                time_per_sample = float(time_per_sample)
            offset = line.find(prefix_memory)
            if offset != -1:
                memory = line[offset + len(prefix_memory):]
                memory = memory.split()[0].replace(',', '')
                memory = float(memory)

        if time_per_sample:
            time = time_per_sample

        if not (time and memory):
            raise Exception("Error parsing render time output")

        return {'time': time, 'peak_memory': memory}


def generate(env):
    filepaths = env.find_blend_files('cycles/*')
    return [CyclesTest(filepath) for filepath in filepaths]
