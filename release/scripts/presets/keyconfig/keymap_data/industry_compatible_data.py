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
        "select_mouse",
        "select_mouse_value",
        "select_tweak",
        "action_mouse",
        "action_tweak",
        "tool_mouse",
        "tool_tweak",
        "use_mouse_emulate_3_button",

    )

    def __init__(
            self,
            *,
            use_mouse_emulate_3_button=False,
    ):
        self.tool_mouse = 'LEFTMOUSE'
        self.tool_tweak = 'EVT_TWEAK_L'
        self.action_tweak = 'EVT_TWEAK_R'
        self.use_mouse_emulate_3_button = use_mouse_emulate_3_button


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


def _template_items_object_subdivision_set():
    return [
        ("object.subdivision_set",
         {"type": NUMBERS_0[i], "value": 'PRESS', "ctrl": True},
         {"properties": [("level", i), ("relative", False)]})
        for i in range(6)
    ]


def _template_items_animation():
    return [
        ("screen.frame_offset", {"type": 'LEFT_ARROW', "value": 'PRESS'},
         {"properties": [("delta", -1)]}),
        ("screen.frame_offset", {"type": 'RIGHT_ARROW', "value": 'PRESS'},
         {"properties": [("delta", 1)]}),
        ("screen.frame_jump", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("end", True)]}),
        ("screen.frame_jump", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("end", False)]}),

    ]

# Gizmos

