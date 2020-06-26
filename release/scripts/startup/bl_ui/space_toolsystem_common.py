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

from bpy.app.translations import pgettext_tip as tip_

__all__ = (
    "ToolDef",
    "ToolSelectPanelHelper",
    "activate_by_id",
    "activate_by_id_or_cycle",
    "description_from_id",
    "keymap_from_id",
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

    def keymap_fn(km):
        if keymap_fn.keymap_data:
            from bl_keymap_utils.io import keymap_init_from_data
            keymap_init_from_data(km, keymap_fn.keymap_data)
    keymap_fn.keymap_data = keymap_data
    return keymap_fn


def _item_is_fn(item):
    return (not (type(item) is ToolDef) and callable(item))


from collections import namedtuple
ToolDef = namedtuple(
    "ToolDef",
    (
        # Unique tool name (withing space & mode context).
        "idname",
        # The name to display in the interface.
        "label",
        # Description (for tool-tip), when not set, use the description of 'operator',
        # may be a string or a 'function(context, item, key-map) -> string'.
        "description",
        # The name of the icon to use (found in ``release/datafiles/icons``) or None for no icon.
        "icon",
        # An optional cursor to use when this tool is active.
        "cursor",
        # An optional gizmo group to activate when the tool is set or None for no gizmo.
        "widget",
        # Optional key-map for tool, possible values are:
        #
        # - ``None`` when the tool doesn't have a key-map.
        #   Also the default value when no key-map value is defined.
        #
        # - A string literal for the key-map name, the key-map items are located in the default key-map.
        #
        # - ``()`` an empty tuple for a default name.
        #   This is convenience functionality for generating a key-map name.
        #   So if a tool name is "Bone Size", in "Edit Armature" mode for the "3D View",
        #   All of these values are combined into an id, e.g:
        #     "3D View Tool: Edit Armature, Bone Envelope"
        #
        #   Typically searching for a string ending with the tool name
        #   in the default key-map will lead you to the key-map for a tool.
        #
        # - A function that populates a key-maps passed in as an argument.
        #
        # - A tuple filled with triple's of:
        #   ``(operator_id, operator_properties, keymap_item_args)``.
        #
        #   Use this to define the key-map in-line.
        #
        #   Note that this isn't used for Blender's built in tools which use the built-in key-map.
        #   Keep this functionality since it's likely useful for add-on key-maps.
        #
        # Warning: currently 'from_dict' this is a list of one item,
        # so internally we can swap the key-map function for the key-map it's self.
        # This isn't very nice and may change, tool definitions shouldn't care about this.
        "keymap",
        # Optional data-block associated with this tool.
        # (Typically brush name, usage depends on mode, we could use for non-brush ID's in other modes).
        "data_block",
        # Optional primary operator (for introspection only).
        "operator",
        # Optional draw settings (operator options, tool_settings).
        "draw_settings",
        # Optional draw cursor.
        "draw_cursor",
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
        "description": None,
        "icon": None,
        "cursor": None,
        "widget": None,
        "keymap": None,
        "data_block": None,
        "operator": None,
        "draw_settings": None,
        "draw_cursor": None,
    }
    kw.update(kw_args)

    keymap = kw["keymap"]
    if keymap is None:
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


def with_args(**kw):
    def from_fn(fn):
        return ToolDef.from_dict(fn(**kw))
    return from_fn


from_fn.with_args = with_args
ToolDef.from_dict = from_dict
ToolDef.from_fn = from_fn
del from_dict, from_fn, with_args


class ToolActivePanelHelper:
    # Sub-class must define.
    # bl_space_type = 'VIEW_3D'
    # bl_region_type = 'UI'
    bl_label = "Active Tool"
    # bl_category = "Tool"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        ToolSelectPanelHelper.draw_active_tool_header(
            context,
            layout.column(),
            show_tool_name=True,
            tool_key=ToolSelectPanelHelper._tool_key_from_context(context, space_type=self.bl_space_type),
        )


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
                dirname = bpy.utils.system_resource('DATAFILES', "icons")
                filename = os.path.join(dirname, icon_name + ".dat")
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

    # tool flattening
    #
    # usually 'tools' is already expanded into ToolDef
    # but when registering a tool, this can still be a function
    # (_tools_flatten is usually called with cls.tools_from_context(context)
    # [that already yields from the function])
    # so if item is still a function (e.g._defs_XXX.generate_from_brushes)
    # seems like we cannot expand here (have no context yet)
    # if we yield None here, this will risk running into duplicate tool bl_idname [in register_tool()]
    # but still better than erroring out
    @staticmethod
    def _tools_flatten(tools):
        for item_parent in tools:
            if item_parent is None:
                yield None
            for item in item_parent if (type(item_parent) is tuple) else (item_parent,):
                if item is None or _item_is_fn(item):
                    yield None
                else:
                    yield item

    @staticmethod
    def _tools_flatten_with_tool_index(tools):
        for item_parent in tools:
            if item_parent is None:
                yield None, -1
            i = 0
            for item in item_parent if (type(item_parent) is tuple) else (item_parent,):
                if item is None or _item_is_fn(item):
                    yield None, -1
                else:
                    yield item, i
                    i += 1

    # Special internal function, gives use items that contain keymaps.
    @staticmethod
    def _tools_flatten_with_keymap(tools):
        for item_parent in tools:
            if item_parent is None:
                continue
            for item in item_parent if (type(item_parent) is tuple) else (item_parent,):
                # skip None or generator function
                if item is None or _item_is_fn(item):
                    continue
                if item.keymap is not None:
                    yield item

    @classmethod
    def _tool_get_active(cls, context, space_type, mode, with_icon=False):
        """
        Return the active Python tool definition and icon name.
        """
        tool_active = ToolSelectPanelHelper._tool_active_from_context(context, space_type, mode)
        tool_active_id = getattr(tool_active, "idname", None)
        for item in ToolSelectPanelHelper._tools_flatten(cls.tools_from_context(context, mode)):
            if item is not None:
                if item.idname == tool_active_id:
                    if with_icon:
                        icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle(item.icon)
                    else:
                        icon_value = 0
                    return (item, tool_active, icon_value)
        return None, None, 0

    @classmethod
    def _tool_get_by_id(cls, context, idname):
        """
        Return the active Python tool definition and index (if in sub-group, else -1).
        """
        for item, index in ToolSelectPanelHelper._tools_flatten_with_tool_index(cls.tools_from_context(context)):
            if item is not None:
                if item.idname == idname:
                    return (item, index)
        return None, -1

    @classmethod
    def _tool_get_by_id_active(cls, context, idname):
        """
        Return the active Python tool definition and index (if in sub-group, else -1).
        """
        for item in cls.tools_from_context(context):
            if item is not None:
                if type(item) is tuple:
                    if item[0].idname == idname:
                        index = cls._tool_group_active.get(item[0].idname, 0)
                        return (item[index], index)
                else:
                    if item.idname == idname:
                        return (item, -1)
        return None, -1

    @classmethod
    def _tool_get_by_id_active_with_group(cls, context, idname):
        """
        Return the active Python tool definition and index (if in sub-group, else -1).
        """
        for item in cls.tools_from_context(context):
            if item is not None:
                if type(item) is tuple:
                    if item[0].idname == idname:
                        index = cls._tool_group_active.get(item[0].idname, 0)
                        return (item[index], index, item)
                else:
                    if item.idname == idname:
                        return (item, -1, None)
        return None, -1, None

    @classmethod
    def _tool_get_group_by_id(cls, context, idname, *, coerce=False):
        """
        Return the group which contains idname, or None.
        """
        for item in cls.tools_from_context(context):
            if item is not None:
                if type(item) is tuple:
                    for subitem in item:
                        if subitem.idname == idname:
                            return item
                else:
                    if item.idname == idname:
                        if coerce:
                            return (item,)
                        else:
                            return None
        return None

    @classmethod
    def _tool_get_by_flat_index(cls, context, tool_index):
        """
        Return the active Python tool definition and index (if in sub-group, else -1).

        Return the index of the expanded list.
        """
        i = 0
        for item, index in ToolSelectPanelHelper._tools_flatten_with_tool_index(cls.tools_from_context(context)):
            if item is not None:
                if i == tool_index:
                    return (item, index)
                i += 1
        return None, -1

    @classmethod
    def _tool_get_active_by_index(cls, context, tool_index):
        """
        Return the active Python tool definition and index (if in sub-group, else -1).

        Return the index of the list without expanding.
        """
        i = 0
        for item in cls.tools_from_context(context):
            if item is not None:
                if i == tool_index:
                    if type(item) is tuple:
                        index = cls._tool_group_active.get(item[0].idname, 0)
                        item = item[index]
                    else:
                        index = -1
                    return (item, index)
                i += 1
        return None, -1

    @classmethod
    def _tool_group_active_set_by_id(cls, context, idname_group, idname):
        item_group = cls._tool_get_group_by_id(context, idname_group, coerce=True)
        if item_group:
            for i, item in enumerate(item_group):
                if item and item.idname == idname:
                    cls._tool_group_active[item_group[0].idname] = i
                    return True
        return False

    @staticmethod
    def _tool_active_from_context(context, space_type, mode=None, create=False):
        if space_type in {'VIEW_3D', 'PROPERTIES'}:
            if mode is None:
                mode = context.mode
            tool = context.workspace.tools.from_space_view3d_mode(mode, create=create)
            if tool is not None:
                tool.refresh_from_context()
                return tool
        elif space_type == 'IMAGE_EDITOR':
            space_data = context.space_data
            if mode is None:
                if space_data is None:
                    mode = 'VIEW'
                else:
                    mode = space_data.mode
            tool = context.workspace.tools.from_space_image_mode(mode, create=create)
            if tool is not None:
                tool.refresh_from_context()
                return tool
        elif space_type == 'NODE_EDITOR':
            space_data = context.space_data
            tool = context.workspace.tools.from_space_node(create=create)
            if tool is not None:
                tool.refresh_from_context()
                return tool
        elif space_type == 'SEQUENCE_EDITOR':
            space_data = context.space_data
            if mode is None:
                mode = space_data.view_type
            tool = context.workspace.tools.from_space_sequencer(mode, create=create)
            if tool is not None:
                tool.refresh_from_context()
                return tool
        return None

    @staticmethod
    def _tool_identifier_from_button(context):
        return context.button_operator.name

    @classmethod
    def _km_action_simple(cls, kc_default, kc, context_descr, label, keymap_fn):
        km_idname = "%s %s, %s" % (cls.keymap_prefix, context_descr, label)
        km = kc.keymaps.get(km_idname)
        km_kwargs = dict(space_type=cls.bl_space_type, region_type='WINDOW', tool=True)
        if km is None:
            km = kc.keymaps.new(km_idname, **km_kwargs)
            keymap_fn[0](km)
        keymap_fn[0] = km.name

        # Ensure we have a default key map, so the add-ons keymap is properly overlayed.
        if kc_default is not kc:
            kc_default.keymaps.new(km_idname, **km_kwargs)

    @classmethod
    def register(cls):
        wm = bpy.context.window_manager
        # Write into defaults, users may modify in preferences.
        kc_default = wm.keyconfigs.default

        # Track which tool-group was last used for non-active groups.
        # Blender stores the active tool-group index.
        #
        # {tool_name_first: index_in_group, ...}
        cls._tool_group_active = {}

        # ignore in background mode
        if kc_default is None:
            return

        for context_mode, tools in cls.tools_all():
            if context_mode is None:
                context_descr = "All"
            else:
                context_descr = context_mode.replace("_", " ").title()

            for item in cls._tools_flatten_with_keymap(tools):
                keymap_data = item.keymap
                if callable(keymap_data[0]):
                    cls._km_action_simple(kc_default, kc_default, context_descr, item.label, keymap_data)

    @classmethod
    def keymap_ui_hierarchy(cls, context_mode):
        # See: bpy_extras.keyconfig_utils

        # Keymaps may be shared, don't show them twice.
        visited = set()

        for context_mode_test, tools in cls.tools_all():
            if context_mode_test == context_mode:
                for item in cls._tools_flatten_with_keymap(tools):
                    km_name = item.keymap[0]
                    # print((km.name, cls.bl_space_type, 'WINDOW', []))

                    if km_name in visited:
                        continue
                    visited.add(km_name)

                    yield (km_name, cls.bl_space_type, 'WINDOW', [])

    # -------------------------------------------------------------------------
    # Layout Generators
    #
    # Meaning of received values:
    # - Bool: True for a separator, otherwise False for regular tools.
    # - None: Signal to finish (complete any final operations, e.g. add padding).

    @staticmethod
    def _layout_generator_single_column(layout, scale_y):
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
    def _layout_generator_multi_columns(layout, column_count, scale_y):
        scale_x = scale_y * 1.1
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
                    row.label(text="")
                col = layout.column(align=True)
                row = col.row(align=True)
                row.scale_x = scale_x
                row.scale_y = scale_y
                column_index = 0

            is_sep = yield row
            if is_sep is None:
                if column_index == column_last:
                    row.label(text="")
                    yield None
                    return

            if column_index == column_count:
                column_index = 0
                row = col.row(align=True)
                row.scale_x = scale_x
                row.scale_y = scale_y
            column_index += 1

    @staticmethod
    def _layout_generator_detect_from_region(layout, region, scale_y):
        """
        Choose an appropriate layout for the toolbar.
        """
        # Currently this just checks the width,
        # we could have different layouts as preferences too.
        system = bpy.context.preferences.system
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
            else:
                column_count = 1

        if column_count == 1:
            ui_gen = ToolSelectPanelHelper._layout_generator_single_column(
                layout, scale_y=scale_y,
            )
        else:
            ui_gen = ToolSelectPanelHelper._layout_generator_multi_columns(
                layout, column_count=column_count, scale_y=scale_y,
            )

        return ui_gen, show_text

    @classmethod
    def draw_cls(cls, layout, context, detect_layout=True, scale_y=1.75):
        # Use a classmethod so it can be called outside of a panel context.

        # XXX, this UI isn't very nice.
        # We might need to create new button types for this.
        # Since we probably want:
        # - tool-tips that include multiple key shortcuts.
        # - ability to click and hold to expose sub-tools.

        space_type = context.space_data.type
        tool_active_id = getattr(
            ToolSelectPanelHelper._tool_active_from_context(context, space_type),
            "idname", None,
        )

        if detect_layout:
            ui_gen, show_text = cls._layout_generator_detect_from_region(layout, context.region, scale_y)
        else:
            ui_gen = ToolSelectPanelHelper._layout_generator_single_column(layout, scale_y)
            show_text = True

        # Start iteration
        ui_gen.send(None)

        for item in cls.tools_from_context(context):
            if item is None:
                ui_gen.send(True)
                continue

            if type(item) is tuple:
                is_active = False
                i = 0
                for i, sub_item in enumerate(item):
                    if sub_item is None:
                        continue
                    is_active = (sub_item.idname == tool_active_id)
                    if is_active:
                        index = i
                        break
                del i, sub_item

                if is_active:
                    # not ideal, write this every time :S
                    cls._tool_group_active[item[0].idname] = index
                else:
                    index = cls._tool_group_active.get(item[0].idname, 0)

                item = item[index]
                use_menu = True
            else:
                index = -1
                use_menu = False

            is_active = (item.idname == tool_active_id)
            icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle(item.icon)

            sub = ui_gen.send(False)

            if use_menu:
                sub.operator_menu_hold(
                    "wm.tool_set_by_id",
                    text=item.label if show_text else "",
                    depress=is_active,
                    menu="WM_MT_toolsystem_submenu",
                    icon_value=icon_value,
                ).name = item.idname
            else:
                sub.operator(
                    "wm.tool_set_by_id",
                    text=item.label if show_text else "",
                    depress=is_active,
                    icon_value=icon_value,
                ).name = item.idname
        # Signal to finish any remaining layout edits.
        ui_gen.send(None)

    def draw(self, context):
        self.draw_cls(self.layout, context)

    @staticmethod
    def _tool_key_from_context(context, *, space_type=None):
        if space_type is None:
            space_data = context.space_data
            space_type = space_data.type
        else:
            space_data = None

        if space_type == 'VIEW_3D':
            return space_type, context.mode
        elif space_type == 'IMAGE_EDITOR':
            if space_data is None:
                space_data = context.space_data
            return space_type, space_data.mode
        elif space_type == 'NODE_EDITOR':
            return space_type, None
        elif space_type == 'SEQUENCE_EDITOR':
            return space_type, context.space_data.view_type
        else:
            return None, None

    @staticmethod
    def tool_active_from_context(context):
        space_type = context.space_data.type
        return ToolSelectPanelHelper._tool_active_from_context(context, space_type)

    @staticmethod
    def draw_active_tool_fallback(
            context, layout, tool,
            *,
            is_horizontal_layout=False,
    ):
        idname_fallback = tool.idname_fallback
        space_type = tool.space_type
        cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
        item_fallback, _index = cls._tool_get_by_id(context, idname_fallback)
        if item_fallback is not None:
            draw_settings = item_fallback.draw_settings
            if draw_settings is not None:
                if not is_horizontal_layout:
                    layout.separator()
                draw_settings(context, layout, tool)

    @staticmethod
    def draw_active_tool_header(
            context, layout,
            *,
            show_tool_name=False,
            tool_key=None,
    ):
        if tool_key is None:
            space_type, mode = ToolSelectPanelHelper._tool_key_from_context(context)
        else:
            space_type, mode = tool_key

        if space_type is None:
            return None

        cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
        item, tool, icon_value = cls._tool_get_active(context, space_type, mode, with_icon=True)
        if item is None:
            return None
        # Note: we could show 'item.text' here but it makes the layout jitter when switching tools.
        # Add some spacing since the icon is currently assuming regular small icon size.
        layout.label(text="    " + item.label if show_tool_name else " ", icon_value=icon_value)
        if show_tool_name:
            layout.separator()
        draw_settings = item.draw_settings
        if draw_settings is not None:
            draw_settings(context, layout, tool)

        idname_fallback = tool.idname_fallback
        if idname_fallback and idname_fallback != item.idname:
            tool_settings = context.tool_settings

            # Show popover which looks like an enum but isn't one.
            if tool_settings.workspace_tool_type == 'FALLBACK':
                tool_fallback_id = cls.tool_fallback_id
                item, _select_index = cls._tool_get_by_id_active(context, tool_fallback_id)
                label = item.label
            else:
                label = "Active Tool"

            split = layout.split(factor=0.33)
            row = split.row()
            row.alignment = 'RIGHT'
            row.label(text="Drag:")
            row = split.row()
            row.context_pointer_set("tool", tool)
            row.popover(panel="TOPBAR_PT_tool_fallback", text=label)

        return tool

    # Show a list of tools in the popover.
    @staticmethod
    def draw_fallback_tool_items(layout, context):
        space_type = context.space_data.type
        if space_type == 'PROPERTIES':
            space_type = 'VIEW_3D'

        cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
        tool_fallback_id = cls.tool_fallback_id

        _item, _select_index, item_group = cls._tool_get_by_id_active_with_group(context, tool_fallback_id)

        if item_group is None:
            # Could print comprehensive message - listing available items.
            raise Exception("Fallback tool doesn't exist")

        col = layout.column(align=True)
        tool_settings = context.tool_settings
        col.prop_enum(
            tool_settings,
            "workspace_tool_type",
            value='DEFAULT',
            text="Active Tool",
        )
        is_active_tool = (tool_settings.workspace_tool_type == 'DEFAULT')

        col = layout.column(align=True)
        if is_active_tool:
            index_current = -1
        else:
            index_current = cls._tool_group_active.get(item_group[0].idname, 0)
        for i, sub_item in enumerate(item_group):
            is_active = (i == index_current)

            props = col.operator(
                "wm.tool_set_by_id",
                text=sub_item.label,
                depress=is_active,
            )
            props.name = sub_item.idname
            props.as_fallback = True
            props.space_type = space_type

    @staticmethod
    def draw_fallback_tool_items_for_pie_menu(layout, context):
        space_type = context.space_data.type
        if space_type == 'PROPERTIES':
            space_type = 'VIEW_3D'

        cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
        tool_fallback_id = cls.tool_fallback_id

        _item, _select_index, item_group = cls._tool_get_by_id_active_with_group(context, tool_fallback_id)

        if item_group is None:
            # Could print comprehensive message - listing available items.
            raise Exception("Fallback tool doesn't exist")

        # Allow changing the active tool,
        # even though this isn't the purpose of the pie menu
        # it's confusing from a user perspective if we don't allow it.
        is_fallback_group_active = getattr(
            ToolSelectPanelHelper._tool_active_from_context(context, space_type),
            "idname", None,
        ) in (item.idname for item in item_group)

        pie = layout.menu_pie()
        tool_settings = context.tool_settings
        pie.prop_enum(
            tool_settings,
            "workspace_tool_type",
            value='DEFAULT',
            text="Active Tool",
            icon='TOOL_SETTINGS',  # Could use a less generic icon.
        )
        is_active_tool = (tool_settings.workspace_tool_type == 'DEFAULT')

        if is_active_tool:
            index_current = -1
        else:
            index_current = cls._tool_group_active.get(item_group[0].idname, 0)
        for i, sub_item in enumerate(item_group):
            is_active = (i == index_current)
            props = pie.operator(
                "wm.tool_set_by_id",
                text=sub_item.label,
                depress=is_active,
                icon_value=ToolSelectPanelHelper._icon_value_from_icon_handle(sub_item.icon),
            )
            props.name = sub_item.idname
            props.space_type = space_type
            if not is_fallback_group_active:
                props.as_fallback = True


# The purpose of this menu is to be a generic popup to select between tools
# in cases when a single tool allows to select alternative tools.
class WM_MT_toolsystem_submenu(Menu):
    bl_label = ""

    @staticmethod
    def _tool_group_from_button(context):
        # Lookup the tool definitions based on the space-type.
        cls = ToolSelectPanelHelper._tool_class_from_space_type(context.space_data.type)
        if cls is not None:
            button_identifier = ToolSelectPanelHelper._tool_identifier_from_button(context)
            for item_group in cls.tools_from_context(context):
                if type(item_group) is tuple:
                    for sub_item in item_group:
                        if (sub_item is not None) and (sub_item.idname == button_identifier):
                            return cls, item_group
        return None, None

    def draw(self, context):
        layout = self.layout
        layout.scale_y = 2.0

        _cls, item_group = self._tool_group_from_button(context)
        if item_group is None:
            # Should never happen, just in case
            layout.label(text="Unable to find toolbar group")
            return

        for item in item_group:
            if item is None:
                layout.separator()
                continue
            icon_value = ToolSelectPanelHelper._icon_value_from_icon_handle(item.icon)
            layout.operator(
                "wm.tool_set_by_id",
                text=item.label,
                icon_value=icon_value,
            ).name = item.idname


def _activate_by_item(context, space_type, item, index, *, as_fallback=False):
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    tool = ToolSelectPanelHelper._tool_active_from_context(context, space_type, create=True)
    tool_fallback_id = cls.tool_fallback_id

    if as_fallback:
        # To avoid complicating logic too much, isolate all fallback logic to this block.
        # This will set the tool again, using the item for the fallback instead of the primary tool.
        #
        # If this ends up needing to be more complicated,
        # it would be better to split it into a separate function.

        _item, _select_index, item_group = cls._tool_get_by_id_active_with_group(context, tool_fallback_id)

        if item_group is None:
            # Could print comprehensive message - listing available items.
            raise Exception("Fallback tool doesn't exist")
        index_new = -1
        for i, sub_item in enumerate(item_group):
            if sub_item.idname == item.idname:
                index_new = i
                break
        if index_new == -1:
            raise Exception("Fallback tool not found in group")

        cls._tool_group_active[tool_fallback_id] = index_new

        # Done, now get the current tool to replace the item & index.
        tool_active = ToolSelectPanelHelper._tool_active_from_context(context, space_type)
        item, index = cls._tool_get_by_id(context, getattr(tool_active, "idname", None))
    else:
        # Ensure the active fallback tool is read from saved state (even if the fallback tool is not in use).
        stored_idname_fallback = tool.idname_fallback
        if stored_idname_fallback:
            cls._tool_group_active_set_by_id(context, tool_fallback_id, stored_idname_fallback)
        del stored_idname_fallback

    # Find fallback keymap.
    item_fallback = None
    _item, select_index = cls._tool_get_by_id(context, tool_fallback_id)
    if select_index != -1:
        item_fallback, _index = cls._tool_get_active_by_index(context, select_index)
    # End calculating fallback.

    tool.setup(
        idname=item.idname,
        keymap=item.keymap[0] if item.keymap is not None else "",
        cursor=item.cursor or 'DEFAULT',
        gizmo_group=item.widget or "",
        data_block=item.data_block or "",
        operator=item.operator or "",
        index=index,
        idname_fallback=(item_fallback and item_fallback.idname) or "",
        keymap_fallback=(item_fallback and item_fallback.keymap and item_fallback.keymap[0]) or "",
    )

    WindowManager = bpy.types.WindowManager

    handle_map = _activate_by_item._cursor_draw_handle
    handle = handle_map.pop(space_type, None)
    if handle is not None:
        WindowManager.draw_cursor_remove(handle)
    if item.draw_cursor is not None:
        def handle_fn(context, item, tool, xy):
            item.draw_cursor(context, tool, xy)
        handle = WindowManager.draw_cursor_add(handle_fn, (context, item, tool), space_type)
        handle_map[space_type] = handle


_activate_by_item._cursor_draw_handle = {}


def activate_by_id(context, space_type, idname, *, as_fallback=False):
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    if cls is None:
        return False
    item, index = cls._tool_get_by_id(context, idname)
    if item is None:
        return False
    _activate_by_item(context, space_type, item, index, as_fallback=as_fallback)
    return True


def activate_by_id_or_cycle(context, space_type, idname, *, offset=1, as_fallback=False):

    # Only cycle when the active tool is activated again.
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    item, _index = cls._tool_get_by_id(context, idname)
    if item is None:
        return False

    tool_active = ToolSelectPanelHelper._tool_active_from_context(context, space_type)
    id_active = getattr(tool_active, "idname", None)

    id_current = ""
    for item_group in cls.tools_from_context(context):
        if type(item_group) is tuple:
            index_current = cls._tool_group_active.get(item_group[0].idname, 0)
            for sub_item in item_group:
                if sub_item.idname == idname:
                    id_current = item_group[index_current].idname
                    break
            if id_current:
                break

    if id_current == "":
        return activate_by_id(context, space_type, idname)
    if id_active != id_current:
        return activate_by_id(context, space_type, id_current)

    index_found = (tool_active.index + offset) % len(item_group)

    cls._tool_group_active[item_group[0].idname] = index_found

    item_found = item_group[index_found]
    _activate_by_item(context, space_type, item_found, index_found)
    return True


def description_from_id(context, space_type, idname, *, use_operator=True):
    # Used directly for tooltips.
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    item, _index = cls._tool_get_by_id(context, idname)
    if item is None:
        return False

    # Custom description.
    description = item.description
    if description is not None:
        if callable(description):
            km = _keymap_from_item(context, item)
            return description(context, item, km)
        return tip_(description)

    # Extract from the operator.
    if use_operator:
        operator = item.operator
        if operator is None:
            if item.keymap is not None:
                km = _keymap_from_item(context, item)
                if km is not None:
                    for kmi in km.keymap_items:
                        if kmi.active:
                            operator = kmi.idname
                            break

        if operator is not None:
            import _bpy
            return tip_(_bpy.ops.get_rna_type(operator).description)
    return ""


def item_from_id(context, space_type, idname):
    # Used directly for tooltips.
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    if cls is None:
        return None
    item, _index = cls._tool_get_by_id(context, idname)
    return item


def item_from_id_active(context, space_type, idname):
    # Used directly for tooltips.
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    if cls is None:
        return None
    item, _index = cls._tool_get_by_id_active(context, idname)
    return item


def item_from_id_active_with_group(context, space_type, idname):
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    if cls is None:
        return None
    cls, item, _index = cls._tool_get_by_id_active_with_group(context, idname)
    return item


def item_group_from_id(context, space_type, idname, *, coerce=False):
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    if cls is None:
        return None
    return cls._tool_get_group_by_id(context, idname, coerce=coerce)


def item_from_flat_index(context, space_type, index):
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    if cls is None:
        return None
    item, _index = cls._tool_get_by_flat_index(context, index)
    return item


def item_from_index_active(context, space_type, index):
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    if cls is None:
        return None
    item, _index = cls._tool_get_active_by_index(context, index)
    return item


def keymap_from_id(context, space_type, idname):
    # Used directly for tooltips.
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    if cls is None:
        return None
    item, _index = cls._tool_get_by_id(context, idname)
    if item is None:
        return False

    keymap = item.keymap
    # List container of one.
    if keymap:
        return keymap[0]
    return ""


def _keymap_from_item(context, item):
    if item.keymap is not None:
        wm = context.window_manager
        keyconf = wm.keyconfigs.active
        return keyconf.keymaps.get(item.keymap[0])
    return None


classes = (
    WM_MT_toolsystem_submenu,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
