# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import api
import json
import pathlib
import tempfile


LOG_KEY = "VSE_PLAYBACK_PERFORMANCE: "


def _run_playback(args):
    import bpy
    import time

    scene = bpy.context.scene
    scene.render.compositor_device = ('CPU' if args['device_type'] == 'CPU' else 'GPU')
    window = bpy.context.window
    preview = next((
        area for area in window.screen.areas
        if area.type == 'SEQUENCE_EDITOR' and
        area.spaces.active.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}
    ), None)
    if preview is None:
        raise RuntimeError("VSE playback benchmark requires a visible Sequencer preview")
    if scene.frame_end <= scene.frame_start:
        raise RuntimeError("VSE playback benchmark requires at least two frames")

    # note: keep saved FPS, playback synchronization, cache, prefetch settings
    # as they are in the file.
    scene.use_preview_range = False  # use full scene range
    if hasattr(scene, 'playback_loop_mode'):
        scene.playback_loop_mode = 'INFINITE'
    scene.frame_set(scene.frame_start)
    previous_frame = scene.frame_start
    num_frames = 0
    start_time = None
    result = None

    def finish():
        if result is not None:
            print(LOG_KEY + json.dumps(result), flush=True)
        bpy.ops.wm.quit_blender()

    def frame_change_handler(current_scene, _depsgraph=None):
        nonlocal previous_frame, num_frames, result
        if current_scene != scene or start_time is None:
            return
        frame = scene.frame_current
        if frame == previous_frame:
            return
        # due to frame drops the last frame might get skipped, so detect the
        # first frame wrap instead.
        if frame < previous_frame:
            elapsed = time.perf_counter() - start_time
            bpy.app.handlers.frame_change_post.remove(frame_change_handler)
            bpy.ops.screen.animation_cancel(restore_frame=False)
            if num_frames:
                result = {'fps': num_frames / elapsed}
            bpy.app.timers.register(finish)
            return
        previous_frame = frame
        num_frames += 1

    def start():
        nonlocal start_time
        bpy.app.handlers.frame_change_post.append(frame_change_handler)
        region = next(region for region in preview.regions if region.type == 'WINDOW')
        with bpy.context.temp_override(window=window, area=preview, region=region):
            start_time = time.perf_counter()
            bpy.ops.screen.animation_play()

    def watchdog():
        if result is not None:
            return None
        duration = (scene.frame_end - scene.frame_start + 1) * scene.render.fps_base / scene.render.fps
        if start_time is not None and time.perf_counter() - start_time > max(60.0, duration * 10):
            print("VSE playback benchmark timed out", flush=True)
            bpy.ops.wm.quit_blender()
            return None
        return 1.0

    bpy.app.timers.register(start)
    bpy.app.timers.register(watchdog, first_interval=1.0)


def _run_render(args):
    import bpy
    import time

    scene = bpy.context.scene
    scene.render.compositor_device = ('CPU' if args['device_type'] == 'CPU' else 'GPU')
    scene.render.filepath = args['output_filepath']

    start_time = time.perf_counter()
    status = bpy.ops.render.render(animation=True)
    elapsed = time.perf_counter() - start_time
    if status != {'FINISHED'}:
        raise RuntimeError("VSE animation render did not finish")
    return {'time': elapsed}


class VSETest(api.Test):
    def __init__(self, filepath, playback):
        self.filepath = filepath
        self.playback = playback

    def name(self):
        suffix = "playback" if self.playback else "render"
        return self.filepath.stem + "_" + suffix

    def category(self):
        return "vse"

    def use_device(self):
        return True

    def supported_device_types(self):
        return [
            "CPU", "METAL", "METAL-RT", "VULKAN", "OPENGL",
        ]

    def remove_redundant_device_types(self, devices):
        return self.remove_redundant_hwrt_device_types(devices)

    def use_background(self):
        return not self.playback

    def run(self, env, device_id, gpu_backend):
        device_type = device_id.split('_')[0]

        blender_args = [self.filepath]
        if device_type != 'CPU':
            blender_args = api.test.Test.blender_gpu_arguments(device_id, gpu_backend) + blender_args

        # similar to compositor tests, device selection controls compositor
        # CPU/GPU execution
        args = {'device_type': device_type}

        if self.playback:
            _, log = env.run_in_blender(_run_playback, args, blender_args, foreground=True)
            for line in log:
                if line.startswith(LOG_KEY):
                    result = json.loads(line[len(LOG_KEY):])
                    return result

            raise RuntimeError("No VSE playback performance result found in log")

        with tempfile.TemporaryDirectory(prefix="vse_benchmark_") as output_dir:
            args['output_filepath'] = str(pathlib.Path(output_dir) / 'render_')
            result, _ = env.run_in_blender(_run_render, args, blender_args)
        return result


def generate(env):
    filepaths = env.find_blend_files('vse/*')
    return [VSETest(filepath, playback) for filepath in filepaths for playback in (True, False)]
