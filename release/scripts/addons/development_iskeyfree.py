# ##### BEGIN GPL LICENSE BLOCK #####
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
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

# PEP8 compliant (https://www.python.org/dev/peps/pep-0008)

bl_info = {
    "name": "Is key Free",
    "author": "Antonio Vazquez (antonioya)",
    "version": (1, 0, 2),
    "blender": (2, 6, 9),
    "location": "Text Editor > Props Shelf (Ctrl/t > IsKeyFree Tools",
    "description": "Find free shortcuts and inform of used keys",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6"
                "/Py/Scripts/Development/IsKeyFree",
    "category": "Development"}

import bpy
from bpy.props import (
        StringProperty,
        BoolProperty,
        EnumProperty,
        PointerProperty,
        )
from bpy.types import (
        Operator,
        Panel,
        PropertyGroup,
        )


# ------------------------------------------------------
# Class to find keymaps
# ------------------------------------------------------


class MyChecker():
    lastfind = None
    lastkey = None
    mylist = []

    # Init
    def __init__(self):
        self.var = 5

    # Verify if the key is used
    @classmethod
    def check(cls, findkey, ctrl, alt, shift, oskey):
        if len(findkey) > 0:
            cmd = ""
            if ctrl is True:
                cmd += "Ctrl+"
            if alt is True:
                cmd += "Alt+"
            if shift is True:
                cmd += "Shift+"
            if oskey is True:
                cmd += "OsKey+"
            cls.lastfind = cmd + findkey.upper()
            cls.lastkey = findkey.upper()
        else:
            cls.lastfind = None
            cls.lastkey = None

        wm = bpy.context.window_manager
        mykeys = []

        for context, keyboardmap in wm.keyconfigs.user.keymaps.items():
            for myitem in keyboardmap.keymap_items:
                if myitem.active is True and myitem.type == findkey:
                    if ctrl is True and myitem.ctrl is not True:
                        continue
                    if alt is True and myitem.alt is not True:
                        continue
                    if shift is True and myitem.shift is not True:
                        continue
                    if oskey is True and myitem.oskey is not True:
                        continue
                    t = (context,
                         myitem.type,
                         "Ctrl" if myitem.ctrl is True else "",
                         "Alt" if myitem.alt is True else "",
                         "Shift" if myitem.shift is True else "",
                         "OsKey" if myitem.oskey is True else "",
                         myitem.name)

                    mykeys.append(t)

        sortkeys = sorted(mykeys, key=lambda key: (key[0], key[1], key[2], key[3], key[4], key[5]))

        cls.mylist.clear()
        for e in sortkeys:
            cmd = ""
            if e[2] is not "":
                cmd += e[2] + "+"
            if e[3] is not "":
                cmd += e[3] + "+"
            if e[4] is not "":
                cmd += e[4] + "+"
            if e[5] is not "":
                cmd += e[5] + "+"

            cmd += e[1]

            if e[6] is not "":
                cmd += "  " + e[6]
            cls.mylist.append([e[0], cmd])

    # return context
    @classmethod
    def getcontext(cls):
        return str(bpy.context.screen.name)

    # return last search
    @classmethod
    def getlast(cls):
        return cls.lastfind

    # return last key
    @classmethod
    def getlastkey(cls):
        return cls.lastkey

    # return result of last search
    @classmethod
    def getlist(cls):
        return cls.mylist

    # verify if key is valid
    @classmethod
    def isvalidkey(cls, txt):
        allkeys = [
            "LEFTMOUSE", "MIDDLEMOUSE", "RIGHTMOUSE", "BUTTON4MOUSE", "BUTTON5MOUSE", "BUTTON6MOUSE",
            "BUTTON7MOUSE",
            "ACTIONMOUSE", "SELECTMOUSE", "MOUSEMOVE", "INBETWEEN_MOUSEMOVE", "TRACKPADPAN", "TRACKPADZOOM",
            "MOUSEROTATE", "WHEELUPMOUSE", "WHEELDOWNMOUSE", "WHEELINMOUSE", "WHEELOUTMOUSE", "EVT_TWEAK_L",
            "EVT_TWEAK_M", "EVT_TWEAK_R", "EVT_TWEAK_A", "EVT_TWEAK_S", "A", "B", "C", "D", "E", "F", "G", "H",
            "I", "J",
            "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "ZERO", "ONE", "TWO",
            "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "LEFT_CTRL", "LEFT_ALT", "LEFT_SHIFT",
            "RIGHT_ALT",
            "RIGHT_CTRL", "RIGHT_SHIFT", "OSKEY", "GRLESS", "ESC", "TAB", "RET", "SPACE", "LINE_FEED",
            "BACK_SPACE",
            "DEL", "SEMI_COLON", "PERIOD", "COMMA", "QUOTE", "ACCENT_GRAVE", "MINUS", "SLASH", "BACK_SLASH",
            "EQUAL",
            "LEFT_BRACKET", "RIGHT_BRACKET", "LEFT_ARROW", "DOWN_ARROW", "RIGHT_ARROW", "UP_ARROW", "NUMPAD_2",
            "NUMPAD_4", "NUMPAD_6", "NUMPAD_8", "NUMPAD_1", "NUMPAD_3", "NUMPAD_5", "NUMPAD_7", "NUMPAD_9",
            "NUMPAD_PERIOD", "NUMPAD_SLASH", "NUMPAD_ASTERIX", "NUMPAD_0", "NUMPAD_MINUS", "NUMPAD_ENTER",
            "NUMPAD_PLUS",
            "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15",
            "F16", "F17",
            "F18", "F19", "PAUSE", "INSERT", "HOME", "PAGE_UP", "PAGE_DOWN", "END", "MEDIA_PLAY", "MEDIA_STOP",
            "MEDIA_FIRST", "MEDIA_LAST", "TEXTINPUT", "WINDOW_DEACTIVATE", "TIMER", "TIMER0", "TIMER1", "TIMER2",
            "TIMER_JOBS", "TIMER_AUTOSAVE", "TIMER_REPORT", "TIMERREGION", "NDOF_MOTION", "NDOF_BUTTON_MENU",
            "NDOF_BUTTON_FIT", "NDOF_BUTTON_TOP", "NDOF_BUTTON_BOTTOM", "NDOF_BUTTON_LEFT", "NDOF_BUTTON_RIGHT",
            "NDOF_BUTTON_FRONT", "NDOF_BUTTON_BACK", "NDOF_BUTTON_ISO1", "NDOF_BUTTON_ISO2",
            "NDOF_BUTTON_ROLL_CW",
            "NDOF_BUTTON_ROLL_CCW", "NDOF_BUTTON_SPIN_CW", "NDOF_BUTTON_SPIN_CCW", "NDOF_BUTTON_TILT_CW",
            "NDOF_BUTTON_TILT_CCW", "NDOF_BUTTON_ROTATE", "NDOF_BUTTON_PANZOOM", "NDOF_BUTTON_DOMINANT",
            "NDOF_BUTTON_PLUS", "NDOF_BUTTON_MINUS", "NDOF_BUTTON_ESC", "NDOF_BUTTON_ALT", "NDOF_BUTTON_SHIFT",
            "NDOF_BUTTON_CTRL", "NDOF_BUTTON_1", "NDOF_BUTTON_2", "NDOF_BUTTON_3", "NDOF_BUTTON_4",
            "NDOF_BUTTON_5",
            "NDOF_BUTTON_6", "NDOF_BUTTON_7", "NDOF_BUTTON_8", "NDOF_BUTTON_9", "NDOF_BUTTON_10",
            "NDOF_BUTTON_A",
            "NDOF_BUTTON_B", "NDOF_BUTTON_C"
            ]
        try:
            allkeys.index(txt)
            return True
        except ValueError:
            return False


