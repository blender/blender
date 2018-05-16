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
        if icon_value != 0:
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


def _item_is_fn(item):
    return (not (type(item) is ToolDef) and callable(item))


from collections import namedtuple
ToolDef = namedtuple(
    "ToolDef",
    (
        # The name to display in the interface.
        "text",
        # The name of the icon to use (found in ``release/datafiles/icons``) or None for no icon.
        "icon",
        # An optional manipulator group to activate when the tool is set or None for no widget.
        "widget",
        # Optional keymap for tool, either:
        # - A function that populates a keymaps passed in as an argument.
        # - A tuple filled with triple's of:
        #   ``(operator_id, operator_properties, keymap_item_args)``.
        #
        # Warning: currently 'from_dict' this is a list of one item,
        # so internally we can swap the keymap function for the keymap it's self.
        # This isn't very nice and may change, tool definitions shouldn't care about this.
        "keymap",
        # Optional data-block assosiated with this tool.
        # (Typically brush name, usage depends on mode, we could use for non-brush ID's in other modes).
        "data_block",
        # Optional draw settings (operator options, toolsettings).
        "draw_settings",
    )
)
del namedtuple


def from_dict(kw_args):
    """
    Use so each tool can avoid defining all members of the named tuple.
    Also convert the keymap from a tuple into a function
    (since keymap is a callback).
    """
    kw = {
        "icon": None,
        "widget": None,
        "keymap": None,
        "data_block": None,
        "draw_settings": None,
    }
    kw.update(kw_args)

    keymap = kw["keymap"]
    if kw["keymap"] is None:
        pass
    elif type(keymap) is tuple:
        keymap = [_keymap_fn_from_seq(keymap)]
    else:
        keymap = [keymap]
    kw["keymap"] = keymap
    return ToolDef(**kw)


def from_fn(fn):
    """
    Use as decorator so we can define functions.
    """
    return ToolDef.from_dict(fn())


ToolDef.from_dict = from_dict
ToolDef.from_fn = from_fn
del from_dict
del from_fn