def _template_items_gizmo_tweak_value():
    return [
        ("gizmogroup.gizmo_tweak", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
    ]


def _template_items_gizmo_tweak_value_click_drag():
    return [
        ("gizmogroup.gizmo_tweak", {"type": 'LEFTMOUSE', "value": 'CLICK'}, None),
        ("gizmogroup.gizmo_tweak", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
    ]


def _template_items_gizmo_tweak_value_drag():
    return [
        ("gizmogroup.gizmo_tweak", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
    ]


# Tool System Templates

def _template_items_basic_tools(*, connected=False):
    return [
        op_tool("builtin.select_box", {"type": 'Q', "value": 'PRESS'}),
        op_tool("builtin.move", {"type": 'W', "value": 'PRESS'}),
        op_tool("builtin.rotate", {"type": 'E', "value": 'PRESS'}),
        op_tool("builtin.scale", {"type": 'R', "value": 'PRESS'}),
        op_tool("builtin.transform", {"type": 'T', "value": 'PRESS'}),
        op_tool("builtin.scale_cage", {"type": 'R', "value": 'PRESS', "shift": True}),
        op_tool("builtin.annotate", {"type": 'D', "value": 'PRESS'}),
        op_tool("builtin.measure", {"type": 'M', "value": 'PRESS'}),
        op_tool("builtin.cursor", {"type": 'C', "value": 'PRESS'}),
    ]

def _template_items_tool_select(params, operator, cursor_operator):
        return [(operator, {"type": 'LEFTMOUSE', "value": 'PRESS'}, None)]


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


# ------------------------------------------------------------------------------
# Window, Screen, Areas, Regions

def km_window(params):
    items = []
    keymap = (
        "Window",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        # File operations
        ("wm.read_homefile", {"type": 'N', "value": 'PRESS', "ctrl": True}, None),
        op_menu("TOPBAR_MT_file_open_recent", {"type": 'O', "value": 'PRESS', "shift": True, "ctrl": True}),
        ("wm.open_mainfile", {"type": 'O', "value": 'PRESS', "ctrl": True}, None),
        ("wm.save_mainfile", {"type": 'S', "value": 'PRESS', "ctrl": True}, None),
        ("wm.save_as_mainfile", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("wm.quit_blender", {"type": 'Q', "value": 'PRESS', "ctrl": True}, None),

        # Quick menu and toolbar
        op_menu("SCREEN_MT_user_menu", {"type": 'TAB', "value": 'PRESS', "shift": True}),

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

    return keymap


def km_screen(params):
    items = []
    keymap = (
        "Screen",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("screen.repeat_last", {"type": 'G', "value": 'PRESS'}, None),
        # Animation
        ("screen.userpref_show", {"type": 'COMMA', "value": 'PRESS', "ctrl": True}, None),
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
        # Files
        ("file.execute", {"type": 'RET', "value": 'PRESS'}, None),
        ("file.execute", {"type": 'NUMPAD_ENTER', "value": 'PRESS'}, None),
        ("file.cancel", {"type": 'ESC', "value": 'PRESS'}, None),
        # Undo
        ("ed.undo", {"type": 'Z', "value": 'PRESS', "ctrl": True}, None),
        ("ed.redo", {"type": 'Z', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("ed.undo_history", {"type": 'Z', "value": 'PRESS', "alt": True, "ctrl": True}, None),
        # Render
        ("render.view_cancel", {"type": 'ESC', "value": 'PRESS'}, None),
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
        # Render
        ("render.render", {"type": 'RET', "value": 'PRESS', "ctrl": True},
         {"properties": [("use_viewport", True)]}),
        ("render.render", {"type": 'RET', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("animation", True), ("use_viewport", True)]}),
        ("render.view_cancel", {"type": 'ESC', "value": 'PRESS'}, None),
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


def km_view2d(params):
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
        ("view2d.pan", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.zoom", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True}, None),
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
        ("view2d.zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("view2d.zoom_border", {"type": 'Z', "value": 'PRESS'}, None),
    ])

    return keymap


def km_view2d_buttons_list(params):
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
        ("view2d.pan", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.pan", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("view2d.scroll_down", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_up", {"type": 'WHEELUPMOUSE', "value": 'PRESS'}, None),
        ("view2d.scroll_down", {"type": 'PAGE_DOWN', "value": 'PRESS'},
         {"properties": [("page", True)]}),
        ("view2d.scroll_up", {"type": 'PAGE_UP', "value": 'PRESS'},
         {"properties": [("page", True)]}),
        # Zoom
        ("view2d.zoom", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view2d.zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("view2d.zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("view2d.zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS'}, None),
        ("view2d.zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS'}, None),
        ("view2d.reset", {"type": 'A', "value": 'PRESS'}, None),
    ])

    return keymap


def km_user_interface(params):
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
        # rames and drivers
        ("anim.keyframe_insert_button", {"type": 'S', "value": 'PRESS'}, None),
        ("anim.keyframe_delete_button", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("anim.keyframe_clear_button", {"type": 'S', "value": 'PRESS', "shift": True, "alt": True}, None),
        ("anim.driver_button_add", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("anim.driver_button_remove", {"type": 'D', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("anim.keyingset_button_add", {"type": 'K', "value": 'PRESS'}, None),
        ("anim.keyingset_button_remove", {"type": 'K', "value": 'PRESS', "alt": True}, None),
    ])

    return keymap


# ------------------------------------------------------------------------------
# Editors


def km_property_editor(params):
    items = []
    keymap = (
        "Property Editor",
        {"space_type": 'PROPERTIES', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
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
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("outliner.highlight_update", {"type": 'MOUSEMOVE', "value": 'ANY', "any": True}, None),
        ("outliner.item_rename", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK'},
         {"properties": [("extend", False), ("recursive", False), ("deselect_all", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True},
         {"properties": [("extend", True), ("recursive", False)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True},
         {"properties": [("extend", False), ("recursive", True)]}),
        ("outliner.item_activate", {"type": 'LEFTMOUSE', "value": 'CLICK', "shift": True, "ctrl": True},
         {"properties": [("extend", True), ("recursive", True)]}),
        ("outliner.select_box", {"type": 'Q', "value": 'PRESS'}, None),
        ("outliner.item_openclose", {"type": 'RIGHT_ARROW', "value": 'PRESS'},
         {"properties": [("all", False)]}),
        ("outliner.item_openclose", {"type": 'LEFT_ARROW', "value": 'PRESS'},
         {"properties": [("all", True)]}),
        ("outliner.item_rename", {"type": 'RET', "value": 'PRESS'}, None),
        ("outliner.operation", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
        ("outliner.item_drag_drop", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("outliner.item_drag_drop", {"type": 'EVT_TWEAK_L', "value": 'ANY', "shift": True}, None),
        ("outliner.show_hierarchy", {"type": 'A', "value": 'PRESS'}, None),
        ("outliner.show_active", {"type": 'PERIOD', "value": 'PRESS'}, None),
        ("outliner.show_active", {"type": 'F', "value": 'PRESS'}, None),
        ("outliner.scroll_page", {"type": 'PAGE_DOWN', "value": 'PRESS'},
         {"properties": [("up", False)]}),
        ("outliner.scroll_page", {"type": 'PAGE_UP', "value": 'PRESS'},
         {"properties": [("up", True)]}),
        ("outliner.show_one_level", {"type": 'NUMPAD_PLUS', "value": 'PRESS'}, None),
        ("outliner.show_one_level", {"type": 'NUMPAD_MINUS', "value": 'PRESS'},
         {"properties": [("open", False)]}),
        ("outliner.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("outliner.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("outliner.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("outliner.keyingset_add_selected", {"type": 'K', "value": 'PRESS'}, None),
        ("outliner.keyingset_remove_selected", {"type": 'K', "value": 'PRESS', "alt": True}, None),
        ("anim.keyframe_insert", {"type": 'S', "value": 'PRESS'}, None),
        ("anim.keyframe_delete", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("outliner.drivers_add_selected", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("outliner.drivers_delete_selected", {"type": 'D', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("outliner.collection_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("outliner.collection_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("outliner.object_operation", {"type": 'BACK_SPACE', "value": 'PRESS'},  {"properties": [("type", 'DELETE')]}),
        ("outliner.object_operation", {"type": 'DEL', "value": 'PRESS'},  {"properties": [("type", 'DELETE')]}),
        ("object.move_to_collection", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("object.link_to_collection", {"type": 'M', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("outliner.collection_exclude_set", {"type": 'E', "value": 'PRESS'}, None),
        ("outliner.collection_exclude_clear", {"type": 'E', "value": 'PRESS', "alt": True}, None),
        ("outliner.hide", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
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
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        # Selection modes.
        ("uv.select", {"type": 'LEFTMOUSE', "value": 'CLICK'},
         {"properties": [("extend", False), ("deselect_all", True)]}),
        ("uv.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True), ("deselect_all", False)]}),
        ("uv.select_loop", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "shift": True},
         {"properties": [("extend", True)]}),
        ("uv.select_loop", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'},
         {"properties": [("extend", False)]}),
        ("uv.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS'}, None),
        ("uv.select_more", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("uv.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
        ("uv.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("uv.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("uv.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("uv.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("uv.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("uv.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        op_menu_pie("IMAGE_MT_uvs_snap_pie", {"type": 'S', "value": 'PRESS', "shift": True}),
        op_menu("IMAGE_MT_uvs_select_mode", {"type": 'TAB', "value": 'PRESS', "ctrl": True}),
        ("transform.translate", {"type": 'W', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("transform.rotate", {"type": 'E', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'R', "value": 'PRESS'}, None),
        op_menu("IMAGE_MT_uvs_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit')]}),
        # Tools
        op_tool("builtin.select_box", {"type": 'Q', "value": 'PRESS'}),
        op_tool("builtin.transform", {"type": 'T', "value": 'PRESS'}),
        op_tool("builtin.cursor", {"type": 'C', "value": 'PRESS'}),
        op_tool("builtin.annotate", {"type": 'D', "value": 'PRESS'}),
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
        ("wm.context_toggle", {"type": 'LEFT_BRACKET', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.show_region_toolbar')]}),
        ("wm.context_toggle", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.show_region_ui')]}),
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

    items.extend([
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        # Visibility.
        ("view3d.localview", {"type": 'I', "value": 'PRESS', "shift": True}, None),
        op_menu_pie("VIEW3D_MT_view_pie", {"type": 'V', "value": 'PRESS'}),
        # Navigation.
        ("view3d.rotate", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view3d.move", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view3d.zoom", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("view3d.view_selected", {"type": 'F', "value": 'PRESS', "shift": True},
         {"properties": [("use_all_regions", True)]}),
        ("view3d.view_selected", {"type": 'F', "value": 'PRESS'},
         {"properties": [("use_all_regions", False)]}),
        ("view3d.smoothview", {"type": 'TIMER1', "value": 'ANY', "any": True}, None),
        # Trackpad
        ("view3d.rotate", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("view3d.rotate", {"type": 'MOUSEROTATE', "value": 'ANY'}, None),
        ("view3d.move", {"type": 'TRACKPADPAN', "value": 'ANY', "shift": True}, None),
        ("view3d.zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("view3d.zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        # Numpad
        ("view3d.zoom", {"type": 'NUMPAD_PLUS', "value": 'PRESS'},
         {"properties": [("delta", 1)]}),
        ("view3d.zoom", {"type": 'NUMPAD_MINUS', "value": 'PRESS'},
         {"properties": [("delta", -1)]}),
        ("view3d.zoom", {"type": 'WHEELINMOUSE', "value": 'PRESS'},
         {"properties": [("delta", 1)]}),
        ("view3d.zoom", {"type": 'WHEELOUTMOUSE', "value": 'PRESS'},
         {"properties": [("delta", -1)]}),
        ("view3d.dolly", {"type": 'NUMPAD_PLUS', "value": 'PRESS', "shift": True},
         {"properties": [("delta", 1)]}),
        ("view3d.dolly", {"type": 'NUMPAD_MINUS', "value": 'PRESS', "shift": True},
         {"properties": [("delta", -1)]}),
        ("view3d.view_all", {"type": 'A', "value": 'PRESS'},
         {"properties": [("center", False)]}),
        ("view3d.view_all", {"type": 'A', "value": 'PRESS', "shift": True},
         {"properties": [("use_all_regions", True), ("center", False)]}),
        # Numpad views.
        ("view3d.view_camera", {"type": 'F4', "value": 'PRESS'}, None),
        ("view3d.view_axis", {"type": 'F1', "value": 'PRESS'},
         {"properties": [("type", 'FRONT')]}),
        ("view3d.view_axis", {"type": 'F2', "value": 'PRESS'},
         {"properties": [("type", 'RIGHT')]}),
        ("view3d.view_axis", {"type": 'F3', "value": 'PRESS'},
         {"properties": [("type", 'TOP')]}),
        ("view3d.view_axis", {"type": 'F1', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'BACK')]}),
        ("view3d.view_axis", {"type": 'F2', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'LEFT')]}),
        ("view3d.view_axis", {"type": 'F3', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'BOTTOM')]}),
        ("view3d.view_orbit", {"type": 'F5', "value": 'PRESS'},
         {"properties": [("angle", 3.1415927), ("type", 'ORBITRIGHT')]}),
        # NDOF
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
           {"type": 'LEFTMOUSE', "value": 'CLICK', **{m: True for m in mods}},
           {"properties": [(c, True) for c in props]},
        ) for operator, props, mods in (
            ("view3d.select", ("deselect_all",), ()),
            ("view3d.select", ("toggle",), ("shift",)),
            ("view3d.select", ("center", "object"), ("ctrl",)),
            ("view3d.select", ("extend", "toggle", "center"), ("shift", "ctrl")),
            ("view3d.select", ("toggle", "enumerate"), ("shift", "alt")),
            ("view3d.select", ("toggle", "center", "enumerate"), ("shift", "ctrl", "alt")),
        )),
        ("view3d.zoom_border", {"type": 'Z', "value": 'PRESS'}, None),
        # Copy/paste.
        ("view3d.copybuffer", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("view3d.pastebuffer", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        # Menus.
        op_menu_pie("VIEW3D_MT_snap_pie", {"type": 'V', "value": 'PRESS'}),
        # Transform.
        ("transform.translate", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        op_menu_pie("VIEW3D_MT_pivot_pie", {"type": 'PERIOD', "value": 'PRESS'}),
        op_menu_pie("VIEW3D_MT_orientations_pie", {"type": 'COMMA', "value": 'PRESS'}),
        ("view3d.toggle_xray", {"type": 'X', "value": 'PRESS', "alt": True}, None),

    ])

    return keymap


def km_mask_editing(params):
    items = []
    keymap = (
        "Mask Editing",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("mask.new", {"type": 'N', "value": 'PRESS', "alt": True}, None),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit_mask')]}),
        ("mask.add_vertex_slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("mask.add_feather_vertex_slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("mask.delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("mask.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("mask.select", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("extend", False), ("deselect", False), ("toggle", False), ("deselect_all", True)]}),
        ("mask.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", False), ("deselect", False), ("toggle", True)]}),
        ("mask.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("mask.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("mask.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        #*_template_items_select_actions(params, "mask.select_all"),
        ("mask.select_linked", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("mask.select_linked_pick", {"type": 'L', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("mask.select_linked_pick", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("mask.select_box", {"type": 'Q', "value": 'PRESS'}, None),
        ("mask.select_circle", {"type": 'C', "value": 'PRESS'}, None),
        ("mask.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ADD')]}),
        ("mask.select_lasso", {"type": params.action_tweak, "value": 'ANY', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("mode", 'SUB')]}),
        ("mask.select_more", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("mask.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
        ("mask.hide_view_clear", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("mask.hide_view_set", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("mask.hide_view_set", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("clip.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("extend", False)]}),
        ("mask.cyclic_toggle", {"type": 'C', "value": 'PRESS', "alt": True}, None),
        ("mask.slide_point", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("mask.slide_spline_curvature", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("mask.handle_type_set", {"type": 'V', "value": 'PRESS'}, None),
        ("mask.parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        ("mask.parent_clear", {"type": 'P', "value": 'PRESS', "shift": True}, None),
        ("mask.shape_key_insert", {"type": 'I', "value": 'PRESS'}, None),
        ("mask.shape_key_clear", {"type": 'I', "value": 'PRESS', "alt": True}, None),
        ("mask.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("mask.copy_splines", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("mask.paste_splines", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("transform.translate", {"type": 'W', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("transform.resize", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.rotate", {"type": 'E', "value": 'PRESS'}, None),

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
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.move", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("marker.duplicate", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("marker.select", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("marker.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("marker.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("extend", False), ("camera", True)]}),
        ("marker.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("extend", True), ("camera", True)]}),
        ("marker.select_box", {"type": 'Q', "value": 'PRESS'}, None),
        ("marker.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("marker.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("marker.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("marker.delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("marker.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("marker.rename", {"type": 'RET', "value": 'PRESS'}, None),
        ("marker.move", {"type": 'W', "value": 'PRESS'}, None),
    ])

    return keymap


def km_graph_editor_generic(params):
    items = []
    keymap = (
        "Graph Editor Generic",
        {"space_type": 'GRAPH_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("anim.channels_find", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("graph.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("graph.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("graph.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
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
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        *_template_items_animation(),
        ("graph.cursor_set", {"type": 'RIGHTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("graph.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("extend", False), ("deselect_all", True), ("column", False), ("curves", False)]}),
        ("graph.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("extend", False), ("column", True), ("curves", False)]}),
        ("graph.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True), ("column", False), ("curves", False)]}),
        ("graph.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("extend", True), ("column", True), ("curves", False)]}),
        ("graph.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("extend", False), ("column", False), ("curves", True)]}),
        ("graph.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("extend", True), ("column", False), ("curves", True)]}),
        ("graph.select_leftright", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("mode", 'CHECK'), ("extend", True)]}),
        ("graph.select_leftright", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'LEFT'), ("extend", False)]}),
        ("graph.select_leftright", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'RIGHT'), ("extend", False)]}),
        ("graph.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("graph.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("graph.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("graph.select_box", {"type": 'Q', "value": 'PRESS'},
         {"properties": [("axis_range", False), ("include_handles", False)]}),
        ("graph.select_box", {"type": 'Q', "value": 'PRESS', "alt": True},
         {"properties": [("axis_range", True), ("include_handles", False)]}),
        ("graph.select_box", {"type": 'Q', "value": 'PRESS', "ctrl": True},
         {"properties": [("axis_range", False), ("include_handles", True)]}),
        ("graph.select_box", {"type": 'Q', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("axis_range", True), ("include_handles", True)]}),
        ("graph.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties":[("tweak", True), ("axis_range", False), ("include_handles", False), ("mode", 'SET')]}),
        ("graph.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY', "shift": True},
         {"properties":[("tweak", True), ("axis_range", False), ("include_handles", False), ("mode", 'ADD')]}),
        ("graph.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY', "ctrl": True},
         {"properties":[("tweak", True), ("axis_range", False),("include_handles", False), ("mode", 'SUB')]}),
        ("graph.select_more", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("graph.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
        ("graph.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS'}, None),
        op_menu("GRAPH_MT_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}),
        op_menu("GRAPH_MT_delete", {"type": 'DEL', "value": 'PRESS'}),
        op_menu("GRAPH_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("graph.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("graph.keyframe_insert", {"type": 'S', "value": 'PRESS'}, None),
        ("graph.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("graph.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("graph.paste", {"type": 'V', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("flipped", True)]}),
        ("graph.previewrange_set", {"type": 'P', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("graph.view_all", {"type": 'A', "value": 'PRESS'}, None),
        ("graph.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("graph.view_selected", {"type": 'F', "value": 'PRESS'}, None),
        ("graph.view_frame", {"type": 'A', "value": 'PRESS', "shift": True}, None),
        ("anim.channels_editable_toggle", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("transform.translate", {"type": 'W', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("transform.translate", {"type": 'EVT_TWEAK_M', "value": 'ANY'}, None),
        ("transform.transform", {"type": 'Y', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        ("transform.rotate", {"type": 'E', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'R', "value": 'PRESS'}, None),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_fcurve')]}),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.rename", {"type": 'RET', "value": 'PRESS'}, None),
    ])

    return keymap


def km_image_generic(params):
    items = []
    keymap = (
        "Image Generic",
        {"space_type": 'IMAGE_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("image.new", {"type": 'N', "value": 'PRESS', "alt": True}, None),
        ("image.open", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("image.reload", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("image.read_viewlayers", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("image.save", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("image.save_as", {"type": 'S', "value": 'PRESS', "shift": True}, None),
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
        ("image.view_all", {"type": 'A', "value": 'PRESS'}, None),
        ("image.view_selected", {"type": 'F', "value": 'PRESS'}, None),
        ("image.view_pan", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("image.view_pan", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("image.view_pan", {"type": 'TRACKPADPAN', "value": 'ANY'}, None),
        ("image.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("image.view_ndof", {"type": 'NDOF_MOTION', "value": 'ANY'}, None),
        ("image.view_zoom_in", {"type": 'WHEELINMOUSE', "value": 'PRESS'}, None),
        ("image.view_zoom_out", {"type": 'WHEELOUTMOUSE', "value": 'PRESS'}, None),
        ("image.view_zoom_in", {"type": 'NUMPAD_PLUS', "value": 'PRESS'}, None),
        ("image.view_zoom_out", {"type": 'NUMPAD_MINUS', "value": 'PRESS'}, None),
        ("image.view_zoom", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("image.view_zoom", {"type": 'TRACKPADZOOM', "value": 'ANY'}, None),
        ("image.view_zoom", {"type": 'TRACKPADPAN', "value": 'ANY', "ctrl": True}, None),
        ("image.view_zoom_border", {"type": 'Z', "value": 'PRESS'}, None),
        ("image.view_zoom_ratio", {"type": 'F4', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 8.0)]}),
        ("image.view_zoom_ratio", {"type": 'F3', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 4.0)]}),
        ("image.view_zoom_ratio", {"type": 'F2', "value": 'PRESS', "ctrl": True},
         {"properties": [("ratio", 2.0)]}),
        ("image.view_zoom_ratio", {"type": 'F4', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 8.0)]}),
        ("image.view_zoom_ratio", {"type": 'F3', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 4.0)]}),
        ("image.view_zoom_ratio", {"type": 'F2', "value": 'PRESS', "shift": True},
         {"properties": [("ratio", 2.0)]}),
        ("image.view_zoom_ratio", {"type": 'F1', "value": 'PRESS'},
         {"properties": [("ratio", 1.0)]}),
        ("image.view_zoom_ratio", {"type": 'F2', "value": 'PRESS'},
         {"properties": [("ratio", 0.5)]}),
        ("image.view_zoom_ratio", {"type": 'F3', "value": 'PRESS'},
         {"properties": [("ratio", 0.25)]}),
        ("image.view_zoom_ratio", {"type": 'F4', "value": 'PRESS'},
         {"properties": [("ratio", 0.125)]}),
        ("image.change_frame", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("image.sample", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("image.curves_point_set", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("point", 'BLACK_POINT')]}),
        ("image.curves_point_set", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("point", 'WHITE_POINT')]}),
        ("object.mode_set", {"type": 'ONE', "value": 'PRESS'},
         {"properties": [("mode", 'EDIT')]}),
        ("object.mode_set", {"type": 'TWO', "value": 'PRESS'},
         {"properties": [("mode", 'OBJECT')]}),
        ("object.mode_set", {"type": 'THREE', "value": 'PRESS'},
         {"properties": [("mode", 'OBJECT')]}),
        ("object.mode_set", {"type": 'FOUR', "value": 'PRESS'},
         {"properties": [("mode", 'OBJECT')]}),
        op_menu_pie("IMAGE_MT_pivot_pie", {"type": 'PERIOD', "value": 'PRESS'}),
        # Tools
        op_tool("builtin.select_box", {"type": 'Q', "value": 'PRESS'}),
        op_tool("builtin.transform", {"type": 'W', "value": 'PRESS'}),
        op_tool("builtin.transform", {"type": 'E', "value": 'PRESS'}),
        op_tool("builtin.transform", {"type": 'R', "value": 'PRESS'}),
        op_tool("builtin.cursor", {"type": 'C', "value": 'PRESS'}),
        op_tool("builtin.annotate", {"type": 'D', "value": 'PRESS'}),

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
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("node.add_search", {"type": 'TAB', "value": 'PRESS'}, None),
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

    items.extend(node_select_ops('LEFTMOUSE'))

    items.extend([
        ("node.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties": [("tweak", True)]}),
        ("node.select_lasso", {"type": 'EVT_TWEAK_L', "value": 'ANY', "ctrl": True, "alt": True},
         {"properties": [("mode", 'ADD')]}),
        ("node.select_lasso", {"type": 'EVT_TWEAK_L', "value": 'ANY', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("mode", 'SUB')]}),
        ("node.link", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties": [("detach", False)]}),
        ("node.link", {"type": 'EVT_TWEAK_L', "value": 'ANY', "ctrl": True},
         {"properties": [("detach", True)]}),
        ("node.resize", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("node.add_reroute", {"type": params.action_tweak, "value": 'ANY', "shift": True}, None),
        ("node.links_cut", {"type": params.action_tweak, "value": 'ANY', "ctrl": True}, None),
        ("node.select_link_viewer", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("node.backimage_fit", {"type": 'A', "value": 'PRESS', "alt": True}, None),
        ("node.backimage_sample", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
        op_menu("NODE_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("node.link_make", {"type": 'L', "value": 'PRESS'},
         {"properties": [("replace", False)]}),
        ("node.link_make", {"type": 'L', "value": 'PRESS', "shift": True},
         {"properties": [("replace", True)]}),
        ("node.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("node.parent_set", {"type": 'P', "value": 'PRESS'}, None),
        ("node.join", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        ("node.hide_toggle", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
        ("node.mute_toggle", {"type": 'M', "value": 'PRESS'}, None),
        ("node.preview_toggle", {"type": 'H', "value": 'PRESS', "shift": True}, None),
        ("node.hide_socket_toggle", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
        ("node.view_all", {"type": 'A', "value": 'PRESS'}, None),
        ("node.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("node.view_selected", {"type": 'F', "value": 'PRESS'}, None),
        ("node.select_box", {"type": 'Q', "value": 'PRESS'},
         {"properties": [("tweak", False)]}),
        ("node.delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("node.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("node.delete_reconnect", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True}, None),
        ("node.delete_reconnect", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        ("node.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("node.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("node.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("node.select_linked_to", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True}, None),
        ("node.select_linked_from", {"type": 'RIGHT_BRACKET', "value": 'PRESS'}, None),
        ("node.select_same_type_step", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("prev", False)]}),
        ("node.select_same_type_step", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("prev", True)]}),
        ("node.find_node", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("node.group_make", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("node.group_ungroup", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("node.group_edit", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'},
         {"properties": [("exit", False)]}),
        ("node.group_edit", {"type": 'ESC', "value": 'PRESS'},
         {"properties": [("exit", True)]}),
        ("node.clipboard_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("node.clipboard_paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("node.viewer_border", {"type": 'Z', "value": 'PRESS'}, None),
        ("node.clear_viewer_border", {"type": 'Z', "value": 'PRESS', "alt": True}, None),
        ("node.translate_attach", {"type": 'W', "value": 'PRESS'}, None),
        ("node.translate_attach", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("node.translate_attach", {"type": 'EVT_TWEAK_M', "value": 'ANY'}, None),
        ("transform.translate", {"type": 'W', "value": 'PRESS'},
         {"properties": [("release_confirm", True)]}),
        ("transform.translate", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties": [("release_confirm", True)]}),
        ("transform.rotate", {"type": 'E', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'R', "value": 'PRESS'}, None),
        ("node.move_detach_links_release", {"type": params.action_tweak, "value": 'ANY', "alt": True}, None),
        ("node.move_detach_links", {"type": 'EVT_TWEAK_L', "value": 'ANY', "alt": True}, None),
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
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("info.select_pick", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("info.select_pick", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("info.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties": [("wait_for_input", False)]}),
        ("info.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True},
         {"properties": [("action", 'SELECT')]}),
        ("info.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True},
         {"properties": [("action", 'DESELECT')]}),
        ("info.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True},
         {"properties": [("action", 'INVERT')]}),
        ("info.select_box", {"type": 'Q', "value": 'PRESS'}, None),
        ("info.report_replay", {"type": 'R', "value": 'PRESS'}, None),
        ("info.report_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("info.report_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("info.report_copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_file_browser(params):
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
        ("file.refresh", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
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
        ("file.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("file.select_box", {"type": 'Q', "value": 'PRESS'}, None),
        ("file.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("file.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY', "shift": True},
         {"properties": [("mode", 'ADD')]}),
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


def km_file_browser_buttons(params):
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


def km_dopesheet_generic(params):
    items = []
    keymap = (
        "Dopesheet Generic",
        {"space_type": 'DOPESHEET_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("wm.context_toggle", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.show_region_ui')]}),
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
        *_template_items_animation(),
        ("action.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("extend", False), ("deselect_all", True), ("column", False), ("channel", False)]}),
        ("action.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("extend", False), ("column", True), ("channel", False)]}),
        ("action.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True), ("column", False), ("channel", False)]}),
        ("action.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("extend", True), ("column", True), ("channel", False)]}),
        ("action.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("extend", False), ("column", False), ("channel", True)]}),
        ("action.clickselect", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True},
         {"properties": [("extend", True), ("column", False), ("channel", True)]}),
        ("action.select_leftright", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("mode", 'CHECK'), ("extend", True)]}),
        ("action.select_leftright", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'LEFT'), ("extend", False)]}),
        ("action.select_leftright", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'RIGHT'), ("extend", False)]}),
        ("action.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("action.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("action.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("action.select_box", {"type": 'Q', "value": 'PRESS'},
         {"properties": [("axis_range", False)]}),
        ("action.select_box", {"type": 'Q', "value": 'PRESS', "alt": True},
         {"properties": [("axis_range", True)]}),
        ("action.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties":[("tweak", True), ("axis_range", False), ("wait_for_input", False), ("mode", 'SET')]}),
        ("action.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY', "shift": True},
         {"properties":[("tweak", True), ("axis_range", False), ("wait_for_input", False), ("mode", 'ADD')]}),
        ("action.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY', "ctrl": True},
         {"properties":[("tweak", True), ("axis_range", False), ("wait_for_input", False), ("mode", 'SUB')]}),
        ("action.select_column", {"type": 'K', "value": 'PRESS'},
         {"properties": [("mode", 'KEYS')]}),
        ("action.select_column", {"type": 'K', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 'CFRA')]}),
        ("action.select_column", {"type": 'K', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'MARKERS_COLUMN')]}),
        ("action.select_column", {"type": 'K', "value": 'PRESS', "alt": True},
         {"properties": [("mode", 'MARKERS_BETWEEN')]}),
        ("action.select_more", {"type": 'UP_ARROW', "value": 'PRESS', "ctrl": True}, None),
        ("action.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS', "ctrl": True}, None),
        ("action.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS'}, None),
        ("action.frame_jump", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        op_menu("DOPESHEET_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        op_menu("DOPESHEET_MT_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}),
        op_menu("DOPESHEET_MT_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("action.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("action.keyframe_insert", {"type": 'S', "value": 'PRESS'}, None),
        ("action.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("action.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("action.paste", {"type": 'V', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("flipped", True)]}),
        ("action.previewrange_set", {"type": 'P', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("action.view_all", {"type": 'A', "value": 'PRESS'}, None),
        ("action.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("action.view_selected", {"type": 'F', "value": 'PRESS'}, None),
        ("action.view_frame", {"type": 'NUMPAD_0', "value": 'PRESS'}, None),
        ("anim.channels_editable_toggle", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("anim.channels_find", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("transform.transform", {"type": 'W', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_TRANSLATE')]}),
        ("transform.transform", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties": [("mode", 'TIME_TRANSLATE')]}),
        ("transform.transform", {"type": 'EVT_TWEAK_M', "value": 'ANY'},
         {"properties": [("mode", 'TIME_TRANSLATE')]}),
        ("transform.transform", {"type": 'E', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        ("transform.transform", {"type": 'R', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_SCALE')]}),
        ("transform.transform", {"type": 'T', "value": 'PRESS', "shift": True},
         {"properties": [("mode", 'TIME_SLIDE')]}),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_action')]}),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.rename", {"type": 'RET', "value": 'PRESS'}, None),
        ("anim.start_frame_set", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True}, None),
        ("anim.end_frame_set", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True}, None),
    ])

    return keymap


def km_nla_generic(params):
    items = []
    keymap = (
        "NLA Generic",
        {"space_type": 'NLA_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        *_template_items_animation(),
        ("nla.tweakmode_enter", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("nla.tweakmode_exit", {"type": 'ESC', "value": 'PRESS'}, None),
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
        ("nla.tracks_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("nla.tracks_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        op_menu("NLA_MT_channel_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
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
        ("nla.click_select", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("extend", False), ("deselect_all", True)]}),
        ("nla.click_select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("nla.select_leftright", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("mode", 'CHECK'), ("extend", True)]}),
        ("nla.select_leftright", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'LEFT'), ("extend", False)]}),
        ("nla.select_leftright", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("mode", 'RIGHT'), ("extend", False)]}),
        ("nla.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("nla.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("nla.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("nla.select_box", {"type": 'Q', "value": 'PRESS'},
         {"properties": [("axis_range", False)]}),
        ("nla.select_box", {"type": 'Q', "value": 'PRESS', "alt": True},
         {"properties": [("axis_range", True)]}),
        ("nla.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties":[("tweak", True), ("mode", 'SET')]}),
        ("nla.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY', "shift": True},
         {"properties":[("tweak", True), ("mode", 'ADD')]}),
        ("nla.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY', "ctrl": True},
         {"properties":[("tweak", True), ("mode", 'SUB')]}),
        ("nla.view_all", {"type": 'A', "value": 'PRESS'}, None),
        ("nla.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("nla.view_selected", {"type": 'F', "value": 'PRESS'}, None),
        ("nla.view_frame", {"type": 'NUMPAD_0', "value": 'PRESS'}, None),
        ("nla.meta_add", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("nla.meta_remove", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("nla.duplicate", {"type": 'D', "value": 'PRESS', "ctrl": True},
         {"properties": [("linked", False)]}),
        ("nla.duplicate", {"type": 'D', "value": 'PRESS', "ctrl": True, "alt": True},
         {"properties": [("linked", True)]}),
        ("nla.make_single_user", {"type": 'U', "value": 'PRESS'}, None),
        ("nla.delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("nla.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("nla.mute_toggle", {"type": 'M', "value": 'PRESS'}, None),
        ("nla.move_up", {"type": 'PAGE_UP', "value": 'PRESS'}, None),
        ("nla.move_down", {"type": 'PAGE_DOWN', "value": 'PRESS'}, None),
        ("transform.transform", {"type": 'W', "value": 'PRESS'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("transform.transform", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("transform.transform", {"type": 'EVT_TWEAK_M', "value": 'ANY'},
         {"properties": [("mode", 'TRANSLATION')]}),
        ("transform.transform", {"type": 'E', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        ("transform.transform", {"type": 'R', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_SCALE')]}),
        op_menu("NLA_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.rename", {"type": 'RET', "value": 'PRESS'}, None),
    ])

    return keymap


def km_text_generic(params):
    items = []
    keymap = (
        "Text Generic",
        {"space_type": 'TEXT_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        ("text.start_find", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        ("text.jump", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        ("text.find", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("text.replace", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
        ("wm.context_toggle", {"type": 'I', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.show_region_ui')]}),
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
        ("text.move", {"type": 'LEFT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'LINE_BEGIN')]}),
        ("text.move", {"type": 'RIGHT_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'LINE_END')]}),
        ("text.move", {"type": 'UP_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'FILE_TOP')]}),
        ("text.move", {"type": 'DOWN_ARROW', "value": 'PRESS', "ctrl": True},
         {"properties": [("type", 'FILE_BOTTOM')]}),
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
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.context_toggle", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.show_region_ui')]}),
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
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        *_template_items_animation(),
        ("sequencer.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("sequencer.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("sequencer.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("sequencer.cut", {"type": 'K', "value": 'PRESS'},
         {"properties": [("type", 'SOFT')]}),
        ("sequencer.cut", {"type": 'K', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'HARD')]}),
        ("sequencer.mute", {"type": 'M', "value": 'PRESS'},
         {"properties": [("unselected", False)]}),
        ("sequencer.mute", {"type": 'M', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("sequencer.unmute", {"type": 'M', "value": 'PRESS', "alt": True},
         {"properties": [("unselected", False)]}),
        ("sequencer.unmute", {"type": 'M', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("unselected", True)]}),
        ("sequencer.lock", {"type": 'L', "value": 'PRESS', "shift": True}, None),
        ("sequencer.unlock", {"type": 'L', "value": 'PRESS', "shift": True, "alt": True}, None),
        ("sequencer.reassign_inputs", {"type": 'R', "value": 'PRESS'}, None),
        ("sequencer.reload", {"type": 'R', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.reload", {"type": 'R', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("adjust_length", True)]}),
        ("sequencer.offset_clear", {"type": 'O', "value": 'PRESS', "alt": True}, None),
        ("sequencer.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("sequencer.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("sequencer.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.images_separate", {"type": 'Y', "value": 'PRESS'}, None),
        ("sequencer.meta_toggle", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("sequencer.meta_make", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.meta_separate", {"type": 'G', "value": 'PRESS', "ctrl": True, "alt": True}, None),
        ("sequencer.view_all", {"type": 'A', "value": 'PRESS'}, None),
        ("sequencer.view_all", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("sequencer.view_selected", {"type": 'F', "value": 'PRESS'}, None),
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
        ("sequencer.select", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("extend", False), ("deselect_all", True), ("linked_handle", False), ("left_right", 'NONE'), ("linked_time", False)]}),
        ("sequencer.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True), ("linked_handle", False), ("left_right", 'NONE'), ("linked_time", False)]}),
        ("sequencer.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True},
         {"properties": [("extend", False), ("linked_handle", True), ("left_right", 'NONE'), ("linked_time", False)]}),
        ("sequencer.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("extend", True), ("linked_handle", True), ("left_right", 'NONE'), ("linked_time", False)]}),
        ("sequencer.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("extend", False), ("linked_handle", False), ("left_right", 'MOUSE'), ("linked_time", True)]}),
        ("sequencer.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("extend", True), ("linked_handle", False), ("left_right", 'NONE'), ("linked_time", True)]}),
        ("sequencer.select_more", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("sequencer.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
        ("sequencer.select_linked_pick", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("extend", False)]}),
        ("sequencer.select_linked_pick", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("sequencer.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "ctrl": True}, None),
        ("sequencer.select_box", {"type": 'Q', "value": 'PRESS'}, None),
        ("sequencer.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY'},
         {"properties":[("tweak", True), ("mode", 'SET')]}),
        ("sequencer.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY', "shift": True},
         {"properties":[("tweak", True), ("mode", 'ADD')]}),
        ("sequencer.select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY', "ctrl": True},
         {"properties":[("tweak", True), ("mode", 'SUB')]}),
        ("sequencer.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("sequencer.slip", {"type": 'R', "value": 'PRESS'}, None),
        ("wm.context_set_int", {"type": 'O', "value": 'PRESS'},
         {"properties": [("data_path", 'scene.sequence_editor.overlay_frame'), ("value", 0)]}),
        ("transform.seq_slide", {"type": 'W', "value": 'PRESS'}, None),
        ("transform.seq_slide", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("transform.seq_slide", {"type": 'EVT_TWEAK_M', "value": 'ANY'}, None),
        ("transform.transform", {"type": 'E', "value": 'PRESS'},
         {"properties": [("mode", 'TIME_EXTEND')]}),
        op_menu("SEQUENCER_MT_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("marker.add", {"type": 'M', "value": 'PRESS'}, None),
        ("marker.rename", {"type": 'RET', "value": 'PRESS'}, None),
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
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("sequencer.view_all_preview", {"type": 'A', "value": 'PRESS'}, None),
        ("sequencer.view_all_preview", {"type": 'NDOF_BUTTON_FIT', "value": 'PRESS'}, None),
        ("sequencer.view_ghost_border", {"type": 'O', "value": 'PRESS'}, None),
        ("sequencer.view_zoom_ratio", {"type": 'NUMPAD_1', "value": 'PRESS'},
         {"properties": [("ratio", 1.0)]}),
        ("sequencer.sample", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
    ])

    return keymap


def km_console(params):
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


def km_clip(params):
    items = []
    keymap = (
        "Clip",
        {"space_type": 'CLIP_EDITOR', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
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
        ("clip.solve_camera", {"type": 'S', "value": 'PRESS', "shift": True}, None),
        ("clip.prefetch", {"type": 'P', "value": 'PRESS'}, None),
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
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
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
        ("clip.view_all", {"type": 'A', "value": 'PRESS'}, None),
        ("clip.view_selected", {"type": 'F', "value": 'PRESS'}, None),
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
        ("clip.select", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("extend", False), ("deselect_all", True)]}),
        ("clip.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("clip.select_box", {"type": 'Q', "value": 'PRESS'}, None),
        ("clip.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("clip.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("clip.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        op_menu("CLIP_MT_select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}),
        ("clip.add_marker_slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("clip.delete_marker", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True}, None),
        ("clip.delete_marker", {"type": 'DEL', "value": 'PRESS', "shift": True}, None),
        ("clip.slide_marker", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("clip.disable_markers", {"type": 'D', "value": 'PRESS', "shift": True},
         {"properties": [("action", 'TOGGLE')]}),
        ("clip.delete_track", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("clip.delete_track", {"type": 'DEL', "value": 'PRESS'}, None),
        ("clip.lock_tracks", {"type": 'L', "value": 'PRESS', "ctrl": True},
         {"properties": [("action", 'LOCK')]}),
        ("clip.lock_tracks", {"type": 'L', "value": 'PRESS', "alt": True},
         {"properties": [("action", 'UNLOCK')]}),
        ("clip.hide_tracks", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("clip.hide_tracks", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("clip.hide_tracks_clear", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("clip.slide_plane_marker", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("clip.keyframe_insert", {"type": 'S', "value": 'PRESS'}, None),
        ("clip.keyframe_delete", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("clip.join_tracks", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        op_menu("CLIP_MT_tracking_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("wm.context_toggle", {"type": 'L', "value": 'PRESS'},
         {"properties": [("data_path", 'space_data.lock_selection')]}),
        ("wm.context_toggle", {"type": 'D', "value": 'PRESS', "alt": True},
         {"properties": [("data_path", 'space_data.show_disabled')]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "alt": True},
         {"properties": [("data_path", 'space_data.show_marker_search')]}),
        ("wm.context_toggle", {"type": 'M', "value": 'PRESS'},
         {"properties": [("data_path", 'space_data.use_mute_footage')]}),
        ("transform.translate", {"type": 'W', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("transform.resize", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.rotate", {"type": 'E', "value": 'PRESS'}, None),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "alt": True},
         {"properties": [("action", 'REMAINED'), ("clear_active", False)]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "shift": True},
         {"properties": [("action", 'UPTO'), ("clear_active", False)]}),
        ("clip.clear_track_path", {"type": 'T', "value": 'PRESS', "shift": True, "alt": True},
         {"properties": [("action", 'ALL'), ("clear_active", False)]}),
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
        op_panel("TOPBAR_PT_name", {"type": 'RET', "value": 'PRESS'}, [("keep_open", False)]),
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
        ("clip.graph_select", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("extend", False)]}),
        ("clip.graph_select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("clip.graph_select_box", {"type": 'Q', "value": 'PRESS'}, None),
        ("clip.graph_select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        # ("clip.graph_select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'DESELECT')]}),
        # ("clip.graph_select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("clip.graph_delete_curve", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("clip.graph_delete_curve", {"type": 'DEL', "value": 'PRESS'}, None),
        ("clip.graph_delete_knot", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True}, None),
        ("clip.graph_delete_knot", {"type": 'DEL', "value": 'PRESS', "shift": True}, None),
        ("clip.graph_view_all", {"type": 'A', "value": 'PRESS'}, None),
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
        ("transform.translate", {"type": 'W', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("transform.resize", {"type": 'R', "value": 'PRESS'}, None),
        ("transform.rotate", {"type": 'E', "value": 'PRESS'}, None),
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
        ("wm.search_menu", {"type": 'TAB', "value": 'PRESS'}, None),
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
        ("screen.frame_jump", {"type": 'MEDIA_LAST', "value": 'PRESS'},
         {"properties": [("end", True)]}),
        ("screen.frame_jump", {"type": 'MEDIA_FIRST', "value": 'PRESS'},
         {"properties": [("end", False)]}),
        ("screen.animation_play", {"type": 'SPACE', "value": 'PRESS'}, None),
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
        ("wm.context_toggle", {"type": 'T', "value": 'PRESS', "ctrl": True},
         {"properties": [("data_path", 'space_data.show_seconds')]}),
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
        ("anim.channels_rename", {"type": 'RET', "value": 'PRESS'}, None),
        ("anim.channels_rename", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        # Select keys.
        ("anim.channel_select_keys", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        ("anim.channel_select_keys", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "shift": True},
         {"properties": [("extend", True)]}),
        # Find (setting the name filter).
        ("anim.channels_find", {"type": 'F', "value": 'PRESS', "ctrl": True}, None),
        # Selection.
        ("anim.channels_select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("anim.channels_select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("anim.channels_select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("anim.channels_select_box", {"type": 'B', "value": 'PRESS'}, None),
        ("anim.channels_select_box", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        # Delete.
        ("anim.channels_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("anim.channels_delete", {"type": 'DEL', "value": 'PRESS'}, None),
        # Settings.
        ("anim.channels_setting_toggle", {"type": 'W', "value": 'PRESS', "shift": True}, None),
        ("anim.channels_setting_enable", {"type": 'W', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("anim.channels_setting_disable", {"type": 'W', "value": 'PRESS', "alt": True}, None),
        ("anim.channels_editable_toggle", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'}, None),
        # Expand/collapse.
        ("anim.channels_expand", {"type": 'RIGHT_ARROW', "value": 'PRESS'}, None),
        ("anim.channels_collapse", {"type": 'LEFT_ARROW', "value": 'PRESS'}, None),
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
        op_menu("DOPESHEET_MT_channel_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
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
        ("gpencil.active_frames_delete_all", {"type": 'BACK_SPACE', "value": 'PRESS', "key_modifier": 'D'}, None),
        ("gpencil.active_frames_delete_all", {"type": 'DEL', "value": 'PRESS', "key_modifier": 'D'}, None),
    ])

    return keymap


def _grease_pencil_selection(params):
    return [
        # Select all
        ("gpencil.select_box", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
        ("gpencil.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True), ("toggle", True)]}),
        # Select linked
        ("gpencil.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS'}, None),
        # Select alternate
        ("gpencil.select_alternate", {"type": 'L', "value": 'PRESS', "shift": True}, None),
        # Select grouped
        ("gpencil.select_grouped", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        # Select more/less
        ("gpencil.select_more", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("gpencil.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
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
        ("gpencil.select", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("deselect_all", True)]}),
        # Selection
        *_grease_pencil_selection(params),
        # Duplicate and move selected points
        ("gpencil.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        # Extrude and move selected points
        ("gpencil.extrude_move", {"type": 'E', "value": 'PRESS'}, None),
        # Delete
        op_menu("VIEW3D_MT_edit_gpencil_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_gpencil_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("gpencil.dissolve", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True}, None),
        ("gpencil.dissolve", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        ("gpencil.active_frames_delete_all", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True}, None),
        ("gpencil.active_frames_delete_all", {"type": 'DEL', "value": 'PRESS', "shift": True}, None),
        # Context menu
        op_menu("VIEW3D_MT_gpencil_edit_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        # Separate
        op_menu("GPENCIL_MT_separate", {"type": 'P', "value": 'PRESS'}),
        # Split and joint strokes
        ("gpencil.stroke_join", {"type": 'J', "value": 'PRESS', "ctrl": True}, None),
        ("gpencil.stroke_join", {"type": 'J', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("type", 'JOINCOPY')]}),
        # Copy + paset
        ("gpencil.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("gpencil.paste", {"type": 'V', "value": 'PRESS', "ctrl": True}, None),
        # Snap
        op_menu("GPENCIL_MT_snap", {"type": 'S', "value": 'PRESS', "shift": True}),
        # Show/hide
        ("gpencil.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("gpencil.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("gpencil.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("gpencil.selection_opacity_toggle", {"type": 'H', "value": 'PRESS', "ctrl": True}, None),
        # Display
        *_grease_pencil_display(),
        # Isolate layer
        ("gpencil.layer_isolate", {"type": 'NUMPAD_ASTERIX', "value": 'PRESS'}, None),
        # Move to layer
        ("gpencil.move_to_layer", {"type": 'G', "value": 'PRESS'}, None),
        # Transform tools
        ("transform.translate", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit')]}),
        # Vertex group menu
        op_menu("GPENCIL_MT_gpencil_vertex_group", {"type": 'G', "value": 'PRESS', "ctrl": True}),
        # Select mode
        ("gpencil.selectmode_toggle", {"type": 'ONE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 0)]}),
        ("gpencil.selectmode_toggle", {"type": 'TWO', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 1)]}),
        ("gpencil.selectmode_toggle", {"type": 'THREE', "value": 'PRESS', "ctrl": True},
         {"properties": [("mode", 2)]}),
        # Tools
        *_template_items_basic_tools(),
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
        ("wm.radial_control", {"type": 'U', "value": 'PRESS'},
         {"properties": [("data_path_primary", 'tool_settings.gpencil_paint.brush.gpencil_settings.pen_strength')]}),
        # Brush size
        ("wm.radial_control", {"type": 'S', "value": 'PRESS'},
         {"properties": [("data_path_primary", 'tool_settings.gpencil_paint.brush.size')]}),
        # Draw context menu
        op_panel("VIEW3D_PT_gpencil_draw_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        # Draw delete menu
        op_menu("GPENCIL_MT_gpencil_draw_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}),
        op_menu("GPENCIL_MT_gpencil_draw_delete", {"type": 'DEL', "value": 'PRESS'}),
        # Tools
        op_tool("builtin_brush.Draw", {"type": 'D', "value": 'PRESS'}),
        op_tool("builtin_brush.Fill", {"type": 'F', "value": 'PRESS'}),
        op_tool("builtin_brush.Erase", {"type": 'E', "value": 'PRESS'}),
        op_tool("builtin.cutter", {"type": 'K', "value": 'PRESS'}),
        op_tool("builtin.cursor", {"type": 'C', "value": 'PRESS'}),
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
        ("gpencil.draw", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "alt": True},
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
        ("gpencil.draw", {"type": 'B', "value": 'PRESS'}, None),
        # Mode
        ("gpencil.draw", {"type": 'P', "value": 'PRESS'}, None),
        # Set reference point
        ("gpencil.draw", {"type": 'P', "value": 'PRESS', "alt": True}, None),
        # Tablet Mappings for Drawing ------------------ */
        # For now, only support direct drawing using the eraser, as most users using a tablet
        # may still want to use that as their primary pointing device!
        ("gpencil.draw", {"type": 'ERASER', "value": 'PRESS'},
         {"properties": [("mode", 'ERASER'), ("wait_for_input", False)]}),

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


def km_grease_pencil_stroke_paint_fill(params):
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
        ("gpencil.draw", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True, "shift": True},
         {"properties": [("mode", 'DRAW'), ("wait_for_input", False), ("disable_straight", True), ("disable_fill", True)]}),
        # Lasso select
        ("gpencil.select_lasso", {"type": params.action_tweak, "value": 'ANY', "ctrl": True, "alt": True}, None),
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
        # Painting
        ("gpencil.sculpt_paint", {"type": 'LEFTMOUSE', "value": 'PRESS'},
         {"properties": [("wait_for_input", False)]}),
        ("gpencil.sculpt_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True},
         {"properties": [("wait_for_input", False)]}),
        ("gpencil.sculpt_paint", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("wait_for_input", False)]}),
        # Brush strength
        ("wm.radial_control", {"type": 'U', "value": 'PRESS'},
         {"properties": [("data_path_primary", 'tool_settings.gpencil_sculpt.brush.strength')]}),
        # Brush size
        ("wm.radial_control", {"type": 'S', "value": 'PRESS'},
         {"properties": [("data_path_primary", 'tool_settings.gpencil_sculpt.brush.size')]}),
        # Context menu
        op_panel("VIEW3D_PT_gpencil_sculpt_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
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
        ("wm.radial_control", {"type": 'U', "value": 'PRESS', "shift": True},
         {"properties": [("data_path_primary", 'tool_settings.gpencil_sculpt.weight_brush.strength')]}),
        # Brush sze
        ("wm.radial_control", {"type": 'S', "value": 'PRESS'},
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
        ("paint.face_select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("paint.face_select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("paint.face_select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("paint.face_select_hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
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
        ("paint.vert_select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, None),
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
        *_template_items_animation(),
        ("object.parent_set", {"type": 'P', "value": 'PRESS'}, None),
        ("pose.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("pose.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("pose.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("pose.rot_clear", {"type": 'E', "value": 'PRESS', "alt": True}, None),
        ("pose.loc_clear", {"type": 'W', "value": 'PRESS', "alt": True}, None),
        ("pose.scale_clear", {"type": 'R', "value": 'PRESS', "alt": True}, None),
        ("pose.copy", {"type": 'C', "value": 'PRESS', "ctrl": True}, None),
        ("pose.paste", {"type": 'V', "value": 'PRESS', "ctrl": True},
         {"properties": [("flipped", False)]}),
        ("pose.paste", {"type": 'V', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("flipped", True)]}),
        ("pose.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("pose.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("pose.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("pose.select_parent", {"type": 'UP_ARROW', "value": 'PRESS', "ctrl": True}, None),
        ("pose.select_hierarchy", {"type": 'UP_ARROW', "value": 'PRESS'},
         {"properties": [("direction", 'PARENT'), ("extend", False)]}),
        ("pose.select_hierarchy", {"type": 'UP_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'PARENT'), ("extend", True)]}),
        ("pose.select_hierarchy", {"type": 'DOWN_ARROW', "value": 'PRESS'},
         {"properties": [("direction", 'CHILD'), ("extend", False)]}),
        ("pose.select_hierarchy", {"type": 'DOWN_ARROW', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'CHILD'), ("extend", True)]}),
        ("pose.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS'}, None),
        ("pose.bone_layers", {"type": 'G', "value": 'PRESS'}, None),
        ("anim.keyframe_insert_menu", {"type": 'S', "value": 'PRESS', "shift": True}, None),
        ("anim.keyframe_insert_by_name", {"type": 'S', "value": 'PRESS'},
         {"properties": [("type", 'LocRotScale')]}),
        ("anim.keyframe_insert_by_name", {"type": 'W', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Location')]}),
        ("anim.keyframe_insert_by_name", {"type": 'E', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Rotation')]}),
        ("anim.keyframe_insert_by_name", {"type": 'R', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Scaling')]}),

        ("anim.keyframe_delete_v3d", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("anim.keying_set_active_set", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        op_menu("VIEW3D_MT_pose_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        # Tools
        op_tool("builtin.select_box", {"type": 'Q', "value": 'PRESS'}),
        op_tool("builtin.move", {"type": 'W', "value": 'PRESS'}),
        op_tool("builtin.rotate", {"type": 'E', "value": 'PRESS'}),
        op_tool("builtin.scale", {"type": 'R', "value": 'PRESS'}),
        op_tool("builtin.scale_cage", {"type": 'R', "value": 'PRESS', "shift": True}),
        op_tool("builtin.transform", {"type": 'T', "value": 'PRESS'}),
        op_tool("builtin.measure", {"type": 'M', "value": 'PRESS'}),
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
        *_template_items_animation(),
        #Selection
        ("object.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("object.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("object.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("object.select_more", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("object.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
        ("object.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS'}, None),
        ("object.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'PARENT'), ("extend", False)]}),
        ("object.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'PARENT'), ("extend", True)]}),
        ("object.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'CHILD'), ("extend", False)]}),
        ("object.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'CHILD'), ("extend", True)]}),
        ("object.parent_set", {"type": 'P', "value": 'PRESS'}, None),

        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit_objects')]}),
        ("object.parent_clear", {"type": 'P', "value": 'PRESS', "shift": True}, None),
        ("object.location_clear", {"type": 'W', "value": 'PRESS', "alt": True},
         {"properties": [("clear_delta", False)]}),
        ("object.rotation_clear", {"type": 'E', "value": 'PRESS', "alt": True},
         {"properties": [("clear_delta", False)]}),
        ("object.scale_clear", {"type": 'R', "value": 'PRESS', "alt": True},
         {"properties": [("clear_delta", False)]}),
        ("object.delete", {"type": 'BACK_SPACE', "value": 'PRESS'},
         {"properties": [("use_global", False), ("confirm", False)]}),
        ("object.delete", {"type": 'BACK_SPACE', "value": 'PRESS', "shift": True},
         {"properties": [("use_global", True), ("confirm", False)]}),
        ("object.delete", {"type": 'DEL', "value": 'PRESS'},
         {"properties": [("use_global", False), ("confirm", False)]}),
        ("object.delete", {"type": 'DEL', "value": 'PRESS', "shift": True},
         {"properties": [("use_global", True), ("confirm", False)]}),
        ("object.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        # Keyframing
        ("anim.keyframe_insert_menu", {"type": 'S', "value": 'PRESS', "shift": True}, None),
        ("anim.keyframe_insert_by_name", {"type": 'S', "value": 'PRESS'},
         {"properties": [("type", 'LocRotScale')]}),
        ("anim.keyframe_insert_by_name", {"type": 'W', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Location')]}),
        ("anim.keyframe_insert_by_name", {"type": 'E', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Rotation')]}),
        ("anim.keyframe_insert_by_name", {"type": 'R', "value": 'PRESS', "shift": True},
         {"properties": [("type", 'Scaling')]}),
        ("anim.keyframe_delete_v3d", {"type": 'S', "value": 'PRESS', "alt": True}, None),
        ("anim.keying_set_active_set", {"type": 'S', "value": 'PRESS', "shift": True, "ctrl": True, "alt": True}, None),
        op_menu("VIEW3D_MT_object_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("object.move_to_collection", {"type": 'G', "value": 'PRESS', "ctrl": True}, None),
        ("object.link_to_collection", {"type": 'G', "value": 'PRESS', "shift": True, "ctrl": True}, None),
        ("object.hide_view_clear", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("object.hide_view_set", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("object.hide_view_set", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),

        *_template_items_basic_tools(),
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
        ("paintcurve.add_point_slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("paintcurve.select", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("paintcurve.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("extend", True)]}),
        ("paintcurve.slide", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("paintcurve.slide", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True},
         {"properties": [("align", True)]}),
        ("paintcurve.select", {"type": 'A', "value": 'PRESS'},
         {"properties": [("toggle", True)]}),
        ("paintcurve.cursor", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("paintcurve.delete_point", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("paintcurve.delete_point", {"type": 'DEL', "value": 'PRESS'}, None),
        ("paintcurve.draw", {"type": 'RET', "value": 'PRESS'}, None),
        ("paintcurve.draw", {"type": 'NUMPAD_ENTER', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'W', "value": 'PRESS'}, None),
        ("transform.translate", {"type": 'EVT_TWEAK_L', "value": 'ANY'}, None),
        ("transform.rotate", {"type": 'E', "value": 'PRESS'}, None),
        ("transform.resize", {"type": 'R', "value": 'PRESS'}, None),
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
        ("curve.handle_type_set", {"type": 'V', "value": 'PRESS'}, None),
        ("curve.vertex_add", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True}, None),
        ("curve.select_all", {"type": 'A', "value": 'CLICK', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("curve.select_all", {"type": 'A', "value": 'CLICK', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("curve.select_all", {"type": 'I', "value": 'CLICK', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("curve.select_row", {"type": 'R', "value": 'PRESS', "shift": True}, None),
        ("curve.select_more", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("curve.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
        ("curve.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS'}, None),
        ("curve.shortest_path_pick", {"type": 'LEFTMOUSE', "value": 'CLICK', "ctrl": True, "shift": True}, None),
        ("curve.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        op_menu("VIEW3D_MT_edit_curve_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_curve_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("curve.dissolve_verts", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True}, None),
        ("curve.dissolve_verts", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        ("curve.tilt_clear", {"type": 'T', "value": 'PRESS', "alt": True}, None),
        ("curve.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("curve.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("curve.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        op_menu("VIEW3D_MT_edit_curve_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit')]}),
        # Tools
        *_template_items_basic_tools(),
        op_tool("builtin.extrude", {"type": 'E', "value": 'PRESS', "ctrl": True}),
        op_tool("builtin.tilt", {"type": 'Y', "value": 'PRESS'}),
        op_tool("builtin.radius", {"type": 'U', "value": 'PRESS'}),

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
        ("wm.radial_control", {"type": 'S', "value": 'PRESS'},
         radial_control_properties(paint, 'size', 'use_unified_size', secondary_rotation=secondary_rotation, color=color, zoom=zoom)),
        ("wm.radial_control", {"type": 'U', "value": 'PRESS'},
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
        ("paint.grab_clone", {"type": 'MIDDLEMOUSE', "value": 'PRESS'}, None),
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
        op_panel("VIEW3D_PT_paint_texture_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
    ])

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
        op_panel("VIEW3D_PT_paint_vertex_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
    ])

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
        ("paint.weight_sample", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
        ("paint.weight_sample_group", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True}, None),
        ("brush.scale_size", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("scalar", 0.9)]}),
        ("brush.scale_size", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("scalar", 1.0 / 0.9)]}),
        *_template_paint_radial_control("weight_paint"),
        ("wm.context_menu_enum", {"type": 'E', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.vertex_paint.brush.stroke_method')]}),
        ("wm.context_toggle", {"type": 'M', "value": 'PRESS'},
         {"properties": [("data_path", 'weight_paint_object.data.use_paint_mask')]}),
        ("wm.context_toggle", {"type": 'V', "value": 'PRESS'},
         {"properties": [("data_path", 'weight_paint_object.data.use_paint_mask_vertex')]}),
        ("wm.context_toggle", {"type": 'S', "value": 'PRESS', "shift": True},
         {"properties": [("data_path", 'tool_settings.weight_paint.brush.use_smooth_stroke')]}),
        op_panel("VIEW3D_PT_paint_weight_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        # Bone selection for combined weight paint + pose mode.
        ("view3d.select", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True}, None),
    ])

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
        ("paint.hide_show", {"type": 'H', "value": 'PRESS', "ctrl": True},
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
        ("paint.brush_select", {"type": 'M', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'MASK'), ("toggle", True), ("create_missing", True)]}),
        ("paint.brush_select", {"type": 'R', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'ROTATE')]}),
        ("paint.brush_select", {"type": 'N', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'NUDGE')]}),
        ("paint.brush_select", {"type": 'T', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'THUMB')]}),
        ("paint.brush_select", {"type": 'H', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'SNAKE_HOOK')]}),
        ("paint.brush_select", {"type": 'B', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'BLOB')]}),
        ("paint.brush_select", {"type": 'D', "value": 'PRESS'},
         {"properties": [("sculpt_tool", 'DRAW')]}),

        # Menus
        op_panel("VIEW3D_PT_sculpt_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
    ])

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
        # Selection
        ("mesh.loop_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK'},
         {"properties": [("extend", False), ("deselect", False), ("toggle", False), ("ring", False)]}),
        ("mesh.loop_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "shift": True},
         {"properties": [("extend", True), ("deselect", False), ("toggle", False), ("ring", False)]}),
        ("mesh.loop_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "ctrl": True},
         {"properties": [("extend", False), ("deselect", True), ("toggle", False), ("ring", False)]}),

        ("mesh.loop_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "alt": True},
         {"properties": [("extend", False), ("deselect", False), ("toggle", False), ("ring", True)]}),
        ("mesh.loop_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "alt": True, "shift": True},
         {"properties": [("extend", True), ("deselect", False), ("toggle", False), ("ring", True)]}),
        ("mesh.loop_select", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "alt": True, "ctrl": True},
         {"properties": [("extend", False), ("deselect", True), ("toggle", False), ("ring", True)]}),

        ("mesh.shortest_path_pick", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True, "ctrl": True},
         {"properties": [("use_fill", False)]}),

        ("mesh.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("mesh.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("mesh.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("mesh.select_more", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("mesh.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
        ("mesh.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS'}, None),
        # Hide/reveal.
        ("mesh.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("mesh.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("mesh.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        # Tools.
        ("mesh.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        op_menu("VIEW3D_MT_edit_mesh_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_mesh_delete", {"type": 'DEL', "value": 'PRESS'}),
        ("mesh.dissolve_mode", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True}, None),
        ("mesh.dissolve_mode", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit')]}),
        # Menus.
        op_menu("VIEW3D_MT_edit_mesh_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        #Tools
        *_template_items_basic_tools(),
        op_tool("builtin.bevel", {"type": 'B', "value": 'PRESS', "ctrl": True}),
        op_tool("builtin.inset_faces", {"type": 'I', "value": 'PRESS'}),
        op_tool("builtin.extrude_region", {"type": 'E', "value": 'PRESS', "ctrl": True}),
        op_tool("builtin.knife", {"type": 'K', "value": 'PRESS'}),
        op_tool("builtin.loop_cut", {"type": 'C', "value": 'PRESS', "alt": True}),

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
        ("armature.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("armature.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("armature.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        # Parenting.
        ("armature.parent_set", {"type": 'P', "value": 'PRESS'}, None),
        ("armature.parent_clear", {"type": 'P', "value": 'PRESS', "shift": True}, None),
        # Selection.
        ("armature.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("armature.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("armature.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),

        ("armature.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'PARENT'), ("extend", False)]}),
        ("armature.select_hierarchy", {"type": 'LEFT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'PARENT'), ("extend", True)]}),
        ("armature.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("direction", 'CHILD'), ("extend", False)]}),
        ("armature.select_hierarchy", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("direction", 'CHILD'), ("extend", True)]}),

        ("armature.select_more", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("armature.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),

        ("armature.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        ("armature.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),

        ("armature.shortest_path_pick", {"type": 'LEFTMOUSE', "value": 'PRESS', "ctrl": True, "shift": True}, None),

        # Editing.
        op_menu("VIEW3D_MT_edit_armature_delete", {"type": 'DEL', "value": 'PRESS'}),
        op_menu("VIEW3D_MT_edit_armature_delete", {"type": 'BACK_SPACE', "value": 'PRESS'}),
        ("armature.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("armature.dissolve", {"type": 'BACK_SPACE', "value": 'PRESS', "ctrl": True}, None),
        ("armature.dissolve", {"type": 'DEL', "value": 'PRESS', "ctrl": True}, None),
        # Menus.
        op_menu("VIEW3D_MT_armature_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        # Tools.
        *_template_items_basic_tools(),
        op_tool("builtin.roll", {"type": 'Y', "value": 'PRESS'}),
        op_tool("builtin.extrude", {"type": 'E', "value": 'PRESS', "ctrl": True}),

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
        ("mball.reveal_metaelems", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("mball.hide_metaelems", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("mball.hide_metaelems", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("mball.delete_metaelems", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("mball.delete_metaelems", {"type": 'DEL', "value": 'PRESS'}, None),
        ("mball.duplicate_move", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("mball.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("mball.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("mball.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("mball.select_similar", {"type": 'G', "value": 'PRESS', "shift": True}, None),
        op_menu("VIEW3D_MT_edit_metaball_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit')]}),
        # Tools
        *_template_items_basic_tools(),
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
        ("lattice.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("lattice.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("lattice.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("lattice.select_more", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("lattice.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
        ("object.vertex_parent_set", {"type": 'P', "value": 'PRESS', "ctrl": True}, None),
        op_menu("VIEW3D_MT_edit_lattice_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit')]}),
            # Tools
        op_tool("builtin.select_box", {"type": 'Q', "value": 'PRESS'}),
        op_tool("builtin.move", {"type": 'W', "value": 'PRESS'}),
        op_tool("builtin.rotate", {"type": 'E', "value": 'PRESS'}),
        op_tool("builtin.scale", {"type": 'R', "value": 'PRESS'}),
        op_tool("builtin.scale_cage", {"type": 'R', "value": 'PRESS', "shift": True}),
        op_tool("builtin.transform", {"type": 'T', "value": 'PRESS'}),
        op_tool("builtin.measure", {"type": 'M', "value": 'PRESS'}),
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
        ("particle.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'SELECT')]}),
        ("particle.select_all", {"type": 'A', "value": 'PRESS', "ctrl": True, "shift": True}, {"properties": [("action", 'DESELECT')]}),
        ("particle.select_all", {"type": 'I', "value": 'PRESS', "ctrl": True}, {"properties": [("action", 'INVERT')]}),
        ("particle.select_more", {"type": 'UP_ARROW', "value": 'PRESS'}, None),
        ("particle.select_less", {"type": 'DOWN_ARROW', "value": 'PRESS'}, None),
        ("particle.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS'},
         {"properties": [("deselect", False)]}),
        ("particle.select_linked", {"type": 'RIGHT_BRACKET', "value": 'PRESS', "shift": True},
         {"properties": [("deselect", True)]}),
        ("particle.delete", {"type": 'BACK_SPACE', "value": 'PRESS'}, None),
        ("particle.delete", {"type": 'DEL', "value": 'PRESS'}, None),
        ("particle.reveal", {"type": 'H', "value": 'PRESS', "alt": True}, None),
        ("particle.hide", {"type": 'H', "value": 'PRESS', "ctrl": True},
         {"properties": [("unselected", False)]}),
        ("particle.hide", {"type": 'H', "value": 'PRESS', "shift": True},
         {"properties": [("unselected", True)]}),
        ("particle.brush_edit", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
        ("particle.brush_edit", {"type": 'LEFTMOUSE', "value": 'PRESS', "shift": True}, None),
        ("wm.radial_control", {"type": 'S', "value": 'PRESS'},
         {"properties": [("data_path_primary", 'tool_settings.particle_edit.brush.size')]}),
        ("wm.radial_control", {"type": 'U', "value": 'PRESS', "shift": True},
         {"properties": [("data_path_primary", 'tool_settings.particle_edit.brush.strength')]}),
        ("wm.context_toggle", {"type": 'B', "value": 'PRESS'},
         {"properties": [("data_path", 'tool_settings.use_proportional_edit')]}),
        op_menu("VIEW3D_MT_particle_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
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
        op_menu("VIEW3D_MT_edit_text_context_menu", {"type": 'RIGHTMOUSE', "value": 'PRESS'}),
    ])


    return keymap


def km_object_non_modal(params):
    items = []
    keymap = (
        "Object Non-modal",
        {"space_type": 'EMPTY', "region_type": 'WINDOW'},
        {"items": items},
    )

    items.extend([

        ("ic_keymap.mesh_select_mode",{"type": 'ONE', "value": 'PRESS'},
         {"properties": [("type", 'VERT')]}),
        ("ic_keymap.mesh_select_mode",{"type": 'TWO', "value": 'PRESS'},
         {"properties": [("type", 'EDGE')]}),
        ("ic_keymap.mesh_select_mode",{"type": 'THREE', "value": 'PRESS'},
         {"properties": [("type", 'FACE')]}),
        ("object.mode_set",{"type": 'ONE', "value": 'PRESS'},
         {"properties": [("mode", 'EDIT')]}),
        ("object.mode_set",{"type": 'FOUR', "value": 'PRESS'},
         {"properties": [("mode", 'OBJECT')]}),
        ("object.mode_set",{"type": 'FIVE', "value": 'PRESS'},
         {"properties": [("mode", 'SCULPT')]}),
        ("object.mode_set",{"type": 'SIX', "value": 'PRESS'},
         {"properties": [("mode", 'VERTEX_PAINT')]}),
        ("object.mode_set",{"type": 'SEVEN', "value": 'PRESS'},
         {"properties": [("mode", 'WEIGHT_PAINT')]}),
        ("object.mode_set",{"type": 'EIGHT', "value": 'PRESS'},
         {"properties": [("mode", 'TEXTURE_PAINT')]}),

        ("object.mode_set",{"type": 'TWO', "value": 'PRESS'},
         {"properties": [("mode", 'EDIT_GPENCIL')]}),
        ("object.mode_set",{"type": 'THREE', "value": 'PRESS'},
         {"properties": [("mode", 'SCULPT_GPENCIL')]}),
        ("object.mode_set",{"type": 'FOUR', "value": 'PRESS'},
         {"properties": [("mode", 'PAINT_GPENCIL')]}),
        ("object.mode_set",{"type": 'FIVE', "value": 'PRESS'},
         {"properties": [("mode", 'WEIGHT_GPENCIL')]}),

        ("object.mode_set",{"type": 'THREE', "value": 'PRESS'},
         {"properties": [("mode", 'POSE')]})
    ])

    return keymap


# ------------------------------------------------------------------------------
# Modal Maps


def km_knife_tool_modal_map(_params):
    items = []
    keymap = (
        "Knife Tool Modal Map",
        {"space_type": 'EMPTY', "region_type": 'WINDOW', "modal": True},
        {"items": items},
    )

    items.extend([
        ("CANCEL", {"type": 'ESC', "value": 'PRESS', "any": True}, None),
        ("PANNING", {"type": 'LEFTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("CONFIRM", {"type": 'RET', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'NUMPAD_ENTER', "value": 'PRESS', "any": True}, None),
        ("CONFIRM", {"type": 'LEFTMOUSE', "value": 'DOUBLE_CLICK', "any": True}, None),
        ("ADD_CUT", {"type": 'LEFTMOUSE', "value": 'ANY', "any": True}, None),
        ("NEW_CUT", {"type": 'E', "value": 'PRESS'}, None),
        ("SNAP_MIDPOINTS_ON", {"type": 'LEFT_CTRL', "value": 'PRESS', "any": False}, None),
        ("SNAP_MIDPOINTS_OFF", {"type": 'LEFT_CTRL', "value": 'RELEASE', "any": False}, None),
        ("SNAP_MIDPOINTS_ON", {"type": 'RIGHT_CTRL', "value": 'PRESS', "any": False}, None),
        ("SNAP_MIDPOINTS_OFF", {"type": 'RIGHT_CTRL', "value": 'RELEASE', "any": False}, None),
        ("IGNORE_SNAP_ON", {"type": 'LEFT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("IGNORE_SNAP_OFF", {"type": 'LEFT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("IGNORE_SNAP_ON", {"type": 'RIGHT_SHIFT', "value": 'PRESS', "any": True}, None),
        ("IGNORE_SNAP_OFF", {"type": 'RIGHT_SHIFT', "value": 'RELEASE', "any": True}, None),
        ("ANGLE_SNAP_TOGGLE", {"type": 'C', "value": 'PRESS'}, None),
        ("CUT_THROUGH_TOGGLE", {"type": 'X', "value": 'PRESS'}, None),
        ("PANNING", {"type": 'MIDDLEMOUSE', "value": 'PRESS', "alt": True}, None),
        ("PANNING", {"type": 'RIGHTMOUSE', "value": 'PRESS', "alt": True}, None),
        ("CONFIRM", {"type": 'RIGHTMOUSE', "value": 'PRESS'}, None),
    ])

    return keymap


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


# ------------------------------------------------------------------------------
# Gizmos

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
        },
    )

    return keymap


# ------------------------------------------------------------------------------
# Tool System Keymaps


def km_3d_view_tool_transform(params):
    return (
        "3D View Tool: Transform",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.from_gizmo", {"type": 'MIDDLEMOUSE', "value": 'ANY'}, None),
            *_template_items_tool_select_actions("view3d.select_box", type=params.tool_tweak, value='ANY'),
        ]},
    )


def km_3d_view_tool_move(params):
    return (
        "3D View Tool: Move",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.translate", {"type": 'MIDDLEMOUSE', "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
            *_template_items_tool_select_actions("view3d.select_box", type=params.tool_tweak, value='ANY'),
        ]},
    )


def km_3d_view_tool_rotate(params):
    return (
        "3D View Tool: Rotate",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.rotate", {"type": 'MIDDLEMOUSE', "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
            *_template_items_tool_select_actions("view3d.select_box", type=params.tool_tweak, value='ANY'),
        ]},
    )


def km_3d_view_tool_scale(params):
    return (
        "3D View Tool: Scale",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.resize", {"type": 'MIDDLEMOUSE', "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
            *_template_items_tool_select_actions("view3d.select_box", type=params.tool_tweak, value='ANY'),
        ]},
    )


def km_3d_view_tool_edit_mesh_extrude_region(params):
    return (
        "3D View Tool: Edit Mesh, Extrude Region",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.extrude_context_move", {"type": 'MIDDLEMOUSE', "value": 'ANY'},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
            *_template_items_tool_select_actions("view3d.select_box", type=params.tool_tweak, value='ANY'),
        ]},
    )


def km_3d_view_tool_edit_mesh_shear(params):
    return (
        "3D View Tool: Edit Mesh, Shear",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("transform.shear", {"type": 'MIDDLEMOUSE', "value": 'ANY'},
             {"properties": [("release_confirm", True)]}),
            *_template_items_tool_select_actions("view3d.select_box", type=params.tool_tweak, value='ANY'),
        ]},
    )


def km_3d_view_tool_edit_mesh_spin(params):
    return (
        "3D View Tool: Edit Mesh, Spin",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.spin", {"type": 'MIDDLEMOUSE', "value": 'ANY'}, None),
            *_template_items_tool_select_actions("view3d.select_box", type=params.tool_tweak, value='ANY'),
        ]},
    )


def km_3d_view_tool_edit_mesh_spin_duplicate(params):
    return (
        "3D View Tool: Edit Mesh, Spin Duplicates",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("mesh.spin", {"type": 'MIDDLEMOUSE', "value": 'ANY'},
             {"properties": [("dupli", True)]}),
            *_template_items_tool_select_actions("view3d.select_box", type=params.tool_tweak, value='ANY'),
        ]},
    )


def km_3d_view_tool_edit_curve_extrude(params):
    return (
        "3D View Tool: Edit Curve, Extrude",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("curve.extrude_move", {"type": 'MIDDLEMOUSE', "value": 'ANY'},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
            *_template_items_tool_select_actions("view3d.select_box", type=params.tool_tweak, value='ANY'),
        ]},
    )


def km_3d_view_tool_edit_armature_extrude(params):
    return (
        "3D View Tool: Edit Armature, Extrude",
        {"space_type": 'VIEW_3D', "region_type": 'WINDOW'},
        {"items": [
            ("armature.extrude_move", {"type": 'MIDDLEMOUSE', "value": 'ANY'},
             {"properties": [("TRANSFORM_OT_translate", [("release_confirm", True)])]}),
            *_template_items_tool_select_actions("view3d.select_box", type=params.tool_tweak, value='ANY'),
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
        km_knife_tool_modal_map(params),
        km_eyedropper_modal_map(params),
        km_eyedropper_colorramp_pointsampling_map(params),
        km_transform_modal_map(params),

        # Gizmos.
        km_generic_gizmo(params),
        km_generic_gizmo_drag(params),
        km_generic_gizmo_maybe_drag(params),
        km_generic_gizmo_click_drag(params),

        # Tool System.
        km_3d_view_tool_transform(params),
        km_3d_view_tool_move(params),
        km_3d_view_tool_rotate(params),
        km_3d_view_tool_scale(params),
        km_3d_view_tool_edit_mesh_extrude_region(params),
        km_3d_view_tool_edit_mesh_shear(params),
        km_3d_view_tool_edit_mesh_spin(params),
        km_3d_view_tool_edit_mesh_spin_duplicate(params),
        km_3d_view_tool_edit_curve_extrude(params),
        km_3d_view_tool_edit_armature_extrude(params),
    ]
