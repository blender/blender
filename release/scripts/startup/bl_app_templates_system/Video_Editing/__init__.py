# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

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
    preset = "H264_in_MP4"
    preset_filepath = bpy.utils.preset_find(preset, preset_path="ffmpeg")
    if not preset_filepath:
        print("Preset %r not found" % preset)

    for scene in bpy.data.scenes:
        render = scene.render
        render.image_settings.file_format = 'FFMPEG'

        if preset_filepath:
            bpy.ops.script.python_file_run({"scene": scene}, filepath=preset_filepath)

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
