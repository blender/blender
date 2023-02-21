# SPDX-License-Identifier: GPL-2.0-or-later

"""
This module (in particular the draw_ui_list function) lets you draw the commonly
used UIList layout, seen all over Blender.

This includes the list itself, and a column of buttons to the right of it, which
contains buttons to add, remove, and move entries up or down, as well as a
drop-down menu.

You can get an example of how to use this via the Blender Text Editor->
Templates->Ui List Generic.
"""

import bpy
from bpy.types import Operator
from bpy.props import (
    EnumProperty,
    StringProperty,
)

__all__ = (
    "draw_ui_list",
)


def draw_ui_list(
        layout,
        context,
        class_name="UI_UL_list",
        *,
        unique_id="",
        list_path,
        active_index_path,
        insertion_operators=True,
        move_operators=True,
        menu_class_name="",
        **kwargs,
):
    """
    Draw a UIList with Add/Remove/Move buttons and a menu.

    :arg layout: UILayout to draw the list in.
    :type layout: :class:`UILayout`
    :arg context: Blender context to get the list data from.
    :type context: :class:`Context`
    :arg class_name: Name of the UIList class to draw. The default is the UIList class that ships with Blender.
    :type class_name: str
    :arg unique_id: Optional identifier, in case wanting to draw multiple unique copies of a list.
    :type unique_id: str
    :arg list_path: Data path of the list relative to context, eg. "object.vertex_groups".
    :type list_path: str
    :arg active_index_path: Data path of the list active index integer relative to context,
       eg. "object.vertex_groups.active_index".
    :type active_index_path: str
    :arg insertion_operators: Whether to draw Add/Remove buttons.
    :type insertion_operators: bool
    :arg move_operators: Whether to draw Move Up/Down buttons.
    :type move_operators: str
    :arg menu_class_name: Identifier of a Menu that should be drawn as a drop-down.
    :type menu_class_name: str

    :returns: The right side column.
    :rtype: :class:`UILayout`.

    Additional keyword arguments are passed to :class:`UIList.template_list`.
    """

    row = layout.row()

    list_owner_path, list_prop_name = list_path.rsplit('.', 1)
    list_owner = _get_context_attr(context, list_owner_path)

    index_owner_path, index_prop_name = active_index_path.rsplit('.', 1)
    index_owner = _get_context_attr(context, index_owner_path)

    list_to_draw = _get_context_attr(context, list_path)

    row.template_list(
        class_name,
        unique_id,
        list_owner, list_prop_name,
        index_owner, index_prop_name,
        rows=4 if list_to_draw else 1,
        **kwargs
    )

    col = row.column()

    if insertion_operators:
        _draw_add_remove_buttons(
            layout=col,
            list_path=list_path,
            active_index_path=active_index_path,
            list_length=len(list_to_draw)
        )
        layout.separator()

    if menu_class_name:
        col.menu(menu_class_name, icon='DOWNARROW_HLT', text="")
        col.separator()

    if move_operators and list_to_draw:
        _draw_move_buttons(
            layout=col,
            list_path=list_path,
            active_index_path=active_index_path,
            list_length=len(list_to_draw)
        )

    # Return the right-side column.
    return col


def _draw_add_remove_buttons(
    *,
    layout,
    list_path,
    active_index_path,
    list_length,
):
    """Draw the +/- buttons to add and remove list entries."""
    add_op = layout.operator(UILIST_OT_entry_add.bl_idname, text="", icon='ADD')
    add_op.list_path = list_path
    add_op.active_index_path = active_index_path

    row = layout.row()
    row.enabled = list_length > 0
    remove_op = row.operator(UILIST_OT_entry_remove.bl_idname, text="", icon='REMOVE')
    remove_op.list_path = list_path
    remove_op.active_index_path = active_index_path


def _draw_move_buttons(
    *,
    layout,
    list_path,
    active_index_path,
    list_length,
):
    """Draw the up/down arrows to move elements in the list."""
    col = layout.column()
    col.enabled = list_length > 1
    move_up_op = layout.operator(UILIST_OT_entry_move.bl_idname, text="", icon='TRIA_UP')
    move_up_op.direction = 'UP'
    move_up_op.list_path = list_path
    move_up_op.active_index_path = active_index_path

    move_down_op = layout.operator(UILIST_OT_entry_move.bl_idname, text="", icon='TRIA_DOWN')
    move_down_op.direction = 'DOWN'
    move_down_op.list_path = list_path
    move_down_op.active_index_path = active_index_path


def _get_context_attr(context, data_path):
    """Return the value of a context member based on its data path."""
    return context.path_resolve(data_path)


def _set_context_attr(context, data_path, value) -> None:
    """Set the value of a context member based on its data path."""
    owner_path, attr_name = data_path.rsplit('.', 1)
    owner = context.path_resolve(owner_path)
    setattr(owner, attr_name, value)


class GenericUIListOperator:
    """Mix-in class containing functionality shared by operators
    that deal with managing Blender list entries."""
    bl_options = {'REGISTER', 'UNDO', 'INTERNAL'}

    list_path: StringProperty()
    active_index_path: StringProperty()

    def get_list(self, context) -> str:
        return _get_context_attr(context, self.list_path)

    def get_active_index(self, context) -> str:
        return _get_context_attr(context, self.active_index_path)

    def set_active_index(self, context, index):
        _set_context_attr(context, self.active_index_path, index)


class UILIST_OT_entry_remove(GenericUIListOperator, Operator):
    """Remove the selected entry from the list"""

    bl_idname = "uilist.entry_remove"
    bl_label = "Remove Selected Entry"

    def execute(self, context):
        my_list = self.get_list(context)
        active_index = self.get_active_index(context)

        my_list.remove(active_index)
        to_index = min(active_index, len(my_list) - 1)
        self.set_active_index(context, to_index)

        return {'FINISHED'}


class UILIST_OT_entry_add(GenericUIListOperator, Operator):
    """Add an entry to the list after the current active item"""

    bl_idname = "uilist.entry_add"
    bl_label = "Add Entry"

    def execute(self, context):
        my_list = self.get_list(context)
        active_index = self.get_active_index(context)

        to_index = min(len(my_list), active_index + 1)

        my_list.add()
        my_list.move(len(my_list) - 1, to_index)
        self.set_active_index(context, to_index)

        return {'FINISHED'}


class UILIST_OT_entry_move(GenericUIListOperator, Operator):
    """Move an entry in the list up or down"""

    bl_idname = "uilist.entry_move"
    bl_label = "Move Entry"

    direction: EnumProperty(
        name="Direction",
        items=(('UP', 'UP', 'UP'),
               ('DOWN', 'DOWN', 'DOWN')),
        default='UP'
    )

    def execute(self, context):
        my_list = self.get_list(context)
        active_index = self.get_active_index(context)

        delta = {
            "DOWN": 1,
            "UP": -1,
        }[self.direction]

        to_index = (active_index + delta) % len(my_list)

        my_list.move(active_index, to_index)
        self.set_active_index(context, to_index)

        return {'FINISHED'}


# Registration.
classes = (
    UILIST_OT_entry_remove,
    UILIST_OT_entry_add,
    UILIST_OT_entry_move,
)

register, unregister = bpy.utils.register_classes_factory(classes)
