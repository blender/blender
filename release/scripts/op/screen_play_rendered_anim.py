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
# --------------------------------------------------------------------------

# History
#
# Originally written by Matt Ebb

import bpy
import subprocess, os, platform

# from BKE_add_image_extension()
img_format_exts = {
    'IRIS':'.rgb',
    'RADHDR':'.hdr',
    'PNG':'png',
    'TARGA':'tga',
    'RAWTARGA':'tga',
    'BMP':'bmp',
    'TIFF':'tif',
    'OPENEXR':'exr',
    'MULTILAYER':'exr',
    'CINEON':'cin',
    'DPX':'dpx',
    'JPEG':'jpg',
    'JPEG2000':'jp2',
    'QUICKTIME_QTKIT':'mov',
    'QUICKTIME_CARBON':'mov',
    'AVIRAW':'avi',
    'AVIJPEG':'avi',
    'AVICODEC':'avi',
    'XVID':'avi',
    'THEORA':'ogg',
    }

def guess_player_path(preset):
    if preset == 'BLENDER24':
        player_path = 'blender'
        
        if platform.system() == 'Darwin':
            test_path = '/Applications/blender 2.49.app/Contents/MacOS/blender'
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
        
        # try and guess a command line if it doesn't exist
        if player_path == '':
            player_path = guess_player_path(preset)
        
        # doesn't support ### frame notation yet
        if preset in ('BLENDER24', 'DJV', 'CUSTOM'):
            file = "%s%04d" % (bpy.utils.expandpath(rd.output_path), sce.start_frame)
        elif preset in ('FRAMECYCLER', 'RV'):
            file = "%s#" % bpy.utils.expandpath(rd.output_path)
        
        if rd.file_extensions:
            file += '.' + img_format_exts[rd.file_format]
        
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

bpy.ops.add(PlayRenderedAnim)