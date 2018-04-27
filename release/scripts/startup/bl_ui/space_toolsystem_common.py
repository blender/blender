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
    "ToolDef",
)

# Support reloading icons.
if "_icon_cache" in locals():
    release = bpy.app.icons.release
    for icon_value in _icon_cache.values():
        release(icon_value)
    del release


# (filename -> icon_value) map
_icon_cache = {}

def _keymap_fn_from_seq(keymap_data):

    # standalone
    def _props_assign_recursive(rna_props, py_props):
        for prop_id, value in py_props.items():
            if isinstance(value, dict):
                _props_assign_recursive(getattr(rna_props, prop_id), value)
            else:
                setattr(rna_props, prop_id, value)

    def keymap_fn(km):
        for op_idname, op_props_dict, kmi_kwargs in keymap_fn.keymap_data:
            kmi = km.keymap_items.new(op_idname, **kmi_kwargs)
            kmi_props = kmi.properties
            if op_props_dict:
                _props_assign_recursive(kmi.properties, op_props_dict)
    keymap_fn.keymap_data = keymap_data
    return keymap_fn


class ToolDef:
    """
    Tool definition,
    This class is never instanced, it's used as definition for tool types.

    Since we want to define functions here, it's more convenient to declare class-methods
    then functions in a dict or tuple.
    """
    __slots__ = ()

    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    def __init_subclass__(cls):
        # All classes must have a name
        assert(cls.text is not None)
        # We must have a key-map or widget (otherwise the tool does nothing!)
        assert(cls.keymap is not None or cls.widget is not None)

        if type(cls.keymap) is tuple:
            cls.keymap = _keymap_fn_from_seq(cls.keymap)


    # The name to display in the interface.
    text = None
    # The name of the icon to use (found in ``release/datafiles/icons``) or None for no icon.
    icon = None
    # An optional manipulator group to activate when the tool is set or None for no widget.
    widget = None
    # Optional keymap for tool, either:
    # - A function that populates a keymaps passed in as an argument.
    # - A tuple filled with triple's of:
    #   ``(operator_id, operator_properties, keymap_item_args)``.
    keymap = None
    # Optional draw settings (operator options, toolsettings).
    draw_settings = None


