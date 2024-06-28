# SPDX-FileCopyrightText: 2022-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Operator, UILayout, Context
from bpy.props import EnumProperty, StringProperty


def get_context_attr(context: Context, data_path):
    return context.path_resolve(data_path)


def set_context_attr(context: Context, data_path, value):
    items = data_path.split('.')
    setattr(context.path_resolve('.'.join(items[:-1])), items[-1], value)


class GenericUIListOperator(Operator):
    bl_options = {'REGISTER', 'UNDO', 'INTERNAL'}

    list_context_path: StringProperty()
    active_index_context_path: StringProperty()

    def get_list(self, context):
        return get_context_attr(context, self.list_context_path)

    def get_active_index(self, context):
        return get_context_attr(context, self.active_index_context_path)

    def set_active_index(self, context, index):
        set_context_attr(context, self.active_index_context_path, index)


# noinspection PyPep8Naming
class UILIST_OT_entry_remove(GenericUIListOperator):
    """Remove the selected entry from the list"""

    bl_idname = "ui.rigify_list_entry_remove"
    bl_label = "Remove Selected Entry"

    def execute(self, context):
        my_list = self.get_list(context)
        active_index = self.get_active_index(context)

        my_list.remove(active_index)

        to_index = min(active_index, len(my_list) - 1)

        self.set_active_index(context, to_index)

        return {'FINISHED'}


# noinspection PyPep8Naming
class UILIST_OT_entry_add(GenericUIListOperator):
    """Add an entry to the list"""

    bl_idname = "ui.rigify_list_entry_add"
    bl_label = "Add Entry"

    def execute(self, context):
        my_list = self.get_list(context)
        active_index = self.get_active_index(context)

        to_index = min(len(my_list), active_index + 1)

        my_list.add()
        my_list.move(len(my_list) - 1, to_index)
        self.set_active_index(context, to_index)

        return {'FINISHED'}


# noinspection PyPep8Naming
class UILIST_OT_entry_move(GenericUIListOperator):
    """Move an entry in the list up or down"""

    bl_idname = "ui.rigify_list_entry_move"
    bl_label = "Move Entry"

    direction: EnumProperty(
        name="Direction",
        items=[('UP', 'UP', 'UP'),
               ('DOWN', 'DOWN', 'DOWN')],
        default='UP'
    )

    def execute(self, context):
        my_list = self.get_list(context)
        active_index = self.get_active_index(context)

        to_index = active_index + (1 if self.direction == 'DOWN' else -1)

        if to_index > len(my_list) - 1:
            to_index = 0
        elif to_index < 0:
            to_index = len(my_list) - 1

        my_list.move(active_index, to_index)
        self.set_active_index(context, to_index)

        return {'FINISHED'}


def draw_ui_list(
        layout, context, class_name="UI_UL_list", *,
        list_context_path: str,  # Eg. "object.vertex_groups".
        active_index_context_path: str,  # Eg., "object.vertex_groups.active_index".
        insertion_operators=True,
        move_operators=True,
        menu_class_name="",
        **kwargs) -> UILayout:
    """
    This is intended as a replacement for row.template_list().
    By changing the requirements of the parameters, we can provide the Add, Remove and Move Up/Down
    operators without the person implementing the UIList having to worry about that stuff.
    """
    row = layout.row()

    list_owner = get_context_attr(context, ".".join(list_context_path.split(".")[:-1]))
    list_prop_name = list_context_path.split(".")[-1]
    idx_owner = get_context_attr(context, ".".join(active_index_context_path.split(".")[:-1]))
    idx_prop_name = active_index_context_path.split(".")[-1]

    my_list = get_context_attr(context, list_context_path)

    row.template_list(
        class_name,
        list_context_path if class_name == 'UI_UL_list' else "",
        list_owner, list_prop_name,
        idx_owner, idx_prop_name,
        rows=4 if len(my_list) > 0 else 1,
        **kwargs
    )

    col = row.column()

    if insertion_operators:
        add_op = col.operator(UILIST_OT_entry_add.bl_idname, text="", icon='ADD')
        add_op.list_context_path = list_context_path
        add_op.active_index_context_path = active_index_context_path

        row = col.row()
        row.enabled = len(my_list) > 0
        remove_op = row.operator(UILIST_OT_entry_remove.bl_idname, text="", icon='REMOVE')
        remove_op.list_context_path = list_context_path
        remove_op.active_index_context_path = active_index_context_path

        col.separator()

    if menu_class_name != '':
        col.menu(menu_class_name, icon='DOWNARROW_HLT', text="")
        col.separator()

    if move_operators and len(my_list) > 0:
        col = col.column()
        col.enabled = len(my_list) > 1
        move_up_op = col.operator(UILIST_OT_entry_move.bl_idname, text="", icon='TRIA_UP')
        move_up_op.direction = 'UP'
        move_up_op.list_context_path = list_context_path
        move_up_op.active_index_context_path = active_index_context_path

        move_down_op = col.operator(UILIST_OT_entry_move.bl_idname, text="", icon='TRIA_DOWN')
        move_down_op.direction = 'DOWN'
        move_down_op.list_context_path = list_context_path
        move_down_op.active_index_context_path = active_index_context_path

    # Return the right-side column.
    return col


# =============================================
# Registration

classes = (
    UILIST_OT_entry_remove,
    UILIST_OT_entry_add,
    UILIST_OT_entry_move,
)


def register():
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)


def unregister():
    from bpy.utils import unregister_class
    for cls in classes:
        unregister_class(cls)
