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


import math
import collections
import enum
import re
import string
import time
from ctypes import (
    c_void_p,
    cast,
    POINTER,
)

import blf
import bpy
import bpy.props

from .common import debug_print
from .utils.bl_class_registry import BlClassRegistry
from .utils import compatibility as compat
from .utils import c_structures

if compat.check_version(2, 80, 0) >= 0:
    from .compat import bglx as bgl
else:
    import bgl


event_type_enum_items = bpy.types.Event.bl_rna.properties["type"].enum_items
EventType = enum.IntEnum(
    "EventType",
    [(e.identifier, e.value) for e in event_type_enum_items]
)
EventType.names = {e.identifier: e.name for e in event_type_enum_items}


def draw_mouse(x, y, w, h, left_pressed, right_pressed, middle_pressed, color,
               round_radius, fill=False, fill_color=None):

    mouse_body = [x, y, w, h/2]
    left_mouse_button = [x, y + h/2, w/3, h/2]
    middle_mouse_button = [x + w/3, y + h/2, w/3, h/2]
    right_mouse_button = [x + 2*w/3, y + h/2, w/3, h/2]

    # Mouse body.
    if fill:
        draw_rounded_box(mouse_body[0], mouse_body[1],
                         mouse_body[2], mouse_body[3],
                         round_radius,
                         fill=True, color=fill_color,
                         round_corner=[True, True, False, False])
    draw_rounded_box(mouse_body[0], mouse_body[1],
                     mouse_body[2], mouse_body[3],
                     round_radius,
                     fill=False, color=color,
                     round_corner=[True, True, False, False])

    # Left button.
    if fill:
        draw_rounded_box(left_mouse_button[0], left_mouse_button[1],
                         left_mouse_button[2], left_mouse_button[3],
                         round_radius / 2,
                         fill=True, color=fill_color,
                         round_corner=[False, False, False, True])
    draw_rounded_box(left_mouse_button[0], left_mouse_button[1],
                     left_mouse_button[2], left_mouse_button[3],
                     round_radius / 2,
                     fill=False, color=color,
                     round_corner=[False, False, False, True])
    if left_pressed:
        draw_rounded_box(left_mouse_button[0], left_mouse_button[1],
                        left_mouse_button[2], left_mouse_button[3],
                        round_radius / 2,
                        fill=True, color=color,
                        round_corner=[False, False, False, True])

    # Middle button.
    if fill:
        draw_rounded_box(middle_mouse_button[0], middle_mouse_button[1],
                         middle_mouse_button[2], middle_mouse_button[3],
                         round_radius / 2,
                         fill=True, color=fill_color,
                         round_corner=[False, False, False, False])
    draw_rounded_box(middle_mouse_button[0], middle_mouse_button[1],
                     middle_mouse_button[2], middle_mouse_button[3],
                     round_radius / 2,
                     fill=False, color=color,
                     round_corner=[False, False, False, False])
    if middle_pressed:
        draw_rounded_box(middle_mouse_button[0], middle_mouse_button[1],
                        middle_mouse_button[2], middle_mouse_button[3],
                        round_radius / 2,
                        fill=True, color=color,
                        round_corner=[False, False, False, False])

    # Right button.
    if fill:
        draw_rounded_box(right_mouse_button[0], right_mouse_button[1],
                         right_mouse_button[2], right_mouse_button[3],
                         round_radius / 2,
                         fill=True, color=fill_color,
                         round_corner=[False, False, True, False])
    draw_rounded_box(right_mouse_button[0], right_mouse_button[1],
                     right_mouse_button[2], right_mouse_button[3],
                     round_radius / 2,
                     fill=False, color=color,
                     round_corner=[False, False, True, False])
    if right_pressed:
        draw_rounded_box(right_mouse_button[0], right_mouse_button[1],
                        right_mouse_button[2], right_mouse_button[3],
                        round_radius / 2,
                        fill=True, color=color,
                        round_corner=[False, False, True, False])


def draw_rounded_box(x, y, w, h, round_radius, fill=False, color=[1.0, 1.0, 1.0],
                     round_corner=[True, True, True, True]):
    """round_corner: [Right Bottom, Left Bottom, Right Top, Left Top]"""

    def circle_verts_num(r):
        """Get number of verticies for circle optimized for drawing."""

        num_verts = 32
        threshold = 2.0  # pixcel
        while True:
            if r * 2 * math.pi / num_verts > threshold:
                return num_verts
            num_verts -= 4
            if num_verts < 1:
                return 1

    num_verts = circle_verts_num(round_radius)
    n = int(num_verts / 4) + 1
    dangle = math.pi * 2 / num_verts

    radius = [round_radius if rc else 0 for rc in round_corner]

    x_origin = [
        x + radius[0],
        x + w - radius[1],
        x + w - radius[2],
        x + radius[3],
    ]
    y_origin = [
        y + radius[0],
        y + radius[1],
        y + h - radius[2],
        y + h - radius[3],
    ]
    angle_start = [
        math.pi * 1.0,
        math.pi * 1.5,
        math.pi * 0.0,
        math.pi * 0.5,
    ]

    bgl.glColor3f(*color)
    if fill:
        bgl.glBegin(bgl.GL_TRIANGLE_FAN)
    else:
        bgl.glBegin(bgl.GL_LINE_LOOP)
    for x0, y0, angle, r in zip(x_origin, y_origin, angle_start, radius):
        for _ in range(n):
            x = x0 + r * math.cos(angle)
            y = y0 + r * math.sin(angle)
            bgl.glVertex2f(x, y)
            angle += dangle
    bgl.glEnd()
    bgl.glColor3f(1.0, 1.0, 1.0)


def draw_rect(x1, y1, x2, y2, color):
    bgl.glColor3f(*color)

    bgl.glBegin(bgl.GL_QUADS)
    bgl.glVertex2f(x1, y1)
    bgl.glVertex2f(x1, y2)
    bgl.glVertex2f(x2, y2)
    bgl.glVertex2f(x2, y1)
    bgl.glEnd()

    bgl.glColor3f(1.0, 1.0, 1.0)


def draw_text_background(text, font_id, x, y, background_color):
    width = blf.dimensions(font_id, text)[0]
    height = blf.dimensions(font_id, string.printable)[1]
    margin = height * 0.2

    draw_rect(x, y - margin, x + width, y + height - margin, background_color)


