# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Originally written by Matt Ebb

import bpy
from bpy.types import Operator
from bpy.app.translations import pgettext_rpt as rpt_


def guess_player_path(preset):
    import sys

    if preset == 'INTERNAL':
        return bpy.app.binary_path

    elif preset == 'DJV':
        player_path = "djv"
        if sys.platform == "darwin":
            import os
            test_path = "/Applications/DJV2.app/Contents/Resources/bin/djv"
            if os.path.exists(test_path):
                player_path = test_path
        elif sys.platform == "win32":
            import winreg

            # NOTE: This can be removed if/when DJV adds their executable to the PATH.
            # See issue 449 on their GITHUB project page.
            reg_path = r"SOFTWARE\Classes\djv\shell\open\command"
            reg_value = None
            try:
                with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, reg_path, 0, winreg.KEY_READ) as regkey:
                    reg_value = winreg.QueryValue(regkey, None)
            except OSError:
                pass

            if reg_value:
                # Remove trailing command line arguments from the path. The
                # registry value looks like: `<full path>\djv.exe "%1"`.
                binary = "djv.exe"
                index = reg_value.find(binary)
                if index > 0:
                    player_path = reg_value[:index + len(binary)]

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

    @staticmethod
    def _frame_path_with_number_char(rd, ch, **kwargs):
        # Replace the number with `ch`.

        # NOTE: make an api call for this would be nice, however this isn't needed in many places.
        file_a = rd.frame_path(frame=0, **kwargs)
        file_b = rd.frame_path(frame=-1, **kwargs)
        assert len(file_b) == len(file_a) + 1

        for number_beg in range(len(file_a)):
            if file_a[number_beg] != file_b[number_beg]:
                break

        for number_end in range(-1, -(len(file_a) + 1), -1):
            if file_a[number_end] != file_b[number_end]:
                break

        number_end += len(file_a) + 1
        return file_a[:number_beg] + (ch * (number_end - number_beg)) + file_a[number_end:]

    def execute(self, context):
        import os
        import subprocess
        from shlex import quote

        scene = context.scene
        rd = scene.render
        prefs = context.preferences
        fps_final = rd.fps / rd.fps_base

        preset = prefs.filepaths.animation_player_preset
        # file_path = bpy.path.abspath(rd.filepath)  # UNUSED
        is_movie = rd.is_movie_format

        views_format = rd.image_settings.views_format
        if rd.use_multiview and views_format == 'INDIVIDUAL':
            view_suffix = rd.views.active.file_suffix
        else:
            view_suffix = ""

        # try and guess a command line if it doesn't exist
        if preset == 'CUSTOM':
            player_path = prefs.filepaths.animation_player
        else:
            player_path = guess_player_path(preset)

        if is_movie is False and preset in {'FRAMECYCLER', 'RV', 'MPLAYER'}:
            file = PlayRenderedAnim._frame_path_with_number_char(rd, "#", view=view_suffix)
            file = bpy.path.abspath(file)  # expand '//'
        else:
            path_valid = True
            # works for movies and images
            file = rd.frame_path(frame=scene.frame_start, preview=scene.use_preview_range, view=view_suffix)
            file = bpy.path.abspath(file)  # expand '//'
            if not os.path.exists(file):
                err_msg = rpt_("File %r not found") % file
                self.report({'WARNING'}, err_msg)
                path_valid = False

            # one last try for full range if we used preview range
            if scene.use_preview_range and not path_valid:
                file = rd.frame_path(frame=scene.frame_start, preview=False, view=view_suffix)
                file = bpy.path.abspath(file)  # expand '//'
                err_msg = rpt_("File %r not found") % file
                if not os.path.exists(file):
                    self.report({'WARNING'}, err_msg)

        cmd = [player_path]
        # extra options, fps controls etc.
        if scene.use_preview_range:
            frame_start = scene.frame_preview_start
            frame_end = scene.frame_preview_end
        else:
            frame_start = scene.frame_start
            frame_end = scene.frame_end
        if preset == 'INTERNAL':
            opts = [
                "-a",
                "-f", str(rd.fps), str(rd.fps_base),
                "-s", str(frame_start),
                "-e", str(frame_end),
                "-j", str(scene.frame_step),
                "-c", str(prefs.system.memory_cache_limit),
                file,
            ]
            cmd.extend(opts)
        elif preset == 'DJV':
            opts = [
                file,
                "-speed", str(fps_final),
                "-in_out", str(frame_start), str(frame_end),
                "-frame", str(scene.frame_current),
                "-time_units", "Frames",
            ]
            cmd.extend(opts)
        elif preset == 'FRAMECYCLER':
            opts = [file, "%d-%d" % (scene.frame_start, scene.frame_end)]
            cmd.extend(opts)
        elif preset == 'RV':
            opts = ["-fps", str(rd.fps), "-play"]
            if scene.use_preview_range:
                opts += [
                    file.replace("#", "", file.count('#') - 1),
                    "%d-%d" % (frame_start, frame_end),
                ]
            else:
                opts.append(file)

            cmd.extend(opts)
        elif preset == 'MPLAYER':
            opts = []
            if is_movie:
                opts.append(file)
            else:
                opts += [
                    ("mf://" + file.replace("#", "?")),
                    "-mf",
                    "fps=%.4f" % fps_final,
                ]

            opts += ["-loop", "0", "-really-quiet", "-fs"]
            cmd.extend(opts)
        else:  # 'CUSTOM'
            cmd.append(file)

        # launch it
        print("Executing command:\n ", " ".join(quote(c) for c in cmd))

        try:
            subprocess.Popen(cmd)
        except BaseException as ex:
            err_msg = rpt_("Couldn't run external animation player with command %r\n%s") % (cmd, ex)
            self.report(
                {'ERROR'},
                err_msg,
            )
            return {'CANCELLED'}

        return {'FINISHED'}


classes = (
    PlayRenderedAnim,
)
