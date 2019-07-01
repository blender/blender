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


# ------------------------------------------------------------------------------
# Configurable Parameters

class Params:
    __slots__ = (
        "apple",
        "legacy",
        "select_mouse",
        "select_mouse_value",
        "select_tweak",
        "action_mouse",
        "action_tweak",
        "tool_mouse",
        "tool_tweak",
        "context_menu_event",
        "cursor_set_event",
        "cursor_tweak_event",
        "use_mouse_emulate_3_button",
        # Experimental option.
        "pie_value",

        # User preferences.
        #
        # Swap 'Space/Shift-Space'.
        "spacebar_action",
        # Key toggles selection with 'A'.
        "use_select_all_toggle",
        # Activate gizmo on drag (which support it).
        "use_gizmo_drag",
        # Use pie menu for tab by default (swap 'Tab/Ctrl-Tab').
        "use_v3d_tab_menu",
        # Use extended pie menu for shading.
        "use_v3d_shade_ex_pie",
        # Experimental option.
        "use_pie_click_drag",
        "v3d_tilde_action",
    )

    def __init__(
            self,
            *,
            legacy=False,
            select_mouse='RIGHT',
            use_mouse_emulate_3_button=False,

            # User preferences.
            spacebar_action='TOOL',
            use_select_all_toggle=False,
            use_gizmo_drag=True,
            use_v3d_tab_menu=False,
            use_v3d_shade_ex_pie=False,
            use_pie_click_drag=False,
            v3d_tilde_action='VIEW',
    ):
        from sys import platform
        self.apple = (platform == 'darwin')
        self.legacy = legacy

        if select_mouse == 'RIGHT':
            # Right mouse select.
            self.select_mouse = 'RIGHTMOUSE'
            self.select_mouse_value = 'PRESS'
            self.select_tweak = 'EVT_TWEAK_R'
            self.action_mouse = 'LEFTMOUSE'
            self.action_tweak = 'EVT_TWEAK_L'
            self.tool_mouse = 'LEFTMOUSE'
            self.tool_tweak = 'EVT_TWEAK_L'
            self.context_menu_event = {"type": 'W', "value": 'PRESS'}
            self.cursor_set_event = {"type": 'LEFTMOUSE', "value": 'CLICK'}
            self.cursor_tweak_event = None
        else:
            # Left mouse select uses Click event for selection. This is a little
            # less immediate, but is needed to distinguish between click and tweak
            # events on the same mouse buttons.
            self.select_mouse = 'LEFTMOUSE'
            self.select_mouse_value = 'CLICK'
            self.select_tweak = 'EVT_TWEAK_L'
            self.action_mouse = 'RIGHTMOUSE'
            self.action_tweak = 'EVT_TWEAK_R'
            self.tool_mouse = 'LEFTMOUSE'
            self.tool_tweak = 'EVT_TWEAK_L'

            if self.legacy:
                self.context_menu_event = {"type": 'W', "value": 'PRESS'}
            else:
                self.context_menu_event = {"type": 'RIGHTMOUSE', "value": 'PRESS'}

            self.cursor_set_event = {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True}
            self.cursor_tweak_event = {"type": 'EVT_TWEAK_R', "value": 'ANY', "shift": True}

        self.use_mouse_emulate_3_button = use_mouse_emulate_3_button

        # User preferences
        self.spacebar_action = spacebar_action

        self.use_gizmo_drag = use_gizmo_drag
        self.use_select_all_toggle = use_select_all_toggle
        self.use_v3d_tab_menu = use_v3d_tab_menu
        self.use_v3d_shade_ex_pie = use_v3d_shade_ex_pie
        self.v3d_tilde_action = v3d_tilde_action

        self.use_pie_click_drag = use_pie_click_drag
        if not use_pie_click_drag:
            self.pie_value = 'PRESS'
        else:
            self.pie_value = 'CLICK_DRAG'


# ------------------------------------------------------------------------------
# Constants