mychecker = MyChecker()  # Global class handler


# ------------------------------------------------------
# Button: Class for search button
# ------------------------------------------------------


class RunActionCheck(Operator):
    bl_idname = "iskeyfree.action_check"
    bl_label = ""
    bl_description = "Verify if the selected shortcut is free"

    # ------------------------------
    # Execute
    # ------------------------------
    # noinspection PyUnusedLocal
    def execute(self, context):
        scene = context.scene.is_keyfree
        txt = scene.data.upper()
        global mychecker
        mychecker.check(txt, scene.use_crtl, scene.use_alt, scene.use_shift,
                        scene.use_oskey)

        return {'FINISHED'}


# ------------------------------------------------------
# Defines UI panel
# ------------------------------------------------------
class UIControlPanel(Panel):
    bl_space_type = "TEXT_EDITOR"
    bl_region_type = "UI"
    bl_label = "Is Key Free"

    # noinspection PyUnusedLocal
    def draw(self, context):
        layout = self.layout
        scene = context.scene.is_keyfree

        row = layout.row(align=True)
        row.prop(scene, "data")
        row.operator("iskeyfree.action_check", icon="VIEWZOOM")

        row = layout.row(align=True)
        row.prop(scene, "use_crtl", toggle=True)
        row.prop(scene, "use_alt", toggle=True)
        row.prop(scene, "use_shift", toggle=True)
        row.prop(scene, "use_oskey", toggle=True)

        row = layout.row()
        row.prop(scene, "numpad")

        global mychecker
        mylist = mychecker.getlist()
        oldcontext = None

        box = None
        if len(mylist) > 0:
            cmd = mychecker.getlast()
            if cmd is not None:
                row = layout.row()
                row.label("Current uses of " + str(cmd), icon="PARTICLE_DATA")
            for e in mylist:
                if oldcontext != e[0]:
                    box = layout.box()
                    box.label(e[0], icon="UNPINNED")
                    oldcontext = e[0]

                row = box.row(align=True)
                row.label(e[1])
        else:
            cmd = mychecker.getlast()
            if cmd is not None:
                box = layout.box()
                if mychecker.isvalidkey(mychecker.getlastkey()) is False:
                    box.label(str(mychecker.getlastkey()) + " looks not valid key", icon="ERROR")
                else:
                    box.label(str(cmd) + " is free", icon="FILE_TICK")


