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

# <pep8-80 compliant>

# Originally written by Matt Ebb

import bpy
from bpy.types import Operator
import os


def guess_player_path(preset):
    import sys

    if preset == 'INTERNAL':
        return bpy.app.binary_path
    elif preset == 'BLENDER24':
        player_path = "blender"

        if sys.platform == "darwin":
            test_path = "/Applications/blender 2.49.app/Contents/MacOS/blender"
        elif sys.platform[:3] == "win":
            test_path = "/Program Files/Blender Foundation/Blender/blender.exe"

            if os.path.exists(test_path):
                player_path = test_path

    elif preset == 'DJV':
        player_path = "djv_view"

        if sys.platform == "darwin":
            # TODO, crummy supporting only 1 version,
            # could find the newest installed version
            test_path = ("/Applications/djv-0.8.2.app"
                         "/Contents/Resources/bin/djv_view")
            if os.path.exists(test_path):
                player_path = test_path

    elif preset == 'FRAMECYCLER':
        player_path = "framecycler"

    elif preset == 'RV':
        player_path = "rv"

    elif preset == 'MPLAYER':
        player_path = "mplayer"

    else:
        player_path = ""

    return player_path


class PlayRenderedAnim(Operator):
    """Play back rendered frames/movies using an external player"""
    bl_idname = "render.play_rendered_anim"
    bl_label = "Play Rendered Animation"
    bl_options = {'REGISTER'}

    def execute(self, context):
        import subprocess

        scene = context.scene
        rd = scene.render
        prefs = context.user_preferences

        preset = prefs.filepaths.animation_player_preset
        player_path = prefs.filepaths.animation_player
        # file_path = bpy.path.abspath(rd.filepath)  # UNUSED
        is_movie = rd.is_movie_format

        # try and guess a command line if it doesn't exist
        if player_path == "":
            player_path = guess_player_path(preset)

        if is_movie is False and preset in {'FRAMECYCLER', 'RV', 'MPLAYER'}:
            # replace the number with '#'
            file_a = rd.frame_path(frame=0)

            # TODO, make an api call for this
            frame_tmp = 9
            file_b = rd.frame_path(frame=frame_tmp)

            while len(file_a) == len(file_b):
                frame_tmp = (frame_tmp * 10) + 9
                file_b = rd.frame_path(frame=frame_tmp)
            file_b = rd.frame_path(frame=int(frame_tmp / 10))

            file = ("".join((c if file_b[i] == c else "#")
                    for i, c in enumerate(file_a)))
            del file_a, file_b, frame_tmp
            file = bpy.path.abspath(file)  # expand '//'
        else:
            # works for movies and images
            file = rd.frame_path(frame=scene.frame_start)
            file = bpy.path.abspath(file)  # expand '//'
            if not os.path.exists(file):
                self.report({'WARNING'}, "File %r not found" % file)

        cmd = [player_path]
        # extra options, fps controls etc.
        if scene.use_preview_range:
            frame_start = scene.frame_preview_start
            frame_end = scene.frame_preview_end
        else:
            frame_start = scene.frame_start
            frame_end = scene.frame_end
        if preset == 'INTERNAL':
            opts = ["-a",
                    "-f", str(rd.fps), str(rd.fps_base),
                    "-s", str(frame_start),
                    "-e", str(frame_end),
                    "-j", str(scene.frame_step),
                    file]
            cmd.extend(opts)
        elif preset == 'DJV':
            opts = [file, "-playback_speed", "%d" % int(rd.fps / rd.fps_base)]
            cmd.extend(opts)
        elif preset == 'FRAMECYCLER':
            opts = [file, "%d-%d" % (scene.frame_start, scene.frame_end)]
            cmd.extend(opts)
        elif preset == 'RV':
            opts = ["-fps", str(rd.fps), "-play", "[ %s ]" % file]
            cmd.extend(opts)
        elif preset == 'MPLAYER':
            opts = []
            if is_movie:
                opts.append(file)
            else:
                opts += [("mf://%s" % file.replace("#", "?")),
                         "-mf",
                         "fps=%.4f" % (rd.fps / rd.fps_base),
                         ]

            opts += ["-loop", "0", "-really-quiet", "-fs"]
            cmd.extend(opts)
        else:  # 'CUSTOM'
            cmd.append(file)

        # launch it
        print("Executing command:\n  %r" % " ".join(cmd))

        # workaround for boost 1.46, can be eventually removed. bug: [#32350]
        env_copy = os.environ.copy()
        if preset == 'INTERNAL':
            env_copy["LC_ALL"] = "C"
        # end workaround

        try:
            subprocess.Popen(cmd, env=env_copy)
        except Exception as e:
            self.report({'ERROR'},
                        "Couldn't run external animation player with command "
                        "%r\n%s" % (" ".join(cmd), str(e)))
            return {'CANCELLED'}

        return {'FINISHED'}