# Physical layout.
NUMBERS_1 = ('ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'SIX', 'SEVEN', 'EIGHT', 'NINE', 'ZERO')
# Numeric order.
NUMBERS_0 = ('ZERO', 'ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'SIX', 'SEVEN', 'EIGHT', 'NINE')


# ------------------------------------------------------------------------------
# Keymap Item Wrappers

def op_menu(menu, kmi_args):
    return ("wm.call_menu", kmi_args, {"properties": [("name", menu)]})


def op_menu_pie(menu, kmi_args):
    return ("wm.call_menu_pie", kmi_args, {"properties": [("name", menu)]})


def op_panel(menu, kmi_args, kmi_data=()):
    return ("wm.call_panel", kmi_args, {"properties": [("name", menu), *kmi_data]})


def op_tool(tool, kmi_args):
    return ("wm.tool_set_by_id", kmi_args, {"properties": [("name", tool)]})


def op_tool_cycle(tool, kmi_args):
    return ("wm.tool_set_by_id", kmi_args, {"properties": [("name", tool), ("cycle", True)]})


# ------------------------------------------------------------------------------
# Keymap Templates

def _template_space_region_type_toggle(*, toolbar_key=None, sidebar_key=None):
    items = []
    if toolbar_key is not None:
        items.append(
            ("wm.context_toggle", toolbar_key,
             {"properties": [("data_path", 'space_data.show_region_toolbar')]})
        )
    if sidebar_key is not None:
        items.append(
            ("wm.context_toggle", sidebar_key,
             {"properties": [("data_path", 'space_data.show_region_ui')]}),
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


def _template_items_object_subdivision_set():
    return [
        ("object.subdivision_set",
         {"type": NUMBERS_0[i], "value": 'PRESS', "ctrl": True},
         {"properties": [("level", i), ("relative", False)]})
        for i in range(6)
    ]


def _template_items_gizmo_tweak_value():
    return [
        ("gizmogroup.gizmo_tweak", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
    ]


def _template_items_gizmo_tweak_value_click_drag():
    return [
        ("gizmogroup.gizmo_tweak", {"type": 'LEFTMOUSE', "value": 'CLICK', "any": True}, None),
        ("gizmogroup.gizmo_tweak", {"type": 'EVT_TWEAK_L', "value": 'ANY', "any": True}, None),
    ]


def _template_items_gizmo_tweak_value_drag():
    return [
        ("gizmogroup.gizmo_tweak", {"type": 'EVT_TWEAK_L', "value": 'ANY', "any": True}, None),
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
                {"type": k, "value": 'PRESS', **key_expand, **key_extend},
                {"properties": [*prop_extend, *prop_expand, ("type", e)]}
            )
            for key_expand, prop_expand in (({}, ()), ({"ctrl": True}, (("use_expand", True),)))
            for key_extend, prop_extend in (({}, ()), ({"shift": True}, (("use_extend", True),)))
            for k, e in (('ONE', 'VERT'), ('TWO', 'EDGE'), ('THREE', 'FACE'))
        ]

def _template_items_uv_select_mode(params):
    if params.legacy:
        return [
            op_menu("IMAGE_MT_uvs_select_mode", {"type": 'TAB', "value": 'PRESS', "ctrl": True}),
        ]
    else:
        return [
            *_template_items_editmode_mesh_select_mode(params),
            ("mesh.select_mode", {"type": 'FOUR', "value": 'PRESS'}, None),
            ("wm.context_set_enum", {"type": 'ONE', "value": 'PRESS'},
             {"properties": [("data_path", 'tool_settings.uv_select_mode'), ("value", 'VERTEX')]}),
            ("wm.context_set_enum", {"type": 'TWO', "value": 'PRESS'},
             {"properties": [("data_path", 'tool_settings.uv_select_mode'), ("value", 'EDGE')]}),
            ("wm.context_set_enum", {"type": 'THREE', "value": 'PRESS'},
             {"properties": [("data_path", 'tool_settings.uv_select_mode'), ("value", 'FACE')]}),
            ("wm.context_set_enum", {"type": 'FOUR', "value": 'PRESS'},
             {"properties": [("data_path", 'tool_settings.uv_select_mode'), ("value", 'ISLAND')]}),
        ]

def _template_items_proportional_editing(*, connected=False):
    return [
        op_menu_pie("VIEW3D_MT_proportional_editing_falloff_pie", {"type": 'O', "value": 'PRESS', "shift": True}),
        ("wm.context_toggle", {"type": 'O', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit')]}),
        *(() if not connected else (
            ("wm.context_toggle", {"type": 'O', "value": 'PRESS', "alt": True},
             {"properties": [("data_path", 'tool_settings.use_proportional_connected')]}),
        ))
    ]


# Tool System Templates

def _template_items_tool_select(params, operator, cursor_operator):
    if params.select_mouse == 'LEFTMOUSE':
        # Immediate select without quick delay.
        return [(operator, {"type": 'LEFTMOUSE', "value": 'PRESS'}, None)]
    else:
        # For right mouse, set the cursor.
        return [
            (cursor_operator, {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
            ("transform.translate", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
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
def _template_items_tool_select_actions_simple(operator, *, type, value, properties=[]):
    kmi_args = {"type": type, "value": value}
    return [
        # Don't define 'SET' here, take from the tool options.
        (operator, kmi_args,
         {"properties": properties}),
        (operator, {**kmi_args, "shift": True},
         {"properties": [*properties, ("mode", 'ADD')]}),
        (operator, {**kmi_args, "ctrl": True},
         {"properties": [*properties, ("mode", 'SUB')]}),
    ]


def _template_items_legacy_tools_from_numbers():
    return [
        ("wm.tool_set_by_index",
         {"type": NUMBERS_1[i % 10], "value": 'PRESS', "shift": i >= 10},
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
        # Old shorctus
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
        op_menu("USERPREF_MT_ndof_settings", {"type": 'NDOF_BUTTON_MENU', "value": 'PRESS'}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_PLUS', "value": 'PRESS'},
         {"properties": [("data_path", 'preferences.inputs.ndof_sensitivity'), ("value", 1.1)]}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_MINUS', "value": 'PRESS'},
         {"properties": [("data_path", 'preferences.inputs.ndof_sensitivity'), ("value", 1.0 / 1.1)]}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_PLUS', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'preferences.inputs.ndof_sensitivity'), ("value", 1.5)]}),
        ("wm.context_scale_float", {"type": 'NDOF_BUTTON_MINUS', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'preferences.inputs.ndof_sensitivity'), ("value", 2.0 / 3.0)]}),
        ("info.reports_display_update", {"type": 'TIMER_REPORT', "value": 'ANY', "any": True}, None),
    ])

    if not params.legacy:
        # New shortcuts
        items.extend([
            ("wm.doc_view_manual_ui_context", {"type": 'F1', "value": 'PRESS'}, None),
            op_panel("TOPBAR_PT_name", {"type": 'F2', "value": 'PRESS'}, [("keep_open", False)]),
            ("wm.search_menu", {"type": 'F3', "value": 'PRESS'}, None),
            op_menu("TOPBAR_MT_file_context_menu", {"type": 'F4', "value": 'PRESS'}),
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
            assert False

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
        ("screen.repeat_last", {"type": 'R', "value": 'PRESS', "shift": True}, None),
        # Files
        ("file.execute", {"type": 'RET', "value": 'PRESS'}, None),
        ("file.execute", {"type": 'NUMPAD_ENTER', "value": 'PRESS'}, None),
        ("file.cancel", {"type": 'ESC', "value": 'PRESS'}, None),
        # Undo
        ("ed.undo", {"type": 'Z', "value": 'PRESS', "ctrl": True}, None),
        ("ed.redo", {"type": 'Z', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        # Render
        ("render.render", {"type": 'F12', "value": 'PRESS'},
         {"properties": [("use_viewport", True)]}),
        ("render.render", {"type": 'F12', "value": 'PRESS', "ctrl": True},
         {"properties": [("animation", True), ("use_viewport", True)]}),
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
            ("screen.screen_set", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True},
             {"properties": [("delta", 1)]}),
            ("screen.screen_set", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True},
             {"properties": [("delta", -1)]}),
            ("screen.screenshot", {"type": 'F3', "value": 'PRESS', "ctrl": True}, None),
            ("screen.repeat_history", {"type": 'R', "value": 'PRESS', "ctrl": True, "alt": True}, None),
            ("screen.region_flip", {"type": 'F5', "value": 'PRESS'}, None),
            ("screen.redo_last", {"type": 'F6', "value": 'PRESS'}, None),
            ("script.reload", {"type": 'F8', "value": 'PRESS'}, None),
            ("screen.userpref_show", {"type": 'U', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ])

    if params.apple:
        # Apple undo and user prefs
        items.extend([
            ("screen.userpref_show", {"type": 'COMMA', "value": 'PRESS', "oskey": True}, None),
        ])

    return keymap


def km_screen_editing(params):
    items = []
    keymap = ("Screen Editing",
              {"space_type": 'EMPTY', "region_type": 'WINDOW'},
              {"items": items})

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
        ("screen.area_options", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
    ])

    if params.legacy:
        items.extend([
            ("wm.context_toggle", {"type": 'F9', "value": 'PRESS', "alt": True},
             {"properties": [("data_path", 'space_data.show_region_header')]})
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
        # Scrollbars
        ("view2d.scroller_activate", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroller_activate", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        # Pan/scroll
        ("view2d.pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("view2d.pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "shift": True}, None),
        ("view2d.pan", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("view2d.scroll_right", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("view2d.scroll_left", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("view2d.scroll_down", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True}, None),
        ("view2d.scroll_up", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True}, None),
        ("view2d.ndof", {"type": 'NDOF_MOTION', "value": 'ANY'}, None),
        # Zoom with single step
        ("view2d.zoom_out", {"type": 'WHEELOUTMOUSE', "value": 'PRESS'}, None),
        ("view2d.zoom_in", {"type": 'WHEELINMOUSE', "value": 'PRESS'}, None),
        ("view2d.zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS'}, None),
        ("view2d.zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS'}, None),
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
        # Scrollbars
        ("view2d.scroller_activate", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroller_activate", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        # Pan scroll
        ("view2d.pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("view2d.pan", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("view2d.scroll_down", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_up", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_down", {"type": 'PAGE_DOWN', "value": 'PRESS'},
         {"properties": [("page", True)]}),
        ("view2d.scroll_up", {"type": 'PAGE_UP', "value": 'PRESS'},
         {"properties": [("page", True)]}),
        # Zoom
        ("view2d.zoom", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("view2d.zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("view2d.zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("view2d.zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS'}, None),
        ("view2d.zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS'}, None),
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
        # Copy data path
        ("ui.copy_data_path_button", {"type": 'C', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("ui.copy_data_path_button", {"type": 'C', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("full_path", True)]}),
        # Keyframes and drivers
        ("anim.keyframe_insert_button", {"type": 'I', "value": 'PRESS'}, None),
        ("anim.keyframe_delete_button", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("anim.keyframe_clear_button", {"type": 'I', "value": 'PRESS', "shift": True, "alt": True}, None),
        ("anim.driver_button_add", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("anim.driver_button_remove", {"type": 'D', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("anim.keyingset_button_add", {"type": 'K', "value": 'PRESS'}, None),
        ("anim.keyingset_button_remove", {"type": 'K', "value": 'PRESS', "alt": True}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editors


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
         {"properties": [("direction", 'PREV'), ], },),
        ("screen.space_context_cycle", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("direction", 'NEXT'), ], },),
    ])

    return keymap


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
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK'},
         {"properties": [("extend", False), ("recursive", False), ("deselect_all", not params.legacy)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("extend", True), ("recursive", False)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True},
         {"properties": [("extend", False), ("recursive", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True, "ctrl": True},
         {"properties": [("extend", True), ("recursive", True)]}),
        ("outliner.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("outliner.item_openclose", {"type": 'RET', "value": 'PRESS'},
         {"properties": [("all", False)]}),
        ("outliner.item_openclose", {"type": 'RET', "value": 'PRESS', "shift": True},
         {"properties": [("all", True)]}),
        ("outliner.item_rename", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("outliner.operation", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        ("outliner.item_drag_drop", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("outliner.item_drag_drop", {"type": 'EVT_TWEAK_L', "value": 'ANY', "shift": True}, None),
        ("outliner.show_hierarchy", {"type": 'HOME', "value": 'PRESS'}, None),
        ("outliner.show_active", {"type": 'PERIOD', "value": 'PRESS'}, None),
        ("outliner.show_active", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("outliner.scroll_page", {"type": 'PAGE_DOWN', "value": 'PRESS'},
         {"properties": [("up", False)]}),
        ("outliner.scroll_page", {"type": 'PAGE_UP', "value": 'PRESS'},
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
        ("outliner.collection_delete", {"type": 'X', "value": 'PRESS'}, None),
        ("outliner.collection_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("object.move_to_collection", {"type": 'M', "value": 'PRESS'}, None),
        ("object.link_to_collection", {"type": 'M', "value": 'PRESS', "shift": True}, None),
        ("outliner.collection_exclude_set", {"type": 'E', "value": 'PRESS'}, None),
        ("outliner.collection_exclude_clear", {"type": 'E', "value": 'PRESS', "alt": True}, None),
        ("outliner.hide", {"type": 'H', "value": 'PRESS'}, None),
        ("outliner.unhide_all", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        # Copy/paste.
        ("outliner.id_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("outliner.id_paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


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
        ("uv.mark_seam", {"type": 'E', "value": 'PRESS', "ctrl": True}, None),
        ("uv.select", {"type": params.select_mouse, "value": params.select_mouse_value},
         {"properties": [("extend", False), ("deselect_all", not params.legacy)]}),
        ("uv.select", {"type": params.select_mouse, "value": params.select_mouse_value, "shift": True},
         {"properties": [("extend", True)]}),
        ("uv.select_loop", {"type": params.select_mouse, "value": params.select_mouse_value, "alt": True},
         {"properties": [("extend", False)]}),
        ("uv.select_loop", {"type": params.select_mouse, "value": params.select_mouse_value, "shift": True, "alt": True},
         {"properties": [("extend", True)]}),
        ("uv.select_split", {"type": 'Y', "value": 'PRESS'}, None),
        ("uv.select_box", {"type": 'B', "value": 'PRESS'},
         {"properties": [("pinned", False)]}),
        ("uv.select_box", {"type": 'B', "value": 'PRESS', "ctrl": True},
         {"properties": [("pinned", True)]}),
        ("uv.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        ("uv.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True},
         {"properties": [("mode", 'ADD')]}),
        ("uv.select_lasso", {"type": params.action_tweak, "value": 'ANY', "shift": True, "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        ("uv.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("uv.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("extend", True), ("deselect", False)]}),
        ("uv.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("extend", False), ("deselect", True)]}),
        ("uv.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("uv.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
        *_template_items_select_actions(params, "uv.select_all"),
        ("uv.select_pinned", {"type": 'P', "value": 'PRESS', "shift": True}, None),
        op_menu("IMAGE_MT_uvs_weldalign", {"type": 'W', "value": 'PRESS', "shift": True}),
        ("uv.stitch", {"type": 'V', "value": 'PRESS'}, None),
        ("uv.pin", {"type": 'P', "value": 'PRESS'},
         {"properties": [("clear", False)]}),
        ("uv.pin", {"type": 'P', "value": 'PRESS', "alt": True},
         {"properties": [("clear", True)]}),
        ("uv.unwrap", {"type": 'U', "value": 'PRESS'}, None),
        ("uv.hide", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("uv.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("uv.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        op_menu_pie("IMAGE_MT_uvs_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True}),
        op_menu("IMAGE_MT_uvs_select_mode", {"type": 'TAB', "value": 'PRESS', "ctrl": True}),
        *_template_items_proportional_editing(connected=False),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_tweak, "value": 'ANY'}, None),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        ("transform.shear", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ("transform.mirror", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
        ("wm.context_toggle", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'tool_settings.use_snap')]}),
        ("wm.context_menu_enum", {"type": 'TAB', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("data_path", 'tool_settings.snap_uv_element')]}),
        op_menu("IMAGE_MT_uvs_context_menu", params.context_menu_event),
    ])

    # Fallback for MMB emulation
    if params.use_mouse_emulate_3_button and params.select_mouse == 'LEFTMOUSE':
        items.extend([
            ("uv.select_loop", {"type": params.select_mouse, "value": 'DOUBLE_CLICK'},
             {"properties": [("extend", False)]}),
            ("uv.select_loop", {"type": params.select_mouse, "value": 'DOUBLE_CLICK', "alt": True},
             {"properties": [("extend", True)]}),
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


# 3D View: all regions.
def km_view3d_generic(_params):
    items = []
    keymap = (
        "3D View Generic",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
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
        ("view3d.localview_remove_from", {"type": 'M', "value": 'PRESS'}, None),
        # Navigation.
        ("view3d.rotate", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("view3d.move", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "shift": True}, None),
        ("view3d.zoom", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("view3d.dolly", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("view3d.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS', "ctrl": True},
         {"properties": [("use_all_regions", True)]}),
        ("view3d.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'},
         {"properties": [("use_all_regions", False)]}),
        ("view3d.smoothview", {"type": 'TIMER1', "value": 'ANY', "any": True}, None),
        ("view3d.rotate", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("view3d.rotate", {"type": 'MOUSEROTATE', "value": 'ANY'}, None),
        ("view3d.move", {"type": 'TRACKPADPAN', "value": 'ANY', "shift": True}, None),
        ("view3d.zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("view3d.zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("view3d.zoom", {"type": 'NUMPAD_PLUS', "value": 'PRESS'},
         {"properties": [("delta", 1)]}),
        ("view3d.zoom", {"type": 'NUMPAD_MINUS', "value": 'PRESS'},
         {"properties": [("delta", -1)]}),
        ("view3d.zoom", {"type": 'EQUAL', "value": 'PRESS', "ctrl": True},
         {"properties": [("delta", 1)]}),
        ("view3d.zoom", {"type": 'MINUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("delta", -1)]}),
        ("view3d.zoom", {"type": 'WHEELINMOUSE', "value": 'PRESS'},
         {"properties": [("delta", 1)]}),
        ("view3d.zoom", {"type": 'WHEELOUTMOUSE', "value": 'PRESS'},
         {"properties": [("delta", -1)]}),
        ("view3d.dolly", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True},
         {"properties": [("delta", 1)]}),
        ("view3d.dolly", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True},
         {"properties": [("delta", -1)]}),
        ("view3d.dolly", {"type": 'EQUAL', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("delta", 1)]}),
        ("view3d.dolly", {"type": 'MINUS', "value": 'PRESS', "shift": True, "ctrl": True},
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
        ("view3d.view_orbit", {"type": 'NUMPAD_2', "value": 'PRESS'},
         {"properties": [("type", 'ORBITDOWN')]}),
        ("view3d.view_axis", {"type": 'NUMPAD_3', "value": 'PRESS'},
         {"properties": [("type", 'RIGHT')]}),
        ("view3d.view_orbit", {"type": 'NUMPAD_4', "value": 'PRESS'},
         {"properties": [("type", 'ORBITLEFT')]}),
        ("view3d.view_persportho", {"type": 'NUMPAD_5', "value": 'PRESS'}, None),
        ("view3d.view_orbit", {"type": 'NUMPAD_6', "value": 'PRESS'},
         {"properties": [("type", 'ORBITRIGHT')]}),
        ("view3d.view_axis", {"type": 'NUMPAD_7', "value": 'PRESS'},
         {"properties": [("type", 'TOP')]}),
        ("view3d.view_orbit", {"type": 'NUMPAD_8', "value": 'PRESS'},
         {"properties": [("type", 'ORBITUP')]}),
        ("view3d.view_axis", {"type": 'NUMPAD_1', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'BACK')]}),
        ("view3d.view_axis", {"type": 'NUMPAD_3', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'LEFT')]}),
        ("view3d.view_axis", {"type": 'NUMPAD_7', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'BOTTOM')]}),
        ("view3d.view_pan", {"type": 'NUMPAD_2', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'PANDOWN')]}),
        ("view3d.view_pan", {"type": 'NUMPAD_4', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'PANLEFT')]}),
        ("view3d.view_pan", {"type": 'NUMPAD_6', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'PANRIGHT')]}),
        ("view3d.view_pan", {"type": 'NUMPAD_8', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'PANUP')]}),
        ("view3d.view_roll", {"type": 'NUMPAD_4', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LEFT')]}),
        ("view3d.view_roll", {"type": 'NUMPAD_6', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'RIGHT')]}),
        ("view3d.view_orbit", {"type": 'NUMPAD_9', "value": 'PRESS'},
         {"properties": [("angle", 3.1415927), ("type", 'ORBITRIGHT')]}),
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
        ("view3d.view_axis", {"type": 'EVT_TWEAK_M', "value": 'NORTH', "alt": True},
         {"properties": [("type", 'TOP'), ("relative", True)]}),
        ("view3d.view_axis", {"type": 'EVT_TWEAK_M', "value": 'SOUTH', "alt": True},
         {"properties": [("type", 'BOTTOM'), ("relative", True)]}),
        ("view3d.view_axis", {"type": 'EVT_TWEAK_M', "value": 'EAST', "alt": True},
         {"properties": [("type", 'RIGHT'), ("relative", True)]}),
        ("view3d.view_axis", {"type": 'EVT_TWEAK_M', "value": 'WEST', "alt": True},
         {"properties": [("type", 'LEFT'), ("relative", True)]}),
        ("view3d.view_center_pick", {"type": 'MIDDLEMOUSE', "value": 'CLICK', "alt": True}, None),
        ("view3d.ndof_orbit_zoom", {"type": 'NDOF_MOTION', "value": 'ANY'}, None),
        ("view3d.ndof_orbit", {"type": 'NDOF_MOTION', "value": 'ANY', "ctrl": True}, None),
        ("view3d.ndof_pan", {"type": 'NDOF_MOTION', "value": 'ANY', "shift": True}, None),
        ("view3d.ndof_all", {"type": 'NDOF_MOTION', "value": 'ANY', "shift": True, "ctrl": True}, None),
        ("view3d.view_selected", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'},
         {"properties": [("use_all_regions", False)]}),
        ("view3d.view_roll", {"type": 'NDOF_BUTTON_ROLL_CCW', "value": 'PRESS'},
         {"properties": [("type", 'LEFT')]}),
        ("view3d.view_roll", {"type": 'NDOF_BUTTON_ROLL_CCW', "value": 'PRESS'},
         {"properties": [("type", 'RIGHT')]}),
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
        *((operator,
           {"type": params.select_mouse, "value": params.select_mouse_value, **{m: True for m in mods}},
           {"properties": [(c, True) for c in props]},
        ) for operator, props, mods in (
            ("view3d.select", ("deselect_all",) if not params.legacy else (), ()),
            ("view3d.select", ("toggle",), ("shift",)),
            ("view3d.select", ("center", "object"), ("ctrl",)),
            ("view3d.select", ("enumerate",), ("alt",)),
            ("view3d.select", ("extend", "toggle", "center"), ("shift", "ctrl")),
            ("view3d.select", ("center", "enumerate"), ("ctrl", "alt")),
            ("view3d.select", ("toggle", "enumerate"), ("shift", "alt")),
            ("view3d.select", ("toggle", "center", "enumerate"), ("shift", "ctrl", "alt")),
        )),
        ("view3d.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("view3d.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True},
         {"properties": [("mode", 'ADD')]}),
        ("view3d.select_lasso", {"type": params.action_tweak, "value": 'ANY', "shift": True, "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        ("view3d.select_circle", {"type": 'C', "value": 'PRESS'}, None),
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
        # Transform.
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_tweak, "value": 'ANY'}, None),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        ("transform.bend", {"type": 'W', "value": 'PRESS', "shift": True}, None),
        ("transform.tosphere", {"type": 'S', "value": 'PRESS', "shift": True, "alt": True}, None),
        ("transform.shear", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ("transform.mirror", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
        ("wm.context_toggle", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'tool_settings.use_snap')]}),
        op_panel("VIEW3D_PT_snapping", {"type": 'TAB', "value": 'PRESS', "shift": True, "ctrl": True}, [("keep_open", False)]),
        ("object.transform_axis_target", {"type": 'T', "value": 'PRESS', "shift": True}, None),
        ("transform.skin_resize", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
    ])

    if not params.legacy:
        # New pie menus.
        items.extend([
            op_menu_pie("VIEW3D_MT_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True}),
            ("wm.context_toggle", {"type": 'ACCENT_GRAVE', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", 'space_data.show_gizmo')]}),
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
            op_menu("VIEW3D_MT_snap", {"type": 'S', "value": 'PRESS', "shift": True}),
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
             {"properties": [("data_path", 'tool_settings.transform_pivot_point'), ("value", 'BOUNDING_BOX_CENTER')]}),
            ("wm.context_set_enum", {"type": 'COMMA', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", 'tool_settings.transform_pivot_point'), ("value", 'MEDIAN_POINT')]}),
            ("wm.context_toggle", {"type": 'COMMA', "value": 'PRESS', "alt": True},
             {"properties": [("data_path", 'tool_settings.use_transform_pivot_point_align')]}),
            ("wm.context_toggle", {"type": 'SPACE', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", 'space_data.show_gizmo_context')]}),
            ("wm.context_set_enum", {"type": 'PERIOD', "value": 'PRESS'},
             {"properties": [("data_path", 'tool_settings.transform_pivot_point'), ("value", 'CURSOR')]}),
            ("wm.context_set_enum", {"type": 'PERIOD', "value": 'PRESS', "ctrl": True},
             {"properties": [("data_path", 'tool_settings.transform_pivot_point'), ("value", 'INDIVIDUAL_ORIGINS')]}),
            ("wm.context_set_enum", {"type": 'PERIOD', "value": 'PRESS', "alt": True},
             {"properties": [("data_path", 'tool_settings.transform_pivot_point'), ("value", 'ACTIVE_ELEMENT')]}),
            # Old shading.
            ("wm.context_toggle_enum", {"type": 'Z', "value": 'PRESS'},
             {"properties": [("data_path", 'space_data.shading.type'), ("value_1", 'WIREFRAME'), ("value_2", 'SOLID')]}),
            ("wm.context_toggle_enum", {"type": 'Z', "value": 'PRESS', "shift": True},
             {"properties": [("data_path", 'space_data.shading.type'), ("value_1", 'RENDERED'), ("value_2", 'SOLID')]}),
            ("wm.context_toggle_enum", {"type": 'Z', "value": 'PRESS', "alt": True},
             {"properties": [("data_path", 'space_data.shading.type'), ("value_1", 'MATERIAL'), ("value_2", 'SOLID')]}),
        ])

    if params.select_mouse == 'LEFTMOUSE' and not params.legacy:
        # Quick switch to select tool, since left select can't easily
        # select with any tool active.
        items.extend([
            op_tool_cycle("builtin.select_box", {"type": 'W', "value": 'PRESS'}),
        ])

    return keymap


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
             {"properties": [("extend", False), ("deselect", False), ("toggle", False),
                             ("deselect_all", not params.legacy)]}),
            ("transform.translate", {"type": 'EVT_TWEAK_R', "value": 'ANY'}, None),
        ])

    items.extend([
        ("mask.new", {"type": 'N', "value": 'PRESS', "alt": True}, None),
        op_menu("MASK_MT_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        op_menu_pie("VIEW3D_MT_proportional_editing_falloff_pie", {"type": 'O', "value": 'PRESS', "shift": True}),
        ("wm.context_toggle", {"type": 'O', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit_mask')]}),
        ("mask.add_vertex_slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("mask.add_feather_vertex_slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("mask.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("mask.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("mask.select", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", False), ("deselect", False), ("toggle", True)]}),
        *_template_items_select_actions(params, "mask.select_all"),
        ("mask.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("mask.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("mask.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("mask.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("mask.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        ("mask.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ADD')]}),
        ("mask.select_lasso", {"type": params.action_tweak, "value": 'ANY', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("mode", 'SUB')]}),
        ("mask.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("mask.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
        ("mask.hide_view_clear", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("mask.hide_view_set", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("mask.hide_view_set", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("clip.select", {"type": params.select_mouse, "value": 'PRESS', "ctrl": True},
         {"properties": [("extend", False)]}),
        ("mask.cyclic_toggle", {"type": 'C', "value": 'PRESS', "alt": True}, None),
        ("mask.slide_point", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("mask.slide_spline_curvature", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("mask.handle_type_set", {"type": 'V', "value": 'PRESS'}, None),
        ("mask.normals_make_consistent", {"type": 'N', "value": 'PRESS', "ctrl" if params.legacy else "shift": True}, None),
        ("mask.parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        ("mask.parent_clear", {"type": 'P', "value": 'PRESS', "alt": True}, None),
        ("mask.shape_key_insert", {"type": 'I', "value": 'PRESS'}, None),
        ("mask.shape_key_clear", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("mask.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("mask.copy_splines", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("mask.paste_splines", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
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
        ("marker.move", {"type": params.select_tweak, "value": 'ANY'},
         {"properties": [("tweak", True)]}),
        ("marker.duplicate", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("marker.select", {"type": params.select_mouse, "value": 'PRESS'}, None),
        ("marker.select", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("marker.select", {"type": params.select_mouse, "value": 'PRESS', "ctrl": True},
         {"properties": [("extend", False), ("camera", True)]}),
        ("marker.select", {"type": params.select_mouse, "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("extend", True), ("camera", True)]}),
        ("marker.select_box", {"type": params.select_tweak, "value": 'ANY'},
         {"properties": [("tweak", True)]}),
        ("marker.select_box", {"type": 'B', "value": 'PRESS'}, None),
        *_template_items_select_actions(params, "marker.select_all"),
        ("marker.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("marker.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("marker.rename", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
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
        ("anim.change_frame", {"type": "LEFTMOUSE", "value": 'PRESS'}, None),
        ("graph.cursor_set", {"type": "LEFTMOUSE", "value": 'PRESS'}, None),
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
        ("clip.change_frame", {"type": "LEFTMOUSE", "value": 'PRESS'}, None),
    ])

    return keymap


def km_graph_editor_generic(_params):
    items = []
    keymap = (
        "Graph Editor Generic",
        {"space_type": 'GRAPH_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("graph.extrapolation_type", {"type": 'E', "value": 'PRESS', "shift": True}, None),
        ("anim.channels_find", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("graph.hide", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("graph.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("graph.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("wm.context_set_enum", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'area.type'), ("value", 'DOPESHEET_EDITOR')]}),
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
         {"properties": [("data_path", 'space_data.show_handles')]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS'},
         {"properties": [("extend", False), ("deselect_all", not params.legacy),
                         ("column", False), ("curves", False)]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS', "alt": True},
         {"properties": [("extend", False), ("column", True), ("curves", False)]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True), ("column", False), ("curves", False)]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("extend", True), ("column", True), ("curves", False)]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("extend", False), ("column", False), ("curves", True)]}),
        ("graph.clickselect", {"type": params.select_mouse, "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("extend", True), ("column", False), ("curves", True)]}),
        ("graph.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True},
         {"properties": [("mode", 'CHECK'), ("extend", False)]}),
        ("graph.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True, "shift": True},
         {"properties": [("mode", 'CHECK'), ("extend", True)]}),
        ("graph.select_leftright", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'LEFT'), ("extend", False)]}),
        ("graph.select_leftright", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'RIGHT'), ("extend", False)]}),
        *_template_items_select_actions(params, "graph.select_all"),
        ("graph.select_box", {"type": 'B', "value": 'PRESS'},
         {"properties": [("axis_range", False), ("include_handles", False)]}),
        ("graph.select_box", {"type": 'B', "value": 'PRESS', "alt": True},
         {"properties": [("axis_range", True), ("include_handles", False)]}),
        ("graph.select_box", {"type": 'B', "value": 'PRESS', "ctrl": True},
         {"properties": [("axis_range", False), ("include_handles", True)]}),
        ("graph.select_box", {"type": 'B', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("axis_range", True), ("include_handles", True)]}),
        ("graph.select_box", {"type": params.select_tweak, "value": 'ANY'},
         {"properties": [("tweak", True), ("mode", 'SET')]}),
        ("graph.select_box", {"type": params.select_tweak, "value": 'ANY', "shift": True},
         {"properties": [("tweak", True), ("mode", 'ADD')]}),
        ("graph.select_box", {"type": params.select_tweak, "value": 'ANY', "ctrl": True},
         {"properties": [("tweak", True), ("mode", 'SUB')]}),
        ("graph.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True},
         {"properties": [("mode", 'ADD')]}),
        ("graph.select_lasso", {"type": params.action_tweak, "value": 'ANY', "shift": True, "ctrl": True},
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
        ("graph.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("graph.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
        ("graph.select_linked", {"type": 'L', "value": 'PRESS'}, None),
        ("graph.frame_jump", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        op_menu_pie("GRAPH_MT_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True}),
        ("graph.mirror", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
        ("graph.handle_type", {"type": 'V', "value": 'PRESS'}, None),
        ("graph.interpolation_type", {"type": 'T', "value": 'PRESS'}, None),
        ("graph.easing_type", {"type": 'E', "value": 'PRESS', "ctrl": True}, None),
        ("graph.smooth", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("graph.sample", {"type": 'O', "value": 'PRESS', "shift": True, "alt": True}, None),
        ("graph.bake", {"type": 'C', "value": 'PRESS', "alt": True}, None),
        op_menu("GRAPH_MT_delete", {"type": 'X', "value": 'PRESS'}),
        op_menu("GRAPH_MT_delete", {"type": 'DEL', "value": 'PRESS'}),
        op_menu("GRAPH_MT_context_menu", params.context_menu_event),
        ("graph.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("graph.keyframe_insert", {"type": 'I', "value": 'PRESS'}, None),
        ("graph.click_insert", {"type": params.action_mouse, "value": 'CLICK', "ctrl": True},
         {"properties": [("extend", False)]}),
        ("graph.click_insert", {"type": params.action_mouse, "value": 'CLICK', "shift": True, "ctrl": True},
         {"properties": [("extend", True)]}),
        ("graph.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("graph.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("graph.paste", {"type": 'V', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("flipped", True)]}),
        ("graph.previewrange_set", {"type": 'P', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("graph.view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("graph.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("graph.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("graph.view_frame", {"type": 'NUMPAD_0', "value": 'PRESS'}, None),
        ("graph.fmodifier_add", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("only_active", False)]}),
        ("anim.channels_editable_toggle", {"type": 'TAB', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_tweak, "value": 'ANY'}, None),
        ("transform.transform", {"type": 'E', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        ("wm.context_toggle", {"type": 'O', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_fcurve')]}),
        op_menu_pie("VIEW3D_MT_proportional_editing_falloff_pie", {"type": 'O', "value": 'PRESS', "shift": True}),
        op_menu_pie("GRAPH_MT_pivot_pie", {"type": 'PERIOD', "value": 'PRESS'}),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.rename", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
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


def km_image_generic(_params):
    items = []
    keymap = (
        "Image Generic",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            toolbar_key={"type": 'T', "value": 'PRESS'},
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("image.new", {"type": 'N', "value": 'PRESS', "alt": True}, None),
        ("image.open", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("image.reload", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("image.read_viewlayers", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("image.save", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("image.save_as", {"type": 'S', "value": 'PRESS', "shift": True}, None),
        ("image.cycle_render_slot", {"type": 'J', "value": 'PRESS'}, None),
        ("image.cycle_render_slot", {"type": 'J', "value": 'PRESS', "alt": True},
         {"properties": [("reverse", True)]}),
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
        ("image.view_pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("image.view_pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "shift": True}, None),
        ("image.view_pan", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("image.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("image.view_ndof", {"type": 'NDOF_MOTION', "value": 'ANY'}, None),
        ("image.view_zoom_in", {"type": 'WHEELINMOUSE', "value": 'PRESS'}, None),
        ("image.view_zoom_out", {"type": 'WHEELOUTMOUSE', "value": 'PRESS'}, None),
        ("image.view_zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS'}, None),
        ("image.view_zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS'}, None),
        ("image.view_zoom", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("image.view_zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("image.view_zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("image.view_zoom_border", {"type": 'B', "value": 'PRESS', "shift": True}, None),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_8', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 8.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_4', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 4.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_2', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 2.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_8', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 8.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_4', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 4.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_2', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 2.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_1', "value": 'PRESS'},
         {"properties": [("ratio", 1.0)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_2', "value": 'PRESS'},
         {"properties": [("ratio", 0.5)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_4', "value": 'PRESS'},
         {"properties": [("ratio", 0.25)]}),
        ("image.view_zoom_ratio", {"type": 'NUMPAD_8', "value": 'PRESS'},
         {"properties": [("ratio", 0.125)]}),
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
              {"properties": [("data_path", 'space_data.image.render_slots.active_index'), ("value", i)]})
             for i in range(9)
             )
        ),
        op_menu_pie("IMAGE_MT_pivot_pie", {"type": 'PERIOD', "value": 'PRESS'}),
        op_menu("IMAGE_MT_mask_context_menu", params.context_menu_event),
        ("image.render_border", {"type": 'B', "value": 'PRESS', "ctrl": True}, None),
        ("image.clear_render_border", {"type": 'B', "value": 'PRESS', "ctrl": True, "alt": True}, None),
    ])

    return keymap


def km_node_generic(_params):
    items = []
    keymap = (
        "Node Generic",
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
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

    def node_select_ops(select_mouse):
        return [
            ("node.select", {"type": select_mouse, "value": 'PRESS'},
             {"properties": [("extend", False), ("deselect_all", True)]}),
            ("node.select", {"type": select_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("extend", False)]}),
            ("node.select", {"type": select_mouse, "value": 'PRESS', "alt": True},
             {"properties": [("extend", False)]}),
            ("node.select", {"type": select_mouse, "value": 'PRESS', "ctrl": True, "alt": True},
             {"properties": [("extend", False)]}),
            ("node.select", {"type": select_mouse, "value": 'PRESS', "shift": True},
             {"properties": [("extend", True)]}),
            ("node.select", {"type": select_mouse, "value": 'PRESS', "shift": True, "ctrl": True},
             {"properties": [("extend", True)]}),
            ("node.select", {"type": select_mouse, "value": 'PRESS', "shift": True, "alt": True},
             {"properties": [("extend", True)]}),
            ("node.select", {"type": select_mouse, "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
             {"properties": [("extend", True)]}),
        ]

    # Allow node selection with both for RMB select
    if params.select_mouse == 'RIGHTMOUSE':
        items.extend(node_select_ops('LEFTMOUSE'))
        items.extend(node_select_ops('RIGHTMOUSE'))
    else:
        items.extend(node_select_ops('LEFTMOUSE'))

    items.extend([
        ("node.select_box", {"type": params.select_tweak, "value": 'ANY'},
         {"properties": [("tweak", True)]}),
        ("node.select_lasso", {"type": 'EVT_TWEAK_L', "value": 'ANY', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ADD')]}),
        ("node.select_lasso", {"type": 'EVT_TWEAK_L', "value": 'ANY', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("mode", 'SUB')]}),
        ("node.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        ("node.link", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties": [("detach", False)]}),
        ("node.link", {"type": 'EVT_TWEAK_L', "value": 'ANY', "ctrl": True},
         {"properties": [("detach", True)]}),
        ("node.resize", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("node.add_reroute", {"type": 'EVT_TWEAK_R', "value": 'ANY', "shift": True}, None),
        ("node.links_cut", {"type": 'EVT_TWEAK_R', "value": 'ANY', "ctrl": True}, None),
        ("node.select_link_viewer", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("node.backimage_move", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("node.backimage_zoom", {"type": 'V', "value": 'PRESS'},
         {"properties": [("factor", 1.0 / 1.2)]}),
        ("node.backimage_zoom", {"type": 'V', "value": 'PRESS', "alt": True},
         {"properties": [("factor", 1.2)]}),
        ("node.backimage_fit", {"type": 'HOME', "value": 'PRESS', "alt": True}, None),
        ("node.backimage_sample", {"type": params.action_mouse, "value": 'PRESS', "alt": True}, None),
        op_menu("NODE_MT_context_menu", params.context_menu_event),
        ("node.link_make", {"type": 'F', "value": 'PRESS'},
         {"properties": [("replace", False)]}),
        ("node.link_make", {"type": 'F', "value": 'PRESS', "shift": True},
         {"properties": [("replace", True)]}),
        op_menu("NODE_MT_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        ("node.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("node.duplicate_move_keep_inputs", {"type": 'D', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("node.parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        ("node.detach", {"type": 'P', "value": 'PRESS', "alt": True}, None),
        ("node.join", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        ("node.hide_toggle", {"type": 'H', "value": 'PRESS'}, None),
        ("node.mute_toggle", {"type": 'M', "value": 'PRESS'}, None),
        ("node.preview_toggle", {"type": 'H', "value": 'PRESS', "shift": True}, None),
        ("node.hide_socket_toggle", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
        ("node.view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("node.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("node.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("node.select_box", {"type": 'B', "value": 'PRESS'},
         {"properties": [("tweak", False)]}),
        ("node.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("node.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("node.delete_reconnect", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("node.delete_reconnect", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        *_template_items_select_actions(params, "node.select_all"),
        ("node.select_linked_to", {"type": 'L', "value": 'PRESS', "shift": True}, None),
        ("node.select_linked_from", {"type": 'L', "value": 'PRESS'}, None),
        ("node.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True},
         {"properties": [("extend", False)]}),
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
        ("node.translate_attach", {"type": 'G', "value": 'PRESS'}, None),
        ("node.translate_attach", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("node.translate_attach", {"type": params.select_tweak, "value": 'ANY'}, None),
        ("transform.translate", {"type": 'G', "value": 'PRESS'},
         {"properties": [("release_confirm", True)]}),
        ("transform.translate", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties": [("release_confirm", True)]}),
        ("transform.translate", {"type": params.select_tweak, "value": 'ANY'},
         {"properties": [("release_confirm", True)]}),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        ("node.move_detach_links", {"type": 'D', "value": 'PRESS', "alt": True}, None),
        ("node.move_detach_links_release", {"type": params.action_tweak, "value": 'ANY', "alt": True}, None),
        ("node.move_detach_links", {"type": params.select_tweak, "value": 'ANY', "alt": True}, None),
        ("wm.context_toggle", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'tool_settings.use_snap')]}),
        ("wm.context_menu_enum", {"type": 'TAB', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("data_path", 'tool_settings.snap_node_element')]}),
    ])

    return keymap


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
        ("info.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties": [("wait_for_input", False)]}),
        *_template_items_select_actions(params, "info.select_all"),
        ("info.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("info.report_replay", {"type": 'R', "value": 'PRESS'}, None),
        ("info.report_delete", {"type": 'X', "value": 'PRESS'}, None),
        ("info.report_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("info.report_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_file_browser(_params):
    items = []
    keymap = (
        "File Browser",
        {"space_type": 'FILE_BROWSER', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("file.parent", {"type": 'UP_ARROW', "value": 'PRESS', "alt": True}, None),
        ("file.previous", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True}, None),
        ("file.next", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True}, None),
        ("file.refresh", {"type": 'R', "value": 'PRESS'}, None),
        ("file.parent", {"type": 'P', "value": 'PRESS'}, None),
        ("file.previous", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("file.next", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True}, None),
        ("wm.context_toggle", {"type": 'H', "value": 'PRESS'},
         {"properties": [("data_path", 'space_data.params.show_hidden')]}),
        ("file.directory_new", {"type": 'I', "value": 'PRESS'}, None),
        ("file.smoothscroll", {"type": 'TIMER1', "value": 'ANY', "any": True}, None),
        ("file.bookmark_toggle", {"type": 'T', "value": 'PRESS'}, None),
        ("file.bookmark_add", {"type": 'B', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_file_browser_main(params):
    items = []
    keymap = (
        "File Browser Main",
        {"space_type": 'FILE_BROWSER', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("file.execute", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'},
         {"properties": [("need_active", True)]}),
        ("file.refresh", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("file.select", {"type": 'LEFTMOUSE', "value": 'CLICK'}, None),
        ("file.select", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("extend", True)]}),
        ("file.select", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True, "ctrl": True},
         {"properties": [("extend", True), ("fill", True)]}),
        ("file.select", {"type": 'RIGHTMOUSE', "value": 'CLICK'},
         {"properties": [("open", False)]}),
        ("file.select", {"type": 'RIGHTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("extend", True), ("open", False)]}),
        ("file.select", {"type": 'RIGHTMOUSE', "value": 'CLICK', "alt": True},
         {"properties": [("extend", True), ("fill", True), ("open", False)]}),
        ("file.select_walk", {"type": 'UP_ARROW', "value": 'PRESS'},
         {"properties": [("direction", 'UP')]}),
        ("file.select_walk", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'UP'), ("extend", True)]}),
        ("file.select_walk", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'UP'), ("extend", True), ("fill", True)]}),
        ("file.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS'},
         {"properties": [("direction", 'DOWN')]}),
        ("file.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'DOWN'), ("extend", True)]}),
        ("file.select_walk", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'DOWN'), ("extend", True), ("fill", True)]}),
        ("file.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS'},
         {"properties": [("direction", 'LEFT')]}),
        ("file.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'LEFT'), ("extend", True)]}),
        ("file.select_walk", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'LEFT'), ("extend", True), ("fill", True)]}),
        ("file.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS'},
         {"properties": [("direction", 'RIGHT')]}),
        ("file.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'RIGHT'), ("extend", True)]}),
        ("file.select_walk", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'RIGHT'), ("extend", True), ("fill", True)]}),
        ("file.previous", {"type": 'BUTTON4MOUSE', "value": 'CLICK'}, None),
        ("file.next", {"type": 'BUTTON5MOUSE', "value": 'CLICK'}, None),
        *_template_items_select_actions(params, "file.select_all"),
        ("file.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("file.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("file.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY', "shift": True},
         {"properties": [("mode", 'ADD')]}),
        ("file.rename", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("file.highlight", {"type": 'MOUSEMOVE', "value": 'ANY', "any": True}, None),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS'},
         {"properties": [("increment", 1)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True},
         {"properties": [("increment", 10)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("increment", 100)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS'},
         {"properties": [("increment", -1)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True},
         {"properties": [("increment", -10)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("increment", -100)]}),
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
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS'},
         {"properties": [("increment", 1)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True},
         {"properties": [("increment", 10)]}),
        ("file.filenum", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("increment", 100)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS'},
         {"properties": [("increment", -1)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True},
         {"properties": [("increment", -10)]}),
        ("file.filenum", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("increment", -100)]}),
    ])

    return keymap


def km_dopesheet_generic(_params):
    items = []
    keymap = (
        "Dopesheet Generic",
        {"space_type": 'DOPESHEET_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("wm.context_set_enum", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'area.type'), ("value", 'GRAPH_EDITOR')]})
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
        ("action.clickselect", {"type": params.select_mouse, "value": 'PRESS'},
         {"properties": [("extend", False), ("deselect_all", not params.legacy),
                         ("column", False), ("channel", False)]}),
        ("action.clickselect", {"type": params.select_mouse, "value": 'PRESS', "alt": True},
         {"properties": [("extend", False), ("column", True), ("channel", False)]}),
        ("action.clickselect", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True), ("column", False), ("channel", False)]}),
        ("action.clickselect", {"type": params.select_mouse, "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("extend", True), ("column", True), ("channel", False)]}),
        ("action.clickselect", {"type": params.select_mouse, "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("extend", False), ("column", False), ("channel", True)]}),
        ("action.clickselect", {"type": params.select_mouse, "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("extend", True), ("column", False), ("channel", True)]}),
        ("action.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True},
         {"properties": [("mode", 'CHECK'), ("extend", False)]}),
        ("action.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True, "shift": True},
         {"properties": [("mode", 'CHECK'), ("extend", True)]}),
        ("action.select_leftright", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'LEFT'), ("extend", False)]}),
        ("action.select_leftright", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'RIGHT'), ("extend", False)]}),
        *_template_items_select_actions(params, "action.select_all"),
        ("action.select_box", {"type": 'B', "value": 'PRESS'},
         {"properties": [("axis_range", False)]}),
        ("action.select_box", {"type": 'B', "value": 'PRESS', "alt": True},
         {"properties": [("axis_range", True)]}),
        ("action.select_box", {"type": params.select_tweak, "value": 'ANY'},
         {"properties": [("tweak", True), ("mode", 'SET')]}),
        ("action.select_box", {"type": params.select_tweak, "value": 'ANY', "shift": True},
         {"properties": [("tweak", True), ("mode", 'ADD')]}),
        ("action.select_box", {"type": params.select_tweak, "value": 'ANY', "ctrl": True},
         {"properties": [("tweak", True), ("mode", 'SUB')]}),
        ("action.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True},
         {"properties": [("mode", 'ADD')]}),
        ("action.select_lasso", {"type": params.action_tweak, "value": 'ANY', "shift": True, "ctrl": True},
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
        ("action.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("action.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
        ("action.select_linked", {"type": 'L', "value": 'PRESS'}, None),
        ("action.frame_jump", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        op_menu_pie("DOPESHEET_MT_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True}),
        ("action.mirror", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
        ("action.handle_type", {"type": 'V', "value": 'PRESS'}, None),
        ("action.interpolation_type", {"type": 'T', "value": 'PRESS'}, None),
        ("action.extrapolation_type", {"type": 'E', "value": 'PRESS', "shift": True}, None),
        ("action.keyframe_type", {"type": 'R', "value": 'PRESS'}, None),
        op_menu("DOPESHEET_MT_context_menu", params.context_menu_event),
        ("action.sample", {"type": 'O', "value": 'PRESS', "shift": True, "alt": True}, None),
        op_menu("DOPESHEET_MT_delete", {"type": 'X', "value": 'PRESS'}),
        op_menu("DOPESHEET_MT_delete", {"type": 'DEL', "value": 'PRESS'}),
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
        ("anim.channels_editable_toggle", {"type": 'TAB', "value": 'PRESS'}, None),
        ("anim.channels_find", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("transform.transform", {"type": 'G', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_TRANSLATE')]}),
        ("transform.transform", {"type": params.select_tweak, "value": 'ANY'},
         {"properties": [("mode", 'TIME_TRANSLATE')]}),
        ("transform.transform", {"type": 'E', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        ("transform.transform", {"type": 'S', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_SCALE')]}),
        ("transform.transform", {"type": 'T', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'TIME_SLIDE')]}),
        ("wm.context_toggle", {"type": 'O', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_action')]}),
        op_menu_pie("VIEW3D_MT_proportional_editing_falloff_pie", {"type": 'O', "value": 'PRESS', "shift": True}),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.rename", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_nla_generic(_params):
    items = []
    keymap = (
        "NLA Generic",
        {"space_type": 'NLA_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("nla.tweakmode_enter", {"type": 'TAB', "value": 'PRESS'}, None),
        ("nla.tweakmode_exit", {"type": 'TAB', "value": 'PRESS'}, None),
        ("nla.tweakmode_enter", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("isolate_action", True)]}),
        ("nla.tweakmode_exit", {"type": 'TAB', "value": 'PRESS', "shift": True},
         {"properties": [("isolate_action", True)]}),
        ("anim.channels_find", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_nla_channels(params):
    items = []
    keymap = (
        "NLA Channels",
        {"space_type": 'NLA_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("nla.channels_click", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("extend", False)]}),
        ("nla.channels_click", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("nla.tracks_add", {"type": 'A', "value": 'PRESS', "shift": True},
         {"properties": [("above_selected", False)]}),
        ("nla.tracks_add", {"type": 'A', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("above_selected", True)]}),
        ("nla.tracks_delete", {"type": 'X', "value": 'PRESS'}, None),
        ("nla.tracks_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        op_menu("NLA_MT_channel_context_menu", params.context_menu_event),
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
         {"properties": [("extend", False), ("deselect_all", not params.legacy)]}),
        ("nla.click_select", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("nla.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True},
         {"properties": [("mode", 'CHECK'), ("extend", False)]}),
        ("nla.select_leftright",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True, "shift": True},
         {"properties": [("mode", 'CHECK'), ("extend", True)]}),
        ("nla.select_leftright", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'LEFT'), ("extend", False)]}),
        ("nla.select_leftright", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'RIGHT'), ("extend", False)]}),
        *_template_items_select_actions(params, "nla.select_all"),
        ("nla.select_box", {"type": 'B', "value": 'PRESS'},
         {"properties": [("axis_range", False)]}),
        ("nla.select_box", {"type": 'B', "value": 'PRESS', "alt": True},
         {"properties": [("axis_range", True)]}),
        ("nla.select_box", {"type": params.select_tweak, "value": 'ANY'},
         {"properties": [("tweak", True), ("mode", 'SET')]}),
        ("nla.select_box", {"type": params.select_tweak, "value": 'ANY', "shift": True},
         {"properties": [("tweak", True), ("mode", 'ADD')]}),
        ("nla.select_box", {"type": params.select_tweak, "value": 'ANY', "ctrl": True},
         {"properties": [("tweak", True), ("mode", 'SUB')]}),
        ("nla.previewrange_set", {"type": 'P', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("nla.view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("nla.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("nla.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("nla.view_frame", {"type": 'NUMPAD_0', "value": 'PRESS'}, None),
        ("nla.actionclip_add", {"type": 'A', "value": 'PRESS', "shift": True}, None),
        ("nla.transition_add", {"type": 'T', "value": 'PRESS', "shift": True}, None),
        ("nla.soundclip_add", {"type": 'K', "value": 'PRESS', "shift": True}, None),
        ("nla.meta_add", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("nla.meta_remove", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("nla.duplicate", {"type": 'D', "value": 'PRESS', "shift": True},
         {"properties": [("linked", False)]}),
        ("nla.duplicate", {"type": 'D', "value": 'PRESS', "alt": True},
         {"properties": [("linked", True)]}),
        ("nla.make_single_user", {"type": 'U', "value": 'PRESS'}, None),
        ("nla.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("nla.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("nla.split", {"type": 'Y', "value": 'PRESS'}, None),
        ("nla.mute_toggle", {"type": 'H', "value": 'PRESS'}, None),
        ("nla.swap", {"type": 'F', "value": 'PRESS', "alt": True}, None),
        ("nla.move_up", {"type": 'PAGE_UP', "value": 'PRESS'}, None),
        ("nla.move_down", {"type": 'PAGE_DOWN', "value": 'PRESS'}, None),
        ("nla.apply_scale", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("nla.clear_scale", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        op_menu_pie("NLA_MT_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True}),
        op_menu("NLA_MT_context_menu", params.context_menu_event),
        ("nla.fmodifier_add", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("transform.transform", {"type": 'G', "value": 'PRESS'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("transform.transform", {"type": params.select_tweak, "value": 'ANY'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("transform.transform", {"type": 'E', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        ("transform.transform", {"type": 'S', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_SCALE')]}),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.rename", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_text_generic(_params):
    items = []
    keymap = (
        "Text Generic",
        {"space_type": 'TEXT_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            sidebar_key={"type": 'T', "value": 'PRESS', "ctrl": True},
        ),
        ("text.start_find", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("text.jump", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        ("text.find", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
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
        ("text.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("text.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("wm.context_cycle_int", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.font_size'), ("reverse", False)]}),
        ("wm.context_cycle_int", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.font_size'), ("reverse", True)]}),
        ("wm.context_cycle_int", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.font_size'), ("reverse", False)]}),
        ("wm.context_cycle_int", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.font_size'), ("reverse", True)]}),
    ])

    if not params.legacy:
        items.extend([
            ("text.new", {"type": 'N', "value": 'PRESS', "alt": True}, None),
        ])
    else:
        items.extend([
            ("text.new", {"type": 'N', "value": 'PRESS', "ctrl": True}, None),
        ])

    items.extend([
        ("text.open", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("text.reload", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("text.save", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("text.save_as", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ("text.run_script", {"type": 'P', "value": 'PRESS', "alt": True}, None),
        ("text.cut", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("text.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("text.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("text.cut", {"type": 'DEL', "value": 'PRESS', "shift": True}, None),
        ("text.copy", {"type": 'INSERT', "value": 'PRESS', "ctrl": True}, None),
        ("text.paste", {"type": 'INSERT', "value": 'PRESS', "shift": True}, None),
        ("text.duplicate_line", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("text.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("text.select_line", {"type": 'A', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("text.select_word", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("text.move_lines", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'UP')]}),
        ("text.move_lines", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("direction", 'DOWN')]}),
        ("text.indent", {"type": 'TAB', "value": 'PRESS'}, None),
        ("text.unindent", {"type": 'TAB', "value": 'PRESS', "shift": True}, None),
        ("text.uncomment", {"type": 'D', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("text.move", {"type": 'HOME', "value": 'PRESS'},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("text.move", {"type": 'END', "value": 'PRESS'},
         {"properties": [("type", 'LINE_END')]}),
        ("text.move", {"type": 'E', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'LINE_END')]}),
        ("text.move", {"type": 'E', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'LINE_END')]}),
        ("text.move", {"type": 'LEFT_ARROW', "value": 'PRESS'},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("text.move", {"type": 'RIGHT_ARROW', "value": 'PRESS'},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("text.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("text.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("text.move", {"type": 'UP_ARROW', "value": 'PRESS'},
         {"properties": [("type", 'PREVIOUS_LINE')]}),
        ("text.move", {"type": 'DOWN_ARROW', "value": 'PRESS'},
         {"properties": [("type", 'NEXT_LINE')]}),
        ("text.move", {"type": 'PAGE_UP', "value": 'PRESS'},
         {"properties": [("type", 'PREVIOUS_PAGE')]}),
        ("text.move", {"type": 'PAGE_DOWN', "value": 'PRESS'},
         {"properties": [("type", 'NEXT_PAGE')]}),
        ("text.move", {"type": 'HOME', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'FILE_TOP')]}),
        ("text.move", {"type": 'END', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'FILE_BOTTOM')]}),
        ("text.move_select", {"type": 'HOME', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("text.move_select", {"type": 'END', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_END')]}),
        ("text.move_select", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("text.move_select", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("text.move_select", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("text.move_select", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("text.move_select", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'PREVIOUS_LINE')]}),
        ("text.move_select", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'NEXT_LINE')]}),
        ("text.move_select", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'PREVIOUS_PAGE')]}),
        ("text.move_select", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'NEXT_PAGE')]}),
        ("text.move_select", {"type": 'HOME', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'FILE_TOP')]}),
        ("text.move_select", {"type": 'END', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'FILE_BOTTOM')]}),
        ("text.delete", {"type": 'DEL', "value": 'PRESS'},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("text.delete", {"type": 'BACK_SPACE', "value": 'PRESS'},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("text.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("text.delete", {"type": 'DEL', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("text.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("text.overwrite_toggle", {"type": 'INSERT', "value": 'PRESS'}, None),
        ("text.scroll_bar", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("text.scroll_bar", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("text.scroll", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
        ("text.scroll", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("text.selection_set", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("text.cursor_set", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("text.selection_set", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("select", True)]}),
        ("text.scroll", {"type": 'WHEELUPMOUSE', "value": 'PRESS'},
         {"properties": [("lines", -1)]}),
        ("text.scroll", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'},
         {"properties": [("lines", 1)]}),
        ("text.line_break", {"type": 'RET', "value": 'PRESS'}, None),
        ("text.line_break", {"type": 'NUMPAD_ENTER', "value": 'PRESS'}, None),
        op_menu("TEXT_MT_toolbox", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}),
        ("text.autocomplete", {"type": 'SPACE', "value": 'PRESS', "ctrl": True}, None),
        ("text.line_number", {"type": 'TEXTINPUT', "value": 'ANY', "any": True}, None),
        ("text.insert", {"type": 'TEXTINPUT', "value": 'ANY', "any": True}, None),
    ])

    return keymap


def km_sequencercommon(_params):
    items = []
    keymap = (
        "SequencerCommon",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("wm.context_toggle", {"type": 'O', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'scene.sequence_editor.show_overlay')]}),
        ("sequencer.view_toggle", {"type": 'TAB', "value": 'PRESS', "ctrl": True}, None),
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
        *_template_items_select_actions(params, "sequencer.select_all"),
        ("sequencer.cut", {"type": 'K', "value": 'PRESS'},
         {"properties": [("type", 'SOFT')]}),
        ("sequencer.cut", {"type": 'K', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'HARD')]}),
        ("sequencer.mute", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("sequencer.mute", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("sequencer.unmute", {"type": 'H', "value": 'PRESS', "alt": True},
         {"properties": [("unselected", False)]}),
        ("sequencer.unmute", {"type": 'H', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("unselected", True)]}),
        ("sequencer.lock", {"type": 'L', "value": 'PRESS', "shift": True}, None),
        ("sequencer.unlock", {"type": 'L', "value": 'PRESS', "shift": True, "alt": True}, None),
        ("sequencer.reassign_inputs", {"type": 'R', "value": 'PRESS'}, None),
        ("sequencer.reload", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("sequencer.reload", {"type": 'R', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("adjust_length", True)]}),
        ("sequencer.refresh_all", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.offset_clear", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("sequencer.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("sequencer.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("sequencer.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("sequencer.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.images_separate", {"type": 'Y', "value": 'PRESS'}, None),
        ("sequencer.meta_toggle", {"type": 'TAB', "value": 'PRESS'}, None),
        ("sequencer.meta_make", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.meta_separate", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("sequencer.view_all", {"type": 'HOME', "value": 'PRESS'}, None),
        ("sequencer.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("sequencer.view_selected", {"type": 'NUMPAD_PERIOD', "value": 'PRESS'}, None),
        ("sequencer.view_frame", {"type": 'NUMPAD_0', "value": 'PRESS'}, None),
        ("sequencer.strip_jump", {"type": 'PAGE_UP', "value": 'PRESS'},
         {"properties": [("next", True), ("center", False)]}),
        ("sequencer.strip_jump", {"type": 'PAGE_DOWN', "value": 'PRESS'},
         {"properties": [("next", False), ("center", False)]}),
        ("sequencer.strip_jump", {"type": 'PAGE_UP', "value": 'PRESS', "alt": True},
         {"properties": [("next", True), ("center", True)]}),
        ("sequencer.strip_jump", {"type": 'PAGE_DOWN', "value": 'PRESS', "alt": True},
         {"properties": [("next", False), ("center", True)]}),
        ("sequencer.swap", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True},
         {"properties": [("side", 'LEFT')]}),
        ("sequencer.swap", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True},
         {"properties": [("side", 'RIGHT')]}),
        ("sequencer.gap_remove", {"type": 'BACK_SPACE', "value": 'PRESS'},
         {"properties": [("all", False)]}),
        ("sequencer.gap_remove", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True},
         {"properties": [("all", True)]}),
        ("sequencer.gap_insert", {"type": 'EQUAL', "value": 'PRESS', "shift": True}, None),
        ("sequencer.snap", {"type": 'S', "value": 'PRESS', "shift": True}, None),
        ("sequencer.swap_inputs", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        *(
            (("sequencer.cut_multicam",
              {"type": NUMBERS_1[i], "value": 'PRESS'},
              {"properties": [("camera", i + 1)]})
             for i in range(10)
             )
        ),
        ("sequencer.select", {"type": params.select_mouse, "value": 'PRESS'},
         {"properties": [("extend", False), ("deselect_all", True),
                         ("linked_handle", False), ("left_right", 'NONE'), ("linked_time", False)]}),
        ("sequencer.select", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True), ("linked_handle", False), ("left_right", 'NONE'), ("linked_time", False)]}),
        ("sequencer.select", {"type": params.select_mouse, "value": 'PRESS', "alt": True},
         {"properties": [("extend", False), ("linked_handle", True), ("left_right", 'NONE'), ("linked_time", False)]}),
        ("sequencer.select", {"type": params.select_mouse, "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("extend", True), ("linked_handle", True), ("left_right", 'NONE'), ("linked_time", False)]}),
        ("sequencer.select",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True},
         {"properties": [("linked_handle", False), ("left_right", 'MOUSE'), ("linked_time", True), ("extend", False)]}),
        ("sequencer.select",
         {"type": params.select_mouse, "value": 'PRESS' if params.legacy else 'CLICK', "ctrl": True, "shift": True},
         {"properties": [("linked_handle", False), ("left_right", 'MOUSE'), ("linked_time", True), ("extend", True)]}),
        ("sequencer.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("extend", False)]}),
        ("sequencer.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("sequencer.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.select_box", {"type": params.select_tweak, "value": 'ANY'},
         {"properties": [("tweak", True), ("mode", 'SET')]}),
        ("sequencer.select_box", {"type": params.select_tweak, "value": 'ANY', "shift": True},
         {"properties": [("tweak", True), ("mode", 'ADD')]}),
        ("sequencer.select_box", {"type": params.select_tweak, "value": 'ANY', "ctrl": True},
         {"properties": [("tweak", True), ("mode", 'SUB')]}),
        ("sequencer.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("sequencer.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        op_menu("SEQUENCER_MT_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        op_menu("SEQUENCER_MT_change", {"type": 'C', "value": 'PRESS'}),
        op_menu("SEQUENCER_MT_context_menu", params.context_menu_event),
        ("sequencer.slip", {"type": 'S', "value": 'PRESS'}, None),
        ("wm.context_set_int", {"type": 'O', "value": 'PRESS'},
         {"properties": [("data_path", 'scene.sequence_editor.overlay_frame'), ("value", 0)]}),
        ("transform.seq_slide", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.seq_slide", {"type": params.select_tweak, "value": 'ANY'}, None),
        ("transform.transform", {"type": 'E', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.rename", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_sequencerpreview(params):
    items = []
    keymap = (
        "SequencerPreview",
        {"space_type": 'SEQUENCE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("sequencer.view_all_preview", {"type": 'HOME', "value": 'PRESS'}, None),
        ("sequencer.view_all_preview", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("sequencer.view_ghost_border", {"type": 'O', "value": 'PRESS'}, None),
        ("sequencer.view_zoom_ratio", {"type": 'NUMPAD_1', "value": 'PRESS'},
         {"properties": [("ratio", 1.0)]}),
        ("sequencer.sample", {"type": params.action_mouse, "value": 'PRESS'}, None),
    ])

    return keymap


def km_console(_params):
    items = []
    keymap = (
        "Console",
        {"space_type": 'CONSOLE', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("console.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("console.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("console.move", {"type": 'HOME', "value": 'PRESS'},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("console.move", {"type": 'END', "value": 'PRESS'},
         {"properties": [("type", 'LINE_END')]}),
        ("wm.context_cycle_int", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.font_size'), ("reverse", False)]}),
        ("wm.context_cycle_int", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.font_size'), ("reverse", True)]}),
        ("wm.context_cycle_int", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.font_size'), ("reverse", False)]}),
        ("wm.context_cycle_int", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.font_size'), ("reverse", True)]}),
        ("console.move", {"type": 'LEFT_ARROW', "value": 'PRESS'},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("console.move", {"type": 'RIGHT_ARROW', "value": 'PRESS'},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("console.history_cycle", {"type": 'UP_ARROW', "value": 'PRESS'},
         {"properties": [("reverse", True)]}),
        ("console.history_cycle", {"type": 'DOWN_ARROW', "value": 'PRESS'},
         {"properties": [("reverse", False)]}),
        ("console.delete", {"type": 'DEL', "value": 'PRESS'},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("console.delete", {"type": 'BACK_SPACE', "value": 'PRESS'},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("console.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("console.delete", {"type": 'DEL', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("console.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("console.clear_line", {"type": 'RET', "value": 'PRESS', "shift": True}, None),
        ("console.clear_line", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "shift": True}, None),
        ("console.execute", {"type": 'RET', "value": 'PRESS'},
         {"properties": [("interactive", True)]}),
        ("console.execute", {"type": 'NUMPAD_ENTER', "value": 'PRESS'},
         {"properties": [("interactive", True)]}),
        ("console.autocomplete", {"type": 'SPACE', "value": 'PRESS', "ctrl": True}, None),
        ("console.copy_as_script", {"type": 'C', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("console.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("console.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("console.select_set", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("console.select_word", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("console.insert", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
         {"properties": [("text", '\t')]}),
        ("console.indent", {"type": 'TAB', "value": 'PRESS'}, None),
        ("console.unindent", {"type": 'TAB', "value": 'PRESS', "shift": True}, None),
        ("console.insert", {"type": 'TEXTINPUT', "value": 'ANY', "any": True}, None),
    ])

    return keymap


def km_clip(_params):
    items = []
    keymap = (
        "Clip",
        {"space_type": 'CLIP_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_space_region_type_toggle(
            toolbar_key={"type": 'T', "value": 'PRESS'},
            sidebar_key={"type": 'N', "value": 'PRESS'},
        ),
        ("clip.open", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("clip.track_markers", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True},
         {"properties": [("backwards", True), ("sequence", False)]}),
        ("clip.track_markers", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True},
         {"properties": [("backwards", False), ("sequence", False)]}),
        ("clip.track_markers", {"type": 'T', "value": 'PRESS', "ctrl": True},
         {"properties": [("backwards", False), ("sequence", True)]}),
        ("clip.track_markers", {"type": 'T', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("backwards", True), ("sequence", True)]}),
        ("wm.context_toggle_enum", {"type": 'TAB', "value": 'PRESS'},
         {"properties": [("data_path", 'space_data.mode'), ("value_1", 'TRACKING'), ("value_2", 'MASK')]}),
        ("clip.prefetch", {"type": 'P', "value": 'PRESS'}, None),
        op_menu_pie("CLIP_MT_tracking_pie", {"type": 'E', "value": 'PRESS'}),
        op_menu_pie("CLIP_MT_solving_pie", {"type": 'S', "value": 'PRESS', "shift": True}),
        op_menu_pie("CLIP_MT_marker_pie", {"type": 'E', "value": 'PRESS', "shift": True}),
        op_menu_pie("CLIP_MT_reconstruction_pie", {"type": 'W', "value": 'PRESS', "shift": True}),
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
        ("clip.view_zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS'}, None),
        ("clip.view_zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS'}, None),
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
        ("clip.frame_jump", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("position", 'PATHSTART')]}),
        ("clip.frame_jump", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("position", 'PATHEND')]}),
        ("clip.frame_jump", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("position", 'FAILEDPREV')]}),
        ("clip.frame_jump", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("position", 'PATHSTART')]}),
        ("clip.change_frame", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("clip.select", {"type": params.select_mouse, "value": 'PRESS'},
         {"properties": [("extend", False), ("deselect_all", not params.legacy)]}),
        ("clip.select", {"type": params.select_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        *_template_items_select_actions(params, "clip.select_all"),
        ("clip.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("clip.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        op_menu("CLIP_MT_select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}),
        ("clip.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ADD')]}),
        ("clip.select_lasso", {"type": params.action_tweak, "value": 'ANY', "shift": True, "ctrl": True, "alt": True},
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
        ("clip.hide_tracks", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("clip.hide_tracks", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("clip.hide_tracks_clear", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("clip.slide_plane_marker", {"type": params.action_mouse, "value": 'PRESS'}, None),
        ("clip.keyframe_insert", {"type": 'I', "value": 'PRESS'}, None),
        ("clip.keyframe_delete", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("clip.join_tracks", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        op_menu("CLIP_MT_tracking_context_menu", params.context_menu_event),
        ("wm.context_toggle", {"type": 'L', "value": 'PRESS'},
         {"properties": [("data_path", 'space_data.lock_selection')]}),
        ("wm.context_toggle", {"type": 'D', "value": 'PRESS', "alt": True},
         {"properties": [("data_path", 'space_data.show_disabled')]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("data_path", 'space_data.show_marker_search')]}),
        ("wm.context_toggle", {"type": 'M', "value": 'PRESS'},
         {"properties": [("data_path", 'space_data.use_mute_footage')]}),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_tweak, "value": 'ANY'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "alt": True},
         {"properties": [("action", 'REMAINED'), ("clear_active", False)]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "shift": True},
         {"properties": [("action", 'UPTO'), ("clear_active", False)]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("action", 'ALL'), ("clear_active", False)]}),
        ("clip.cursor_set", params.cursor_set_event, None),
        op_menu_pie("CLIP_MT_pivot_pie", {"type": 'PERIOD', "value": 'PRESS'}),
        ("clip.copy_tracks", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("clip.paste_tracks", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
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
        ("clip.graph_select", {"type": params.select_mouse, "value": 'PRESS'},
         {"properties": [("extend", False)]}),
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
         {"properties": [("data_path", 'space_data.lock_time_cursor')]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "alt": True},
         {"properties": [("action", 'REMAINED'), ("clear_active", True)]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "shift": True},
         {"properties": [("action", 'UPTO'), ("clear_active", True)]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("action", 'ALL'), ("clear_active", True)]}),
        ("clip.graph_disable_markers", {"type": 'D', "value": 'PRESS', "shift": True},
         {"properties": [("action", 'TOGGLE')]}),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_tweak, "value": 'ANY'}, None),
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
        ("screen.frame_offset", {"type": 'LEFT_ARROW', "value": 'PRESS'},
         {"properties": [("delta", -1)]}),
        ("screen.frame_offset", {"type": 'RIGHT_ARROW', "value": 'PRESS'},
         {"properties": [("delta", 1)]}),
        ("screen.frame_jump", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("end", True)]}),
        ("screen.frame_jump", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("end", False)]}),
        ("screen.keyframe_jump", {"type": 'UP_ARROW', "value": 'PRESS'},
         {"properties": [("next", True)]}),
        ("screen.keyframe_jump", {"type": 'DOWN_ARROW', "value": 'PRESS'},
         {"properties": [("next", False)]}),
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
            assert False

        items.extend([
            ("screen.animation_play", {"type": 'SPACE', "value": 'PRESS', "shift": True, "ctrl": True},
             {"properties": [("reverse", True)]}),
        ])
    else:
        # Old playback
        items.extend([
            ("screen.frame_offset", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True},
             {"properties": [("delta", 10)]}),
            ("screen.frame_offset", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True},
             {"properties": [("delta", -10)]}),
            ("screen.frame_jump", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
             {"properties": [("end", True)]}),
            ("screen.frame_jump", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
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


def km_animation(params):
    items = []
    keymap = (
        "Animation",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Frame management.
        ("wm.context_toggle", {"type": 'T', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.show_seconds')]}),
        # Preview range.
        ("anim.previewrange_set", {"type": 'P', "value": 'PRESS'}, None),
        ("anim.previewrange_clear", {"type": 'P', "value": 'PRESS', "alt": True}, None),
        ("anim.start_frame_set", {"type": 'HOME', "value": 'PRESS', "ctrl": True}, None),
        ("anim.end_frame_set", {"type": 'END', "value": 'PRESS', "ctrl": True}, None),
    ])

    if params.select_mouse == 'LEFTMOUSE' and not params.legacy:
        items.extend([
            ("anim.change_frame", {"type": 'RIGHTMOUSE', "value": 'PRESS', "shift": True}, None),
        ])
    else:
        items.extend([
            ("anim.change_frame", {"type": params.action_mouse, "value": 'PRESS'}, None),
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
        ("anim.channels_click", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("anim.channels_click", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("children_only", True)]}),
        # Rename.
        ("anim.channels_rename", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("anim.channels_rename", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        # Select keys.
        ("anim.channel_select_keys", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("anim.channel_select_keys", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "shift": True},
         {"properties": [("extend", True)]}),
        # Find (setting the name filter).
        ("anim.channels_find", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        # Selection.
        *_template_items_select_actions(params, "anim.channels_select_all"),
        ("anim.channels_select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("anim.channels_select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
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
        ("anim.channels_move", {"type": 'PAGE_UP', "value": 'PRESS'},
         {"properties": [("direction", 'UP')]}),
        ("anim.channels_move", {"type": 'PAGE_DOWN', "value": 'PRESS'},
         {"properties": [("direction", 'DOWN')]}),
        ("anim.channels_move", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'TOP')]}),
        ("anim.channels_move", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'BOTTOM')]}),
        # Group.
        ("anim.channels_group", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("anim.channels_ungroup", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        # Menus.
        op_menu("DOPESHEET_MT_channel_context_menu", params.context_menu_event),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Modes


def km_grease_pencil(_params):
    items = []
    keymap = (
        "Grease Pencil",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Draw
        ("gpencil.annotate", {"type": 'LEFTMOUSE', "value": 'PRESS', "key_modifier": 'D'},
         {"properties": [("mode", 'DRAW'), ("wait_for_input", False)]}),
        # Draw - straight lines
        ("gpencil.annotate", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True, "key_modifier": 'D'},
         {"properties": [("mode", 'DRAW_STRAIGHT'), ("wait_for_input", False)]}),
        # Draw - poly lines
        ("gpencil.annotate", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True, "key_modifier": 'D'},
         {"properties": [("mode", 'DRAW_POLY'), ("wait_for_input", False)]}),
        # Erase
        ("gpencil.annotate", {"type": 'RIGHTMOUSE', "value": 'PRESS', "key_modifier": 'D'},
         {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),

        # Add blank frame (B because it's easy to reach from D).
        ("gpencil.blank_frame_add", {"type": 'B', "value": 'PRESS', "key_modifier": 'D'}, None),
        # Delete active frame - for easier video tutorials/review sessions.
        # This works even when not in edit mode.
        ("gpencil.active_frames_delete_all", {"type": 'X', "value": 'PRESS', "key_modifier": 'D'}, None),
        ("gpencil.active_frames_delete_all", {"type": 'DEL', "value": 'PRESS', "key_modifier": 'D'}, None),
    ])

    return keymap


def _grease_pencil_selection(params):
    return [
        # Select all
        *_template_items_select_actions(params, "gpencil.select_all"),
        # Circle select
        ("gpencil.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        # Box select
        ("gpencil.select_box", {"type": 'B', "value": 'PRESS'}, None),
        # Lasso select
        ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True},
         {"properties": [("mode", 'ADD')]}),
        ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "shift": True, "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        # In the Node Editor, lasso select needs ALT modifier too
        # (as somehow CTRL+LMB drag gets taken for "cut" quite early).
        # There probably isn't too much harm adding this for other editors too
        # as part of standard GP editing keymap. This hotkey combo doesn't seem
        # to see much use under standard scenarios?
        ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ADD')]}),
        ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("mode", 'SUB')]}),
        ("gpencil.select", {"type": params.select_mouse, "value": params.select_mouse_value, "shift": True},
         {"properties": [("extend", True), ("toggle", True)]}),
        # Whole stroke select
        ("gpencil.select", {"type": params.select_mouse, "value": params.select_mouse_value, "alt": True},
         {"properties": [("entire_strokes", True)]}),
        ("gpencil.select", {"type": params.select_mouse, "value": params.select_mouse_value, "shift": True, "alt": True},
         {"properties": [("extend", True), ("entire_strokes", True)]}),
        # Select linked
        ("gpencil.select_linked", {"type": 'L', "value": 'PRESS'}, None),
        ("gpencil.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        # Select alternate
        ("gpencil.select_alternate", {"type": 'L', "value": 'PRESS', "shift": True}, None),
        # Select grouped
        ("gpencil.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        # Select more/less
        ("gpencil.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("gpencil.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
    ]


def _grease_pencil_display():
    return [
        ("wm.context_toggle", {"type": 'Q', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'space_data.overlay.use_gpencil_edit_lines')]}),
        ("wm.context_toggle", {"type": 'Q', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("data_path", 'space_data.overlay.use_gpencil_multiedit_line_only')]}),
    ]


def km_grease_pencil_stroke_edit_mode(params):
    items = []
    keymap = (
        "Grease Pencil Stroke Edit Mode",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Interpolation
        ("gpencil.interpolate", {"type": 'E', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("gpencil.interpolate_sequence", {"type": 'E', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        # Normal select
        ("gpencil.select", {"type": params.select_mouse, "value": params.select_mouse_value},
         {"properties": [("deselect_all", not params.legacy)]}),
        # Selection
        *_grease_pencil_selection(params),
        # Duplicate and move selected points
        ("gpencil.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        # Extrude and move selected points
        ("gpencil.extrude_move", {"type": 'E', "value": 'PRESS'}, None),
        # Delete
        op_menu("VIEW3D_MT_edit_gpencil_delete", {"type": 'X', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_gpencil_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("gpencil.dissolve", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("gpencil.dissolve", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        ("gpencil.active_frames_delete_all", {"type": 'X', "value": 'PRESS', "shift": True}, None),
        ("gpencil.active_frames_delete_all", {"type": 'DEL', "value": 'PRESS', "shift": True}, None),
        # Context menu
        op_menu("VIEW3D_MT_gpencil_edit_context_menu", params.context_menu_event),
        # Separate
        op_menu("GPENCIL_MT_separate", {"type": 'P', "value": 'PRESS'}),
        # Split and joint strokes
        ("gpencil.stroke_split", {"type": 'V', "value": 'PRESS'}, None),
        ("gpencil.stroke_join", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        ("gpencil.stroke_join", {"type": 'J', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'JOINCOPY')]}),
        # Close strokes
        ("gpencil.stroke_cyclical_set", {"type": 'F', "value": 'PRESS'},
         {"properties": [("type", 'CLOSE'), ("geometry", True)]}),
        # Copy + paset
        ("gpencil.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("gpencil.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        # Snap
        op_menu("GPENCIL_MT_snap", {"type": 'S', "value": 'PRESS', "shift": True}),
        # Show/hide
        ("gpencil.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("gpencil.hide", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("gpencil.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("gpencil.selection_opacity_toggle", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
        # Display
        *_grease_pencil_display(),
        # Isolate layer
        ("gpencil.layer_isolate", {"type": 'NUMPAD_ASTERIX', "value": 'PRESS'}, None),
        # Move to layer
        ("gpencil.move_to_layer", {"type": 'M', "value": 'PRESS'}, None),
        # Transform tools
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_tweak, "value": 'ANY'}, None),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
        ("transform.mirror", {"type": 'M', "value": 'PRESS', "ctrl": True}, None),
        ("transform.bend", {"type": 'W', "value": 'PRESS', "shift": True}, None),
        ("transform.tosphere", {"type": 'S', "value": 'PRESS', "shift": True, "alt": True}, None),
        ("transform.shear", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ("transform.transform", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'GPENCIL_SHRINKFATTEN')]}),
        ("transform.transform", {"type": 'F', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'GPENCIL_OPACITY')]}),
        # Proportonal editing
        *_template_items_proportional_editing(connected=True),
        # Add menu
        ("object.gpencil_add", {"type": 'A', "value": 'PRESS', "shift": True}, None),
        # Vertex group menu
        op_menu("GPENCIL_MT_gpencil_vertex_group", {"type": 'G', "value": 'PRESS', "ctrl": True}),
        # Select mode
        ("gpencil.selectmode_toggle", {"type": 'ONE', "value": 'PRESS'},
         {"properties": [("mode", 0)]}),
        ("gpencil.selectmode_toggle", {"type": 'TWO', "value": 'PRESS'},
         {"properties": [("mode", 1)]}),
        ("gpencil.selectmode_toggle", {"type": 'THREE', "value": 'PRESS'},
         {"properties": [("mode", 2)]}),
    ])

    if params.legacy:
        items.extend([
            # Convert to geometry
            ("gpencil.convert", {"type": 'C', "value": 'PRESS', "alt": True}, None),
        ])

    return keymap


def km_grease_pencil_stroke_paint_mode(params):
    items = []
    keymap = (
        "Grease Pencil Stroke Paint Mode",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Brush strength
        ("wm.radial_control", {"type": 'F', "value": 'PRESS', "shift": True},
         {"properties": [("data_path_primary", 'tool_settings.gpencil_paint.brush.gpencil_settings.pen_strength')]}),
        # Brush size
        ("wm.radial_control", {"type": 'F', "value": 'PRESS'},
         {"properties": [("data_path_primary", 'tool_settings.gpencil_paint.brush.size')]}),
        # Draw context menu
        op_panel("VIEW3D_PT_gpencil_draw_context_menu", params.context_menu_event),
        # Draw delete menu
        op_menu("GPENCIL_MT_gpencil_draw_delete", {"type": 'X', "value": 'PRESS'}),
    ])

    return keymap


def km_grease_pencil_stroke_paint_draw_brush(params):
    items = []
    keymap = (
        "Grease Pencil Stroke Paint (Draw brush)",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Draw
        ("gpencil.draw", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'DRAW'), ("wait_for_input", False)]}),
        ("gpencil.draw", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'DRAW'), ("wait_for_input", False)]}),
        # Draw - straight lines
        ("gpencil.draw", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'DRAW_STRAIGHT'), ("wait_for_input", False)]}),
        # Draw - poly lines
        ("gpencil.draw", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("mode", 'DRAW_POLY'), ("wait_for_input", False)]}),
        # Erase
        ("gpencil.draw", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),
        # Constrain Guides Speedlines
		# Freehand
        ("gpencil.draw", {"type": 'O', "value": 'PRESS'}, None),
        ("gpencil.draw", {"type": 'J', "value": 'PRESS'}, None),
        ("gpencil.draw", {"type": 'J', "value": 'PRESS', "alt": True}, None),
        ("gpencil.draw", {"type": 'J', "value": 'PRESS', "shift": True}, None),
        ("gpencil.draw", {"type": 'K', "value": 'PRESS'}, None),
        ("gpencil.draw", {"type": 'K', "value": 'PRESS', "alt": True}, None),
        ("gpencil.draw", {"type": 'K', "value": 'PRESS', "shift": True}, None),
        ("gpencil.draw", {"type": 'L', "value": 'PRESS'}, None),
        ("gpencil.draw", {"type": 'L', "value": 'PRESS', "alt": True}, None),
        ("gpencil.draw", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("gpencil.draw", {"type": 'V', "value": 'PRESS'}, None),
		# Mirror or flip
        ("gpencil.draw", {"type": 'M', "value": 'PRESS'}, None),
		# Mode
        ("gpencil.draw", {"type": 'C', "value": 'PRESS'}, None),
		# Set reference point
        ("gpencil.draw", {"type": 'C', "value": 'PRESS', "alt": True}, None),
        # Tablet Mappings for Drawing ------------------ */
        # For now, only support direct drawing using the eraser, as most users using a tablet
        # may still want to use that as their primary pointing device!
        ("gpencil.draw", {"type": 'ERASER', "value": 'PRESS'},
         {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),
        # Selected (used by eraser)
        # Box select
        ("gpencil.select_box", {"type": 'B', "value": 'PRESS'}, None),
        # Lasso select
        ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True}, None),
    ])

    return keymap


def km_grease_pencil_stroke_paint_erase(params):
    items = []
    keymap = (
        "Grease Pencil Stroke Paint (Erase)",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Erase
        ("gpencil.draw", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),
        ("gpencil.draw", {"type": 'ERASER', "value": 'PRESS'},
         {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),
        # Box select (used by eraser)
        ("gpencil.select_box", {"type": 'B', "value": 'PRESS'}, None),
        # Lasso select
        ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True}, None),
    ])

    return keymap


def km_grease_pencil_stroke_paint_fill(_params):
    items = []
    keymap = (
        "Grease Pencil Stroke Paint (Fill)",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Fill
        ("gpencil.fill", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("on_back", False)]}),
        ("gpencil.fill", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("on_back", True)]}),
        # If press alternate key, the brush now it's for drawing areas
        ("gpencil.draw", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'DRAW'), ("wait_for_input", False), ("disable_straight", True)]}),
        # If press alternative key, the brush now it's for drawing lines
        ("gpencil.draw", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'DRAW'), ("wait_for_input", False), ("disable_straight", True), ("disable_fill", True)]}),
        # Lasso select
        ("gpencil.select_lasso", {"type": _params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True}, None),
    ])

    return keymap


def km_grease_pencil_stroke_sculpt_mode(params):
    items = []
    keymap = (
        "Grease Pencil Stroke Sculpt Mode",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items}
    )

    items.extend([
        # Selection
        *_grease_pencil_selection(params),

        # Brush strength
        ("wm.radial_control", {"type": 'F', "value": 'PRESS', "shift": True},
         {"properties": [("data_path_primary", 'tool_settings.gpencil_sculpt.brush.strength')]}),
        # Brush size
        ("wm.radial_control", {"type": 'F', "value": 'PRESS'},
         {"properties": [("data_path_primary", 'tool_settings.gpencil_sculpt.brush.size')]}),
        # Context menu
        op_panel("VIEW3D_PT_gpencil_sculpt_context_menu", params.context_menu_event),
        # Copy
        ("gpencil.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        # Display
        *_grease_pencil_display(),
    ])

    return keymap


def km_grease_pencil_stroke_weight_mode(params):
    items = []
    keymap = (
        "Grease Pencil Stroke Weight Mode",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Selection
        *_grease_pencil_selection(params),
        # Painting
        ("gpencil.sculpt_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("wait_for_input", False)]}),
        ("gpencil.sculpt_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("wait_for_input", False)]}),
        # Brush strength
        ("wm.radial_control", {"type": 'F', "value": 'PRESS', "shift": True},
         {"properties": [("data_path_primary", 'tool_settings.gpencil_sculpt.weight_brush.strength')]}),
        # Brush sze
        ("wm.radial_control", {"type": 'F', "value": 'PRESS'},
         {"properties": [("data_path_primary", 'tool_settings.gpencil_sculpt.weight_brush.size')]}),
        # Display
        *_grease_pencil_display(),
    ])

    return keymap


def km_face_mask(params):
    items = []
    keymap = (
        "Face Mask",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_items_select_actions(params, "paint.face_select_all"),
        ("paint.face_select_hide", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("paint.face_select_hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("paint.face_select_reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("paint.face_select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("paint.face_select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("paint.face_select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
    ])

    return keymap


def km_weight_paint_vertex_selection(params):
    items = []
    keymap = (
        "Weight Paint Vertex Selection",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_items_select_actions(params, "paint.vert_select_all"),
        ("view3d.select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("view3d.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True},
         {"properties": [("mode", 'ADD')]}),
        ("view3d.select_lasso", {"type": params.action_tweak, "value": 'ANY', "shift": True, "ctrl": True},
         {"properties": [("mode", 'SUB')]}),
        ("view3d.select_circle", {"type": 'C', "value": 'PRESS'}, None),
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
        ("object.parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        op_menu("VIEW3D_MT_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        ("pose.hide", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("pose.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("pose.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
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
        ("pose.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'PARENT'), ("extend", False)]}),
        ("pose.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'PARENT'), ("extend", True)]}),
        ("pose.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'CHILD'), ("extend", False)]}),
        ("pose.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'CHILD'), ("extend", True)]}),
        ("pose.select_linked", {"type": 'L', "value": 'PRESS'}, None),
        ("pose.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("pose.select_mirror", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("pose.constraint_add_with_targets", {"type": 'C', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("pose.constraints_clear", {"type": 'C', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("pose.ik_add", {"type": 'I', "value": 'PRESS', "shift": True}, None),
        ("pose.ik_clear", {"type": 'I', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        op_menu("VIEW3D_MT_pose_group", {"type": 'G', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_bone_options_toggle", {"type": 'W', "value": 'PRESS', "shift": True}),
        op_menu("VIEW3D_MT_bone_options_enable", {"type": 'W', "value": 'PRESS', "shift": True, "ctrl": True}),
        op_menu("VIEW3D_MT_bone_options_disable", {"type": 'W', "value": 'PRESS', "alt": True}),
        ("armature.layers_show_all", {"type": 'ACCENT_GRAVE', "value": 'PRESS', "ctrl": True}, None),
        ("armature.armature_layers", {"type": 'M', "value": 'PRESS', "shift": True}, None),
        ("pose.bone_layers", {"type": 'M', "value": 'PRESS'}, None),
        ("transform.transform", {"type": 'S', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("mode", 'BONE_SIZE')]}),
        ("anim.keyframe_insert_menu", {"type": 'I', "value": 'PRESS'}, None),
        ("anim.keyframe_delete_v3d", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("anim.keying_set_active_set", {"type": 'I', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ("poselib.browse_interactive", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("poselib.pose_add", {"type": 'L', "value": 'PRESS', "shift": True}, None),
        ("poselib.pose_remove", {"type": 'L', "value": 'PRESS', "alt": True}, None),
        ("poselib.pose_rename", {"type": 'L', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("pose.push", {"type": 'E', "value": 'PRESS', "ctrl": True}, None),
        ("pose.relax", {"type": 'E', "value": 'PRESS', "alt": True}, None),
        ("pose.breakdown", {"type": 'E', "value": 'PRESS', "shift": True}, None),
        op_menu("VIEW3D_MT_pose_context_menu", params.context_menu_event),
        op_menu("VIEW3D_MT_pose_propagate", {"type": 'P', "value": 'PRESS', "alt": True}),
        *(
            (("object.hide_collection",
              {"type": NUMBERS_1[i], "value": 'PRESS', "any": True},
              {"properties": [("collection_index", i + 1)]})
             for i in range(10)
             )
        ),
    ])

    return keymap


def km_object_mode(params):
    items = []
    keymap = (
        "Object Mode",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        op_menu_pie("VIEW3D_MT_proportional_editing_falloff_pie", {"type": 'O', "value": 'PRESS', "shift": True}),
        ("wm.context_toggle", {"type": 'O', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit_objects')]}),
        *_template_items_select_actions(params, "object.select_all"),
        ("object.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("object.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
        ("object.select_linked", {"type": 'L', "value": 'PRESS', "shift": True}, None),
        ("object.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("object.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'PARENT'), ("extend", False)]}),
        ("object.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'PARENT'), ("extend", True)]}),
        ("object.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'CHILD'), ("extend", False)]}),
        ("object.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'CHILD'), ("extend", True)]}),
        ("object.parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        ("object.parent_clear", {"type": 'P', "value": 'PRESS', "alt": True}, None),
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
        ("anim.keyframe_insert_menu", {"type": 'I', "value": 'PRESS'}, None),
        ("anim.keyframe_delete_v3d", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("anim.keying_set_active_set", {"type": 'I', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ("collection.create", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("collection.objects_remove", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("collection.objects_remove_all", {"type": 'G', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        ("collection.objects_add_active", {"type": 'G', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("collection.objects_remove_active", {"type": 'G', "value": 'PRESS', "shift": True, "alt": True}, None),
        op_menu("VIEW3D_MT_object_context_menu", params.context_menu_event),
        *_template_items_object_subdivision_set(),
        ("object.move_to_collection", {"type": 'M', "value": 'PRESS'}, None),
        ("object.link_to_collection", {"type": 'M', "value": 'PRESS', "shift": True}, None),
        ("object.hide_view_clear", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("object.hide_view_set", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("object.hide_view_set", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("object.hide_collection", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
        *(
            (("object.hide_collection",
              {"type": NUMBERS_1[i], "value": 'PRESS', "any": True},
              {"properties": [("collection_index", i + 1)]})
             for i in range(10)
             )
        ),
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
            ("object.proxy_make", {"type": 'P', "value": 'PRESS', "ctrl": True, "alt": True}, None),
            ("object.make_local", {"type": 'L', "value": 'PRESS'}, None),
            ("object.data_transfer", {"type": 'T', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ])

    return keymap


def km_paint_curve(params):
    items = []
    keymap = (
        "Paint Curve",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("paintcurve.add_point_slide", {"type": params.action_mouse, "value": 'PRESS', "ctrl": True}, None),
        ("paintcurve.select", {"type": params.select_mouse, "value": params.select_mouse_value}, None),
        ("paintcurve.select", {"type": params.select_mouse, "value": params.select_mouse_value, "shift": True},
         {"properties": [("extend", True)]}),
        ("paintcurve.slide", {"type": params.action_mouse, "value": 'PRESS'}, None),
        ("paintcurve.slide", {"type": params.action_mouse, "value": 'PRESS', "shift": True},
         {"properties": [("align", True)]}),
        ("paintcurve.select", {"type": 'A', "value": 'PRESS'},
         {"properties": [("toggle", True)]}),
        ("paintcurve.cursor", {"type": params.action_mouse, "value": 'PRESS'}, None),
        ("paintcurve.delete_point", {"type": 'X', "value": 'PRESS'}, None),
        ("paintcurve.delete_point", {"type": 'DEL', "value": 'PRESS'}, None),
        ("paintcurve.draw", {"type": 'RET', "value": 'PRESS'}, None),
        ("paintcurve.draw", {"type": 'NUMPAD_ENTER', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'G', "value": 'PRESS'}, None),
        ("transform.translate", {"type": params.select_tweak, "value": 'ANY'}, None),
        ("transform.rotate", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'S', "value": 'PRESS'}, None),
    ])

    return keymap


def km_curve(params):
    items = []
    keymap = (
        "Curve",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        op_menu("VIEW3D_MT_curve_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        ("curve.handle_type_set", {"type": 'V', "value": 'PRESS'}, None),
        ("curve.vertex_add", {"type": params.action_mouse, "value": 'CLICK', "ctrl": True}, None),
        *_template_items_select_actions(params, "curve.select_all"),
        ("curve.select_row", {"type": 'R', "value": 'PRESS', "shift": True}, None),
        ("curve.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("curve.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
        ("curve.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("curve.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("curve.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("curve.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("curve.shortest_path_pick", {"type": params.select_mouse, "value": 'CLICK', "ctrl": True}, None),
        ("curve.separate", {"type": 'P', "value": 'PRESS'}, None),
        ("curve.split", {"type": 'Y', "value": 'PRESS'}, None),
        ("curve.extrude_move", {"type": 'E', "value": 'PRESS'}, None),
        ("curve.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("curve.make_segment", {"type": 'F', "value": 'PRESS'}, None),
        ("curve.cyclic_toggle", {"type": 'C', "value": 'PRESS', "alt": True}, None),
        op_menu("VIEW3D_MT_edit_curve_delete", {"type": 'X', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_curve_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("curve.dissolve_verts", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("curve.dissolve_verts", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        ("curve.tilt_clear", {"type": 'T', "value": 'PRESS', "alt": True}, None),
        ("transform.tilt", {"type": 'T', "value": 'PRESS', "ctrl": True}, None),
        ("transform.transform", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'CURVE_SHRINKFATTEN')]}),
        ("curve.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("curve.hide", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("curve.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("curve.normals_make_consistent", {"type": 'N', "value": 'PRESS', "ctrl" if params.legacy else "shift": True}, None),
        ("object.vertex_parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        op_menu("VIEW3D_MT_edit_curve_context_menu", params.context_menu_event),
        op_menu("VIEW3D_MT_hook", {"type": 'H', "value": 'PRESS', "ctrl": True}),
        *_template_items_proportional_editing(connected=True),
    ])

    return keymap

# Radial control setup helpers, this operator has a lot of properties.


def radial_control_properties(paint, prop, secondary_prop, secondary_rotation=False, color=False, zoom=False):
    brush_path = 'tool_settings.' + paint + '.brush'
    unified_path = 'tool_settings.unified_paint_settings'
    rotation = 'mask_texture_slot.angle' if secondary_rotation else 'texture_slot.angle'
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
            ("zoom_path", 'space_data.zoom' if zoom else ''),
            ("image_id", brush_path + ''),
            ("secondary_tex", secondary_rotation),
        ],
    }

# Radial controls for the paint and sculpt modes.


def _template_paint_radial_control(paint, rotation=False, secondary_rotation=False, color=False, zoom=False):
    items = []

    items.extend([
        ("wm.radial_control", {"type": 'F', "value": 'PRESS'},
         radial_control_properties(paint, 'size', 'use_unified_size', secondary_rotation=secondary_rotation, color=color, zoom=zoom)),
        ("wm.radial_control", {"type": 'F', "value": 'PRESS', "shift": True},
         radial_control_properties(paint, 'strength', 'use_unified_strength', secondary_rotation=secondary_rotation, color=color)),
    ])

    if rotation:
        items.extend([
            ("wm.radial_control", {"type": 'F', "value": 'PRESS', "ctrl": True},
             radial_control_properties(paint, 'texture_slot.angle', None, color=color)),
        ])

    if secondary_rotation:
        items.extend([
            ("wm.radial_control", {"type": 'F', "value": 'PRESS', "ctrl": True, "alt": True},
             radial_control_properties(paint, 'mask_texture_slot.angle', None, secondary_rotation=secondary_rotation, color=color)),
        ])

    return items


def km_image_paint(params):
    items = []
    keymap = (
        "Image Paint",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("paint.image_paint", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'NORMAL')]}),
        ("paint.image_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("paint.brush_colors_flip", {"type": 'X', "value": 'PRESS'}, None),
        ("paint.grab_clone", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        ("paint.sample_color", {"type": 'S', "value": 'PRESS'}, None),
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
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
        ("wm.context_toggle", {"type": 'M', "value": 'PRESS'},
         {"properties": [("data_path", 'image_paint_object.data.use_paint_mask')]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'tool_settings.image_paint.brush.use_smooth_stroke')]}),
        op_menu("VIEW3D_MT_angle_control", {"type": 'R', "value": 'PRESS'}),
        ("wm.context_menu_enum", {"type": 'E', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.image_paint.brush.stroke_method')]}),
        op_panel("VIEW3D_PT_paint_texture_context_menu", params.context_menu_event),
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
        ("paint.brush_colors_flip", {"type": 'X', "value": 'PRESS'}, None),
        ("paint.sample_color", {"type": 'S', "value": 'PRESS'}, None),
        ("paint.vertex_color_set", {"type": 'K', "value": 'PRESS', "shift": True}, None),
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
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
        ("wm.context_toggle", {"type": 'M', "value": 'PRESS'},
         {"properties": [("data_path", 'vertex_paint_object.data.use_paint_mask')]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'tool_settings.vertex_paint.brush.use_smooth_stroke')]}),
        op_menu("VIEW3D_MT_angle_control", {"type": 'R', "value": 'PRESS'}),
        ("wm.context_menu_enum", {"type": 'E', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.vertex_paint.brush.stroke_method')]}),
        op_panel("VIEW3D_PT_paint_vertex_context_menu", params.context_menu_event),
    ])

    if params.legacy:
        items.extend(_template_items_legacy_tools_from_numbers())

    return keymap


def km_weight_paint(params):
    items = []
    keymap = (
        "Weight Paint",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("paint.weight_paint", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("paint.weight_sample", {"type": params.action_mouse, "value": 'PRESS', "ctrl": True}, None),
        ("paint.weight_sample_group", {"type": params.action_mouse, "value": 'PRESS', "shift": True}, None),
        ("paint.weight_gradient", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("type", 'LINEAR')]}),
        ("paint.weight_gradient", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("type", 'RADIAL')]}),
        ("paint.weight_set", {"type": 'K', "value": 'PRESS', "shift": True}, None),
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        *_template_paint_radial_control("weight_paint"),
        ("wm.radial_control", {"type": 'F', "value": 'PRESS', "ctrl": True},
         radial_control_properties("weight_paint", 'weight', 'use_unified_weight')),
        ("wm.context_menu_enum", {"type": 'E', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.vertex_paint.brush.stroke_method')]}),
        ("wm.context_toggle", {"type": 'M', "value": 'PRESS'},
         {"properties": [("data_path", 'weight_paint_object.data.use_paint_mask')]}),
        ("wm.context_toggle", {"type": 'V', "value": 'PRESS'},
         {"properties": [("data_path", 'weight_paint_object.data.use_paint_mask_vertex')]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'tool_settings.weight_paint.brush.use_smooth_stroke')]}),
        op_panel("VIEW3D_PT_paint_weight_context_menu", params.context_menu_event),
    ])

    if params.select_mouse == 'LEFTMOUSE':
        # Bone selection for combined weight paint + pose mode.
        items.extend([
            ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ])


    if params.legacy:
        items.extend(_template_items_legacy_tools_from_numbers())

    return keymap


def km_sculpt(params):
    items = []
    keymap = (
        "Sculpt",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Brush strokes
        ("sculpt.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("mode", 'NORMAL')]}),
        ("sculpt.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("sculpt.brush_stroke", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'SMOOTH')]}),
        # Partial Visibility Show/hide
        ("paint.hide_show", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("action", 'SHOW'), ("area", 'INSIDE')]}),
        ("paint.hide_show", {"type": 'H', "value": 'PRESS'},
         {"properties": [("action", 'HIDE'), ("area", 'INSIDE')]}),
        ("paint.hide_show", {"type": 'H', "value": 'PRESS', "alt": True},
         {"properties": [("action", 'SHOW'), ("area", 'ALL')]}),
        # Subdivision levels
        *_template_items_object_subdivision_set(),
        ("object.subdivision_set", {"type": 'PAGE_UP', "value": 'PRESS'},
         {"properties": [("level", 1), ("relative", True)]}),
        ("object.subdivision_set", {"type": 'PAGE_DOWN', "value": 'PRESS'},
         {"properties": [("level", -1), ("relative", True)]}),
        # Mask
        ("paint.mask_flood_fill", {"type": 'M', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'VALUE'), ("value", 0.0)]}),
        ("paint.mask_flood_fill", {"type": 'I', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'INVERT')]}),
        ("paint.mask_lasso_gesture", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("wm.context_toggle", {"type": 'M', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'scene.tool_settings.sculpt.show_mask')]}),
        # Dynamic topology
        ("sculpt.dynamic_topology_toggle", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("sculpt.set_detail_size", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        # Brush properties
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
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
        # Tools
        ("paint.brush_select", {"type": 'X', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'DRAW')]}),
        ("paint.brush_select", {"type": 'S', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'SMOOTH')]}),
        ("paint.brush_select", {"type": 'P', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'PINCH')]}),
        ("paint.brush_select", {"type": 'I', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'INFLATE')]}),
        ("paint.brush_select", {"type": 'G', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'GRAB')]}),
        ("paint.brush_select", {"type": 'L', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'LAYER')]}),
        ("paint.brush_select", {"type": 'T', "value": 'PRESS', "shift": True},
         {"properties": [("sculpt_tool", 'FLATTEN')]}),
        ("paint.brush_select", {"type": 'C', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'CLAY')]}),
        ("paint.brush_select", {"type": 'C', "value": 'PRESS', "shift": True},
         {"properties": [("sculpt_tool", 'CREASE')]}),
        ("paint.brush_select", {"type": 'K', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'SNAKE_HOOK')]}),
        ("paint.brush_select", {"type": 'M', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'MASK'), ("toggle", True), ("create_missing", True)]}),
        # Menus
        ("wm.context_menu_enum", {"type": 'E', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.sculpt.brush.stroke_method')]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'tool_settings.sculpt.brush.use_smooth_stroke')]}),
        op_menu("VIEW3D_MT_angle_control", {"type": 'R', "value": 'PRESS'}),
        op_panel("VIEW3D_PT_sculpt_context_menu", params.context_menu_event),
    ])

    if params.legacy:
        items.extend(_template_items_legacy_tools_from_numbers())

    return keymap


# Mesh edit mode.
def km_mesh(params):
    items = []
    keymap = (
        "Mesh",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Tools.
        ("mesh.loopcut_slide", {"type": 'R', "value": 'PRESS', "ctrl": True},
         {"properties": [("TRANSFORM_OT_edge_slide", [("release_confirm", False), ],)]}),
        ("mesh.offset_edge_loops_slide", {"type": 'R', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("TRANSFORM_OT_edge_slide", [("release_confirm", False), ],)]}),
        ("mesh.inset", {"type": 'I', "value": 'PRESS'}, None),
        ("mesh.bevel", {"type": 'B', "value": 'PRESS', "ctrl": True},
         {"properties": [("vertex_only", False)]}),
        ("mesh.bevel", {"type": 'B', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("vertex_only", True)]}),
        # Selection modes.
        *_template_items_editmode_mesh_select_mode(params),
        # Loop Select with alt. Double click in case MMB emulation is on (below).
        ("mesh.loop_select", {"type": params.select_mouse, "value": params.select_mouse_value, "alt": True},
         {"properties": [("extend", False), ("deselect", False), ("toggle", False)]}),
        ("mesh.loop_select", {"type": params.select_mouse, "value": params.select_mouse_value, "shift": True, "alt": True},
         {"properties": [("extend", False), ("deselect", False), ("toggle", True)]}),
        # Selection
        ("mesh.edgering_select", {"type": params.select_mouse, "value": params.select_mouse_value, "ctrl": True, "alt": True},
         {"properties": [("extend", False), ("deselect", False), ("toggle", False)]}),
        ("mesh.edgering_select", {"type": params.select_mouse, "value": params.select_mouse_value, "shift": True, "ctrl": True, "alt": True},
         {"properties": [("extend", False), ("deselect", False), ("toggle", True)]}),
        ("mesh.shortest_path_pick", {"type": params.select_mouse, "value": params.select_mouse_value, "ctrl": True},
         {"properties": [("use_fill", False)]}),
        ("mesh.shortest_path_pick", {"type": params.select_mouse, "value": params.select_mouse_value, "shift": True, "ctrl": True},
         {"properties": [("use_fill", True)]}),
        *_template_items_select_actions(params, "mesh.select_all"),
        ("mesh.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("mesh.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
        ("mesh.select_next_item", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("mesh.select_prev_item", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("mesh.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("mesh.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("mesh.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("mesh.select_mirror", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        op_menu("VIEW3D_MT_edit_mesh_select_similar", {"type": 'G', "value": 'PRESS', "shift": True}),
        # Hide/reveal.
        ("mesh.hide", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("mesh.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("mesh.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        # Tools.
        ("mesh.normals_make_consistent", {"type": 'N', "value": 'PRESS', "ctrl" if params.legacy else "shift": True},
         {"properties": [("inside", False)]}),
        ("mesh.normals_make_consistent", {"type": 'N', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("inside", True)]}),
        ("view3d.edit_mesh_extrude_move_normal", {"type": 'E', "value": 'PRESS'}, None),
        op_menu("VIEW3D_MT_edit_mesh_extrude", {"type": 'E', "value": 'PRESS', "alt": True}),
        ("transform.edge_crease", {"type": 'E', "value": 'PRESS', "shift": True}, None),
        ("mesh.fill", {"type": 'F', "value": 'PRESS', "alt": True}, None),
        ("mesh.quads_convert_to_tris", {"type": 'T', "value": 'PRESS', "ctrl": True},
         {"properties": [("quad_method", 'BEAUTY'), ("ngon_method", 'BEAUTY')]}),
        ("mesh.quads_convert_to_tris", {"type": 'T', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("quad_method", 'FIXED'), ("ngon_method", 'CLIP')]}),
        ("mesh.tris_convert_to_quads", {"type": 'J', "value": 'PRESS', "alt": True}, None),
        ("mesh.rip_move", {"type": 'V', "value": 'PRESS'},
         {"properties": [("MESH_OT_rip", [("use_fill", False), ],)]}),
        ("mesh.rip_move", {"type": 'V', "value": 'PRESS', "alt": True},
         {"properties": [("MESH_OT_rip", [("use_fill", True), ],)]}),
        ("mesh.rip_edge_move", {"type": 'D', "value": 'PRESS', "alt": True}, None),
        op_menu("VIEW3D_MT_edit_mesh_merge", {"type": 'M', "value": 'PRESS', "alt": True}),
        ("transform.shrink_fatten", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("mesh.edge_face_add", {"type": 'F', "value": 'PRESS'}, None),
        ("mesh.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        op_menu("VIEW3D_MT_mesh_add", {"type": 'A', "value": 'PRESS', "shift": True}),
        ("mesh.separate", {"type": 'P', "value": 'PRESS'}, None),
        ("mesh.split", {"type": 'Y', "value": 'PRESS'}, None),
        ("mesh.vert_connect_path", {"type": 'J', "value": 'PRESS'}, None),
        ("mesh.point_normals", {"type": 'L', "value": 'PRESS', "alt": True}, None),
        ("transform.vert_slide", {"type": 'V', "value": 'PRESS', "shift": True}, None),
        ("mesh.dupli_extrude_cursor", {"type": params.action_mouse, "value": 'CLICK', "ctrl": True},
         {"properties": [("rotate_source", True)]}),
        ("mesh.dupli_extrude_cursor", {"type": params.action_mouse, "value": 'CLICK', "shift": True, "ctrl": True},
         {"properties": [("rotate_source", False)]}),
        op_menu("VIEW3D_MT_edit_mesh_delete", {"type": 'X', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_mesh_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("mesh.dissolve_mode", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("mesh.dissolve_mode", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        ("mesh.knife_tool", {"type": 'K', "value": 'PRESS'},
         {"properties": [("use_occlude_geometry", True), ("only_selected", False)]}),
        ("object.vertex_parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        # Menus.
        op_menu("VIEW3D_MT_edit_mesh_context_menu", params.context_menu_event),
        op_menu("VIEW3D_MT_edit_mesh_faces", {"type": 'F', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_edit_mesh_edges", {"type": 'E', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_edit_mesh_vertices", {"type": 'V', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_hook", {"type": 'H', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_uv_map", {"type": 'U', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_vertex_group", {"type": 'G', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_edit_mesh_normals", {"type": 'N', "value": 'PRESS', "alt" : True}),
        ("object.vertex_group_remove_from", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        *_template_items_proportional_editing(connected=True),
    ])

    if params.use_mouse_emulate_3_button and params.select_mouse == 'LEFTMOUSE':
        items.extend([
            ("mesh.loop_select", {"type": params.select_mouse, "value": 'DOUBLE_CLICK'},
             {"properties": [("extend", False), ("deselect", False), ("toggle", False)]}),
            ("mesh.loop_select", {"type": params.select_mouse, "value": 'DOUBLE_CLICK', "shift": True},
             {"properties": [("extend", True), ("deselect", False), ("toggle", False)]}),
            ("mesh.loop_select", {"type": params.select_mouse, "value": 'DOUBLE_CLICK', "alt": True},
             {"properties": [("extend", False), ("deselect", True), ("toggle", False)]}),
            ("mesh.edgering_select", {"type": params.select_mouse, "value": 'DOUBLE_CLICK', "ctrl": True},
             {"properties": [("extend", False), ("deselect", False), ("toggle", False)]}),
            ("mesh.edgering_select", {"type": params.select_mouse, "value": 'DOUBLE_CLICK', "shift": True, "ctrl": True},
             {"properties": [("extend", False), ("deselect", False), ("toggle", True)]}),
        ])

    if params.legacy:
        items.extend([
            ("mesh.poke", {"type": 'P', "value": 'PRESS', "alt": True}, None),
            ("mesh.select_non_manifold", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
            ("mesh.faces_select_linked_flat", {"type": 'F', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
            ("mesh.spin", {"type": 'R', "value": 'PRESS', "alt": True}, None),
            ("mesh.beautify_fill", {"type": 'F', "value": 'PRESS', "shift": True, "alt": True}, None),
            ("mesh.knife_tool", {"type": 'K', "value": 'PRESS', "shift": True},
             {"properties": [("use_occlude_geometry", False), ("only_selected", True)]}),
            *_template_items_object_subdivision_set(),
        ])

    return keymap


# Armature edit mode
def km_armature(params):
    items = []
    keymap = (
        "Armature",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # Hide/reveal.
        ("armature.hide", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("armature.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("armature.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
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
        ("armature.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("armature.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
        ("armature.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("armature.select_linked", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("armature.select_linked", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("armature.shortest_path_pick", {"type": params.select_mouse, "value": params.select_mouse_value, "ctrl": True}, None),
        # Editing.
        op_menu("VIEW3D_MT_edit_armature_delete", {"type": 'X', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_armature_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("armature.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        ("armature.dissolve", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("armature.dissolve", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        ("armature.extrude_move", {"type": 'E', "value": 'PRESS'}, None),
        ("armature.extrude_forked", {"type": 'E', "value": 'PRESS', "shift": True}, None),
        ("armature.click_extrude", {"type": params.action_mouse, "value": 'CLICK', "ctrl": True}, None),
        ("armature.fill", {"type": 'F', "value": 'PRESS'}, None),
        ("armature.merge", {"type": 'M', "value": 'PRESS', "alt": True}, None),
        ("armature.split", {"type": 'Y', "value": 'PRESS'}, None),
        ("armature.separate", {"type": 'P', "value": 'PRESS'}, None),
        # Set flags.
        op_menu("VIEW3D_MT_bone_options_toggle", {"type": 'W', "value": 'PRESS', "shift": True}),
        op_menu("VIEW3D_MT_bone_options_enable", {"type": 'W', "value": 'PRESS', "shift": True, "ctrl": True}),
        op_menu("VIEW3D_MT_bone_options_disable", {"type": 'W', "value": 'PRESS', "alt": True}),
        # Armature/bone layers.
        ("armature.layers_show_all", {"type": 'ACCENT_GRAVE', "value": 'PRESS', "ctrl": True}, None),
        ("armature.armature_layers", {"type": 'M', "value": 'PRESS', "shift": True}, None),
        ("armature.bone_layers", {"type": 'M', "value": 'PRESS'}, None),
        # Special transforms.
        ("transform.transform", {"type": 'S', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("mode", 'BONE_SIZE')]}),
        ("transform.transform", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'BONE_ENVELOPE')]}),
        ("transform.transform", {"type": 'R', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'BONE_ROLL')]}),
        # Menus.
        op_menu("VIEW3D_MT_armature_context_menu", params.context_menu_event),
    ])

    return keymap


# Metaball edit mode.
def km_metaball(params):
    items = []
    keymap = (
        "Metaball",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("object.metaball_add", {"type": 'A', "value": 'PRESS', "shift": True}, None),
        ("mball.reveal_metaelems", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("mball.hide_metaelems", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("mball.hide_metaelems", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("mball.delete_metaelems", {"type": 'X', "value": 'PRESS'}, None),
        ("mball.delete_metaelems", {"type": 'DEL', "value": 'PRESS'}, None),
        ("mball.duplicate_move", {"type": 'D', "value": 'PRESS', "shift": True}, None),
        *_template_items_select_actions(params, "mball.select_all"),
        ("mball.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        *_template_items_proportional_editing(connected=True),
        op_menu("VIEW3D_MT_edit_metaball_context_menu", params.context_menu_event),
    ])

    return keymap


# Lattice edit mode.
def km_lattice(params):
    items = []
    keymap = (
        "Lattice",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_items_select_actions(params, "lattice.select_all"),
        ("lattice.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("lattice.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
        ("object.vertex_parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        ("lattice.flip", {"type": 'F', "value": 'PRESS', "alt": True}, None),
        op_menu("VIEW3D_MT_hook", {"type": 'H', "value": 'PRESS', "ctrl": True}),
        op_menu("VIEW3D_MT_edit_lattice_context_menu", params.context_menu_event),
        *_template_items_proportional_editing(connected=False),
    ])

    return keymap


# Particle edit mode.
def km_particle(params):
    items = []
    keymap = (
        "Particle",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        *_template_items_select_actions(params, "particle.select_all"),
        ("particle.select_more", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "ctrl": True}, None),
        ("particle.select_less", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "ctrl": True}, None),
        ("particle.select_linked", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("particle.select_linked", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("particle.delete", {"type": 'X', "value": 'PRESS'}, None),
        ("particle.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("particle.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("particle.hide", {"type": 'H', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("particle.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("particle.brush_edit", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("particle.brush_edit", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True}, None),
        ("wm.radial_control", {"type": 'F', "value": 'PRESS'},
         {"properties": [("data_path_primary", 'tool_settings.particle_edit.brush.size')]}),
        ("wm.radial_control", {"type": 'F', "value": 'PRESS', "shift": True},
         {"properties": [("data_path_primary", 'tool_settings.particle_edit.brush.strength')]}),
        op_menu("VIEW3D_MT_particle_context_menu", params.context_menu_event),
        ("particle.weight_set", {"type": 'K', "value": 'PRESS', "shift": True}, None),
        *_template_items_proportional_editing(connected=False),
    ])

    return keymap


# Text edit mode.
def km_font(params):
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
        ("font.delete", {"type": 'DEL', "value": 'PRESS'},
         {"properties": [("type", 'NEXT_OR_SELECTION')]}),
        ("font.delete", {"type": 'DEL', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("font.delete", {"type": 'BACK_SPACE', "value": 'PRESS'},
         {"properties": [("type", 'PREVIOUS_OR_SELECTION')]}),
        ("font.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'PREVIOUS_OR_SELECTION')]}),
        ("font.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("font.move", {"type": 'HOME', "value": 'PRESS'},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("font.move", {"type": 'END', "value": 'PRESS'},
         {"properties": [("type", 'LINE_END')]}),
        ("font.move", {"type": 'LEFT_ARROW', "value": 'PRESS'},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("font.move", {"type": 'RIGHT_ARROW', "value": 'PRESS'},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("font.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("font.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("font.move", {"type": 'UP_ARROW', "value": 'PRESS'},
         {"properties": [("type", 'PREVIOUS_LINE')]}),
        ("font.move", {"type": 'DOWN_ARROW', "value": 'PRESS'},
         {"properties": [("type", 'NEXT_LINE')]}),
        ("font.move", {"type": 'PAGE_UP', "value": 'PRESS'},
         {"properties": [("type", 'PREVIOUS_PAGE')]}),
        ("font.move", {"type": 'PAGE_DOWN', "value": 'PRESS'},
         {"properties": [("type", 'NEXT_PAGE')]}),
        ("font.move_select", {"type": 'HOME', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("font.move_select", {"type": 'END', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'LINE_END')]}),
        ("font.move_select", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'PREVIOUS_CHARACTER')]}),
        ("font.move_select", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'NEXT_CHARACTER')]}),
        ("font.move_select", {"type": 'LEFT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'PREVIOUS_WORD')]}),
        ("font.move_select", {"type": 'RIGHT_ARROW', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'NEXT_WORD')]}),
        ("font.move_select", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'PREVIOUS_LINE')]}),
        ("font.move_select", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'NEXT_LINE')]}),
        ("font.move_select", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'PREVIOUS_PAGE')]}),
        ("font.move_select", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'NEXT_PAGE')]}),
        ("font.change_spacing", {"type": 'LEFT_ARROW', "value": 'PRESS', "alt": True},
         {"properties": [("delta", -1)]}),
        ("font.change_spacing", {"type": 'RIGHT_ARROW', "value": 'PRESS', "alt": True},
         {"properties": [("delta", 1)]}),
        ("font.change_character", {"type": 'UP_ARROW', "value": 'PRESS', "alt": True},
         {"properties": [("delta", 1)]}),
        ("font.change_character", {"type": 'DOWN_ARROW', "value": 'PRESS', "alt": True},
         {"properties": [("delta", -1)]}),
        ("font.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("font.text_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("font.text_cut", {"type": 'X', "value": 'PRESS', "ctrl": True}, None),
        ("font.text_paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("font.line_break", {"type": 'RET', "value": 'PRESS'}, None),
        ("font.text_insert", {"type": 'TEXTINPUT', "value": 'ANY', "any": True}, None),
        ("font.text_insert", {"type": 'BACK_SPACE', "value": 'PRESS', "alt": True},
         {"properties": [("accent", True)]}),
        op_menu("VIEW3D_MT_edit_text_context_menu", params.context_menu_event),
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
        ])
    elif params.use_pie_click_drag:
        items.extend([
            ("object.mode_set", {"type": 'TAB', "value": 'CLICK'},
             {"properties": [("mode", 'EDIT'), ("toggle", True)]}),
            op_menu_pie("VIEW3D_MT_object_mode_pie", {"type": 'TAB', "value": 'CLICK_DRAG'}),
            ("view3d.object_mode_pie_or_toggle", {"type": 'TAB', "value": 'PRESS', "ctrl": True}, None),
        ])
    elif not params.use_v3d_tab_menu:
        items.extend([
            ("object.mode_set", {"type": 'TAB', "value": 'PRESS'},
             {"properties": [("mode", 'EDIT'), ("toggle", True)]}),
            ("view3d.object_mode_pie_or_toggle", {"type": 'TAB', "value": 'PRESS', "ctrl": True}, None),
        ])
    else:
        # Swap Tab/Ctrl-Tab
        items.extend([
            ("object.mode_set", {"type": 'TAB', "value": 'PRESS', "ctrl": True},
             {"properties": [("mode", 'EDIT'), ("toggle", True)]}),
            op_menu_pie("VIEW3D_MT_object_mode_pie", {"type": 'TAB', "value": 'PRESS'}),
        ])

    if params.legacy:
        items.extend([
            ("object.origin_set", {"type": 'C', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
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


def km_transform_modal_map(_params):
    items = []
    keymap = (
        "Transform Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
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
        ("ROTATE", {"type": 'R', "value": 'PRESS'}, None),
        ("RESIZE", {"type": 'S', "value": 'PRESS'}, None),
        ("SNAP_TOGGLE", {"type": 'TAB', "value": 'PRESS', "shift": True}, None),
        ("SNAP_INV_ON", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_INV_OFF", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("SNAP_INV_ON", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_INV_OFF", {"type": 'RIGHT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("ADD_SNAP", {"type": 'A', "value": 'PRESS'}, None),
        ("REMOVE_SNAP", {"type": 'A', "value": 'PRESS', "alt": True}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'PAGE_UP', "value": 'PRESS'}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'PAGE_DOWN', "value": 'PRESS'}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("PROPORTIONAL_SIZE_UP", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True}, None),
        ("PROPORTIONAL_SIZE_DOWN", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True}, None),
        ("PROPORTIONAL_SIZE", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("EDGESLIDE_EDGE_NEXT", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "alt": True}, None),
        ("EDGESLIDE_PREV_NEXT", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "alt": True}, None),
        ("AUTOIK_CHAIN_LEN_UP", {"type": 'PAGE_UP', "value": 'PRESS', "shift": True}, None),
        ("AUTOIK_CHAIN_LEN_DOWN", {"type": 'PAGE_DOWN', "value": 'PRESS', "shift": True}, None),
        ("AUTOIK_CHAIN_LEN_UP", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "shift": True}, None),
        ("AUTOIK_CHAIN_LEN_DOWN", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "shift": True}, None),
        ("INSERTOFS_TOGGLE_DIR", {"type": 'T', "value": 'PRESS'}, None),
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
        ("SUBTRACT", {"type": 'NUMPAD_MINUS', "value": 'PRESS'}, None),
        ("ADD", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("ADD", {"type": 'NUMPAD_PLUS', "value": 'PRESS'}, None),
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
        ("CANCEL", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'SPACE', "value": 'PRESS', "any": True}, None),
        ("NEW_CUT", {"type": 'E', "value": 'PRESS'}, None),
        ("SNAP_MIDPOINTS_ON", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_MIDPOINTS_OFF", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("SNAP_MIDPOINTS_ON", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": True}, None),
        ("SNAP_MIDPOINTS_OFF", {"type": 'RIGHT_CTRL', "value": 'RELEASE', "any": True}, None),
        ("IGNORE_SNAP_ON", {"type": 'LEFT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("IGNORE_SNAP_OFF", {"type": 'LEFT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("IGNORE_SNAP_ON", {"type": 'RIGHT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("IGNORE_SNAP_OFF", {"type": 'RIGHT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("ANGLE_SNAP_TOGGLE", {"type": 'C', "value": 'PRESS'}, None),
        ("CUT_THROUGH_TOGGLE", {"type": 'Z', "value": 'PRESS'}, None),
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
        ("VERTEX_ONLY_TOGGLE", {"type": 'V', "value": 'PRESS', "any": True}, None),
        ("HARDEN_NORMALS_TOGGLE", {"type": 'H', "value": 'PRESS', "any": True}, None),
        ("MARK_SEAM_TOGGLE", {"type": 'U', "value": 'PRESS', "any": True}, None),
        ("MARK_SHARP_TOGGLE", {"type": 'K', "value": 'PRESS', "any": True}, None),
        ("OUTER_MITER_CHANGE", {"type": 'O', "value": 'PRESS', "any": True}, None),
        ("INNER_MITER_CHANGE", {"type": 'I', "value": 'PRESS', "any": True}, None),
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
        ("ACCELERATE", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "any": True}, None),
        ("DECELERATE", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "any": True}, None),
        ("ACCELERATE", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "any": True}, None),
        ("DECELERATE", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("PAN_ENABLE", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "any": True}, None),
        ("PAN_DISABLE", {"type": 'MIDDLEMOUSE', "value": 'RELEASE', "any": True}, None),
        ("FORWARD", {"type": 'W', "value": 'PRESS'}, None),
        ("BACKWARD", {"type": 'S', "value": 'PRESS'}, None),
        ("LEFT", {"type": 'A', "value": 'PRESS'}, None),
        ("RIGHT", {"type": 'D', "value": 'PRESS'}, None),
        ("UP", {"type": 'E', "value": 'PRESS'}, None),
        ("DOWN", {"type": 'Q', "value": 'PRESS'}, None),
        ("UP", {"type": 'R', "value": 'PRESS'}, None),
        ("DOWN", {"type": 'F', "value": 'PRESS'}, None),
        ("FORWARD", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("BACKWARD", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
        ("LEFT", {"type": 'LEFT_ARROW', "value": 'PRESS'}, None),
        ("RIGHT", {"type": 'RIGHT_ARROW', "value": 'PRESS'}, None),
        ("AXIS_LOCK_X", {"type": 'X', "value": 'PRESS'}, None),
        ("AXIS_LOCK_Z", {"type": 'Z', "value": 'PRESS'}, None),
        ("PRECISION_ENABLE", {"type": 'LEFT_ALT', "value": 'PRESS', "any": True}, None),
        ("PRECISION_DISABLE", {"type": 'LEFT_ALT', "value": 'RELEASE', "any": True}, None),
        ("PRECISION_ENABLE", {"type": 'LEFT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("PRECISION_DISABLE", {"type": 'LEFT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("FREELOOK_ENABLE", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": True}, None),
        ("FREELOOK_DISABLE", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": True}, None),
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
        ("SLOW_ENABLE", {"type": 'LEFT_ALT', "value": 'PRESS', "any": True}, None),
        ("SLOW_DISABLE", {"type": 'LEFT_ALT', "value": 'RELEASE', "any": True}, None),
        ("FORWARD", {"type": 'W', "value": 'PRESS', "any": True}, None),
        ("BACKWARD", {"type": 'S', "value": 'PRESS', "any": True}, None),
        ("LEFT", {"type": 'A', "value": 'PRESS', "any": True}, None),
        ("RIGHT", {"type": 'D', "value": 'PRESS', "any": True}, None),
        ("UP", {"type": 'E', "value": 'PRESS', "any": True}, None),
        ("DOWN", {"type": 'Q', "value": 'PRESS', "any": True}, None),
        ("FORWARD_STOP", {"type": 'W', "value": 'RELEASE', "any": True}, None),
        ("BACKWARD_STOP", {"type": 'S', "value": 'RELEASE', "any": True}, None),
        ("LEFT_STOP", {"type": 'A', "value": 'RELEASE', "any": True}, None),
        ("RIGHT_STOP", {"type": 'D', "value": 'RELEASE', "any": True}, None),
        ("UP_STOP", {"type": 'E', "value": 'RELEASE', "any": True}, None),
        ("DOWN_STOP", {"type": 'Q', "value": 'RELEASE', "any": True}, None),
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
        ("ACCELERATE", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "any": True}, None),
        ("DECELERATE", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "any": True}, None),
        ("ACCELERATE", {"type": 'WHEELUPMOUSE', "value": 'PRESS', "any": True}, None),
        ("DECELERATE", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS', "any": True}, None),
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
        ("CONFIRM", {"type": 'MIDDLEMOUSE', "value": 'RELEASE', "any": True}, None),
        ("CONFIRM", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("AXIS_SNAP_ENABLE", {"type": 'LEFT_ALT', "value": 'PRESS', "any": True}, None),
        ("AXIS_SNAP_DISABLE", {"type": 'LEFT_ALT', "value": 'RELEASE', "any": True}, None),
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
        ("CONFIRM", {"type": 'MIDDLEMOUSE', "value": 'RELEASE', "any": True}, None),
        ("CONFIRM", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
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
        ("CONFIRM", {"type": 'MIDDLEMOUSE', "value": 'RELEASE', "any": True}, None),
        ("CONFIRM", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
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
        ("CONFIRM", {"type": 'MIDDLEMOUSE', "value": 'RELEASE', "any": True}, None),
        ("CONFIRM", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
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
# Popup Keymaps

def km_popup_toolbar(_params):
    return (
        "Toolbar Popup",
        {"space_type": 'EMPTY', "region_type": 'TEMPORARY'},
        {"items": [
            op_tool("builtin.select", {"type": 'W', "value": 'PRESS'}),
            op_tool("builtin.select_lasso", {"type": 'L', "value": 'PRESS'}),
            op_tool("builtin.transform", {"type": 'T', "value": 'PRESS'}),
            op_tool("builtin.measure", {"type": 'M', "value": 'PRESS'}),
        ]},
    )


# ------------------------------------------------------------------------------
# Tool System Keymaps
#
# Named are auto-generated based on the tool name and it's toolbar.


def km_generic_tool_annotate(params):
    return (
        "Generic Tool: Annotate",
        {"region_type": 'WINDOW'},
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
        {"region_type": 'WINDOW'},
        {"items": [
            ("gpencil.annotate", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("mode", 'DRAW_STRAIGHT'), ("wait_for_input", False)]}),
            ("gpencil.annotate", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),
        ]},
    )


def km_generic_tool_annotate_polygon(params):
    return (
        "Generic Tool: Annotate Polygon",
        {"region_type": 'WINDOW'},
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
        {"region_type": 'WINDOW'},
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


def km_image_editor_tool_uv_cursor(params):
    return (
        "Image Editor Tool: Uv, Cursor",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("uv.cursor_set", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            ("transform.translate", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        ]},
    )


def km_image_editor_tool_uv_select(params):
    return (
        "Image Editor Tool: Uv, Select",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select(params, "uv.select", "uv.cursor_set")},
    )


def km_image_editor_tool_uv_select_box(params):
    return (
        "Image Editor Tool: Uv, Select Box",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions_simple("uv.select_box", type=params.tool_tweak, value='ANY')},
    )


def km_image_editor_tool_uv_select_circle(params):
    return (
        "Image Editor Tool: Uv, Select Circle",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions_simple(
            "uv.select_circle", type=params.tool_mouse, value='PRESS',
            properties=[("wait_for_input", False)],
        )},
    )


def km_image_editor_tool_uv_select_lasso(params):
    return (
        "Image Editor Tool: Uv, Select Lasso",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions_simple("uv.select_lasso", type=params.tool_tweak, value='ANY')},
    )


def km_image_editor_tool_uv_sculpt_stroke(params):
    return (
        "Image Editor Tool: Uv, Sculpt Stroke",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("sculpt.uv_sculpt_stroke", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            ("sculpt.uv_sculpt_stroke", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("mode", 'INVERT')]}),
            ("sculpt.uv_sculpt_stroke", {"type": params.tool_mouse, "value": 'PRESS', "shift": True},
             {"properties": [("mode", 'RELAX')]}),
            ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
             {"properties": [("scalar", 0.9)]}),
            ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
             {"properties": [("scalar", 1.0 / 0.9)]}),
            *_template_paint_radial_control("uv_sculpt"),
        ]},
    )


def km_image_editor_tool_uv_move(params):
    return (
        "Image Editor Tool: Uv, Move",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.translate", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_image_editor_tool_uv_rotate(params):
    return (
        "Image Editor Tool: Uv, Rotate",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.rotate", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_image_editor_tool_uv_scale(params):
    return (
        "Image Editor Tool: Uv, Scale",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("transform.resize", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_node_editor_tool_select(params):
    return (
        "Node Tool: Select",
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("node.select", {"type": params.select_mouse, "value": 'PRESS'},
             {"properties": [("extend", False), ("deselect_all", not params.legacy)]}),
        ]},
    )


def km_node_editor_tool_select_box(params):
    return (
        "Node Tool: Select Box",
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions_simple(
            "node.select_box", type=params.tool_tweak, value='ANY',
            properties=[("tweak", True)],
        )},
    )


def km_node_editor_tool_select_lasso(params):
    return (
        "Node Tool: Select Lasso",
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions_simple(
            "node.select_lasso", type=params.tool_mouse, value='PRESS',
            properties=[("tweak", True)],
        )},
    )

def km_node_editor_tool_select_circle(params):
    return (
        "Node Tool: Select Circle",
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions_simple(
            "node.select_circle", type=params.tool_mouse, value='PRESS',
            properties=[("wait_for_input", False)],
        )},
    )

def km_node_editor_tool_links_cut(params):
    return (
        "Node Tool: Links Cut",
        {"space_type": 'NODE_EDITOR', "region_type": 'WINDOW'},
        {"items": [
            ("node.links_cut", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


def km_3d_view_tool_cursor(params):
    return (
        "3D View Tool: Cursor",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("view3d.cursor3d", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            ("transform.translate", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True), ("cursor_transform", True)]}),
        ]},
    )


def km_3d_view_tool_select(params):
    return (
        "3D View Tool: Select",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select(params, "view3d.select", "view3d.cursor3d")},
    )


def km_3d_view_tool_select_box(params):
    return (
        "3D View Tool: Select Box",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions("view3d.select_box", type=params.tool_tweak, value='ANY')},
    )


def km_3d_view_tool_select_circle(params):
    return (
        "3D View Tool: Select Circle",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions_simple(
            "view3d.select_circle", type=params.tool_mouse, value='PRESS',
            properties=[("wait_for_input", False)],
        )},
    )


def km_3d_view_tool_select_lasso(params):
    return (
        "3D View Tool: Select Lasso",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions("view3d.select_lasso", type=params.tool_tweak, value='ANY')},
    )


def km_3d_view_tool_transform(params):
    return (
        "3D View Tool: Transform",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.from_gizmo", {"type": params.tool_tweak, "value": 'ANY'}, None),
        ]},
    )


def km_3d_view_tool_move(params):
    return (
        "3D View Tool: Move",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.translate", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_rotate(params):
    return (
        "3D View Tool: Rotate",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.rotate", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_scale(params):
    return (
        "3D View Tool: Scale",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.resize", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_measure(params):
    return (
        "3D View Tool: Measure",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("view3d.ruler_add", {"type": params.tool_tweak, "value": 'ANY'}, None),
            ("view3d.ruler_remove", {"type": 'X', "value": 'PRESS'}, None),
            ("view3d.ruler_remove", {"type": 'DEL', "value": 'PRESS'}, None),
        ]},
    )


def km_3d_view_tool_pose_breakdowner(params):
    return (
        "3D View Tool: Pose, Breakdowner",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("pose.breakdown", {"type": params.tool_tweak, "value": 'ANY'}, None),
        ]},
    )


def km_3d_view_tool_pose_push(params):
    return (
        "3D View Tool: Pose, Push",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("pose.push", {"type": params.tool_tweak, "value": 'ANY'}, None),
        ]},
    )


def km_3d_view_tool_pose_relax(params):
    return (
        "3D View Tool: Pose, Relax",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("pose.relax", {"type": params.tool_tweak, "value": 'ANY'}, None),
        ]},
    )


def km_3d_view_tool_edit_armature_roll(params):
    return (
        "3D View Tool: Edit Armature, Roll",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.transform", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True), ("mode", 'BONE_ROLL')]}),
        ]},
    )


def km_3d_view_tool_edit_armature_bone_size(params):
    return (
        "3D View Tool: Edit Armature, Bone Size",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.transform", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True), ("mode", 'BONE_SIZE')]}),
        ]},
    )


def km_3d_view_tool_edit_armature_bone_envelope(params):
    return (
        "3D View Tool: Edit Armature, Bone Envelope",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.transform", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True), ("mode", 'BONE_ENVELOPE')]}),
        ]},
    )


def km_3d_view_tool_edit_armature_extrude(params):
    return (
        "3D View Tool: Edit Armature, Extrude",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("armature.extrude_move", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_armature_extrude_to_cursor(params):
    return (
        "3D View Tool: Edit Armature, Extrude to Cursor",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("armature.click_extrude", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


def km_3d_view_tool_edit_mesh_add_cube(params):
    return (
        "3D View Tool: Edit Mesh, Add Cube",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("view3d.cursor3d", {"type": params.tool_mouse, "value": 'CLICK'}, None),
            ("mesh.primitive_cube_add_gizmo", {"type": params.tool_tweak, "value": 'ANY'}, None),
        ]},
    )


def km_3d_view_tool_edit_mesh_extrude_region(params):
    return (
        "3D View Tool: Edit Mesh, Extrude Region",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.extrude_context_move", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_extrude_along_normals(params):
    return (
        "3D View Tool: Edit Mesh, Extrude Along Normals",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.extrude_region_shrink_fatten", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("TRANSFORM_OT_shrink_fatten", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_extrude_individual(params):
    return (
        "3D View Tool: Edit Mesh, Extrude Individual",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.extrude_faces_move", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("TRANSFORM_OT_shrink_fatten", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_extrude_to_cursor(params):
    return (
        "3D View Tool: Edit Mesh, Extrude to Cursor",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.dupli_extrude_cursor", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


def km_3d_view_tool_edit_mesh_inset_faces(params):
    return (
        "3D View Tool: Edit Mesh, Inset Faces",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.inset", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_bevel(params):
    return (
        "3D View Tool: Edit Mesh, Bevel",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.bevel", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_loop_cut(params):
    return (
        "3D View Tool: Edit Mesh, Loop Cut",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.loopcut_slide", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("TRANSFORM_OT_edge_slide", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_offset_edge_loop_cut(params):
    return (
        "3D View Tool: Edit Mesh, Offset Edge Loop Cut",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.offset_edge_loops_slide", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


def km_3d_view_tool_edit_mesh_knife(params):
    return (
        "3D View Tool: Edit Mesh, Knife",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.knife_tool", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("wait_for_input", False)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_bisect(params):
    return (
        "3D View Tool: Edit Mesh, Bisect",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.bisect", {"type": params.tool_tweak, "value": 'ANY'}, None),
        ]},
    )


def km_3d_view_tool_edit_mesh_poly_build(params):
    return (
        "3D View Tool: Edit Mesh, Poly Build",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.polybuild_face_at_cursor_move", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
            ("mesh.polybuild_split_at_cursor_move", {"type": params.tool_mouse, "value": 'PRESS', "ctrl": True},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
            ("mesh.polybuild_dissolve_at_cursor", {"type": params.tool_mouse, "value": 'CLICK', "alt": True}, None),
        ]},
    )


def km_3d_view_tool_edit_mesh_spin(params):
    return (
        "3D View Tool: Edit Mesh, Spin",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.spin", {"type": params.tool_tweak, "value": 'ANY'}, None),
        ]},
    )


def km_3d_view_tool_edit_mesh_spin_duplicate(params):
    return (
        "3D View Tool: Edit Mesh, Spin Duplicates",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.spin", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("dupli", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_smooth(params):
    return (
        "3D View Tool: Edit Mesh, Smooth",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.vertices_smooth", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("factor", 0.0)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_randomize(params):
    return (
        "3D View Tool: Edit Mesh, Randomize",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.vertex_random", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("offset", 0.0)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_edge_slide(params):
    return (
        "3D View Tool: Edit Mesh, Edge Slide",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.edge_slide", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_vertex_slide(params):
    return (
        "3D View Tool: Edit Mesh, Vertex Slide",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.vert_slide", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_shrink_fatten(params):
    return (
        "3D View Tool: Edit Mesh, Shrink/Fatten",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.shrink_fatten", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_push_pull(params):
    return (
        "3D View Tool: Edit Mesh, Push/Pull",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.push_pull", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_shear(params):
    return (
        "3D View Tool: Edit Mesh, Shear",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.shear", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_to_sphere(params):
    return (
        "3D View Tool: Edit Mesh, To Sphere",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.tosphere", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_rip_region(params):
    return (
        "3D View Tool: Edit Mesh, Rip Region",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.rip_move", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_mesh_rip_edge(params):
    return (
        "3D View Tool: Edit Mesh, Rip Edge",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.rip_edge_move", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_curve_draw(params):
    return (
        "3D View Tool: Edit Curve, Draw",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("curve.draw", {"type": params.tool_mouse, "value": 'PRESS'},
             {"properties": [("wait_for_input", False)]}),
        ]},
    )


def km_3d_view_tool_edit_curve_tilt(params):
    return (
        "3D View Tool: Edit Curve, Tilt",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.tilt", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_curve_radius(params):
    return (
        "3D View Tool: Edit Curve, Radius",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.transform", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("mode", 'CURVE_SHRINKFATTEN'), ("release_confirm", True)]}),
        ]},
    )

def km_3d_view_tool_edit_curve_randomize(params):
    return (
        "3D View Tool: Edit Curve, Randomize",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.vertex_random", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("offset", 0.0)]}),
        ]},
    )

def km_3d_view_tool_edit_curve_extrude(params):
    return (
        "3D View Tool: Edit Curve, Extrude",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("curve.extrude_move", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
        ]},
    )


def km_3d_view_tool_edit_curve_extrude_cursor(params):
    return (
        "3D View Tool: Edit Curve, Extrude Cursor",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("curve.vertex_add", {"type": params.tool_mouse, "value": 'PRESS'}, None),
        ]},
    )


def km_3d_view_tool_sculpt_box_hide(params):
    return (
        "3D View Tool: Sculpt, Box Hide",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.hide_show", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("action", 'HIDE')]}),
            ("paint.hide_show", {"type": params.tool_tweak, "value": 'ANY', "ctrl": True},
             {"properties": [("action", 'SHOW')]}),
            ("paint.hide_show", {"type": params.select_mouse, "value": 'PRESS'},
             {"properties": [("action", 'SHOW'), ("area", 'ALL')]}),
        ]},
    )


def km_3d_view_tool_sculpt_box_mask(params):
    return (
        "3D View Tool: Sculpt, Box Mask",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("view3d.select_box", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("mode", 'ADD')]}),
            ("view3d.select_box", {"type": params.tool_tweak, "value": 'ANY', "ctrl": True},
             {"properties": [("mode", 'SUB')]}),
        ]},
    )


def km_3d_view_tool_sculpt_lasso_mask(params):
    return (
        "3D View Tool: Sculpt, Lasso Mask",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.mask_lasso_gesture", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("value", 1.0)]}),
            ("paint.mask_lasso_gesture", {"type": params.tool_tweak, "value": 'ANY', "ctrl": True},
             {"properties": [("value", 0.0)]}),
        ]},
    )


def km_3d_view_tool_paint_weight_sample_weight(params):
    return (
        "3D View Tool: Paint Weight, Sample Weight",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("paint.weight_sample", {"type": params.tool_mouse, "value": 'PRESS'}, None),
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
            ("paint.weight_gradient", {"type": params.tool_tweak, "value": 'ANY'}, None),
        ]},
    )


def km_3d_view_tool_paint_gpencil_line(params):
    return (
        "3D View Tool: Paint Gpencil, Line",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("gpencil.primitive", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("type", 'LINE'), ("wait_for_input", False)]}),
            ("gpencil.primitive", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
             {"properties": [("type", 'LINE'), ("wait_for_input", False)]}),
            ("gpencil.primitive", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
             {"properties": [("type", 'LINE'), ("wait_for_input", False)]}),
            # Lasso select
            ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True}, None),
        ]},
    )


def km_3d_view_tool_paint_gpencil_box(params):
    return (
        "3D View Tool: Paint Gpencil, Box",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("gpencil.primitive", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("type", 'BOX'), ("wait_for_input", False)]}),
            ("gpencil.primitive", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
             {"properties": [("type", 'BOX'), ("wait_for_input", False)]}),
            ("gpencil.primitive", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
             {"properties": [("type", 'BOX'), ("wait_for_input", False)]}),
            # Lasso select
            ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True}, None),
        ]},
    )


def km_3d_view_tool_paint_gpencil_circle(params):
    return (
        "3D View Tool: Paint Gpencil, Circle",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("gpencil.primitive", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("type", 'CIRCLE'), ("wait_for_input", False)]}),
            ("gpencil.primitive", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
             {"properties": [("type", 'CIRCLE'), ("wait_for_input", False)]}),
            ("gpencil.primitive", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
             {"properties": [("type", 'CIRCLE'), ("wait_for_input", False)]}),
            # Lasso select
            ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True}, None),
        ]},
    )


def km_3d_view_tool_paint_gpencil_arc(params):
    return (
        "3D View Tool: Paint Gpencil, Arc",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("gpencil.primitive", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("type", 'ARC'), ("wait_for_input", False)]}),
            ("gpencil.primitive", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
             {"properties": [("type", 'ARC'), ("wait_for_input", False)]}),
            ("gpencil.primitive", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
             {"properties": [("type", 'ARC'), ("wait_for_input", False)]}),
            # Lasso select
            ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True}, None),
        ]},
    )


def km_3d_view_tool_paint_gpencil_curve(params):
    return (
        "3D View Tool: Paint Gpencil, Curve",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("gpencil.primitive", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("type", 'CURVE'), ("wait_for_input", False)]}),
            # Lasso select
            ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True}, None),
        ]},
    )


def km_3d_view_tool_paint_gpencil_cutter(params):
    return (
        "3D View Tool: Paint Gpencil, Cutter",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("gpencil.stroke_cutter", {"type": params.tool_mouse, "value": 'PRESS'}, None),
            # Lasso select
            ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True}, None),
        ]},
    )


def km_3d_view_tool_edit_gpencil_select(params):
    return (
        "3D View Tool: Edit Gpencil, Select",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select(params, "gpencil.select", "view3d.cursor3d")},
    )


def km_3d_view_tool_edit_gpencil_select_box(params):
    return (
        "3D View Tool: Edit Gpencil, Select Box",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions("gpencil.select_box", type=params.tool_tweak, value='ANY')},
    )


def km_3d_view_tool_edit_gpencil_select_circle(params):
    return (
        "3D View Tool: Edit Gpencil, Select Circle",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions_simple(
            "gpencil.select_circle", type=params.tool_mouse, value='PRESS',
            properties=[("wait_for_input", False)],
        )},
    )


def km_3d_view_tool_edit_gpencil_select_lasso(params):
    return (
        "3D View Tool: Edit Gpencil, Select Lasso",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions("gpencil.select_lasso", type=params.tool_tweak, value='ANY')},
    )


def km_3d_view_tool_edit_gpencil_radius(params):
    return (
        "3D View Tool: Edit Gpencil, Radius",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.transform", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("mode", 'CURVE_SHRINKFATTEN'), ("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_gpencil_bend(params):
    return (
        "3D View Tool: Edit Gpencil, Bend",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.bend", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_gpencil_shear(params):
    return (
        "3D View Tool: Edit Gpencil, Shear",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.shear", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


def km_3d_view_tool_edit_gpencil_to_sphere(params):
    return (
        "3D View Tool: Edit Gpencil, To Sphere",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.tosphere", {"type": params.tool_tweak, "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
        ]},
    )


# Also used for weight paint.
def km_3d_view_tool_sculpt_gpencil_paint(_params):
    return (
        "3D View Tool: Sculpt Gpencil, Paint",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("gpencil.sculpt_paint", {"type": 'LEFTMOUSE', "value": 'PRESS'},
             {"properties": [("wait_for_input", False)]}),
            ("gpencil.sculpt_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
             {"properties": [("wait_for_input", False)]}),
            ("gpencil.sculpt_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
             {"properties": [("wait_for_input", False)]}),
        ]},
    )


def km_3d_view_tool_sculpt_gpencil_select(params):
    return (
        "3D View Tool: Sculpt Gpencil, Select",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select(params, "gpencil.select", "view3d.cursor3d")},
    )


def km_3d_view_tool_sculpt_gpencil_select_box(params):
    return (
        "3D View Tool: Sculpt Gpencil, Select Box",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions("gpencil.select_box", type=params.tool_tweak, value='ANY')},
    )


def km_3d_view_tool_sculpt_gpencil_select_circle(params):
    return (
        "3D View Tool: Sculpt Gpencil, Select Circle",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions_simple(
            "gpencil.select_circle", type=params.tool_mouse, value='PRESS',
            properties=[("wait_for_input", False)],
        )},
    )


def km_3d_view_tool_sculpt_gpencil_select_lasso(params):
    return (
        "3D View Tool: Sculpt Gpencil, Select Lasso",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": _template_items_tool_select_actions("gpencil.select_lasso", type=params.tool_tweak, value='ANY')},
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
        km_info(params),
        km_file_browser(params),
        km_file_browser_main(params),
        km_file_browser_buttons(params),
        km_dopesheet_generic(params),
        km_dopesheet(params),
        km_nla_generic(params),
        km_nla_channels(params),
        km_nla_editor(params),
        km_text_generic(params),
        km_text(params),
        km_sequencercommon(params),
        km_sequencer(params),
        km_sequencerpreview(params),
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
        km_grease_pencil(params),
        km_grease_pencil_stroke_edit_mode(params),
        km_grease_pencil_stroke_paint_mode(params),
        km_grease_pencil_stroke_paint_draw_brush(params),
        km_grease_pencil_stroke_paint_erase(params),
        km_grease_pencil_stroke_paint_fill(params),
        km_grease_pencil_stroke_sculpt_mode(params),
        km_grease_pencil_stroke_weight_mode(params),
        km_face_mask(params),
        km_weight_paint_vertex_selection(params),
        km_pose(params),
        km_object_mode(params),
        km_paint_curve(params),
        km_curve(params),
        km_image_paint(params),
        km_vertex_paint(params),
        km_weight_paint(params),
        km_sculpt(params),
        km_mesh(params),
        km_armature(params),
        km_metaball(params),
        km_lattice(params),
        km_particle(params),
        km_font(params),
        km_object_non_modal(params),

        # Modal maps.
        km_eyedropper_modal_map(params),
        km_eyedropper_colorramp_pointsampling_map(params),
        km_transform_modal_map(params),
        km_view3d_gesture_circle(params),
        km_gesture_border(params),
        km_gesture_zoom_border(params),
        km_gesture_straight_line(params),
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

        # Gizmos.
        km_generic_gizmo(params),
        km_generic_gizmo_drag(params),
        km_generic_gizmo_maybe_drag(params),
        km_generic_gizmo_click_drag(params),
        km_generic_gizmo_select(params),
        km_generic_gizmo_tweak_modal_map(params),

        # Pop-Up Keymaps.
        km_popup_toolbar(params),

        # Tool System.
        km_generic_tool_annotate(params),
        km_generic_tool_annotate_line(params),
        km_generic_tool_annotate_polygon(params),
        km_generic_tool_annotate_eraser(params),

        km_image_editor_tool_generic_sample(params),
        km_image_editor_tool_uv_cursor(params),
        km_image_editor_tool_uv_select(params),
        km_image_editor_tool_uv_select_box(params),
        km_image_editor_tool_uv_select_circle(params),
        km_image_editor_tool_uv_select_lasso(params),
        km_image_editor_tool_uv_sculpt_stroke(params),
        km_image_editor_tool_uv_move(params),
        km_image_editor_tool_uv_rotate(params),
        km_image_editor_tool_uv_scale(params),
        km_node_editor_tool_select(params),
        km_node_editor_tool_select_box(params),
        km_node_editor_tool_select_lasso(params),
        km_node_editor_tool_select_circle(params),
        km_node_editor_tool_links_cut(params),
        km_3d_view_tool_cursor(params),
        km_3d_view_tool_select(params),
        km_3d_view_tool_select_box(params),
        km_3d_view_tool_select_circle(params),
        km_3d_view_tool_select_lasso(params),
        km_3d_view_tool_transform(params),
        km_3d_view_tool_move(params),
        km_3d_view_tool_rotate(params),
        km_3d_view_tool_scale(params),
        km_3d_view_tool_measure(params),
        km_3d_view_tool_pose_breakdowner(params),
        km_3d_view_tool_pose_push(params),
        km_3d_view_tool_pose_relax(params),
        km_3d_view_tool_edit_armature_roll(params),
        km_3d_view_tool_edit_armature_bone_size(params),
        km_3d_view_tool_edit_armature_bone_envelope(params),
        km_3d_view_tool_edit_armature_extrude(params),
        km_3d_view_tool_edit_armature_extrude_to_cursor(params),
        km_3d_view_tool_edit_mesh_add_cube(params),
        km_3d_view_tool_edit_mesh_extrude_region(params),
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
        km_3d_view_tool_edit_mesh_shear(params),
        km_3d_view_tool_edit_mesh_to_sphere(params),
        km_3d_view_tool_edit_mesh_rip_region(params),
        km_3d_view_tool_edit_mesh_rip_edge(params),
        km_3d_view_tool_edit_curve_draw(params),
        km_3d_view_tool_edit_curve_radius(params),
        km_3d_view_tool_edit_curve_tilt(params),
        km_3d_view_tool_edit_curve_randomize(params),
        km_3d_view_tool_edit_curve_extrude(params),
        km_3d_view_tool_edit_curve_extrude_cursor(params),
        km_3d_view_tool_sculpt_box_hide(params),
        km_3d_view_tool_sculpt_box_mask(params),
        km_3d_view_tool_sculpt_lasso_mask(params),
        km_3d_view_tool_paint_weight_sample_weight(params),
        km_3d_view_tool_paint_weight_sample_vertex_group(params),
        km_3d_view_tool_paint_weight_gradient(params),
        km_3d_view_tool_paint_gpencil_line(params),
        km_3d_view_tool_paint_gpencil_box(params),
        km_3d_view_tool_paint_gpencil_circle(params),
        km_3d_view_tool_paint_gpencil_arc(params),
        km_3d_view_tool_paint_gpencil_curve(params),
        km_3d_view_tool_paint_gpencil_cutter(params),
        km_3d_view_tool_edit_gpencil_select(params),
        km_3d_view_tool_edit_gpencil_select_box(params),
        km_3d_view_tool_edit_gpencil_select_circle(params),
        km_3d_view_tool_edit_gpencil_select_lasso(params),
        km_3d_view_tool_edit_gpencil_radius(params),
        km_3d_view_tool_edit_gpencil_bend(params),
        km_3d_view_tool_edit_gpencil_shear(params),
        km_3d_view_tool_edit_gpencil_to_sphere(params),
        km_3d_view_tool_sculpt_gpencil_paint(params),
        km_3d_view_tool_sculpt_gpencil_select(params),
        km_3d_view_tool_sculpt_gpencil_select_box(params),
        km_3d_view_tool_sculpt_gpencil_select_circle(params),
        km_3d_view_tool_sculpt_gpencil_select_lasso(params),
    ]

# ------------------------------------------------------------------------------
# Refactoring (Testing Only)
#
# Allows running outside of Blender to generate data for diffing
#
# To compare:
#
#    python3 release/scripts/presets/keyconfig/keymap_data/blender_default.py && \
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
#    pylint release/scripts/presets/keyconfig/keymap_data/blender_default.py --disable=C0111,C0301,C0302,R0902,R0903,R0913
