# ##### BEGIN GPL LICENSE BLOCK #####
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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENCE BLOCK #####

bl_info = {
    "name": "Bone Selection Sets",
    "author": "Inês Almeida, Antony Riakiotakis, Dan Eicher",
    "version": (2, 0, 1),
    "blender": (2, 75, 0),
    "location": "Properties > Object Data (Armature) > Selection Sets",
    "description": "List of Bone sets for easy selection while animating",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Animation/SelectionSets",
    "category": "Animation",
}

import bpy
from bpy.types import (
        Operator,
        Menu,
        Panel,
        UIList,
        PropertyGroup,
        )
from bpy.props import (
        StringProperty,
        IntProperty,
        EnumProperty,
        CollectionProperty,
        )


# Data Structure ##############################################################

# Note: bones are stored by name, this means that if the bone is renamed,
# there can be problems. However, bone renaming is unlikely during animation
class SelectionEntry(PropertyGroup):
    name = StringProperty(name="Bone Name")


class SelectionSet(PropertyGroup):
    name = StringProperty(name="Set Name")
    bone_ids = CollectionProperty(type=SelectionEntry)


# UI Panel w/ UIList ##########################################################

class POSE_MT_selection_sets_specials(Menu):
    bl_label = "Selection Sets Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.selection_set_delete_all", icon='X')
        layout.operator("pose.selection_set_remove_bones", icon='X')


