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

# <pep8 compliant>

import bpy

from bpy.props import *


class SequencerCrossfadeSounds(bpy.types.Operator):
    '''Do crossfading volume animation of two selected sound strips.'''

    bl_idname = "sequencer.crossfade_sounds"
    bl_label = "Crossfade sounds"
    bl_options = {'REGISTER', 'UNDO'}

    def poll(self, context):
        if context.scene and context.scene.sequence_editor and context.scene.sequence_editor.active_strip:
            return context.scene.sequence_editor.active_strip.type == 'SOUND'
        else:
            return False

    def execute(self, context):
        seq1 = None
        seq2 = None
        for s in context.scene.sequence_editor.sequences:
            if s.selected and s.type == 'SOUND':
                if seq1 == None:
                    seq1 = s
                elif seq2 == None:
                    seq2 = s
                else:
                    seq2 = None
                    break
        if seq2 == None:
            self.report({'ERROR'}, "Select 2 sound strips.")
            return {'CANCELLED'}
        if seq1.frame_final_start > seq2.frame_final_start:
            s = seq1
            seq1 = seq2
            seq2 = s
        if seq1.frame_final_end > seq2.frame_final_start:
            tempcfra = context.scene.frame_current
            context.scene.frame_current = seq2.frame_final_start
            seq1.keyframe_insert('volume')
            context.scene.frame_current = seq1.frame_final_end
            seq1.volume = 0
            seq1.keyframe_insert('volume')
            seq2.keyframe_insert('volume')
            context.scene.frame_current = seq2.frame_final_start
            seq2.volume = 0
            seq2.keyframe_insert('volume')
            context.scene.frame_current = tempcfra
            return {'FINISHED'}
        else:
            self.report({'ERROR'}, "The selected strips don't overlap.")
            return {'CANCELLED'}


class SequencerCutMulticam(bpy.types.Operator):
    '''Cut multicam strip and select camera.'''

    bl_idname = "sequencer.cut_multicam"
    bl_label = "Cut multicam"
    bl_options = {'REGISTER', 'UNDO'}

    camera = IntProperty(name="Camera",
            default=1, min=1, max=32, soft_min=1, soft_max=32)

    def poll(self, context):
        if context.scene and context.scene.sequence_editor and context.scene.sequence_editor.active_strip:
            return context.scene.sequence_editor.active_strip.type == 'MULTICAM'
        else:
            return False

    def execute(self, context):
        camera = self.properties.camera

        s = context.scene.sequence_editor.active_strip

        if s.multicam_source == camera:
            return {'FINISHED'}

        if not s.selected:
            s.selected = True

        cfra = context.scene.frame_current
        bpy.ops.sequencer.cut(frame=cfra, type='HARD', side='RIGHT')
        for s in context.scene.sequence_editor.sequences_all:
            if s.selected and s.type == 'MULTICAM' and s.frame_final_start <= cfra and cfra < s.frame_final_end:
                context.scene.sequence_editor.active_strip = s

        context.scene.sequence_editor.active_strip.multicam_source = camera
        return {'FINISHED'}


class SequencerDeinterlaceSelectedMovies(bpy.types.Operator):
    '''Deinterlace all selected movie sources.'''

    bl_idname = "sequencer.deinterlace_selected_movies"
    bl_label = "Deinterlace Movies"
    bl_options = {'REGISTER', 'UNDO'}

    def poll(self, context):
        if context.scene and context.scene.sequence_editor:
            return True
        else:
            return False

    def execute(self, context):
        for s in context.scene.sequence_editor.sequences_all:
            if s.selected and s.type == 'MOVIE':
                s.de_interlace = True

        return {'FINISHED'}


def register():
    register = bpy.types.register

    register(SequencerCrossfadeSounds)
    register(SequencerCutMulticam)
    register(SequencerDeinterlaceSelectedMovies)

def unregister():
    unregister = bpy.types.unregister

    unregister(SequencerCrossfadeSounds)
    unregister(SequencerCutMulticam)
    unregister(SequencerDeinterlaceSelectedMovies)

 

if __name__ == "__main__":
    register()
