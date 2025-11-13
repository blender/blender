# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "generate_keymaps",
    "Params",
)

# ------------------------------------------------------------------------------
# Developer Notes
#
# - This script should run without Blender (no references to the `bpy` module for example).
# - All configuration must be passed into the `generate_keymaps` function (via `Params`).
# - Supporting some combinations of options is becoming increasingly complex,
#   especially `Params.select_mouse` & `Params.use_fallback_tool`.
#   To ensure changes don't unintentionally break other configurations, see:
#   `tools/utils/blender_keyconfig_export_permutations.py --help`
#

# ------------------------------------------------------------------------------
# Configurable Parameters


class Params:
    __slots__ = (
        "legacy",
        "select_mouse",
        "select_mouse_value",
        "action_mouse",
        "tool_mouse",
        "tool_maybe_tweak_value",
        "context_menu_event",
        "cursor_set_event",
        "cursor_tweak_event",
        # NOTE: this is intended to be used so pressing a button can then drag the current selection.
        # This should not be used for button release values such as `CLICK` or `RELEASE` which should
        # instead be bound to a binding that doesn't de-select all, this way:
        # - Click-drag moves the current selection.
        # - Click selects only the item at the cursor position.
        # See: #97032.
        "use_tweak_select_passthrough",
        "use_mouse_emulate_3_button",

        # User preferences:
        #
        # Swap 'Space/Shift-Space'.
        "spacebar_action",
        # Key toggles selection with 'A'.
        "use_select_all_toggle",
        # Activate gizmo on drag (which support it).
        "use_gizmo_drag",
        # Use the fallback tool instead of tweak for RMB select.
        "use_fallback_tool",
        # Selection actions are already accounted for, no need to add additional selection keys.
        "use_fallback_tool_select_handled",
        # Use pie menu for tab by default (swap 'Tab/Ctrl-Tab').
        "use_v3d_tab_menu",
        # Use extended pie menu for shading.
        "use_v3d_shade_ex_pie",
        # Swap orbit/pan keys (for 2D workflows).
        "use_v3d_mmb_pan",
        # Alt click to access tools.
        "use_alt_click_leader",
        # Transform keys G/S/R activate tools instead of immediately transforming.
        "use_key_activate_tools",
        # Side-bar toggle opens a pie menu instead of immediately toggling the side-bar.
        "use_region_toggle_pie",
        # Optionally use a modifier to access tools.
        "tool_modifier",
        # Experimental option.
        "use_pie_click_drag",
        "v3d_tilde_action",
        # Alt-MMB axis switching 'RELATIVE' or 'ABSOLUTE' axis switching.
        "v3d_alt_mmb_drag_action",
        # Changes some transformers modal key-map items to avoid conflicts with navigation operations.
        "use_alt_navigation",
        # File selector actions on single click.
        "use_file_single_click",

        # Convenience variables:
        # (derived from other settings).
        #
        # The fallback tool is activated on the same button as selection.
        # Shorthand for: `(True if (select_mouse == 'LEFT') else self.use_fallback_tool)`
        "use_fallback_tool_select_mouse",
        # Shorthand for: `(self.select_mouse_value if self.use_fallback_tool_select_handled else 'CLICK')`.
        "select_mouse_value_fallback",
        # Shorthand for: `{"type": params.select_mouse, "value": 'CLICK_DRAG'}`.
        "select_tweak_event",
        # Shorthand for: `('CLICK_DRAG' if params.use_pie_click_drag else 'PRESS')`
        "pie_value",
        # Shorthand for: `{"type": params.tool_mouse, "value": 'CLICK_DRAG'}`.
        "tool_tweak_event",
        # Shorthand for: `{"type": params.tool_mouse, "value": params.tool_maybe_tweak_value}`.
        #
        # NOTE: This is typically used for active tool key-map items however it should never
        # be used for selection tools (the default box-select tool for example).
        # Since this means with RMB select enabled in edit-mode for example
        # `Ctrl-LMB` would be caught by box-select instead of add/extrude.
        "tool_maybe_tweak_event",
    )

    def __init__(
            self,
            *,
            legacy=False,
            select_mouse='RIGHT',
            use_mouse_emulate_3_button=False,

            # User preferences.
            spacebar_action='TOOL',
            use_key_activate_tools=False,
            use_region_toggle_pie=False,
            use_select_all_toggle=False,
            use_gizmo_drag=True,
            use_fallback_tool=False,
            use_fallback_tool_select_handled=True,
            use_v3d_tab_menu=False,
            use_v3d_shade_ex_pie=False,
            use_v3d_mmb_pan=False,
            use_alt_tool_or_cursor=False,
            use_alt_click_leader=False,
            use_pie_click_drag=False,
            use_alt_navigation=True,
            use_file_single_click=False,
            v3d_tilde_action='VIEW',
            v3d_alt_mmb_drag_action='RELATIVE',
    ):
        self.legacy = legacy

        if use_mouse_emulate_3_button:
            assert use_alt_tool_or_cursor is False

        if select_mouse == 'RIGHT':
            # Right mouse select.
            self.select_mouse = 'RIGHTMOUSE'
            self.select_mouse_value = 'PRESS'
            self.action_mouse = 'LEFTMOUSE'
            self.tool_mouse = 'LEFTMOUSE'
            if use_alt_tool_or_cursor:
                self.tool_maybe_tweak_value = 'PRESS'
            else:
                self.tool_maybe_tweak_value = 'CLICK_DRAG'

            self.context_menu_event = {"type": 'W', "value": 'PRESS'}

            # Use the "cursor" functionality for RMB select.
            if use_alt_tool_or_cursor:
                self.cursor_set_event = {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}
                self.cursor_tweak_event = {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "alt": True}
            else:
                self.cursor_set_event = {"type": 'LEFTMOUSE', "value": 'CLICK'}
                self.cursor_tweak_event = None

            self.tool_modifier = {}
        else:
            # Left mouse select uses Click event for selection. This is a little
            # less immediate, but is needed to distinguish between click and tweak
            # events on the same mouse buttons.
            self.select_mouse = 'LEFTMOUSE'
            self.select_mouse_value = 'CLICK'
            self.action_mouse = 'RIGHTMOUSE'
            self.tool_mouse = 'LEFTMOUSE'
            self.tool_maybe_tweak_value = 'CLICK_DRAG'

            if self.legacy:
                self.context_menu_event = {"type": 'W', "value": 'PRESS'}
            else:
                self.context_menu_event = {"type": 'RIGHTMOUSE', "value": 'PRESS'}

            self.cursor_set_event = {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True}
            self.cursor_tweak_event = {"type": 'RIGHTMOUSE', "value": 'CLICK_DRAG', "shift": True}

            # Use the "tool" functionality for LMB select.
            if use_alt_tool_or_cursor:
                # Allow `Alt` to be pressed or not.
                self.tool_modifier = {"alt": -1}
            else:
                self.tool_modifier = {}

        self.use_mouse_emulate_3_button = use_mouse_emulate_3_button

        # User preferences:
        self.spacebar_action = spacebar_action
        self.use_key_activate_tools = use_key_activate_tools
        self.use_region_toggle_pie = use_region_toggle_pie

        self.use_gizmo_drag = use_gizmo_drag
        self.use_select_all_toggle = use_select_all_toggle
        self.use_v3d_tab_menu = use_v3d_tab_menu
        self.use_v3d_shade_ex_pie = use_v3d_shade_ex_pie
        self.use_v3d_mmb_pan = use_v3d_mmb_pan
        self.v3d_tilde_action = v3d_tilde_action
        self.v3d_alt_mmb_drag_action = v3d_alt_mmb_drag_action

        self.use_alt_click_leader = use_alt_click_leader
        self.use_pie_click_drag = use_pie_click_drag

        self.use_alt_navigation = use_alt_navigation

        self.use_file_single_click = use_file_single_click

        self.use_tweak_select_passthrough = not legacy

        self.use_fallback_tool = use_fallback_tool

        # Convenience variables:
        self.use_fallback_tool_select_handled = (
            True if (select_mouse == 'LEFT') else
            use_fallback_tool_select_handled
        )
        self.use_fallback_tool_select_mouse = (
            True if (select_mouse == 'LEFT') else
            (not self.use_fallback_tool_select_handled)
        )
        self.select_mouse_value_fallback = (
            self.select_mouse_value if self.use_fallback_tool_select_handled else
            'CLICK'
        )
        self.select_tweak_event = {"type": self.select_mouse, "value": 'CLICK_DRAG'}
        self.pie_value = 'CLICK_DRAG' if use_pie_click_drag else 'PRESS'
        self.tool_tweak_event = {"type": self.tool_mouse, "value": 'CLICK_DRAG'}
        self.tool_maybe_tweak_event = {"type": self.tool_mouse, "value": self.tool_maybe_tweak_value}


# ------------------------------------------------------------------------------
# Constants

from math import pi
PI_2 = pi / 2.0

# Physical layout.
NUMBERS_1 = ('ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'SIX', 'SEVEN', 'EIGHT', 'NINE', 'ZERO')
# Numeric order.
NUMBERS_0 = ('ZERO', 'ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'SIX', 'SEVEN', 'EIGHT', 'NINE')

NUMPAD_1 = (
    'NUMPAD_1',
    'NUMPAD_2',
    'NUMPAD_3',
    'NUMPAD_4',
    'NUMPAD_5',
    'NUMPAD_6',
    'NUMPAD_7',
    'NUMPAD_8',
    'NUMPAD_9',
    'NUMPAD_0',
)


# ------------------------------------------------------------------------------
# Generic Utilities

def _fallback_id(text, fallback):
    if fallback:
        return text + " (fallback)"
    return text


def any_except(*args):
    mod = {"ctrl": -1, "alt": -1, "shift": -1, "oskey": -1, "hyper": -1}
    for arg in args:
        del mod[arg]
    return mod


# ------------------------------------------------------------------------------
# Key-map Item Wrappers

def op_menu(menu, kmi_args):
    return ("wm.call_menu", kmi_args, {"properties": [("name", menu)]})


def op_menu_pie(menu, kmi_args):
    return ("wm.call_menu_pie", kmi_args, {"properties": [("name", menu)]})


def op_panel(menu, kmi_args, kmi_data=()):
    return ("wm.call_panel", kmi_args, {"properties": [("name", menu), *kmi_data]})


def _template_asset_shelf_popup(asset_shelf, spacebar_action):
    if spacebar_action == 'SEARCH':
        return []

    if spacebar_action == 'PLAY':
        kmi_args = {"type": 'SPACE', "value": 'PRESS', "shift": True}
    elif spacebar_action == 'TOOL':
        kmi_args = {"type": 'SPACE', "value": 'PRESS'}

    return [("wm.call_asset_shelf_popover", kmi_args, {"properties": [("name", asset_shelf)]})]


def op_tool(tool, kmi_args):
    return ("wm.tool_set_by_id", kmi_args, {"properties": [("name", tool)]})


def op_tool_cycle(tool, kmi_args):
    return ("wm.tool_set_by_id", kmi_args, {"properties": [("name", tool), ("cycle", True)]})


# Utility to select between an operator and a tool,
# without having to duplicate key map item arguments.
def op_tool_optional(op_args, tool_pair, params):
    if params.use_key_activate_tools:
        kmi_args = op_args[1]
        op_tool_fn, tool_id = tool_pair
        return op_tool_fn(tool_id, kmi_args)
    return op_args


# ------------------------------------------------------------------------------
# Keymap Templates

def _template_items_context_menu(menu, key_args_primary):
    return [
        op_menu(menu, kmi_args)
        for kmi_args in (key_args_primary, {"type": 'APP', "value": 'PRESS'})
    ]


def _template_items_context_panel(menu, key_args_primary):
    return [
        op_panel(menu, kmi_args)
        for kmi_args in (key_args_primary, {"type": 'APP', "value": 'PRESS'})
    ]


def _template_space_region_type_toggle(
        params,
        *,
        toolbar_key=None,
        sidebar_key=None,
        channels_key=None,
):
    items = []

    if params.use_region_toggle_pie:
        pie_key = sidebar_key or sidebar_key or channels_key
        if pie_key is not None:
            items.append(op_menu_pie("WM_MT_region_toggle_pie", pie_key))
        return items

    if toolbar_key is not None:
        items.append(
            ("wm.context_toggle", toolbar_key,
             {"properties": [("data_path", "space_data.show_region_toolbar")]})
        )
    if sidebar_key is not None:
        items.append(
            ("wm.context_toggle", sidebar_key,
             {"properties": [("data_path", "space_data.show_region_ui")]}),
        )
    if channels_key is not None:
        items.append(
            ("wm.context_toggle", channels_key,
             {"properties": [("data_path", "space_data.show_region_channels")]}),
        )

    return items


def _template_items_transform_actions(
        params,
        *,
        use_bend=False,
        use_mirror=False,
        use_tosphere=False,
        use_shear=False,
):
    items = [
        op_tool_optional(
            ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
            (op_tool_cycle, "builtin.move"), params),
        op_tool_optional(
            ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
            (op_tool_cycle, "builtin.rotate"), params),
        op_tool_optional(
            ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
            (op_tool_cycle, "builtin.scale"), params),

        ("transform.translate", {"type": params.select_mouse, "value": 'CLICK_DRAG'}, None),
    ]

    if use_bend:
        items.append(
            ("transform.bend", {"type": 'W', "value": 'PRESS', "shift": True}, None)
        )
    if use_mirror:
        items.append(
            ("transform.mirror", {"type": 'M', "value": 'PRESS', "ctrl": True}, None)
        )
    if use_tosphere:
        items.append(
            op_tool_optional(
                ("transform.tosphere", {"type": 'S', "value": 'PRESS', "shift": True, "alt": True}, None),
                (op_tool_cycle, "builtin.to_sphere"), params)
        )
    if use_shear:
        items.append(
            op_tool_optional(
                ("transform.shear", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
                (op_tool_cycle, "builtin.shear"), params)
        )

    return items


def _template_items_select_actions(params, operator):
    if not params.use_select_all_toggle:
        return [
            (operator, {"type": 'A', "value": 'PRESS'}, {"properties": [("action", 'SELECT')]}),
            (operator, {"type": 'A', "value": 'PRESS', "alt": True}, {"properties": [("action", 'DESELECT')]}),
            (operator, {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
            (operator, {"type": 'A', "value": 'DOUBLE_CLICK'}, {"properties": [("action", 'DESELECT')]}),
        ]
    elif params.legacy:
        # Alt+A is for playback in legacy keymap.
        return [
            (operator, {"type": 'A', "value": 'PRESS'}, {"properties": [("action", 'TOGGLE')]}),
            (operator, {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ]
    else:
        return [
            (operator, {"type": 'A', "value": 'PRESS'}, {"properties": [("action", 'TOGGLE')]}),
            (operator, {"type": 'A', "value": 'PRESS', "alt": True}, {"properties": [("action", 'DESELECT')]}),
            (operator, {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ]


def _template_items_select_lasso(params, operator):
    # Needed because of shortcut conflicts on CTRL-LMB on right click select with brush modes,
    # all modifier keys are used together to unmask/deselect.
    if params.select_mouse == 'RIGHTMOUSE':
        return [
            (operator, {"type": params.action_mouse, "value": 'CLICK_DRAG', "shift": True, "ctrl": True},
             {"properties": [("mode", 'SUB')]}),
            (operator, {"type": params.action_mouse, "value": 'CLICK_DRAG', "shift": True, "ctrl": True, "alt": True},
             {"properties": [("mode", 'ADD')]}),
        ]
    else:
        return [
            (operator, {"type": params.action_mouse, "value": 'CLICK_DRAG', "shift": True, "ctrl": True},
             {"properties": [("mode", 'SUB')]}),
            (operator, {"type": params.action_mouse, "value": 'CLICK_DRAG', "ctrl": True},
             {"properties": [("mode", 'ADD')]}),
        ]


def _template_items_hide_reveal_actions(op_hide, op_reveal):
    return [
        (op_reveal, {"type": 'H', "value": 'PRESS', "alt": True}, None),
        (op_hide, {"type": 'H', "value": 'PRESS'}, {"properties": [("unselected", False)]}),
        (op_hide, {"type": 'H', "value": 'PRESS', "shift": True}, {"properties": [("unselected", True)]}),
    ]


def _template_object_hide_collection_from_number_keys():
    return [
        ("object.hide_collection", {
            "type": NUMBERS_1[i], "value": 'PRESS',
            **({"shift": True} if extend else {}),
            **({"alt": True} if add_10 else {}),
        }, {"properties": [
            ("collection_index", i + (11 if add_10 else 1)),
            ("extend", extend),
        ]})
        for extend in (False, True)
        for add_10 in (False, True)
        for i in range(10)
    ]


def _template_items_object_subdivision_set():
    return [
        ("object.subdivision_set",
         {"type": NUMBERS_0[i], "value": 'PRESS', "ctrl": True},
         {"properties": [("level", i), ("relative", False), ("ensure_modifier", True)]})
        for i in range(6)
    ]


def _template_items_gizmo_tweak_value():
    return [
        ("gizmogroup.gizmo_tweak",
         {"type": 'LEFTMOUSE', "value": 'PRESS', **any_except("alt")}, None),
    ]


def _template_items_gizmo_tweak_value_click_drag():
    return [
        ("gizmogroup.gizmo_tweak",
         {"type": 'LEFTMOUSE', "value": 'CLICK', **any_except("alt")}, None),
        ("gizmogroup.gizmo_tweak",
         {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', **any_except("alt")}, None),
    ]


def _template_items_gizmo_tweak_value_drag():
    return [
        ("gizmogroup.gizmo_tweak", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', **any_except("alt")}, None),
    ]


def _template_items_editmode_mesh_select_mode(params):
    if params.legacy:
        return [
            op_menu("VIEW3D_MT_edit_mesh_select_mode", {"type": 'TAB', "value": 'PRESS', "ctrl": True}),
        ]
    else:
        return [
            (
                "mesh.select_mode",
                {"type": NUMBERS_1[i], "value": 'PRESS', **key_expand, **key_extend},
                {"properties": [*prop_extend, *prop_expand, ("type", e)]}
            )
            for key_expand, prop_expand in (({}, ()), ({"ctrl": True}, (("use_expand", True),)))
            for key_extend, prop_extend in (({}, ()), ({"shift": True}, (("use_extend", True),)))
            for i, e in enumerate(('VERT', 'EDGE', 'FACE'))
        ]


def _template_items_uv_select_mode(params):
    if params.legacy:
        return [
            op_menu("IMAGE_MT_uvs_select_mode", {"type": 'TAB', "value": 'PRESS', "ctrl": True}),
        ]
    else:
        return [
            # TODO(@campbellbarton): should this be kept?
            # Seems it was included in the new key-map by accident, check on removing
            # although it's not currently used for anything else.
            op_menu("IMAGE_MT_uvs_select_mode", {"type": 'TAB', "value": 'PRESS', "ctrl": True}),

            *_template_items_editmode_mesh_select_mode(params),
            *(("uv.select_mode", {"type": NUMBERS_1[i], "value": 'PRESS'},
               {"properties": [("type", e)]})
              for i, e in enumerate(('VERTEX', 'EDGE', 'FACE'))),
            # Prior to v5.0 UV island was exposed as a selection mode.
            # Even though it's not longer a distinct mode, keep the shortcut
            # as it's handy and visually the 4th item in the UI.
            ("wm.context_toggle", {"type": 'FOUR', "value": 'PRESS'},
             {"properties": [("data_path", "tool_settings.use_uv_select_island")]}),
        ]


def _template_items_proportional_editing(params, *, connected, toggle_data_path):
    return [
        (
            op_menu_pie("VIEW3D_MT_proportional_editing_falloff_pie", {"type": 'O', "value": 'PRESS', "shift": True})
            if not params.legacy else
            ("wm.context_cycle_enum", {"type": 'O', "value": 'PRESS', "shift": True},
             {"properties": [("data_path", "tool_settings.proportional_edit_falloff"), ("wrap", True)]})
        ),
        ("wm.context_toggle", {"type": 'O', "value": 'PRESS'},
         {"properties": [("data_path", toggle_data_path)]}),
        *(() if not connected else (
            ("wm.context_toggle", {"type": 'O', "value": 'PRESS', "alt": True},
             {"properties": [("data_path", "tool_settings.use_proportional_connected")]}),
        ))
    ]


def _template_items_change_frame(params):
    if params.select_mouse == 'LEFTMOUSE' and not params.legacy:
        return [
            ("anim.change_frame", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True}, None),
        ]
    else:
        return [
            ("anim.change_frame", {"type": params.action_mouse, "value": 'PRESS'}, None),
        ]


# Tool System Templates

def _template_items_tool_select(
        params, operator, cursor_operator, *,
        # Always use the cursor operator where possible,
        # needed for time-line views where we always want to be able to scrub time.
        cursor_prioritize=False,
        fallback=False,
):
    select_passthrough = False
    if not params.legacy:
        # Experimental support for LMB interaction for the tweak tool. see: #96544.
        # NOTE: For RMB-select this is a much bigger change as it disables 3D cursor placement on LMB.
        # For LMB-select this means an LMB -drag will not first de-select all (similar to node/graph editor).
        if params.select_mouse == 'LEFTMOUSE':
            select_passthrough = params.use_tweak_select_passthrough
        else:
            if not cursor_prioritize:
                select_passthrough = True

        if not fallback and select_passthrough:
            return [
                (operator, {"type": 'LEFTMOUSE', "value": 'PRESS'},
                 {"properties": [("deselect_all", True), ("select_passthrough", True)]}),
                (operator, {"type": 'LEFTMOUSE', "value": 'CLICK'},
                 {"properties": [("deselect_all", True)]}),
                (operator, {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
                 {"properties": [("deselect_all", False), ("toggle", True)]}),
            ]

    if params.select_mouse == 'LEFTMOUSE':
        # Use 'PRESS' for immediate select without delay.
        # Tools that allow dragging anywhere should _NOT_ enable the fallback tool
        # unless it is expected that the tool should operate on the selection (click-drag to rip for example).
        return [
            (operator, {"type": 'LEFTMOUSE', "value": 'PRESS'},
             {"properties": [
                 ("deselect_all", True),
                 # Without this, fallback tool doesn't support pass-through, see: #115887.
                 *((("select_passthrough", True),) if select_passthrough else ()),
             ]}),
            (operator, {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
             {"properties": [("toggle", True)]}),

            # Fallback key-map must transform as the primary tool is expected
            # to be accessed via gizmos in this case. See: #96885.
            *(() if not fallback else (
                ("transform.translate", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'},
                 {"properties": [("release_confirm", True)]}),
            ))
        ]
    else:
        # For right mouse, set the cursor.
        return [
            (cursor_operator, {"type": 'LEFTMOUSE', "value": 'PRESS'}, None) if cursor_operator is not None else (),
            ("transform.translate", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'},
             {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        ]


def _template_items_tool_select_actions(operator, *, type, value):
    kmi_args = {"type": type, "value": value}
    return [
        (operator, kmi_args, None),
        (operator, {**kmi_args, "shift": True},
         {"properties": [("mode", 'ADD')]}),
        (operator, {**kmi_args, "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        (operator, {**kmi_args, "shift": True, "ctrl": True},
         {"properties": [("mode", 'AND')]}),
    ]


# This could have a more generic name, for now use for circle select.
def _template_items_tool_select_actions_simple(operator, *, type, value, properties=()):
    kmi_args = {"type": type, "value": value}
    return [
        # Don't define 'SET' here, take from the tool options.
        (operator, kmi_args,
         {"properties": [*properties]}),
        (operator, {**kmi_args, "shift": True},
         {"properties": [*properties, ("mode", 'ADD')]}),
        (operator, {**kmi_args, "ctrl": True},
         {"properties": [*properties, ("mode", 'SUB')]}),
    ]


def _template_items_legacy_tools_from_numbers():
    return [
        ("wm.tool_set_by_index",
         {
             "type": NUMBERS_1[i % 10],
             "value": 'PRESS',
             **({"shift": True} if i >= 10 else {}),
         },
         {"properties": [("index", i)]})
        for i in range(20)
    ]


# ------------------------------------------------------------------------------
# Window, Screen, Areas, Regions

def km_window(params):
    items = []
    keymap = (
        "Window",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    if params.legacy:
        # Old shortcuts
        items.extend([
            ("wm.save_homefile", {"type": 'U', "value": 'PRESS', "ctrl": True}, None),
            ("wm.open_mainfile", {"type": 'F1', "value": 'PRESS'}, None),
            ("wm.link", {"type": 'O', "value": 'PRESS', "ctrl": True, "alt": True}, None),
            ("wm.append", {"type": 'F1', "value": 'PRESS', "shift": True}, None),
            ("wm.save_mainfile", {"type": 'W', "value": 'PRESS', "ctrl": True}, None),
            ("wm.save_as_mainfile", {"type": 'F2', "value": 'PRESS'}, None),
            ("wm.save_as_mainfile", {"type": 'S', "value": 'PRESS', "ctrl": True, "alt": True},
             {"properties": [("copy", True)]}),
            ("wm.window_new", {"type": 'W', "value": 'PRESS', "ctrl": True, "alt": True}, None),
            ("wm.window_fullscreen_toggle", {"type": 'F11', "value": 'PRESS', "alt": True}, None),
            ("wm.doc_view_manual_ui_context", {"type": 'F1', "value": 'PRESS', "alt": True}, None),
            ("wm.search_menu", {"type": 'SPACE', "value": 'PRESS'}, None),
            ("wm.redraw_timer", {"type": 'T', "value": 'PRESS', "ctrl": True, "alt": True}, None),
            ("wm.debug_menu", {"type": 'D', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ])

    items.extend([
        # File operations
        op_menu("TOPBAR_MT_file_new", {"type": 'N', "value": 'PRESS', "ctrl": True}),
        op_menu("TOPBAR_MT_file_open_recent", {"type": 'O', "value": 'PRESS', "shift": True, "ctrl": True}),
        ("wm.open_mainfile", {"type": 'O', "value": 'PRESS', "ctrl": True}, None),
        ("wm.save_mainfile", {"type": 'S', "value": 'PRESS', "ctrl": True}, None),
        ("wm.save_as_mainfile", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("wm.save_mainfile",
         {"type": 'S', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("incremental", True)]}),
        ("wm.quit_blender", {"type": 'Q', "value": 'PRESS', "ctrl": True}, None),

        # Quick menu and toolbar
        op_menu("SCREEN_MT_user_menu", {"type": 'Q', "value": 'PRESS'}),

        # Fast editor switching
        *(
            ("screen.space_type_set_or_cycle",
             {"type": k, "value": 'PRESS', "shift": True},
             {"properties": [("space_type", t)]})
            for k, t in (
                ('F1', 'FILE_BROWSER'),
                ('F2', 'CLIP_EDITOR'),
                ('F3', 'NODE_EDITOR'),
                ('F4', 'CONSOLE'),
                ('F5', 'VIEW_3D'),
                ('F6', 'GRAPH_EDITOR'),
                ('F7', 'PROPERTIES'),
                ('F8', 'SEQUENCE_EDITOR'),
                ('F9', 'OUTLINER'),
                ('F10', 'IMAGE_EDITOR'),
                ('F11', 'TEXT_EDITOR'),
                ('F12', 'DOPESHEET_EDITOR'),
            )
        ),

        # NDOF settings
        op_panel("USERPREF_PT_ndof_settings", {"type": 'NDOF_BUTTON_MENU', "value": 'PRESS'}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_PLUS', "value": 'PRESS'},
         {"properties": [("data_path", "preferences.inputs.ndof_translation_sensitivity"), ("value", 1.1)]}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_MINUS', "value": 'PRESS'},
         {"properties": [("data_path", "preferences.inputs.ndof_translation_sensitivity"), ("value", 1.0 / 1.1)]}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_PLUS', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "preferences.inputs.ndof_translation_sensitivity"), ("value", 1.5)]}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_MINUS', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "preferences.inputs.ndof_translation_sensitivity"), ("value", 2.0 / 3.0)]}),
        ("info.reports_display_update", {"type": 'TIMER_REPORT', "value": 'ANY', "any": True}, None),
    ])

    if not params.legacy:
        # New shortcuts
        items.extend([
            ("wm.doc_view_manual_ui_context", {"type": 'F1', "value": 'PRESS'}, None),
            op_panel("TOPBAR_PT_name", {"type": 'F2', "value": 'PRESS'}, [("keep_open", False)]),
            ("wm.batch_rename", {"type": 'F2', "value": 'PRESS', "ctrl": True}, None),
            ("wm.search_menu", {"type": 'F3', "value": 'PRESS'}, None),
            op_menu("TOPBAR_MT_file_context_menu", {"type": 'F4', "value": 'PRESS'}),
            # Pass through when no tool-system exists or the fallback isn't available.
            ("wm.toolbar_fallback_pie", {"type": 'W', "value": 'PRESS', "alt": True}, None),
        ])

        if params.use_alt_click_leader:
            items.extend([
                # Alt as "Leader-Key".
                ("wm.toolbar_prompt", {"type": 'LEFT_ALT', "value": 'CLICK'}, None),
                ("wm.toolbar_prompt", {"type": 'RIGHT_ALT', "value": 'CLICK'}, None),
            ])

        if params.spacebar_action == 'TOOL':
            items.append(
                ("wm.toolbar", {"type": 'SPACE', "value": 'PRESS'}, None),
            )
        elif params.spacebar_action == 'PLAY':
            items.append(
                ("wm.toolbar", {"type": 'SPACE', "value": 'PRESS', "shift": True}, None),
            )
        elif params.spacebar_action == 'SEARCH':
            items.append(
                ("wm.search_menu", {"type": 'SPACE', "value": 'PRESS'}, None),
            )
        else:
            assert False, "unreachable"

    return keymap


def km_screen(params):
    items = []
    keymap = (
        "Screen",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Animation
        ("screen.animation_step", {"type": 'TIMER0', "value": 'ANY', "any": True}, None),
        ("screen.region_blend", {"type": 'TIMERREGION', "value": 'ANY', "any": True}, None),
        # Full screen and cycling
        ("screen.space_context_cycle", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
         {"properties": [("direction", 'NEXT')]}),
        ("screen.space_context_cycle", {"type": 'TAB', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'PREV')]}),
        ("screen.workspace_cycle", {"type": 'PAGE_DOWN', "value": 'PRESS', "ctrl": True},
         {"properties": [("direction", 'NEXT')]}),
        ("screen.workspace_cycle", {"type": 'PAGE_UP', "value": 'PRESS', "ctrl": True},
         {"properties": [("direction", 'PREV')]}),
        # Quad view
        ("screen.region_quadview", {"type": 'Q', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        # Repeat last
        ("screen.repeat_last", {"type": 'R', "value": 'PRESS', "shift": True, "repeat": True}, None),
        # Files
        ("file.execute", {"type": 'RET', "value": 'PRESS'}, None),
        ("file.execute", {"type": 'NUMPAD_ENTER', "value": 'PRESS'}, None),
        ("file.cancel", {"type": 'ESC', "value": 'PRESS'}, None),
        # Asset Catalog undo is only available in the asset browser, and should take priority over `ed.undo`.
        ("asset.catalog_undo", {"type": 'Z', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("asset.catalog_redo", {"type": 'Z', "value": 'PRESS', "ctrl": True, "shift": True, "repeat": True}, None),
        # Undo
        ("ed.undo", {"type": 'Z', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("ed.redo", {"type": 'Z', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True}, None),
        # Render
        ("render.render", {"type": 'F12', "value": 'PRESS'},
         {"properties": [("use_viewport", True)]}),
        ("render.render", {"type": 'F12', "value": 'PRESS', "ctrl": True},
         {"properties": [("animation", True), ("use_viewport", True)]}),
        ("render.render", {"type": 'F12', "value": 'PRESS', "alt": True},
         {"properties": [("use_sequencer_scene", True), ("use_viewport", True)]}),
        ("render.render", {"type": 'F12', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("animation", True), ("use_sequencer_scene", True), ("use_viewport", True)]}),
        ("render.view_cancel", {"type": 'ESC', "value": 'PRESS'}, None),
        ("render.view_show", {"type": 'F11', "value": 'PRESS'}, None),
        ("render.play_rendered_anim", {"type": 'F11', "value": 'PRESS', "ctrl": True}, None),
    ])

    if not params.legacy:
        items.extend([
            ("screen.screen_full_area", {"type": 'SPACE', "value": 'PRESS', "ctrl": True}, None),
            ("screen.screen_full_area", {"type": 'SPACE', "value": 'PRESS', "ctrl": True, "alt": True},
             {"properties": [("use_hide_panels", True)]}),
            ("screen.redo_last", {"type": 'F9', "value": 'PRESS'}, None),
        ])
    else:
        # Old keymap
        items.extend([
            ("ed.undo_history", {"type": 'Z', "value": 'PRESS', "ctrl": True, "alt": True}, None),
            ("screen.screen_full_area", {"type": 'UP_ARROW', "value": 'PRESS', "ctrl": True}, None),
            ("screen.screen_full_area", {"type": 'DOWN_ARROW', "value": 'PRESS', "ctrl": True}, None),
            ("screen.screen_full_area", {"type": 'SPACE', "value": 'PRESS', "shift": True}, None),
            ("screen.screen_full_area", {"type": 'F10', "value": 'PRESS', "alt": True},
             {"properties": [("use_hide_panels", True)]}),
            ("screen.screen_set", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True},
             {"properties": [("delta", 1)]}),
            ("screen.screen_set", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True},
             {"properties": [("delta", -1)]}),
            ("screen.screenshot", {"type": 'F3', "value": 'PRESS', "ctrl": True}, None),
            ("screen.repeat_history", {"type": 'R', "value": 'PRESS', "ctrl": True, "alt": True}, None),
            ("screen.region_flip", {"type": 'F5', "value": 'PRESS'}, None),
            ("screen.redo_last", {"type": 'F6', "value": 'PRESS'}, None),
            ("script.reload", {"type": 'F8', "value": 'PRESS'}, None),
        ])

    # Preferences.
    if not params.legacy:
        items.extend([
            ("screen.userpref_show", {"type": 'COMMA', "value": 'PRESS', "ctrl": True}, None),
        ])
    else:
        items.extend([
            ("screen.userpref_show", {"type": 'U', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ])

    return keymap


def km_screen_editing(params):
    items = []
    keymap = (
        "Screen Editing",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Action zones
        ("screen.actionzone", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("modifier", 0)]}),
        ("screen.actionzone", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("modifier", 1)]}),
        ("screen.actionzone", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("modifier", 2)]}),
        # Screen tools
        ("screen.area_split", {"type": 'ACTIONZONE_AREA', "value": 'ANY'}, None),
        ("screen.area_join", {"type": 'ACTIONZONE_AREA', "value": 'ANY'}, None),
        ("screen.area_dupli", {"type": 'ACTIONZONE_AREA', "value": 'ANY', "shift": True}, None),
        ("screen.area_swap", {"type": 'ACTIONZONE_AREA', "value": 'ANY', "ctrl": True}, None),
        ("screen.region_scale", {"type": 'ACTIONZONE_REGION', "value": 'ANY'}, None),
        ("screen.screen_full_area", {"type": 'ACTIONZONE_FULLSCREEN', "value": 'ANY'},
         {"properties": [("use_hide_panels", True)]}),
        # Area move after action zones
        ("screen.area_move", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("screen.area_move", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("snap", True)]}),
        ("screen.area_options", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
    ])

    if params.legacy:
        items.extend([
            ("wm.context_toggle", {"type": 'F9', "value": 'PRESS', "alt": True},
             {"properties": [("data_path", "space_data.show_region_header")]})
        ])

    return keymap


def km_screen_region_context_menu(_params):
    items = []
    keymap = (
        "Region Context Menu",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("screen.region_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
    ])

    return keymap


def km_view2d(_params):
    items = []
    keymap = (
        "View2D",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Scroll-bars.
        ("view2d.scroller_activate", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroller_activate", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        # Pan/scroll
        ("view2d.pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("view2d.pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "shift": True}, None),
        ("view2d.pan", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("view2d.scroll_right", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("view2d.scroll_right", {"type": 'WHEELRIGHTMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_left", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("view2d.scroll_left", {"type": 'WHEELLEFTMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_down", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True}, None),
        ("view2d.scroll_up", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True}, None),
        ("view2d.ndof", {"type": 'NDOF_MOTION', "value": 'ANY'}, None),
        # Zoom with single step
        ("view2d.zoom_out", {"type": 'WHEELOUTMOUSE', "value": 'PRESS'}, None),
        ("view2d.zoom_in", {"type": 'WHEELINMOUSE', "value": 'PRESS'}, None),
        ("view2d.zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True}, None),
        ("view2d.zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True}, None),
        ("view2d.zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("view2d.smoothview", {"type": 'TIMER1', "value": 'ANY', "any": True}, None),
        # Scroll up/down, only when zoom is not available.
        ("view2d.scroll_down", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_up", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_right", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_left", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        # Zoom with drag and border
        ("view2d.zoom", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("view2d.zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("view2d.zoom_border", {"type": 'B', "value": 'PRESS', "shift": True}, None),
    ])

    return keymap


def km_view2d_buttons_list(_params):
    items = []
    keymap = (
        "View2D Buttons List",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Scroll-bars.
        ("view2d.scroller_activate", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroller_activate", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        # Pan scroll
        ("view2d.pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("view2d.pan", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("view2d.scroll_down", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_up", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_down", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True},
         {"properties": [("page", True)]}),
        ("view2d.scroll_up", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True},
         {"properties": [("page", True)]}),
        # Zoom
        ("view2d.zoom", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("view2d.zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("view2d.zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("view2d.zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True}, None),
        ("view2d.zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True}, None),
        ("view2d.reset", {"type": 'HOME', "value": 'PRESS'}, None),
    ])

    return keymap


def km_user_interface(_params):
    items = []
    keymap = (
        "User Interface",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Eyedroppers all have the same event, and pass it through until
        # a suitable eyedropper handles it.
        ("ui.eyedropper_color", {"type": 'E', "value": 'PRESS'}, None),
        ("ui.eyedropper_colorramp", {"type": 'E', "value": 'PRESS'}, None),
        ("ui.eyedropper_colorramp_point", {"type": 'E', "value": 'PRESS', "alt": True}, None),
        ("ui.eyedropper_id", {"type": 'E', "value": 'PRESS'}, None),
        ("ui.eyedropper_depth", {"type": 'E', "value": 'PRESS'}, None),
        ("ui.eyedropper_bone", {"type": 'E', "value": 'PRESS'}, None),
        # Copy data path
        ("ui.copy_data_path_button", {"type": 'C', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("ui.copy_data_path_button", {"type": 'C', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("full_path", True)]}),
        # Keyframes and drivers
        ("anim.keyframe_insert_button", {"type": 'I', "value": 'PRESS'},
         {"properties": [("all", True)]}),
        ("anim.keyframe_delete_button", {"type": 'I', "value": 'PRESS', "alt": True},
         {"properties": [("all", True)]}),
        ("anim.keyframe_clear_button", {"type": 'I', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("all", True)]}),
        ("anim.driver_button_add", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("anim.driver_button_remove", {"type": 'D', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("anim.keyingset_button_add", {"type": 'K', "value": 'PRESS'}, None),
        ("anim.keyingset_button_remove", {"type": 'K', "value": 'PRESS', "alt": True}, None),
        ("ui.reset_default_button", {"type": 'BACK_SPACE', "value": 'PRESS'}, {"properties": [("all", True)]}),
        # UI lists (polls check if there's a UI list under the cursor).
        ("ui.list_start_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        # UI views (polls check if there's a UI view under the cursor).
        ("ui.view_start_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("ui.view_scroll", {"type": 'WHEELUPMOUSE', "value": 'ANY'}, None),
        ("ui.view_scroll", {"type": 'WHEELDOWNMOUSE', "value": 'ANY'}, None),
        ("ui.view_scroll", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("ui.view_item_select", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("ui.view_item_select", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("extend", True)]}),
        ("ui.view_item_select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("range_select", True)]}),
        ("ui.view_item_rename", {"type": 'F2', "value": 'PRESS'}, None),
        ("ui.view_item_delete", {"type": 'X', "value": 'PRESS'}, None),
        ("ui.view_item_delete", {"type": 'DEL', "value": 'PRESS'}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Shared Between Editors (Mask, Time-Line)

def km_mask_editing(params):
    items = []
    keymap = (
        "Mask Editing",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    if params.select_mouse == 'RIGHTMOUSE':
        # mask.slide_point performs mostly the same function, so for the left
        # click select keymap it's fine to have the context menu instead.
        items.extend([
            ("mask.select", {"type": 'RIGHTMOUSE', "value": 'PRESS'},
             {"properties": [("deselect_all", not params.legacy)]}),
        ])

    items.extend([
        ("mask.new", {"type": 'N', "value": 'PRESS', "alt": True}, None),
        op_menu("MASK_MT_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        *_template_items_proportional_editing(
            params, connected=False, toggle_data_path="tool_settings.use_proportional_edit_mask"),
        ("mask.add_vertex_slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("mask.add_feather_vertex_slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("mask.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("mask.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("mask.select", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("toggle", True)]}),
        *_template_items_select_actions(params, "mask.select_all"),
        ("mask.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("mask.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("mask.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("mask.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("mask.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        ("mask.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ADD')]}),
        ("mask.select_lasso",
         {"type": params.action_mouse, "value": 'CLICK_DRAG', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("mode", 'SUB')]}),
        ("mask.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("mask.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        *_template_items_hide_reveal_actions("mask.hide_view_set", "mask.hide_view_clear"),
        ("clip.select", {"type": params.select_mouse, "value": 'PRESS', "ctrl": True}, None),
        ("mask.cyclic_toggle", {"type": 'C', "value": 'PRESS', "alt": True}, None),
        ("mask.slide_point", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("mask.slide_spline_curvature", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("mask.handle_type_set", {"type": 'V', "value": 'PRESS'}, None),
        ("mask.normals_make_consistent",
         {"type": 'N', "value": 'PRESS', "ctrl" if params.legacy else "shift": True}, None),
        ("mask.parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        ("mask.parent_clear", {"type": 'P', "value": 'PRESS', "alt": True}, None),
        ("mask.shape_key_insert", {"type": 'I', "value": 'PRESS'}, None),
        ("mask.shape_key_clear", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("mask.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("mask.copy_splines", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("mask.paste_splines", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_mouse, "value": 'CLICK_DRAG'}, None),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        ("transform.tosphere", {"type": 'S', "value": 'PRESS', "shift": True, "alt": True}, None),
        ("transform.shear", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ("transform.transform", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'MASK_SHRINKFATTEN')]}),
    ])

    # 3D cursor
    if params.cursor_tweak_event:
        items.extend([
            ("uv.cursor_set", params.cursor_set_event, None),
            ("transform.translate", params.cursor_tweak_event,
             {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        ])
    else:
        items.extend([
            ("uv.cursor_set", params.cursor_set_event, None),
        ])

    return keymap


def km_markers(params):
    items = []
    keymap = (
        "Markers",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.move", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
         {"properties": [("tweak", True)]}),
        ("marker.duplicate", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("marker.select", {"type": params.select_mouse, "value": 'PRESS'}, None),
        ("marker.select", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("marker.select", {"type": params.select_mouse, "value": 'PRESS', "ctrl": True},
         {"properties": [("camera", True)]}),
        ("marker.select", {"type": params.select_mouse, "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("extend", True), ("camera", True)]}),
        ("marker.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
         {"properties": [("tweak", True)]}),
        ("marker.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG', "shift": True},
         {"properties": [("tweak", True), ("mode", 'ADD')]}),
        ("marker.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("tweak", True), ("mode", 'SUB')]}),
        ("marker.select_box", {"type": 'B', "value": 'PRESS'}, None),
        *_template_items_select_actions(params, "marker.select_all"),
        ("marker.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("marker.delete", {"type": 'DEL', "value": 'PRESS'}, {"properties": [("confirm", False)]}),
        op_panel("TOPBAR_PT_name_marker", {"type": 'F2', "value": 'PRESS'}, [("keep_open", False)]),
        op_panel("TOPBAR_PT_name_marker", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, [("keep_open", False)]),
        ("marker.move", {"type": 'G', "value": 'PRESS'}, None),
        ("marker.camera_bind", {"type": 'B', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_time_scrub(_params):
    items = []
    keymap = (
        "Time Scrub",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("anim.change_frame", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
    ])

    return keymap


def km_time_scrub_clip(_params):
    items = []
    keymap = (
        "Clip Time Scrub",
        {"space_type": 'CLIP_EDITOR', "region_type": 'PREVIEW'},
        {"items": items},
    )

    items.extend([
        ("clip.change_frame", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (Property Editor)

def km_property_editor(_params):
    items = []
    keymap = (
        "Property Editor",
        {"space_type": 'PROPERTIES', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("buttons.context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        ("screen.space_context_cycle", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("direction", 'PREV')]}),
        ("screen.space_context_cycle", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("direction", 'NEXT')]}),
        ("buttons.start_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("buttons.clear_filter", {"type": 'F', "value": 'PRESS', "alt": True}, None),
        # Modifier panels
        ("object.modifier_set_active", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("object.modifier_remove", {"type": 'X', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("object.modifier_remove", {"type": 'DEL', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("object.modifier_copy", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("object.add_modifier_menu", {"type": 'A', "value": 'PRESS', "shift": True}, None),
        ("object.modifier_apply", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("report", True)]}),
        # ShaderFX panels
        ("object.shaderfx_remove", {"type": 'X', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("object.shaderfx_remove", {"type": 'DEL', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("object.shaderfx_copy", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        # Constraint panels
        ("constraint.delete", {"type": 'X', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("constraint.delete", {"type": 'DEL', "value": 'PRESS'}, {"properties": [("report", True)]}),
        ("constraint.copy", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("constraint.apply", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("report", True)]}),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (Outliner)

def km_outliner(params):
    items = []
    keymap = (
        "Outliner",
        {"space_type": 'OUTLINER', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("outliner.highlight_update", {"type": 'MOUSEMOVE', "value": 'ANY', "any": True}, None),
        ("outliner.item_rename", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("outliner.item_rename", {"type": 'F2', "value": 'PRESS'},
         {"properties": [("use_active", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK'},
         {"properties": [("deselect_all", not params.legacy)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True},
         {"properties": [("extend", True), ("deselect_all", not params.legacy)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("extend_range", True), ("deselect_all", not params.legacy)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True, "shift": True},
         {"properties": [("extend", True), ("extend_range", True), ("deselect_all", not params.legacy)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'},
         {"properties": [("recurse", True), ("deselect_all", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "ctrl": True},
         {"properties": [("recurse", True), ("extend", True), ("deselect_all", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "shift": True},
         {"properties": [("recurse", True), ("extend_range", True), ("deselect_all", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "ctrl": True, "shift": True},
            {"properties": [("recurse", True), ("extend", True), ("extend_range", True), ("deselect_all", True)]}),
        ("outliner.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("outliner.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, {"properties": [("tweak", True)]}),
        ("outliner.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "shift": True},
         {"properties": [("tweak", True), ("mode", 'ADD')]}),
        ("outliner.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("tweak", True), ("mode", 'SUB')]}),
        ("outliner.select_walk", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'UP')]}),
        ("outliner.select_walk", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'UP'), ("extend", True)]}),
        ("outliner.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'DOWN')]}),
        ("outliner.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'DOWN'), ("extend", True)]}),
        ("outliner.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'LEFT')]}),
        ("outliner.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'LEFT'), ("toggle_all", True)]}),
        ("outliner.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'RIGHT')]}),
        ("outliner.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'RIGHT'), ("toggle_all", True)]}),
        ("outliner.item_openclose", {"type": 'LEFTMOUSE', "value": 'CLICK'},
         {"properties": [("all", False)]}),
        ("outliner.item_openclose", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("all", True)]}),
        ("outliner.item_openclose", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'},
         {"properties": [("all", False)]}),
        # Fall through to generic context menu if the item(s) selected have no type specific actions.
        ("outliner.operation", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        op_menu("OUTLINER_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        op_menu_pie("OUTLINER_MT_view_pie", {"type": 'ACCENT_GRAVE', "value": 'PRESS'}),
        ("outliner.item_drag_drop", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
        ("outliner.item_drag_drop", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "shift": True}, None),
        ("outliner.show_hierarchy", {"type": 'HOME', "value": 'PRESS'}, None),
        ("outliner.show_active", {"type": 'PERIOD', "value": 'PRESS'}, None),
        ("outliner.show_active", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("outliner.scroll_page", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True},
         {"properties": [("up", False)]}),
        ("outliner.scroll_page", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True},
         {"properties": [("up", True)]}),
        ("outliner.show_one_level", {"type": 'NUMPAD_PLUS', "value": 'PRESS'}, None),
        ("outliner.show_one_level", {"type": 'NUMPAD_MINUS', "value": 'PRESS'},
         {"properties": [("open", False)]}),
        *_template_items_select_actions(params, "outliner.select_all"),
        ("outliner.expanded_toggle", {"type": 'A', "value": 'PRESS', "shift": True}, None),
        ("outliner.keyingset_add_selected", {"type": 'K', "value": 'PRESS'}, None),
        ("outliner.keyingset_remove_selected", {"type": 'K', "value": 'PRESS', "alt": True}, None),
        ("anim.keyframe_insert", {"type": 'I', "value": 'PRESS'}, None),
        ("anim.keyframe_delete", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("outliner.drivers_add_selected", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("outliner.drivers_delete_selected", {"type": 'D', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("outliner.collection_new", {"type": 'C', "value": 'PRESS'}, None),
        ("outliner.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("outliner.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        op_menu("OBJECT_MT_move_to_collection", {"type": 'M', "value": 'PRESS'}),
        op_menu("OBJECT_MT_link_to_collection", {"type": 'M', "value": 'PRESS', "shift": True}),
        ("outliner.collection_exclude_set", {"type": 'E', "value": 'PRESS'}, None),
        ("outliner.collection_exclude_clear", {"type": 'E', "value": 'PRESS', "alt": True}, None),
        ("outliner.hide", {"type": 'H', "value": 'PRESS'}, None),
        ("outliner.unhide_all", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("outliner.start_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("outliner.clear_filter", {"type": 'F', "value": 'PRESS', "alt": True}, None),
        # Copy/paste.
        ("outliner.id_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("outliner.id_paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        *_template_object_hide_collection_from_number_keys(),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (UV Editor)

def km_uv_editor(params):
    items = []
    keymap = (
        "UV Editor",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Selection modes.
        *_template_items_uv_select_mode(params),
        *_template_uv_select(
            type=params.select_mouse,
            value=params.select_mouse_value_fallback,
            select_passthrough=params.use_tweak_select_passthrough,
            legacy=params.legacy,
        ),
        ("uv.mark_seam", {"type": 'E', "value": 'PRESS', "ctrl": True}, None),
        ("uv.select_loop",
         {"type": params.select_mouse, "value": params.select_mouse_value, "alt": True}, None),
        ("uv.select_loop",
         {"type": params.select_mouse, "value": params.select_mouse_value, "shift": True, "alt": True},
         {"properties": [("extend", True)]}),
        ("uv.select_edge_ring",
         {"type": params.select_mouse, "value": params.select_mouse_value, "ctrl": True, "alt": True}, None),
        ("uv.select_edge_ring",
         {"type": params.select_mouse, "value": params.select_mouse_value, "ctrl": True, "shift": True, "alt": True},
         {"properties": [("extend", True)]}),
        ("uv.shortest_path_pick",
         {"type": params.select_mouse, "value": params.select_mouse_value_fallback, "ctrl": True},
         {"properties": [("use_fill", False)]}),
        ("uv.shortest_path_pick",
         {"type": params.select_mouse, "value": params.select_mouse_value_fallback, "ctrl": True, "shift": True},
         {"properties": [("use_fill", True)]}),
        ("uv.select_split", {"type": 'Y', "value": 'PRESS'}, None),
        op_tool_optional(
            ("uv.select_box", {"type": 'B', "value": 'PRESS'},
             {"properties": [("pinned", False)]}),
            (op_tool, "builtin.select_box"), params),
        ("uv.select_box", {"type": 'B', "value": 'PRESS', "alt": True},
         {"properties": [("pinned", True)]}),
        op_tool_optional(
            ("uv.select_circle", {"type": 'C', "value": 'PRESS'}, None),
            (op_tool, "builtin.select_circle"), params),
        ("uv.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("mode", 'ADD')]}),
        ("uv.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "shift": True, "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        ("uv.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("uv.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("extend", True), ("deselect", False)]}),
        ("uv.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("uv.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("uv.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("uv.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        *_template_items_select_actions(params, "uv.select_all"),
        *_template_items_hide_reveal_actions("uv.hide", "uv.reveal"),
        ("uv.select_pinned", {"type": 'P', "value": 'PRESS', "shift": True}, None),
        op_menu("IMAGE_MT_uvs_merge", {"type": 'M', "value": 'PRESS'}),
        op_menu("IMAGE_MT_uvs_split", {"type": 'M', "value": 'PRESS', "alt": True}),
        op_menu("IMAGE_MT_uvs_align", {"type": 'W', "value": 'PRESS', "shift": True}),
        *[
            (
                "uv.move_on_axis",
                {"type": key, "value": 'PRESS', **mod_dict},
                {"properties": [("axis", axis), ("type", move_type), ("distance", distance)]},
            )
            for mod_dict, move_type in (
                ({"ctrl": True}, 'DYNAMIC'),
                ({"shift": True}, 'PIXEL'),
                ({}, 'UDIM'),
            )
            for key, axis, distance in (
                ('NUMPAD_8', 'Y', 1),
                ('NUMPAD_2', 'Y', -1),
                ('NUMPAD_6', 'X', 1),
                ('NUMPAD_4', 'X', -1),
            )
        ],
        ("uv.stitch", {"type": 'V', "value": 'PRESS', "alt": True}, None),
        ("uv.rip_move", {"type": 'V', "value": 'PRESS'}, None),
        ("uv.pin", {"type": 'P', "value": 'PRESS'},
         {"properties": [("clear", False)]}),
        ("uv.pin", {"type": 'P', "value": 'PRESS', "alt": True},
         {"properties": [("clear", True)]}),
        ("uv.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("uv.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("uv.custom_region_set", {"type": 'B', "value": 'PRESS', "ctrl": True}, None),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("data_path", "tool_settings.use_uv_custom_region")]}),

        op_menu("IMAGE_MT_uvs_unwrap", {"type": 'U', "value": 'PRESS'}),
        (
            op_menu_pie("IMAGE_MT_uvs_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True})
            if not params.legacy else
            op_menu("IMAGE_MT_uvs_snap", {"type": 'S', "value": 'PRESS', "shift": True})
        ),
        *_template_items_proportional_editing(
            params, connected=False, toggle_data_path="tool_settings.use_proportional_edit"),

        # Transform Actions.
        *_template_items_transform_actions(params, use_mirror=True, use_shear=True),

        ("wm.context_toggle", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "tool_settings.use_snap_uv")]}),
        ("wm.context_menu_enum", {"type": 'TAB', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("data_path", "tool_settings.snap_uv_element")]}),
        ("wm.context_toggle", {"type": 'ACCENT_GRAVE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.show_gizmo")]}),
        ("wm.context_toggle", {"type": 'Z', "value": 'PRESS', "alt": True, "shift": True},
         {"properties": [("data_path", "space_data.overlay.show_overlays")]}),
        *_template_items_context_menu("IMAGE_MT_uvs_context_menu", params.context_menu_event),
    ])

    # Fallback for MMB emulation
    if params.use_mouse_emulate_3_button and params.select_mouse == 'LEFTMOUSE':
        items.extend([
            ("uv.select_loop", {"type": params.select_mouse, "value": 'DOUBLE_CLICK'}, None),
            ("uv.select_loop", {"type": params.select_mouse, "value": 'DOUBLE_CLICK', "alt": True},
             {"properties": [("extend", True)]}),
        ])

    # 2D cursor
    if params.cursor_tweak_event:
        items.extend([
            ("uv.cursor_set", params.cursor_set_event, None),
            ("transform.translate", params.cursor_tweak_event,
             {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        ])
    else:
        items.extend([
            ("uv.cursor_set", params.cursor_set_event, None),
        ])

    if params.legacy:
        items.extend([
            ("uv.minimize_stretch", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
            ("uv.pack_islands", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
            ("uv.average_islands_scale", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ])

    if params.select_mouse == 'LEFTMOUSE' and not params.legacy:
        # Quick switch to select tool, since left select can't easily
        # select with any tool active.
        items.extend([
            op_tool_cycle("builtin.select_box", {"type": 'W', "value": 'PRESS'}),
        ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (3D View)

# 3D View: all regions.
def km_view3d_generic(params):
    items = []
    keymap = (
        "3D View Generic",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            params,
            toolbar_key={"type": 'T', "value": 'PRESS'},
            sidebar_key={"type": 'N', "value": 'PRESS'},
        )
    ])

    return keymap


# 3D View: main region.
def km_view3d(params):
    items = []
    keymap = (
        "3D View",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": items},
    )

    # 3D cursor
    if params.cursor_tweak_event:
        items.extend([
            ("view3d.cursor3d", params.cursor_set_event, None),
            ("transform.translate", params.cursor_tweak_event,
             {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        ])
    else:
        items.extend([
            ("view3d.cursor3d", params.cursor_set_event, None),
        ])

    items.extend([
        # Visibility.
        ("view3d.localview", {"type": 'NUMPAD_SLASH', "value": 'PRESS'}, None),
        ("view3d.localview", {"type": 'SLASH', "value": 'PRESS'}, None),
        ("view3d.localview", {"type": 'MOUSESMARTZOOM', "value": 'ANY'}, None),
        ("view3d.localview_remove_from", {"type": 'NUMPAD_SLASH', "value": 'PRESS', "alt": True}, None),
        ("view3d.localview_remove_from", {"type": 'SLASH', "value": 'PRESS', "alt": True}, None),
        # Navigation.
        ("view3d.rotate", {"type": 'MOUSEROTATE', "value": 'ANY'}, None),
        *((
            ("view3d.rotate", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "shift": True}, None),
            ("view3d.move", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
            ("view3d.rotate", {"type": 'TRACKPADPAN', "value": 'ANY', "shift": True}, None),
            ("view3d.move", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ) if params.use_v3d_mmb_pan else (
            ("view3d.rotate", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
            ("view3d.move", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "shift": True}, None),
            ("view3d.rotate", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
            ("view3d.move", {"type": 'TRACKPADPAN', "value": 'ANY', "shift": True}, None),
        )),
        ("view3d.view_pan", {"type": 'WHEELLEFTMOUSE', "value": 'PRESS'},
            {"properties": [("type", 'PANLEFT')]}),
        ("view3d.view_pan", {"type": 'WHEELRIGHTMOUSE', "value": 'PRESS'},
            {"properties": [("type", 'PANRIGHT')]}),
        ("view3d.zoom", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("view3d.dolly", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("view3d.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS', "ctrl": True},
         {"properties": [("use_all_regions", True)]}),
        ("view3d.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("view3d.smoothview", {"type": 'TIMER1', "value": 'ANY', "any": True}, None),
        ("view3d.zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("view3d.zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("view3d.zoom", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True},
         {"properties": [("delta", 1)]}),
        ("view3d.zoom", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True},
         {"properties": [("delta", -1)]}),
        ("view3d.zoom", {"type": 'EQUAL', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("delta", 1)]}),
        ("view3d.zoom", {"type": 'MINUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("delta", -1)]}),
        ("view3d.zoom", {"type": 'WHEELINMOUSE', "value": 'PRESS'},
         {"properties": [("delta", 1)]}),
        ("view3d.zoom", {"type": 'WHEELOUTMOUSE', "value": 'PRESS'},
         {"properties": [("delta", -1)]}),
        ("view3d.dolly", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("delta", 1)]}),
        ("view3d.dolly", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("delta", -1)]}),
        ("view3d.dolly", {"type": 'EQUAL', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("delta", 1)]}),
        ("view3d.dolly", {"type": 'MINUS', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("delta", -1)]}),
        ("view3d.view_center_camera", {"type": 'HOME', "value": 'PRESS'}, None),
        ("view3d.view_center_lock", {"type": 'HOME', "value": 'PRESS'}, None),
        ("view3d.view_all", {"type": 'HOME', "value": 'PRESS'},
         {"properties": [("center", False)]}),
        ("view3d.view_all", {"type": 'HOME', "value": 'PRESS', "ctrl": True},
         {"properties": [("use_all_regions", True), ("center", False)]}),
        ("view3d.view_all", {"type": 'C', "value": 'PRESS', "shift": True},
         {"properties": [("center", True)]}),
        op_menu_pie(
            "VIEW3D_MT_view_pie" if params.v3d_tilde_action == 'VIEW' else "VIEW3D_MT_transform_gizmo_pie",
            {"type": 'ACCENT_GRAVE', "value": params.pie_value},
        ),
        *(() if not params.use_pie_click_drag else
          (("view3d.navigate", {"type": 'ACCENT_GRAVE', "value": 'CLICK'}, None),)),
        ("view3d.navigate", {"type": 'ACCENT_GRAVE', "value": 'PRESS', "shift": True}, None),
        # Numpad views.
        ("view3d.view_camera", {"type": 'NUMPAD_0', "value": 'PRESS'}, None),
        ("view3d.view_axis", {"type": 'NUMPAD_1', "value": 'PRESS'},
         {"properties": [("type", 'FRONT')]}),
        ("view3d.view_orbit", {"type": 'NUMPAD_2', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'ORBITDOWN')]}),
        ("view3d.view_axis", {"type": 'NUMPAD_3', "value": 'PRESS'},
         {"properties": [("type", 'RIGHT')]}),
        ("view3d.view_orbit", {"type": 'NUMPAD_4', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'ORBITLEFT')]}),
        ("view3d.view_persportho", {"type": 'NUMPAD_5', "value": 'PRESS'}, None),
        ("view3d.view_orbit", {"type": 'NUMPAD_6', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'ORBITRIGHT')]}),
        ("view3d.view_axis", {"type": 'NUMPAD_7', "value": 'PRESS'},
         {"properties": [("type", 'TOP')]}),
        ("view3d.view_orbit", {"type": 'NUMPAD_8', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'ORBITUP')]}),
        ("view3d.view_axis", {"type": 'NUMPAD_1', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'BACK')]}),
        ("view3d.view_axis", {"type": 'NUMPAD_3', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'LEFT')]}),
        ("view3d.view_axis", {"type": 'NUMPAD_7', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'BOTTOM')]}),
        ("view3d.view_pan", {"type": 'NUMPAD_2', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PANDOWN')]}),
        ("view3d.view_pan", {"type": 'NUMPAD_4', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PANLEFT')]}),
        ("view3d.view_pan", {"type": 'NUMPAD_6', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PANRIGHT')]}),
        ("view3d.view_pan", {"type": 'NUMPAD_8', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PANUP')]}),
        ("view3d.view_roll", {"type": 'NUMPAD_4', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'LEFT')]}),
        ("view3d.view_roll", {"type": 'NUMPAD_6', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'RIGHT')]}),
        ("view3d.view_orbit", {"type": 'NUMPAD_9', "value": 'PRESS'},
         {"properties": [("angle", pi), ("type", 'ORBITRIGHT')]}),
        ("view3d.view_axis", {"type": 'NUMPAD_1', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'FRONT'), ("align_active", True)]}),
        ("view3d.view_axis", {"type": 'NUMPAD_3', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'RIGHT'), ("align_active", True)]}),
        ("view3d.view_axis", {"type": 'NUMPAD_7', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'TOP'), ("align_active", True)]}),
        ("view3d.view_axis", {"type": 'NUMPAD_1', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'BACK'), ("align_active", True)]}),
        ("view3d.view_axis", {"type": 'NUMPAD_3', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'LEFT'), ("align_active", True)]}),
        ("view3d.view_axis", {"type": 'NUMPAD_7', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'BOTTOM'), ("align_active", True)]}),
        *((
            ("view3d.view_axis", {"type": 'MIDDLEMOUSE', "value": 'CLICK_DRAG', "direction": 'NORTH', "alt": True},
             {"properties": [("type", 'TOP'), ("relative", True)]}),
            ("view3d.view_axis", {"type": 'MIDDLEMOUSE', "value": 'CLICK_DRAG', "direction": 'SOUTH', "alt": True},
             {"properties": [("type", 'BOTTOM'), ("relative", True)]}),
            ("view3d.view_axis", {"type": 'MIDDLEMOUSE', "value": 'CLICK_DRAG', "direction": 'EAST', "alt": True},
             {"properties": [("type", 'RIGHT'), ("relative", True)]}),
            ("view3d.view_axis", {"type": 'MIDDLEMOUSE', "value": 'CLICK_DRAG', "direction": 'WEST', "alt": True},
             {"properties": [("type", 'LEFT'), ("relative", True)]}),
        ) if params.v3d_alt_mmb_drag_action == 'RELATIVE' else (
            ("view3d.view_axis", {"type": 'MIDDLEMOUSE', "value": 'CLICK_DRAG', "direction": 'NORTH', "alt": True},
             {"properties": [("type", 'TOP')]}),
            ("view3d.view_axis", {"type": 'MIDDLEMOUSE', "value": 'CLICK_DRAG', "direction": 'SOUTH', "alt": True},
             {"properties": [("type", 'BOTTOM')]}),
            ("view3d.view_axis", {"type": 'MIDDLEMOUSE', "value": 'CLICK_DRAG', "direction": 'EAST', "alt": True},
             {"properties": [("type", 'RIGHT')]}),
            ("view3d.view_axis", {"type": 'MIDDLEMOUSE', "value": 'CLICK_DRAG', "direction": 'WEST', "alt": True},
             {"properties": [("type", 'LEFT')]}),
            ("view3d.view_axis", {"type": 'MIDDLEMOUSE', "value": 'CLICK_DRAG', "direction": 'NORTH_WEST', "alt": True},
             {"properties": [("type", 'FRONT')]}),
            ("view3d.view_axis", {"type": 'MIDDLEMOUSE', "value": 'CLICK_DRAG', "direction": 'NORTH_EAST', "alt": True},
             {"properties": [("type", 'BACK')]}),
            # Match the pie menu.
        )),
        ("view3d.view_center_pick", {"type": 'MIDDLEMOUSE', "value": 'CLICK', "alt": True}, None),
        ("view3d.ndof_orbit_zoom", {"type": 'NDOF_MOTION', "value": 'ANY'}, None),
        ("view3d.ndof_orbit", {"type": 'NDOF_MOTION', "value": 'ANY', "ctrl": True}, None),
        ("view3d.ndof_pan", {"type": 'NDOF_MOTION', "value": 'ANY', "shift": True}, None),
        ("view3d.ndof_all", {"type": 'NDOF_MOTION', "value": 'ANY', "shift": True, "ctrl": True}, None),
        ("view3d.view_selected", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("view3d.view_roll", {"type": 'NDOF_BUTTON_ROLL_CW', "value": 'PRESS'},
         {"properties": [("angle", PI_2)]}),
        ("view3d.view_roll", {"type": 'NDOF_BUTTON_ROLL_CCW', "value": 'PRESS'},
         {"properties": [("angle", -PI_2)]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_FRONT', "value": 'PRESS'},
         {"properties": [("type", 'FRONT')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_BACK', "value": 'PRESS'},
         {"properties": [("type", 'BACK')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_LEFT', "value": 'PRESS'},
         {"properties": [("type", 'LEFT')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_RIGHT', "value": 'PRESS'},
         {"properties": [("type", 'RIGHT')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_TOP', "value": 'PRESS'},
         {"properties": [("type", 'TOP')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_BOTTOM', "value": 'PRESS'},
         {"properties": [("type", 'BOTTOM')]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_FRONT', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'FRONT'), ("align_active", True)]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_RIGHT', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'RIGHT'), ("align_active", True)]}),
        ("view3d.view_axis", {"type": 'NDOF_BUTTON_TOP', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'TOP'), ("align_active", True)]}),
        # Selection.
        *_template_view3d_select(
            type=params.select_mouse,
            value=params.select_mouse_value_fallback,
            legacy=params.legacy,
            select_passthrough=params.use_tweak_select_passthrough,
        ),
        op_tool_optional(
            ("view3d.select_box", {"type": 'B', "value": 'PRESS'}, None),
            (op_tool, "builtin.select_box"), params),
        ("view3d.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("mode", 'ADD')]}),
        ("view3d.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "shift": True, "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        op_tool_optional(
            ("view3d.select_circle", {"type": 'C', "value": 'PRESS'}, None),
            (op_tool, "builtin.select_circle"), params),
        # Borders.
        ("view3d.clip_border", {"type": 'B', "value": 'PRESS', "alt": True}, None),
        ("view3d.zoom_border", {"type": 'B', "value": 'PRESS', "shift": True}, None),
        ("view3d.render_border", {"type": 'B', "value": 'PRESS', "ctrl": True}, None),
        ("view3d.clear_render_border", {"type": 'B', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        # Cameras.
        ("view3d.camera_to_view", {"type": 'NUMPAD_0', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("view3d.object_as_camera", {"type": 'NUMPAD_0', "value": 'PRESS', "ctrl": True}, None),
        # Copy/paste.
        ("view3d.copybuffer", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("view3d.pastebuffer", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        # Transform (handled by `_template_items_transform_actions`).
        # Snapping.
        ("wm.context_toggle", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "tool_settings.use_snap")]}),
        op_panel(
            "VIEW3D_PT_snapping",
            {"type": 'TAB', "value": 'PRESS', "shift": True, "ctrl": True},
            [("keep_open", True)],
        ),
        (
            op_menu_pie("VIEW3D_MT_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True})
            if not params.legacy else
            op_menu("VIEW3D_MT_snap", {"type": 'S', "value": 'PRESS', "shift": True})
        ),
    ])

    if not params.legacy:
        # New pie menus.
        items.extend([
            ("wm.context_toggle", {"type": 'ACCENT_GRAVE', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", "space_data.show_gizmo")]}),
            op_menu_pie("VIEW3D_MT_pivot_pie", {"type": 'PERIOD', "value": 'PRESS'}),
            op_menu_pie("VIEW3D_MT_orientations_pie", {"type": 'COMMA', "value": 'PRESS'}),
            op_menu_pie(
                "VIEW3D_MT_shading_pie" if not params.use_v3d_shade_ex_pie else
                "VIEW3D_MT_shading_ex_pie",
                {"type": 'Z', "value": params.pie_value}),
            *(() if not params.use_pie_click_drag else
              (("view3d.toggle_shading", {"type": 'Z', "value": 'CLICK'},
                {"properties": [("type", 'WIREFRAME')]}),)),
            ("view3d.toggle_shading", {"type": 'Z', "value": 'PRESS', "shift": True},
             {"properties": [("type", 'WIREFRAME')]}),
            ("view3d.toggle_xray", {"type": 'Z', "value": 'PRESS', "alt": True}, None),
            ("wm.context_toggle", {"type": 'Z', "value": 'PRESS', "alt": True, "shift": True},
             {"properties": [("data_path", "space_data.overlay.show_overlays")]}),
        ])
    else:
        items.extend([
            # Old navigation.
            ("view3d.view_lock_to_active", {"type": 'NUMPAD_PERIOD', "value": 'PRESS', "shift": True}, None),
            ("view3d.view_lock_clear", {"type": 'NUMPAD_PERIOD', "value": 'PRESS', "alt": True}, None),
            ("view3d.navigate", {"type": 'F', "value": 'PRESS', "shift": True}, None),
            ("view3d.zoom_camera_1_to_1", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "shift": True}, None),
            ("view3d.view_center_cursor", {"type": 'HOME', "value": 'PRESS', "alt": True}, None),
            ("view3d.view_center_pick", {"type": 'F', "value": 'PRESS', "alt": True}, None),
            ("view3d.view_pan", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "ctrl": True},
             {"properties": [("type", 'PANRIGHT')]}),
            ("view3d.view_pan", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True},
             {"properties": [("type", 'PANLEFT')]}),
            ("view3d.view_pan", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True},
             {"properties": [("type", 'PANUP')]}),
            ("view3d.view_pan", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True},
             {"properties": [("type", 'PANDOWN')]}),
            ("view3d.view_orbit", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "ctrl": True, "alt": True},
             {"properties": [("type", 'ORBITLEFT')]}),
            ("view3d.view_orbit", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True, "alt": True},
             {"properties": [("type", 'ORBITRIGHT')]}),
            ("view3d.view_orbit", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True, "alt": True},
             {"properties": [("type", 'ORBITUP')]}),
            ("view3d.view_orbit", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True, "alt": True},
             {"properties": [("type", 'ORBITDOWN')]}),
            ("view3d.view_roll", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
             {"properties": [("type", 'LEFT')]}),
            ("view3d.view_roll", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
             {"properties": [("type", 'RIGHT')]}),
            ("transform.create_orientation", {"type": 'SPACE', "value": 'PRESS', "ctrl": True, "alt": True},
             {"properties": [("use", True)]}),
            ("transform.translate", {"type": 'T', "value": 'PRESS', "shift": True},
             {"properties": [("texture_space", True)]}),
            ("transform.resize", {"type": 'T', "value": 'PRESS', "shift": True, "alt": True},
             {"properties": [("texture_space", True)]}),
            # Old pivot.
            ("wm.context_set_enum", {"type": 'COMMA', "value": 'PRESS'},
             {"properties": [("data_path", "tool_settings.transform_pivot_point"), ("value", 'BOUNDING_BOX_CENTER')]}),
            ("wm.context_set_enum", {"type": 'COMMA', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", "tool_settings.transform_pivot_point"), ("value", 'MEDIAN_POINT')]}),
            ("wm.context_toggle", {"type": 'COMMA', "value": 'PRESS', "alt": True},
             {"properties": [("data_path", "tool_settings.use_transform_pivot_point_align")]}),
            ("wm.context_toggle", {"type": 'SPACE', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", "space_data.show_gizmo_context")]}),
            ("wm.context_set_enum", {"type": 'PERIOD', "value": 'PRESS'},
             {"properties": [("data_path", "tool_settings.transform_pivot_point"), ("value", 'CURSOR')]}),
            ("wm.context_set_enum", {"type": 'PERIOD', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", "tool_settings.transform_pivot_point"), ("value", 'INDIVIDUAL_ORIGINS')]}),
            ("wm.context_set_enum", {"type": 'PERIOD', "value": 'PRESS', "alt": True},
             {"properties": [("data_path", "tool_settings.transform_pivot_point"), ("value", 'ACTIVE_ELEMENT')]}),
            # Old shading.
            ("wm.context_toggle_enum", {"type": 'Z', "value": 'PRESS'},
             {"properties": [
                 ("data_path", "space_data.shading.type"), ("value_1", 'WIREFRAME'), ("value_2", 'SOLID'),
             ]}),
            ("wm.context_toggle_enum", {"type": 'Z', "value": 'PRESS', "shift": True},
             {"properties": [
                 ("data_path", "space_data.shading.type"), ("value_1", 'RENDERED'), ("value_2", 'SOLID'),
             ]}),
            ("wm.context_toggle_enum", {"type": 'Z', "value": 'PRESS', "alt": True},
             {"properties": [
                 ("data_path", "space_data.shading.type"), ("value_1", 'MATERIAL'), ("value_2", 'SOLID'),
             ]}),
        ])

    if params.select_mouse == 'LEFTMOUSE' and not params.legacy:
        # Quick switch to select tool, since left select can't easily
        # select with any tool active.
        items.extend([
            op_tool_cycle("builtin.select_box", {"type": 'W', "value": 'PRESS'}),
        ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (Graph Editor)

def km_graph_editor_generic(params):
    items = []
    keymap = (
        "Graph Editor Generic",
        {"space_type": 'GRAPH_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            params,
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("graph.extrapolation_type", {"type": 'E', "value": 'PRESS', "shift": True}, None),
        ("graph.fmodifier_add", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("only_active", False)]}),
        ("anim.channels_select_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        *_template_items_hide_reveal_actions("graph.hide", "graph.reveal"),
        ("screen.space_type_set_or_cycle", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
         {"properties": [("space_type", 'DOPESHEET_EDITOR')]}),
    ])

    return keymap


def km_graph_editor(params):
    items = []
    keymap = (
        "Graph Editor",
        {"space_type": 'GRAPH_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("wm.context_toggle", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.show_handles")]}),
        ("wm.context_toggle", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "tool_settings.use_snap_anim")]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS'},
         {"properties": [("deselect_all", not params.legacy)]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS', "alt": True},
         {"properties": [("column", True)]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("extend", True), ("column", True)]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("curves", True)]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("extend", True), ("curves", True)]}),
        ("graph.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True},
         {"properties": [("mode", 'CHECK')]}),
        ("graph.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True, "shift": True},
         {"properties": [("mode", 'CHECK'), ("extend", True)]}),
        ("graph.select_leftright", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'LEFT')]}),
        ("graph.select_leftright", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'RIGHT')]}),
        *_template_items_select_actions(params, "graph.select_all"),
        ("graph.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("graph.select_box", {"type": 'B', "value": 'PRESS', "alt": True},
         {"properties": [("axis_range", True)]}),
        ("graph.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
         {"properties": [("tweak", True), ("mode", 'SET')]}),
        ("graph.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG', "shift": True},
         {"properties": [("tweak", True), ("mode", 'ADD')]}),
        ("graph.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("tweak", True), ("mode", 'SUB')]}),
        ("graph.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("mode", 'ADD')]}),
        ("graph.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "shift": True, "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        ("graph.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        ("graph.select_column", {"type": 'K', "value": 'PRESS'},
         {"properties": [("mode", 'KEYS')]}),
        ("graph.select_column", {"type": 'K', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'CFRA')]}),
        ("graph.select_column", {"type": 'K', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'MARKERS_COLUMN')]}),
        ("graph.select_column", {"type": 'K', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'MARKERS_BETWEEN')]}),
        ("graph.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("graph.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("graph.select_linked", {"type": 'L', "value": 'PRESS'}, None),
        ("graph.frame_jump", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        (
            op_menu_pie("GRAPH_MT_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True})
            if not params.legacy else
            ("graph.snap", {"type": 'S', "value": 'PRESS', "shift": True}, None)
        ),
        ("graph.mirror", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
        ("graph.handle_type", {"type": 'V', "value": 'PRESS'}, None),
        ("graph.interpolation_type", {"type": 'T', "value": 'PRESS'}, None),
        ("graph.easing_type", {"type": 'E', "value": 'PRESS', "ctrl": True}, None),
        ("graph.smooth", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("graph.bake_keys", {"type": 'O', "value": 'PRESS', "shift": True, "alt": True}, None),
        ("graph.keys_to_samples", {"type": 'C', "value": 'PRESS', "alt": True}, None),
        op_menu("GRAPH_MT_delete", {"type": 'X', "value": 'PRESS'}),
        ("graph.delete", {"type": 'DEL', "value": 'PRESS'}, {"properties": [("confirm", False)]}),
        ("graph.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("graph.keyframe_insert", {"type": 'I', "value": 'PRESS'}, None),
        ("graph.click_insert", {"type": params.action_mouse, "value": 'CLICK', "ctrl": True}, None),
        ("graph.click_insert", {"type": params.action_mouse, "value": 'CLICK', "shift": True, "ctrl": True},
         {"properties": [("extend", True)]}),
        ("graph.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("graph.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("graph.paste", {"type": 'V', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("flipped", True)]}),
        op_menu("GRAPH_MT_key_smoothing", {"type": 'S', "value": 'PRESS', "alt": True}),
        op_menu("GRAPH_MT_key_blending", {"type": 'D', "value": 'PRESS', "alt": True}),
        ("graph.previewrange_set", {"type": 'P', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("graph.view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("graph.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("graph.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("graph.view_frame", {"type": 'NUMPAD_0', "value": 'PRESS'}, None),
        op_menu_pie("GRAPH_MT_view_pie", {"type": 'ACCENT_GRAVE', "value": 'PRESS'}),
        ("anim.channels_editable_toggle", {"type": 'TAB', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_mouse, "value": 'CLICK_DRAG'}, None),
        ("transform.transform", {"type": 'E', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        *_template_items_proportional_editing(
            params, connected=False, toggle_data_path="tool_settings.use_proportional_fcurve"),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        *_template_items_context_menu("GRAPH_MT_context_menu", params.context_menu_event),
    ])

    if not params.legacy:
        items.extend([
            op_menu_pie("GRAPH_MT_pivot_pie", {"type": 'PERIOD', "value": 'PRESS'}),
        ])
    else:
        items.extend([
            # Old pivot.
            ("wm.context_set_enum", {"type": 'COMMA', "value": 'PRESS'},
             {"properties": [("data_path", "space_data.pivot_point"), ("value", 'BOUNDING_BOX_CENTER')]}),
            ("wm.context_set_enum", {"type": 'PERIOD', "value": 'PRESS'},
             {"properties": [("data_path", "space_data.pivot_point"), ("value", 'CURSOR')]}),
            ("wm.context_set_enum", {"type": 'PERIOD', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", "space_data.pivot_point"), ("value", 'INDIVIDUAL_ORIGINS')]}),
        ])

    if params.select_mouse == 'LEFTMOUSE' and not params.legacy:
        items.extend([
            ("graph.cursor_set", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True}, None),
        ])
    else:
        items.extend([
            ("graph.cursor_set", {"type": params.action_mouse, "value": 'PRESS'}, None),
        ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (Image)

def km_image_generic(params):
    items = []
    keymap = (
        "Image Generic",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            params,
            toolbar_key={"type": 'T', "value": 'PRESS'},
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("image.new", {"type": 'N', "value": 'PRESS', "alt": True}, None),
        ("image.open", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("image.reload", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("image.read_viewlayers", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("image.save", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("image.cycle_render_slot", {"type": 'J', "value": 'PRESS', "repeat": True}, None),
        ("image.cycle_render_slot", {"type": 'J', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("reverse", True)]}),
        op_menu_pie("IMAGE_MT_view_pie", {"type": 'ACCENT_GRAVE', "value": 'PRESS'}),
    ])

    if not params.legacy:
        items.extend([
            ("image.save_as", {"type": 'S', "value": 'PRESS', "shift": True, "alt": True}, None),
        ])
    else:
        items.extend([
            ("image.save_as", {"type": 'F3', "value": 'PRESS'}, None),
        ])

    return keymap


def km_image(params):
    items = []
    keymap = (
        "Image",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("image.view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("image.view_all", {"type": 'HOME', "value": 'PRESS', "shift": True},
         {"properties": [("fit_view", True)]}),
        ("image.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("image.view_cursor_center", {"type": 'C', "value": 'PRESS', "shift": True}, None),
        ("image.view_pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("image.view_pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "shift": True}, None),
        ("image.view_pan", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("image.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("image.view_ndof", {"type": 'NDOF_MOTION', "value": 'ANY'}, None),
        ("image.view_zoom_in", {"type": 'WHEELINMOUSE', "value": 'PRESS'}, None),
        ("image.view_zoom_out", {"type": 'WHEELOUTMOUSE', "value": 'PRESS'}, None),
        ("image.view_zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True}, None),
        ("image.view_zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True}, None),
        ("image.view_zoom", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("image.view_zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("image.view_zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("image.view_zoom_border", {"type": 'B', "value": 'PRESS', "shift": True}, None),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_1', "value": 'PRESS'},
         {"properties": [("ratio", 1.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_2', "value": 'PRESS'},
         {"properties": [("ratio", 0.5)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_4', "value": 'PRESS'},
         {"properties": [("ratio", 0.25)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_8', "value": 'PRESS'},
         {"properties": [("ratio", 0.125)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_8', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 8.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_4', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 4.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_2', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 2.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_1', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 1.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_8', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 8.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_4', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 4.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_2', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 2.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_1', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 1.0)]}),
        ("image.change_frame", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("image.sample", {"type": params.action_mouse, "value": 'PRESS'}, None),
        ("image.curves_point_set", {"type": params.action_mouse, "value": 'PRESS', "ctrl": True},
         {"properties": [("point", 'BLACK_POINT')]}),
        ("image.curves_point_set", {"type": params.action_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("point", 'WHITE_POINT')]}),
        ("object.mode_set", {"type": 'TAB', "value": 'PRESS'},
         {"properties": [("mode", 'EDIT'), ("toggle", True)]}),
        *(
            (("wm.context_set_int",
              {"type": NUMBERS_1[i], "value": 'PRESS'},
              {"properties": [("data_path", "space_data.image.render_slots.active_index"), ("value", i)]})
             for i in range(9)
             )
        ),
        ("image.render_border", {"type": 'B', "value": 'PRESS', "ctrl": True}, None),
        ("image.clear_render_border", {"type": 'B', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("wm.context_toggle", {"type": 'ACCENT_GRAVE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.show_gizmo")]}),
        ("wm.context_toggle", {"type": 'Z', "value": 'PRESS', "alt": True, "shift": True},
         {"properties": [("data_path", "space_data.overlay.show_overlays")]}),
        *_template_items_context_menu("IMAGE_MT_mask_context_menu", params.context_menu_event),
    ])

    if not params.legacy:
        items.extend([
            op_menu_pie("IMAGE_MT_pivot_pie", {"type": 'PERIOD', "value": 'PRESS'}),
        ])
    else:
        items.extend([
            # Old pivot.
            ("wm.context_set_enum", {"type": 'COMMA', "value": 'PRESS'},
             {"properties": [("data_path", "space_data.pivot_point"), ("value", 'CENTER')]}),
            ("wm.context_set_enum", {"type": 'COMMA', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", "space_data.pivot_point"), ("value", 'MEDIAN')]}),
            ("wm.context_set_enum", {"type": 'PERIOD', "value": 'PRESS'},
             {"properties": [("data_path", "space_data.pivot_point"), ("value", 'CURSOR')]}),

            ("image.view_center_cursor", {"type": 'HOME', "value": 'PRESS', "alt": True}, None),
        ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (Node)

def km_node_generic(params):
    items = []
    keymap = (
        "Node Generic",
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            params,
            toolbar_key={"type": 'T', "value": 'PRESS'},
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
    ])

    return keymap


def km_node_editor(params):
    items = []
    keymap = (
        "Node Editor",
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    if not params.legacy:
        items.extend(_template_node_select(
            type=params.select_mouse,
            value=params.select_mouse_value,
            select_passthrough=True,
        ))
        # Allow node selection with both for RMB select.
        if params.select_mouse == 'RIGHTMOUSE':
            items.extend(_template_node_select(type='LEFTMOUSE', value='PRESS', select_passthrough=True))
        else:
            items.extend([
                op_tool_cycle("builtin.select_box", {"type": 'W', "value": 'PRESS'}),
            ])
    else:
        items.extend(_template_node_select(
            type='RIGHTMOUSE',
            value=params.select_mouse_value,
            select_passthrough=True,
        ))
        items.extend(_template_node_select(
            type='LEFTMOUSE',
            value='PRESS',
            select_passthrough=True,
        ))

    items.extend([
        ("node.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
         {"properties": [("tweak", True)]}),
        ("node.select_lasso", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ADD')]}),
        ("node.select_lasso", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("mode", 'SUB')]}),
        op_tool_optional(
            ("node.select_box", {"type": 'B', "value": 'PRESS'},
             {"properties": [("tweak", False)]}),
            (op_tool, "builtin.select_box"), params),
        op_tool_optional(
            ("node.select_circle", {"type": 'C', "value": 'PRESS'}, None),
            (op_tool, "builtin.select_circle"), params),
        ("node.link", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'},
         {"properties": [("detach", False)]}),
        ("node.link", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("detach", True)]}),
        ("node.resize", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
        ("node.add_reroute",
         {"type": 'LEFTMOUSE' if params.legacy else 'RIGHTMOUSE', "value": 'CLICK_DRAG', "shift": True}, None),
        ("node.links_cut",
         {"type": 'LEFTMOUSE' if params.legacy else 'RIGHTMOUSE', "value": 'CLICK_DRAG', "ctrl": True}, None),
        ("node.links_mute", {"type": 'RIGHTMOUSE', "value": 'CLICK_DRAG', "ctrl": True, "alt": True}, None),
        ("node.select_link_viewer", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        # Shortcut is added three times, one for geometry nodes editor, and two for shader editor.
        # It's duplicated in shader editor so that it can act as stand-in for viewer node until it's added.
        ("node.connect_to_output", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("run_in_geometry_nodes", True)]}),
        ("node.connect_to_output", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("run_in_geometry_nodes", False)]}),
        ("node.connect_to_output", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("run_in_geometry_nodes", False)]}),
        ("node.backimage_move", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("node.backimage_zoom", {"type": 'V', "value": 'PRESS', "repeat": True},
         {"properties": [("factor", 1.0 / 1.2)]}),
        ("node.backimage_zoom", {"type": 'V', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("factor", 1.2)]}),
        ("node.backimage_fit", {"type": 'HOME', "value": 'PRESS', "alt": True}, None),
        ("node.backimage_sample", {"type": params.action_mouse, "value": 'PRESS', "alt": True}, None),
        ("node.link_make", {"type": 'J', "value": 'PRESS'},
         {"properties": [("replace", False)]}),
        ("node.link_make", {"type": 'J', "value": 'PRESS', "shift": True},
         {"properties": [("replace", True)]}),
        ("node.join_nodes", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        op_menu("NODE_MT_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        op_menu("NODE_MT_swap", {"type": 'S', "value": 'PRESS', "shift": True}),
        ("node.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True},
         {"properties": [("NODE_OT_translate_attach", [("TRANSFORM_OT_translate", [("view2d_edge_pan", True)])])]}),
        ("node.duplicate_move_linked", {"type": 'D', "value": 'PRESS', "alt": True},
         {"properties": [("NODE_OT_translate_attach", [("TRANSFORM_OT_translate", [("view2d_edge_pan", True)])])]}),
        ("node.duplicate_move_keep_inputs", {"type": 'D', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("NODE_OT_translate_attach", [("TRANSFORM_OT_translate", [("view2d_edge_pan", True)])])]}),
        ("node.parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        ("node.detach", {"type": 'P', "value": 'PRESS', "alt": True}, None),
        ("node.join_named", {"type": 'F', "value": 'PRESS'}, None),
        ("node.hide_toggle", {"type": 'H', "value": 'PRESS'}, None),
        ("node.mute_toggle", {"type": 'M', "value": 'PRESS'}, None),
        ("node.preview_toggle", {"type": 'H', "value": 'PRESS', "shift": True}, None),
        ("node.hide_socket_toggle", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
        ("node.view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("node.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("node.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        op_menu_pie("NODE_MT_view_pie", {"type": 'ACCENT_GRAVE', "value": 'PRESS'}),
        ("node.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("node.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("node.delete_reconnect", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("node.delete_reconnect", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        *_template_items_select_actions(params, "node.select_all"),
        ("node.select_linked_to", {"type": 'L', "value": 'PRESS', "shift": True}, None),
        ("node.select_linked_from", {"type": 'L', "value": 'PRESS'}, None),
        ("node.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("node.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("extend", True)]}),
        ("node.select_same_type_step", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("prev", False)]}),
        ("node.select_same_type_step", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("prev", True)]}),
        ("node.find_node", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("node.group_make", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("node.group_ungroup", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("node.group_separate", {"type": 'P', "value": 'PRESS'}, None),
        ("node.group_enter_exit", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("node.group_edit", {"type": 'TAB', "value": 'PRESS'},
         {"properties": [("exit", False)]}),
        ("node.group_edit", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
         {"properties": [("exit", True)]}),
        ("node.read_viewlayers", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("node.render_changed", {"type": 'Z', "value": 'PRESS'}, None),
        ("node.clipboard_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("node.clipboard_paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("node.viewer_border", {"type": 'B', "value": 'PRESS', "ctrl": True}, None),
        ("node.clear_viewer_border", {"type": 'B', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("node.translate_attach",
         {"type": 'G', "value": 'PRESS'},
         {"properties": [("TRANSFORM_OT_translate", [("view2d_edge_pan", True)])]}),
        ("node.translate_attach",
         {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'},
         {"properties": [("TRANSFORM_OT_translate", [("view2d_edge_pan", True)])]}),
        # Avoid duplicating the previous item.
        *([] if params.select_mouse == 'LEFTMOUSE' else (
            ("node.translate_attach", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
             {"properties": [("TRANSFORM_OT_translate", [("view2d_edge_pan", True)])]}),
        )),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, {"properties": [("view2d_edge_pan", True)]}),
        ("transform.translate", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'},
         {"properties": [("release_confirm", True), ("view2d_edge_pan", True)]}),
        # Avoid duplicating the previous item.
        *([] if params.select_mouse == 'LEFTMOUSE' else (
            ("transform.translate", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
             {"properties": [("release_confirm", True), ("view2d_edge_pan", True)]}),
        )),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        ("node.move_detach_links_release",
         {"type": params.action_mouse, "value": 'CLICK_DRAG', "alt": True},
         {"properties": [("NODE_OT_translate_attach", [("TRANSFORM_OT_translate", [("view2d_edge_pan", True)])])]}),
        ("node.move_detach_links",
         {"type": params.select_mouse, "value": 'CLICK_DRAG', "alt": True},
         {"properties": [("TRANSFORM_OT_translate", [("view2d_edge_pan", True)])]}),
        ("wm.context_toggle", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'tool_settings.use_snap_node')]}),
        ("wm.context_toggle", {"type": 'Z', "value": 'PRESS', "alt": True, "shift": True},
         {"properties": [("data_path", "space_data.overlay.show_overlays")]}),
        *_template_items_context_menu("NODE_MT_context_menu", params.context_menu_event),
        # Viewer shortcuts.
        *(
            ("node.viewer_shortcut_get", {"type": NUMBERS_0[i], "value": 'PRESS'},
             {"properties": [("viewer_index", i)]})
            for i in range(1, 10)
        ),
        *(
            ("node.viewer_shortcut_set", {"type": NUMBERS_0[i], "value": 'PRESS', "ctrl": True},
             {"properties": [("viewer_index", i)]})
            for i in range(1, 10)
        ),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (Info)

def km_info(params):
    items = []
    keymap = (
        "Info",
        {"space_type": 'INFO', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("info.select_pick", {"type": 'LEFTMOUSE', "value": 'CLICK'}, None),
        ("info.select_pick", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("extend", True)]}),
        ("info.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'},
         {"properties": [("wait_for_input", False)]}),
        *_template_items_select_actions(params, "info.select_all"),
        ("info.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("info.report_replay", {"type": 'R', "value": 'PRESS'}, None),
        ("info.report_delete", {"type": 'X', "value": 'PRESS'}, None),
        ("info.report_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("info.report_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        *_template_items_context_menu("INFO_MT_context_menu", params.context_menu_event),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (File Browser)

def km_file_browser(params):
    items = []
    keymap = (
        "File Browser",
        {"space_type": 'FILE_BROWSER', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            params,
            toolbar_key={"type": 'T', "value": 'PRESS'},
        ),
        ("wm.context_toggle", {"type": 'N', "value": 'PRESS'},
         {"properties": [("data_path", "space_data.show_region_tool_props")]}),
        ("file.parent", {"type": 'UP_ARROW', "value": 'PRESS', "alt": True}, None),
        ("file.previous", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True}, None),
        ("file.previous", {"type": 'BUTTON4MOUSE', "value": 'PRESS'}, None),
        ("file.next", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True}, None),
        ("file.next", {"type": 'BUTTON5MOUSE', "value": 'PRESS'}, None),
        # The two refresh operators have polls excluding each other (so only one is available depending on context).
        ("file.refresh", {"type": 'R', "value": 'PRESS'}, None),
        ("asset.library_refresh", {"type": 'R', "value": 'PRESS'}, None),
        ("file.parent", {"type": 'P', "value": 'PRESS'}, None),
        ("file.previous", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("file.next", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True}, None),
        ("wm.context_toggle", {"type": 'H', "value": 'PRESS'},
         {"properties": [("data_path", "space_data.params.show_hidden")]}),
        ("file.directory_new", {"type": 'I', "value": 'PRESS'},
         {"properties": [("confirm", False)]}),
        ("file.rename", {"type": 'F2', "value": 'PRESS'}, None),
        ("file.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("file.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("file.smoothscroll", {"type": 'TIMER1', "value": 'ANY', "any": True}, None),
        ("file.bookmark_add", {"type": 'B', "value": 'PRESS', "ctrl": True}, None),
        ("file.start_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("file.edit_directory_path", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True},
         {"properties": [("increment", 1)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("increment", 10)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("increment", 100)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True},
         {"properties": [("increment", -1)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("increment", -10)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("increment", -100)]}),
        op_menu_pie("FILEBROWSER_MT_view_pie", {"type": 'ACCENT_GRAVE', "value": 'PRESS'}),

        # Select file under cursor before spawning the context menu.
        ("file.select", {"type": 'RIGHTMOUSE', "value": 'PRESS'},
         {"properties": [
             ("open", False),
             ("only_activate_if_selected", params.select_mouse == 'LEFTMOUSE'), ("pass_through", True),
         ]}),
        *_template_items_context_menu("FILEBROWSER_MT_context_menu", params.context_menu_event),
    ])

    return keymap


def km_file_browser_main(params):
    items = []
    keymap = (
        "File Browser Main",
        {"space_type": 'FILE_BROWSER', "region_type": 'WINDOW'},
        {"items": items},
    )

    if not params.use_file_single_click:
        items.extend([
            ("file.select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'},
             {"properties": [("open", True), ("deselect_all", not params.legacy)]}),
        ])

    items.extend([
        ("file.mouse_execute", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        # Both .execute and .select are needed here. The former only works if
        # there's a file operator (i.e. not in regular editor mode) but is
        # needed to load files. The latter makes selection work if there's no
        # operator (i.e. in regular editor mode).
        ("file.select", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("open", params.use_file_single_click), ("deselect_all", not params.legacy)]}),
        ("file.select", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True},
         {"properties": [("extend", True), ("open", False)]}),
        ("file.select", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("extend", True), ("fill", True), ("open", False)]}),
        ("file.select_walk", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'UP')]}),
        ("file.select_walk", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'UP'), ("extend", True)]}),
        ("file.select_walk", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("direction", 'UP'), ("extend", True), ("fill", True)]}),
        ("file.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'DOWN')]}),
        ("file.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'DOWN'), ("extend", True)]}),
        ("file.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("direction", 'DOWN'), ("extend", True), ("fill", True)]}),
        ("file.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'LEFT')]}),
        ("file.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'LEFT'), ("extend", True)]}),
        ("file.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("direction", 'LEFT'), ("extend", True), ("fill", True)]}),
        ("file.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'RIGHT')]}),
        ("file.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'RIGHT'), ("extend", True)]}),
        ("file.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("direction", 'RIGHT'), ("extend", True), ("fill", True)]}),
        ("file.previous", {"type": 'BUTTON4MOUSE', "value": 'CLICK'}, None),
        ("file.next", {"type": 'BUTTON5MOUSE', "value": 'CLICK'}, None),
        *_template_items_select_actions(params, "file.select_all"),
        ("file.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("file.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
        ("file.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "shift": True},
         {"properties": [("mode", 'ADD')]}),
        ("file.select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        ("file.highlight", {"type": 'MOUSEMOVE', "value": 'ANY', "any": True}, None),
        ("file.sort_column_ui_context", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("file.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        *_template_items_context_menu("ASSETBROWSER_MT_context_menu", params.context_menu_event),
    ])

    return keymap


def km_file_browser_buttons(_params):
    items = []
    keymap = (
        "File Browser Buttons",
        {"space_type": 'FILE_BROWSER', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True},
         {"properties": [("increment", 1)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("increment", 10)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("increment", 100)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True},
         {"properties": [("increment", -1)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("increment", -10)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("increment", -100)]}),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (Dope Sheet)

def km_dopesheet_generic(params):
    items = []
    keymap = (
        "Dopesheet Generic",
        {"space_type": 'DOPESHEET_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            params,
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("wm.context_set_enum", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "area.type"), ("value", 'GRAPH_EDITOR')]}),
        ("action.extrapolation_type", {"type": 'E', "value": 'PRESS', "shift": True}, None),
    ])

    return keymap


def km_dopesheet(params):
    items = []
    keymap = (
        "Dopesheet",
        {"space_type": 'DOPESHEET_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("action.clickselect",
         {"type": params.select_mouse, "value": 'PRESS'},
         {"properties": [("deselect_all", not params.legacy)]}),
        ("action.clickselect",
         {"type": params.select_mouse, "value": 'PRESS', "alt": True},
         {"properties": [("column", True)]}),
        ("action.clickselect",
         {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("action.clickselect",
         {"type": params.select_mouse, "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("extend", True), ("column", True)]}),
        ("action.clickselect",
         {"type": params.select_mouse, "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("channel", True)]}),
        ("action.clickselect",
         {"type": params.select_mouse, "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("extend", True), ("channel", True)]}),
        ("action.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True},
         {"properties": [("mode", 'CHECK')]}),
        ("action.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True, "shift": True},
         {"properties": [("mode", 'CHECK'), ("extend", True)]}),
        ("action.select_leftright", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'LEFT')]}),
        ("action.select_leftright", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'RIGHT')]}),
        *_template_items_select_actions(params, "action.select_all"),
        ("action.select_box", {"type": 'B', "value": 'PRESS'},
         {"properties": [("axis_range", False)]}),
        ("action.select_box", {"type": 'B', "value": 'PRESS', "alt": True},
         {"properties": [("axis_range", True)]}),
        ("action.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
         {"properties": [("tweak", True), ("mode", 'SET')]}),
        ("action.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG', "shift": True},
         {"properties": [("tweak", True), ("mode", 'ADD')]}),
        ("action.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("tweak", True), ("mode", 'SUB')]}),
        ("action.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("mode", 'ADD')]}),
        ("action.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "shift": True, "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        ("action.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        ("action.select_column", {"type": 'K', "value": 'PRESS'},
         {"properties": [("mode", 'KEYS')]}),
        ("action.select_column", {"type": 'K', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'CFRA')]}),
        ("action.select_column", {"type": 'K', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'MARKERS_COLUMN')]}),
        ("action.select_column", {"type": 'K', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'MARKERS_BETWEEN')]}),
        ("action.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("action.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("action.select_linked", {"type": 'L', "value": 'PRESS'}, None),
        ("action.frame_jump", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        (
            op_menu_pie("DOPESHEET_MT_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True})
            if not params.legacy else
            ("action.snap", {"type": 'S', "value": 'PRESS', "shift": True}, None)
        ),
        ("action.mirror", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
        ("action.handle_type", {"type": 'V', "value": 'PRESS'}, None),
        ("action.interpolation_type", {"type": 'T', "value": 'PRESS'}, None),
        ("action.extrapolation_type", {"type": 'E', "value": 'PRESS', "shift": True}, None),
        ("action.easing_type", {"type": 'E', "value": 'PRESS', "ctrl": True}, None),
        ("action.keyframe_type", {"type": 'R', "value": 'PRESS'}, None),
        ("action.bake_keys", {"type": 'O', "value": 'PRESS', "shift": True, "alt": True}, None),
        ("grease_pencil.layer_isolate", {"type": 'NUMPAD_ASTERIX', "value": 'PRESS'}, None),
        op_menu("DOPESHEET_MT_delete", {"type": 'X', "value": 'PRESS'}),
        ("action.delete", {"type": 'DEL', "value": 'PRESS'}, {"properties": [("confirm", False)]}),
        ("action.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("action.keyframe_insert", {"type": 'I', "value": 'PRESS'}, None),
        ("action.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("action.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("action.paste", {"type": 'V', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("flipped", True)]}),
        ("action.previewrange_set", {"type": 'P', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("action.view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("action.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("action.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("action.view_frame", {"type": 'NUMPAD_0', "value": 'PRESS'}, None),
        op_menu_pie("DOPESHEET_MT_view_pie", {"type": 'ACCENT_GRAVE', "value": 'PRESS'}),
        ("anim.channels_editable_toggle", {"type": 'TAB', "value": 'PRESS'}, None),
        ("anim.channels_select_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("transform.transform", {"type": 'G', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_TRANSLATE')]}),
        ("transform.transform", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
         {"properties": [("mode", 'TIME_TRANSLATE')]}),
        ("transform.transform", {"type": 'E', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        ("transform.transform", {"type": 'S', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_SCALE')]}),
        ("transform.transform", {"type": 'T', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'TIME_SLIDE')]}),
        ("wm.context_toggle", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "tool_settings.use_snap_anim")]}),
        *_template_items_proportional_editing(
            params, connected=False, toggle_data_path="tool_settings.use_proportional_action"),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.camera_bind", {"type": 'B', "value": 'PRESS', "ctrl": True}, None),
        *_template_items_context_menu("DOPESHEET_MT_context_menu", params.context_menu_event),
        *_template_items_change_frame(params),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (NLA)

def km_nla_generic(params):
    items = []
    keymap = (
        "NLA Generic",
        {"space_type": 'NLA_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            params,
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("nla.tweakmode_enter", {"type": 'TAB', "value": 'PRESS'},
         {"properties": [("use_upper_stack_evaluation", True)]}),
        ("nla.tweakmode_exit", {"type": 'TAB', "value": 'PRESS'}, None),
        ("nla.tweakmode_enter", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("isolate_action", True)]}),
        ("nla.tweakmode_exit", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("isolate_action", True)]}),
        ("anim.channels_select_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_nla_tracks(params):
    items = []
    keymap = (
        "NLA Tracks",
        {"space_type": 'NLA_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("nla.channels_click", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("nla.channels_click", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("nla.tracks_add", {"type": 'A', "value": 'PRESS', "shift": True},
         {"properties": [("above_selected", False)]}),
        ("nla.tracks_add", {"type": 'A', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("above_selected", True)]}),
        ("nla.tracks_delete", {"type": 'X', "value": 'PRESS'}, None),
        ("nla.tracks_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        *_template_items_context_menu("NLA_MT_channel_context_menu", params.context_menu_event),
    ])

    return keymap


def km_nla_editor(params):
    items = []
    keymap = (
        "NLA Editor",
        {"space_type": 'NLA_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("nla.click_select", {"type": params.select_mouse, "value": 'PRESS'},
         {"properties": [("deselect_all", not params.legacy)]}),
        ("nla.click_select", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("nla.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True},
         {"properties": [("mode", 'CHECK')]}),
        ("nla.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True, "shift": True},
         {"properties": [("mode", 'CHECK'), ("extend", True)]}),
        ("nla.select_leftright", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'LEFT')]}),
        ("nla.select_leftright", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'RIGHT')]}),
        *_template_items_select_actions(params, "nla.select_all"),
        ("nla.select_box", {"type": 'B', "value": 'PRESS'},
         {"properties": [("axis_range", False)]}),
        ("nla.select_box", {"type": 'B', "value": 'PRESS', "alt": True},
         {"properties": [("axis_range", True)]}),
        ("nla.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
         {"properties": [("tweak", True), ("mode", 'SET')]}),
        ("nla.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG', "shift": True},
         {"properties": [("tweak", True), ("mode", 'ADD')]}),
        ("nla.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("tweak", True), ("mode", 'SUB')]}),
        ("nla.previewrange_set", {"type": 'P', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("nla.view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("nla.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("nla.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("nla.view_frame", {"type": 'NUMPAD_0', "value": 'PRESS'}, None),
        op_menu_pie("NLA_MT_view_pie", {"type": 'ACCENT_GRAVE', "value": 'PRESS'}),
        ("nla.actionclip_add", {"type": 'A', "value": 'PRESS', "shift": True}, None),
        ("nla.transition_add", {"type": 'T', "value": 'PRESS', "shift": True}, None),
        ("nla.soundclip_add", {"type": 'K', "value": 'PRESS', "shift": True}, None),
        ("nla.meta_add", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("nla.meta_remove", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("nla.duplicate_linked_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("nla.duplicate_move", {"type": 'D', "value": 'PRESS', "alt": True}, None),
        ("nla.make_single_user", {"type": 'U', "value": 'PRESS'}, None),
        ("nla.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("nla.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("nla.split", {"type": 'Y', "value": 'PRESS'}, None),
        ("nla.mute_toggle", {"type": 'H', "value": 'PRESS'}, None),
        ("nla.swap", {"type": 'F', "value": 'PRESS', "alt": True}, None),
        ("nla.move_up", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True}, None),
        ("nla.move_down", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True}, None),
        ("nla.apply_scale", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("nla.clear_scale", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        (
            op_menu_pie("NLA_MT_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True})
            if not params.legacy else
            ("nla.snap", {"type": 'S', "value": 'PRESS', "shift": True}, None)
        ),
        ("nla.fmodifier_add", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("transform.transform", {"type": 'G', "value": 'PRESS'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("transform.transform", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("transform.transform", {"type": 'E', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        ("transform.transform", {"type": 'S', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_SCALE')]}),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        *_template_items_context_menu("NLA_MT_context_menu", params.context_menu_event),
    ])

    if params.select_mouse == 'LEFTMOUSE' and not params.legacy:
        items.extend([
            ("anim.change_frame", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True},
             {"properties": [("seq_solo_preview", True)]}),
        ])
    else:
        items.extend([
            ("anim.change_frame", {"type": params.action_mouse, "value": 'PRESS'},
             {"properties": [("seq_solo_preview", True)]}),
        ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (Text)

def km_text_generic(params):
    items = []
    keymap = (
        "Text Generic",
        {"space_type": 'TEXT_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            params,
            sidebar_key={"type": 'T', "value": 'PRESS', "ctrl": True},
        ),
        ("text.start_find", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("text.jump", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        ("text.find_set_selected", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("text.replace", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_text(params):
    items = []
    keymap = (
        "Text",
        {"space_type": 'TEXT_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("wm.context_cycle_int", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.font_size"), ("reverse", False)]}),
        ("wm.context_cycle_int", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.font_size"), ("reverse", True)]}),
        ("wm.context_cycle_int", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("data_path", "space_data.font_size"), ("reverse", False)]}),
        ("wm.context_cycle_int", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("data_path", "space_data.font_size"), ("reverse", True)]}),
    ])

    if not params.legacy:
        items.extend([
            ("text.new", {"type": 'N', "value": 'PRESS', "alt": True}, None),
        ])
    else:
        items.extend([
            ("text.new", {"type": 'N', "value": 'PRESS', "ctrl": True}, None),

            ("text.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
             {"properties": [("type", 'PREVIOUS_WORD')]}),
            ("text.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
             {"properties": [("type", 'NEXT_WORD')]}),
        ])

    items.extend([
        ("text.open", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("text.reload", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("text.save", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("text.save_as", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ("text.run_script", {"type": 'P', "value": 'PRESS', "alt": True}, None),
        ("text.cut", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("text.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("text.paste", {"type": 'V', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("text.cut", {"type": 'DEL', "value": 'PRESS', "shift": True}, None),
        ("text.copy", {"type": 'INSERT', "value": 'PRESS', "ctrl": True}, None),
        ("text.paste", {"type": 'INSERT', "value": 'PRESS', "shift": True, "repeat": True}, None),
        ("text.duplicate_line", {"type": 'D', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("text.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("text.select_line", {"type": 'A', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("text.select_word", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("text.move_lines", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("direction", 'UP')]}),
        ("text.move_lines", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("direction", 'DOWN')]}),
        ("text.indent_or_autocomplete", {"type": 'TAB', "value": 'PRESS', "repeat": True}, None),
        ("text.unindent", {"type": 'TAB', "value": 'PRESS', "shift": True, "repeat": True}, None),
        ("text.comment_toggle", {"type": 'SLASH', "value": 'PRESS', "ctrl": True}, None),
        ("text.move", {"type": 'HOME', "value": 'PRESS'},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("text.move", {"type": 'END', "value": 'PRESS'},
         {"properties": [("type", 'LINE_END')]}),
        ("text.move", {"type": 'E', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'LINE_END')]}),
        ("text.move", {"type": 'E', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'LINE_END')]}),
        ("text.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("text.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("text.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("text.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("text.move", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_LINE')]}),
        ("text.move", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_LINE')]}),
        ("text.move", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_PAGE')]}),
        ("text.move", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_PAGE')]}),
        ("text.move", {"type": 'HOME', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'FILE_TOP')]}),
        ("text.move", {"type": 'END', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'FILE_BOTTOM')]}),
        ("text.move_select", {"type": 'HOME', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("text.move_select", {"type": 'END', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_END')]}),
        ("text.move_select", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("text.move_select", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("text.move_select", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("text.move_select", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("text.move_select", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_LINE')]}),
        ("text.move_select", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'NEXT_LINE')]}),
        ("text.move_select", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_PAGE')]}),
        ("text.move_select", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'NEXT_PAGE')]}),
        ("text.move_select", {"type": 'HOME', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'FILE_TOP')]}),
        ("text.move_select", {"type": 'END', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'FILE_BOTTOM')]}),
        ("text.delete", {"type": 'DEL', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("text.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("text.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("text.delete", {"type": 'DEL', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("text.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("text.overwrite_toggle", {"type": 'INSERT', "value": 'PRESS'}, None),
        ("text.scroll_bar", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("text.scroll_bar", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("text.scroll", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("text.scroll", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("text.selection_set", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
        ("text.cursor_set", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("text.selection_set", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True}, None),
        ("text.scroll", {"type": 'WHEELUPMOUSE', "value": 'PRESS'},
         {"properties": [("lines", -1)]}),
        ("text.scroll", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'},
         {"properties": [("lines", 1)]}),
        ("text.line_break", {"type": 'RET', "value": 'PRESS', "repeat": True}, None),
        ("text.line_break", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "repeat": True}, None),
        ("text.line_number", {"type": 'TEXTINPUT', "value": 'ANY', "any": True, "repeat": True}, None),
        op_menu("TEXT_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("text.insert", {"type": 'TEXTINPUT', "value": 'ANY', "any": True, "repeat": True}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (Sequencer)


def km_sequencer_generic(params):
    items = []
    keymap = (
        "Video Sequence Editor",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            params,
            toolbar_key={"type": 'T', "value": 'PRESS'},
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("wm.context_toggle", {"type": 'O', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "scene.sequence_editor.show_overlay_frame")]}),
        ("wm.context_toggle_enum", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.view_type"), ("value_1", 'SEQUENCER'), ("value_2", 'PREVIEW')]}),
        ("wm.context_toggle", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "tool_settings.use_snap_sequencer")]}),
        ("sequencer.refresh_all", {"type": 'E', "value": 'PRESS', "ctrl": True}, None),
    ])

    if params.select_mouse == 'LEFTMOUSE' and not params.legacy:
        # Quick switch to select tool, since left select can't easily
        # select with any tool active.
        items.extend([
            op_tool_cycle("builtin.select_box", {"type": 'W', "value": 'PRESS'}),
        ])

    return keymap


def km_sequencer(params):
    items = []
    keymap = (
        "Sequencer",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_sequencer_generic_select(
            type=params.select_mouse, value=params.select_mouse_value_fallback, legacy=params.legacy,
        ),
        ("sequencer.select", {"type": params.select_mouse, "value": 'PRESS', "ctrl": True},
         {"properties": [("linked_time", True)]}),
        ("sequencer.select", {"type": params.select_mouse, "value": 'PRESS', "ctrl": True, "shift": True},
         {"properties": [("linked_time", True), ("extend", True)]}),
        ("sequencer.select", {"type": params.select_mouse, "value": 'CLICK', "ctrl": True},
         {"properties": [("side_of_frame", True)]}),
        ("sequencer.select", {"type": params.select_mouse, "value": 'PRESS', "alt": True},
         {"properties": [("deselect_all", True), ("ignore_connections", True)]}),
        ("sequencer.select", {"type": params.select_mouse, "value": 'PRESS', "alt": True, "shift": True},
         {"properties": [("toggle", True), ("ignore_connections", True)]}),
        ("sequencer.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("sequencer.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("sequencer.select_linked_pick", {"type": 'L', "value": 'PRESS'}, None),
        ("sequencer.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("sequencer.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
         {"properties": [("tweak", True), ("mode", 'SET')]}),
        ("sequencer.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG', "shift": True},
         {"properties": [("tweak", True), ("mode", 'ADD')]}),
        ("sequencer.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("tweak", True), ("mode", 'SUB')]}),
        ("sequencer.select_box", {"type": params.select_mouse, "value": 'CLICK_DRAG', "alt": True},
         {"properties": [("tweak", True), ("ignore_connections", True), ("mode", 'SET')]}),
        ("sequencer.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("sequencer.select_box", {"type": 'B', "value": 'PRESS', "ctrl": True},
         {"properties": [("include_handles", True)]}),
        ("sequencer.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        ("sequencer.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        *_template_items_select_actions(params, "sequencer.select_all"),
        ("sequencer.split", {"type": 'K', "value": 'PRESS'},
         {"properties": [("type", 'SOFT')]}),
        ("sequencer.split", {"type": 'K', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'HARD')]}),
        ("sequencer.mute", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("sequencer.mute", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("sequencer.unmute", {"type": 'H', "value": 'PRESS', "alt": True},
         {"properties": [("unselected", False)]}),
        ("sequencer.unmute", {"type": 'H', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("unselected", True)]}),
        ("sequencer.lock", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.unlock", {"type": 'H', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("sequencer.connect", {"type": 'C', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("toggle", True)]}),
        ("sequencer.reassign_inputs", {"type": 'R', "value": 'PRESS'}, None),
        ("sequencer.reload", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("sequencer.reload", {"type": 'R', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("adjust_length", True)]}),
        ("sequencer.offset_clear", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("sequencer.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("sequencer.duplicate_move_linked", {"type": 'D', "value": 'PRESS', "alt": True}, None),
        ("sequencer.retiming_key_delete", {"type": 'X', "value": 'PRESS'}, None),
        ("sequencer.retiming_key_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("sequencer.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("sequencer.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("sequencer.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.paste", {"type": 'V', "value": 'PRESS', "ctrl": True, "shift": True},
         {"properties": [("keep_offset", True)]}),
        ("sequencer.images_separate", {"type": 'Y', "value": 'PRESS'}, None),
        ("sequencer.meta_toggle", {"type": 'TAB', "value": 'PRESS'}, None),
        ("sequencer.meta_make", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.meta_separate", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("sequencer.view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("sequencer.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("sequencer.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("sequencer.view_frame", {"type": 'NUMPAD_0', "value": 'PRESS'}, None),
        ("sequencer.strip_jump", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True},
         {"properties": [("next", False), ("center", False)]}),
        ("sequencer.strip_jump", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True},
         {"properties": [("next", True), ("center", False)]}),
        ("sequencer.strip_jump", {"type": 'PAGE_UP', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("next", False), ("center", True)]}),
        ("sequencer.strip_jump", {"type": 'PAGE_DOWN', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("next", True), ("center", True)]}),
        ("sequencer.swap", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("side", 'LEFT')]}),
        ("sequencer.swap", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("side", 'RIGHT')]}),
        ("sequencer.gap_remove", {"type": 'BACK_SPACE', "value": 'PRESS'},
         {"properties": [("all", False)]}),
        ("sequencer.gap_remove", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True},
         {"properties": [("all", True)]}),
        ("sequencer.gap_insert", {"type": 'EQUAL', "value": 'PRESS', "shift": True}, None),
        ("sequencer.snap", {"type": 'S', "value": 'PRESS', "shift": True}, None),
        ("sequencer.swap_inputs", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        *(
            (("sequencer.split_multicam",
              {"type": NUMBERS_1[i], "value": 'PRESS'},
              {"properties": [("camera", i + 1)]})
             for i in range(10)
             )
        ),
        op_menu("SEQUENCER_MT_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        op_menu("SEQUENCER_MT_change", {"type": 'C', "value": 'PRESS', "shift": True}),
        op_menu_pie("SEQUENCER_MT_view_pie", {"type": 'ACCENT_GRAVE', "value": 'PRESS'}),
        ("sequencer.slip", {"type": 'S', "value": 'PRESS'}, {"properties": [("use_cursor_position", False)]}),
        ("wm.context_set_int", {"type": 'O', "value": 'PRESS'},
         {"properties": [("data_path", "scene.sequence_editor.overlay_frame"), ("value", 0)]}),
        ("transform.seq_slide", {"type": 'G', "value": 'PRESS'},
         {"properties": [("view2d_edge_pan", True)]}),
        ("transform.seq_slide", {"type": params.select_mouse, "value": 'CLICK_DRAG'},
         {"properties": [("view2d_edge_pan", True), ("use_restore_handle_selection", True)]}),
        ("transform.seq_slide", {"type": params.select_mouse, "value": 'CLICK_DRAG', "alt": True},
         {"properties": [("view2d_edge_pan", True), ("use_restore_handle_selection", True)]}),
        ("transform.seq_slide", {"type": params.select_mouse, "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("view2d_edge_pan", True), ("use_restore_handle_selection", True)]}),
        ("transform.transform", {"type": 'E', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("sequencer.select_side_of_frame", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("side", 'LEFT')]}),
        ("sequencer.select_side_of_frame", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("side", 'RIGHT')]}),
        ("wm.context_toggle", {"type": 'Z', "value": 'PRESS', "alt": True, "shift": True},
         {"properties": [("data_path", "space_data.show_overlays")]}),
        *_template_items_context_menu("SEQUENCER_MT_context_menu", params.context_menu_event),
        op_menu("SEQUENCER_MT_retiming", {"type": 'I', "value": 'PRESS'}),
        ("sequencer.retiming_segment_speed_set", {"type": 'R', "value": 'PRESS'}, None),
        ("sequencer.retiming_show", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def _seq_preview_text_edit_cursor_move():
    items = []

    for ty, mod, prop in (
        ('LEFT_ARROW', None, ("type", 'PREVIOUS_CHARACTER')),
        ('RIGHT_ARROW', None, ("type", 'NEXT_CHARACTER')),
        ('UP_ARROW', None, ("type", 'PREVIOUS_LINE')),
        ('DOWN_ARROW', None, ("type", 'NEXT_LINE')),
        ('HOME', None, ("type", 'LINE_BEGIN')),
        ('END', None, ("type", 'LINE_END')),
        ('LEFT_ARROW', 'ctrl', ("type", 'PREVIOUS_WORD')),
        ('RIGHT_ARROW', 'ctrl', ("type", 'NEXT_WORD')),
        ('PAGE_UP', None, ("type", 'TEXT_BEGIN')),
        ('PAGE_DOWN', None, ("type", 'TEXT_END')),
    ):
        if mod is not None:
            items.append(
                ("sequencer.text_cursor_move",
                 {"type": ty, "value": 'PRESS', mod: True, "repeat": True},
                 {"properties": [prop]}))
            items.append(
                ("sequencer.text_cursor_move",
                 {"type": ty, "value": 'PRESS', mod: True, "shift": True, "repeat": True},
                 {"properties": [prop, ('select_text', True)]}))
        else:
            items.append(
                ("sequencer.text_cursor_move",
                 {"type": ty, "value": 'PRESS', "repeat": True},
                 {"properties": [prop]}))
            items.append(
                ("sequencer.text_cursor_move",
                 {"type": ty, "value": 'PRESS', "shift": True, "repeat": True},
                 {"properties": [prop, ('select_text', True)]}))
    return items


def km_sequencer_preview(params):
    items = []
    keymap = (
        "Preview",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Text editing.
        *_seq_preview_text_edit_cursor_move(),
        ("sequencer.text_delete", {"type": 'DEL', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_OR_SELECTION')]}),
        ("sequencer.text_delete", {"type": 'BACK_SPACE', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_OR_SELECTION')]}),
        ("sequencer.text_line_break", {"type": 'RET', "value": 'PRESS', "repeat": True}, None),
        ("sequencer.text_line_break", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "repeat": True}, None),
        ("sequencer.text_select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.text_deselect_all", {"type": 'ESC', "value": 'PRESS'}, None),
        ("sequencer.text_edit_mode_toggle", {"type": 'TAB', "value": 'PRESS'}, None),
        ("sequencer.text_edit_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.text_edit_paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.text_edit_cut", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.text_insert", {"type": 'TEXTINPUT', "value": 'ANY', "any": True, "repeat": True}, None),

        # Selection.
        *_template_sequencer_preview_select(
            type=params.select_mouse,
            value=params.select_mouse_value_fallback,
            legacy=params.legacy,
        ),
        *_template_items_select_actions(params, "sequencer.select_all"),
        ("sequencer.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("sequencer.select_circle", {"type": 'C', "value": 'PRESS'}, None),

        # View.
        ("sequencer.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("sequencer.view_all_preview", {"type": 'HOME', "value": 'PRESS'}, None),
        ("sequencer.view_all_preview", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("sequencer.view_ghost_border", {"type": 'O', "value": 'PRESS'}, None),
        ("sequencer.view_zoom_ratio", {"type": 'NUMPAD_8', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 8.0)]}),
        ("sequencer.view_zoom_ratio", {"type": 'NUMPAD_4', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 4.0)]}),
        ("sequencer.view_zoom_ratio", {"type": 'NUMPAD_2', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 2.0)]}),
        ("sequencer.view_zoom_ratio", {"type": 'NUMPAD_1', "value": 'PRESS'},
         {"properties": [("ratio", 1.0)]}),
        ("sequencer.view_zoom_ratio", {"type": 'NUMPAD_2', "value": 'PRESS'},
         {"properties": [("ratio", 0.5)]}),
        ("sequencer.view_zoom_ratio", {"type": 'NUMPAD_4', "value": 'PRESS'},
         {"properties": [("ratio", 0.25)]}),
        ("sequencer.view_zoom_ratio", {"type": 'NUMPAD_8', "value": 'PRESS'},
         {"properties": [("ratio", 0.125)]}),
        op_menu_pie("SEQUENCER_MT_preview_view_pie", {"type": 'ACCENT_GRAVE', "value": 'PRESS'}),

        # Transform Actions.
        *_template_items_transform_actions(params, use_mirror=True),
        ("transform.translate", {"type": 'PERIOD', "ctrl": True, "value": 'PRESS'},
         {"properties": [("translate_origin", True)]}),

        # Edit.
        ("sequencer.strip_transform_clear", {"type": 'G', "alt": True, "value": 'PRESS'},
         {"properties": [("property", 'POSITION')]}),
        ("sequencer.strip_transform_clear", {"type": 'S', "alt": True, "value": 'PRESS'},
         {"properties": [("property", 'SCALE')]}),
        ("sequencer.strip_transform_clear", {"type": 'R', "alt": True, "value": 'PRESS'},
         {"properties": [("property", 'ROTATION')]}),

        ("sequencer.preview_duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("sequencer.preview_duplicate_move_linked", {"type": 'D', "value": 'PRESS', "alt": True}, None),
        ("sequencer.mute", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("sequencer.mute", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("sequencer.unmute", {"type": 'H', "value": 'PRESS', "alt": True},
         {"properties": [("unselected", False)]}),
        ("sequencer.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("sequencer.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("sequencer.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("mode", 'ADD')]}),
        ("sequencer.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "shift": True, "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        ("sequencer.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.paste", {"type": 'V', "value": 'PRESS', "ctrl": True, "shift": True},
         {"properties": [("keep_offset", True)]}),

        # Animation
        ("anim.keying_set_active_set", {"type": 'K', "value": 'PRESS', "shift": True}, None),
        ("anim.keyframe_insert_menu", {"type": 'K', "value": 'PRESS'}, {"properties": [("always_prompt", True)]}),
        ("anim.keyframe_delete_vse", {"type": 'I', "value": 'PRESS', "alt": True}, None),

        *_template_items_context_menu("SEQUENCER_MT_preview_context_menu", params.context_menu_event),
    ])

    if params.use_pie_click_drag:
        items.extend([
            ("anim.keyframe_insert", {"type": 'I', "value": 'CLICK'}, None),
            op_menu_pie("ANIM_MT_keyframe_insert_pie", {"type": 'I', "value": 'CLICK_DRAG'}),
        ])
    else:
        items.extend([
            ("anim.keyframe_insert", {"type": 'I', "value": 'PRESS'}, None),
        ])

    if not params.legacy:
        # New pie menus.
        items.extend([
            ("wm.context_toggle", {"type": 'ACCENT_GRAVE', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", "space_data.show_gizmo")]}),
            op_menu_pie("SEQUENCER_MT_pivot_pie", {"type": 'PERIOD', "value": 'PRESS'}),
            ("wm.context_toggle", {"type": 'Z', "value": 'PRESS', "alt": True, "shift": True},
             {"properties": [("data_path", "space_data.show_overlays")]}),
        ])

    # 2D cursor.
    if params.cursor_tweak_event:
        items.extend([
            ("sequencer.cursor_set", params.cursor_set_event, None),
            ("transform.translate", params.cursor_tweak_event,
             {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        ])
    else:
        items.extend([
            ("sequencer.cursor_set", params.cursor_set_event, None),
        ])

    return keymap


def km_sequencer_channels(_params):
    items = []
    keymap = (
        "Sequencer Channels",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Rename.
        ("sequencer.rename_channel", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.rename_channel", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
    ])
    return keymap


# ------------------------------------------------------------------------------
# Editor (Console)

def km_console(_params):
    items = []
    keymap = (
        "Console",
        {"space_type": 'CONSOLE', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("console.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("console.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True, "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD'), ("select", True)]}),
        ("console.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("console.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True, "shift": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD'), ("select", True)]}),
        ("console.move", {"type": 'HOME', "value": 'PRESS'},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("console.move", {"type": 'HOME', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_BEGIN'), ("select", True)]}),
        ("console.move", {"type": 'END', "value": 'PRESS'},
         {"properties": [("type", 'LINE_END')]}),
        ("console.move", {"type": 'END', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_END'), ("select", True)]}),
        ("wm.context_cycle_int", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.font_size"), ("reverse", False)]}),
        ("wm.context_cycle_int", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.font_size"), ("reverse", True)]}),
        ("wm.context_cycle_int", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("data_path", "space_data.font_size"), ("reverse", False)]}),
        ("wm.context_cycle_int", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("data_path", "space_data.font_size"), ("reverse", True)]}),
        ("console.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("console.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "repeat": True, "shift": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER'), ("select", True)]}),
        ("console.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("console.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "repeat": True, "shift": True},
         {"properties": [("type", 'NEXT_CHARACTER'), ("select", True)]}),
        ("console.history_cycle", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("reverse", True)]}),
        ("console.history_cycle", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("reverse", False)]}),
        ("console.delete", {"type": 'DEL', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("console.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("console.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("console.delete", {"type": 'DEL', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("console.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("console.clear_line", {"type": 'RET', "value": 'PRESS', "shift": True}, None),
        ("console.clear_line", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "shift": True}, None),
        ("console.execute", {"type": 'RET', "value": 'PRESS'},
         {"properties": [("interactive", True)]}),
        ("console.execute", {"type": 'NUMPAD_ENTER', "value": 'PRESS'},
         {"properties": [("interactive", True)]}),
        ("console.copy_as_script", {"type": 'C', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("console.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("console.copy", {"type": 'X', "value": 'PRESS', "ctrl": True}, {"properties": [("delete", True)]}),
        ("console.paste", {"type": 'V', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("console.select_set", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("console.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("console.select_word", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("console.insert", {"type": 'TAB', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("text", '\t')]}),
        ("console.indent_or_autocomplete", {"type": 'TAB', "value": 'PRESS', "repeat": True}, None),
        ("console.unindent", {"type": 'TAB', "value": 'PRESS', "shift": True, "repeat": True}, None),
        *_template_items_context_menu("CONSOLE_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("console.insert", {"type": 'TEXTINPUT', "value": 'ANY', "any": True, "repeat": True}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (Clip)

def km_clip(params):
    items = []
    keymap = (
        "Clip",
        {"space_type": 'CLIP_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            params,
            toolbar_key={"type": 'T', "value": 'PRESS'},
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("clip.open", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("clip.track_markers", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("backwards", True), ("sequence", False)]}),
        ("clip.track_markers", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("backwards", False), ("sequence", False)]}),
        ("clip.track_markers", {"type": 'T', "value": 'PRESS', "ctrl": True},
         {"properties": [("backwards", False), ("sequence", True)]}),
        ("clip.track_markers", {"type": 'T', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("backwards", True), ("sequence", True)]}),
        ("wm.context_toggle_enum", {"type": 'TAB', "value": 'PRESS'},
         {"properties": [("data_path", "space_data.mode"), ("value_1", 'TRACKING'), ("value_2", 'MASK')]}),
        ("clip.prefetch", {"type": 'P', "value": 'PRESS'}, None),
        op_menu_pie("CLIP_MT_tracking_pie", {"type": 'E', "value": 'PRESS'}),
        op_menu_pie("CLIP_MT_solving_pie", {"type": 'S', "value": 'PRESS', "shift": True}),
        op_menu_pie("CLIP_MT_marker_pie", {"type": 'E', "value": 'PRESS', "shift": True}),
        op_menu_pie("CLIP_MT_reconstruction_pie", {"type": 'W', "value": 'PRESS', "shift": True}),
        op_menu_pie("CLIP_MT_view_pie", {"type": 'ACCENT_GRAVE', "value": 'PRESS'}),
    ])

    return keymap


def km_clip_editor(params):
    items = []
    keymap = (
        "Clip Editor",
        {"space_type": 'CLIP_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("clip.view_pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("clip.view_pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "shift": True}, None),
        ("clip.view_pan", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("clip.view_zoom", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("clip.view_zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("clip.view_zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("clip.view_zoom_in", {"type": 'WHEELINMOUSE', "value": 'PRESS'}, None),
        ("clip.view_zoom_out", {"type": 'WHEELOUTMOUSE', "value": 'PRESS'}, None),
        ("clip.view_zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True}, None),
        ("clip.view_zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True}, None),
        ("clip.view_zoom_ratio", {"type": 'NUMPAD_8', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 8.0)]}),
        ("clip.view_zoom_ratio", {"type": 'NUMPAD_4', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 4.0)]}),
        ("clip.view_zoom_ratio", {"type": 'NUMPAD_2', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 2.0)]}),
        ("clip.view_zoom_ratio", {"type": 'NUMPAD_8', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 8.0)]}),
        ("clip.view_zoom_ratio", {"type": 'NUMPAD_4', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 4.0)]}),
        ("clip.view_zoom_ratio", {"type": 'NUMPAD_2', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 2.0)]}),
        ("clip.view_zoom_ratio", {"type": 'NUMPAD_1', "value": 'PRESS'},
         {"properties": [("ratio", 1.0)]}),
        ("clip.view_zoom_ratio", {"type": 'NUMPAD_2', "value": 'PRESS'},
         {"properties": [("ratio", 0.5)]}),
        ("clip.view_zoom_ratio", {"type": 'NUMPAD_4', "value": 'PRESS'},
         {"properties": [("ratio", 0.25)]}),
        ("clip.view_zoom_ratio", {"type": 'NUMPAD_8', "value": 'PRESS'},
         {"properties": [("ratio", 0.125)]}),
        ("clip.view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("clip.view_all", {"type": 'F', "value": 'PRESS'},
         {"properties": [("fit_view", True)]}),
        ("clip.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("clip.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("clip.view_ndof", {"type": 'NDOF_MOTION', "value": 'ANY'}, None),
        ("clip.frame_jump", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("position", 'PATHSTART')]}),
        ("clip.frame_jump", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("position", 'PATHEND')]}),
        ("clip.frame_jump", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "alt": True, "repeat": True},
         {"properties": [("position", 'FAILEDPREV')]}),
        ("clip.frame_jump", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "alt": True, "repeat": True},
         {"properties": [("position", 'PATHSTART')]}),
        ("clip.change_frame", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("clip.select", {"type": params.select_mouse, "value": 'PRESS'},
         {"properties": [("deselect_all", not params.legacy)]}),
        ("clip.select", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        *_template_items_select_actions(params, "clip.select_all"),
        ("clip.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("clip.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        op_menu("CLIP_MT_select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}),
        ("clip.select_lasso", {"type": params.action_mouse, "value": 'CLICK_DRAG', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ADD')]}),
        ("clip.select_lasso",
         {"type": params.action_mouse, "value": 'CLICK_DRAG', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("mode", 'SUB')]}),
        ("clip.add_marker_slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("clip.delete_marker", {"type": 'X', "value": 'PRESS', "shift": True}, None),
        ("clip.delete_marker", {"type": 'DEL', "value": 'PRESS', "shift": True}, None),
        ("clip.slide_marker", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("clip.disable_markers", {"type": 'D', "value": 'PRESS', "shift": True},
         {"properties": [("action", 'TOGGLE')]}),
        ("clip.delete_track", {"type": 'X', "value": 'PRESS'}, None),
        ("clip.delete_track", {"type": 'DEL', "value": 'PRESS'}, None),
        ("clip.lock_tracks", {"type": 'L', "value": 'PRESS', "ctrl": True},
         {"properties": [("action", 'LOCK')]}),
        ("clip.lock_tracks", {"type": 'L', "value": 'PRESS', "alt": True},
         {"properties": [("action", 'UNLOCK')]}),
        *_template_items_hide_reveal_actions("clip.hide_tracks", "clip.hide_tracks_clear"),
        ("clip.slide_plane_marker", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
        ("clip.keyframe_insert", {"type": 'I', "value": 'PRESS'}, None),
        ("clip.keyframe_delete", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("clip.join_tracks", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        ("clip.lock_selection_toggle", {"type": 'L', "value": 'PRESS'}, None),
        ("wm.context_toggle", {"type": 'D', "value": 'PRESS', "alt": True},
         {"properties": [("data_path", "space_data.show_disabled")]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("data_path", "space_data.show_marker_search")]}),
        ("wm.context_toggle", {"type": 'M', "value": 'PRESS'},
         {"properties": [("data_path", "space_data.use_mute_footage")]}),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_mouse, "value": 'CLICK_DRAG'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "alt": True},
         {"properties": [("action", 'REMAINED'), ("clear_active", False)]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "shift": True},
         {"properties": [("action", 'UPTO'), ("clear_active", False)]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("action", 'ALL'), ("clear_active", False)]}),
        ("clip.cursor_set", params.cursor_set_event, None),
        ("clip.copy_tracks", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("clip.paste_tracks", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("wm.context_toggle", {"type": 'Z', "value": 'PRESS', "alt": True, "shift": True},
         {"properties": [("data_path", "space_data.overlay.show_overlays")]}),
        *_template_items_context_menu("CLIP_MT_tracking_context_menu", params.context_menu_event),
    ])

    if not params.legacy:
        items.extend([
            op_menu_pie("CLIP_MT_pivot_pie", {"type": 'PERIOD', "value": 'PRESS'}),
        ])
    else:
        items.extend([
            # Old pivot.
            ("wm.context_set_enum", {"type": 'COMMA', "value": 'PRESS'},
             {"properties": [("data_path", "space_data.pivot_point"), ("value", 'BOUNDING_BOX_CENTER')]}),
            ("wm.context_set_enum", {"type": 'COMMA', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", "space_data.pivot_point"), ("value", 'MEDIAN_POINT')]}),
            ("wm.context_set_enum", {"type": 'PERIOD', "value": 'PRESS'},
             {"properties": [("data_path", "space_data.pivot_point"), ("value", 'CURSOR')]}),
            ("wm.context_set_enum", {"type": 'PERIOD', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", "space_data.pivot_point"), ("value", 'INDIVIDUAL_ORIGINS')]}),

            ("clip.view_center_cursor", {"type": 'HOME', "value": 'PRESS', "alt": True}, None),
        ])

    return keymap


def km_clip_graph_editor(params):
    items = []
    keymap = (
        "Clip Graph Editor",
        {"space_type": 'CLIP_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("clip.graph_select", {"type": params.select_mouse, "value": 'PRESS'}, None),
        ("clip.graph_select", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        *_template_items_select_actions(params, "clip.graph_select_all_markers"),
        ("clip.graph_select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("clip.graph_delete_curve", {"type": 'X', "value": 'PRESS'}, None),
        ("clip.graph_delete_curve", {"type": 'DEL', "value": 'PRESS'}, None),
        ("clip.graph_delete_knot", {"type": 'X', "value": 'PRESS', "shift": True}, None),
        ("clip.graph_delete_knot", {"type": 'DEL', "value": 'PRESS', "shift": True}, None),
        ("clip.graph_view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("clip.graph_view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("clip.graph_center_current_frame", {"type": 'NUMPAD_0', "value": 'PRESS'}, None),
        ("wm.context_toggle", {"type": 'L', "value": 'PRESS'},
         {"properties": [("data_path", "space_data.lock_time_cursor")]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "alt": True},
         {"properties": [("action", 'REMAINED'), ("clear_active", True)]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "shift": True},
         {"properties": [("action", 'UPTO'), ("clear_active", True)]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("action", 'ALL'), ("clear_active", True)]}),
        ("clip.graph_disable_markers", {"type": 'D', "value": 'PRESS', "shift": True},
         {"properties": [("action", 'TOGGLE')]}),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_mouse, "value": 'CLICK_DRAG'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
    ])

    if params.select_mouse == 'LEFTMOUSE' and not params.legacy:
        items.extend([
            ("clip.change_frame", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True}, None),
        ])
    else:
        items.extend([
            ("clip.change_frame", {"type": params.action_mouse, "value": 'PRESS'}, None),
        ])

    return keymap


def km_clip_dopesheet_editor(_params):
    items = []
    keymap = (
        "Clip Dopesheet Editor",
        {"space_type": 'CLIP_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("clip.dopesheet_select_channel", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("extend", True)]}),
        ("clip.dopesheet_view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("clip.dopesheet_view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("clip.delete_track", {"type": 'X', "value": 'PRESS'}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editor (Spreadsheet)

def km_spreadsheet_generic(params):
    items = []
    keymap = (
        "Spreadsheet Generic",
        {"space_type": 'SPREADSHEET', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            params,
            sidebar_key={"type": 'N', "value": 'PRESS'},
            channels_key={"type": 'T', "value": 'PRESS'},
        ),
        ("spreadsheet.resize_column", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("spreadsheet.fit_column", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("spreadsheet.reorder_columns", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Animation

def km_frames(params):
    items = []
    keymap = (
        "Frames",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Frame offsets
        ("screen.frame_offset", {"type": 'LEFT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("delta", -1)]}),
        ("screen.frame_offset", {"type": 'RIGHT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("delta", 1)]}),
        ("screen.frame_jump", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("end", True)]}),
        ("screen.frame_jump", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("end", False)]}),
        ("screen.time_jump", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("backward", False)]}),
        ("screen.time_jump", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("backward", True)]}),
        ("screen.keyframe_jump", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("next", False)]}),
        ("screen.keyframe_jump", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("next", True)]}),
        ("screen.keyframe_jump", {"type": 'MEDIA_LAST', "value": 'PRESS'},
         {"properties": [("next", True)]}),
        ("screen.keyframe_jump", {"type": 'MEDIA_FIRST', "value": 'PRESS'},
         {"properties": [("next", False)]}),
        ("screen.frame_offset", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("delta", 1)]}),
        ("screen.frame_offset", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("delta", -1)]}),
    ])

    if not params.legacy:
        # New playback
        if params.spacebar_action in {'TOOL', 'SEARCH'}:
            items.append(
                ("screen.animation_play", {"type": 'SPACE', "value": 'PRESS', "shift": True}, None),
            )
        elif params.spacebar_action == 'PLAY':
            items.append(
                ("screen.animation_play", {"type": 'SPACE', "value": 'PRESS'}, None),
            )
        else:
            assert False, "unreachable"

        items.extend([
            ("screen.animation_play", {"type": 'SPACE', "value": 'PRESS', "shift": True, "ctrl": True},
             {"properties": [("reverse", True)]}),
        ])
    else:
        # Old playback
        items.extend([
            ("screen.frame_offset", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
             {"properties": [("delta", 10)]}),
            ("screen.frame_offset", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
             {"properties": [("delta", -10)]}),
            ("screen.frame_jump", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
             {"properties": [("end", True)]}),
            ("screen.frame_jump", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
             {"properties": [("end", False)]}),
            ("screen.animation_play", {"type": 'A', "value": 'PRESS', "alt": True}, None),
            ("screen.animation_play", {"type": 'A', "value": 'PRESS', "shift": True, "alt": True},
             {"properties": [("reverse", True)]}),
        ])

    items.extend([
        ("screen.animation_cancel", {"type": 'ESC', "value": 'PRESS'}, None),
        ("screen.animation_play", {"type": 'MEDIA_PLAY', "value": 'PRESS'}, None),
        ("screen.animation_cancel", {"type": 'MEDIA_STOP', "value": 'PRESS'}, None),
    ])

    return keymap


def km_animation(_params):
    items = []
    keymap = (
        "Animation",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Frame management.
        ("wm.context_toggle", {"type": 'T', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "space_data.show_seconds")]}),
        # Preview range.
        ("anim.previewrange_set", {"type": 'P', "value": 'PRESS'}, None),
        ("anim.previewrange_clear", {"type": 'P', "value": 'PRESS', "alt": True}, None),
        ("anim.start_frame_set", {"type": 'HOME', "value": 'PRESS', "ctrl": True}, None),
        ("anim.end_frame_set", {"type": 'END', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_animation_channels(params):
    items = []
    keymap = (
        "Animation Channels",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Click select.
        ("anim.channels_click", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("anim.channels_click", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("extend_range", True)]}),
        ("anim.channels_click", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True},
         {"properties": [("extend", True)]}),
        ("anim.channels_click", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("children_only", True)]}),
        # Rename.
        ("anim.channels_rename", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        # Select keys.
        ("anim.channel_select_keys", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("anim.channel_select_keys", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "shift": True},
         {"properties": [("extend", True)]}),
        # Find (setting the name filter).
        ("anim.channels_select_filter", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        # Selection.
        *_template_items_select_actions(params, "anim.channels_select_all"),
        ("anim.channels_select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("anim.channels_select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'},
         {"properties": [("extend", False)]}),
        ("anim.channels_select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "shift": True},
         {"properties": [("extend", True)]}),
        ("anim.channels_select_box", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG', "ctrl": True},
         {"properties": [("deselect", True)]}),
        # Delete.
        ("anim.channels_delete", {"type": 'X', "value": 'PRESS'}, None),
        ("anim.channels_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        # Settings.
        ("anim.channels_setting_toggle", {"type": 'W', "value": 'PRESS', "shift": True}, None),
        ("anim.channels_setting_enable", {"type": 'W', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("anim.channels_setting_disable", {"type": 'W', "value": 'PRESS', "alt": True}, None),
        ("anim.channels_editable_toggle", {"type": 'TAB', "value": 'PRESS'}, None),
        # Expand/collapse.
        ("anim.channels_expand", {"type": 'NUMPAD_PLUS', "value": 'PRESS'}, None),
        ("anim.channels_collapse", {"type": 'NUMPAD_MINUS', "value": 'PRESS'}, None),
        ("anim.channels_expand", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("all", False)]}),
        ("anim.channels_collapse", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("all", False)]}),
        # Move.
        ("anim.channels_move", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'UP')]}),
        ("anim.channels_move", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'DOWN')]}),
        ("anim.channels_move", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'TOP')]}),
        ("anim.channels_move", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'BOTTOM')]}),
        # Group.
        ("anim.channels_group", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("anim.channels_ungroup", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        # Menus.
        *_template_items_context_menu("DOPESHEET_MT_channel_context_menu", params.context_menu_event),
        # View
        ("anim.channel_view_pick", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("anim.channels_view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Object Grease Pencil Modes

def km_annotate(params):
    items = []
    keymap = (
        "Grease Pencil",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    if params.use_key_activate_tools:
        items.extend([
            op_tool_cycle("builtin.annotate", {"type": 'D', "value": 'PRESS'}),
        ])
    else:
        items.extend([
            # Draw
            ("gpencil.annotate",
             {"type": 'LEFTMOUSE', "value": 'PRESS', "key_modifier": 'D'},
             {"properties": [("mode", 'DRAW'), ("wait_for_input", False)]}),
            ("gpencil.annotate",
             {"type": 'LEFTMOUSE', "value": 'PRESS', "key_modifier": 'D', "shift": True},
             {"properties": [("mode", 'DRAW'), ("wait_for_input", False)]}),
            # Draw - straight lines
            ("gpencil.annotate",
             {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True, "key_modifier": 'D'},
             {"properties": [("mode", 'DRAW_STRAIGHT'), ("wait_for_input", False)]}),
            # Draw - poly lines
            ("gpencil.annotate",
             {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True, "key_modifier": 'D'},
             {"properties": [("mode", 'DRAW_POLY'), ("wait_for_input", False)]}),
            # Erase
            ("gpencil.annotate",
             {"type": 'RIGHTMOUSE', "value": 'PRESS', "key_modifier": 'D'},
             {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),
        ])

    return keymap


# Grease Pencil
def km_grease_pencil_selection(params):
    items = []
    keymap = (
        "Grease Pencil Selection",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Select All
        *_template_items_select_actions(params, "grease_pencil.select_all"),
        # Select linked
        ("grease_pencil.select_linked", {"type": 'L', "value": 'PRESS'}, None),
        ("grease_pencil.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        # Select more/less
        ("grease_pencil.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("grease_pencil.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        # Select Similar
        ("grease_pencil.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
    ])

    return keymap


def km_grease_pencil_paint_mode(params):
    items = []
    keymap = (
        "Grease Pencil Paint Mode",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Active material
        op_menu("VIEW3D_MT_greasepencil_material_active", {"type": 'U', "value": 'PRESS'}),
        # Active layer
        op_menu("GREASE_PENCIL_MT_layer_active", {"type": 'Y', "value": 'PRESS'}),

        # Show/hide
        *_template_items_hide_reveal_actions("grease_pencil.layer_hide", "grease_pencil.layer_reveal"),
        # Flip primary and secondary color
        ("paint.brush_colors_flip", {"type": 'X', "value": 'PRESS'}, None),
        ("paint.sample_color", {"type": 'X', "value": 'PRESS', "shift": True}, {"properties": [("merged", False)]}),


        # Isolate Layer
        ("grease_pencil.layer_isolate", {"type": 'NUMPAD_ASTERIX', "value": 'PRESS'}, None),

        # Keyframe Menu
        op_menu("VIEW3D_MT_edit_greasepencil_animation", {"type": 'I', "value": 'PRESS'}),

        # Insert Blank Keyframe
        ("grease_pencil.insert_blank_frame", {"type": 'I', "value": 'PRESS', "shift": True}, None),

        # Delete all active frames
        ("grease_pencil.delete_frame", {"type": 'DEL', "value": 'PRESS', "shift": True},
         {"properties": [("type", "ALL_FRAMES")]}),

        # Delete Animation menu
        op_menu("GREASE_PENCIL_MT_draw_delete", {"type": 'I', "value": 'PRESS', "alt": True}),

        # Merge Down
        ("grease_pencil.layer_merge", {"type": 'M', "value": 'PRESS',
         "ctrl": True, "shift": True}, {"properties": [("mode", 'ACTIVE')]}),

        op_tool_optional(
            ("grease_pencil.interpolate", {"type": 'E', "value": 'PRESS',
             "ctrl": True}, {"properties": [("use_selection", False)]}),
            (op_tool_cycle, "builtin.interpolate"), params),
        ("grease_pencil.interpolate_sequence", {"type": 'E', "value": 'PRESS',
         "shift": True, "ctrl": True}, {"properties": [("use_selection", False)]}),

        # Lasso/Box erase
        ("grease_pencil.erase_lasso", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("grease_pencil.erase_box", {"type": "B", "value": 'PRESS'}, {"properties": [("wait_for_input", True)]}),
        # Brush size
        ("wm.radial_control", {"type": 'F', "value": 'PRESS'},
         {"properties": [("data_path_primary", "tool_settings.gpencil_paint.brush.size")]}),
        # Brush strength
        ("wm.radial_control", {"type": 'F', "value": 'PRESS', "shift": True},
         {"properties": [("data_path_primary", "tool_settings.gpencil_paint.brush.strength")]}),

        *_template_asset_shelf_popup("VIEW3D_AST_brush_gpencil_paint", params.spacebar_action),

        *_template_items_context_panel("VIEW3D_PT_greasepencil_draw_context_menu", params.context_menu_event),
    ])

    return keymap


def km_grease_pencil_brush_stroke(_params):
    items = []
    keymap = (
        "Grease Pencil Brush Stroke",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("grease_pencil.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("grease_pencil.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'ERASE')]}),
        ("grease_pencil.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SMOOTH')]}),
        ("grease_pencil.brush_stroke", {"type": 'ERASER', "value": 'PRESS'},
         {"properties": [("mode", 'ERASE')]}),
        # Increase/Decrease brush size
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
    ])

    return keymap


def km_grease_pencil_edit_mode(params):
    items = []
    keymap = (
        "Grease Pencil Edit Mode",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Delete menu
        op_menu("VIEW3D_MT_edit_greasepencil_delete", {"type": 'X', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_greasepencil_delete", {"type": 'DEL', "value": 'PRESS'}),
        # Dissolve
        ("grease_pencil.dissolve", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("grease_pencil.dissolve", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        # Copy/paste
        ("grease_pencil.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("grease_pencil.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("grease_pencil.paste", {"type": 'V', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("paste_back", True)]}),
        # Snap
        op_menu_pie("GREASE_PENCIL_MT_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True}),
        # Separate
        ("grease_pencil.separate", {"type": 'P', "value": 'PRESS'}, None),
        # Delete all active frames
        ("grease_pencil.delete_frame", {"type": 'DEL', "value": 'PRESS', "shift": True},
         {"properties": [("type", "ALL_FRAMES")]}),
        # Keyframe Menu
        op_menu("VIEW3D_MT_edit_greasepencil_animation", {"type": 'I', "value": 'PRESS'}),

        # Insert Blank Keyframe
        ("grease_pencil.insert_blank_frame", {"type": 'I', "value": 'PRESS', "shift": True}, None),

        # Delete Animation menu
        op_menu("GREASE_PENCIL_MT_draw_delete", {"type": 'I', "value": 'PRESS', "alt": True}),

        # Show/hide
        *_template_items_hide_reveal_actions("grease_pencil.layer_hide", "grease_pencil.layer_reveal"),

        # Transform Actions.
        *_template_items_transform_actions(params, use_bend=True, use_mirror=True, use_tosphere=True, use_shear=True),
        ("transform.transform", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'CURVE_SHRINKFATTEN')]}),
        ("transform.transform", {"type": 'F', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'GPENCIL_OPACITY')]}),

        # Proportional editing.
        *_template_items_proportional_editing(
            params, connected=True, toggle_data_path="tool_settings.use_proportional_edit"),

        # Cyclical set
        ("grease_pencil.cyclical_set", {"type": 'F', "value": 'PRESS'},
         {"properties": [("type", "CLOSE"), ("subdivide_cyclic_segment", True)]}),
        ("grease_pencil.cyclical_set", {"type": 'C', "value": 'PRESS',
         "alt": True}, {"properties": [("type", "TOGGLE"), ("subdivide_cyclic_segment", False)]}),

        # Join selection
        ("grease_pencil.join_selection", {"type": 'J', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'JOINSTROKES')]}),
        ("grease_pencil.join_selection", {"type": 'J', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'SPLITCOPY')]}),

        ("grease_pencil.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),

        # Split Stroke
        ("grease_pencil.stroke_split", {"type": 'V', "value": 'PRESS', "shift": True}, None),

        # Extrude and move selected points
        op_tool_optional(
            ("grease_pencil.extrude_move", {"type": 'E', "value": 'PRESS'}, None),
            (op_tool_cycle, "builtin.extrude"), params),

        # Active layer
        op_menu("GREASE_PENCIL_MT_layer_active", {"type": 'Y', "value": 'PRESS'}),

        # Move to layer
        op_menu("GREASE_PENCIL_MT_move_to_layer", {"type": 'M', "value": 'PRESS'}),

        # Merge Down
        ("grease_pencil.layer_merge", {"type": 'M', "value": 'PRESS',
         "ctrl": True, "shift": True}, {"properties": [("mode", 'ACTIVE')]}),

        # Edit Lines overlay
        ("wm.context_toggle", {"type": 'Q', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'space_data.overlay.use_gpencil_edit_lines')]}),
        ("wm.context_toggle", {"type": 'Q', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("data_path", 'space_data.overlay.use_gpencil_multiedit_line_only')]}),

        # Context menu
        *_template_items_context_menu("VIEW3D_MT_greasepencil_edit_context_menu", params.context_menu_event),

        # Vertex Groups
        op_menu("VIEW3D_MT_greasepencil_vertex_group", {"type": 'G', "value": 'PRESS', "ctrl": True}),

        # Reorder
        ("grease_pencil.reorder", {"type": 'UP_ARROW', "value": 'PRESS',
         "ctrl": True, "shift": True}, {"properties": [("direction", "TOP")]}),
        ("grease_pencil.reorder", {"type": 'UP_ARROW', "value": 'PRESS',
         "ctrl": True, "repeat": True}, {"properties": [("direction", "UP")]}),
        ("grease_pencil.reorder", {"type": 'DOWN_ARROW', "value": 'PRESS',
         "ctrl": True, "repeat": True}, {"properties": [("direction", "DOWN")]}),
        ("grease_pencil.reorder", {"type": 'DOWN_ARROW', "value": 'PRESS',
         "ctrl": True, "shift": True}, {"properties": [("direction", "BOTTOM")]}),

        # Isolate Layer
        ("grease_pencil.layer_isolate", {"type": 'NUMPAD_ASTERIX', "value": 'PRESS'}, None),

        # Select mode
        ("grease_pencil.set_selection_mode", {"type": 'ONE', "value": 'PRESS'}, {"properties": [("mode", 'POINT')]}),
        ("grease_pencil.set_selection_mode", {"type": 'TWO', "value": 'PRESS'}, {"properties": [("mode", 'STROKE')]}),
        ("grease_pencil.set_selection_mode", {"type": 'THREE',
         "value": 'PRESS'}, {"properties": [("mode", 'SEGMENT')]}),

        # Set Handle Type
        ("grease_pencil.set_handle_type", {"type": 'V', "value": 'PRESS'}, None),

        op_tool_optional(
            ("grease_pencil.interpolate", {"type": 'E', "value": 'PRESS',
             "ctrl": True}, {"properties": [("use_selection", True)]}),
            (op_tool_cycle, "builtin.interpolate"), params),
        ("grease_pencil.interpolate_sequence", {"type": 'E', "value": 'PRESS',
         "shift": True, "ctrl": True}, {"properties": [("use_selection", True)]}),
    ])

    return keymap


def km_grease_pencil_sculpt_mode(params):
    items = []
    keymap = (
        "Grease Pencil Sculpt Mode",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items}
    )

    items.extend([
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        # Invoke sculpt operator
        ("grease_pencil.sculpt_paint", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("grease_pencil.sculpt_paint", {"type": 'LEFTMOUSE', "value": 'PRESS',
         "ctrl": True}, {"properties": [("mode", 'INVERT')]}),
        ("grease_pencil.sculpt_paint", {"type": 'LEFTMOUSE', "value": 'PRESS',
         "shift": True}, {"properties": [("mode", 'SMOOTH')]}),
        # Selection mode
        ("wm.context_toggle", {"type": 'ONE', "value": 'PRESS'},
         {"properties": [("data_path", "scene.tool_settings.use_gpencil_select_mask_point")]}),
        ("wm.context_toggle", {"type": 'TWO', "value": 'PRESS'},
         {"properties": [("data_path", "scene.tool_settings.use_gpencil_select_mask_stroke")]}),
        ("wm.context_toggle", {"type": 'THREE', "value": 'PRESS'},
         {"properties": [("data_path", "scene.tool_settings.use_gpencil_select_mask_segment")]}),

        # Edit Lines overlay
        ("wm.context_toggle", {"type": 'Q', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'space_data.overlay.use_gpencil_edit_lines')]}),
        ("wm.context_toggle", {"type": 'Q', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("data_path", 'space_data.overlay.use_gpencil_multiedit_line_only')]}),

        # Keyframe Menu
        op_menu("VIEW3D_MT_edit_greasepencil_animation", {"type": 'I', "value": 'PRESS'}),

        # Insert Blank Keyframe
        ("grease_pencil.insert_blank_frame", {"type": 'I', "value": 'PRESS', "shift": True}, None),

        # Delete Animation menu
        op_menu("GREASE_PENCIL_MT_draw_delete", {"type": 'I', "value": 'PRESS', "alt": True}),

        # Delete all active frames
        ("grease_pencil.delete_frame", {"type": 'DEL', "value": 'PRESS', "shift": True},
         {"properties": [("type", "ALL_FRAMES")]}),

        # Merge Down
        ("grease_pencil.layer_merge", {"type": 'M', "value": 'PRESS',
         "ctrl": True, "shift": True}, {"properties": [("mode", 'ACTIVE')]}),

        # Copy/paste
        ("grease_pencil.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("grease_pencil.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("grease_pencil.paste", {"type": 'V', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("paste_back", True)]}),

        # Active material
        op_menu("VIEW3D_MT_greasepencil_material_active", {"type": 'U', "value": 'PRESS'}),

        # Active layer
        op_menu("GREASE_PENCIL_MT_layer_active", {"type": 'Y', "value": 'PRESS'}),

        # Auto-masking menu.
        op_menu_pie(
            "VIEW3D_MT_grease_pencil_sculpt_automasking_pie",
            {"type": 'A', "value": 'PRESS', "shift": True, "alt": True},
        ),
        ("wm.context_menu_enum", {"type": 'E', "value": 'PRESS', "alt": True},
         {"properties": [("data_path", "tool_settings.gpencil_sculpt_paint.brush.stroke_method")]}),

        *_template_paint_radial_control("gpencil_sculpt_paint"),
        *_template_asset_shelf_popup("VIEW3D_AST_brush_gpencil_sculpt", params.spacebar_action),
        *_template_items_context_panel("VIEW3D_PT_greasepencil_sculpt_context_menu", params.context_menu_event),
    ])

    return keymap


def km_grease_pencil_weight_paint(params):
    # NOTE: This keymap falls through to "Pose" when an armature modifying the GP object
    # is selected in weight paint mode. When editing the key-map take care that pose operations
    # (such as transforming bones) is not impacted.
    items = []
    keymap = (
        "Grease Pencil Weight Paint",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Paint weight
        ("grease_pencil.weight_brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("grease_pencil.weight_brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        # Increase/Decrease brush size
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        # Radial controls
        *_template_paint_radial_control("gpencil_weight_paint"),
        ("wm.radial_control", {"type": 'F', "value": 'PRESS', "ctrl": True},
         radial_control_properties("gpencil_weight_paint", "weight", "use_unified_weight")),
        # Toggle Add/Subtract for weight draw tool
        ("grease_pencil.weight_toggle_direction", {"type": 'D', "value": 'PRESS'}, None),

        # Edit Lines overlay
        ("wm.context_toggle", {"type": 'Q', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'space_data.overlay.use_gpencil_edit_lines')]}),
        ("wm.context_toggle", {"type": 'Q', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("data_path", 'space_data.overlay.use_gpencil_multiedit_line_only')]}),

        # Active layer
        op_menu("GREASE_PENCIL_MT_layer_active", {"type": 'Y', "value": 'PRESS'}),

        # Merge Down
        ("grease_pencil.layer_merge", {"type": 'M', "value": 'PRESS',
         "ctrl": True, "shift": True}, {"properties": [("mode", 'ACTIVE')]}),

        # Keyframe Menu
        op_menu("VIEW3D_MT_edit_greasepencil_animation", {"type": 'I', "value": 'PRESS'}),

        # Insert Blank Keyframe
        ("grease_pencil.insert_blank_frame", {"type": 'I', "value": 'PRESS', "shift": True}, None),

        # Delete Animation menu
        op_menu("GREASE_PENCIL_MT_draw_delete", {"type": 'I', "value": 'PRESS', "alt": True}),

        # Delete all active frames
        ("grease_pencil.delete_frame", {"type": 'DEL', "value": 'PRESS', "shift": True},
         {"properties": [("type", "ALL_FRAMES")]}),

        # Sample weight
        ("grease_pencil.weight_sample", {"type": 'X', "value": 'PRESS', "shift": True}, None),
        # Context menu
        *_template_items_context_panel("VIEW3D_PT_greasepencil_weight_context_menu", params.context_menu_event),

        # Show/hide layer
        *_template_items_hide_reveal_actions("grease_pencil.layer_hide", "grease_pencil.layer_reveal"),

        *_template_asset_shelf_popup("VIEW3D_AST_brush_gpencil_weight", params.spacebar_action),
    ])

    if params.select_mouse == 'LEFTMOUSE':
        # Bone selection for combined weight paint + pose mode (Alt).
        items.extend([
            ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
            ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
             {"properties": [("toggle", True)]}),

            # Ctrl-Shift-LMB is needed for MMB emulation (which conflicts with Alt).
            # NOTE: this works reasonably well for pose-mode where typically selecting a single bone is sufficient.
            # For selecting faces/vertices, this is less useful. Selection tools are needed in this case.
            ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True, "shift": True}, None),
        ])

    return keymap


def km_grease_pencil_vertex_paint(params):
    items = []
    keymap = (
        "Grease Pencil Vertex Paint",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items}
    )

    items.extend([
        # Paint vertex
        ("grease_pencil.vertex_brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("grease_pencil.vertex_brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        # Increase/Decrease brush size
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        # Selection mode
        ("wm.context_toggle", {"type": 'ONE', "value": 'PRESS'},
         {"properties": [("data_path", "scene.tool_settings.use_gpencil_vertex_select_mask_point")]}),
        ("wm.context_toggle", {"type": 'TWO', "value": 'PRESS'},
         {"properties": [("data_path", "scene.tool_settings.use_gpencil_vertex_select_mask_stroke")]}),
        ("wm.context_toggle", {"type": 'THREE', "value": 'PRESS'},
         {"properties": [("data_path", "scene.tool_settings.use_gpencil_vertex_select_mask_segment")]}),
        # Flip primary and secondary color
        ("paint.brush_colors_flip", {"type": 'X', "value": 'PRESS'}, None),
        ("paint.sample_color", {"type": 'X', "value": 'PRESS', "shift": True}, {"properties": [("merged", False)]}),

        # Edit Lines overlay
        ("wm.context_toggle", {"type": 'Q', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'space_data.overlay.use_gpencil_edit_lines')]}),
        ("wm.context_toggle", {"type": 'Q', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("data_path", 'space_data.overlay.use_gpencil_multiedit_line_only')]}),

        # Active layer
        op_menu("GREASE_PENCIL_MT_layer_active", {"type": 'Y', "value": 'PRESS'}),

        # Merge Down
        ("grease_pencil.layer_merge", {"type": 'M', "value": 'PRESS',
         "ctrl": True, "shift": True}, {"properties": [("mode", 'ACTIVE')]}),

        # Keyframe Menu
        op_menu("VIEW3D_MT_edit_greasepencil_animation", {"type": 'I', "value": 'PRESS'}),

        # Insert Blank Keyframe
        ("grease_pencil.insert_blank_frame", {"type": 'I', "value": 'PRESS', "shift": True}, None),

        # Delete Animation menu
        op_menu("GREASE_PENCIL_MT_draw_delete", {"type": 'I', "value": 'PRESS', "alt": True}),

        # Delete all active frames
        ("grease_pencil.delete_frame", {"type": 'DEL', "value": 'PRESS', "shift": True},
         {"properties": [("type", "ALL_FRAMES")]}),

        # Radial controls
        *_template_paint_radial_control("gpencil_vertex_paint"),
        # Context menu
        *_template_items_context_panel("VIEW3D_PT_greasepencil_vertex_paint_context_menu", params.context_menu_event),

        *_template_asset_shelf_popup("VIEW3D_AST_brush_gpencil_vertex", params.spacebar_action),
    ])

    return keymap

# Grease Pencil Fill Tool.


def km_grease_pencil_fill_tool(_params):
    items = []
    keymap = (
        "Grease Pencil Fill Tool",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Fill operator.
        ("grease_pencil.fill", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         None),
        ("grease_pencil.fill", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("invert", True)]}),
        # Use regular stroke operator when holding alt to draw fill guides.
        ("grease_pencil.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
         None),
    ])

    return keymap


def km_grease_pencil_fill_tool_modal_map(_params):
    items = []
    keymap = (
        "Fill Tool Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("EXTENSION_MODE_TOGGLE", {"type": 'S', "value": 'PRESS'}, None),
        ("EXTENSION_LENGTHEN", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True}, None),
        ("EXTENSION_LENGTHEN", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("EXTENSION_SHORTEN", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True}, None),
        ("EXTENSION_SHORTEN", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("EXTENSION_DRAG", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("EXTENSION_COLLIDE", {"type": 'D', "value": 'PRESS'}, None),
        ("INVERT", {"type": 'LEFT_CTRL', "value": 'ANY', "any": True}, None),
        ("INVERT", {"type": 'RIGHT_CTRL', "value": 'ANY', "any": True}, None),
        ("PRECISION", {"type": 'LEFT_SHIFT', "value": 'ANY', "any": True}, None),
        ("PRECISION", {"type": 'RIGHT_SHIFT', "value": 'ANY', "any": True}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Object/Pose Modes

def km_object_mode(params):
    items = []
    keymap = (
        "Object Mode",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_items_proportional_editing(
            params, connected=False, toggle_data_path="tool_settings.use_proportional_edit_objects"),
        *_template_items_select_actions(params, "object.select_all"),
        ("object.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("object.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("object.select_linked", {"type": 'L', "value": 'PRESS', "shift": True}, None),
        ("object.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("object.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'PARENT'), ("extend", False)]}),
        ("object.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'PARENT'), ("extend", True)]}),
        ("object.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'CHILD'), ("extend", False)]}),
        ("object.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'CHILD'), ("extend", True)]}),
        ("object.parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        ("object.parent_clear", {"type": 'P', "value": 'PRESS', "alt": True}, None),
        # Transform Actions.
        *_template_items_transform_actions(params, use_mirror=True),
        ("object.transform_axis_target", {"type": 'T', "value": 'PRESS', "shift": True}, None),
        ("object.location_clear", {"type": 'G', "value": 'PRESS', "alt": True},
         {"properties": [("clear_delta", False)]}),
        ("object.rotation_clear", {"type": 'R', "value": 'PRESS', "alt": True},
         {"properties": [("clear_delta", False)]}),
        ("object.scale_clear", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("clear_delta", False)]}),
        ("object.delete", {"type": 'X', "value": 'PRESS'},
         {"properties": [("use_global", False)]}),
        ("object.delete", {"type": 'X', "value": 'PRESS', "shift": True},
         {"properties": [("use_global", True)]}),
        ("object.delete", {"type": 'DEL', "value": 'PRESS'},
         {"properties": [("use_global", False), ("confirm", False)]}),
        ("object.delete", {"type": 'DEL', "value": 'PRESS', "shift": True},
         {"properties": [("use_global", True), ("confirm", False)]}),
        op_menu("VIEW3D_MT_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        op_menu("VIEW3D_MT_object_apply", {"type": 'A', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_make_links", {"type": 'L', "value": 'PRESS', "ctrl": True}),
        ("object.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("object.duplicate_move_linked", {"type": 'D', "value": 'PRESS', "alt": True}, None),
        ("object.join", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        ("wm.context_toggle", {"type": 'PERIOD', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", "tool_settings.use_transform_data_origin")]}),
        ("anim.keyframe_insert_menu", {"type": 'K', "value": 'PRESS'}, {"properties": [("always_prompt", True)]}),
        ("anim.keyframe_delete_v3d", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("anim.keying_set_active_set", {"type": 'K', "value": 'PRESS', "shift": True}, None),
        ("collection.create", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("collection.objects_remove", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("collection.objects_remove_all",
         {"type": 'G', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ("collection.objects_add_active",
         {"type": 'G', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("collection.objects_remove_active", {"type": 'G', "value": 'PRESS', "shift": True, "alt": True}, None),
        *_template_items_object_subdivision_set(),
        op_menu("OBJECT_MT_move_to_collection", {"type": 'M', "value": 'PRESS'}),
        op_menu("OBJECT_MT_link_to_collection", {"type": 'M', "value": 'PRESS', "shift": True}),
        *_template_items_hide_reveal_actions("object.hide_view_set", "object.hide_view_clear"),
        ("object.hide_collection", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
        *_template_items_context_menu("VIEW3D_MT_object_context_menu", params.context_menu_event),
    ])

    if params.use_pie_click_drag:
        items.extend([
            ("anim.keyframe_insert", {"type": 'I', "value": 'CLICK'}, None),
            op_menu_pie("ANIM_MT_keyframe_insert_pie", {"type": 'I', "value": 'CLICK_DRAG'}),
        ])
    else:
        items.extend([
            ("anim.keyframe_insert", {"type": 'I', "value": 'PRESS'}, None),
        ])

    if params.legacy:
        items.extend([
            ("object.select_mirror", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True}, None),
            ("object.parent_no_inverse_set", {"type": 'P', "value": 'PRESS', "shift": True, "ctrl": True}, None),
            ("object.track_set", {"type": 'T', "value": 'PRESS', "ctrl": True}, None),
            ("object.track_clear", {"type": 'T', "value": 'PRESS', "alt": True}, None),
            ("object.constraint_add_with_targets", {"type": 'C', "value": 'PRESS', "shift": True, "ctrl": True}, None),
            ("object.constraints_clear", {"type": 'C', "value": 'PRESS', "ctrl": True, "alt": True}, None),
            ("object.origin_clear", {"type": 'O', "value": 'PRESS', "alt": True}, None),
            ("object.duplicates_make_real", {"type": 'A', "value": 'PRESS', "shift": True, "ctrl": True}, None),
            op_menu("VIEW3D_MT_make_single_user", {"type": 'U', "value": 'PRESS'}),
            ("object.convert", {"type": 'C', "value": 'PRESS', "alt": True}, None),
            ("object.make_local", {"type": 'L', "value": 'PRESS'}, None),
            ("object.data_transfer", {"type": 'T', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ])

    return keymap


def km_object_non_modal(params):
    items = []
    keymap = (
        "Object Non-modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    if params.legacy:
        items.extend([
            ("object.mode_set", {"type": 'TAB', "value": 'PRESS'},
             {"properties": [("mode", 'EDIT'), ("toggle", True)]}),
            ("object.mode_set", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
             {"properties": [("mode", 'POSE'), ("toggle", True)]}),
            ("object.mode_set", {"type": 'V', "value": 'PRESS'},
             {"properties": [("mode", 'VERTEX_PAINT'), ("toggle", True)]}),
            ("object.mode_set", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
             {"properties": [("mode", 'WEIGHT_PAINT'), ("toggle", True)]}),

            ("object.origin_set", {"type": 'C', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ])
    else:
        items.extend([
            # NOTE: this shortcut (while not temporary) is not ideal, see: #89757.
            ("object.transfer_mode", {"type": 'Q', "value": 'PRESS', "alt": True}, None),
        ])

        if params.use_pie_click_drag:
            items.extend([
                ("object.mode_set", {"type": 'TAB', "value": 'CLICK'},
                 {"properties": [("mode", 'EDIT'), ("toggle", True)]}),
                op_menu_pie("VIEW3D_MT_object_mode_pie", {"type": 'TAB', "value": 'CLICK_DRAG'}),
                ("view3d.object_mode_pie_or_toggle", {"type": 'TAB', "value": 'PRESS', "ctrl": True}, None),
            ])
        elif params.use_v3d_tab_menu:
            # Swap Tab/Ctrl-Tab
            items.extend([
                ("object.mode_set", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
                 {"properties": [("mode", 'EDIT'), ("toggle", True)]}),
                op_menu_pie("VIEW3D_MT_object_mode_pie", {"type": 'TAB', "value": 'PRESS'}),
            ])
        else:
            items.extend([
                ("object.mode_set", {"type": 'TAB', "value": 'PRESS'},
                 {"properties": [("mode", 'EDIT'), ("toggle", True)]}),
                ("view3d.object_mode_pie_or_toggle", {"type": 'TAB', "value": 'PRESS', "ctrl": True}, None),
            ])

    return keymap


def km_pose(params):
    items = []
    keymap = (
        "Pose",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Transform Actions.
        *_template_items_transform_actions(params, use_mirror=True),

        ("object.parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        *_template_items_hide_reveal_actions("pose.hide", "pose.reveal"),
        op_menu("VIEW3D_MT_pose_apply", {"type": 'A', "value": 'PRESS', "ctrl": True}),
        ("pose.rot_clear", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("pose.loc_clear", {"type": 'G', "value": 'PRESS', "alt": True}, None),
        ("pose.scale_clear", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("pose.quaternions_flip", {"type": 'F', "value": 'PRESS', "alt": True}, None),
        ("pose.rotation_mode_set", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("pose.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("pose.paste", {"type": 'V', "value": 'PRESS', "ctrl": True},
         {"properties": [("flipped", False)]}),
        ("pose.paste", {"type": 'V', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("flipped", True)]}),
        *_template_items_select_actions(params, "pose.select_all"),
        ("pose.select_parent", {"type": 'P', "value": 'PRESS', "shift": True}, None),
        ("pose.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'PARENT'), ("extend", False)]}),
        ("pose.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'PARENT'), ("extend", True)]}),
        ("pose.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("direction", 'CHILD'), ("extend", False)]}),
        ("pose.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("direction", 'CHILD'), ("extend", True)]}),
        ("pose.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("pose.select_linked_pick", {"type": 'L', "value": 'PRESS'}, None),
        ("pose.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("pose.select_mirror", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("pose.constraint_add_with_targets", {"type": 'C', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("pose.constraints_clear", {"type": 'C', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("pose.ik_add", {"type": 'I', "value": 'PRESS', "shift": True}, None),
        ("pose.ik_clear", {"type": 'I', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        op_menu("VIEW3D_MT_bone_options_toggle", {"type": 'W', "value": 'PRESS', "shift": True}),
        op_menu("VIEW3D_MT_bone_options_enable", {"type": 'W', "value": 'PRESS', "shift": True, "ctrl": True}),
        op_menu("VIEW3D_MT_bone_options_disable", {"type": 'W', "value": 'PRESS', "alt": True}),
        ("armature.assign_to_collection", {"type": 'M', "value": 'PRESS', "shift": True}, None),
        ("armature.move_to_collection", {"type": 'M', "value": 'PRESS'}, None),
        ("transform.bbone_resize", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ("anim.keyframe_insert_menu", {"type": 'K', "value": 'PRESS'}, {"properties": [("always_prompt", True)]}),
        ("anim.keyframe_delete_v3d", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("anim.keying_set_active_set", {"type": 'K', "value": 'PRESS', "shift": True}, None),
        ("pose.push", {"type": 'E', "value": 'PRESS', "ctrl": True}, None),
        ("pose.relax", {"type": 'E', "value": 'PRESS', "alt": True}, None),
        ("pose.breakdown", {"type": 'E', "value": 'PRESS', "shift": True}, None),
        ("pose.blend_to_neighbor", {"type": 'E', "value": 'PRESS', "shift": True, "alt": True}, None),
        op_menu("VIEW3D_MT_pose_propagate", {"type": 'P', "value": 'PRESS', "alt": True}),
        *_template_items_context_menu("VIEW3D_MT_pose_context_menu", params.context_menu_event),
        op_menu("POSE_MT_selection_sets_select", {"type": 'W', "value": 'PRESS', "shift": True, "alt": True}),
    ])

    if params.use_pie_click_drag:
        items.extend([
            ("anim.keyframe_insert", {"type": 'I', "value": 'CLICK'}, None),
            op_menu_pie("ANIM_MT_keyframe_insert_pie", {"type": 'I', "value": 'CLICK_DRAG'}),
        ])
    else:
        items.extend([
            ("anim.keyframe_insert", {"type": 'I', "value": 'PRESS'}, None),
        ])

    return keymap


# ------------------------------------------------------------------------------
# Object Paint Modes

def km_paint_curve(params):
    items = []
    keymap = (
        "Paint Curve",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("paintcurve.add_point_slide", {"type": params.action_mouse, "value": 'PRESS', "ctrl": True}, None),
        ("paintcurve.select", {"type": params.select_mouse, "value": 'PRESS'}, None),
        ("paintcurve.select", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("paintcurve.slide", {"type": params.action_mouse, "value": 'PRESS'},
         {"properties": [("align", False)]}),
        ("paintcurve.slide", {"type": params.action_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("align", True)]}),
        ("paintcurve.select", {"type": 'A', "value": 'PRESS'},
         {"properties": [("toggle", True)]}),
        ("paintcurve.cursor", {"type": params.action_mouse, "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("paintcurve.delete_point", {"type": 'X', "value": 'PRESS'}, None),
        ("paintcurve.delete_point", {"type": 'DEL', "value": 'PRESS'}, None),
        ("paintcurve.draw", {"type": 'RET', "value": 'PRESS'}, None),
        ("paintcurve.draw", {"type": 'NUMPAD_ENTER', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_mouse, "value": 'CLICK_DRAG'}, None),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
    ])

    return keymap


# Radial control setup helpers, this operator has a lot of properties.


def radial_control_properties(paint, prop, secondary_prop, secondary_rotation=False, color=False, zoom=False):
    brush_path = "tool_settings." + paint + ".brush"
    unified_path = "tool_settings." + paint + ".unified_paint_settings"
    rotation = "mask_texture_slot.angle" if secondary_rotation else "texture_slot.angle"
    return {
        "properties": [
            ("data_path_primary", brush_path + '.' + prop),
            ("data_path_secondary", unified_path + '.' + prop if secondary_prop else ''),
            ("use_secondary", unified_path + '.' + secondary_prop if secondary_prop else ''),
            ("rotation_path", brush_path + '.' + rotation),
            ("color_path", brush_path + '.cursor_color_add'),
            ("fill_color_path", brush_path + '.color' if color else ''),
            ("fill_color_override_path", unified_path + '.color' if color else ''),
            ("fill_color_override_test_path", unified_path + '.use_unified_color' if color else ''),
            ("zoom_path", "space_data.zoom" if zoom else ''),
            ("image_id", brush_path + ''),
            ("secondary_tex", secondary_rotation),
        ],
    }

# Radial controls for the paint and sculpt modes.


def _template_paint_radial_control(paint, rotation=False, secondary_rotation=False, color=False, zoom=False):
    items = []

    items.extend([
        ("wm.radial_control", {"type": 'F', "value": 'PRESS'},
         radial_control_properties(
             paint, "size", "use_unified_size", secondary_rotation=secondary_rotation, color=color, zoom=zoom)),
        ("wm.radial_control", {"type": 'F', "value": 'PRESS', "shift": True},
         radial_control_properties(
             paint, "strength", "use_unified_strength", secondary_rotation=secondary_rotation, color=color)),
    ])

    if rotation:
        items.extend([
            ("wm.radial_control", {"type": 'F', "value": 'PRESS', "ctrl": True},
             radial_control_properties(paint, "texture_slot.angle", None, color=color)),
        ])

    if secondary_rotation:
        items.extend([
            ("wm.radial_control", {"type": 'F', "value": 'PRESS', "ctrl": True, "alt": True},
             radial_control_properties(
                 paint, "mask_texture_slot.angle", None, secondary_rotation=secondary_rotation, color=color)),
        ])

    return items


def _template_view3d_select(*, type, value, legacy, select_passthrough, exclude_mod=None):
    # NOTE: `exclude_mod` is needed since we don't want this tool to exclude Control-RMB actions when this is used
    # as a tool key-map with RMB-select and `use_fallback_tool` is enabled with RMB select. See #92467.

    # See: `use_tweak_select_passthrough` doc-string.
    if select_passthrough and (value in {'CLICK', 'RELEASE'}):
        select_passthrough = False

    items = [(
        "view3d.select",
        {"type": type, "value": value, **{m: True for m in mods}},
        {"properties": [(c, True) for c in props]},
    ) for props, mods in (
        ((("deselect_all", "select_passthrough") if select_passthrough else
          ("deselect_all",)) if not legacy else (), ()),
        (("toggle",), ("shift",)),
        (("center", "object"), ("ctrl",)),
        (("enumerate",), ("alt",)),
        (("toggle", "center"), ("shift", "ctrl")),
        (("center", "enumerate"), ("ctrl", "alt")),
        (("toggle", "enumerate"), ("shift", "alt")),
        (("toggle", "center", "enumerate"), ("shift", "ctrl", "alt")),
    ) if exclude_mod is None or exclude_mod not in mods]

    if select_passthrough:
        # Add an additional click item to de-select all other items,
        # needed so pass-through is able to de-select other items.
        items.append((
            "view3d.select",
            {"type": type, "value": 'CLICK'},
            {"properties": [
                (c, True)
                for c in ("deselect_all",)
            ]},
        ))

    return items


def _template_view3d_paint_mask_select_loop(params):
    # NOTE: loop select is isolate so it can optionally be in the tool-select map,
    # so that with LMB select, Alt-LMB can be used for selection picking.
    # While the selection tool can still use Alt-LMB for loop selection.
    return [
        ("paint.face_select_loop",
         {"type": params.select_mouse, "value": 'PRESS', "alt": True},
         {"properties": [("extend", False), ("select", True)]}),
        ("paint.face_select_loop",
         {"type": params.select_mouse, "value": 'PRESS', "alt": True, "shift": True},
         {"properties": [("extend", True), ("select", True)]}),
        ("paint.face_select_loop",
         {"type": params.select_mouse, "value": 'PRESS', "alt": True, "shift": True, "ctrl": True},
         {"properties": [("extend", True), ("select", False)]}),
    ]


def _template_node_select(*, type, value, select_passthrough):
    items = [
        ("node.select", {"type": type, "value": value},
         {"properties": [("select_passthrough", select_passthrough)]}),
        ("node.select", {"type": type, "value": value, "ctrl": True}, None),
        ("node.select", {"type": type, "value": value, "alt": True}, None),
        ("node.select", {"type": type, "value": value, "ctrl": True, "alt": True}, None),
        ("node.select", {"type": type, "value": value, "shift": True},
         {"properties": [("toggle", True)]}),
        ("node.select", {"type": type, "value": value, "shift": True, "ctrl": True},
         {"properties": [("toggle", True)]}),
        ("node.select", {"type": type, "value": value, "shift": True, "alt": True},
         {"properties": [("toggle", True)]}),
        ("node.select", {"type": type, "value": value, "shift": True, "ctrl": True, "alt": True},
         {"properties": [("toggle", True)]}),
    ]

    if select_passthrough and (value == 'PRESS'):
        # Add an additional click item to de-select all other items,
        # needed so pass-through is able to de-select other items.
        items.append((
            "node.select",
            {"type": type, "value": 'CLICK'},
            {"properties": [("deselect_all", True)]},
        ))

    return items


def _template_uv_select(*, type, value, select_passthrough, legacy):

    # See: `use_tweak_select_passthrough` doc-string.
    if select_passthrough and (value in {'CLICK', 'RELEASE'}):
        select_passthrough = False

    items = [
        ("uv.select", {"type": type, "value": value},
         {"properties": [
             *((("deselect_all", True),) if not legacy else ()),
             *((("select_passthrough", True),) if select_passthrough else ()),
         ]}),
        ("uv.select", {"type": type, "value": value, "shift": True},
         {"properties": [("toggle", True)]}),
    ]

    if select_passthrough:
        # Add an additional click item to de-select all other items,
        # needed so pass-through is able to de-select other items.
        items.append((
            "uv.select",
            {"type": type, "value": 'CLICK'},
            {"properties": [("deselect_all", True)]},
        ))

    return items


def _template_mask_select(*, type, value, select_passthrough, legacy):

    # See: `use_tweak_select_passthrough` doc-string.
    if select_passthrough and (value in {'CLICK', 'RELEASE'}):
        select_passthrough = False

    items = [
        ("mask.select", {"type": type, "value": value},
         {"properties": [
             *((("deselect_all", True),) if not legacy else ()),
             *((("select_passthrough", True),) if select_passthrough else ()),
         ]}),
        ("mask.select", {"type": type, "value": value, "shift": True},
         {"properties": [("toggle", True)]}),
    ]

    if select_passthrough:
        # Add an additional click item to de-select all other items,
        # needed so pass-through is able to de-select other items.
        items.append((
            "mask.select",
            {"type": type, "value": 'CLICK'},
            {"properties": [("deselect_all", True)]},
        ))

    return items


def _template_sequencer_generic_select(*, type, value, legacy):
    return [(
        "sequencer.select",
        {"type": type, "value": value, **{m: True for m in mods}},
        {"properties": [(c, True) for c in props]},
    ) for props, mods in (
        (("deselect_all",) if not legacy else (), ()),
        (("toggle",), ("shift",)),
    )]


def _template_sequencer_preview_select(*, type, value, legacy):
    return _template_sequencer_generic_select(
        type=type, value=value, legacy=legacy,
    ) + [(
        "sequencer.select",
        {"type": type, "value": value, **{m: True for m in mods}},
        {"properties": [(c, True) for c in props]},
    ) for props, mods in (
        (("center",), ("ctrl",)),
        (("ignore_connections",), ("alt",)),
        # TODO:
        # (("enumerate",), ("alt",)),
        (("toggle", "center"), ("shift", "ctrl")),
        # (("center", "enumerate"), ("ctrl", "alt")),
        (("toggle", "ignore_connections"), ("shift", "alt")),
        # (("toggle", "enumerate"), ("shift", "alt")),
        # (("toggle", "center", "enumerate"), ("shift", "ctrl", "alt")),
    )]


def km_image_paint(params):
    items = []
    keymap = (
        "Image Paint",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("paint.image_paint", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("paint.image_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("paint.image_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SMOOTH')]}),
        ("paint.brush_colors_flip", {"type": 'X', "value": 'PRESS'}, None),
        ("paint.grab_clone", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        ("paint.sample_color",
         {"type": 'X', "value": 'PRESS', "shift": True},
         {"properties": [("merged", False)]}),
        ("paint.sample_color",
         {"type": 'X', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("merged", True)]}),
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        *_template_paint_radial_control("image_paint", color=True, zoom=True, rotation=True, secondary_rotation=True),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SCALE')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'ROTATION')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'TRANSLATION'), ("texmode", 'SECONDARY')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("mode", 'SCALE'), ("texmode", 'SECONDARY')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ROTATION'), ("texmode", 'SECONDARY')]}),
        ("wm.context_toggle", {"type": 'ONE', "value": 'PRESS'},
         {"properties": [("data_path", "image_paint_object.data.use_paint_mask")]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "tool_settings.image_paint.brush.use_smooth_stroke")]}),
        ("wm.context_menu_enum", {"type": 'E', "value": 'PRESS', "alt": True},
         {"properties": [("data_path", "tool_settings.image_paint.brush.stroke_method")]}),
        *_template_items_context_panel("VIEW3D_PT_paint_texture_context_menu", params.context_menu_event),
        *_template_asset_shelf_popup("VIEW3D_AST_brush_texture_paint", params.spacebar_action),
        *_template_asset_shelf_popup("IMAGE_AST_brush_paint", params.spacebar_action),
    ])

    if params.legacy:
        items.extend(_template_items_legacy_tools_from_numbers())

    return keymap


def km_vertex_paint(params):
    items = []
    keymap = (
        "Vertex Paint",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("paint.vertex_paint", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("paint.vertex_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("paint.vertex_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SMOOTH')]}),
        ("paint.brush_colors_flip", {"type": 'X', "value": 'PRESS'}, None),
        ("paint.sample_color", {"type": 'X', "value": 'PRESS', "shift": True}, {"properties": [("merged", False)]}),
        ("paint.vertex_color_set", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        *_template_paint_radial_control("vertex_paint", color=True, rotation=True),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SCALE')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'ROTATION')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'TRANSLATION'), ("texmode", 'SECONDARY')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("mode", 'SCALE'), ("texmode", 'SECONDARY')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ROTATION'), ("texmode", 'SECONDARY')]}),
        ("wm.context_toggle", {"type": 'ONE', "value": 'PRESS'},
         {"properties": [("data_path", "vertex_paint_object.data.use_paint_mask")]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "tool_settings.vertex_paint.brush.use_smooth_stroke")]}),
        ("wm.context_menu_enum", {"type": 'E', "value": 'PRESS', "alt": True},
         {"properties": [("data_path", "tool_settings.vertex_paint.brush.stroke_method")]}),
        ("paint.face_vert_reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        *_template_items_context_panel("VIEW3D_PT_paint_vertex_context_menu", params.context_menu_event),
        *_template_asset_shelf_popup("VIEW3D_AST_brush_vertex_paint", params.spacebar_action),
    ])

    if params.legacy:
        items.extend(_template_items_legacy_tools_from_numbers())
    else:
        items.append(
            ("wm.context_toggle", {"type": 'TWO', "value": 'PRESS'},
             {"properties": [("data_path", "vertex_paint_object.data.use_paint_mask_vertex")]})
        )

    return keymap


def km_weight_paint(params):
    # NOTE: This keymap falls through to "Pose" when an armature modifying the mesh
    # is selected in weight paint mode. When editing the key-map take care that pose operations
    # (such as transforming bones) is not impacted.
    items = []
    keymap = (
        "Weight Paint",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("paint.weight_paint", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("paint.weight_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("paint.weight_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SMOOTH')]}),
        ("paint.weight_sample", {"type": 'X', "value": 'PRESS', "shift": True}, None),
        ("paint.weight_sample_group", {"type": 'X', "value": 'PRESS', "ctrl": True, "shift": True}, None),
        ("paint.weight_gradient", {"type": 'A', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("type", 'RADIAL')]}),
        ("paint.weight_gradient", {"type": 'A', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINEAR')]}),
        ("paint.weight_set", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        *_template_paint_radial_control("weight_paint"),
        ("wm.radial_control", {"type": 'F', "value": 'PRESS', "ctrl": True},
         radial_control_properties("weight_paint", "weight", "use_unified_weight")),
        ("wm.context_menu_enum", {"type": 'E', "value": 'PRESS', "alt": True},
         {"properties": [("data_path", "tool_settings.vertex_paint.brush.stroke_method")]}),
        ("wm.context_toggle", {"type": 'ONE', "value": 'PRESS'},
         {"properties": [("data_path", "weight_paint_object.data.use_paint_mask")]}),
        ("wm.context_toggle", {"type": 'TWO', "value": 'PRESS'},
         {"properties": [("data_path", "weight_paint_object.data.use_paint_mask_vertex")]}),
        ("wm.context_toggle", {"type": 'THREE', "value": 'PRESS'},
         {"properties": [("data_path", "weight_paint_object.data.use_paint_bone_selection")]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "tool_settings.weight_paint.brush.use_smooth_stroke")]}),
        op_menu_pie("VIEW3D_MT_wpaint_vgroup_lock_pie", {"type": 'K', "value": 'PRESS'}),
        *_template_items_context_panel("VIEW3D_PT_paint_weight_context_menu", params.context_menu_event),
        *_template_asset_shelf_popup("VIEW3D_AST_brush_weight_paint", params.spacebar_action),
    ])

    if params.select_mouse == 'LEFTMOUSE':
        # Bone selection for combined weight paint + pose mode (Alt).
        items.extend([
            ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
            ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
             {"properties": [("toggle", True)]}),

            # Ctrl-Shift-LMB is needed for MMB emulation (which conflicts with Alt).
            # NOTE: this works reasonably well for pose-mode where typically selecting a single bone is sufficient.
            # For selecting faces/vertices, this is less useful. Selection tools are needed in this case.
            ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True, "shift": True}, None),
        ])

    if params.legacy:
        items.extend(_template_items_legacy_tools_from_numbers())

    return keymap


def km_paint_face_mask(params):
    # Use for vertex-paint & weight-paint modes.
    items = []
    keymap = (
        "Paint Face Mask (Weight, Vertex, Texture)",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_items_select_actions(params, "paint.face_select_all"),
        *_template_items_select_lasso(params, "view3d.select_lasso"),
        *_template_items_hide_reveal_actions("paint.face_select_hide", "paint.face_vert_reveal"),
        ("paint.face_select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("paint.face_select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("paint.face_select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("paint.face_select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("paint.face_select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
    ])

    # For left mouse the tool key-maps are used because this interferes with Alt-LMB for regular selection.
    if params.select_mouse == 'RIGHTMOUSE':
        items.extend(_template_view3d_paint_mask_select_loop(params))

    return keymap


def km_paint_vertex_mask(params):
    # Use for vertex-paint & weight-paint modes.
    items = []
    keymap = (
        "Paint Vertex Selection (Weight, Vertex)",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_items_select_actions(params, "paint.vert_select_all"),
        *_template_items_select_lasso(params, "view3d.select_lasso"),
        *_template_items_hide_reveal_actions("paint.vert_select_hide", "paint.face_vert_reveal"),
        ("view3d.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("view3d.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        ("paint.vert_select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("paint.vert_select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("select", True)]}),
        ("paint.vert_select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("select", False)]}),
        ("paint.vert_select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("paint.vert_select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
    ])

    # TODO: use `_template_view3d_paint_mask_select_loop` if loop-select is supported.
    # See: `km_paint_face_mask`.

    return keymap


# ------------------------------------------------------------------------------
# Object Sculpt Modes

def km_sculpt(params):
    items = []
    keymap = (
        "Sculpt",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Brush strokes
        ("sculpt.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("sculpt.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("sculpt.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SMOOTH')]}),
        # Expand
        ("sculpt.expand", {"type": 'A', "value": 'PRESS', "shift": True},
         {"properties": [
             ("target", "MASK"),
             ("falloff_type", "GEODESIC"),
             ("invert", False),
             ("use_auto_mask", False),
             ("use_mask_preserve", True),
         ]}),
        ("sculpt.expand", {"type": 'A', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [
             ("target", "MASK"),
             ("falloff_type", "NORMALS"),
             ("invert", False),
             ("use_mask_preserve", True),
         ]}),
        ("sculpt.expand", {"type": 'W', "value": 'PRESS', "shift": True},
         {"properties": [
             ("target", "FACE_SETS"),
             ("falloff_type", "GEODESIC"),
             ("invert", False),
             ("use_mask_preserve", False),
             ("use_modify_active", False),
         ]}),
        ("sculpt.expand", {"type": 'W', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [
             ("target", "FACE_SETS"),
             ("falloff_type", "BOUNDARY_FACE_SET"),
             ("invert", False),
             ("use_mask_preserve", False),
             ("use_modify_active", True),
         ]}),
        # Partial Visibility Show/hide
        # Match keys from: `_template_items_hide_reveal_actions`, cannot use because arguments aren't compatible.
        ("sculpt.face_set_change_visibility", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'TOGGLE')]}),
        ("sculpt.face_set_change_visibility", {"type": 'H', "value": 'PRESS'},
         {"properties": [("mode", 'HIDE_ACTIVE')]}),
        ("paint.hide_show_all", {"type": 'H', "value": 'PRESS', "alt": True},
         {"properties": [("action", "SHOW")]}),
        ("paint.visibility_filter", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True},
         {"properties": [("action", 'GROW')]}),
        ("paint.visibility_filter", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True},
         {"properties": [("action", 'SHRINK')]}),
        ("sculpt.face_set_edit", {"type": 'W', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'GROW')]}),
        ("sculpt.face_set_edit", {"type": 'W', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("mode", 'SHRINK')]}),
        # Subdivision levels
        *_template_items_object_subdivision_set(),
        ("object.subdivision_set", {"type": 'ONE', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("level", -1), ("relative", True), ("ensure_modifier", False)]}),
        ("object.subdivision_set", {"type": 'TWO', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("level", 1), ("relative", True), ("ensure_modifier", False)]}),
        # Mask
        ("paint.mask_flood_fill", {"type": 'M', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'VALUE'), ("value", 0.0)]}),
        ("paint.mask_flood_fill", {"type": 'I', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("paint.mask_box_gesture", {"type": 'B', "value": 'PRESS'},
         {"properties": [("mode", 'VALUE'), ("value", 0.0)]}),
        # Dynamic topology
        ("sculpt.dyntopo_detail_size_edit", {"type": 'R', "value": 'PRESS'}, None),
        ("sculpt.detail_flood_fill", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        # Remesh
        ("object.voxel_remesh", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("object.voxel_size_edit", {"type": 'R', "value": 'PRESS'}, None),
        # Color
        ("sculpt.sample_color", {"type": 'X', "value": 'PRESS', "shift": True}, None),
        ("paint.brush_colors_flip", {"type": 'X', "value": 'PRESS', }, None),
        # Brush properties
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        *_template_paint_radial_control("sculpt", rotation=True),
        # Stencil
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SCALE')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'ROTATION')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'TRANSLATION'), ("texmode", 'SECONDARY')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("mode", 'SCALE'), ("texmode", 'SECONDARY')]}),
        ("brush.stencil_control", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ROTATION'), ("texmode", 'SECONDARY')]}),
        # Sculpt Session Pivot Point
        ("sculpt.set_pivot_position", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SURFACE')]}),
        # Menus
        ("wm.context_menu_enum", {"type": 'E', "value": 'PRESS', "alt": True},
         {"properties": [("data_path", "tool_settings.sculpt.brush.stroke_method")]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", "tool_settings.sculpt.brush.use_smooth_stroke")]}),
        op_menu_pie("VIEW3D_MT_sculpt_mask_edit_pie", {"type": 'A', "value": 'PRESS'}),
        op_menu_pie("VIEW3D_MT_sculpt_automasking_pie", {"type": 'A', "alt": True, "value": 'PRESS'}),
        op_menu_pie("VIEW3D_MT_sculpt_face_sets_edit_pie", {"type": 'W', "value": 'PRESS', "alt": True}),
        *_template_items_context_panel("VIEW3D_PT_sculpt_context_menu", params.context_menu_event),
        # Brushes
        ("brush.asset_activate", {"type": 'V', "value": 'PRESS'},
         {"properties": [
             ("asset_library_type", 'ESSENTIALS'),
             ("relative_asset_identifier", "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Draw"),
         ]}),
        ("brush.asset_activate", {"type": 'S', "value": 'PRESS'},
         {"properties": [
             ("asset_library_type", 'ESSENTIALS'),
             ("relative_asset_identifier", "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Smooth"),
         ]}),
        ("brush.asset_activate", {"type": 'P', "value": 'PRESS'},
         {"properties": [
             ("asset_library_type", 'ESSENTIALS'),
             ("relative_asset_identifier", "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Pinch/Magnify"),
         ]}),
        ("brush.asset_activate", {"type": 'I', "value": 'PRESS'},
         {"properties": [
             ("asset_library_type", 'ESSENTIALS'),
             ("relative_asset_identifier", "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Inflate/Deflate"),
         ]}),
        ("brush.asset_activate", {"type": 'G', "value": 'PRESS'},
         {"properties": [
             ("asset_library_type", 'ESSENTIALS'),
             ("relative_asset_identifier", "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Grab"),
         ]}),
        ("brush.asset_activate", {"type": 'T', "value": 'PRESS', "shift": True},
         {"properties": [
             ("asset_library_type", 'ESSENTIALS'),
             ("relative_asset_identifier", "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Scrape/Fill"),
         ]}),
        ("brush.asset_activate", {"type": 'C', "value": 'PRESS'},
         {"properties": [
             ("asset_library_type", 'ESSENTIALS'),
             ("relative_asset_identifier", "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Clay Strips"),
         ]}),
        ("brush.asset_activate", {"type": 'C', "value": 'PRESS', "shift": True},
         {"properties": [
             ("asset_library_type", 'ESSENTIALS'),
             ("relative_asset_identifier", "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Crease Polish"),
         ]}),
        ("brush.asset_activate", {"type": 'K', "value": 'PRESS'},
         {"properties": [
             ("asset_library_type", 'ESSENTIALS'),
             ("relative_asset_identifier", "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Snake Hook"),
         ]}),
        ("brush.asset_activate", {"type": 'M', "value": 'PRESS'},
         {"properties": [
             ("asset_library_type", 'ESSENTIALS'),
             ("relative_asset_identifier", "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Mask"),
             ("use_toggle", True)
         ]}),
        *_template_asset_shelf_popup("VIEW3D_AST_brush_sculpt", params.spacebar_action),
    ])

    # Lasso Masking.
    # Needed because of shortcut conflicts on CTRL-LMB on right click select,
    # all modifier keys are used together to unmask (equivalent of selecting).
    if params.select_mouse == 'RIGHTMOUSE':
        items.extend([
            ("paint.mask_lasso_gesture",
             {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
             {"properties": [("value", 1.0)]}),
            ("paint.mask_lasso_gesture",
             {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
             {"properties": [("value", 0.0)]}),
        ])
    else:
        items.extend([
            ("paint.mask_lasso_gesture", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
             {"properties": [("value", 1.0)]}),
            ("paint.mask_lasso_gesture", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True},
             {"properties": [("value", 0.0)]}),
        ])

    if params.legacy:
        items.extend(_template_items_legacy_tools_from_numbers())

    return keymap


def km_sculpt_curves(params):
    items = []
    keymap = (
        "Sculpt Curves",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("sculpt_curves.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("sculpt_curves.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("sculpt_curves.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SMOOTH')]}),
        ("curves.set_selection_domain", {"type": 'ONE', "value": 'PRESS'}, {"properties": [("domain", 'POINT')]}),
        ("curves.set_selection_domain", {"type": 'TWO', "value": 'PRESS'}, {"properties": [("domain", 'CURVE')]}),
        *_template_paint_radial_control("curves_sculpt"),
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "repeat": True},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        *_template_items_select_actions(params, "curves.select_all"),
        ("sculpt_curves.min_distance_edit", {"type": 'R', "value": 'PRESS'}, {}),
        ("sculpt_curves.select_grow", {"type": 'A', "value": 'PRESS', "shift": True}, {}),
        *_template_asset_shelf_popup("VIEW3D_AST_brush_sculpt_curves", params.spacebar_action),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Object Edit Modes

# Mesh edit mode.
def km_edit_mesh(params):
    items = []
    keymap = (
        "Mesh",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Transform Actions.
        *_template_items_transform_actions(params, use_bend=True, use_mirror=True, use_tosphere=True, use_shear=True),
        ("transform.skin_resize", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),

        # Tools.
        op_tool_optional(
            ("mesh.loopcut_slide", {"type": 'R', "value": 'PRESS', "ctrl": True},
             {"properties": [("TRANSFORM_OT_edge_slide", [("release_confirm", False)],)]}),
            (op_tool_cycle, "builtin.loop_cut"), params),
        op_tool_optional(
            ("mesh.offset_edge_loops_slide", {"type": 'R', "value": 'PRESS', "shift": True, "ctrl": True},
             {"properties": [("TRANSFORM_OT_edge_slide", [("release_confirm", False)],)]}),
            (op_tool_cycle, "builtin.offset_edge_loop_cut"), params),
        op_tool_optional(
            ("mesh.inset", {"type": 'I', "value": 'PRESS'}, None),
            (op_tool_cycle, "builtin.inset_faces"), params),
        op_tool_optional(
            ("mesh.bevel", {"type": 'B', "value": 'PRESS', "ctrl": True},
             {"properties": [("affect", 'EDGES')]}),
            (op_tool_cycle, "builtin.bevel"), params),
        op_tool_optional(
            ("transform.shrink_fatten", {"type": 'S', "value": 'PRESS', "alt": True}, None),
            (op_tool_cycle, "builtin.shrink_fatten"), params),
        ("mesh.bevel", {"type": 'B', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("affect", 'VERTICES')]}),
        # Selection modes.
        *_template_items_editmode_mesh_select_mode(params),
        # Loop Select with alt. Double click in case MMB emulation is on (below).
        ("mesh.loop_select",
         {"type": params.select_mouse, "value": params.select_mouse_value, "alt": True}, None),
        ("mesh.loop_select",
         {"type": params.select_mouse, "value": params.select_mouse_value, "shift": True, "alt": True},
         {"properties": [("toggle", True)]}),
        # Selection
        ("mesh.edgering_select",
         {"type": params.select_mouse, "value": params.select_mouse_value, "ctrl": True, "alt": True}, None),
        ("mesh.edgering_select",
         {"type": params.select_mouse, "value": params.select_mouse_value, "shift": True, "ctrl": True, "alt": True},
         {"properties": [("toggle", True)]}),
        ("mesh.shortest_path_pick",
         {"type": params.select_mouse, "value": params.select_mouse_value_fallback, "ctrl": True},
         {"properties": [("use_fill", False)]}),
        ("mesh.shortest_path_pick",
         {"type": params.select_mouse, "value": params.select_mouse_value_fallback, "shift": True, "ctrl": True},
         {"properties": [("use_fill", True)]}),
        *_template_items_select_actions(params, "mesh.select_all"),
        ("mesh.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("mesh.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("mesh.select_next_item",
         {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True}, None),
        ("mesh.select_prev_item",
         {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True}, None),
        ("mesh.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("mesh.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("mesh.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("mesh.select_mirror", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        op_menu("VIEW3D_MT_edit_mesh_select_similar", {"type": 'G', "value": 'PRESS', "shift": True}),
        # Hide/reveal.
        *_template_items_hide_reveal_actions("mesh.hide", "mesh.reveal"),
        # Tools.
        ("mesh.normals_make_consistent", {"type": 'N', "value": 'PRESS', "ctrl" if params.legacy else "shift": True},
         {"properties": [("inside", False)]}),
        ("mesh.normals_make_consistent", {"type": 'N', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("inside", True)]}),
        op_tool_optional(
            ("view3d.edit_mesh_extrude_move_normal", {"type": 'E', "value": 'PRESS'}, None),
            (op_tool_cycle, "builtin.extrude_region"), params),
        op_menu("VIEW3D_MT_edit_mesh_extrude", {"type": 'E', "value": 'PRESS', "alt": True}),
        ("transform.edge_crease", {"type": 'E', "value": 'PRESS', "shift": True}, None),
        ("mesh.fill", {"type": 'F', "value": 'PRESS', "alt": True}, None),
        ("mesh.quads_convert_to_tris", {"type": 'T', "value": 'PRESS', "ctrl": True},
         {"properties": [("quad_method", 'BEAUTY'), ("ngon_method", 'BEAUTY')]}),
        ("mesh.quads_convert_to_tris", {"type": 'T', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("quad_method", 'FIXED'), ("ngon_method", 'CLIP')]}),
        ("mesh.tris_convert_to_quads", {"type": 'J', "value": 'PRESS', "alt": True}, None),
        op_tool_optional(
            ("mesh.rip_move", {"type": 'V', "value": 'PRESS'},
             {"properties": [("MESH_OT_rip", [("use_fill", False)],)]}),
            (op_tool_cycle, "builtin.rip_region"), params),
        # No tool is available for this.
        ("mesh.rip_move", {"type": 'V', "value": 'PRESS', "alt": True},
         {"properties": [("MESH_OT_rip", [("use_fill", True)],)]}),
        ("mesh.rip_edge_move", {"type": 'D', "value": 'PRESS', "alt": True}, None),
        op_menu("VIEW3D_MT_edit_mesh_merge", {"type": 'M', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_mesh_split", {"type": 'M', "value": 'PRESS', "alt": True}),
        ("mesh.edge_face_add", {"type": 'F', "value": 'PRESS', "repeat": True}, None),
        ("mesh.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        op_menu("VIEW3D_MT_mesh_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        ("mesh.separate", {"type": 'P', "value": 'PRESS'}, None),
        ("mesh.split", {"type": 'Y', "value": 'PRESS'}, None),
        ("mesh.vert_connect_path", {"type": 'J', "value": 'PRESS'}, None),
        ("mesh.point_normals", {"type": 'L', "value": 'PRESS', "alt": True}, None),
        op_tool_optional(
            ("transform.vert_slide", {"type": 'V', "value": 'PRESS', "shift": True}, None),
            (op_tool_cycle, "builtin.vertex_slide"), params),
        ("mesh.dupli_extrude_cursor", {"type": params.action_mouse, "value": 'CLICK', "ctrl": True},
         {"properties": [("rotate_source", True)]}),
        ("mesh.dupli_extrude_cursor", {"type": params.action_mouse, "value": 'CLICK', "shift": True, "ctrl": True},
         {"properties": [("rotate_source", False)]}),
        op_menu("VIEW3D_MT_edit_mesh_delete", {"type": 'X', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_mesh_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("mesh.dissolve_mode", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("mesh.dissolve_mode", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        op_tool_optional(
            ("mesh.knife_tool", {"type": 'K', "value": 'PRESS'},
             {"properties": [("use_occlude_geometry", True), ("only_selected", False)]}),
            (op_tool_cycle, "builtin.knife"), params),
        ("mesh.knife_tool", {"type": 'K', "value": 'PRESS', "shift": True},
         {"properties": [("use_occlude_geometry", False), ("only_selected", True)]}),
        ("object.vertex_parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        # Menus.
        op_menu("VIEW3D_MT_edit_mesh_faces", {"type": 'F', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_edit_mesh_edges", {"type": 'E', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_edit_mesh_vertices", {"type": 'V', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_hook", {"type": 'H', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_uv_map", {"type": 'U', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_vertex_group", {"type": 'G', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_edit_mesh_normals", {"type": 'N', "value": 'PRESS', "alt": True}),
        ("object.vertex_group_remove_from", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        *_template_items_proportional_editing(
            params, connected=True, toggle_data_path="tool_settings.use_proportional_edit"),
        *_template_items_context_menu("VIEW3D_MT_edit_mesh_context_menu", params.context_menu_event),
    ])

    if params.use_mouse_emulate_3_button and params.select_mouse == 'LEFTMOUSE':
        items.extend([
            ("mesh.loop_select", {"type": params.select_mouse, "value": 'DOUBLE_CLICK'}, None),
            ("mesh.loop_select", {"type": params.select_mouse, "value": 'DOUBLE_CLICK', "shift": True},
             {"properties": [("extend", True)]}),
            ("mesh.loop_select", {"type": params.select_mouse, "value": 'DOUBLE_CLICK', "alt": True},
             {"properties": [("deselect", True)]}),
            ("mesh.edgering_select",
             {"type": params.select_mouse, "value": 'DOUBLE_CLICK', "ctrl": True}, None),
            ("mesh.edgering_select",
             {"type": params.select_mouse, "value": 'DOUBLE_CLICK', "shift": True, "ctrl": True},
             {"properties": [("toggle", True)]}),
        ])

    if params.legacy:
        items.extend([
            ("mesh.poke", {"type": 'P', "value": 'PRESS', "alt": True}, None),
            ("mesh.select_non_manifold",
             {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
            ("mesh.faces_select_linked_flat",
             {"type": 'F', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
            ("mesh.spin", {"type": 'R', "value": 'PRESS', "alt": True}, None),
            ("mesh.beautify_fill", {"type": 'F', "value": 'PRESS', "shift": True, "alt": True}, None),
            *_template_items_object_subdivision_set(),
        ])

    return keymap


# Armature edit mode
def km_edit_armature(params):
    items = []
    keymap = (
        "Armature",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Transform Actions.
        *_template_items_transform_actions(params, use_mirror=True),

        # Hide/reveal.
        *_template_items_hide_reveal_actions("armature.hide", "armature.reveal"),
        # Align & roll.
        ("armature.align", {"type": 'A', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("armature.calculate_roll", {"type": 'N', "value": 'PRESS', "ctrl" if params.legacy else "shift": True}, None),
        ("armature.roll_clear", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("armature.switch_direction", {"type": 'F', "value": 'PRESS', "alt": True}, None),
        # Add.
        ("armature.bone_primitive_add", {"type": 'A', "value": 'PRESS', "shift": True}, None),
        # Parenting.
        ("armature.parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        ("armature.parent_clear", {"type": 'P', "value": 'PRESS', "alt": True}, None),
        # Selection.
        *_template_items_select_actions(params, "armature.select_all"),
        ("armature.select_mirror", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("extend", False)]}),
        ("armature.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'PARENT'), ("extend", False)]}),
        ("armature.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'PARENT'), ("extend", True)]}),
        ("armature.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'CHILD'), ("extend", False)]}),
        ("armature.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'CHILD'), ("extend", True)]}),
        ("armature.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("armature.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("armature.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("armature.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("armature.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("armature.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("armature.shortest_path_pick",
         {"type": params.select_mouse, "value": params.select_mouse_value_fallback, "ctrl": True}, None),
        # Editing.
        op_menu("VIEW3D_MT_edit_armature_delete", {"type": 'X', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_armature_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("armature.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("armature.dissolve", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("armature.dissolve", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        op_tool_optional(
            ("armature.extrude_move", {"type": 'E', "value": 'PRESS'}, None),
            (op_tool_cycle, "builtin.extrude"), params),
        ("armature.extrude_forked", {"type": 'E', "value": 'PRESS', "shift": True}, None),
        ("armature.click_extrude", {"type": params.action_mouse, "value": 'CLICK', "ctrl": True}, None),
        ("armature.fill", {"type": 'F', "value": 'PRESS'}, None),
        ("armature.split", {"type": 'Y', "value": 'PRESS'}, None),
        ("armature.separate", {"type": 'P', "value": 'PRESS'}, None),
        # Set flags.
        op_menu("VIEW3D_MT_bone_options_toggle", {"type": 'W', "value": 'PRESS', "shift": True}),
        op_menu("VIEW3D_MT_bone_options_enable", {"type": 'W', "value": 'PRESS', "shift": True, "ctrl": True}),
        op_menu("VIEW3D_MT_bone_options_disable", {"type": 'W', "value": 'PRESS', "alt": True}),
        # Armature/bone layers.
        ("armature.assign_to_collection", {"type": 'M', "value": 'PRESS', "shift": True}, None),
        ("armature.move_to_collection", {"type": 'M', "value": 'PRESS'}, None),
        # Special transforms.
        op_tool_optional(
            ("transform.bbone_resize", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
            (op_tool_cycle, "builtin.bone_size"), params),
        op_tool_optional(
            ("transform.transform", {"type": 'S', "value": 'PRESS', "alt": True},
             {"properties": [("mode", 'BONE_ENVELOPE')]}),
            (op_tool_cycle, "builtin.bone_envelope"), params),
        op_tool_optional(
            ("transform.transform", {"type": 'R', "value": 'PRESS', "ctrl": True},
             {"properties": [("mode", 'BONE_ROLL')]}),
            (op_tool_cycle, "builtin.roll"), params),
        # Menus.
        *_template_items_context_menu("VIEW3D_MT_armature_context_menu", params.context_menu_event),
    ])

    return keymap


# Meta-ball edit mode.
def km_edit_metaball(params):
    items = []
    keymap = (
        "Metaball",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Transform Actions.
        *_template_items_transform_actions(params, use_mirror=True),

        ("object.metaball_add", {"type": 'A', "value": 'PRESS', "shift": True}, None),
        *_template_items_hide_reveal_actions("mball.hide_metaelems", "mball.reveal_metaelems"),
        ("mball.delete_metaelems", {"type": 'X', "value": 'PRESS'}, None),
        ("mball.delete_metaelems", {"type": 'DEL', "value": 'PRESS'}, None),
        ("mball.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        *_template_items_select_actions(params, "mball.select_all"),
        ("mball.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        *_template_items_proportional_editing(
            params, connected=True, toggle_data_path="tool_settings.use_proportional_edit"),
        *_template_items_context_menu("VIEW3D_MT_edit_metaball_context_menu", params.context_menu_event),
    ])

    return keymap


# Lattice edit mode.
def km_edit_lattice(params):
    items = []
    keymap = (
        "Lattice",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Transform Actions.
        *_template_items_transform_actions(params, use_bend=True, use_mirror=True, use_tosphere=True, use_shear=True),

        *_template_items_select_actions(params, "lattice.select_all"),
        ("lattice.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("lattice.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("object.vertex_parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        ("lattice.flip", {"type": 'F', "value": 'PRESS', "alt": True}, None),
        op_menu("VIEW3D_MT_hook", {"type": 'H', "value": 'PRESS', "ctrl": True}),
        *_template_items_proportional_editing(
            params, connected=False, toggle_data_path="tool_settings.use_proportional_edit"),
        *_template_items_context_menu("VIEW3D_MT_edit_lattice_context_menu", params.context_menu_event),
    ])

    return keymap


# Particle edit mode.
def km_edit_particle(params):
    items = []
    keymap = (
        "Particle",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_items_select_actions(params, "particle.select_all"),
        ("particle.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("particle.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("particle.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("particle.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("particle.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("particle.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("particle.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        *_template_items_hide_reveal_actions("particle.hide", "particle.reveal"),
        ("particle.brush_edit", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("particle.brush_edit", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True}, None),
        ("wm.radial_control", {"type": 'F', "value": 'PRESS'},
         {"properties": [("data_path_primary", "tool_settings.particle_edit.brush.size")]}),
        ("wm.radial_control", {"type": 'F', "value": 'PRESS', "shift": True},
         {"properties": [("data_path_primary", "tool_settings.particle_edit.brush.strength")]}),
        ("particle.weight_set", {"type": 'K', "value": 'PRESS', "shift": True}, None),
        *(
            (("wm.context_set_enum",
              {"type": NUMBERS_1[i], "value": 'PRESS'},
              {"properties": [("data_path", "tool_settings.particle_edit.select_mode"), ("value", value)]})
             for i, value in enumerate(('PATH', 'POINT', 'TIP'))
             )
        ),
        *_template_items_proportional_editing(
            params, connected=False, toggle_data_path="tool_settings.use_proportional_edit"),
        *_template_items_context_menu("VIEW3D_MT_particle_context_menu", params.context_menu_event),
        *_template_items_transform_actions(params),
    ])

    return keymap


# Text edit mode.
def km_edit_font(params):
    items = []
    keymap = (
        "Font",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("font.style_toggle", {"type": 'B', "value": 'PRESS', "ctrl": True},
         {"properties": [("style", 'BOLD')]}),
        ("font.style_toggle", {"type": 'I', "value": 'PRESS', "ctrl": True},
         {"properties": [("style", 'ITALIC')]}),
        ("font.style_toggle", {"type": 'U', "value": 'PRESS', "ctrl": True},
         {"properties": [("style", 'UNDERLINE')]}),
        ("font.style_toggle", {"type": 'P', "value": 'PRESS', "ctrl": True},
         {"properties": [("style", 'SMALL_CAPS')]}),
        ("font.delete", {"type": 'DEL', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_OR_SELECTION')]}),
        ("font.delete", {"type": 'DEL', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("font.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_OR_SELECTION')]}),
        ("font.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_OR_SELECTION')]}),
        ("font.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("font.move", {"type": 'HOME', "value": 'PRESS'},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("font.move", {"type": 'END', "value": 'PRESS'},
         {"properties": [("type", 'LINE_END')]}),
        ("font.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("font.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("font.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("font.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("font.move", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_LINE')]}),
        ("font.move", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_LINE')]}),
        ("font.move", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'PREVIOUS_PAGE')]}),
        ("font.move", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True},
         {"properties": [("type", 'NEXT_PAGE')]}),
        ("font.move", {"type": 'HOME', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'TEXT_BEGIN')]}),
        ("font.move", {"type": 'END', "value": 'PRESS', "ctrl": True, "repeat": True},
         {"properties": [("type", 'TEXT_END')]}),
        ("font.move_select", {"type": 'HOME', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("font.move_select", {"type": 'END', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_END')]}),
        ("font.move_select", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("font.move_select", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("font.move_select", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("font.move_select", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("font.move_select", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_LINE')]}),
        ("font.move_select", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'NEXT_LINE')]}),
        ("font.move_select", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'PREVIOUS_PAGE')]}),
        ("font.move_select", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True, "repeat": True},
         {"properties": [("type", 'NEXT_PAGE')]}),
        ("font.move_select", {"type": 'HOME', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("type", 'TEXT_BEGIN')]}),
        ("font.move_select", {"type": 'END', "value": 'PRESS', "shift": True, "ctrl": True, "repeat": True},
         {"properties": [("type", 'TEXT_END')]}),
        ("font.change_spacing", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("delta", -1.0)]}),
        ("font.change_spacing", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("delta", 1.0)]}),
        ("font.change_spacing", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "alt": True, "repeat": True},
         {"properties": [("delta", -0.1)]}),
        ("font.change_spacing", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "alt": True, "repeat": True},
         {"properties": [("delta", 0.1)]}),
        ("font.change_character", {"type": 'UP_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("delta", 1)]}),
        ("font.change_character", {"type": 'DOWN_ARROW', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("delta", -1)]}),
        ("font.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("font.text_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("font.text_cut", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("font.text_paste", {"type": 'V', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("font.line_break", {"type": 'RET', "value": 'PRESS', "repeat": True}, None),
        ("font.line_break", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "repeat": True}, None),
        ("font.text_insert", {"type": 'TEXTINPUT', "value": 'ANY', "any": True, "repeat": True}, None),
        ("font.text_insert", {"type": 'BACK_SPACE', "value": 'PRESS', "alt": True, "repeat": True},
         {"properties": [("accent", True)]}),
        *_template_items_context_menu("VIEW3D_MT_edit_font_context_menu", params.context_menu_event),
    ])

    return keymap


def km_edit_curve_legacy(params):
    items = []
    keymap = (
        "Curve",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Transform Actions.
        *_template_items_transform_actions(params, use_bend=True, use_mirror=True),

        op_menu("TOPBAR_MT_edit_curve_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        ("curve.handle_type_set", {"type": 'V', "value": 'PRESS'}, None),
        ("curve.vertex_add", {"type": params.action_mouse, "value": 'CLICK', "ctrl": True}, None),
        *_template_items_select_actions(params, "curve.select_all"),
        ("curve.select_row", {"type": 'R', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("curve.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("curve.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("curve.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("curve.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("curve.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("curve.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("curve.shortest_path_pick",
         {"type": params.select_mouse, "value": params.select_mouse_value_fallback, "ctrl": True}, None),
        ("curve.separate", {"type": 'P', "value": 'PRESS'}, None),
        ("curve.split", {"type": 'Y', "value": 'PRESS'}, None),
        op_tool_optional(
            ("curve.extrude_move", {"type": 'E', "value": 'PRESS'}, None),
            (op_tool_cycle, "builtin.extrude"), params),
        ("curve.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("curve.make_segment", {"type": 'F', "value": 'PRESS'}, None),
        ("curve.cyclic_toggle", {"type": 'C', "value": 'PRESS', "alt": True}, None),
        op_menu("VIEW3D_MT_edit_curve_delete", {"type": 'X', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_curve_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("curve.dissolve_verts", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("curve.dissolve_verts", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        ("curve.tilt_clear", {"type": 'T', "value": 'PRESS', "alt": True}, None),
        op_tool_optional(
            ("transform.tilt", {"type": 'T', "value": 'PRESS', "ctrl": True}, None),
            (op_tool_cycle, "builtin.tilt"), params),
        ("transform.transform", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'CURVE_SHRINKFATTEN')]}),
        *_template_items_hide_reveal_actions("curve.hide", "curve.reveal"),
        ("curve.normals_make_consistent",
         {"type": 'N', "value": 'PRESS', "ctrl" if params.legacy else "shift": True}, None),
        ("object.vertex_parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        op_menu("VIEW3D_MT_hook", {"type": 'H', "value": 'PRESS', "ctrl": True}),
        *_template_items_proportional_editing(
            params, connected=True, toggle_data_path="tool_settings.use_proportional_edit"),
        *_template_items_context_menu("VIEW3D_MT_edit_curve_context_menu", params.context_menu_event),
    ])

    return keymap


# Curves edit mode.
def km_edit_curves(params):
    items = []
    keymap = (
        "Curves",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Transform Actions.
        *_template_items_transform_actions(params, use_bend=True, use_mirror=True),

        ("curves.set_selection_domain", {"type": 'ONE', "value": 'PRESS'}, {"properties": [("domain", 'POINT')]}),
        ("curves.set_selection_domain", {"type": 'TWO', "value": 'PRESS'}, {"properties": [("domain", 'CURVE')]}),
        ("curves.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        *_template_items_select_actions(params, "curves.select_all"),
        ("curves.extrude_move", {"type": 'E', "value": 'PRESS'}, None),
        ("curves.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("curves.select_linked_pick", {"type": 'L', "value": 'PRESS'}, {"properties": [("deselect", False)]}),
        ("curves.select_linked_pick", {"type": 'L', "value": 'PRESS',
         "shift": True}, {"properties": [("deselect", True)]}),
        ("curves.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("curves.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("curves.separate", {"type": 'P', "value": 'PRESS'}, None),
        ("curves.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("curves.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True, "repeat": True}, None),
        ("curves.split", {"type": 'Y', "value": 'PRESS'}, None),
        *_template_items_proportional_editing(
            params, connected=True, toggle_data_path="tool_settings.use_proportional_edit"),
        ("curves.tilt_clear", {"type": 'T', "value": 'PRESS', "alt": True}, None),
        op_tool_optional(
            ("transform.tilt", {"type": 'T', "value": 'PRESS', "ctrl": True}, None),
            (op_tool_cycle, "builtin.tilt"), params),
        ("transform.transform", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'CURVE_SHRINKFATTEN')]}),
        ("curves.cyclic_toggle", {"type": 'C', "value": 'PRESS', "alt": True}, None),
        ("curves.handle_type_set", {"type": 'V', "value": 'PRESS'}, None),
        op_menu("VIEW3D_MT_edit_curves_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        *_template_items_context_menu("VIEW3D_MT_edit_curves_context_menu", params.context_menu_event),
    ])

    return keymap


# Point cloud edit mode.
def km_edit_pointcloud(params):
    items = []
    keymap = (
        "Point Cloud",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Transform Actions.
        *_template_items_transform_actions(params, use_bend=True, use_mirror=True),

        ("pointcloud.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        *_template_items_select_actions(params, "pointcloud.select_all"),
        ("pointcloud.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("pointcloud.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("pointcloud.separate", {"type": 'P', "value": 'PRESS'}, None),
        ("transform.transform", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'CURVE_SHRINKFATTEN')]}),
        *_template_items_proportional_editing(
            params, connected=True, toggle_data_path="tool_settings.use_proportional_edit"),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Modal Maps and Gizmos

def km_eyedropper_modal_map(_params):
    items = []
    keymap = (
        "Eyedropper Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'RET', "value": 'RELEASE', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'RELEASE', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'LEFTMOUSE', "value": 'RELEASE', "any": True}, None),
        ("SAMPLE_BEGIN", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("SAMPLE_RESET", {"type": 'SPACE', "value": 'RELEASE', "any": True}, None),
    ])

    return keymap


def km_eyedropper_colorramp_pointsampling_map(_params):
    items = []
    keymap = (
        "Eyedropper ColorRamp PointSampling Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'BACK_SPACE', "value": 'PRESS', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'RET', "value": 'RELEASE', "any": True}, None),
        ("SAMPLE_CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'RELEASE', "any": True}, None),
        ("SAMPLE_SAMPLE", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("SAMPLE_RESET", {"type": 'SPACE', "value": 'RELEASE', "any": True}, None),
    ])

    return keymap


def km_transform_modal_map(params):
    items = []
    keymap = (
        "Transform Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    alt_without_navigaton = {} if params.use_alt_navigation else {"alt": True}

    items.extend([
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'SPACE', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("AXIS_X", {"type": 'X', "value": 'PRESS'}, None),
        ("AXIS_Y", {"type": 'Y', "value": 'PRESS'}, None),
        ("AXIS_Z", {"type": 'Z', "value": 'PRESS'}, None),
        ("PLANE_X", {"type": 'X', "value": 'PRESS', "shift": True}, None),
        ("PLANE_Y", {"type": 'Y', "value": 'PRESS', "shift": True}, None),
        ("PLANE_Z", {"type": 'Z', "value": 'PRESS', "shift": True}, None),
        ("CONS_OFF", {"type": 'C', "value": 'PRESS'}, None),
        ("TRANSLATE", {"type": 'G', "value": 'PRESS'}, None),
        ("VERT_EDGE_SLIDE", {"type": 'G', "value": 'PRESS'}, None),
        ("ROTATE", {"type": 'R', "value": 'PRESS'}, None),
        ("TRACKBALL", {"type": 'R', "value": 'PRESS'}, None),
        ("RESIZE", {"type": 'S', "value": 'PRESS'}, None),
        ("ROTATE_NORMALS", {"type": 'N', "value": 'PRESS'}, None),
        ("EDIT_SNAP_SOURCE_ON", {"type": 'B', "value": 'PRESS'}, None),
        ("EDIT_SNAP_SOURCE_OFF", {"type": 'B', "value": 'PRESS'}, None),
        ("SNAP_TOGGLE", {"type": 'TAB', "value": 'PRESS', "shift": True}, None),
        ("SNAP_INV_ON", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_INV_OFF", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("SNAP_INV_ON", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_INV_OFF", {"type": 'RIGHT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("ADD_SNAP", {"type": 'A', "value": 'PRESS'}, None),
        ("ADD_SNAP", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("REMOVE_SNAP", {"type": 'A', "value": 'PRESS', "alt": True}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True, "repeat": True}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True, "repeat": True}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', **alt_without_navigaton}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'WHEELUPMOUSE', "value": 'PRESS', **alt_without_navigaton}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True}, None),
        ("PROPORTIONAL_SIZE", {"type": 'TRACKPADPAN', "value": 'ANY', **alt_without_navigaton}, None),
        ("AUTOIK_CHAIN_LEN_UP", {"type": 'PAGE_UP', "value": 'PRESS', "repeat": True}, None),
        ("AUTOIK_CHAIN_LEN_DOWN", {"type": 'PAGE_DOWN', "value": 'PRESS', "repeat": True}, None),
        ("AUTOIK_CHAIN_LEN_UP", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True, "repeat": True}, None),
        ("AUTOIK_CHAIN_LEN_DOWN", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True, "repeat": True}, None),
        ("AUTOIK_CHAIN_LEN_UP", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', **alt_without_navigaton}, None),
        ("AUTOIK_CHAIN_LEN_DOWN", {"type": 'WHEELUPMOUSE', "value": 'PRESS', **alt_without_navigaton}, None),
        ("AUTOIK_CHAIN_LEN_UP", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True}, None),
        ("AUTOIK_CHAIN_LEN_DOWN", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True}, None),
        ("INSERTOFS_TOGGLE_DIR", {"type": 'T', "value": 'PRESS'}, None),
        ("NODE_ATTACH_ON", {"type": 'LEFT_ALT', "value": 'RELEASE', "any": True}, None),
        ("NODE_ATTACH_OFF", {"type": 'LEFT_ALT', "value": 'PRESS', "any": True}, None),
        ("NODE_FRAME", {"type": 'F', "value": 'PRESS'}, None),
        ("AUTOCONSTRAIN", {"type": 'MIDDLEMOUSE', "value": 'ANY', **alt_without_navigaton}, None),
        ("AUTOCONSTRAINPLANE", {"type": 'MIDDLEMOUSE', "value": 'ANY', "shift": True, **alt_without_navigaton}, None),
        ("PRECISION", {"type": 'LEFT_SHIFT', "value": 'ANY', "any": True}, None),
        ("PRECISION", {"type": 'RIGHT_SHIFT', "value": 'ANY', "any": True}, None),
        ("STRIP_CLAMP_TOGGLE", {"type": 'C', "value": 'PRESS', "any": True}, None),
    ])

    if params.use_alt_navigation:
        items.append(("PASSTHROUGH_NAVIGATE", {"type": 'LEFT_ALT', "value": 'ANY', "any": True}, None))

    return keymap


def km_view3d_interactive_add_tool_modal(_params):
    items = []
    keymap = (
        "View3D Placement Modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("PIVOT_CENTER_ON", {"type": 'LEFT_ALT', "value": 'PRESS', "any": True}, None),
        ("PIVOT_CENTER_OFF", {"type": 'LEFT_ALT', "value": 'RELEASE', "any": True}, None),
        ("PIVOT_CENTER_ON", {"type": 'RIGHT_ALT', "value": 'PRESS', "any": True}, None),
        ("PIVOT_CENTER_OFF", {"type": 'RIGHT_ALT', "value": 'RELEASE', "any": True}, None),
        ("FIXED_ASPECT_ON", {"type": 'LEFT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("FIXED_ASPECT_OFF", {"type": 'LEFT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("FIXED_ASPECT_ON", {"type": 'RIGHT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("FIXED_ASPECT_OFF", {"type": 'RIGHT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("SNAP_ON", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_OFF", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("SNAP_ON", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_OFF", {"type": 'RIGHT_CTRL', "value": 'RELEASE', "any": True}, None),
    ])

    return keymap


def km_view3d_gesture_circle(_params):
    items = []
    keymap = (
        "View3D Gesture Circle",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        # Note: use 'KM_ANY' for release, so the circle exits on any mouse release,
        # this is needed when circle select is activated as a tool.
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'ANY', "any": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS'}, None),
        ("SELECT", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("DESELECT", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True}, None),
        ("NOP", {"type": 'LEFTMOUSE', "value": 'RELEASE', "any": True}, None),
        ("DESELECT", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("NOP", {"type": 'MIDDLEMOUSE', "value": 'RELEASE', "any": True}, None),
        ("SUBTRACT", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("SUBTRACT", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "repeat": True}, None),
        ("ADD", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("ADD", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "repeat": True}, None),
        ("SIZE", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
    ])

    return keymap


def km_gesture_border(_params):
    items = []
    keymap = (
        "Gesture Box",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("SELECT", {"type": 'RIGHTMOUSE', "value": 'RELEASE', "any": True}, None),
        ("BEGIN", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True}, None),
        ("DESELECT", {"type": 'LEFTMOUSE', "value": 'RELEASE', "shift": True}, None),
        ("BEGIN", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("SELECT", {"type": 'LEFTMOUSE', "value": 'RELEASE', "any": True}, None),
        ("BEGIN", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("DESELECT", {"type": 'MIDDLEMOUSE', "value": 'RELEASE'}, None),
        ("MOVE", {"type": 'SPACE', "value": 'ANY', "any": True}, None),
    ])

    return keymap


def km_gesture_zoom_border(_params):
    items = []
    keymap = (
        "Gesture Zoom Border",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'ANY', "any": True}, None),
        ("BEGIN", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("IN", {"type": 'LEFTMOUSE', "value": 'RELEASE'}, None),
        ("BEGIN", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("OUT", {"type": 'MIDDLEMOUSE', "value": 'RELEASE'}, None),
    ])

    return keymap


def km_gesture_straight_line(_params):
    items = []
    keymap = (
        "Gesture Straight Line",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'ANY', "any": True}, None),
        ("BEGIN", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("SELECT", {"type": 'LEFTMOUSE', "value": 'RELEASE', "any": True}, None),
        ("MOVE", {"type": 'SPACE', "value": 'ANY', "any": True}, None),
        ("SNAP", {"type": 'LEFT_CTRL', "value": 'ANY', "any": True}, None),
        ("FLIP", {"type": 'F', "value": 'PRESS', "any": True}, None),
    ])

    return keymap


def km_gesture_lasso(_params):
    items = []
    keymap = (
        "Gesture Lasso",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("MOVE", {"type": 'SPACE', "value": 'ANY', "any": True}, None),
    ])

    return keymap


def km_gesture_polyline(_params):
    items = []
    keymap = (
        "Gesture Polyline",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "any": True}, None),
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'ANY', "any": True}, None),
        ("SELECT", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("MOVE", {"type": 'SPACE', "value": 'ANY', "any": True}, None),
    ])

    return keymap


def km_standard_modal_map(_params):
    items = []
    keymap = (
        "Standard Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("APPLY", {"type": 'LEFTMOUSE', "value": 'ANY', "any": True}, None),
        ("APPLY", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("APPLY", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("SNAP", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_OFF", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("SNAP", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_OFF", {"type": 'RIGHT_CTRL', "value": 'RELEASE', "any": True}, None),
    ])

    return keymap


def km_knife_tool_modal_map(_params):
    items = []
    keymap = (
        "Knife Tool Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("PANNING", {"type": 'MIDDLEMOUSE', "value": 'ANY', "any": True}, None),
        ("ADD_CUT_CLOSED", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "any": True}, None),
        ("ADD_CUT", {"type": 'LEFTMOUSE', "value": 'ANY', "any": True}, None),
        ("UNDO", {"type": 'Z', "value": 'PRESS', "ctrl": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'SPACE', "value": 'PRESS', "any": True}, None),
        ("NEW_CUT", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        ("SNAP_MIDPOINTS_ON", {"type": 'LEFT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("SNAP_MIDPOINTS_OFF", {"type": 'LEFT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("SNAP_MIDPOINTS_ON", {"type": 'RIGHT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("SNAP_MIDPOINTS_OFF", {"type": 'RIGHT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("IGNORE_SNAP_ON", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": True}, None),
        ("IGNORE_SNAP_OFF", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("IGNORE_SNAP_ON", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": True}, None),
        ("IGNORE_SNAP_OFF", {"type": 'RIGHT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("X_AXIS", {"type": 'X', "value": 'PRESS'}, None),
        ("Y_AXIS", {"type": 'Y', "value": 'PRESS'}, None),
        ("Z_AXIS", {"type": 'Z', "value": 'PRESS'}, None),
        ("ANGLE_SNAP_TOGGLE", {"type": 'A', "value": 'PRESS'}, None),
        ("CYCLE_ANGLE_SNAP_EDGE", {"type": 'R', "value": 'PRESS'}, None),
        ("CUT_THROUGH_TOGGLE", {"type": 'C', "value": 'PRESS'}, None),
        ("SHOW_DISTANCE_ANGLE_TOGGLE", {"type": 'S', "value": 'PRESS'}, None),
        ("DEPTH_TEST_TOGGLE", {"type": 'V', "value": 'PRESS'}, None),
    ])

    return keymap


def km_custom_normals_modal_map(_params):
    items = []
    keymap = (
        "Custom Normals Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("RESET", {"type": 'R', "value": 'PRESS'}, None),
        ("INVERT", {"type": 'I', "value": 'PRESS'}, None),
        ("SPHERIZE", {"type": 'S', "value": 'PRESS'}, None),
        ("ALIGN", {"type": 'A', "value": 'PRESS'}, None),
        ("USE_MOUSE", {"type": 'M', "value": 'PRESS'}, None),
        ("USE_PIVOT", {"type": 'L', "value": 'PRESS'}, None),
        ("USE_OBJECT", {"type": 'O', "value": 'PRESS'}, None),
        ("SET_USE_3DCURSOR", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True}, None),
        ("SET_USE_SELECTED", {"type": 'RIGHTMOUSE', "value": 'CLICK', "ctrl": True}, None),
    ])

    return keymap


def km_bevel_modal_map(_params):
    items = []
    keymap = (
        "Bevel Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("VALUE_OFFSET", {"type": 'A', "value": 'PRESS', "any": True}, None),
        ("VALUE_PROFILE", {"type": 'P', "value": 'PRESS', "any": True}, None),
        ("VALUE_SEGMENTS", {"type": 'S', "value": 'PRESS', "any": True}, None),
        ("SEGMENTS_UP", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "any": True}, None),
        ("SEGMENTS_UP", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "any": True}, None),
        ("SEGMENTS_DOWN", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "any": True}, None),
        ("SEGMENTS_DOWN", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "any": True}, None),
        ("OFFSET_MODE_CHANGE", {"type": 'M', "value": 'PRESS', "any": True}, None),
        ("CLAMP_OVERLAP_TOGGLE", {"type": 'C', "value": 'PRESS', "any": True}, None),
        ("AFFECT_CHANGE", {"type": 'V', "value": 'PRESS', "any": True}, None),
        ("HARDEN_NORMALS_TOGGLE", {"type": 'H', "value": 'PRESS', "any": True}, None),
        ("MARK_SEAM_TOGGLE", {"type": 'U', "value": 'PRESS', "any": True}, None),
        ("MARK_SHARP_TOGGLE", {"type": 'K', "value": 'PRESS', "any": True}, None),
        ("OUTER_MITER_CHANGE", {"type": 'O', "value": 'PRESS', "any": True}, None),
        ("INNER_MITER_CHANGE", {"type": 'I', "value": 'PRESS', "any": True}, None),
        ("PROFILE_TYPE_CHANGE", {"type": 'Z', "value": 'PRESS', "any": True}, None),
        ("VERTEX_MESH_CHANGE", {"type": 'N', "value": 'PRESS', "any": True}, None),
    ])

    return keymap


def km_view3d_fly_modal(_params):
    items = []
    keymap = (
        "View3D Fly Modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'ANY', "any": True}, None),
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'ANY', "any": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'SPACE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("ACCELERATE", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "any": True, "repeat": True}, None),
        ("DECELERATE", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "any": True, "repeat": True}, None),
        ("ACCELERATE", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "any": True}, None),
        ("DECELERATE", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("PAN_ENABLE", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "any": True}, None),
        ("PAN_DISABLE", {"type": 'MIDDLEMOUSE', "value": 'RELEASE', "any": True}, None),
        ("FORWARD", {"type": 'W', "value": 'PRESS', "repeat": True}, None),
        ("BACKWARD", {"type": 'S', "value": 'PRESS', "repeat": True}, None),
        ("LEFT", {"type": 'A', "value": 'PRESS', "repeat": True}, None),
        ("RIGHT", {"type": 'D', "value": 'PRESS', "repeat": True}, None),
        ("UP", {"type": 'E', "value": 'PRESS', "repeat": True}, None),
        ("DOWN", {"type": 'Q', "value": 'PRESS', "repeat": True}, None),
        ("UP", {"type": 'R', "value": 'PRESS', "repeat": True}, None),
        ("DOWN", {"type": 'F', "value": 'PRESS', "repeat": True}, None),
        ("FORWARD", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("BACKWARD", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("LEFT", {"type": 'LEFT_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("RIGHT", {"type": 'RIGHT_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("AXIS_LOCK_X", {"type": 'X', "value": 'PRESS'}, None),
        ("AXIS_LOCK_Z", {"type": 'Z', "value": 'PRESS'}, None),
        ("PRECISION_ENABLE", {"type": 'LEFT_ALT', "value": 'PRESS', "any": True}, None),
        ("PRECISION_DISABLE", {"type": 'LEFT_ALT', "value": 'RELEASE', "any": True}, None),
        ("PRECISION_ENABLE", {"type": 'RIGHT_ALT', "value": 'PRESS', "any": True}, None),
        ("PRECISION_DISABLE", {"type": 'RIGHT_ALT', "value": 'RELEASE', "any": True}, None),
        ("PRECISION_ENABLE", {"type": 'LEFT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("PRECISION_DISABLE", {"type": 'LEFT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("PRECISION_ENABLE", {"type": 'RIGHT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("PRECISION_DISABLE", {"type": 'RIGHT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("FREELOOK_ENABLE", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": True}, None),
        ("FREELOOK_DISABLE", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("FREELOOK_ENABLE", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": True}, None),
        ("FREELOOK_DISABLE", {"type": 'RIGHT_CTRL', "value": 'RELEASE', "any": True}, None),
    ])

    return keymap


def km_view3d_walk_modal(_params):
    items = []
    keymap = (
        "View3D Walk Modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'ANY', "any": True}, None),
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'ANY', "any": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("FAST_ENABLE", {"type": 'LEFT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("FAST_DISABLE", {"type": 'LEFT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("FAST_ENABLE", {"type": 'RIGHT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("FAST_DISABLE", {"type": 'RIGHT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("SLOW_ENABLE", {"type": 'LEFT_ALT', "value": 'PRESS', "any": True}, None),
        ("SLOW_DISABLE", {"type": 'LEFT_ALT', "value": 'RELEASE', "any": True}, None),
        ("SLOW_ENABLE", {"type": 'RIGHT_ALT', "value": 'PRESS', "any": True}, None),
        ("SLOW_DISABLE", {"type": 'RIGHT_ALT', "value": 'RELEASE', "any": True}, None),
        ("FORWARD", {"type": 'W', "value": 'PRESS', "any": True}, None),
        ("BACKWARD", {"type": 'S', "value": 'PRESS', "any": True}, None),
        ("LEFT", {"type": 'A', "value": 'PRESS', "any": True}, None),
        ("RIGHT", {"type": 'D', "value": 'PRESS', "any": True}, None),
        ("UP", {"type": 'E', "value": 'PRESS', "any": True}, None),
        ("DOWN", {"type": 'Q', "value": 'PRESS', "any": True}, None),
        ("LOCAL_UP", {"type": 'R', "value": 'PRESS', "any": True}, None),
        ("LOCAL_DOWN", {"type": 'F', "value": 'PRESS', "any": True}, None),
        ("FORWARD_STOP", {"type": 'W', "value": 'RELEASE', "any": True}, None),
        ("BACKWARD_STOP", {"type": 'S', "value": 'RELEASE', "any": True}, None),
        ("LEFT_STOP", {"type": 'A', "value": 'RELEASE', "any": True}, None),
        ("RIGHT_STOP", {"type": 'D', "value": 'RELEASE', "any": True}, None),
        ("UP_STOP", {"type": 'E', "value": 'RELEASE', "any": True}, None),
        ("DOWN_STOP", {"type": 'Q', "value": 'RELEASE', "any": True}, None),
        ("LOCAL_UP_STOP", {"type": 'R', "value": 'RELEASE', "any": True}, None),
        ("LOCAL_DOWN_STOP", {"type": 'F', "value": 'RELEASE', "any": True}, None),
        ("FORWARD", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("BACKWARD", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
        ("LEFT", {"type": 'LEFT_ARROW', "value": 'PRESS'}, None),
        ("RIGHT", {"type": 'RIGHT_ARROW', "value": 'PRESS'}, None),
        ("FORWARD_STOP", {"type": 'UP_ARROW', "value": 'RELEASE', "any": True}, None),
        ("BACKWARD_STOP", {"type": 'DOWN_ARROW', "value": 'RELEASE', "any": True}, None),
        ("LEFT_STOP", {"type": 'LEFT_ARROW', "value": 'RELEASE', "any": True}, None),
        ("RIGHT_STOP", {"type": 'RIGHT_ARROW', "value": 'RELEASE', "any": True}, None),
        ("GRAVITY_TOGGLE", {"type": 'TAB', "value": 'PRESS'}, None),
        ("GRAVITY_TOGGLE", {"type": 'G', "value": 'PRESS'}, None),
        ("JUMP", {"type": 'V', "value": 'PRESS', "any": True}, None),
        ("JUMP_STOP", {"type": 'V', "value": 'RELEASE', "any": True}, None),
        ("TELEPORT", {"type": 'SPACE', "value": 'PRESS', "any": True}, None),
        ("TELEPORT", {"type": 'MIDDLEMOUSE', "value": 'ANY', "any": True}, None),
        ("ACCELERATE", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "any": True, "repeat": True}, None),
        ("DECELERATE", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "any": True, "repeat": True}, None),
        ("ACCELERATE", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "any": True}, None),
        ("DECELERATE", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "any": True}, None),
        ("AXIS_LOCK_Z", {"type": 'Z', "value": 'PRESS'}, None),
        ("INCREASE_JUMP", {"type": 'PERIOD', "value": 'PRESS', "any": True}, None),
        ("DECREASE_JUMP", {"type": 'COMMA', "value": 'PRESS', "any": True}, None),
    ])

    return keymap


def km_view3d_rotate_modal(_params):
    items = []
    keymap = (
        "View3D Rotate Modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("AXIS_SNAP_ENABLE", {"type": 'LEFT_ALT', "value": 'PRESS', "any": True}, None),
        ("AXIS_SNAP_DISABLE", {"type": 'LEFT_ALT', "value": 'RELEASE', "any": True}, None),
        ("AXIS_SNAP_ENABLE", {"type": 'RIGHT_ALT', "value": 'PRESS', "any": True}, None),
        ("AXIS_SNAP_DISABLE", {"type": 'RIGHT_ALT', "value": 'RELEASE', "any": True}, None),
    ])

    return keymap


def km_view3d_move_modal(_params):
    items = []
    keymap = (
        "View3D Move Modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
    ])

    return keymap


def km_view3d_zoom_modal(_params):
    items = []
    keymap = (
        "View3D Zoom Modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
    ])

    return keymap


def km_view3d_dolly_modal(_params):
    items = []
    keymap = (
        "View3D Dolly Modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
    ])

    return keymap


def km_paint_stroke_modal(_params):
    items = []
    keymap = (
        "Paint Stroke Modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
    ])

    return keymap


def km_sculpt_expand_modal(_params):
    items = []
    keymap = (
        "Sculpt Expand Modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'RELEASE', "any": True}, None),
        ("INVERT", {"type": 'F', "value": 'PRESS', "any": True}, None),
        ("PRESERVE", {"type": 'E', "value": 'PRESS', "any": True}, None),
        ("GRADIENT", {"type": 'G', "value": 'PRESS', "any": True}, None),
        ("RECURSION_STEP_GEODESIC", {"type": 'R', "value": 'PRESS'}, None),
        ("RECURSION_STEP_TOPOLOGY", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("MOVE_TOGGLE", {"type": 'SPACE', "value": 'ANY', "any": True}, None),
        *((e, {"type": numseq_1[i], "value": 'PRESS', "any": True}, None)
          for numseq_1 in (NUMBERS_1, NUMPAD_1)
          for i, e in enumerate(
            ('FALLOFF_GEODESICS', 'FALLOFF_TOPOLOGY', 'FALLOFF_TOPOLOGY_DIAGONALS', 'FALLOFF_SPHERICAL'),
        )),
        ("SNAP_TOGGLE", {"type": 'LEFT_CTRL', "value": 'ANY'}, None),
        ("SNAP_TOGGLE", {"type": 'RIGHT_CTRL', "value": 'ANY'}, None),
        ("LOOP_COUNT_INCREASE", {"type": 'W', "value": 'PRESS', "any": True, "repeat": True}, None),
        ("LOOP_COUNT_DECREASE", {"type": 'Q', "value": 'PRESS', "any": True, "repeat": True}, None),
        ("BRUSH_GRADIENT_TOGGLE", {"type": 'B', "value": 'PRESS', "any": True}, None),
        ("TEXTURE_DISTORTION_INCREASE", {"type": 'Y', "value": 'PRESS'}, None),
        ("TEXTURE_DISTORTION_DECREASE", {"type": 'T', "value": 'PRESS'}, None),
    ])
    return keymap


def km_sculpt_mesh_filter_modal_map(_params):
    items = []
    keymap = (
        "Mesh Filter Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'RELEASE', "any": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'RELEASE', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'RELEASE', "any": True}, None),

        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
    ])
    return keymap


def km_curve_pen_modal_map(_params):
    items = []
    keymap = (
        "Curve Pen Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("FREE_ALIGN_TOGGLE", {"type": 'LEFT_SHIFT', "value": 'ANY', "any": True}, None),
        ("MOVE_ADJACENT", {"type": 'LEFT_CTRL', "value": 'ANY', "any": True}, None),
        ("MOVE_ENTIRE", {"type": 'SPACE', "value": 'ANY', "any": True}, None),
        ("LOCK_ANGLE", {"type": 'LEFT_ALT', "value": 'ANY', "any": True}, None),
        ("LINK_HANDLES", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": True}, None),
    ])

    return keymap


def km_node_link_modal_map(_params):
    items = []
    keymap = (
        "Node Link Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("BEGIN", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'RELEASE', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("SWAP", {"type": 'LEFT_ALT', "value": 'ANY', "any": True}, None),
        ("SWAP", {"type": 'RIGHT_ALT', "value": 'ANY', "any": True}, None),
    ])

    return keymap


def km_node_resize_modal_map(_params):
    items = []
    keymap = (
        "Node Resize Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("BEGIN", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("SNAP_INVERT_ON", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_INVERT_OFF", {"type": 'RIGHT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("SNAP_INVERT_ON", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_INVERT_OFF", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": True}, None),
    ])

    return keymap


def km_sequencer_slip_modal_map(_params):
    items = []
    keymap = (
        "Slip Modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'RELEASE', "any": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'RELEASE', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'RELEASE', "any": True}, None),
        ("CONFIRM", {"type": 'SPACE', "value": 'RELEASE', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'ANY', "any": True}, None),
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("PRECISION_ENABLE", {"type": 'LEFT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("PRECISION_DISABLE", {"type": 'LEFT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("PRECISION_ENABLE", {"type": 'RIGHT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("PRECISION_DISABLE", {"type": 'RIGHT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("CLAMP_TOGGLE", {"type": 'C', "value": 'PRESS', "any": True}, None),
    ])

    return keymap

# Fallback for gizmos that don't have custom a custom key-map.


def km_generic_gizmo(_params):
    keymap = (
        "Generic Gizmo",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": _template_items_gizmo_tweak_value()},
    )

    return keymap


def km_generic_gizmo_drag(_params):
    keymap = (
        "Generic Gizmo Drag",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": _template_items_gizmo_tweak_value_drag()},
    )

    return keymap


def km_generic_gizmo_click_drag(_params):
    keymap = (
        "Generic Gizmo Click Drag",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": _template_items_gizmo_tweak_value_click_drag()},
    )

    return keymap


def km_generic_gizmo_maybe_drag(params):
    keymap = (
        "Generic Gizmo Maybe Drag",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items":
         _template_items_gizmo_tweak_value_drag()
         if params.use_gizmo_drag else
         _template_items_gizmo_tweak_value()
         },
    )

    return keymap


def km_generic_gizmo_select(_params):
    keymap = (
        "Generic Gizmo Select",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        # TODO, currently in C code.
        {"items": _template_items_gizmo_tweak_value()},
    )

    return keymap


def km_generic_gizmo_tweak_modal_map(_params):
    keymap = (
        "Generic Gizmo Tweak Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": [
            ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
            ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
            ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
            ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
            ("PRECISION_ON", {"type": 'RIGHT_SHIFT', "value": 'PRESS', "any": True}, None),
            ("PRECISION_OFF", {"type": 'RIGHT_SHIFT', "value": 'RELEASE', "any": True}, None),
            ("PRECISION_ON", {"type": 'LEFT_SHIFT', "value": 'PRESS', "any": True}, None),
            ("PRECISION_OFF", {"type": 'LEFT_SHIFT', "value": 'RELEASE', "any": True}, None),
            ("SNAP_ON", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": True}, None),
            ("SNAP_OFF", {"type": 'RIGHT_CTRL', "value": 'RELEASE', "any": True}, None),
            ("SNAP_ON", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": True}, None),
            ("SNAP_OFF", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": True}, None),
        ]},
    )
    return keymap


# ------------------------------------------------------------------------------
# Popup Key-maps

def km_popup_toolbar(_params):
    return (
        "Toolbar Popup",
        {"space_type": 'EMPTY', "region_type": 'TEMPORARY'},
        {"items": [
            op_tool("builtin.cursor", {"type": 'SPACE', "value": 'PRESS'}),
            op_tool("builtin.select", {"type": 'W', "value": 'PRESS'}),
            op_tool("builtin.select_lasso", {"type": 'L', "value": 'PRESS'}),
            op_tool("builtin.transform", {"type": 'T', "value": 'PRESS'}),
            op_tool("builtin.measure", {"type": 'M', "value": 'PRESS'}),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (Generic)
#
# Named are auto-generated based on the tool name and it's toolbar.

def km_generic_tool_annotate(params):
    return (
        "Generic Tool: Annotate",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": [
            ("gpencil.annotate", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("mode", 'DRAW'), ("wait_for_input", False)]}),
            ("gpencil.annotate", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),
        ]},
    )


def km_generic_tool_annotate_line(params):
    return (
        "Generic Tool: Annotate Line",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": [
            ("gpencil.annotate", params.tool_maybe_tweak_event,
             {"properties": [("mode", 'DRAW_STRAIGHT'), ("wait_for_input", False)]}),
            ("gpencil.annotate", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),
        ]},
    )


def km_generic_tool_annotate_polygon(params):
    return (
        "Generic Tool: Annotate Polygon",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": [
            ("gpencil.annotate", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("mode", 'DRAW_POLY'), ("wait_for_input", False)]}),
            ("gpencil.annotate", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),
        ]},
    )


def km_generic_tool_annotate_eraser(params):
    return (
        "Generic Tool: Annotate Eraser",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": [
            ("gpencil.annotate", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),
            ("gpencil.annotate", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),
        ]},
    )


def km_image_editor_tool_generic_sample(params):
    return (
        "Image Editor Tool: Sample",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("image.sample", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (UV Editor)

def km_image_editor_tool_uv_cursor(params):
    return (
        "Image Editor Tool: Uv, Cursor",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("uv.cursor_set", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            # Don't use `tool_maybe_tweak_event` since it conflicts with `PRESS` that places the cursor.
            ("transform.translate", params.tool_tweak_event,
             {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        ]},
    )


def km_image_editor_tool_uv_select(params, *, fallback):
    return (
        _fallback_id("Image Editor Tool: Uv, Tweak", fallback),
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and (params.select_mouse == 'RIGHTMOUSE')) else _template_items_tool_select(
                params, "uv.select", "uv.cursor_set", fallback=fallback)),
            *([] if params.use_fallback_tool_select_handled else
              _template_uv_select(
                  type=params.select_mouse,
                  value=params.select_mouse_value,
                  select_passthrough=params.use_tweak_select_passthrough,
                  legacy=params.legacy,
            )),
        ]},
    )


def km_image_editor_tool_uv_select_box(params, *, fallback):
    return (
        _fallback_id("Image Editor Tool: Uv, Select Box", fallback),
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "uv.select_box",
                # Don't use `tool_maybe_tweak_event`, see comment for this slot.
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   params.tool_tweak_event))),
        ]},
    )


def km_image_editor_tool_uv_select_circle(params, *, fallback):
    return (
        _fallback_id("Image Editor Tool: Uv, Select Circle", fallback),
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "uv.select_circle",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   {"type": params.tool_mouse, "value": 'PRESS'}),
                properties=[("wait_for_input", False)])),
            # No selection fallback since this operates on press.
        ]},
    )


def km_image_editor_tool_uv_select_lasso(params, *, fallback):
    return (
        _fallback_id("Image Editor Tool: Uv, Select Lasso", fallback),
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},

        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "uv.select_lasso",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   params.tool_tweak_event))),
        ]},
    )


def km_image_editor_tool_uv_rip_region(params):
    return (
        "Image Editor Tool: Uv, Rip Region",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("uv.rip_move", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
        ]},
    )


def km_image_editor_tool_uv_grab(params):
    return (
        "Image Editor Tool: Uv, Grab",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.uv_sculpt_grab", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            ("sculpt.uv_sculpt_grab", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("use_invert", True)]}),
            ("sculpt.uv_sculpt_relax", {"type": params.tool_mouse, "value": 'PRESS', "shift": True}, None),
            ("wm.radial_control", {"type": 'F', "value": 'PRESS'},
             {"properties": [("data_path_primary", "tool_settings.uv_sculpt.size"), ], }),
            ("wm.radial_control", {"type": 'F', "value": 'PRESS', "shift": True},
             {"properties": [("data_path_primary", "tool_settings.uv_sculpt.strength"), ], }),
        ]},
    )


def km_image_editor_tool_uv_relax(params):
    return (
        "Image Editor Tool: Uv, Relax",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.uv_sculpt_relax", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            ("sculpt.uv_sculpt_relax", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("use_invert", True)]}),
            ("sculpt.uv_sculpt_relax", {"type": params.tool_mouse, "value": 'PRESS', "shift": True}, None),
            ("wm.radial_control", {"type": 'F', "value": 'PRESS'},
             {"properties": [("data_path_primary", "tool_settings.uv_sculpt.size"), ], }),
            ("wm.radial_control", {"type": 'F', "value": 'PRESS', "shift": True},
             {"properties": [("data_path_primary", "tool_settings.uv_sculpt.strength"), ], }),
        ]},
    )


def km_image_editor_tool_uv_pinch(params):
    return (
        "Image Editor Tool: Uv, Pinch",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.uv_sculpt_pinch", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            ("sculpt.uv_sculpt_pinch", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("use_invert", True)]}),
            ("sculpt.uv_sculpt_relax", {"type": params.tool_mouse, "value": 'PRESS', "shift": True}, None),
            ("wm.radial_control", {"type": 'F', "value": 'PRESS'},
             {"properties": [("data_path_primary", "tool_settings.uv_sculpt.size"), ], }),
            ("wm.radial_control", {"type": 'F', "value": 'PRESS', "shift": True},
             {"properties": [("data_path_primary", "tool_settings.uv_sculpt.strength"), ], }),
        ]},
    )


def km_image_editor_tool_uv_move(params):
    return (
        "Image Editor Tool: Uv, Move",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.translate", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_image_editor_tool_uv_rotate(params):
    return (
        "Image Editor Tool: Uv, Rotate",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.rotate", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_image_editor_tool_uv_scale(params):
    return (
        "Image Editor Tool: Uv, Scale",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.resize", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (Mask Editor)

def km_image_editor_tool_mask_cursor(params):
    return (
        "Image Editor Tool: Mask, Cursor",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("uv.cursor_set", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            # Don't use `tool_maybe_tweak_event` since it conflicts with `PRESS` that places the cursor.
            ("transform.translate", params.tool_tweak_event,
             {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        ]},
    )


def km_image_editor_tool_mask_select(params, *, fallback):
    return (
        _fallback_id("Image Editor Tool: Mask, Tweak", fallback),
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and (params.select_mouse == 'RIGHTMOUSE')) else _template_items_tool_select(
                params, "mask.select", "uv.cursor_set", fallback=fallback)),
            *([] if params.use_fallback_tool_select_handled else
              _template_mask_select(
                  type=params.select_mouse,
                  value=params.select_mouse_value,
                  select_passthrough=params.use_tweak_select_passthrough,
                  legacy=params.legacy,
            )),
        ]},
    )


def km_image_editor_tool_mask_select_box(params, *, fallback):
    return (
        _fallback_id("Image Editor Tool: Mask, Select Box", fallback),
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "mask.select_box",
                # Don't use `tool_maybe_tweak_event`, see comment for this slot.
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   params.tool_tweak_event))),
        ]},
    )


def km_image_editor_tool_mask_select_circle(params, *, fallback):
    return (
        _fallback_id("Image Editor Tool: Mask, Select Circle", fallback),
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "mask.select_circle",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   {"type": params.tool_mouse, "value": 'PRESS'}),
                properties=[("wait_for_input", False)])),
        ]},
    )


def km_image_editor_tool_mask_select_lasso(params, *, fallback):
    return (
        _fallback_id("Image Editor Tool: Mask, Select Lasso", fallback),
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},

        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "mask.select_lasso",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   params.tool_tweak_event))),
        ]},
    )


def km_image_editor_tool_mask_move(params):
    return (
        "Image Editor Tool: Mask, Move",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.translate", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_image_editor_tool_mask_rotate(params):
    return (
        "Image Editor Tool: Mask, Rotate",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.rotate", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_image_editor_tool_mask_scale(params):
    return (
        "Image Editor Tool: Mask, Scale",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.resize", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_image_editor_tool_mask_transform(params):
    return (
        "Image Editor Tool: Mask, Transform",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.resize", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_image_editor_tool_mask_primitive_square(params):
    return (
        "Image Editor Tool: Mask, Box",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("mask.primitive_square_add", {"type": 'LEFTMOUSE', "value": 'PRESS'},
             {"properties": []}),
        ]},
    )


def km_image_editor_tool_mask_primitive_circle(params):
    return (
        "Image Editor Tool: Mask, Circle",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("mask.primitive_circle_add", {"type": 'LEFTMOUSE', "value": 'PRESS'},
             {"properties": []}),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (Node Editor)

def km_node_editor_tool_select(params, *, fallback):
    return (
        _fallback_id("Node Tool: Tweak", fallback),
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            # The node key-map already selects, leave this empty.
            # NOTE: intentionally don't check `fallback` here (unlike other tweak tool checks).
            # as this should only be used on LMB select which would otherwise activate on click, not press.
            *([] if (params.select_mouse == 'RIGHTMOUSE') else
              _template_node_select(type=params.select_mouse, value='PRESS', select_passthrough=True)),
        ]},
    )


def km_node_editor_tool_select_box(params, *, fallback):
    return (
        _fallback_id("Node Tool: Select Box", fallback),
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "node.select_box",
                # Don't use `tool_maybe_tweak_event`, see comment for this slot.
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   params.tool_tweak_event),
                properties=[("tweak", True)],
            )),
            *([] if (params.select_mouse == 'RIGHTMOUSE') else
              _template_node_select(type='LEFTMOUSE', value='PRESS', select_passthrough=True)),
        ]},
    )


def km_node_editor_tool_select_lasso(params, *, fallback):
    return (
        _fallback_id("Node Tool: Select Lasso", fallback),
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "node.select_lasso",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   params.tool_tweak_event),
                properties=[("tweak", True)]))
        ]},
    )


def km_node_editor_tool_select_circle(params, *, fallback):
    return (
        _fallback_id("Node Tool: Select Circle", fallback),
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "node.select_circle",
                # Why circle select should be used on tweak?
                # So that RMB or Shift-RMB is still able to set an element as active.
                type=params.select_mouse if (fallback and params.use_fallback_tool_select_mouse) else params.tool_mouse,
                value='CLICK_DRAG' if (fallback and params.use_fallback_tool_select_mouse) else 'PRESS',
                properties=[("wait_for_input", False)])),
        ]},
    )


def km_node_editor_tool_links_cut(params):
    return (
        "Node Tool: Links Cut",
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("node.links_cut", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


def km_node_editor_tool_links_mute(params):
    return (
        "Node Tool: Mute Links",
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("node.links_mute", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


def km_node_editor_tool_add_reroute(params):
    return (
        "Node Tool: Add Reroute",
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("node.add_reroute", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (3D View, Generic)

def km_3d_view_tool_cursor(params):
    return (
        "3D View Tool: Cursor",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("view3d.cursor3d", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            # Don't use `tool_maybe_tweak_event` since it conflicts with `PRESS` that places the cursor.
            ("transform.translate", params.tool_tweak_event,
             {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        ]},
    )


def km_3d_view_tool_text_select(_params):
    return (
        "3D View Tool: Edit Text, Select Text",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("font.selection_set", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
            ("font.select_word", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ]},
    )


def km_3d_view_tool_select(params, *, fallback):
    return (
        _fallback_id("3D View Tool: Tweak", fallback),
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and (params.select_mouse == 'RIGHTMOUSE')) else _template_items_tool_select(
                params, "view3d.select", "view3d.cursor3d", fallback=fallback)),
            *([] if params.use_fallback_tool_select_handled else
              _template_view3d_select(
                  type=params.select_mouse,
                  value=params.select_mouse_value,
                  legacy=params.legacy,
                  select_passthrough=params.use_tweak_select_passthrough,
                  exclude_mod="ctrl",
            )),
            # Instance weight/vertex selection actions here, see code-comment for details.
            *([] if (params.select_mouse == 'RIGHTMOUSE') else _template_view3d_paint_mask_select_loop(params)),
        ]},
    )


def km_3d_view_tool_select_box(params, *, fallback):
    return (
        _fallback_id("3D View Tool: Select Box", fallback),
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions(
                "view3d.select_box",
                # Don't use `tool_maybe_tweak_event`, see comment for this slot.
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   params.tool_tweak_event))),
            # Instance weight/vertex selection actions here, see code-comment for details.
            *([] if (params.select_mouse == 'RIGHTMOUSE') else _template_view3d_paint_mask_select_loop(params)),
        ]},
    )


def km_3d_view_tool_select_circle(params, *, fallback):
    return (
        _fallback_id("3D View Tool: Select Circle", fallback),
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "view3d.select_circle",
                # Why circle select should be used on tweak?
                # So that RMB or Shift-RMB is still able to set an element as active.
                type=params.select_mouse if (fallback and params.use_fallback_tool_select_mouse) else params.tool_mouse,
                value='CLICK_DRAG' if (fallback and params.use_fallback_tool_select_mouse) else 'PRESS',
                properties=[("wait_for_input", False)])),
            # Instance weight/vertex selection actions here, see code-comment for details.
            *([] if (params.select_mouse == 'RIGHTMOUSE') else _template_view3d_paint_mask_select_loop(params)),
        ]},
    )


def km_3d_view_tool_select_lasso(params, *, fallback):
    return (
        _fallback_id("3D View Tool: Select Lasso", fallback),
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions(
                "view3d.select_lasso",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   params.tool_tweak_event))),
            # Instance weight/vertex selection actions here, see code-comment for details.
            *([] if (params.select_mouse == 'RIGHTMOUSE') else _template_view3d_paint_mask_select_loop(params)),
        ]}
    )


def km_3d_view_tool_transform(params):
    return (
        "3D View Tool: Transform",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.from_gizmo", {**params.tool_maybe_tweak_event, **params.tool_modifier}, None),
        ]},
    )


def km_3d_view_tool_move(params):
    return (
        "3D View Tool: Move",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.translate", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_rotate(params):
    return (
        "3D View Tool: Rotate",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.rotate", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_scale(params):
    return (
        "3D View Tool: Scale",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.resize", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_shear(params):
    # Don't use "tool_maybe_tweak_value" since we would loose tweak direction support.
    return (
        "3D View Tool: Shear",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.shear",
             {"type": params.tool_mouse, "value": 'CLICK_DRAG', "direction": 'NORTH', **params.tool_modifier},
             {"properties": [("release_confirm", True), ("orient_axis_ortho", 'Y')]}),
            ("transform.shear",
             {"type": params.tool_mouse, "value": 'CLICK_DRAG', "direction": 'SOUTH', **params.tool_modifier},
             {"properties": [("release_confirm", True), ("orient_axis_ortho", 'Y')]}),

            # Use as fallback to catch diagonals too.
            ("transform.shear",
             {"type": params.tool_mouse, "value": 'CLICK_DRAG', **params.tool_modifier},
             {"properties": [("release_confirm", True), ("orient_axis_ortho", 'X')]}),
        ]},
    )


def km_3d_view_tool_bend(params):
    return (
        "3D View Tool: Bend",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            # No need for `tool_modifier` since this takes all input.
            ("transform.bend", params.tool_maybe_tweak_event,
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_measure(params):
    return (
        "3D View Tool: Measure",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("view3d.ruler_add", params.tool_maybe_tweak_event, None),
            ("view3d.ruler_remove", {"type": 'X', "value": 'PRESS'}, None),
            ("view3d.ruler_remove", {"type": 'DEL', "value": 'PRESS'}, None),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (3D View, Pose Mode)

def km_3d_view_tool_pose_breakdowner(params):
    return (
        "3D View Tool: Pose, Breakdowner",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("pose.breakdown", {**params.tool_maybe_tweak_event, **params.tool_modifier}, None),
        ]},
    )


def km_3d_view_tool_pose_push(params):
    return (
        "3D View Tool: Pose, Push",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("pose.push", {**params.tool_maybe_tweak_event, **params.tool_modifier}, None),
        ]},
    )


def km_3d_view_tool_pose_relax(params):
    return (
        "3D View Tool: Pose, Relax",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("pose.relax", {**params.tool_maybe_tweak_event, **params.tool_modifier}, None),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (3D View, Edit Armature)

def km_3d_view_tool_edit_armature_roll(params):
    return (
        "3D View Tool: Edit Armature, Roll",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.transform", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True), ("mode", 'BONE_ROLL')]}),
        ]},
    )


def km_3d_view_tool_edit_armature_bone_size(params):
    return (
        "3D View Tool: Edit Armature, Bone Size",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.transform", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True), ("mode", 'BONE_ENVELOPE')]}),
        ]},
    )


def km_3d_view_tool_edit_armature_bone_envelope(params):
    return (
        "3D View Tool: Edit Armature, Bone Envelope",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},

        {"items": [
            ("transform.bbone_resize", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_armature_extrude(params):
    return (
        "3D View Tool: Edit Armature, Extrude",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("armature.extrude_move", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_armature_extrude_to_cursor(params):
    return (
        "3D View Tool: Edit Armature, Extrude to Cursor",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("armature.click_extrude", {"type": params.tool_mouse, "value": 'PRESS', **params.tool_modifier}, None),
            # Support LMB click-drag for RMB key-map.
            *(([] if (params.select_mouse == 'LEFTMOUSE') else [
                ("transform.translate", {"type": params.tool_mouse, "value": 'CLICK_DRAG'},
                 {"properties": [("release_confirm", True)]})
            ])),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (3D View, Object Mode)

def km_3d_view_tool_interactive_add(params):
    return (
        "3D View Tool: Object, Add Primitive",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("view3d.interactive_add",
             {**params.tool_maybe_tweak_event,
              # While "Alt" isn't an important shortcut to support,
              # when the preferences to activate tools when "Alt" is held is used,
              # it's illogical not to support holding "Alt", even though it is not required.
              **({"any": True} if "alt" in params.tool_modifier else any_except("alt"))},
             {"properties": [("wait_for_input", False)]}),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (3D View, Edit Mesh)

def km_3d_view_tool_edit_mesh_extrude_region(params):
    return (
        "3D View Tool: Edit Mesh, Extrude Region",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.extrude_context_move", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_extrude_manifold(params):
    return (
        "3D View Tool: Edit Mesh, Extrude Manifold",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.extrude_manifold", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [
                 ("MESH_OT_extrude_region", [("use_dissolve_ortho_edges", True)]),
                 ("TRANSFORM_OT_translate", [
                     ("release_confirm", True),
                     ("use_automerge_and_split", True),
                     ("constraint_axis", (False, False, True)),
                     ("orient_type", 'NORMAL'),
                 ]),
             ]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_extrude_along_normals(params):
    return (
        "3D View Tool: Edit Mesh, Extrude Along Normals",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.extrude_region_shrink_fatten", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("TRANSFORM_OT_shrink_fatten", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_extrude_individual(params):
    return (
        "3D View Tool: Edit Mesh, Extrude Individual",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.extrude_faces_move", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("TRANSFORM_OT_shrink_fatten", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_extrude_to_cursor(params):
    return (
        "3D View Tool: Edit Mesh, Extrude to Cursor",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            # No need for `tool_modifier` since this takes all input.
            ("mesh.dupli_extrude_cursor", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            # Support LMB click-drag for RMB key-map.
            *(([] if (params.select_mouse == 'LEFTMOUSE') else [
                ("transform.translate", {"type": params.tool_mouse, "value": 'CLICK_DRAG'},
                 {"properties": [("release_confirm", True)]})
            ])),
        ]},
    )


def km_3d_view_tool_edit_mesh_inset_faces(params):
    return (
        "3D View Tool: Edit Mesh, Inset Faces",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.inset", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_bevel(params):
    return (
        "3D View Tool: Edit Mesh, Bevel",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.bevel", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_loop_cut(params):
    return (
        "3D View Tool: Edit Mesh, Loop Cut",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            # No need for `tool_modifier` since this takes all input.
            ("mesh.loopcut_slide", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("TRANSFORM_OT_edge_slide", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_offset_edge_loop_cut(params):
    return (
        "3D View Tool: Edit Mesh, Offset Edge Loop Cut",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            # No need for `tool_modifier` since this takes all input.
            ("mesh.offset_edge_loops_slide", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


def km_3d_view_tool_edit_mesh_knife(params):
    return (
        "3D View Tool: Edit Mesh, Knife",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            # No need for `tool_modifier` since this takes all input.
            ("mesh.knife_tool", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("wait_for_input", False)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_bisect(params):
    return (
        "3D View Tool: Edit Mesh, Bisect",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            # No need for `tool_modifier` since this takes all input.
            ("mesh.bisect", params.tool_maybe_tweak_event, None),
        ]},
    )


def km_3d_view_tool_edit_mesh_poly_build(params):
    return (
        "3D View Tool: Edit Mesh, Poly Build",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            # No need for `tool_modifier` since this takes all input.
            ("mesh.polybuild_extrude_at_cursor_move", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
            ("mesh.polybuild_face_at_cursor_move", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
            ("mesh.polybuild_delete_at_cursor", {"type": params.tool_mouse, "value": 'CLICK', "shift": True}, None),
        ]},
    )


def km_3d_view_tool_edit_mesh_spin(params):
    return (
        "3D View Tool: Edit Mesh, Spin",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.spin", {**params.tool_maybe_tweak_event, **params.tool_modifier}, None),
        ]},
    )


def km_3d_view_tool_edit_mesh_spin_duplicate(params):
    return (
        "3D View Tool: Edit Mesh, Spin Duplicates",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.spin", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("dupli", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_smooth(params):
    return (
        "3D View Tool: Edit Mesh, Smooth",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.vertices_smooth", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("wait_for_input", False)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_randomize(params):
    return (
        "3D View Tool: Edit Mesh, Randomize",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.vertex_random", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("wait_for_input", False)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_edge_slide(params):
    return (
        "3D View Tool: Edit Mesh, Edge Slide",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.edge_slide", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_vertex_slide(params):
    return (
        "3D View Tool: Edit Mesh, Vertex Slide",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.vert_slide", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_shrink_fatten(params):
    return (
        "3D View Tool: Edit Mesh, Shrink/Fatten",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.shrink_fatten", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_push_pull(params):
    return (
        "3D View Tool: Edit Mesh, Push/Pull",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.push_pull", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_to_sphere(params):
    return (
        "3D View Tool: Edit Mesh, To Sphere",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.tosphere", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_rip_region(params):
    return (
        "3D View Tool: Edit Mesh, Rip Region",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.rip_move", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_rip_edge(params):
    return (
        "3D View Tool: Edit Mesh, Rip Edge",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.rip_edge_move", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (3D View, Edit Curve)

def km_3d_view_tool_edit_curve_draw(params):
    return (
        "3D View Tool: Edit Curve, Draw",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            # No need for `tool_modifier` since this takes all input.
            ("curve.draw", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("wait_for_input", False)]}),
        ]},
    )


def km_3d_view_tool_edit_curves_draw(params):
    return (
        "3D View Tool: Edit Curves, Draw",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            # No need for `tool_modifier` since this takes all input.
            ("curves.draw", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("wait_for_input", False)]}),
        ]},
    )


def km_3d_view_tool_edit_curve_pen(params):
    return (
        "3D View Tool: Edit Curve, Curve Pen",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("curve.pen", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [
                 ("extrude_point", True),
                 ("move_segment", True),
                 ("select_point", True),
                 ("move_point", True),
                 ("close_spline_method", "ON_CLICK"),
             ]}),
            ("curve.pen", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("insert_point", True), ("delete_point", True)]}),
            ("curve.pen", {"type": params.tool_mouse, "value": 'DOUBLE_CLICK'},
             {"properties": [("toggle_vector", True), ("cycle_handle_type", True), ]}),
        ]},
    )


def km_3d_view_tool_edit_curves_pen(params):
    return (
        "3D View Tool: Edit Curves, Pen",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("curves.pen", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [
                 ("extrude_point", True),
                 ("move_segment", True),
                 ("select_point", True),
                 ("move_point", True),
                 ("extrude_handle", "VECTOR"),
             ]}),
            ("curves.pen", {"type": params.tool_mouse, "value": 'PRESS', "shift": True},
             {"properties": [
                 ("extrude_point", True),
                 ("move_segment", True),
                 ("select_point", True),
                 ("move_point", True),
                 ("extrude_handle", "AUTO"),
             ]}),
            ("curves.pen", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("insert_point", True), ("delete_point", True)]}),
            ("curves.pen", {"type": params.tool_mouse, "value": 'DOUBLE_CLICK'},
             {"properties": [("cycle_handle_type", True)]}),
        ]},
    )


def km_3d_view_tool_edit_curve_tilt(params):
    return (
        "3D View Tool: Edit Curve, Tilt",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.tilt", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_curve_radius(params):
    return (
        "3D View Tool: Edit Curve, Radius",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.transform", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("mode", 'CURVE_SHRINKFATTEN'), ("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_curve_randomize(params):
    return (
        "3D View Tool: Edit Curve, Randomize",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.vertex_random", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("wait_for_input", False)]}),
        ]},
    )


def km_3d_view_tool_edit_curve_extrude(params):
    return (
        "3D View Tool: Edit Curve, Extrude",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("curve.extrude_move", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_curve_extrude_to_cursor(params):
    return (
        "3D View Tool: Edit Curve, Extrude to Cursor",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            # No need for `tool_modifier` since this takes all input.
            ("curve.vertex_add", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            # Support LMB click-drag for RMB key-map.
            *(([] if (params.select_mouse == 'LEFTMOUSE') else [
                ("transform.translate", {"type": params.tool_mouse, "value": 'CLICK_DRAG'},
                 {"properties": [("release_confirm", True)]})
            ])),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (3D View, Sculpt)

def km_3d_view_tool_sculpt_box_mask(params):
    return (
        "3D View Tool: Sculpt, Box Mask",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.mask_box_gesture", params.tool_maybe_tweak_event,
             {"properties": [("value", 1.0)]}),
            ("paint.mask_box_gesture", {**params.tool_maybe_tweak_event, "ctrl": True},
             {"properties": [("value", 0.0)]}),
        ]},
    )


def km_3d_view_tool_sculpt_lasso_mask(params):
    return (
        "3D View Tool: Sculpt, Lasso Mask",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.mask_lasso_gesture", params.tool_maybe_tweak_event,
             {"properties": [("value", 1.0)]}),
            ("paint.mask_lasso_gesture", {**params.tool_maybe_tweak_event, "ctrl": True},
             {"properties": [("value", 0.0)]}),
        ]},
    )


def km_3d_view_tool_sculpt_line_mask(params):
    return (
        "3D View Tool: Sculpt, Line Mask",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.mask_line_gesture", params.tool_maybe_tweak_event,
             {"properties": [("value", 1.0)]}),
            ("paint.mask_line_gesture", {**params.tool_maybe_tweak_event, "ctrl": True},
             {"properties": [("value", 0.0)]}),
        ]},
    )


def km_3d_view_tool_sculpt_polyline_mask(params):
    return (
        "3D View Tool: Sculpt, Polyline Mask",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.mask_polyline_gesture", {"type": params.tool_mouse, "value": "PRESS"},
             {"properties": [("value", 1.0)]}),
            ("paint.mask_polyline_gesture", {"type": params.tool_mouse, "value": "PRESS", "ctrl": True},
             {"properties": [("value", 0.0)]}),
        ]},
    )


def km_3d_view_tool_sculpt_box_hide(params):
    return (
        "3D View Tool: Sculpt, Box Hide",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.hide_show", params.tool_maybe_tweak_event,
             {"properties": [("action", 'HIDE')]}),
            ("paint.hide_show", {**params.tool_maybe_tweak_event, "ctrl": True},
             {"properties": [("action", 'SHOW')]}),
            ("paint.hide_show_all", {"type": params.select_mouse, "value": params.select_mouse_value},
             {"properties": [("action", 'SHOW')]}),
        ]},
    )


def km_3d_view_tool_sculpt_lasso_hide(params):
    return (
        "3D View Tool: Sculpt, Lasso Hide",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.hide_show_lasso_gesture", params.tool_maybe_tweak_event,
             {"properties": [("action", 'HIDE')]}),
            ("paint.hide_show_lasso_gesture", {**params.tool_maybe_tweak_event, "ctrl": True},
             {"properties": [("action", 'SHOW')]}),
            ("paint.hide_show_all", {"type": params.select_mouse, "value": params.select_mouse_value},
             {"properties": [("action", 'SHOW')]}),
        ]},
    )


def km_3d_view_tool_sculpt_line_hide(params):
    return (
        "3D View Tool: Sculpt, Line Hide",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.hide_show_line_gesture", params.tool_maybe_tweak_event,
             {"properties": [("action", 'HIDE')]}),
            ("paint.hide_show_line_gesture", {**params.tool_maybe_tweak_event, "ctrl": True},
             {"properties": [("action", 'SHOW')]}),
            ("paint.hide_show_all", {"type": params.select_mouse, "value": params.select_mouse_value},
             {"properties": [("action", 'SHOW')]}),
        ]},
    )


def km_3d_view_tool_sculpt_polyline_hide(params):
    return (
        "3D View Tool: Sculpt, Polyline Hide",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.hide_show_polyline_gesture", {"type": params.tool_mouse, "value": "PRESS"},
             {"properties": [("action", 'HIDE')]}),
            ("paint.hide_show_polyline_gesture", {"type": params.tool_mouse, "value": "PRESS", "ctrl": True},
             {"properties": [("action", 'SHOW')]}),
        ]},
    )


def km_3d_view_tool_sculpt_box_face_set(params):
    return (
        "3D View Tool: Sculpt, Box Face Set",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.face_set_box_gesture", params.tool_maybe_tweak_event, None),
        ]},
    )


def km_3d_view_tool_sculpt_lasso_face_set(params):
    return (
        "3D View Tool: Sculpt, Lasso Face Set",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.face_set_lasso_gesture", params.tool_maybe_tweak_event, None),
        ]},
    )


def km_3d_view_tool_sculpt_line_face_set(params):
    return (
        "3D View Tool: Sculpt, Line Face Set",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.face_set_line_gesture", params.tool_maybe_tweak_event, None),
        ]},
    )


def km_3d_view_tool_sculpt_polyline_face_set(params):
    return (
        "3D View Tool: Sculpt, Polyline Face Set",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.face_set_polyline_gesture", {"type": params.tool_mouse, "value": "PRESS"}, None)
        ]},
    )


def km_3d_view_tool_sculpt_box_trim(params):
    return (
        "3D View Tool: Sculpt, Box Trim",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.trim_box_gesture", params.tool_maybe_tweak_event, None),
        ]},
    )


def km_3d_view_tool_sculpt_lasso_trim(params):
    return (
        "3D View Tool: Sculpt, Lasso Trim",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.trim_lasso_gesture", params.tool_maybe_tweak_event, None),
        ]},
    )


def km_3d_view_tool_sculpt_line_trim(params):
    return (
        "3D View Tool: Sculpt, Line Trim",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.trim_line_gesture", params.tool_maybe_tweak_event, None),
        ]},
    )


def km_3d_view_tool_sculpt_polyline_trim(params):
    return (
        "3D View Tool: Sculpt, Polyline Trim",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.trim_polyline_gesture", {"type": params.tool_mouse, "value": "PRESS"}, None)
        ]}
    )


def km_3d_view_tool_sculpt_line_project(params):
    return (
        "3D View Tool: Sculpt, Line Project",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.project_line_gesture", params.tool_maybe_tweak_event, None),
        ]},
    )


def km_3d_view_tool_sculpt_mesh_filter(params):
    return (
        "3D View Tool: Sculpt, Mesh Filter",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.mesh_filter", params.tool_maybe_tweak_event, None)
        ]},
    )


def km_3d_view_tool_sculpt_cloth_filter(params):
    return (
        "3D View Tool: Sculpt, Cloth Filter",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.cloth_filter", params.tool_maybe_tweak_event, None)
        ]},
    )


def km_3d_view_tool_sculpt_color_filter(params):
    return (
        "3D View Tool: Sculpt, Color Filter",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.color_filter", params.tool_maybe_tweak_event, None)
        ]},
    )


def km_3d_view_tool_sculpt_mask_by_color(params):
    return (
        "3D View Tool: Sculpt, Mask by Color",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.mask_by_color", {"type": params.tool_mouse, "value": 'PRESS'}, None)
        ]},
    )


def km_3d_view_tool_sculpt_face_set_edit(params):
    return (
        "3D View Tool: Sculpt, Face Set Edit",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.face_set_edit", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (3D View, Weight Paint)

def km_3d_view_tool_paint_weight_sample_weight(params):
    return (
        "3D View Tool: Paint Weight, Sample Weight",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.weight_sample", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            ("grease_pencil.weight_sample", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


def km_3d_view_tool_paint_weight_sample_vertex_group(params):
    return (
        "3D View Tool: Paint Weight, Sample Vertex Group",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.weight_sample_group", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


def km_3d_view_tool_paint_weight_gradient(params):
    return (
        "3D View Tool: Paint Weight, Gradient",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.weight_gradient", params.tool_maybe_tweak_event, None),
        ]},
    )


def km_3d_view_tool_paint_grease_pencil_trim(params):
    return (
        "3D View Tool: Paint Grease Pencil, Trim",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("grease_pencil.stroke_trim", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (3D View, Grease Pencil, Paint)

def km_grease_pencil_primitive_tool_modal_map(_params):
    items = []
    keymap = (
        "Primitive Tool Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'Q', "value": 'PRESS', "any": True}, None),
        ("PANNING", {"type": 'MIDDLEMOUSE', "value": 'ANY', "shift": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("EXTRUDE", {"type": 'E', "value": 'PRESS'}, None),
        ("GRAB", {"type": 'G', "value": 'PRESS'}, None),
        ("ROTATE", {"type": 'R', "value": 'PRESS'}, None),
        ("SCALE", {"type": 'S', "value": 'PRESS'}, None),
        ("INCREASE_SUBDIVISION", {"type": 'UP_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("DECREASE_SUBDIVISION", {"type": 'DOWN_ARROW', "value": 'PRESS', "repeat": True}, None),
        ("CHANGE_RADIUS", {"type": 'F', "value": 'PRESS'}, None),
        ("CHANGE_OPACITY", {"type": 'F', "value": 'PRESS', "shift": True}, None),
    ])

    return keymap


def km_3d_view_tool_paint_grease_pencil_primitive_line(_params):
    return (
        "3D View Tool: Paint Grease Pencil, Line",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("grease_pencil.primitive_line", {"type": 'LEFTMOUSE', "value": 'PRESS'},
                {"properties": []}),
            ("grease_pencil.primitive_line", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
                {"properties": []}),
            ("grease_pencil.primitive_line", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
                {"properties": []}),
        ]},
    )


def km_3d_view_tool_paint_grease_pencil_primitive_polyline(_params):
    return (
        "3D View Tool: Paint Grease Pencil, Polyline",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("grease_pencil.primitive_polyline", {"type": 'LEFTMOUSE', "value": 'PRESS'},
             {"properties": []}),
            ("grease_pencil.primitive_polyline", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
             {"properties": []}),
        ]},
    )


def km_3d_view_tool_paint_grease_pencil_primitive_box(_params):
    return (
        "3D View Tool: Paint Grease Pencil, Box",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("grease_pencil.primitive_box", {"type": 'LEFTMOUSE', "value": 'PRESS'},
             {"properties": []}),
            ("grease_pencil.primitive_box", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
             {"properties": []}),
            ("grease_pencil.primitive_box", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
             {"properties": []}),
        ]},
    )


def km_3d_view_tool_paint_grease_pencil_primitive_circle(_params):
    return (
        "3D View Tool: Paint Grease Pencil, Circle",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("grease_pencil.primitive_circle", {"type": 'LEFTMOUSE', "value": 'PRESS'},
             {"properties": []}),
            ("grease_pencil.primitive_circle", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
             {"properties": []}),
            ("grease_pencil.primitive_circle", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
             {"properties": []}),
        ]},
    )


def km_3d_view_tool_paint_grease_pencil_primitive_arc(_params):
    return (
        "3D View Tool: Paint Grease Pencil, Arc",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("grease_pencil.primitive_arc", {"type": 'LEFTMOUSE', "value": 'PRESS'},
             {"properties": []}),
            ("grease_pencil.primitive_arc", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
             {"properties": []}),
            ("grease_pencil.primitive_arc", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
             {"properties": []}),
        ]},
    )


def km_3d_view_tool_paint_grease_pencil_primitive_curve(_params):
    return (
        "3D View Tool: Paint Grease Pencil, Curve",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("grease_pencil.primitive_curve", {"type": 'LEFTMOUSE', "value": 'PRESS'},
             {"properties": []}),
        ]},
    )


def km_3d_view_tool_paint_grease_pencil_eyedropper(params):
    return (
        "3D View Tool: Paint Grease Pencil, Eyedropper",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("ui.eyedropper_grease_pencil_color",
             {"type": params.tool_mouse, "value": 'PRESS'}, None),
            ("ui.eyedropper_grease_pencil_color",
             {"type": params.tool_mouse, "value": 'PRESS', "shift": True}, None),
            ("ui.eyedropper_grease_pencil_color",
             {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True}, None),
            ("ui.eyedropper_grease_pencil_color",
             {"type": params.tool_mouse, "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ]},
    )


def km_grease_pencil_interpolate_tool_modal_map(_params):
    items = []
    keymap = (
        "Interpolate Tool Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'RELEASE', "any": True}, None),
        ("INCREASE", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("DECREASE", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
    ])

    return keymap


def km_pen_tool_modal_map(_params):
    items = []
    keymap = (
        "Pen Tool Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("MOVE_HANDLE", {"type": 'LEFT_CTRL', "value": 'ANY', "any": True}, None),
        ("MOVE_ENTIRE", {"type": 'LEFT_ALT', "value": 'ANY', "any": True}, None),
        ("SNAP_ANGLE", {"type": 'LEFT_SHIFT', "value": 'ANY', "any": True}, None),
    ])

    return keymap


def km_3d_view_tool_edit_grease_pencil_pen(params):
    return (
        "3D View Tool: Edit Grease Pencil, Pen",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("grease_pencil.pen", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [
                 ("extrude_point", True),
                 ("move_segment", True),
                 ("select_point", True),
                 ("move_point", True),
                 ("extrude_handle", "VECTOR"),
             ]}),
            ("grease_pencil.pen", {"type": params.tool_mouse, "value": 'PRESS', "shift": True},
             {"properties": [
                 ("extrude_point", True),
                 ("move_segment", True),
                 ("select_point", True),
                 ("move_point", True),
                 ("extrude_handle", "AUTO"),
             ]}),
            ("grease_pencil.pen", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("insert_point", True), ("delete_point", True)]}),
            ("grease_pencil.pen", {"type": params.tool_mouse, "value": 'DOUBLE_CLICK'},
             {"properties": [("cycle_handle_type", True)]}),
        ]},
    )


# ------------------------------------------------------------------------------
# Grease Pencil: Texture Gradient Tool


def km_3d_view_tool_edit_grease_pencil_texture_gradient(params):
    return (
        "3D View Tool: Edit Grease Pencil, Gradient",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("grease_pencil.texture_gradient", params.tool_maybe_tweak_event, None),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (Sequencer, Generic)

def km_sequencer_tool_generic_select_rcs(params):
    return [
        ("sequencer.select_handle", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("sequencer.select_handle", {"type": 'LEFTMOUSE', "value": 'PRESS',
         "alt": True}, {"properties": [("ignore_connections", True)]}),
        ("anim.change_frame", {"type": params.action_mouse, "value": 'PRESS'},
         {"properties": [("seq_solo_preview", True), ("pass_through_on_strip_handles", True)]}),
        # Change frame takes precedence over the sequence slide operator. If a
        # mouse press happens on a strip handle, it is canceled, and the sequence
        # slide below activates instead.
        ("transform.seq_slide", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("view2d_edge_pan", True), ("use_restore_handle_selection", True)]}),
    ]


def km_sequencer_tool_generic_select_lcs(_params):
    return [
        ("sequencer.select", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("deselect_all", True)]}),
        ("sequencer.select", {"type": 'LEFTMOUSE', "value": 'PRESS',
         "shift": True}, {"properties": [("toggle", True)]}),
        ("anim.change_frame", {"type": 'RIGHTMOUSE', "value": 'PRESS',
         "shift": True}, {"properties": [("seq_solo_preview", True), ("pass_through_on_strip_handles", False)]}),
    ]


def km_sequencer_tool_generic_select_box(params, *, fallback):
    return (
        _fallback_id("Sequencer Tool: Select Box", fallback),
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            # Add tweak functionality to the select box tool.
            # This gives one standard tool for all selection and transform behavior.
            *(km_sequencer_tool_generic_select_rcs(params)
              if (params.select_mouse == 'RIGHTMOUSE') else
              km_sequencer_tool_generic_select_lcs(params)),
            # Don't use `tool_maybe_tweak_event`, see comment for this slot.
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "sequencer.select_box",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                    params.tool_tweak_event),
                properties=[("tweak", params.select_mouse == 'LEFTMOUSE')])),
        ]},
    )


def km_sequencer_tool_generic_select_lasso(params, *, fallback):
    return (
        _fallback_id("Sequencer Tool: Select Lasso", fallback),
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "sequencer.select_lasso",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   params.tool_tweak_event))),
        ]},
    )


def km_sequencer_tool_generic_select_circle(params, *, fallback):
    return (
        _fallback_id("Sequencer Tool: Select Circle", fallback),
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "sequencer.select_circle",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   {"type": params.tool_mouse, "value": 'PRESS'}),
                properties=[("wait_for_input", False)])),
        ]},
    )


def km_sequencer_preview_tool_generic_select(params, *, fallback):
    return (
        _fallback_id("Preview Tool: Tweak", fallback),
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("sequencer.text_cursor_set", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
            ("sequencer.text_cursor_set", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
            *([] if (fallback and (params.select_mouse == 'RIGHTMOUSE')) else _template_items_tool_select(
                params, "sequencer.select", "sequencer.cursor_set", cursor_prioritize=True, fallback=fallback)),

            *([] if params.use_fallback_tool_select_handled else
              _template_sequencer_preview_select(
                  type=params.select_mouse, value=params.select_mouse_value, legacy=params.legacy)),
        ]},
    )


def km_sequencer_preview_tool_generic_select_box(params, *, fallback):
    return (
        _fallback_id("Preview Tool: Select Box", fallback),
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("sequencer.text_cursor_set", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
            ("sequencer.text_cursor_set", {"type": 'LEFTMOUSE', "value": 'CLICK_DRAG'}, None),
            # Don't use `tool_maybe_tweak_event`, see comment for this slot.
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "sequencer.select_box",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   params.tool_tweak_event))),
        ]},
    )


def km_sequencer_preview_tool_generic_select_lasso(params, *, fallback):
    return (
        _fallback_id("Preview Tool: Select Lasso", fallback),
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "sequencer.select_lasso",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   params.tool_tweak_event))),
        ]},
    )


def km_sequencer_preview_tool_generic_select_circle(params, *, fallback):
    return (
        _fallback_id("Preview Tool: Select Circle", fallback),
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            *([] if (fallback and not params.use_fallback_tool) else _template_items_tool_select_actions_simple(
                "sequencer.select_circle",
                **(params.select_tweak_event if (fallback and params.use_fallback_tool_select_mouse) else
                   {"type": params.tool_mouse, "value": 'PRESS'}),
                properties=[("wait_for_input", False)])),
        ]},
    )


def km_sequencer_preview_tool_generic_cursor(params):
    return (
        "Preview Tool: Cursor",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("sequencer.cursor_set", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            # Don't use `tool_maybe_tweak_event` since it conflicts with `PRESS` that places the cursor.
            ("transform.translate", params.tool_tweak_event,
             {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (Sequencer, Timeline)

def km_sequencer_tool_blade(_params):
    return (
        "Sequencer Tool: Blade",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("sequencer.split", {"type": 'LEFTMOUSE', "value": 'PRESS'},
             {"properties": [
                 ("type", 'SOFT'),
                 ("side", 'NO_CHANGE'),
                 ("use_cursor_position", True),
                 ("ignore_selection", True),
             ]}),
            ("sequencer.split", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
             {"properties": [
                 ("type", 'SOFT'),
                 ("side", 'NO_CHANGE'),
                 ("use_cursor_position", True),
                 ("ignore_selection", True),
                 ("ignore_connections", True),
             ]}),
        ]},
    )


def km_sequencer_tool_slip(_params):
    return (
        "Sequencer Tool: Slip",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("sequencer.slip", {"type": 'LEFTMOUSE', "value": 'PRESS'},
             {"properties": [
                 ("slip_keyframes", True),
                 ("use_cursor_position", True),
             ]}),
            ("sequencer.slip", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
             {"properties": [
                 ("slip_keyframes", True),
                 ("use_cursor_position", True),
                 ("ignore_connections", True),
             ]}),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System (Sequencer, Preview)

def km_sequencer_preview_tool_sample(params):
    return (
        "Preview Tool: Sample",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("sequencer.sample", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


def km_sequencer_preview_tool_move(params):
    return (
        "Preview Tool: Move",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.translate", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_sequencer_preview_tool_rotate(params):
    return (
        "Preview Tool: Rotate",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.rotate", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_sequencer_preview_tool_scale(params):
    return (
        "Preview Tool: Scale",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.resize", {**params.tool_maybe_tweak_event, **params.tool_modifier},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_grease_pencil_interpolate(params):
    return (
        "3D View Tool: Edit Grease Pencil, Interpolate",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("grease_pencil.interpolate", params.tool_maybe_tweak_event,
             {"properties": [("use_selection", True)]}),
        ]},
    )


def km_3d_view_tool_paint_grease_pencil_interpolate(params):
    return (
        "3D View Tool: Paint Grease Pencil, Interpolate",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("grease_pencil.interpolate", params.tool_maybe_tweak_event,
             {"properties": [("use_selection", False)]}),
        ]},
    )


# ------------------------------------------------------------------------------
# Full Configuration

def generate_keymaps(params=None):
    if params is None:
        params = Params()
    return [
        # Window, screen, area, region.
        km_window(params),
        km_screen(params),
        km_screen_editing(params),
        km_screen_region_context_menu(params),
        km_view2d(params),
        km_view2d_buttons_list(params),
        km_user_interface(params),
        km_property_editor(params),

        # Editors.
        km_outliner(params),
        km_uv_editor(params),
        km_view3d_generic(params),
        km_view3d(params),
        km_mask_editing(params),
        km_markers(params),
        km_time_scrub(params),
        km_time_scrub_clip(params),
        km_graph_editor_generic(params),
        km_graph_editor(params),
        km_image_generic(params),
        km_image(params),
        km_node_generic(params),
        km_node_editor(params),
        km_spreadsheet_generic(params),
        km_info(params),
        km_file_browser(params),
        km_file_browser_main(params),
        km_file_browser_buttons(params),
        km_dopesheet_generic(params),
        km_dopesheet(params),
        km_nla_generic(params),
        km_nla_tracks(params),
        km_nla_editor(params),
        km_text_generic(params),
        km_text(params),
        km_sequencer_generic(params),
        km_sequencer(params),
        km_sequencer_preview(params),
        km_sequencer_channels(params),
        km_console(params),
        km_clip(params),
        km_clip_editor(params),
        km_clip_graph_editor(params),
        km_clip_dopesheet_editor(params),

        # Animation.
        km_frames(params),
        km_animation(params),
        km_animation_channels(params),

        # Modes.
        # Annotations
        km_annotate(params),
        # Grease Pencil
        km_grease_pencil_selection(params),
        km_grease_pencil_paint_mode(params),
        km_grease_pencil_edit_mode(params),
        km_grease_pencil_sculpt_mode(params),
        km_grease_pencil_weight_paint(params),
        km_grease_pencil_vertex_paint(params),
        km_grease_pencil_brush_stroke(params),
        km_grease_pencil_fill_tool(params),
        # Object mode.
        km_object_mode(params),
        km_object_non_modal(params),
        km_pose(params),
        # Object paint modes.
        km_paint_curve(params),
        km_image_paint(params),
        km_vertex_paint(params),
        km_weight_paint(params),
        km_paint_face_mask(params),
        km_paint_vertex_mask(params),
        # Object sculpt modes.
        km_sculpt(params),
        km_sculpt_curves(params),
        # Object edit modes.
        km_edit_mesh(params),
        km_edit_armature(params),
        km_edit_metaball(params),
        km_edit_lattice(params),
        km_edit_particle(params),
        km_edit_font(params),
        km_edit_curve_legacy(params),
        km_edit_curves(params),
        km_edit_pointcloud(params),

        # Modal maps.
        km_eyedropper_modal_map(params),
        km_eyedropper_colorramp_pointsampling_map(params),
        km_transform_modal_map(params),
        km_view3d_interactive_add_tool_modal(params),
        km_view3d_gesture_circle(params),
        km_gesture_border(params),
        km_gesture_zoom_border(params),
        km_gesture_straight_line(params),
        km_gesture_lasso(params),
        km_gesture_polyline(params),
        km_standard_modal_map(params),
        km_knife_tool_modal_map(params),
        km_custom_normals_modal_map(params),
        km_bevel_modal_map(params),
        km_view3d_fly_modal(params),
        km_view3d_walk_modal(params),
        km_view3d_rotate_modal(params),
        km_view3d_move_modal(params),
        km_view3d_zoom_modal(params),
        km_view3d_dolly_modal(params),
        km_paint_stroke_modal(params),
        km_sculpt_expand_modal(params),
        km_sculpt_mesh_filter_modal_map(params),
        km_curve_pen_modal_map(params),
        km_pen_tool_modal_map(params),
        km_node_link_modal_map(params),
        km_node_resize_modal_map(params),
        km_grease_pencil_primitive_tool_modal_map(params),
        km_grease_pencil_fill_tool_modal_map(params),
        km_grease_pencil_interpolate_tool_modal_map(params),
        km_sequencer_slip_modal_map(params),

        # Gizmos.
        km_generic_gizmo(params),
        km_generic_gizmo_drag(params),
        km_generic_gizmo_maybe_drag(params),
        km_generic_gizmo_click_drag(params),
        km_generic_gizmo_select(params),
        km_generic_gizmo_tweak_modal_map(params),

        # Pop-Up Key-maps.
        km_popup_toolbar(params),

        # Tool System.
        km_generic_tool_annotate(params),
        km_generic_tool_annotate_line(params),
        km_generic_tool_annotate_polygon(params),
        km_generic_tool_annotate_eraser(params),

        km_image_editor_tool_generic_sample(params),
        km_image_editor_tool_uv_cursor(params),
        *(km_image_editor_tool_uv_select(params, fallback=fallback) for fallback in (False, True)),
        *(km_image_editor_tool_uv_select_box(params, fallback=fallback) for fallback in (False, True)),
        *(km_image_editor_tool_uv_select_circle(params, fallback=fallback) for fallback in (False, True)),
        *(km_image_editor_tool_uv_select_lasso(params, fallback=fallback) for fallback in (False, True)),
        km_image_editor_tool_uv_rip_region(params),
        km_image_editor_tool_uv_grab(params),
        km_image_editor_tool_uv_relax(params),
        km_image_editor_tool_uv_pinch(params),
        km_image_editor_tool_uv_move(params),
        km_image_editor_tool_uv_rotate(params),
        km_image_editor_tool_uv_scale(params),
        km_image_editor_tool_mask_cursor(params),
        *(km_image_editor_tool_mask_select(params, fallback=fallback) for fallback in (False, True)),
        *(km_image_editor_tool_mask_select_box(params, fallback=fallback) for fallback in (False, True)),
        *(km_image_editor_tool_mask_select_circle(params, fallback=fallback) for fallback in (False, True)),
        *(km_image_editor_tool_mask_select_lasso(params, fallback=fallback) for fallback in (False, True)),
        km_image_editor_tool_mask_move(params),
        km_image_editor_tool_mask_rotate(params),
        km_image_editor_tool_mask_scale(params),
        km_image_editor_tool_mask_transform(params),
        km_image_editor_tool_mask_primitive_circle(params),
        km_image_editor_tool_mask_primitive_square(params),
        *(km_node_editor_tool_select(params, fallback=fallback) for fallback in (False, True)),
        *(km_node_editor_tool_select_box(params, fallback=fallback) for fallback in (False, True)),
        *(km_node_editor_tool_select_lasso(params, fallback=fallback) for fallback in (False, True)),
        *(km_node_editor_tool_select_circle(params, fallback=fallback) for fallback in (False, True)),
        km_node_editor_tool_links_cut(params),
        km_node_editor_tool_links_mute(params),
        km_node_editor_tool_add_reroute(params),
        km_3d_view_tool_cursor(params),
        km_3d_view_tool_text_select(params),
        *(km_3d_view_tool_select(params, fallback=fallback) for fallback in (False, True)),
        *(km_3d_view_tool_select_box(params, fallback=fallback) for fallback in (False, True)),
        *(km_3d_view_tool_select_circle(params, fallback=fallback) for fallback in (False, True)),
        *(km_3d_view_tool_select_lasso(params, fallback=fallback) for fallback in (False, True)),
        km_3d_view_tool_transform(params),
        km_3d_view_tool_move(params),
        km_3d_view_tool_rotate(params),
        km_3d_view_tool_scale(params),
        km_3d_view_tool_shear(params),
        km_3d_view_tool_bend(params),
        km_3d_view_tool_measure(params),
        km_3d_view_tool_interactive_add(params),
        km_3d_view_tool_pose_breakdowner(params),
        km_3d_view_tool_pose_push(params),
        km_3d_view_tool_pose_relax(params),
        km_3d_view_tool_edit_armature_roll(params),
        km_3d_view_tool_edit_armature_bone_size(params),
        km_3d_view_tool_edit_armature_bone_envelope(params),
        km_3d_view_tool_edit_armature_extrude(params),
        km_3d_view_tool_edit_armature_extrude_to_cursor(params),
        km_3d_view_tool_edit_mesh_extrude_region(params),
        km_3d_view_tool_edit_mesh_extrude_manifold(params),
        km_3d_view_tool_edit_mesh_extrude_along_normals(params),
        km_3d_view_tool_edit_mesh_extrude_individual(params),
        km_3d_view_tool_edit_mesh_extrude_to_cursor(params),
        km_3d_view_tool_edit_mesh_inset_faces(params),
        km_3d_view_tool_edit_mesh_bevel(params),
        km_3d_view_tool_edit_mesh_loop_cut(params),
        km_3d_view_tool_edit_mesh_offset_edge_loop_cut(params),
        km_3d_view_tool_edit_mesh_knife(params),
        km_3d_view_tool_edit_mesh_bisect(params),
        km_3d_view_tool_edit_mesh_poly_build(params),
        km_3d_view_tool_edit_mesh_spin(params),
        km_3d_view_tool_edit_mesh_spin_duplicate(params),
        km_3d_view_tool_edit_mesh_smooth(params),
        km_3d_view_tool_edit_mesh_randomize(params),
        km_3d_view_tool_edit_mesh_edge_slide(params),
        km_3d_view_tool_edit_mesh_vertex_slide(params),
        km_3d_view_tool_edit_mesh_shrink_fatten(params),
        km_3d_view_tool_edit_mesh_push_pull(params),
        km_3d_view_tool_edit_mesh_to_sphere(params),
        km_3d_view_tool_edit_mesh_rip_region(params),
        km_3d_view_tool_edit_mesh_rip_edge(params),
        km_3d_view_tool_edit_curve_draw(params),
        km_3d_view_tool_edit_curve_pen(params),
        km_3d_view_tool_edit_curve_radius(params),
        km_3d_view_tool_edit_curve_tilt(params),
        km_3d_view_tool_edit_curve_randomize(params),
        km_3d_view_tool_edit_curve_extrude(params),
        km_3d_view_tool_edit_curve_extrude_to_cursor(params),
        km_3d_view_tool_edit_curves_draw(params),
        km_3d_view_tool_edit_curves_pen(params),
        km_3d_view_tool_sculpt_box_mask(params),
        km_3d_view_tool_sculpt_lasso_mask(params),
        km_3d_view_tool_sculpt_line_mask(params),
        km_3d_view_tool_sculpt_polyline_mask(params),
        km_3d_view_tool_sculpt_box_hide(params),
        km_3d_view_tool_sculpt_lasso_hide(params),
        km_3d_view_tool_sculpt_line_hide(params),
        km_3d_view_tool_sculpt_polyline_hide(params),
        km_3d_view_tool_sculpt_box_face_set(params),
        km_3d_view_tool_sculpt_lasso_face_set(params),
        km_3d_view_tool_sculpt_line_face_set(params),
        km_3d_view_tool_sculpt_polyline_face_set(params),
        km_3d_view_tool_sculpt_box_trim(params),
        km_3d_view_tool_sculpt_lasso_trim(params),
        km_3d_view_tool_sculpt_line_trim(params),
        km_3d_view_tool_sculpt_polyline_trim(params),
        km_3d_view_tool_sculpt_line_project(params),
        km_3d_view_tool_sculpt_mesh_filter(params),
        km_3d_view_tool_sculpt_cloth_filter(params),
        km_3d_view_tool_sculpt_color_filter(params),
        km_3d_view_tool_sculpt_mask_by_color(params),
        km_3d_view_tool_sculpt_face_set_edit(params),
        km_3d_view_tool_paint_weight_sample_weight(params),
        km_3d_view_tool_paint_weight_sample_vertex_group(params),
        km_3d_view_tool_paint_weight_gradient(params),
        km_3d_view_tool_paint_grease_pencil_primitive_line(params),
        km_3d_view_tool_paint_grease_pencil_primitive_polyline(params),
        km_3d_view_tool_paint_grease_pencil_primitive_box(params),
        km_3d_view_tool_paint_grease_pencil_primitive_circle(params),
        km_3d_view_tool_paint_grease_pencil_primitive_arc(params),
        km_3d_view_tool_paint_grease_pencil_primitive_curve(params),
        *(km_sequencer_tool_generic_select_box(params, fallback=fallback)
          for fallback in (False, True)),
        *(km_sequencer_preview_tool_generic_select(params, fallback=fallback)
          for fallback in (False, True)),
        *(km_sequencer_preview_tool_generic_select_box(params, fallback=fallback)
          for fallback in (False, True)),
        *(km_sequencer_tool_generic_select_lasso(params, fallback=fallback)
          for fallback in (False, True)),
        *(km_sequencer_preview_tool_generic_select_lasso(params, fallback=fallback)
          for fallback in (False, True)),
        *(km_sequencer_preview_tool_generic_select_circle(params, fallback=fallback)
          for fallback in (False, True)),
        *(km_sequencer_tool_generic_select_circle(params, fallback=fallback)
          for fallback in (False, True)),
        km_3d_view_tool_paint_grease_pencil_trim(params),
        km_3d_view_tool_edit_grease_pencil_texture_gradient(params),
        km_sequencer_tool_blade(params),
        km_sequencer_tool_slip(params),
        km_sequencer_preview_tool_generic_cursor(params),
        km_sequencer_preview_tool_sample(params),
        km_sequencer_preview_tool_move(params),
        km_sequencer_preview_tool_rotate(params),
        km_sequencer_preview_tool_scale(params),
        km_3d_view_tool_edit_grease_pencil_pen(params),
        km_3d_view_tool_edit_grease_pencil_interpolate(params),
        km_3d_view_tool_paint_grease_pencil_interpolate(params),
        km_3d_view_tool_paint_grease_pencil_eyedropper(params),
    ]


# ------------------------------------------------------------------------------
# Refactoring (Testing Only)
#
# Allows running outside of Blender to generate data for diffing
#
# To compare:
#
#    python3 scripts/presets/keyconfig/keymap_data/blender_default.py && \
#      diff -u keymap_default.py.orig keymap_default.py && \
#      diff -u keymap_legacy.py.orig  keymap_legacy.py
#
# # begin code:
# import pprint
# for legacy in (False, True):
#     with open("keymap_default.py" if not legacy else "keymap_legacy.py", 'w') as fh:
#         fh.write(pprint.pformat(generate_keymaps(Params(legacy=legacy)), indent=2, width=80))
# import sys
# sys.exit()
# # end code


# ------------------------------------------------------------------------------
# PyLint (Testing Only)
#
# Command to lint:
#
#    pylint \
#        scripts/presets/keyconfig/keymap_data/blender_default.py \
#        --disable=C0111,C0209,C0301,C0302,C0413,C0415,R1705,R0902,R0903,R0913,R0914,R0915,W0511,W0622
