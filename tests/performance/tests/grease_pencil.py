# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import api
import enum
import time


class RecordStage(enum.Enum):
    INIT = 0,
    WARMUP = 1,
    RECORD = 2,
    FINISHED = 3


WARMUP_SECONDS = 4
WARMUP_FRAMES = 10
RECORD_PLAYBACK_ITER = 3
MIN_NUM_FRAMES_TOTAL = 250
LOG_KEY = "VIEWPORT_PERFORMANCE: "


def _run(args):
    import bpy

    global record_stage
    record_stage = RecordStage.INIT

    bpy.app.handlers.frame_change_post.append(frame_change_handler)
    bpy.ops.screen.animation_play()


def frame_change_handler(scene):
    import bpy

    global record_stage
    global frame_set_mode
    global start_record_time
    global start_warmup_time
    global warmup_frame
    global stop_record_time
    global playback_iteration
    global num_frames

    if record_stage == RecordStage.INIT:
        bpy.context.scene.sync_mode = 'NONE'
        frame_set_mode = False
        # Overwrite animation FPS limit set by .blend files.
        bpy.context.scene.render.fps = 1000

        start_warmup_time = time.perf_counter()
        warmup_frame = 0
        record_stage = RecordStage.WARMUP

    elif record_stage == RecordStage.WARMUP:
        if frame_set_mode:
            # scene.frame_set results in a recursive call to frame_change_handler.
            # Avoid running into a RecursionError.
            return
        warmup_frame += 1
        if time.perf_counter() - start_warmup_time > WARMUP_SECONDS and warmup_frame > WARMUP_FRAMES:
            start_record_time = time.perf_counter()
            playback_iteration = 0
            num_frames = 0
            scene = bpy.context.scene
            frame_set_mode = True
            scene.frame_set(scene.frame_start)
            frame_set_mode = False
            record_stage = RecordStage.RECORD

    elif record_stage == RecordStage.RECORD:
        current_time = time.perf_counter()
        scene = bpy.context.scene
        num_frames += 1
        if scene.frame_current == scene.frame_end:
            playback_iteration += 1

        if playback_iteration >= RECORD_PLAYBACK_ITER and num_frames >= MIN_NUM_FRAMES_TOTAL:
            stop_record_time = current_time
            record_stage = RecordStage.FINISHED

    elif record_stage == RecordStage.FINISHED:
        bpy.ops.screen.animation_cancel()
        elapsed_seconds = stop_record_time - start_record_time
        avg_frame_time = elapsed_seconds / num_frames
        fps = 1.0 / avg_frame_time
        print(f"{LOG_KEY}{{'fps': {fps} }}")
        bpy.app.handlers.frame_change_post.remove(frame_change_handler)
        bpy.ops.wm.quit_blender()


class GreasePencilTest(api.Test):
    def __init__(self, filepath):
        self.filepath = filepath

    def name(self):
        return self.filepath.stem

    def category(self):
        return "grease_pencil"

    def use_device(self) -> bool:
        return True

    def supported_device_types(self):
        return [
            "METAL", "VULKAN", "OPENGL"
        ]

    def use_background(self):
        return False

    def run(self, env, _device_id, gpu_backend):
        args = {}
        _, log = env.run_in_blender(_run, args, ['--gpu-backend', gpu_backend, self.filepath], foreground=True)
        for line in log:
            if line.startswith(LOG_KEY):
                result_str = line[len(LOG_KEY):]
                result = eval(result_str)
                return result

        raise Exception("No playback performance result found in log.")


def generate(env):
    filepaths = env.find_blend_files('grease_pencil/*')
    return [GreasePencilTest(filepath) for filepath in filepaths]