class POSE_PT_selection_sets(Panel):
    bl_label = "Selection Sets"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return (context.object and
                context.object.type == 'ARMATURE' and
                context.object.pose)

    def draw(self, context):
        layout = self.layout

        arm = context.object

        row = layout.row()
        row.enabled = (context.mode == 'POSE')

        # UI list
        rows = 4 if len(arm.selection_sets) > 0 else 1
        row.template_list(
            "POSE_UL_selection_set", "",  # type and unique id
            arm, "selection_sets",  # pointer to the CollectionProperty
            arm, "active_selection_set",  # pointer to the active identifier
            rows=rows
        )

        # add/remove/specials UI list Menu
        col = row.column(align=True)
        col.operator("pose.selection_set_add", icon='ZOOMIN', text="")
        col.operator("pose.selection_set_remove", icon='ZOOMOUT', text="")
        col.menu("POSE_MT_selection_sets_specials", icon='DOWNARROW_HLT', text="")

        # move up/down arrows
        if len(arm.selection_sets) > 0:
            col.separator()
            col.operator("pose.selection_set_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("pose.selection_set_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

        # buttons
        row = layout.row()

        sub = row.row(align=True)
        sub.operator("pose.selection_set_assign", text="Assign")
        sub.operator("pose.selection_set_unassign", text="Remove")

        sub = row.row(align=True)
        sub.operator("pose.selection_set_select", text="Select")
        sub.operator("pose.selection_set_deselect", text="Deselect")


class POSE_UL_selection_set(UIList):
    def draw_item(self, context, layout, data, set, icon, active_data, active_propname, index):
        layout.prop(set, "name", text="", icon='GROUP_BONE', emboss=False)


class POSE_MT_create_new_selection_set(Menu):
    bl_idname = "pose.selection_set_create_new_popup"
    bl_label = "Choose Selection Set"

    def draw(self, context):
        layout = self.layout
        layout.operator("pose.selection_set_add_and_assign",
            text="New Selection Set")


# Operators ###################################################################

class PluginOperator(Operator):
    @classmethod
    def poll(self, context):
        return (context.object and
                context.object.type == 'ARMATURE' and
                context.mode == 'POSE')


class NeedSelSetPluginOperator(PluginOperator):
    @classmethod
    def poll(self, context):
        if super().poll(context):
            arm = context.object
            return (arm.active_selection_set < len(arm.selection_sets) and
                    arm.active_selection_set >= 0)
        return False


class POSE_OT_selection_set_delete_all(PluginOperator):
    bl_idname = "pose.selection_set_delete_all"
    bl_label = "Delete All Sets"
    bl_description = "Deletes All Selection Sets"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object
        arm.selection_sets.clear()
        return {'FINISHED'}


class POSE_OT_selection_set_remove_bones(PluginOperator):
    bl_idname = "pose.selection_set_remove_bones"
    bl_label = "Remove Bones from Sets"
    bl_description = "Removes the Active Bones from All Sets"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object

        # iterate only the selected bones in current pose that are not hidden
        for bone in context.selected_pose_bones:
            for selset in arm.selection_sets:
                if bone.name in selset.bone_ids:
                    idx = selset.bone_ids.find(bone.name)
                    selset.bone_ids.remove(idx)

        return {'FINISHED'}


class POSE_OT_selection_set_move(NeedSelSetPluginOperator):
    bl_idname = "pose.selection_set_move"
    bl_label = "Move Selection Set in List"
    bl_description = "Move the active Selection Set up/down the list of sets"
    bl_options = {'UNDO', 'REGISTER'}

    direction = EnumProperty(
        name="Move Direction",
        description="Direction to move the active Selection Set: UP (default) or DOWN",
        items=[
            ('UP', "Up", "", -1),
            ('DOWN', "Down", "", 1),
        ],
        default='UP'
    )

    @classmethod
    def poll(self, context):
        if super().poll(context):
            arm = context.object
            return len(arm.selection_sets) > 1
        return False

    def execute(self, context):
        arm = context.object

        active_idx = arm.active_selection_set
        new_idx = active_idx + (-1 if self.direction == 'UP' else 1)

        if new_idx < 0 or new_idx >= len(arm.selection_sets):
            return {'FINISHED'}

        arm.selection_sets.move(active_idx, new_idx)
        arm.active_selection_set = new_idx

        return {'FINISHED'}


class POSE_OT_selection_set_add(PluginOperator):
    bl_idname = "pose.selection_set_add"
    bl_label = "Create Selection Set"
    bl_description = "Creates a new empty Selection Set"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object

        new_sel_set = arm.selection_sets.add()

        # naming
        if "SelectionSet" not in arm.selection_sets:
            new_sel_set.name = "SelectionSet"
        else:
            sorted_sets = []
            for selset in arm.selection_sets:
                if selset.name.startswith("SelectionSet."):
                    index = selset.name[13:]
                    if index.isdigit():
                        sorted_sets.append(index)
            sorted_sets = sorted(sorted_sets)
            min_index = 1
            for num in sorted_sets:
                num = int(num)
                if min_index < num:
                    break
                min_index = num + 1
            new_sel_set.name = "SelectionSet.{:03d}".format(min_index)

        # select newly created set
        arm.active_selection_set = len(arm.selection_sets) - 1

        return {'FINISHED'}


class POSE_OT_selection_set_remove(NeedSelSetPluginOperator):
    bl_idname = "pose.selection_set_remove"
    bl_label = "Delete Selection Set"
    bl_description = "Delete a Selection Set"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object

        arm.selection_sets.remove(arm.active_selection_set)

        # change currently active selection set
        numsets = len(arm.selection_sets)
        if (arm.active_selection_set > (numsets - 1) and numsets > 0):
            arm.active_selection_set = len(arm.selection_sets) - 1

        return {'FINISHED'}


class POSE_OT_selection_set_assign(PluginOperator):
    bl_idname = "pose.selection_set_assign"
    bl_label = "Add Bones to Selection Set"
    bl_description = "Add selected bones to Selection Set"
    bl_options = {'UNDO', 'REGISTER'}

    def invoke(self, context, event):
        arm = context.object

        if not (arm.active_selection_set < len(arm.selection_sets)):
            bpy.ops.wm.call_menu("INVOKE_DEFAULT",
                name="pose.selection_set_create_new_popup")
        else:
            bpy.ops.pose.selection_set_assign('EXEC_DEFAULT')

        return {'FINISHED'}

    def execute(self, context):
        arm = context.object
        act_sel_set = arm.selection_sets[arm.active_selection_set]

        # iterate only the selected bones in current pose that are not hidden
        for bone in context.selected_pose_bones:
            if bone.name not in act_sel_set.bone_ids:
                bone_id = act_sel_set.bone_ids.add()
                bone_id.name = bone.name

        return {'FINISHED'}


class POSE_OT_selection_set_unassign(NeedSelSetPluginOperator):
    bl_idname = "pose.selection_set_unassign"
    bl_label = "Remove Bones from Selection Set"
    bl_description = "Remove selected bones from Selection Set"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object
        act_sel_set = arm.selection_sets[arm.active_selection_set]

        # iterate only the selected bones in current pose that are not hidden
        for bone in context.selected_pose_bones:
            if bone.name in act_sel_set.bone_ids:
                idx = act_sel_set.bone_ids.find(bone.name)
                act_sel_set.bone_ids.remove(idx)

        return {'FINISHED'}


class POSE_OT_selection_set_select(NeedSelSetPluginOperator):
    bl_idname = "pose.selection_set_select"
    bl_label = "Select Selection Set"
    bl_description = "Add Selection Set bones to current selection"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object
        act_sel_set = arm.selection_sets[arm.active_selection_set]

        for bone in context.visible_pose_bones:
            if bone.name in act_sel_set.bone_ids:
                bone.bone.select = True

        return {'FINISHED'}


class POSE_OT_selection_set_deselect(NeedSelSetPluginOperator):
    bl_idname = "pose.selection_set_deselect"
    bl_label = "Deselect Selection Set"
    bl_description = "Remove Selection Set bones from current selection"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object
        act_sel_set = arm.selection_sets[arm.active_selection_set]

        for bone in context.selected_pose_bones:
            if bone.name in act_sel_set.bone_ids:
                bone.bone.select = False

        return {'FINISHED'}


class POSE_OT_selection_set_add_and_assign(PluginOperator):
    bl_idname = "pose.selection_set_add_and_assign"
    bl_label = "Create and Add Bones to Selection Set"
    bl_description = "Creates a new Selection Set with the currently selected bones"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        bpy.ops.pose.selection_set_add('EXEC_DEFAULT')
        bpy.ops.pose.selection_set_assign('EXEC_DEFAULT')
        return {'FINISHED'}


# Registry ####################################################################

classes = (
    POSE_MT_create_new_selection_set,
    POSE_MT_selection_sets_specials,
    POSE_PT_selection_sets,
    POSE_UL_selection_set,
    SelectionEntry,
    SelectionSet,
    POSE_OT_selection_set_delete_all,
    POSE_OT_selection_set_remove_bones,
    POSE_OT_selection_set_move,
    POSE_OT_selection_set_add,
    POSE_OT_selection_set_remove,
    POSE_OT_selection_set_assign,
    POSE_OT_selection_set_unassign,
    POSE_OT_selection_set_select,
    POSE_OT_selection_set_deselect,
    POSE_OT_selection_set_add_and_assign,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.Object.selection_sets = CollectionProperty(
            type=SelectionSet,
            name="Selection Sets",
            description="List of groups of bones for easy selection"
            )
    bpy.types.Object.active_selection_set = IntProperty(
            name="Active Selection Set",
            description="Index of the currently active selection set",
            default=0
            )


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)

    del bpy.types.Object.selection_sets
    del bpy.types.Object.active_selection_set


if __name__ == "__main__":
    register()
