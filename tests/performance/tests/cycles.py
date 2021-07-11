# Apache License, Version 2.0

import api
import os


def _run(args):
    import bpy
    import time

    device_type = args['device_type']
    device_index = args['device_index']

    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.render.filepath = args['render_filepath']
    scene.render.image_settings.file_format = 'PNG'
    scene.cycles.device = 'CPU' if device_type == 'CPU' else 'GPU'

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

        _, lines = env.run_in_blender(_run, args, ['--debug-cycles', '--verbose', '1', self.filepath])

        # Parse render time from output
        prefix = "Render time (without synchronization): "
        time = 0.0
        for line in lines:
            line = line.strip()
            offset = line.find(prefix)
            if offset != -1:
                time = line[offset + len(prefix):]
                return {'time': float(time)}

        raise Exception("Error parsing render time output")


def generate(env):
    filepaths = env.find_blend_files('cycles-x/*')
    return [CyclesTest(filepath) for filepath in filepaths]
