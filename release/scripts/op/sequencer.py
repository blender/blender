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

class SequencerCrossfadeSounds(bpy.types.Operator):
    '''Do crossfading volume animation of two selected sound strips.'''

    bl_idname = "sequencer.crossfade_sounds"
    bl_label = "Crossfade sounds"
    bl_register = True
    bl_undo = True

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
        if seq1.start_frame_final > seq2.start_frame_final:
            s = seq1
            seq1 = seq2
            seq2 = s
        if seq1.end_frame_final > seq2.start_frame_final:
            tempcfra = context.scene.current_frame
            context.scene.current_frame = seq2.start_frame_final
            seq1.keyframe_insert('volume')
            context.scene.current_frame = seq1.end_frame_final
            seq1.volume = 0
            seq1.keyframe_insert('volume')
            seq2.keyframe_insert('volume')
            context.scene.current_frame = seq2.start_frame_final
            seq2.volume = 0
            seq2.keyframe_insert('volume')
            context.scene.current_frame = tempcfra
            return {'FINISHED'}
        else:
            self.report({'ERROR'}, "The selected strips don't overlap.")
            return {'CANCELLED'}


def register():
    bpy.types.register(SequencerCrossfadeSounds)

def unregister():
    bpy.types.unregister(SequencerCrossfadeSounds)

if __name__ == "__main__":
    bpy.ops.sequencer.crossfade_sounds()