# ------------------------------------------------------
# Update key (special values) event handler
# ------------------------------------------------------


# noinspection PyUnusedLocal
def update_data(self, context):
    scene = context.scene.is_keyfree
    if scene.numpad != "NONE":
        scene.data = scene.numpad


class IskeyFreeProperties(PropertyGroup):
    data = StringProperty(
                    name="Key", maxlen=32,
                    description="Shortcut to verify"
                    )
    use_crtl = BoolProperty(
                    name="Ctrl",
                    description="Ctrl key used in shortcut",
                    default=False
                    )
    use_alt = BoolProperty(
                    name="Alt",
                    description="Alt key used in shortcut",
                    default=False
                    )
    use_shift = BoolProperty(
                    name="Shift",
                    description="Shift key used in shortcut",
                    default=False
                    )
    use_oskey = BoolProperty(
                    name="OsKey",
                    description="Operating system key used in shortcut",
                    default=False
                    )
    numpad = EnumProperty(
                    items=(('NONE', "Select key", ""),
                       ("LEFTMOUSE", "LEFTMOUSE", ""),
                       ("MIDDLEMOUSE", "MIDDLEMOUSE", ""),
                       ("RIGHTMOUSE", "RIGHTMOUSE", ""),
                       ("BUTTON4MOUSE", "BUTTON4MOUSE", ""),
                       ("BUTTON5MOUSE", "BUTTON5MOUSE", ""),
                       ("BUTTON6MOUSE", "BUTTON6MOUSE", ""),
                       ("BUTTON7MOUSE", "BUTTON7MOUSE", ""),
                       ("ACTIONMOUSE", "ACTIONMOUSE", ""),
                       ("SELECTMOUSE", "SELECTMOUSE", ""),
                       ("MOUSEMOVE", "MOUSEMOVE", ""),
                       ("INBETWEEN_MOUSEMOVE", "INBETWEEN_MOUSEMOVE", ""),
                       ("TRACKPADPAN", "TRACKPADPAN", ""),
                       ("TRACKPADZOOM", "TRACKPADZOOM", ""),
                       ("MOUSEROTATE", "MOUSEROTATE", ""),
                       ("WHEELUPMOUSE", "WHEELUPMOUSE", ""),
                       ("WHEELDOWNMOUSE", "WHEELDOWNMOUSE", ""),
                       ("WHEELINMOUSE", "WHEELINMOUSE", ""),
                       ("WHEELOUTMOUSE", "WHEELOUTMOUSE", ""),
                       ("EVT_TWEAK_L", "EVT_TWEAK_L", ""),
                       ("EVT_TWEAK_M", "EVT_TWEAK_M", ""),
                       ("EVT_TWEAK_R", "EVT_TWEAK_R", ""),
                       ("EVT_TWEAK_A", "EVT_TWEAK_A", ""),
                       ("EVT_TWEAK_S", "EVT_TWEAK_S", ""),
                       ("A", "A", ""),
                       ("B", "B", ""),
                       ("C", "C", ""),
                       ("D", "D", ""),
                       ("E", "E", ""),
                       ("F", "F", ""),
                       ("G", "G", ""),
                       ("H", "H", ""),
                       ("I", "I", ""),
                       ("J", "J", ""),
                       ("K", "K", ""),
                       ("L", "L", ""),
                       ("M", "M", ""),
                       ("N", "N", ""),
                       ("O", "O", ""),
                       ("P", "P", ""),
                       ("Q", "Q", ""),
                       ("R", "R", ""),
                       ("S", "S", ""),
                       ("T", "T", ""),
                       ("U", "U", ""),
                       ("V", "V", ""),
                       ("W", "W", ""),
                       ("X", "X", ""),
                       ("Y", "Y", ""),
                       ("Z", "Z", ""),
                       ("ZERO", "ZERO", ""),
                       ("ONE", "ONE", ""),
                       ("TWO", "TWO", ""),
                       ("THREE", "THREE", ""),
                       ("FOUR", "FOUR", ""),
                       ("FIVE", "FIVE", ""),
                       ("SIX", "SIX", ""),
                       ("SEVEN", "SEVEN", ""),
                       ("EIGHT", "EIGHT", ""),
                       ("NINE", "NINE", ""),
                       ("LEFT_CTRL", "LEFT_CTRL", ""),
                       ("LEFT_ALT", "LEFT_ALT", ""),
                       ("LEFT_SHIFT", "LEFT_SHIFT", ""),
                       ("RIGHT_ALT", "RIGHT_ALT", ""),
                       ("RIGHT_CTRL", "RIGHT_CTRL", ""),
                       ("RIGHT_SHIFT", "RIGHT_SHIFT", ""),
                       ("OSKEY", "OSKEY", ""),
                       ("GRLESS", "GRLESS", ""),
                       ("ESC", "ESC", ""),
                       ("TAB", "TAB", ""),
                       ("RET", "RET", ""),
                       ("SPACE", "SPACE", ""),
                       ("LINE_FEED", "LINE_FEED", ""),
                       ("BACK_SPACE", "BACK_SPACE", ""),
                       ("DEL", "DEL", ""),
                       ("SEMI_COLON", "SEMI_COLON", ""),
                       ("PERIOD", "PERIOD", ""),
                       ("COMMA", "COMMA", ""),
                       ("QUOTE", "QUOTE", ""),
                       ("ACCENT_GRAVE", "ACCENT_GRAVE", ""),
                       ("MINUS", "MINUS", ""),
                       ("SLASH", "SLASH", ""),
                       ("BACK_SLASH", "BACK_SLASH", ""),
                       ("EQUAL", "EQUAL", ""),
                       ("LEFT_BRACKET", "LEFT_BRACKET", ""),
                       ("RIGHT_BRACKET", "RIGHT_BRACKET", ""),
                       ("LEFT_ARROW", "LEFT_ARROW", ""),
                       ("DOWN_ARROW", "DOWN_ARROW", ""),
                       ("RIGHT_ARROW", "RIGHT_ARROW", ""),
                       ("UP_ARROW", "UP_ARROW", ""),
                       ("NUMPAD_1", "NUMPAD_1", ""),
                       ("NUMPAD_2", "NUMPAD_2", ""),
                       ("NUMPAD_3", "NUMPAD_3", ""),
                       ("NUMPAD_4", "NUMPAD_4", ""),
                       ("NUMPAD_5", "NUMPAD_5", ""),
                       ("NUMPAD_6", "NUMPAD_6", ""),
                       ("NUMPAD_7", "NUMPAD_7", ""),
                       ("NUMPAD_8", "NUMPAD_8", ""),
                       ("NUMPAD_9", "NUMPAD_9", ""),
                       ("NUMPAD_0", "NUMPAD_0", ""),
                       ("NUMPAD_PERIOD", "NUMPAD_PERIOD", ""),
                       ("NUMPAD_SLASH", "NUMPAD_SLASH", ""),
                       ("NUMPAD_ASTERIX", "NUMPAD_ASTERIX", ""),
                       ("NUMPAD_MINUS", "NUMPAD_MINUS", ""),
                       ("NUMPAD_ENTER", "NUMPAD_ENTER", ""),
                       ("NUMPAD_PLUS", "NUMPAD_PLUS", ""),
                       ("F1", "F1", ""),
                       ("F2", "F2", ""),
                       ("F3", "F3", ""),
                       ("F4", "F4", ""),
                       ("F5", "F5", ""),
                       ("F6", "F6", ""),
                       ("F7", "F7", ""),
                       ("F8", "F8", ""),
                       ("F9", "F9", ""),
                       ("F10", "F10", ""),
                       ("F11", "F11", ""),
                       ("F12", "F12", ""),
                       ("F13", "F13", ""),
                       ("F14", "F14", ""),
                       ("F15", "F15", ""),
                       ("F16", "F16", ""),
                       ("F17", "F17", ""),
                       ("F18", "F18", ""),
                       ("F19", "F19", ""),
                       ("PAUSE", "PAUSE", ""),
                       ("INSERT", "INSERT", ""),
                       ("HOME", "HOME", ""),
                       ("PAGE_UP", "PAGE_UP", ""),
                       ("PAGE_DOWN", "PAGE_DOWN", ""),
                       ("END", "END", ""),
                       ("MEDIA_PLAY", "MEDIA_PLAY", ""),
                       ("MEDIA_STOP", "MEDIA_STOP", ""),
                       ("MEDIA_FIRST", "MEDIA_FIRST", ""),
                       ("MEDIA_LAST", "MEDIA_LAST", ""),
                       ("TEXTINPUT", "TEXTINPUT", ""),
                       ("WINDOW_DEACTIVATE", "WINDOW_DEACTIVATE", ""),
                       ("TIMER", "TIMER", ""),
                       ("TIMER0", "TIMER0", ""),
                       ("TIMER1", "TIMER1", ""),
                       ("TIMER2", "TIMER2", ""),
                       ("TIMER_JOBS", "TIMER_JOBS", ""),
                       ("TIMER_AUTOSAVE", "TIMER_AUTOSAVE", ""),
                       ("TIMER_REPORT", "TIMER_REPORT", ""),
                       ("TIMERREGION", "TIMERREGION", ""),
                       ("NDOF_MOTION", "NDOF_MOTION", ""),
                       ("NDOF_BUTTON_MENU", "NDOF_BUTTON_MENU", ""),
                       ("NDOF_BUTTON_FIT", "NDOF_BUTTON_FIT", ""),
                       ("NDOF_BUTTON_TOP", "NDOF_BUTTON_TOP", ""),
                       ("NDOF_BUTTON_BOTTOM", "NDOF_BUTTON_BOTTOM", ""),
                       ("NDOF_BUTTON_LEFT", "NDOF_BUTTON_LEFT", ""),
                       ("NDOF_BUTTON_RIGHT", "NDOF_BUTTON_RIGHT", ""),
                       ("NDOF_BUTTON_FRONT", "NDOF_BUTTON_FRONT", ""),
                       ("NDOF_BUTTON_BACK", "NDOF_BUTTON_BACK", ""),
                       ("NDOF_BUTTON_ISO1", "NDOF_BUTTON_ISO1", ""),
                       ("NDOF_BUTTON_ISO2", "NDOF_BUTTON_ISO2", ""),
                       ("NDOF_BUTTON_ROLL_CW", "NDOF_BUTTON_ROLL_CW", ""),
                       ("NDOF_BUTTON_ROLL_CCW", "NDOF_BUTTON_ROLL_CCW", ""),
                       ("NDOF_BUTTON_SPIN_CW", "NDOF_BUTTON_SPIN_CW", ""),
                       ("NDOF_BUTTON_SPIN_CCW", "NDOF_BUTTON_SPIN_CCW", ""),
                       ("NDOF_BUTTON_TILT_CW", "NDOF_BUTTON_TILT_CW", ""),
                       ("NDOF_BUTTON_TILT_CCW", "NDOF_BUTTON_TILT_CCW", ""),
                       ("NDOF_BUTTON_ROTATE", "NDOF_BUTTON_ROTATE", ""),
                       ("NDOF_BUTTON_PANZOOM", "NDOF_BUTTON_PANZOOM", ""),
                       ("NDOF_BUTTON_DOMINANT", "NDOF_BUTTON_DOMINANT", ""),
                       ("NDOF_BUTTON_PLUS", "NDOF_BUTTON_PLUS", ""),
                       ("NDOF_BUTTON_MINUS", "NDOF_BUTTON_MINUS", ""),
                       ("NDOF_BUTTON_ESC", "NDOF_BUTTON_ESC", ""),
                       ("NDOF_BUTTON_ALT", "NDOF_BUTTON_ALT", ""),
                       ("NDOF_BUTTON_SHIFT", "NDOF_BUTTON_SHIFT", ""),
                       ("NDOF_BUTTON_CTRL", "NDOF_BUTTON_CTRL", ""),
                       ("NDOF_BUTTON_1", "NDOF_BUTTON_1", ""),
                       ("NDOF_BUTTON_2", "NDOF_BUTTON_2", ""),
                       ("NDOF_BUTTON_3", "NDOF_BUTTON_3", ""),
                       ("NDOF_BUTTON_4", "NDOF_BUTTON_4", ""),
                       ("NDOF_BUTTON_5", "NDOF_BUTTON_5", ""),
                       ("NDOF_BUTTON_6", "NDOF_BUTTON_6", ""),
                       ("NDOF_BUTTON_7", "NDOF_BUTTON_7", ""),
                       ("NDOF_BUTTON_8", "NDOF_BUTTON_8", ""),
                       ("NDOF_BUTTON_9", "NDOF_BUTTON_9", ""),
                       ("NDOF_BUTTON_10", "NDOF_BUTTON_10", ""),
                       ("NDOF_BUTTON_A", "NDOF_BUTTON_A", ""),
                       ("NDOF_BUTTON_B", "NDOF_BUTTON_B", ""),
                       ("NDOF_BUTTON_C", "NDOF_BUTTON_C", "")
                    ),
                    name="Quick Type",
                    description="Enter key code in find text",
                    update=update_data
                    )


# -----------------------------------------------------
# Registration
# ------------------------------------------------------

def register():
    bpy.utils.register_module(__name__)
    bpy.types.Scene.is_keyfree = PointerProperty(type=IskeyFreeProperties)


def unregister():
    bpy.utils.unregister_module(__name__)
    del bpy.types.Scene.is_keyfree
