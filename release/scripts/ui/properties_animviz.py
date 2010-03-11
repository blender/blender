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

narrowui = 180

################################################
# Generic Panels (Independent of DataType)


class MotionPathButtonsPanel(bpy.types.Panel):
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
            sub.prop(mps, "start_frame", text="Start")
            sub.prop(mps, "end_frame", text="End")

        sub.prop(mps, "frame_step", text="Step")
        if bones:
            col.row().prop(mps, "bake_location", expand=True)

        if wide_ui:
            col = split.column()
        col.label(text="Display:")
        col.prop(mps, "show_frame_numbers", text="Frame Numbers")
        col.prop(mps, "highlight_keyframes", text="Keyframes")
        col.prop(mps, "show_keyframe_numbers", text="Keyframe Numbers")


# FIXME: this panel still needs to be ported so that it will work correctly with animviz
class OnionSkinButtonsPanel(bpy.types.Panel):
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
            sub.prop(arm, "ghost_start_frame", text="Start")
            sub.prop(arm, "ghost_end_frame", text="End")
            sub.prop(arm, "ghost_size", text="Step")
        elif arm.ghost_type == 'CURRENT_FRAME':
            sub.prop(arm, "ghost_step", text="Range")
            sub.prop(arm, "ghost_size", text="Step")

        if wide_ui:
            col = split.column()
        col.label(text="Display:")
        col.prop(arm, "ghost_only_selected", text="Selected Only")

################################################
# Specific Panels for DataTypes


class OBJECT_PT_motion_paths(MotionPathButtonsPanel):
    #bl_label = "Object Motion Paths"
    bl_context = "object"

    def poll(self, context):
        return (context.object)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        self.draw_settings(context, ob.animation_visualisation, wide_ui)

        layout.separator()

        split = layout.split()

        col = split.column()
        col.operator("object.paths_calculate", text="Calculate Paths")

        if wide_ui:
            col = split.column()
        col.operator("object.paths_clear", text="Clear Paths")


class OBJECT_PT_onion_skinning(OnionSkinButtonsPanel):
    #bl_label = "Object Onion Skinning"
    bl_context = "object"

    def poll(self, context):
        return (context.object)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        self.draw_settings(context, ob.animation_visualisation, wide_ui)


class DATA_PT_motion_paths(MotionPathButtonsPanel):
    #bl_label = "Bones Motion Paths"
    bl_context = "data"

    def poll(self, context):
        # XXX: include posemode check?
        return (context.object) and (context.armature)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        self.draw_settings(context, ob.pose.animation_visualisation, wide_ui, bones=True)

        layout.separator()

        split = layout.split()

        col = split.column()
        col.operator("pose.paths_calculate", text="Calculate Paths")

        if wide_ui:
            col = split.column()
        col.operator("pose.paths_clear", text="Clear Paths")


class DATA_PT_onion_skinning(OnionSkinButtonsPanel):
    #bl_label = "Bones Onion Skinning"
    bl_context = "data"

    def poll(self, context):
        # XXX: include posemode check?
        return (context.object) and (context.armature)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        self.draw_settings(context, ob.pose.animation_visualisation, wide_ui, bones=True)

# NOTE:
# The specialised panel types defined here (i.e. OBJECT_PT_*, etc.)
# aren't registered here, but are rather imported to (and registered)
# in the files defining the contexts where they reside. Otherwise,
# these panels appear at the top of the lists by default.
#
# However, we keep these empty register funcs here just in case
# something will need them again one day, and also to make
# it easier to maintain these scripts.
classes = []


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