def draw_text(text, font_id, color, shadow=False, shadow_color=None):
    blf.enable(font_id, blf.SHADOW)

    # Draw shadow.
    if shadow:
        blf.shadow_offset(font_id, 3, -3)
        blf.shadow(font_id, 5, *shadow_color, 1.0)

    # Draw text.
    compat.set_blf_font_color(font_id, *color, 1.0)
    blf.draw(font_id, text)

    blf.disable(font_id, blf.SHADOW)


def draw_line(p1, p2, color, shadow=False, shadow_color=None):
    bgl.glEnable(bgl.GL_BLEND)
    bgl.glEnable(bgl.GL_LINE_SMOOTH)

    # Draw shadow.
    if shadow:
        bgl.glLineWidth(3.0)
        bgl.glColor4f(*shadow_color, 1.0)
        bgl.glBegin(bgl.GL_LINES)
        bgl.glVertex2f(*p1)
        bgl.glVertex2f(*p2)
        bgl.glEnd()

    # Draw line.
    bgl.glLineWidth(1.5 if shadow else 1.0)
    bgl.glColor3f(*color)
    bgl.glBegin(bgl.GL_LINES)
    bgl.glVertex2f(*p1)
    bgl.glVertex2f(*p2)
    bgl.glEnd()

    bgl.glLineWidth(1.0)
    bgl.glDisable(bgl.GL_LINE_SMOOTH)


def intersect_aabb(min1, max1, min2, max2):
    """Check intersection using AABB method."""

    for i in range(len(min1)):
        if (max1[i] < min2[i]) or (max2[i] < min1[i]):
            return False

    return True


def get_window_region_rect(area):
    """Return 'WINDOW' region rectangle."""

    rect = [99999, 99999, 0, 0]
    for region in area.regions:
        if region.type == 'WINDOW':
            rect[0] = min(rect[0], region.x)
            rect[1] = min(rect[1], region.y)
            rect[2] = max(region.x + region.width - 1, rect[2])
            rect[3] = max(region.y + region.height - 1, rect[3])

    return rect


def get_region_rect_on_v3d(context, area=None, region=None):
    """On VIEW_3D, we need to handle region overlap.
       This function takes into accout this, and return rectangle.
    """

    if not area:
        area = context.area
    if not region:
        region = context.region

    # We don't need to handle non-'WINDOW' region which is not effected by
    # region overlap. So we can return region rectangle as it is.
    if region.type != 'WINDOW':
        return [region.x, region.y,
                region.x + region.width, region.y + region.height]

    # From here, we handle 'WINDOW' region with considering region overlap.
    window = region
    tools = ui = None
    for ar in area.regions:
        # We need to dicard regions whose width is 1.
        if ar.width > 1:
            if ar.type == 'WINDOW':
                if ar == window:
                    window = ar
            elif ar.type == 'TOOLS':
                tools = ar
            elif ar.type == 'UI':
                ui = ar

    xmin, _, xmax, _ = get_window_region_rect(area)
    sys_pref = compat.get_user_preferences(context).system
    if sys_pref.use_region_overlap:
        left_width = right_width = 0

        if tools and ui:
            r1, r2 = sorted([tools, ui], key=lambda ar: ar.x)
            if r1.x == area.x:
                # 'TOOLS' and 'UI' are located on left side.
                if r2.x == r1.x + r1.width:
                    left_width = r1.width + r2.width
                # 'TOOLS' and 'UI' are located on each side.
                else:
                    left_width = r1.width
                    right_width = r2.width
            # 'TOOLS' and 'UI' are located on right side.
            else:
                right_width = r1.width + r2.width

        elif tools:
            # 'TOOLS' is located on left side.
            if tools.x == area.x:
                left_width = tools.width
            # 'TOOLS' is located on right side.
            else:
                right_width = tools.width

        elif ui:
            # 'UI' is located on left side.
            if ui.x == area.x:
                left_width = ui.width
            # 'TOOLS' is located on right side.
            else:
                right_width = ui.width

        # Clip 'UI' and 'TOOLS' region from 'WINDOW' region, which enables us
        # to show only 'WINDOW' region.
        xmin = max(xmin, area.x + left_width)
        xmax = min(xmax, area.x + area.width - right_width - 1)

    ymin = window.y
    ymax = window.y + window.height - 1

    return xmin, ymin, xmax, ymax


def get_display_event_text(event_id):
    prefs = compat.get_user_preferences(bpy.context).addons[__package__].preferences

    if not prefs.enable_display_event_text_aliases:
        return EventType.names[event_id]

    for prop in prefs.display_event_text_aliases_props:
        if prop.event_id == event_id:
            if prop.alias_text == "":
                return prop.default_text
            else:
                return prop.alias_text

    return "UNKNOWN"


def show_mouse_hold_status(prefs):
    if not prefs.show_mouse_events:
        return False
    return prefs.mouse_events_show_mode in ['HOLD_STATUS', 'EVENT_HISTORY_AND_HOLD_STATUS']


def show_mouse_event_history(prefs):
    if not prefs.show_mouse_events:
        return False
    return prefs.mouse_events_show_mode in ['EVENT_HISTORY', 'EVENT_HISTORY_AND_HOLD_STATUS']


def show_text_background(prefs):
    if not prefs.background:
        return False
    return prefs.background_mode == 'TEXT'


def show_draw_area_background(prefs):
    if not prefs.background:
        return False
    return prefs.background_mode == 'DRAW_AREA'


