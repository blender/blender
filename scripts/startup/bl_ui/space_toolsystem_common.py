# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Menu,
)

from bpy.app.translations import pgettext_tip as tip_
from bpy.app.translations import pgettext_iface as iface_

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
    for icon_value in set(_icon_cache.values()):
        if icon_value != 0:
            release(icon_value)
    del release


# (icon_name -> icon_value) map
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
        # Unique tool name (within space & mode context).
        "idname",
        # The name to display in the interface.
        "label",
        # Description (for tool-tip), when not set, use the description of `operator`,
        # may be a string or a `function(context, item, key-map) -> string`.
        "description",
        # The name of the icon to use (found in `release/datafiles/icons`) or None for no icon.
        "icon",
        # An optional cursor to use when this tool is active.
        "cursor",
        # The properties to use for the widget.
        "widget_properties",
        # An optional gizmo group to activate when the tool is set or None for no gizmo.
        "widget",
        # Optional key-map for tool, possible values are:
        #
        # - `None` when the tool doesn't have a key-map.
        #   Also the default value when no key-map value is defined.
        #
        # - A string literal for the key-map name, the key-map items are located in the default key-map.
        #
        # - `()` an empty tuple for a default name.
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
        #   `(operator_id, operator_properties, keymap_item_args)`.
        #
        #   Use this to define the key-map in-line.
        #
        #   Note that this isn't used for Blender's built in tools which use the built-in key-map.
        #   Keep this functionality since it's likely useful for add-on key-maps.
        #
        # Warning: currently `from_dict` this is a list of one item,
        # so internally we can swap the key-map function for the key-map itself.
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
        # Various options, see: `bpy.types.WorkSpaceTool.setup` options argument.
        "options",
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
        "options": None,
        "widget": None,
        "widget_properties": None,
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
            show_tool_icon_always=True,
            tool_key=ToolSelectPanelHelper._tool_key_from_context(context, space_type=self.bl_space_type),
        )


