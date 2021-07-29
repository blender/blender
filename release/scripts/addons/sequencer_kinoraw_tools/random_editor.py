# ##### BEGIN GPL LICENSE BLOCK #####
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# Note: the Operator LoadRandomEditOperator was removed since is not
# working. If it is fixed, reimplemented it can be reintroduced later

import bpy
from bpy.types import (
        Operator,
        Panel,
        )
from . import functions


# classes
class RandomScratchOperator(Operator):
    bl_idname = "sequencer.randomscratchoperator"
    bl_label = "Random Scratch Operator"
    bl_description = "Random Scratch Operator"

    @classmethod
    def poll(self, context):
        strip = functions.act_strip(context)
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return strip.type in ('META')
        else:
            return False

    def invoke(self, context, event):
        preferences = context.user_preferences
        random_frames = preferences.addons[__package__].preferences.random_frames

        sce = context.scene
        seq = sce.sequence_editor
        markers = sce.timeline_markers

        if seq:
            strip = seq.active_strip
            if strip is not None:
                if "IN" and "OUT" in markers:
                    sin = markers["IN"].frame
                    sout = markers["OUT"].frame

                    # select active strip
                    strip = context.scene.sequence_editor.active_strip
                    stripname = strip.name
                    # collect strip names inside the meta
                    stripnames = []
                    stripnames.append(strip.name)
                    for i in seq.active_strip.sequences:
                        stripnames.append(i.name)
                    # get strip channel
                    channel = strip.channel
                    repeat = range(int((sout - sin) / random_frames))
                    print(sin, sout, sout - sin, (sout - sin) / random_frames, repeat)

                    for i in repeat:
                        # select all related strips
                        for j in stripnames:
                            strip = seq.sequences_all[j]
                            strip.select = True
                        strip = seq.sequences_all[stripname]
                        seq.active_strip = strip
                        # deselect all other strips
                        for j in context.selected_editable_sequences:
                            if j.name not in stripnames:
                                j.select = False
                        a = bpy.ops.sequencer.duplicate_move()
                        # select new strip
                        newstrip = seq.active_strip
                        # deselect all other strips

                        for j in context.selected_editable_sequences:
                            if j.name != newstrip.name:
                                j.select = False
                        # random cut
                        newstrip.frame_start = sin + i * random_frames
                        rand = functions.randomframe(newstrip)
                        functions.triminout(newstrip, rand, rand + random_frames)
                        newstrip.frame_start = i * random_frames + sin - newstrip.frame_offset_start
                        newstrip.channel = channel + 1
                else:
                    self.report({'WARNING'}, "There is no IN and OUT Markers")
            bpy.ops.sequencer.reload()

        return {'FINISHED'}


class RandomEditorPanel(Panel):
    bl_label = "Random Editor"
    bl_idname = "OBJECT_PT_RandomEditor"
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    @classmethod
    def poll(self, context):
        if context.space_data.view_type in {'SEQUENCER',
                                            'SEQUENCER_PREVIEW'}:
            strip = functions.act_strip(context)
            scn = context.scene
            preferences = context.user_preferences
            prefs = preferences.addons[__package__].preferences
            if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
                if prefs.use_random_editor:
                    return strip.type in ('META')
        else:
            return False

    def draw_header(self, context):
        layout = self.layout
        layout.label(text="", icon="MOD_BUILD")

    def draw(self, context):

        preferences = context.user_preferences
        prefs = preferences.addons[__package__].preferences

        layout = self.layout
        col = layout.column(align=True)
        col.label("Cut duration:")
        col.prop(prefs, "random_frames")
        col.operator("sequencer.randomscratchoperator")
