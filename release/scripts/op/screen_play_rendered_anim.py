# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****

# <pep8 compliant>

# History
#
# Originally written by Matt Ebb

import bpy
import subprocess
import os
import platform


def guess_player_path(preset):
    if preset == 'BLENDER24':
        player_path = 'blender'

        if platform.system() == 'Darwin':
            test_path = '/Applications/blender 2.49.app/Contents/MacOS/blender'
        elif platform.system() == 'Windows':
            test_path = '/Program Files/Blender Foundation/Blender/blender.exe'

            if os.path.exists(test_path):
                player_path = test_path

    elif preset == 'DJV':
        player_path = 'djv_view'

        if platform.system() == 'Darwin':
            test_path = '/Applications/djv-0.8.2.app/Contents/Resources/bin/djv_view'
            if os.path.exists(test_path):
                player_path = test_path

    elif preset == 'FRAMECYCLER':
        player_path = 'framecycler'

    elif preset == 'RV':
        player_path = 'rv'


    return player_path


class PlayRenderedAnim(bpy.types.Operator):
    '''Plays back rendered frames/movies using an external player.'''
    bl_idname = "screen.play_rendered_anim"
    bl_label = "Play Rendered Animation"
    bl_register = True
    bl_undo = False

    def execute(self, context):
        sce = context.scene
        rd = sce.render_data
        prefs = context.user_preferences

        preset = prefs.filepaths.animation_player_preset
        player_path = prefs.filepaths.animation_player
        file_path = bpy.utils.expandpath(rd.output_path)

        # try and guess a command line if it doesn't exist
        if player_path == '':
            player_path = guess_player_path(preset)

        if preset in ('FRAMECYCLER', 'RV'):
            # replace the number with '#'
            file_a, file_b = rd.frame_path(frame=0), rd.frame_path(frame=1)
            file = ''.join([(c if file_b[i] == c else "#") for i, c in enumerate(file_a)])
        else:
            # works for movies and images
            file = rd.frame_path(frame=sce.start_frame)

        cmd = [player_path]
        # extra options, fps controls etc.
        if preset == 'BLENDER24':
            opts = ["-a", "-f", str(rd.fps), str(rd.fps_base), file]
            cmd.extend(opts)
        elif preset == 'DJV':
            opts = [file, "-playback_speed", str(rd.fps)]
            cmd.extend(opts)
        elif preset == 'FRAMECYCLER':
            opts = [file, "%d-%d" % (sce.start_frame, sce.end_frame)]
            cmd.extend(opts)
        elif preset == 'RV':
            opts = ["-fps", str(rd.fps), "-play", "[ %s ]" % file]
            cmd.extend(opts)
        else: # 'CUSTOM'
            cmd.append(file)

        # launch it
        try:
            process = subprocess.Popen(cmd)
        except:
            pass
            #raise OSError("Couldn't find an external animation player.")

        return('FINISHED',)

bpy.types.register(PlayRenderedAnim)