class ToolSelectPanelHelper:
    """
    Generic Class, can be used for any toolbar.

    - keymap_prefix:
      The text prefix for each key-map for this spaces tools.
    - tools_all():
      Generator (context_mode, tools) tuple pairs for all tools defined.
    - tools_from_context(context, mode=None):
      A generator for all tools available in the current context.

    Tool Sequence Structure
    =======================

    Sequences of tools as returned by tools_all() and tools_from_context() are comprised of:

    - A `ToolDef` instance (representing a tool that can be activated).
    - None (a visual separator in the tool list).
    - A tuple of `ToolDef` or None values
      (representing a group of tools that can be selected between using a click-drag action).
      Note that only a single level of nesting is supported (groups cannot contain sub-groups).
    - A callable which takes a single context argument and returns a tuple of values described above.
      When the context is None, all potential tools must be returned.
    """

    @classmethod
    def tools_all(cls):
        """
        Return all tools for this toolbar, this must include all available tools ignoring the current context.
        The value is must be a sequence of (mode, tool_list) pairs, where mode may be object-mode edit-mode etc.
        The mode may be None for tool-bars that don't make use of sub-modes.
        """
        raise Exception("Sub-class {!r} must implement this method!".format(cls))

    @classmethod
    def tools_from_context(cls, context, mode=None):
        """
        Return all tools for the current context,
        this result is used at run-time and may filter out tools to display.
        """
        raise Exception("Sub-class {!r} must implement this method!".format(cls))

    @staticmethod
    def _tool_class_from_space_type(space_type):
        return next(
            (cls for cls in ToolSelectPanelHelper.__subclasses__() if cls.bl_space_type == space_type),
            None,
        )

    @staticmethod
    def _icon_value_from_icon_handle(icon_name):
        import os
        if icon_name is not None:
            assert type(icon_name) is str
            icon_value = _icon_cache.get(icon_name)
            if icon_value is None:
                dirname = bpy.utils.system_resource('DATAFILES', path="icons")
                filepath = os.path.join(dirname, icon_name + ".dat")
                try:
                    icon_value = bpy.app.icons.new_triangles_from_file(filepath)
                except BaseException as ex:
                    if not os.path.exists(filepath):
                        print("Missing icons:", filepath, ex)
                    else:
                        print("Corrupt icon:", filepath, ex)
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
    # usually "tools" is already expanded into `ToolDef`
    # but when registering a tool, this can still be a function
    # (`_tools_flatten` is usually called with `cls.tools_from_context(context)`
    # [that already yields from the function])
    # so if item is still a function (e.g._defs_XXX.generate_from_brushes)
    # seems like we cannot expand here (have no context yet)
    # if we yield None here, this will risk running into duplicate tool bl_idname [in register_tool()]
    # but still better than raising an error to the user.
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

    @staticmethod
    def _tools_flatten_with_dynamic(tools, *, context):
        """
        Expands dynamic items, indices aren't aligned with other flatten functions.
        The context may be None, use as signal to return all items.
        """
        for item_parent in tools:
            if item_parent is None:
                yield None
            for item in item_parent if (type(item_parent) is tuple) else (item_parent,):
                if item is None:
                    yield None
                elif _item_is_fn(item):
                    yield from ToolSelectPanelHelper._tools_flatten_with_dynamic(item(context), context=context)
                else:
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
                        index = cls._tool_group_active_get_from_item(item)
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
                        index = cls._tool_group_active_get_from_item(item)
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
                        index = cls._tool_group_active_get_from_item(item)
                        item = item[index]
                    else:
                        index = -1
                    return (item, index)
                i += 1
        return None, -1

    @classmethod
    def _tool_group_active_get_from_item(cls, item):
        index = cls._tool_group_active.get(item[0].idname, 0)
        # Can happen in the case a group is dynamic.
        #
        # NOTE(Campbell): that in this case it's possible the order could change too,
        # So if we want to support this properly we will need to switch away from using
        # an index and instead use an ID.
        # Currently this is such a rare case occurrence that a range check is OK for now.
        if index >= len(item):
            index = 0
        return index

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
        km_idname = "{:s} {:s}, {:s}".format(cls.keymap_prefix, context_descr, label)
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
    def register_ensure(cls):
        """
        Ensure register has created key-map data, needed when key-map data is needed in background mode.
        """
        if cls._has_keymap_data:
            return
        cls.register()

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
            cls._has_keymap_data = False
            return

        for context_mode, tools in cls.tools_all():
            if context_mode is None:
                context_descr = "All"
            else:
                context_descr = context_mode.replace("_", " ").title()

            for item in cls._tools_flatten_with_dynamic(tools, context=None):
                if item is None:
                    continue
                keymap_data = item.keymap
                if keymap_data is None:
                    continue
                if callable(keymap_data[0]):
                    cls._km_action_simple(kc_default, kc_default, context_descr, item.label, keymap_data)

        cls._has_keymap_data = True

    @classmethod
    def keymap_ui_hierarchy(cls, context_mode):
        # See: bpy_extras.keyconfig_utils

        # Key-maps may be shared, don't show them twice.
        visited = set()

        for context_mode_test, tools in cls.tools_all():
            if context_mode_test == context_mode:
                for item in cls._tools_flatten(tools):
                    if item is None:
                        continue
                    keymap_data = item.keymap
                    if keymap_data is None:
                        continue
                    km_name = keymap_data[0]
                    # print((km.name, cls.bl_space_type, 'WINDOW', []))

                    if km_name in visited:
                        continue
                    visited.add(km_name)

                    yield (km_name, cls.bl_space_type, 'WINDOW', [])
                    # Callable types don't use fall-backs.
                    if isinstance(km_name, str):
                        yield (km_name + " (fallback)", cls.bl_space_type, 'WINDOW', [])

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
                    index = cls._tool_group_active_get_from_item(item)

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
            show_tool_icon_always=False,
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
        # NOTE: we could show `item.text` here but it makes the layout jitter when switching tools.
        # Add some spacing since the icon is currently assuming regular small icon size.
        if show_tool_icon_always:
            layout.label(text="    " + iface_(item.label, "Operator"), icon_value=icon_value)
            layout.separator()
        else:
            if not context.space_data.show_region_toolbar:
                layout.template_icon(icon_value=icon_value, scale=0.5)
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
            row.popover(panel="TOPBAR_PT_tool_fallback", text=iface_(label, "Operator"))

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
            index_current = cls._tool_group_active_get_from_item(item_group)

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
            # Could use a less generic icon.
            icon='TOOL_SETTINGS',
        )
        is_active_tool = (tool_settings.workspace_tool_type == 'DEFAULT')

        if is_active_tool:
            index_current = -1
        else:
            index_current = cls._tool_group_active_get_from_item(item_group)
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

    gizmo_group = item.widget or ""

    idname_fallback = (item_fallback and item_fallback.idname) or ""
    keymap_fallback = (item_fallback and item_fallback.keymap and item_fallback.keymap[0]) or ""
    if keymap_fallback:
        keymap_fallback = keymap_fallback + " (fallback)"

    tool.setup(
        idname=item.idname,
        keymap=item.keymap[0] if item.keymap is not None else "",
        cursor=item.cursor or 'DEFAULT',
        options=item.options or set(),
        gizmo_group=gizmo_group,
        data_block=item.data_block or "",
        operator=item.operator or "",
        index=index,
        idname_fallback=idname_fallback,
        keymap_fallback=keymap_fallback,
    )

    if (
            (gizmo_group != "") and
            (props := tool.gizmo_group_properties(gizmo_group))
    ):
        if props is None:
            print("Error:", gizmo_group, "could not access properties!")
        else:
            gizmo_properties = item.widget_properties
            if gizmo_properties is not None:
                if not isinstance(gizmo_properties, list):
                    raise Exception("expected a list, not a {!r}".format(type(gizmo_properties)))

                from bl_keymap_utils.io import _init_properties_from_data
                _init_properties_from_data(props, gizmo_properties)

    WindowManager = bpy.types.WindowManager

    handle_map = _activate_by_item._cursor_draw_handle
    handle = handle_map.pop(space_type, None)
    if handle is not None:
        WindowManager.draw_cursor_remove(handle)
    if item.draw_cursor is not None:
        def handle_fn(context, item, tool, xy):
            item.draw_cursor(context, tool, xy)
        handle = WindowManager.draw_cursor_add(handle_fn, (context, item, tool), space_type, 'WINDOW')
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
            index_current = cls._tool_group_active_get_from_item(item_group)
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
