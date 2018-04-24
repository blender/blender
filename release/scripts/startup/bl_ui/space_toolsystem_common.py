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
from bpy.types import (
    Menu,
)

__all__ = (
    "ToolSelectPanelHelper",
)


# (filename -> icon_value) map
_icon_cache = {}

class ToolSelectPanelHelper:
    """
    Generic Class, can be used for any toolbar.

    - keymap_prefix:
      The text prefix for each key-map for this spaces tools.
    - tools_all():
      Returns all tools defined.
    - tools_from_context(context):
      Returns tools available in this context.

    Each tool is a triplet:
      ``(tool_name, manipulator_group_idname, keymap_actions)``
    For a separator in the toolbar, use ``None``.

      Where:
      ``tool_name``
        is the name to display in the interface.
      ``manipulator_group_idname``
        is an optional manipulator group to activate when the tool is set.
      ``keymap_actions``
        an optional triple of: ``(operator_id, operator_properties, keymap_item_args)``
    """

    @staticmethod
    def _icon_value_from_icon_handle(icon_name):
        import os
        if icon_name is not None:
            assert(type(icon_name) is str)
            icon_value = _icon_cache.get(icon_name)
            if icon_value is None:
                dirname = bpy.utils.resource_path('SYSTEM')
                if not dirname:
                    # TODO(campbell): use a better way of finding datafiles.
                    dirname = bpy.utils.resource_path('LOCAL')
                filename = os.path.join(dirname, "datafiles", "icons", icon_name + ".dat")
                try:
                    icon_value = bpy.app.icons.new_triangles_from_file(filename)
                except Exception as ex:
                    if os.path.exists(filename):
                        print("Missing icons:", filename, ex)
                    else:
                        print("Corrupt icon:", filename, ex)
                    icon_value = 0
                _icon_cache[icon_name] = icon_value
            return icon_value
        else:
            return 0

    @staticmethod
    def _tool_is_group(tool):
        return type(tool[0]) is not str

    @staticmethod
    def _tools_flatten(tools):
        for item in tools:
            if item is not None:
                if ToolSelectPanelHelper._tool_is_group(item):
                    for sub_item in item:
                        if sub_item is not None:
                            yield sub_item
                else:
                    yield item

    @classmethod
    def _tool_vars_from_def(cls, item):
        text, icon_name, mp_idname, actions = item
        km, km_idname = (None, None) if actions is None else cls._tool_keymap[text]
        return (km_idname, mp_idname), icon_name

    @staticmethod
    def _tool_vars_from_active_with_index(context):
        workspace = context.workspace
        return (
            (workspace.tool_keymap or None, workspace.tool_manipulator_group or None),
            workspace.tool_index,
        )

    @staticmethod
    def _tool_vars_from_button_with_index(context):
        props = context.button_operator
        return (
            (props.keymap or None or None, props.manipulator_group or None),
            props.index,
        )

    @classmethod
    def _km_actionmouse_simple(cls, kc, text, icon_name, actions):

        # standalone
        def props_assign_recursive(rna_props, py_props):
            for prop_id, value in py_props.items():
                if isinstance(value, dict):
                    props_assign_recursive(getattr(rna_props, prop_id), value)
                else:
                    setattr(rna_props, prop_id, value)

        km_idname = cls.keymap_prefix + text
        km = kc.keymaps.get(km_idname)
        if km is not None:
            return km, km_idname
        km = kc.keymaps.new(km_idname, space_type=cls.bl_space_type, region_type='WINDOW')
        for op_idname, op_props_dict, kmi_kwargs in actions:
            kmi = km.keymap_items.new(op_idname, **kmi_kwargs)
            kmi_props = kmi.properties
            if op_props_dict:
                props_assign_recursive(kmi.properties, op_props_dict)
        return km, km_idname

    @classmethod
    def register(cls):
        wm = bpy.context.window_manager

        # XXX, should we be manipulating the user-keyconfig on load?
        # Perhaps this should only add when keymap items don't already exist.
        #
        # This needs some careful consideration.
        kc = wm.keyconfigs.user

        # {tool_name: (keymap, keymap_idname, manipulator_group_idname), ...}
        cls._tool_keymap = {}

        # Track which tool-group was last used for non-active groups.
        # Blender stores the active tool-group index.
        #
        # {tool_name_first: index_in_group, ...}
        cls._tool_group_active = {}

        # ignore in background mode
        if kc is None:
            return

        for item in ToolSelectPanelHelper._tools_flatten(cls.tools_all()):
            text, icon_name, mp_idname, actions = item
            if actions is not None:
                km, km_idname = cls._km_actionmouse_simple(kc, text, icon_name, actions)
                cls._tool_keymap[text] = km, km_idname

    def draw(self, context):
        # XXX, this UI isn't very nice.
        # We might need to create new button types for this.
        # Since we probably want:
        # - tool-tips that include multiple key shortcuts.
        # - ability to click and hold to expose sub-tools.

        workspace = context.workspace
        tool_def_active, index_active = self._tool_vars_from_active_with_index(context)
        layout = self.layout

        scale_y = 2.0

        for tool_items in self.tools_from_context(context):
            if tool_items:
                col = layout.column(align=True)
                col.scale_y = scale_y
                for item in tool_items:
                    if item is None:
                        col = layout.column(align=True)
                        col.scale_y = scale_y
                        continue

                    if self._tool_is_group(item):
                        is_active = False
                        i = 0
                        for i, sub_item in enumerate(item):
                            if sub_item is None:
                                continue
                            tool_def, icon_name = self._tool_vars_from_def(sub_item)
                            is_active = (tool_def == tool_def_active)
                            if is_active:
                                index = i
                                break
                        del i, sub_item

                        if is_active:
                            # not ideal, write this every time :S
                            self._tool_group_active[item[0][0]] = index
                        else:
                            index = self._tool_group_active.get(item[0][0], 0)

                        item = item[index]
                        use_menu = True
                    else:
                        index = -1
                        use_menu = False

                    tool_def, icon_name = self._tool_vars_from_def(item)
                    is_active = (tool_def == tool_def_active)
                    icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle(icon_name)
                    if use_menu:
                        props = col.operator_menu_hold(
                            "wm.tool_set",
                            text=item[0],
                            depress=is_active,
                            menu="WM_MT_toolsystem_submenu",
                            icon_value=icon_value,
                        )
                    else:
                        props = col.operator(
                            "wm.tool_set",
                            text=item[0],
                            depress=is_active,
                            icon_value=icon_value,
                        )

                    props.keymap = tool_def[0] or ""
                    props.manipulator_group = tool_def[1] or ""
                    props.index = index

    def tools_from_context(cls, context):
        return (cls._tools[None], cls._tools.get(context.mode, ()))


