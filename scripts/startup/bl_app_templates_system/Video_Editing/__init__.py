# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.app.handlers import persistent


def update_factory_startup_screens():
    screen = bpy.data.screens["Video Editing"]
    for area in screen.areas:
        if area.type == 'FILE_BROWSER':
            space = area.spaces.active
            params = space.params
            params.use_filter_folder = True


def update_factory_startup_ffmpeg_preset():
    from bpy import context

    preset = "H264_in_MP4"
    preset_filepath = bpy.utils.preset_find(preset, preset_path="ffmpeg")
    if not preset_filepath:
        print("Preset %r not found" % preset)

    for scene in bpy.data.scenes:
        render = scene.render
        render.image_settings.file_format = 'FFMPEG'

        if preset_filepath:
            with context.temp_override(scene=scene):
                bpy.ops.script.python_file_run(filepath=preset_filepath)

        render.ffmpeg.audio_codec = 'AAC'
        render.ffmpeg.audio_bitrate = 256


@persistent
def load_handler(_):
    update_factory_startup_screens()
    if bpy.app.build_options.codec_ffmpeg:
        update_factory_startup_ffmpeg_preset()


def register():
    bpy.app.handlers.load_factory_startup_post.append(load_handler)


def unregister():
    bpy.app.handlers.load_factory_startup_post.remove(load_handler)