@BlClassRegistry()
class SK_OT_ScreencastKeys(bpy.types.Operator):
    bl_idname = "wm.sk_screencast_keys"
    bl_label = "Screencast Keys"
    bl_description = "Display keys pressed"
    bl_options = {'REGISTER'}

    # Hold modifier keys.
    hold_modifier_keys = []
    # Hold mouse buttons.
    hold_mouse_buttons = {
        'LEFTMOUSE': False,
        'RIGHTMOUSE': False,
        'MIDDLEMOUSE': False,
    }
    # Event history.
    # Format: [time, event_type, modifiers, repeat_count]
    event_history = []
    # Operator history.
    # Format: [time, bl_label, idname_py, addr]
    operator_history = []

    MODIFIER_EVENT_TYPES = [
        EventType.LEFT_SHIFT,
        EventType.RIGHT_SHIFT,
        EventType.LEFT_CTRL,
        EventType.RIGHT_CTRL,
        EventType.LEFT_ALT,
        EventType.RIGHT_ALT,
        EventType.OSKEY
    ]

    MOUSE_EVENT_TYPES = {
        EventType.LEFTMOUSE,
        EventType.MIDDLEMOUSE,
        EventType.RIGHTMOUSE,
        EventType.BUTTON4MOUSE,
        EventType.BUTTON5MOUSE,
        EventType.BUTTON6MOUSE,
        EventType.BUTTON7MOUSE,
        EventType.TRACKPADPAN,
        EventType.TRACKPADZOOM,
        EventType.MOUSEROTATE,
        EventType.WHEELUPMOUSE,
        EventType.WHEELDOWNMOUSE,
        EventType.WHEELINMOUSE,
        EventType.WHEELOUTMOUSE,
    }

    SPACE_TYPES = compat.get_all_space_types()

    # Height ratio for separator (against text height).
    HEIGHT_RATIO_FOR_SEPARATOR = 0.6

    # Height ratio for hold mouse status (against width).
    HEIGHT_RATIO_FOR_MOUSE_HOLD_STATUS = 1.3

    # Margin ratio for hold modifier keys box (against text height).
    MARGIN_RATIO_FOR_HOLD_MODIFIER_KEYS_BOX = 0.2

    # Width ratio for separator between hold mouse status and
    # hold modifier keys (against mouse width).
    WIDTH_RATIO_FOR_SEPARATOR_BETWEEN_MOUSE_AND_MODIFIER_KEYS = 0.4

    # Draw area margin.
    DRAW_AREA_MARGIN_LEFT = 15
    DRAW_AREA_MARGIN_RIGHT = 15
    DRAW_AREA_MARGIN_TOP = 15
    DRAW_AREA_MARGIN_BOTTOM = 15

    # Interval for 'TIMER' event (redraw).
    TIMER_STEP = 0.1

    # Previous redraw time.
    prev_time = 0.0

    # Timer handlers.
    # Format: {Window.as_pointer(): Timer}
    timers = {}

    # Draw handlers.
    # Format: {(Space, Region.type): handle}
    handlers = {}

    # Regions which are drawing in previous redraw.
    # Format: {Region.as_pointer()}
    draw_regions_prev = set()

    # Draw target.
    origin = {
        "window": "",       # Window.as_pointer()
        "area": "",         # Area.as_pointer()
        "space": "",        # Space.as_pointer()
        "region_type": "",  # Region.type
    }

    # Area - Space mapping.
    # Format: {Area.as_pointer(), [Space.as_pointer(), ...]}
    # TODO: Clear when this model is finished.
    area_spaces = collections.defaultdict(set)

    # Check if this operator is running.
    # TODO: We can check it with the valid of event handler.
    running = False

    # Current mouse coordinate.
    current_mouse_co = [0.0, 0.0]

    @classmethod
    def is_running(cls):
        return cls.running

    @classmethod
    def sorted_modifier_keys(cls, modifiers):
        """Sort and unique modifier keys."""

        def key_fn(event_type):
            if event_type in cls.MODIFIER_EVENT_TYPES:
                return cls.MODIFIER_EVENT_TYPES.index(event_type)
            else:
                return 100

        modifiers = sorted(modifiers, key=key_fn)
        names = []
        for mod in modifiers:
            name = mod.names[mod.name]
            assert mod in cls.MODIFIER_EVENT_TYPES, \
                   "{} must be modifier types".format(name)

            # Remove left and right identifier.
            name = re.sub("(Left |Right )", "", name)

            # Unique.
            if name not in names:
                names.append(name)

        return names

    @classmethod
    def removed_old_event_history(cls):
        """Return event history whose old events are removed."""

        prefs = compat.get_user_preferences(bpy.context).addons[__package__].preferences
        current_time = time.time()

        event_history = []
        for item in cls.event_history:
            event_time = item[0]
            t = current_time - event_time
            if t <= prefs.display_time:
                event_history.append(item)

        if len(event_history) >= prefs.max_event_history:
            event_history = event_history[-prefs.max_event_history:]

        return event_history

    @classmethod
    def removed_old_operator_history(cls):
        """Return operator history whose old operators are removed."""
        # TODO: Control number of history from Preferences.

        return cls.operator_history[-32:]

    @classmethod
    def get_offset_for_alignment(cls, width, context):
        prefs = compat.get_user_preferences(context).addons[__package__].preferences

        dw, _ = cls.calc_draw_area_size(context)
        dw -= cls.DRAW_AREA_MARGIN_LEFT + cls.DRAW_AREA_MARGIN_RIGHT

        offset_x = 0.0
        if prefs.align == 'CENTER':
            offset_x = (dw - width) / 2.0
        elif prefs.align == 'RIGHT':
            offset_x = dw - width

        return offset_x + cls.DRAW_AREA_MARGIN_LEFT, cls.DRAW_AREA_MARGIN_BOTTOM

    @classmethod
    def get_text_offset_for_alignment(cls, font_id, text, context):
        tw = blf.dimensions(font_id, text)[0]

        return cls.get_offset_for_alignment(tw, context)

    @classmethod
    def get_origin(cls, context):
        """Get draw target.
           Retrun value: (Window, Area, Region, x, y)
        """

        prefs = compat.get_user_preferences(context).addons[__package__].preferences

        def is_window_match(window):
            return window.as_pointer() == cls.origin["window"]

        def is_area_match(area):
            if area.as_pointer() == cls.origin["area"]:
                return True     # Area is just same as user specified area.
            elif area.spaces.active.as_pointer() == cls.origin["space"]:
                return True     # Area is not same, but active space information is same.
            else:
                area_p = area.as_pointer()
                if area_p in cls.area_spaces:
                    spaces_p = {s.as_pointer() for s in area.spaces}
                    if cls.origin["space"] in spaces_p:
                        # Exists in inactive space information.
                        return True
            return False

        def is_region_match(area):
            return region.type == cls.origin["region_type"]

        for window in context.window_manager.windows:
            if is_window_match(window):
                break
        else:
            return None, None, None, 0, 0

        # Calculate draw offset
        draw_area_width, draw_area_height = cls.calc_draw_area_size(context)
        if prefs.align == 'LEFT':
            x, y = prefs.offset
            if prefs.origin == 'CURSOR':
                x += cls.current_mouse_co[0] - draw_area_width / 2
                y += cls.current_mouse_co[1] - draw_area_height
        elif prefs.align == 'CENTER':
            x, y = prefs.offset
            if prefs.origin == 'WINDOW':
                x += (window.width * 2 - draw_area_width) / 2
            elif prefs.origin == 'AREA':
                for area in window.screen.areas:
                    if is_area_match(area):
                        x += (area.width - draw_area_width) / 2
                        break
            elif prefs.origin == 'REGION':
                found = False
                for area in window.screen.areas:
                    if found:
                        break
                    if not is_area_match(area):
                        continue
                    for region in area.regions:
                        if found:
                            break
                        if is_region_match(region):
                            if area.type == 'VIEW_3D':
                                rect = get_region_rect_on_v3d(context, area, region)
                                x += (rect[2] - rect[0] - draw_area_width) / 2
                            else:
                                x += (region.width - draw_area_width) / 2
                            found = True
            elif prefs.origin == 'CURSOR':
                x += cls.current_mouse_co[0] - draw_area_width / 2
                y += cls.current_mouse_co[1] - draw_area_height
        elif prefs.align == 'RIGHT':
            x, y = prefs.offset
            if prefs.origin == 'WINDOW':
                x += window.width * 2 - draw_area_width
            elif prefs.origin == 'AREA':
                for area in window.screen.areas:
                    if is_area_match(area):
                        x += area.width - draw_area_width
                        break
            elif prefs.origin == 'REGION':
                found = False
                for area in window.screen.areas:
                    if found:
                        break
                    if not is_area_match(area):
                        continue
                    for region in area.regions:
                        if found:
                            break
                        if is_region_match(region):
                            if area.type == 'VIEW_3D':
                                rect = get_region_rect_on_v3d(context, area, region)
                                x += rect[2] - rect[0] - draw_area_width
                            else:
                                x += region.width - draw_area_width
                            found = True
            elif prefs.origin == 'CURSOR':
                x += cls.current_mouse_co[0] - draw_area_width / 2
                y += cls.current_mouse_co[1] - draw_area_height

        if (prefs.origin == 'WINDOW') or (prefs.origin == 'CURSOR'):
            return window, None, None, x, y
        elif prefs.origin == 'AREA':
            for area in window.screen.areas:
                if is_area_match(area):
                    return window, area, None, x + area.x, y + area.y
        elif prefs.origin == 'REGION':
            for area in window.screen.areas:
                if not is_area_match(area):
                    continue
                for region in area.regions:
                    if is_region_match(region):
                        if area.type == 'VIEW_3D':
                            rect = get_region_rect_on_v3d(context, area, region)
                            x += rect[0]
                            y += rect[1]
                        else:
                            x += region.x
                            y += region.y
                        return window, area, region, x, y

        return None, None, None, 0, 0

    @classmethod
    def calc_draw_area_size(cls, context):
        """Return draw area size.

        Draw format:

            Overview:
                ....
                Event history[-3]
                Event history[-2]
                Event history[-1]

                Mouse hold status  Hold modifier key list
                ----------------
                Operator history

            Event history format:
                With count: {key} x{count}
                With modifier key: {modifier key} + {key}

            Hold modifier key list format:
                 --------------     --------------
                |{modifier key}| + |{modifier key}|
                 --------------     --------------
        """

        prefs = compat.get_user_preferences(context).addons[__package__].preferences

        font_size = prefs.font_size
        font_id = 0         # TODO: font_id should be constant.
        dpi = compat.get_user_preferences(context).system.dpi
        blf.size(font_id, font_size, dpi)

        # Get string height in draw area.
        sh = blf.dimensions(font_id, string.printable)[1]

        # Calculate width/height of draw area.
        draw_area_width = 0
        draw_area_height = 0

        # Last operator.
        if prefs.show_last_operator:
            operator_history = cls.removed_old_operator_history()
            if operator_history:
                _, name, idname_py, _ = operator_history[-1]
                text = bpy.app.translations.pgettext(name, "Operator")
                text += " ('{}')".format(idname_py)

                sw = blf.dimensions(font_id, text)[0]
                draw_area_width = max(draw_area_width, sw)
            draw_area_height += sh + sh * cls.HEIGHT_RATIO_FOR_SEPARATOR

        # Hold mouse status / Hold modifier keys.
        mouse_hold_status_width = 0.0
        mouse_hold_status_height = 0.0
        if show_mouse_hold_status(prefs):
            mouse_hold_status_width = prefs.mouse_size
            mouse_hold_status_height = prefs.mouse_size * cls.HEIGHT_RATIO_FOR_MOUSE_HOLD_STATUS

        margin = sh * cls.MARGIN_RATIO_FOR_HOLD_MODIFIER_KEYS_BOX
        tw = mouse_hold_status_width
        if cls.hold_modifier_keys:
            mod_names = cls.sorted_modifier_keys(cls.hold_modifier_keys)
            text = " + ".join(mod_names)
            tw += blf.dimensions(font_id, text)[0] + margin * 2 + \
                mouse_hold_status_width * cls.WIDTH_RATIO_FOR_SEPARATOR_BETWEEN_MOUSE_AND_MODIFIER_KEYS

        draw_area_width = max(draw_area_width, tw)
        if mouse_hold_status_height > sh:
            draw_area_height += mouse_hold_status_height + margin * 2
        else:
            draw_area_height += sh + margin * 2

        # Event history.
        event_history = cls.removed_old_event_history()
        draw_area_height += sh * cls.HEIGHT_RATIO_FOR_SEPARATOR
        for _, event_type, modifiers, repeat_count in event_history[::-1]:
            text = event_type.names[event_type.name]
            if modifiers:
                mod_keys = cls.sorted_modifier_keys(modifiers)
                text = "{} + {}".format(" + ".join(mod_keys), text)
            if repeat_count > 1:
                text += " x{}".format(repeat_count)

            sw = blf.dimensions(font_id, text)[0]
            draw_area_width = max(draw_area_width, sw)
        draw_area_height += prefs.max_event_history * sh

        # Add margin.
        draw_area_width += cls.DRAW_AREA_MARGIN_LEFT + cls.DRAW_AREA_MARGIN_RIGHT
        draw_area_height += cls.DRAW_AREA_MARGIN_TOP + cls.DRAW_AREA_MARGIN_BOTTOM

        return draw_area_width, draw_area_height

    @classmethod
    def calc_draw_area_rect(cls, context):
        """Return draw area rectangle."""

        prefs = compat.get_user_preferences(context).addons[__package__].preferences

        # Get draw target.
        window, area, region, x, y = cls.get_origin(context)
        if not window:
            return None

        # Calculate width/height of draw area.
        draw_area_width, draw_area_height = cls.calc_draw_area_size(context)

        if (prefs.origin == 'WINDOW') or (prefs.origin == 'CURSOR'):
            return (x,
                    y,
                    x + draw_area_width,
                    y + draw_area_height)
        elif prefs.origin == 'AREA':
            xmin = area.x
            ymin = area.y
            xmax = area.x + area.width - 1
            ymax = area.y + area.height - 1
            return (max(x, xmin),
                    max(y, ymin),
                    min(x + draw_area_width, xmax),
                    min(y + draw_area_height, ymax))
        elif prefs.origin == 'REGION':
            xmin = region.x
            ymin = region.y
            xmax = region.x + region.width - 1
            ymax = region.y + region.height - 1
            return (max(x, xmin),
                    max(y, ymin),
                    min(x + draw_area_width, xmax),
                    min(y + draw_area_height, ymax))
        
        assert False, "Value 'prefs.origin' is invalid (value={}).".format(prefs.origin)


    @classmethod
    def find_redraw_regions(cls, context):
        """Find regions to redraw."""

        rect = cls.calc_draw_area_rect(context)
        if not rect:
            return []       # No draw target.

        draw_area_min_x, draw_area_min_y, draw_area_max_x, draw_area_max_y = rect
        width = draw_area_max_x - draw_area_min_x
        height = draw_area_max_y - draw_area_min_y
        if width == height == 0:
            return []       # Zero size region.
        
        draw_area_min = [draw_area_min_x, draw_area_min_y]
        draw_area_max = [draw_area_max_x - 1, draw_area_max_y - 1]

        # Collect regions which overlaps with draw area.
        regions = []
        for area in context.screen.areas:
            for region in area.regions:
                if region.type == '':
                    continue    # Skip region with no type.
                region_min = [region.x, region.y]
                region_max = [region.x + region.width - 1,
                              region.y + region.height - 1]
                if intersect_aabb(region_min, region_max,
                                  draw_area_min, draw_area_max):
                    regions.append((area, region))

        return regions

    @classmethod
    def draw_callback(cls, context):
        prefs = compat.get_user_preferences(context).addons[__package__].preferences

        if context.window.as_pointer() != cls.origin["window"]:
            return      # Not match target window.

        rect = cls.calc_draw_area_rect(context)
        if not rect:
            return      # No draw target.

        draw_area_min_x, draw_area_min_y, draw_area_max_x, draw_area_max_y = rect
        _, _, _, origin_x, origin_y = cls.get_origin(context)
        draw_area_width = draw_area_max_x - origin_x
        draw_area_height = draw_area_max_y - origin_y
        if draw_area_width == draw_area_height == 0:
            return

        region = context.region
        area = context.area
        if region.type == 'WINDOW':
            region_min_x, region_min_y, region_max_x, region_max_y = get_window_region_rect(area)
        else:
            region_min_x = region.x
            region_min_y = region.y
            region_max_x = region.x + region.width - 1
            region_max_y = region.y + region.height - 1
        if not intersect_aabb(
                [region_min_x, region_min_y], [region_max_x, region_max_y],
                [draw_area_min_x + 1, draw_area_min_y + 1], [draw_area_max_x - 1, draw_area_max_x - 1]):
            # We don't need to draw if draw area is not overlapped with region.
            return

        current_time = time.time()
        region_drawn = False

        font_size = prefs.font_size
        font_id = 0
        dpi = compat.get_user_preferences(context).system.dpi
        blf.size(font_id, font_size, dpi)

        scissor_box = bgl.Buffer(bgl.GL_INT, 4)
        bgl.glGetIntegerv(bgl.GL_SCISSOR_BOX, scissor_box)
        # Clip 'TOOLS' and 'UI' region from 'WINDOW' region if need.
        # This prevents from drawing multiple time when
        # user_preferences.system.use_region_overlap is True.
        if context.area.type == 'VIEW_3D' and region.type == 'WINDOW':
            x_min, y_min, x_max, y_max = get_region_rect_on_v3d(context)
            bgl.glScissor(x_min, y_min, x_max - x_min + 1, y_max - y_min + 1)

        # Get string height in draw area.
        sh = blf.dimensions(0, string.printable)[1]
        x = origin_x - region.x
        y = origin_y - region.y

        # Draw draw area based background.
        if show_draw_area_background(prefs):
            draw_rect(draw_area_min_x - region.x,
                      draw_area_min_y - region.y,
                      draw_area_max_x - region.x,
                      draw_area_max_y - region.y,
                      prefs.background_color)

        # Draw last operator.
        operator_history = cls.removed_old_operator_history()
        if prefs.show_last_operator:
            if operator_history:
                time_, bl_label, idname_py, _ = operator_history[-1]
                if current_time - time_ <= prefs.display_time:
                    compat.set_blf_font_color(font_id, *prefs.color, 1.0)

                    # Draw operator text.
                    text = bpy.app.translations.pgettext_iface(bl_label, "Operator")
                    text += " ('{}')".format(idname_py)
                    offset_x, offset_y = cls.get_text_offset_for_alignment(font_id, text, context)
                    blf.position(font_id, x + offset_x, y + offset_y, 0)
                    if show_text_background(prefs):
                        draw_text_background(text, font_id, x + offset_x, y + offset_y, prefs.background_color)
                    draw_text(text, font_id, prefs.color, prefs.shadow, prefs.shadow_color)
                    y += sh + sh * cls.HEIGHT_RATIO_FOR_SEPARATOR * 0.2

                    # Draw separator.
                    sw = blf.dimensions(font_id, "Left Mouse")[0]
                    offset_x, offset_y = cls.get_offset_for_alignment(sw, context)
                    draw_line([x + offset_x, y + offset_y],
                              [x + sw + offset_x, y + offset_y],
                              prefs.color, prefs.shadow, prefs.shadow_color)
                    y += sh * cls.HEIGHT_RATIO_FOR_SEPARATOR * 0.8

                    region_drawn = True
                else:   # if current_time - time_ <= prefs.display_time:
                    y += sh + sh * cls.HEIGHT_RATIO_FOR_SEPARATOR
            else:   # if operator_history:
                y += sh + sh * cls.HEIGHT_RATIO_FOR_SEPARATOR

        # Draw hold modifier keys.
        drawing = False     # TODO: Need to check if drawing is now on progress.
        compat.set_blf_font_color(font_id, *prefs.color, 1.0)
        margin = sh * cls.MARGIN_RATIO_FOR_HOLD_MODIFIER_KEYS_BOX

        # Calculate width which includes mouse size
        text_and_mouse_width = 0
        modifier_keys_text = ""
        if cls.hold_modifier_keys or drawing:
            mod_keys = cls.sorted_modifier_keys(cls.hold_modifier_keys)
            if drawing:
                modifier_keys_text = ""
            else:
                modifier_keys_text = " + ".join(mod_keys)
            text_and_mouse_width = blf.dimensions(font_id, modifier_keys_text)[0] + margin * 2

        mouse_hold_status_width = 0.0
        mouse_hold_status_height = 0.0
        offset_x_for_hold_modifier_keys = 0
        offset_y_for_hold_modifier_keys = 0
        if show_mouse_hold_status(prefs):
            mouse_hold_status_width = prefs.mouse_size
            mouse_hold_status_height = prefs.mouse_size * cls.HEIGHT_RATIO_FOR_MOUSE_HOLD_STATUS
            offset_x_for_hold_modifier_keys = mouse_hold_status_width + \
                mouse_hold_status_width * \
                    cls.WIDTH_RATIO_FOR_SEPARATOR_BETWEEN_MOUSE_AND_MODIFIER_KEYS
            offset_y_for_hold_modifier_keys = (mouse_hold_status_height - sh) / 2
            text_and_mouse_width += mouse_hold_status_width
            if cls.hold_modifier_keys:
                text_and_mouse_width += mouse_hold_status_width * \
                    cls.WIDTH_RATIO_FOR_SEPARATOR_BETWEEN_MOUSE_AND_MODIFIER_KEYS
        offset_x, offset_y = cls.get_offset_for_alignment(text_and_mouse_width, context)

        # Draw hold mouse status.
        if show_mouse_hold_status(prefs):
            draw_mouse(x + offset_x, y + offset_y,
                       mouse_hold_status_width, mouse_hold_status_height,
                       cls.hold_mouse_buttons['LEFTMOUSE'],
                       cls.hold_mouse_buttons['RIGHTMOUSE'],
                       cls.hold_mouse_buttons['MIDDLEMOUSE'],
                       prefs.color,
                       prefs.mouse_size * 0.5,
                       fill=prefs.background,
                       fill_color=prefs.background_color)

        # Draw hold modifier keys.
        if cls.hold_modifier_keys or drawing:
            # Draw rounded box.
            box_height = sh + margin * 2
            box_width = blf.dimensions(font_id, modifier_keys_text)[0] + margin * 2
            draw_rounded_box(x + offset_x + offset_x_for_hold_modifier_keys,
                             y + offset_y - margin + offset_y_for_hold_modifier_keys,
                             box_width, box_height, box_height * 0.2,
                             show_text_background(prefs),
                             prefs.background_color if show_text_background(prefs) else prefs.color)

            # Draw modifier key text.
            blf.position(font_id,
                         x + margin + offset_x + offset_x_for_hold_modifier_keys,
                         y + margin + offset_y + offset_y_for_hold_modifier_keys, 0)
            draw_text(modifier_keys_text, font_id, prefs.color, prefs.shadow, prefs.shadow_color)
            bgl.glColor4f(*prefs.color, 1.0)

            region_drawn = True

        if mouse_hold_status_height > sh:
            y += mouse_hold_status_height + margin * 2
        else:
            y += sh + margin * 2

        # Draw event history.
        event_history = cls.removed_old_event_history()
        y += sh * cls.HEIGHT_RATIO_FOR_SEPARATOR
        for _, event_type, modifiers, repeat_count in event_history[::-1]:
            color = prefs.color
            compat.set_blf_font_color(font_id, *color, 1.0)

            text = get_display_event_text(event_type.name)
            if modifiers:
                mod_keys = cls.sorted_modifier_keys(modifiers)
                text = "{} + {}".format(" + ".join(mod_keys), text)
            if repeat_count > 1:
                text += " x{}".format(repeat_count)

            offset_x, offset_y = cls.get_text_offset_for_alignment(font_id, text, context)
            blf.position(font_id, x + offset_x, y + offset_y, 0)
            if show_text_background(prefs):
                draw_text_background(text, font_id, x + offset_x, y + offset_y, prefs.background_color)
            draw_text(text, font_id, prefs.color, prefs.shadow, prefs.shadow_color)

            y += sh

            region_drawn = True

        bgl.glDisable(bgl.GL_BLEND)
        bgl.glScissor(*scissor_box)
        bgl.glLineWidth(1.0)

        if region_drawn:
            cls.draw_regions_prev.add(region.as_pointer())

    @staticmethod
    @bpy.app.handlers.persistent
    def sort_modalhandlers(scene):
        """Sort modalhandlers registered on wmWindow.
           This makes SK_OT_ScreencastKeys.model method enable to get events
           consumed by other modalhandlers."""

        prefs = compat.get_user_preferences(bpy.context).addons[__package__].preferences
        if not prefs.get_event_aggressively:
            return

        for w in bpy.context.window_manager.windows:
            window = cast(c_void_p(w.as_pointer()),
                    POINTER(c_structures.wmWindow)).contents
            handler_ptr = cast(window.modalhandlers.first,
                            POINTER(c_structures.wmEventHandler))
            indices = []
            i = 0
            debug_print("====== HANDLER_LIST ======")
            has_ui_handler = False
            while handler_ptr:
                handler = handler_ptr.contents
                if compat.check_version(2, 80, 0) >= 0:
                    if handler.type == c_structures.eWM_EventHandlerType.WM_HANDLER_TYPE_OP:
                        op = handler.op.contents
                        idname = op.idname.decode()
                        op_prefix, op_name = idname.split("_OT_")
                        idname_py = "{}.{}".format(op_prefix.lower(), op_name)
                        if idname_py == SK_OT_ScreencastKeys.bl_idname:
                            indices.append(i)
                        debug_print("  TYPE: WM_HANDLER_TYPE_OP ({})".format(idname_py))
                    elif handler.type == c_structures.eWM_EventHandlerType.WM_HANDLER_TYPE_UI:
                        has_ui_handler = True
                        debug_print("  TYPE: WM_HANDLER_TYPE_UI")
                    else:
                        debug_print("  TYPE: {}".format(handler.type))
                else:
                    if handler.op:
                        op = handler.op.contents
                        idname = op.idname.decode()
                        op_prefix, op_name = idname.split("_OT_")
                        idname_py = "{}.{}".format(op_prefix.lower(), op_name)
                        if idname_py == SK_OT_ScreencastKeys.bl_idname:
                            indices.append(i)
                handler_ptr = cast(handler.next,
                                POINTER(c_structures.wmEventHandler))
                i += 1
            debug_print("==========================")

            # Blender will crash when we change the space type while Screencast Key is running.
            # This issue is caused by changing order of WM_HANDLER_TYPE_UI handler.
            # So, do nothing if there is a WM_HANDLER_TYPE_UI handler.
            # TODO: Sort only WM_HANDLER_TYPE_OP handlers.
            if has_ui_handler:
                return

            if indices:
                handlers = window.modalhandlers
                for count, index in enumerate(indices):
                    if index != count:
                        prev = handlers.find(index - 2)
                        handler = handlers.find(index)
                        handlers.remove(handler)
                        handlers.insert_after(prev, handler)

    def update_hold_modifier_keys(self, event):
        """Update hold modifier keys."""

        self.hold_modifier_keys.clear()

        if event.shift:
            self.hold_modifier_keys.append(EventType.LEFT_SHIFT)
        if event.oskey:
            self.hold_modifier_keys.append(EventType.OSKEY)
        if event.alt:
            self.hold_modifier_keys.append(EventType.LEFT_ALT)
        if event.ctrl:
            self.hold_modifier_keys.append(EventType.LEFT_CTRL)

        if EventType[event.type] == EventType.WINDOW_DEACTIVATE:
            self.hold_modifier_keys.clear()

    def update_hold_mouse_buttons(self, event):
        """Update hold mouse buttons."""

        if (event.type != 'MOUSEMOVE') and (event.type not in self.hold_mouse_buttons.keys()):
            return

        if event.type == 'MOUSEMOVE':
            if event.value == 'RELEASE':
                for k in self.hold_mouse_buttons.keys():
                    self.hold_mouse_buttons[k] = False
                return

        if event.value == 'PRESS':
            self.hold_mouse_buttons[event.type] = True
        elif event.value == 'RELEASE':
            self.hold_mouse_buttons[event.type] = False

    def is_ignore_event(self, event, prefs=None):
        """Return True if event will not be shown."""

        event_type = EventType[event.type]
        if event_type in {EventType.NONE, EventType.MOUSEMOVE,
                          EventType.INBETWEEN_MOUSEMOVE,
                          EventType.WINDOW_DEACTIVATE, EventType.TEXTINPUT}:
            return True
        elif (prefs is not None
              and not show_mouse_event_history(prefs)
              and event_type in self.MOUSE_EVENT_TYPES):
            return True
        elif event_type.name.startswith("EVT_TWEAK"):
            return True
        elif event_type.name.startswith("TIMER"):
            return True

        return False

    def is_modifier_event(self, event):
        """Return True if event came from modifier key."""

        event_type = EventType[event.type]
        return event_type in self.MODIFIER_EVENT_TYPES

    def modal(self, context, event):
        prefs = compat.get_user_preferences(context).addons[__package__].preferences

        if not self.__class__.is_running():
            return {'FINISHED'}

        if event.type == '':
            # Many events that should be identified as 'NONE', instead are
            # identified as '' and raise KeyErrors in EventType
            # (i.e. caps lock and the spin tool in edit mode)
            return {'PASS_THROUGH'}

        if event.type == 'MOUSEMOVE':
            self.__class__.current_mouse_co = [event.mouse_x, event.mouse_y]

        event_type = EventType[event.type]

        current_time = time.time()

        # Update Area - Space mapping.
        for area in context.screen.areas:
            for space in area.spaces:
                self.area_spaces[area.as_pointer()].add(space.as_pointer())

        # Update hold modifiers keys.
        self.update_hold_modifier_keys(event)
        current_mod_keys = self.hold_modifier_keys.copy()
        if event_type in current_mod_keys:
            # Remove modifier key which is just pressed.
            current_mod_keys.remove(event_type)

        # Update hold mouse buttons.
        self.update_hold_mouse_buttons(event)

        # Update event history.
        if (not self.is_ignore_event(event, prefs=prefs) and
                not self.is_modifier_event(event) and
                event.value == 'PRESS'):
            last_event = self.event_history[-1] if self.event_history else None
            current_event = [current_time, event_type, current_mod_keys, 1]

            # If this event has same event_type and modifiers, we increment
            # repeat_count. However, we reset repeat_count if event interval
            # overs display time.
            if (last_event and
                    last_event[1:-1] == current_event[1:-1] and
                    current_time - last_event[0] < prefs.display_time):
                last_event[0] = current_time
                last_event[-1] += 1
            else:
                self.event_history.append(current_event)
        self.event_history[:] = self.removed_old_event_history()

        # Update operator history.
        operators = list(context.window_manager.operators)
        if operators:
            # Find last operator which detects in previous modal call.
            if self.operator_history:
                addr = self.operator_history[-1][-1]
            else:
                addr = None
            prev_last_op_index = 0
            for i, op in enumerate(operators[::-1]):
                if op.as_pointer() == addr:
                    prev_last_op_index = len(operators) - i
                    break
            
            # Add operators to history.
            for op in operators[prev_last_op_index:]:
                op_prefix, op_name = op.bl_idname.split("_OT_")
                idname_py = "{}.{}".format(op_prefix.lower(), op_name)
                self.operator_history.append(
                    [current_time, op.bl_label, idname_py, op.as_pointer()])
        self.operator_history[:] = self.removed_old_operator_history()

        # Redraw regions which we want.
        prev_time = self.prev_time
        if (not self.is_ignore_event(event, prefs=prefs) or
                prev_time and current_time - prev_time >= self.TIMER_STEP):
            regions = self.find_redraw_regions(context)

            # If regions which are drawn at previous time, is not draw target
            # at this time, we don't need to redraw anymore.
            # But we raise redraw notification to make sure there are no
            # updates on their regions.
            # If there is update on the region, it will be added to
            # self.draw_regions_prev in draw_callback function.
            for area in context.screen.areas:
                for region in area.regions:
                    if region.as_pointer() in self.draw_regions_prev:
                        region.tag_redraw()
                        self.draw_regions_prev.remove(region.as_pointer())

            # Redraw all target regions.
            # If there is no draw handler attached to the region, we add it to.
            for area, region in regions:
                space_type = self.SPACE_TYPES[area.type]
                handler_key = (space_type, region.type)
                if handler_key not in self.handlers:
                    self.handlers[handler_key] = space_type.draw_handler_add(
                        self.draw_callback, (context, ), region.type,
                        'POST_PIXEL')
                region.tag_redraw()
                self.draw_regions_prev.add(region.as_pointer())

            self.__class__.prev_time = current_time

        return {'PASS_THROUGH'}

    @classmethod
    def draw_handler_remove_all(cls):
        for (space_type, region_type), handle in cls.handlers.items():
            space_type.draw_handler_remove(handle, region_type)
        cls.handlers.clear()

    @classmethod
    def event_timer_add(cls, context):
        wm = context.window_manager

        # Add timer to all windows.
        for window in wm.windows:
            key = window.as_pointer()
            if key not in cls.timers:
                cls.timers[key] = wm.event_timer_add(cls.TIMER_STEP,
                                                     window=window)

    @classmethod
    def event_timer_remove(cls, context):
        wm = context.window_manager

        # Delete timer from all windows.
        for win in wm.windows:
            key = win.as_pointer()
            if key in cls.timers:
                wm.event_timer_remove(cls.timers[key])
        cls.timers.clear()

    def invoke(self, context, event):
        cls = self.__class__
        prefs = compat.get_user_preferences(context).addons[__package__].preferences

        if cls.is_running():
            if compat.check_version(2, 80, 0) >= 0:
                if cls.sort_modalhandlers in bpy.app.handlers.depsgraph_update_pre:
                    bpy.app.handlers.depsgraph_update_pre.remove(cls.sort_modalhandlers)
            else:
                if cls.sort_modalhandlers in bpy.app.handlers.scene_update_pre:
                    bpy.app.handlers.scene_update_pre.remove(cls.sort_modalhandlers)
            self.event_timer_remove(context)
            self.draw_handler_remove_all()
            self.hold_modifier_keys.clear()
            self.event_history.clear()
            self.operator_history.clear()
            self.draw_regions_prev.clear()
            context.area.tag_redraw()
            cls.running = False
            return {'CANCELLED'}
        else:
            self.update_hold_modifier_keys(event)
            self.event_timer_add(context)
            context.window_manager.modal_handler_add(self)
            self.origin["window"] = context.window.as_pointer()
            self.origin["area"] = context.area.as_pointer()
            self.origin["space"] = context.space_data.as_pointer()
            self.origin["region_type"] = context.region.type
            context.area.tag_redraw()
            if prefs.get_event_aggressively:
                if compat.check_version(2, 80, 0) >= 0:
                    bpy.app.handlers.depsgraph_update_pre.append(cls.sort_modalhandlers)
                else:
                    bpy.app.handlers.scene_update_pre.append(cls.sort_modalhandlers)
            cls.running = True
            return {'RUNNING_MODAL'}