class ToolSelectPanelHelper:
    """
    Generic Class, can be used for any toolbar.

    - keymap_prefix:
      The text prefix for each key-map for this spaces tools.
    - tools_all():
      Returns (context_mode, tools) tuple pair for all tools defined.
    - tools_from_context(context, mode=None):
      Returns tools available in this context.

    Each tool is a 'ToolDef' or None for a separator in the toolbar, use ``None``.
    """

    @staticmethod
    def _tool_class_from_space_type(space_type):
        return next(
            (cls for cls in ToolSelectPanelHelper.__subclasses__()
             if cls.bl_space_type == space_type),
            None
        )

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
                    # Use none as a fallback (avoids layout issues).
                    if icon_name != "none":
                        icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle("none")
                    else:
                        icon_value = 0
                _icon_cache[icon_name] = icon_value
            return icon_value
        else:
            return 0

    @staticmethod
    def _tools_flatten(tools):
        """
        Flattens, skips None and calls generators.
        """
        for item in tools:
            if item is None:
                yield None
            elif type(item) is tuple:
                for sub_item in item:
                    if sub_item is None:
                        yield None
                    elif _item_is_fn(sub_item):
                        yield from sub_item(context)
                    else:
                        yield sub_item
            else:
                if _item_is_fn(item):
                    yield from item(context)
                else:
                    yield item

    @staticmethod
    def _tools_flatten_with_tool_index(tools):
        for item in tools:
            if item is None:
                yield None, -1
            elif type(item) is tuple:
                i = 0
                for sub_item in item:
                    if sub_item is None:
                        yield None
                    elif _item_is_fn(sub_item):
                        for item_dyn in sub_item(context):
                            yield item_dyn, i
                            i += 1
                    else:
                        yield sub_item, i
                        i += 1
            else:
                if _item_is_fn(item):
                    for item_dyn in item(context):
                        yield item_dyn, -1
                else:
                    yield item, -1

    @staticmethod
    def _tool_get_active(context, space_type, mode, with_icon=False):
        """
        Return the active Python tool definition and icon name.
        """

        workspace = context.workspace
        cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
        if cls is not None:
            tool_active_text = getattr(
                ToolSelectPanelHelper._tool_active_from_context(context, space_type, mode),
                "name", None)

            for item in ToolSelectPanelHelper._tools_flatten(cls.tools_from_context(context, mode)):
                if item is not None:
                    if item.text == tool_active_text:
                        if with_icon:
                            icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle(item.icon)
                        else:
                            icon_value = 0
                        return (item, icon_value)
        return None, 0

    @staticmethod
    def _tool_get_by_name(context, space_type, text):
        """
        Return the active Python tool definition and index (if in sub-group, else -1).
        """
        cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
        if cls is not None:
            for item, index in ToolSelectPanelHelper._tools_flatten_with_tool_index(cls.tools_from_context(context)):
                if item is not None:
                    if item.text == text:
                        return (item, index)
        return None, -1

    @staticmethod
    def _tool_vars_from_def(item):
        # For now be strict about whats in this dict
        # prevent accidental adding unknown keys.
        text = item.text
        icon_name = item.icon
        mp_idname = item.widget
        datablock_idname = item.data_block
        keymap = item.keymap
        if keymap is None:
            km_idname = None
        else:
            km_idname = keymap[0].name
        return (km_idname, mp_idname, datablock_idname), icon_name

    @staticmethod
    def _tool_active_from_context(context, space_type, mode=None, create=False):
        if space_type == 'VIEW_3D':
            if mode is None:
                obj = context.active_object
                mode = obj.mode if obj is not None else 'OBJECT'
            tool = context.workspace.tools.from_space_view3d_mode(mode, create)
            if tool is not None:
                return tool
        elif space_type == 'IMAGE_EDITOR':
            space_data = context.space_data
            if mode is None:
                mode = space_data.mode
            tool = context.workspace.tools.from_space_image_mode(mode, create)
            if tool is not None:
                return tool
        return None

    @staticmethod
    def _tool_text_from_button(context):
        return context.button_operator.name

    @classmethod
    def _km_action_simple(cls, kc, context_mode, text, keymap_fn):
        if context_mode is None:
            context_mode = "All"
        km_idname = f"{cls.keymap_prefix} {context_mode}, {text}"
        km = kc.keymaps.get(km_idname)
        if km is None:
            km = kc.keymaps.new(km_idname, space_type=cls.bl_space_type, region_type='WINDOW')
            keymap_fn[0](km)
        keymap_fn[0] = km

    @classmethod
    def register(cls):
        wm = bpy.context.window_manager

        # XXX, should we be manipulating the user-keyconfig on load?
        # Perhaps this should only add when keymap items don't already exist.
        #
        # This needs some careful consideration.
        kc = wm.keyconfigs.user

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
                    # skip None or generator function
                    if item is None or _item_is_fn(item):
                        continue
                    keymap_data = item.keymap
                    if keymap_data is not None and callable(keymap_data[0]):
                        text = item.text
                        icon_name = item.icon
                        cls._km_action_simple(kc, context_mode, text, keymap_data)

    # -------------------------------------------------------------------------
    # Layout Generators
    #
    # Meaning of recieved values:
    # - Bool: True for a separator, otherwise False for regular tools.
    # - None: Signal to finish (complete any final operations, e.g. add padding).

    @staticmethod
    def _layout_generator_single_column(layout):
        scale_y = 2.0

        col = layout.column(align=True)
        col.scale_y = scale_y
        is_sep = False
        while True:
            if is_sep is True:
                col = layout.column(align=True)
                col.scale_y = scale_y
            elif is_sep is None:
                yield None
                return
            is_sep = yield col

    @staticmethod
    def _layout_generator_multi_columns(layout, column_count):
        scale_y = 2.0
        scale_x = 2.2
        column_last = column_count - 1

        col = layout.column(align=True)

        row = col.row(align=True)

        row.scale_x = scale_x
        row.scale_y = scale_y

        is_sep = False
        column_index = 0
        while True:
            if is_sep is True:
                if column_index != column_last:
                    row.label("")
                col = layout.column(align=True)
                row = col.row(align=True)
                row.scale_x = scale_x
                row.scale_y = scale_y
                column_index = 0

            is_sep = yield row
            if is_sep is None:
                if column_index == column_last:
                    row.label("")
                    yield None
                    return

            if column_index == column_count:
                column_index = 0
                row = col.row(align=True)
                row.scale_x = scale_x
                row.scale_y = scale_y
            column_index += 1

    @staticmethod
    def _layout_generator_detect_from_region(layout, region):
        """
        Choose an appropriate layout for the toolbar.
        """
        # Currently this just checks the width,
        # we could have different layouts as preferences too.
        system = bpy.context.user_preferences.system
        view2d = region.view2d
        view2d_scale = (
            view2d.region_to_view(1.0, 0.0)[0] -
            view2d.region_to_view(0.0, 0.0)[0]
        )
        width_scale = region.width * view2d_scale / system.ui_scale

        if width_scale > 120.0:
            show_text = True
            column_count = 1
        else:
            show_text = False
            # 2 column layout, disabled
            if width_scale > 80.0:
                column_count = 2
                use_columns = True
            else:
                column_count = 1

        if column_count == 1:
            ui_gen = ToolSelectPanelHelper._layout_generator_single_column(layout)
        else:
            ui_gen = ToolSelectPanelHelper._layout_generator_multi_columns(layout, column_count=column_count)

        return ui_gen, show_text

    def draw(self, context):
        # XXX, this UI isn't very nice.
        # We might need to create new button types for this.
        # Since we probably want:
        # - tool-tips that include multiple key shortcuts.
        # - ability to click and hold to expose sub-tools.

        space_type = context.space_data.type
        tool_active_text = getattr(
            ToolSelectPanelHelper._tool_active_from_context(context, space_type),
            "name", None,
        )

        ui_gen, show_text = self._layout_generator_detect_from_region(self.layout, context.region)

        # Start iteration
        ui_gen.send(None)

        for item in self.tools_from_context(context):
            if item is None:
                ui_gen.send(True)
                continue

            if type(item) is tuple:
                is_active = False
                i = 0
                for i, sub_item in enumerate(item):
                    if sub_item is None:
                        continue
                    is_active = (sub_item.text == tool_active_text)
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

            tool_def, icon_name = ToolSelectPanelHelper._tool_vars_from_def(item)
            is_active = (item.text == tool_active_text)

            icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle(icon_name)

            sub = ui_gen.send(False)

            if use_menu:
                sub.operator_menu_hold(
                    "wm.tool_set_by_name",
                    text=item.text if show_text else "",
                    depress=is_active,
                    menu="WM_MT_toolsystem_submenu",
                    icon_value=icon_value,
                ).name = item.text
            else:
                sub.operator(
                    "wm.tool_set_by_name",
                    text=item.text if show_text else "",
                    depress=is_active,
                    icon_value=icon_value,
                ).name = item.text
        # Signal to finish any remaining layout edits.
        ui_gen.send(None)

    @staticmethod
    def draw_active_tool_header(context, layout):
        # BAD DESIGN WARNING: last used tool
        workspace = context.workspace
        space_type = workspace.tools_space_type
        mode = workspace.tools_mode
        item, icon_value = ToolSelectPanelHelper._tool_get_active(context, space_type, mode, with_icon=True)
        if item is None:
            return
        # Note: we could show 'item.text' here but it makes the layout jitter when switcuing tools.
        layout.label(" ", icon_value=icon_value)
        draw_settings = item.draw_settings
        if draw_settings is not None:
            draw_settings(context, layout)


