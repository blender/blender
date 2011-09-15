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

# Generic Panels (Independent of DataType)

# NOTE:
# The specialised panel types are derived in their respective UI modules
# dont register these classes since they are only helpers.
from blf import gettext as _

class MotionPathButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_label = "Motion Paths"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_settings(self, context, avs, bones=False):
        layout = self.layout

        mps = avs.motion_path

        layout.prop(mps, "type", expand=True)

        split = layout.split()

        col = split.column()
        sub = col.column(align=True)
        if (mps.type == 'CURRENT_FRAME'):
            sub.prop(mps, "frame_before", text=_("Before"))
            sub.prop(mps, "frame_after", text=_("After"))
        elif (mps.type == 'RANGE'):
            sub.prop(mps, "frame_start", text=_("Start"))
            sub.prop(mps, "frame_end", text=_("End"))

        sub.prop(mps, "frame_step", text=_("Step"))
        if bones:
            col.row().prop(mps, "bake_location", expand=True)

        col = split.column()
        col.label(text=_("Display:"))
        col.prop(mps, "show_frame_numbers", text=_("Frame Numbers"))
        col.prop(mps, "show_keyframe_highlight", text=_("Keyframes"))
        if bones:
            col.prop(mps, "show_keyframe_action_all", text=_("+ Non-Grouped Keyframes"))
        col.prop(mps, "show_keyframe_numbers", text=_("Keyframe Numbers"))


# FIXME: this panel still needs to be ported so that it will work correctly with animviz
class OnionSkinButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_label = "Onion Skinning"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        arm = context.armature

        layout.prop(arm, "ghost_type", expand=True)

        split = layout.split()

        col = split.column()

        sub = col.column(align=True)
        if arm.ghost_type == 'RANGE':
            sub.prop(arm, "ghost_frame_start", text=_("Start"))
            sub.prop(arm, "ghost_frame_end", text=_("End"))
            sub.prop(arm, "ghost_size", text=_("Step"))
        elif arm.ghost_type == 'CURRENT_FRAME':
            sub.prop(arm, "ghost_step", text=_("Range"))
            sub.prop(arm, "ghost_size", text=_("Step"))

        col = split.column()
        col.label(text=_("Display:"))
        col.prop(arm, "show_only_ghost_selected", text=_("Selected Only"))

if __name__ == "__main__":  # only for live edit.
    import bpy
    bpy.utils.register_module(__name__)