@BlClassRegistry()
class SK_OT_SetOrigin(bpy.types.Operator):
    bl_idname = "wm.sk_set_origin"
    bl_label = "Screencast Keys Set Origin"
    bl_description = ""
    bl_options = {'REGISTER'}

    # Draw handlers.
    # Format: {(Space, Region.type): handle}
    handlers = {}

    # Previous mouseovered area.
    area_prev = None

    # Mouseovered region.
    mouseovered_region = None

    def draw_callback(self, context):
        region = context.region
        if region and region == self.mouseovered_region:
            bgl.glEnable(bgl.GL_BLEND)
            bgl.glColor4f(1.0, 0.0, 0.0, 0.3)
            bgl.glRecti(0, 0, region.width, region.height)
            bgl.glDisable(bgl.GL_BLEND)
            bgl.glColor4f(1.0, 1.0, 1.0, 1.0)

    def draw_handler_add(self, context):
        for area in context.screen.areas:
            space_type = SK_OT_ScreencastKeys.SPACE_TYPES[area.type]
            for region in area.regions:
                if region.type == "":
                    continue
                key = (space_type, region.type)
                if key not in self.handlers:
                    handle = space_type.draw_handler_add(
                        self.draw_callback, (context,), region.type,
                        'POST_PIXEL')
                    self.handlers[key] = handle

    def draw_handler_remove_all(self):
        for (space_type, region_type), handle in self.handlers.items():
            space_type.draw_handler_remove(handle, region_type)
        self.handlers.clear()

    def get_mouseovered_region(self, context, event):
        """Get mouseovered area and region."""

        x, y = event.mouse_x, event.mouse_y
        for area in context.screen.areas:
            for region in area.regions:
                if region.type == "":
                    continue
                if ((region.x <= x < region.x + region.width) and
                        (region.y <= y < region.y + region.height)):
                    return area, region

        return None, None

    def modal(self, context, event):
        area, region = self.get_mouseovered_region(context, event)

        # Redraw previous mouseovered area.
        if self.area_prev:
            self.area_prev.tag_redraw()

        if area:
            area.tag_redraw()

        self.mouseovered_region = region
        self.area_prev = area

        if event.type in {'LEFTMOUSE', 'SPACE', 'RET', 'NUMPAD_ENTER'}:
            if event.value == 'PRESS':
                # Set origin.
                origin = SK_OT_ScreencastKeys.origin
                origin["window"] = context.window.as_pointer()
                origin["area"] = area.as_pointer()
                origin["space"] = area.spaces.active.as_pointer()
                origin["region_type"] = region.type
                self.draw_handler_remove_all()
                return {'FINISHED'}
        elif event.type in {'RIGHTMOUSE', 'ESC'}:
            # Canceled.
            self.draw_handler_remove_all()
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        self.area_prev = None
        self.mouseovered_region = None
        self.draw_handler_add(context)
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}