# The purpose of this menu is to be a generic popup to select between tools
# in cases when a single tool allows to select alternative tools.
class WM_MT_toolsystem_submenu(Menu):
    bl_label = ""

    @staticmethod
    def _tool_group_from_button(context):
        # Lookup the tool definitions based on the space-type.
        cls = ToolSelectPanelHelper._tool_class_from_space_type(context.space_data.type)
        if cls is not None:
            button_text = ToolSelectPanelHelper._tool_text_from_button(context)
            for item_group in cls.tools_from_context(context):
                if type(item_group) is tuple:
                    for sub_item in item_group:
                        if sub_item.text == button_text:
                            return cls, item_group
        return None, None

    def draw(self, context):
        layout = self.layout
        layout.scale_y = 2.0

        cls, item_group = self._tool_group_from_button(context)
        if item_group is None:
            # Should never happen, just in case
            layout.label("Unable to find toolbar group")
            return

        for item in item_group:
            if item is None:
                layout.separator()
                continue
            tool_def, icon_name = ToolSelectPanelHelper._tool_vars_from_def(item)
            icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle(icon_name)
            layout.operator(
                "wm.tool_set_by_name",
                text=item.text,
                icon_value=icon_value,
            ).name = item.text


def activate_by_name(context, space_type, text):
    item, index = ToolSelectPanelHelper._tool_get_by_name(context, space_type, text)
    if item is not None:
        tool = ToolSelectPanelHelper._tool_active_from_context(context, space_type, create=True)
        tool_def, icon_name = ToolSelectPanelHelper._tool_vars_from_def(item)
        tool.setup(
            name=text,
            keymap=tool_def[0] or "",
            manipulator_group=tool_def[1] or "",
            data_block=tool_def[2] or "",
            index=index,
        )
        return True
    return False


classes = (
    WM_MT_toolsystem_submenu,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