# The purpose of this menu is to be a generic popup to select between tools
# in cases when a single tool allows to select alternative tools.
class WM_MT_toolsystem_submenu(Menu):
    bl_label = ""

    @staticmethod
    def _tool_group_from_button(context):
        # Lookup the tool definitions based on the space-type.
        space_type = context.space_data.type
        cls = next(
            (cls for cls in ToolSelectPanelHelper.__subclasses__()
             if cls.bl_space_type == space_type),
            None
        )
        if cls is not None:
            tool_def_button, index_button = cls._tool_vars_from_button_with_index(context)

            for item_items in cls.tools_from_context(context):
                for item_group in item_items:
                    if (item_group is not None) and ToolSelectPanelHelper._tool_is_group(item_group):
                        if index_button < len(item_group):
                            item = item_group[index_button]
                            tool_def, icon_name = cls._tool_vars_from_def(item)
                            is_active = (tool_def == tool_def_button)
                            if is_active:
                                return cls, item_group, index_button
        return None, None, -1

    def draw(self, context):
        layout = self.layout
        layout.scale_y = 2.0

        cls, item_group, index_active = self._tool_group_from_button(context)
        if item_group is None:
            # Should never happen, just in case
            layout.label("Unable to find toolbar group")
            return

        index = 0
        for item in item_group:
            if item is None:
                layout.separator()
                continue
            tool_def, icon_name = cls._tool_vars_from_def(item)
            icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle(icon_name)
            props = layout.operator(
                "wm.tool_set",
                text=item[0],
                icon_value=icon_value,
            )
            props.keymap = tool_def[0] or ""
            props.manipulator_group = tool_def[1] or ""
            props.index = index
            index += 1


classes = (
    WM_MT_toolsystem_submenu,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