class ToolSelectPanelHelper:
    """
    Generic Class, can be used for any toolbar.

    - keymap_prefix:
      The text prefix for each key-map for this spaces tools.
    - tools_all():
      Returns (context_mode, tools) tuple pair for all tools defined.
    - tools_from_context(context):
      Returns tools available in this context.

    Each tool is a 'ToolDef' or None for a separator in the toolbar, use ``None``.
    """

    @staticmethod
    def _icon_value_from_icon_handle(icon_name):
        import os
        if icon_name is not None:
            assert(type(icon_name) is str)
            icon_value = _icon_cache.get(icon_name)
            if icon_value is None:
                dirname = bpy.utils.resource_path('LOCAL')
                if not os.path.exists(dirname):
                    # TODO(campbell): use a better way of finding datafiles.
                    dirname = bpy.utils.resource_path('SYSTEM')
                filename = os.path.join(dirname, "datafiles", "icons", icon_name + ".dat")
                try:
                    icon_value = bpy.app.icons.new_triangles_from_file(filename)
                except Exception as ex:
                    if not os.path.exists(filename):
                        print("Missing icons:", filename, ex)
                    else:
                        print("Corrupt icon:", filename, ex)
                    icon_value = 0
                _icon_cache[icon_name] = icon_value
            return icon_value
        else:
            return 0

    @staticmethod
    def _tools_flatten(tools):
        for item in tools:
            if item is not None:
                if type(item) is tuple:
                    for sub_item in item:
                        if sub_item is not None:
                            yield sub_item
                else:
                    yield item

    @classmethod
    def _tool_vars_from_def(cls, item, context_mode):
        # For now be strict about whats in this dict
        # prevent accidental adding unknown keys.
        text = item.text
        icon_name = item.icon
        mp_idname = item.widget
        keymap_fn = item.keymap
        if keymap_fn is None:
            km, km_idname = (None, None)
        else:
            km_test = cls._tool_keymap.get((context_mode, text))
            if km_test is None and context_mode is not None:
                km_test = cls._tool_keymap[None, text]
            km, km_idname = km_test
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
    def _km_action_simple(cls, kc, context_mode, text, keymap_fn):

        if context_mode is None:
            context_mode = "All"
        km_idname = f"{cls.keymap_prefix} {context_mode}, {text}"
        km = kc.keymaps.get(km_idname)
        if km is not None:
            return km, km_idname
        km = kc.keymaps.new(km_idname, space_type=cls.bl_space_type, region_type='WINDOW')
        keymap_fn(km)
        return km, km_idname

    @classmethod
    def register(cls):
        wm = bpy.context.window_manager

        # XXX, should we be manipulating the user-keyconfig on load?
        # Perhaps this should only add when keymap items don't already exist.
        #
        # This needs some careful consideration.
        kc = wm.keyconfigs.user

        # {context_mode: {tool_name: (keymap, keymap_idname, manipulator_group_idname), ...}, ...}
        cls._tool_keymap = {}

        # Track which tool-group was last used for non-active groups.
        # Blender stores the active tool-group index.
        #
        # {tool_name_first: index_in_group, ...}
        cls._tool_group_active = {}

        # ignore in background mode
        if kc is None:
            return

        for context_mode, tools in cls.tools_all():
            for item_parent in tools:
                if item_parent is None:
                    continue
                for item in item_parent if (type(item_parent) is tuple) else (item_parent,):
                    if item is None:
                        continue
                    keymap_data = item.keymap
                    if keymap_data is not None:
                        text = item.text
                        icon_name = item.icon
                        km, km_idname = cls._km_action_simple(kc, context_mode, text, keymap_data)
                        cls._tool_keymap[context_mode, text] = km, km_idname

    def draw(self, context):
        # XXX, this UI isn't very nice.
        # We might need to create new button types for this.
        # Since we probably want:
        # - tool-tips that include multiple key shortcuts.
        # - ability to click and hold to expose sub-tools.

        context_mode = context.mode
        tool_def_active, index_active = self._tool_vars_from_active_with_index(context)
        layout = self.layout

        scale_y = 2.0

        # TODO(campbell): expose ui_scale.
        view2d = context.region.view2d
        ui_scale = (
            view2d.region_to_view(1.0, 0.0)[0] -
            view2d.region_to_view(0.0, 0.0)[0]
        )
        width_scale = context.region.width * ui_scale
        del view2d, ui_scale

        empty_text = ""
        if width_scale > 120.0:
            show_text = True
            use_columns = False
        else:
            show_text = False
            # 2 column layout, disabled
            if width_scale > 80.0:
                column_count = 2
                use_columns = True
                empty_text = " "  # needed for alignment, grr
            else:
                use_columns = False

        # Could support 3x columns.
        column_index = 0

        for tool_items in self.tools_from_context(context):
            if tool_items:
                col = layout.column(align=True)
                if not use_columns:
                    col.scale_y = scale_y
                for item in tool_items:
                    if item is None:
                        col = layout.column(align=True)
                        if not use_columns:
                            col.scale_y = scale_y
                        continue

                    if type(item) is tuple:
                        is_active = False
                        i = 0
                        for i, sub_item in enumerate(item):
                            if sub_item is None:
                                continue
                            tool_def, icon_name = self._tool_vars_from_def(sub_item, context_mode)
                            is_active = (tool_def == tool_def_active)
                            if is_active:
                                index = i
                                break
                        del i, sub_item

                        if is_active:
                            # not ideal, write this every time :S
                            self._tool_group_active[item[0].text] = index
                        else:
                            index = self._tool_group_active.get(item[0].text, 0)

                        item = item[index]
                        use_menu = True
                    else:
                        index = -1
                        use_menu = False

                    tool_def, icon_name = self._tool_vars_from_def(item, context_mode)
                    is_active = (tool_def == tool_def_active)
                    icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle(icon_name)

                    if use_columns:
                        col.scale_y = scale_y
                        if column_index == 0:
                            row = col.row(align=True)
                            row.scale_y = scale_y
                        sub = row
                    else:
                        sub = col

                    if use_menu:
                        props = sub.operator_menu_hold(
                            "wm.tool_set",
                            text=item.text if show_text else empty_text,
                            depress=is_active,
                            menu="WM_MT_toolsystem_submenu",
                            icon_value=icon_value,
                        )
                    else:
                        props = sub.operator(
                            "wm.tool_set",
                            text=item.text if show_text else empty_text,
                            depress=is_active,
                            icon_value=icon_value,
                        )
                    props.keymap = tool_def[0] or ""
                    props.manipulator_group = tool_def[1] or ""
                    props.index = index

                    if use_columns:
                        col.scale_y = 1.0
                        column_index += 1
                        if column_index == column_count:
                            column_index = 0

    def tools_from_context(cls, context):
        return (cls._tools[None], cls._tools.get(context.mode, ()))

    @staticmethod
    def _active_tool(context, with_icon=False):
        """
        Return the active Python tool definition and icon name.
        """

        workspace = context.workspace
        space_type = workspace.tool_space_type
        cls = next(
            (cls for cls in ToolSelectPanelHelper.__subclasses__()
             if cls.bl_space_type == space_type),
            None
        )
        if cls is not None:
            tool_def_active, index_active = ToolSelectPanelHelper._tool_vars_from_active_with_index(context)

            context_mode = context.mode
            for tool_items in cls.tools_from_context(context):
                for item in cls._tools_flatten(tool_items):
                    tool_def, icon_name = cls._tool_vars_from_def(item, context_mode)
                    if (tool_def == tool_def_active):
                        if with_icon:
                            icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle(icon_name)
                        else:
                            icon_value = 0
                        return (item, icon_value)
        return None, 0

    @staticmethod
    def draw_active_tool_header(context, layout):
        item, icon_value = ToolSelectPanelHelper._active_tool(context, with_icon=True)
        if item is None:
            layout.label("No Tool Found")
            return
        # Indent until we have better icon scaling.
        layout.label("      " + item.text, icon_value=icon_value)

        draw_settings = item.draw_settings
        if draw_settings is not None:
            draw_settings(context, layout)


# The purpose of this menu is to be a generic popup to select between tools
# in cases when a single tool allows to select alternative tools.
class WM_MT_toolsystem_submenu(Menu):
    bl_label = ""

    @staticmethod
    def _tool_group_from_button(context):
        context_mode = context.mode
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
                    if type(item_group) is tuple:
                        if index_button < len(item_group):
                            item = item_group[index_button]
                            tool_def, icon_name = cls._tool_vars_from_def(item, context_mode)
                            is_active = (tool_def == tool_def_button)
                            if is_active:
                                return cls, item_group, index_button
        return None, None, -1

    def draw(self, context):
        context_mode = context.mode
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
            tool_def, icon_name = cls._tool_vars_from_def(item, context_mode)
            icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle(icon_name)
            props = layout.operator(
                "wm.tool_set",
                text=item.text,
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
