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

narrowui = bpy.context.user_preferences.view.properties_width_check

################################################
# Generic Panels (Independent of DataType)


class MotionPathButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_label = "Motion Paths"
    bl_default_closed = True

    def draw_settings(self, context, avs, wide_ui, bones=False):
        layout = self.layout

        mps = avs.motion_paths

        if wide_ui:
            layout.prop(mps, "type", expand=True)
        else:
            layout.prop(mps, "type", text="")

        split = layout.split()

        col = split.column()
        sub = col.column(align=True)
        if (mps.type == 'CURRENT_FRAME'):
            sub.prop(mps, "before_current", text="Before")
            sub.prop(mps, "after_current", text="After")
        elif (mps.type == 'RANGE'):
            sub.prop(mps, "frame_start", text="Start")
            sub.prop(mps, "frame_end", text="End")

        sub.prop(mps, "frame_step", text="Step")
        if bones:
            col.row().prop(mps, "bake_location", expand=True)

        if wide_ui:
            col = split.column()
        col.label(text="Display:")
        col.prop(mps, "show_frame_numbers", text="Frame Numbers")
        col.prop(mps, "highlight_keyframes", text="Keyframes")
        if bones:
            col.prop(mps, "search_all_action_keyframes", text="+ Non-Grouped Keyframes")
        col.prop(mps, "show_keyframe_numbers", text="Keyframe Numbers")


# FIXME: this panel still needs to be ported so that it will work correctly with animviz
class OnionSkinButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_label = "Onion Skinning"
    bl_default_closed = True

    def draw(self, context):
        layout = self.layout

        arm = context.armature
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(arm, "ghost_type", expand=True)
        else:
            layout.prop(arm, "ghost_type", text="")

        split = layout.split()

        col = split.column()

        sub = col.column(align=True)
        if arm.ghost_type == 'RANGE':
            sub.prop(arm, "ghost_frame_start", text="Start")
            sub.prop(arm, "ghost_frame_end", text="End")
            sub.prop(arm, "ghost_size", text="Step")
        elif arm.ghost_type == 'CURRENT_FRAME':
            sub.prop(arm, "ghost_step", text="Range")
            sub.prop(arm, "ghost_size", text="Step")

        if wide_ui:
            col = split.column()
        col.label(text="Display:")
        col.prop(arm, "ghost_only_selected", text="Selected Only")



# NOTE:
# The specialised panel types are derived in their respective UI modules



def register():
    pass


def unregister():
    pass

if __name__ == "__main__":
    register()
