/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_wm.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME
static EnumPropertyItem event_keymouse_value_items[] = {
	{KM_ANY, "ANY", 0, "Any", ""},
	{KM_PRESS, "PRESS", 0, "Press", ""},
	{KM_RELEASE, "RELEASE", 0, "Release", ""},
	{KM_CLICK, "CLICK", 0, "Click", ""},
	{KM_DBL_CLICK, "DOUBLE_CLICK", 0, "Double Click", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem event_tweak_value_items[] = {
	{KM_ANY, "ANY", 0, "Any", ""},
	{EVT_GESTURE_N, "NORTH", 0, "North", ""},
	{EVT_GESTURE_NE, "NORTH_EAST", 0, "North-East", ""},
	{EVT_GESTURE_E, "EAST", 0, "East", ""},
	{EVT_GESTURE_SE, "SOUTH_EAST", 0, "South-East", ""},
	{EVT_GESTURE_S, "SOUTH", 0, "South", ""},
	{EVT_GESTURE_SW, "SOUTH_WEST", 0, "South-West", ""},
	{EVT_GESTURE_W, "WEST", 0, "West", ""},
	{EVT_GESTURE_NW, "NORTH_WEST", 0, "North-West", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem event_tweak_type_items[] = {
	{EVT_TWEAK_L, "EVT_TWEAK_L", 0, "Left", ""},
	{EVT_TWEAK_M, "EVT_TWEAK_M", 0, "Middle", ""},
	{EVT_TWEAK_R, "EVT_TWEAK_R", 0, "Right", ""},
	{EVT_TWEAK_A, "EVT_TWEAK_A", 0, "Action", ""},
	{EVT_TWEAK_S, "EVT_TWEAK_S", 0, "Select", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem event_mouse_type_items[] = {
	{LEFTMOUSE, "LEFTMOUSE", 0, "Left", ""},
	{MIDDLEMOUSE, "MIDDLEMOUSE", 0, "Middle", ""},
	{RIGHTMOUSE, "RIGHTMOUSE", 0, "Right", ""},
	{BUTTON4MOUSE, "BUTTON4MOUSE", 0, "Button4", ""},
	{BUTTON5MOUSE, "BUTTON5MOUSE", 0, "Button5", ""},
	{BUTTON6MOUSE, "BUTTON6MOUSE", 0, "Button6", ""},
	{BUTTON7MOUSE, "BUTTON7MOUSE", 0, "Button7", ""},
	{ACTIONMOUSE, "ACTIONMOUSE", 0, "Action", ""},
	{SELECTMOUSE, "SELECTMOUSE", 0, "Select", ""},
	{0, "", 0, NULL, NULL},
	{TABLET_STYLUS, "PEN", 0, "Pen", ""},
	{TABLET_ERASER, "ERASER", 0, "Eraser", ""},
	{0, "", 0, NULL, NULL},
	{MOUSEMOVE, "MOUSEMOVE", 0, "Move", ""},
	{MOUSEPAN, "TRACKPADPAN", 0, "Mouse/Trackpad Pan", ""},
	{MOUSEZOOM, "TRACKPADZOOM", 0, "Mouse/Trackpad Zoom", ""},
	{MOUSEROTATE, "MOUSEROTATE", 0, "Mouse/Trackpad Rotate", ""},
	{0, "", 0, NULL, NULL},
	{WHEELUPMOUSE, "WHEELUPMOUSE", 0, "Wheel Up", ""},
	{WHEELDOWNMOUSE, "WHEELDOWNMOUSE", 0, "Wheel Down", ""},
	{WHEELINMOUSE, "WHEELINMOUSE", 0, "Wheel In", ""},
	{WHEELOUTMOUSE, "WHEELOUTMOUSE", 0, "Wheel Out", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem event_timer_type_items[] = {
	{TIMER, "TIMER", 0, "Timer", ""},
	{TIMER0, "TIMER0", 0, "Timer 0", ""},
	{TIMER1, "TIMER1", 0, "Timer 1", ""},
	{TIMER2, "TIMER2", 0, "Timer 2", ""},
	{TIMERJOBS, "TIMER_JOBS", 0, "Timer Jobs", ""},
	{TIMERAUTOSAVE, "TIMER_AUTOSAVE", 0, "Timer Autosave", ""},
	{TIMERREPORT, "TIMER_REPORT", 0, "Timer Report", ""},
	{TIMERREGION, "TIMERREGION", 0, "Timer Region", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem event_textinput_type_items[] = {
	{KM_TEXTINPUT, "TEXTINPUT", 0, "Text Input", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem event_ndof_type_items[] = {
	{NDOF_MOTION, "NDOF_MOTION", 0, "Motion", ""},
	/* buttons on all 3dconnexion devices */
	{NDOF_BUTTON_MENU, "NDOF_BUTTON_MENU", 0, "Menu", ""},
	{NDOF_BUTTON_FIT, "NDOF_BUTTON_FIT", 0, "Fit", ""},
	/* view buttons */
	{NDOF_BUTTON_TOP, "NDOF_BUTTON_TOP", 0, "Top", ""},
	{NDOF_BUTTON_BOTTOM, "NDOF_BUTTON_BOTTOM", 0, "Bottom", ""},
	{NDOF_BUTTON_LEFT, "NDOF_BUTTON_LEFT", 0, "Left", ""},
	{NDOF_BUTTON_RIGHT, "NDOF_BUTTON_RIGHT", 0, "Right", ""},
	{NDOF_BUTTON_FRONT, "NDOF_BUTTON_FRONT", 0, "Front", ""},
	{NDOF_BUTTON_BACK, "NDOF_BUTTON_BACK", 0, "Back", ""},
	/* more views */
	{NDOF_BUTTON_ISO1, "NDOF_BUTTON_ISO1", 0, "Isometric 1", ""},
	{NDOF_BUTTON_ISO2, "NDOF_BUTTON_ISO2", 0, "Isometric 2", ""},
	/* 90 degree rotations */
	{NDOF_BUTTON_ROLL_CW, "NDOF_BUTTON_ROLL_CW", 0, "Roll CW", ""},
	{NDOF_BUTTON_ROLL_CCW, "NDOF_BUTTON_ROLL_CCW", 0, "Roll CCW", ""},
	{NDOF_BUTTON_SPIN_CW, "NDOF_BUTTON_SPIN_CW", 0, "Spin CW", ""},
	{NDOF_BUTTON_SPIN_CCW, "NDOF_BUTTON_SPIN_CCW", 0, "Spin CCW", ""},
	{NDOF_BUTTON_TILT_CW, "NDOF_BUTTON_TILT_CW", 0, "Tilt CW", ""},
	{NDOF_BUTTON_TILT_CCW, "NDOF_BUTTON_TILT_CCW", 0, "Tilt CCW", ""},
	/* device control */
	{NDOF_BUTTON_ROTATE, "NDOF_BUTTON_ROTATE", 0, "Rotate", ""},
	{NDOF_BUTTON_PANZOOM, "NDOF_BUTTON_PANZOOM", 0, "Pan/Zoom", ""},
	{NDOF_BUTTON_DOMINANT, "NDOF_BUTTON_DOMINANT", 0, "Dominant", ""},
	{NDOF_BUTTON_PLUS, "NDOF_BUTTON_PLUS", 0, "Plus", ""},
	{NDOF_BUTTON_MINUS, "NDOF_BUTTON_MINUS", 0, "Minus", ""},
	/* keyboard emulation */
	{NDOF_BUTTON_ESC, "NDOF_BUTTON_ESC", 0, "Esc"},
	{NDOF_BUTTON_ALT, "NDOF_BUTTON_ALT", 0, "Alt"},
	{NDOF_BUTTON_SHIFT, "NDOF_BUTTON_SHIFT", 0, "Shift"},
	{NDOF_BUTTON_CTRL, "NDOF_BUTTON_CTRL", 0, "Ctrl"},
	/* general-purpose buttons */
	{NDOF_BUTTON_1, "NDOF_BUTTON_1", 0, "Button 1", ""},
	{NDOF_BUTTON_2, "NDOF_BUTTON_2", 0, "Button 2", ""},
	{NDOF_BUTTON_3, "NDOF_BUTTON_3", 0, "Button 3", ""},
	{NDOF_BUTTON_4, "NDOF_BUTTON_4", 0, "Button 4", ""},
	{NDOF_BUTTON_5, "NDOF_BUTTON_5", 0, "Button 5", ""},
	{NDOF_BUTTON_6, "NDOF_BUTTON_6", 0, "Button 6", ""},
	{NDOF_BUTTON_7, "NDOF_BUTTON_7", 0, "Button 7", ""},
	{NDOF_BUTTON_8, "NDOF_BUTTON_8", 0, "Button 8", ""},
	{NDOF_BUTTON_9, "NDOF_BUTTON_9", 0, "Button 9", ""},
	{NDOF_BUTTON_10, "NDOF_BUTTON_10", 0, "Button 10", ""},
	{NDOF_BUTTON_A, "NDOF_BUTTON_A", 0, "Button A", ""},
	{NDOF_BUTTON_B, "NDOF_BUTTON_B", 0, "Button B", ""},
	{NDOF_BUTTON_C, "NDOF_BUTTON_C", 0, "Button C", ""},
	{0, NULL, 0, NULL, NULL}
};
#endif /* RNA_RUNTIME */

/* not returned: CAPSLOCKKEY, UNKNOWNKEY */
EnumPropertyItem rna_enum_event_type_items[] = {
	/* Note we abuse 'tooltip' message here to store a 'compact' form of some (too) long names. */
	{0, "NONE", 0, "", ""},
	{LEFTMOUSE, "LEFTMOUSE", 0, "Left Mouse", "LMB"},
	{MIDDLEMOUSE, "MIDDLEMOUSE", 0, "Middle Mouse", "MMB"},
	{RIGHTMOUSE, "RIGHTMOUSE", 0, "Right Mouse", "RMB"},
	{BUTTON4MOUSE, "BUTTON4MOUSE", 0, "Button4 Mouse", "MB4"},
	{BUTTON5MOUSE, "BUTTON5MOUSE", 0, "Button5 Mouse", "MB5"},
	{BUTTON6MOUSE, "BUTTON6MOUSE", 0, "Button6 Mouse", "MB6"},
	{BUTTON7MOUSE, "BUTTON7MOUSE", 0, "Button7 Mouse", "MB7"},
	{ACTIONMOUSE, "ACTIONMOUSE", 0, "Action Mouse", "MBA"},
	{SELECTMOUSE, "SELECTMOUSE", 0, "Select Mouse", "MBS"},
	{0, "", 0, NULL, NULL},
	{TABLET_STYLUS, "PEN", 0, "Pen", ""},
	{TABLET_ERASER, "ERASER", 0, "Eraser", ""},
	{0, "", 0, NULL, NULL},
	{MOUSEMOVE, "MOUSEMOVE", 0, "Mouse Move", "MsMov"},
	{INBETWEEN_MOUSEMOVE, "INBETWEEN_MOUSEMOVE", 0, "In-between Move", "MsSubMov"},
	{MOUSEPAN, "TRACKPADPAN", 0, "Mouse/Trackpad Pan", "MsPan"},
	{MOUSEZOOM, "TRACKPADZOOM", 0, "Mouse/Trackpad Zoom", "MsZoom"},
	{MOUSEROTATE, "MOUSEROTATE", 0, "Mouse/Trackpad Rotate", "MsRot"},
	{0, "", 0, NULL, NULL},
	{WHEELUPMOUSE, "WHEELUPMOUSE", 0, "Wheel Up", "WhUp"},
	{WHEELDOWNMOUSE, "WHEELDOWNMOUSE", 0, "Wheel Down", "WhDown"},
	{WHEELINMOUSE, "WHEELINMOUSE", 0, "Wheel In", "WhIn"},
	{WHEELOUTMOUSE, "WHEELOUTMOUSE", 0, "Wheel Out", "WhOut"},
	{0, "", 0, NULL, NULL},
	{EVT_TWEAK_L, "EVT_TWEAK_L", 0, "Tweak Left", "TwkL"},
	{EVT_TWEAK_M, "EVT_TWEAK_M", 0, "Tweak Middle", "TwkM"},
	{EVT_TWEAK_R, "EVT_TWEAK_R", 0, "Tweak Right", "TwkR"},
	{EVT_TWEAK_A, "EVT_TWEAK_A", 0, "Tweak Action", "TwkA"},
	{EVT_TWEAK_S, "EVT_TWEAK_S", 0, "Tweak Select", "TwkS"},
	{0, "", 0, NULL, NULL},
	{AKEY, "A", 0, "A", ""},
	{BKEY, "B", 0, "B", ""},
	{CKEY, "C", 0, "C", ""},
	{DKEY, "D", 0, "D", ""},
	{EKEY, "E", 0, "E", ""},
	{FKEY, "F", 0, "F", ""},
	{GKEY, "G", 0, "G", ""},
	{HKEY, "H", 0, "H", ""},
	{IKEY, "I", 0, "I", ""},
	{JKEY, "J", 0, "J", ""},
	{KKEY, "K", 0, "K", ""},
	{LKEY, "L", 0, "L", ""},
	{MKEY, "M", 0, "M", ""},
	{NKEY, "N", 0, "N", ""},
	{OKEY, "O", 0, "O", ""},
	{PKEY, "P", 0, "P", ""},
	{QKEY, "Q", 0, "Q", ""},
	{RKEY, "R", 0, "R", ""},
	{SKEY, "S", 0, "S", ""},
	{TKEY, "T", 0, "T", ""},
	{UKEY, "U", 0, "U", ""},
	{VKEY, "V", 0, "V", ""},
	{WKEY, "W", 0, "W", ""},
	{XKEY, "X", 0, "X", ""},
	{YKEY, "Y", 0, "Y", ""},
	{ZKEY, "Z", 0, "Z", ""},
	{0, "", 0, NULL, NULL},
	{ZEROKEY, "ZERO",   0, "0", ""},
	{ONEKEY, "ONE",     0, "1", ""},
	{TWOKEY, "TWO",     0, "2", ""},
	{THREEKEY, "THREE", 0, "3", ""},
	{FOURKEY, "FOUR",   0, "4", ""},
	{FIVEKEY, "FIVE",   0, "5", ""},
	{SIXKEY, "SIX",     0, "6", ""},
	{SEVENKEY, "SEVEN", 0, "7", ""},
	{EIGHTKEY, "EIGHT", 0, "8", ""},
	{NINEKEY, "NINE",   0, "9", ""},
	{0, "", 0, NULL, NULL},
	{LEFTCTRLKEY,   "LEFT_CTRL",    0, "Left Ctrl", "CtrlL"},
	{LEFTALTKEY,    "LEFT_ALT",     0, "Left Alt", "AltL"},
	{LEFTSHIFTKEY,  "LEFT_SHIFT",   0, "Left Shift", "ShiftL"},
	{RIGHTALTKEY,   "RIGHT_ALT",    0, "Right Alt", "AltR"},
	{RIGHTCTRLKEY,  "RIGHT_CTRL",   0, "Right Ctrl", "CtrlR"},
	{RIGHTSHIFTKEY, "RIGHT_SHIFT",  0, "Right Shift", "ShiftR"},
	{0, "", 0, NULL, NULL},
	{OSKEY, "OSKEY",    0, "OS Key", "Cmd"},
	{GRLESSKEY, "GRLESS",   0, "Grless", ""},
	{ESCKEY, "ESC", 0, "Esc", ""},
	{TABKEY, "TAB", 0, "Tab", ""},
	{RETKEY, "RET", 0, "Return", "Enter"},
	{SPACEKEY, "SPACE", 0, "Spacebar", "Space"},
	{LINEFEEDKEY, "LINE_FEED", 0, "Line Feed", ""},
	{BACKSPACEKEY, "BACK_SPACE", 0, "Back Space", "BkSpace"},
	{DELKEY, "DEL", 0, "Delete", "Del"},
	{SEMICOLONKEY, "SEMI_COLON", 0, ";", ""},
	{PERIODKEY, "PERIOD", 0, ".", ""},
	{COMMAKEY, "COMMA", 0, ",", ""},
	{QUOTEKEY, "QUOTE", 0, "\"", ""},
	{ACCENTGRAVEKEY, "ACCENT_GRAVE", 0, "`", ""},
	{MINUSKEY, "MINUS", 0, "-", ""},
	{PLUSKEY, "PLUS", 0, "+", ""},
	{SLASHKEY, "SLASH", 0, "/", ""},
	{BACKSLASHKEY, "BACK_SLASH", 0, "\\", ""},
	{EQUALKEY, "EQUAL", 0, "=", ""},
	{LEFTBRACKETKEY, "LEFT_BRACKET", 0, "[", ""},
	{RIGHTBRACKETKEY, "RIGHT_BRACKET", 0, "]", ""},
	{LEFTARROWKEY, "LEFT_ARROW", 0, "Left Arrow", "←"},
	{DOWNARROWKEY, "DOWN_ARROW", 0, "Down Arrow", "↓"},
	{RIGHTARROWKEY, "RIGHT_ARROW", 0, "Right Arrow", "→"},
	{UPARROWKEY, "UP_ARROW", 0, "Up Arrow", "↑"},
	{PAD2, "NUMPAD_2", 0, "Numpad 2", "Pad2"},
	{PAD4, "NUMPAD_4", 0, "Numpad 4", "Pad4"},
	{PAD6, "NUMPAD_6", 0, "Numpad 6", "Pad6"},
	{PAD8, "NUMPAD_8", 0, "Numpad 8", "Pad8"},
	{PAD1, "NUMPAD_1", 0, "Numpad 1", "Pad1"},
	{PAD3, "NUMPAD_3", 0, "Numpad 3", "Pad3"},
	{PAD5, "NUMPAD_5", 0, "Numpad 5", "Pad5"},
	{PAD7, "NUMPAD_7", 0, "Numpad 7", "Pad7"},
	{PAD9, "NUMPAD_9", 0, "Numpad 9", "Pad9"},
	{PADPERIOD, "NUMPAD_PERIOD", 0, "Numpad .", "Pad."},
	{PADSLASHKEY, "NUMPAD_SLASH", 0, "Numpad /", "Pad/"},
	{PADASTERKEY, "NUMPAD_ASTERIX", 0, "Numpad *", "Pad*"},
	{PAD0, "NUMPAD_0", 0, "Numpad 0", "Pad0"},
	{PADMINUS, "NUMPAD_MINUS", 0, "Numpad -", "Pad-"},
	{PADENTER, "NUMPAD_ENTER", 0, "Numpad Enter", "PadEnter"},
	{PADPLUSKEY, "NUMPAD_PLUS", 0, "Numpad +", "Pad+"},
	{F1KEY, "F1", 0, "F1", ""},
	{F2KEY, "F2", 0, "F2", ""},
	{F3KEY, "F3", 0, "F3", ""},
	{F4KEY, "F4", 0, "F4", ""},
	{F5KEY, "F5", 0, "F5", ""},
	{F6KEY, "F6", 0, "F6", ""},
	{F7KEY, "F7", 0, "F7", ""},
	{F8KEY, "F8", 0, "F8", ""},
	{F9KEY, "F9", 0, "F9", ""},
	{F10KEY, "F10", 0, "F10", ""},
	{F11KEY, "F11", 0, "F11", ""},
	{F12KEY, "F12", 0, "F12", ""},
	{F13KEY, "F13", 0, "F13", ""},
	{F14KEY, "F14", 0, "F14", ""},
	{F15KEY, "F15", 0, "F15", ""},
	{F16KEY, "F16", 0, "F16", ""},
	{F17KEY, "F17", 0, "F17", ""},
	{F18KEY, "F18", 0, "F18", ""},
	{F19KEY, "F19", 0, "F19", ""},
	{PAUSEKEY, "PAUSE", 0, "Pause", ""},
	{INSERTKEY, "INSERT", 0, "Insert", "Ins"},
	{HOMEKEY, "HOME", 0, "Home", ""},
	{PAGEUPKEY, "PAGE_UP", 0, "Page Up", "PgUp"},
	{PAGEDOWNKEY, "PAGE_DOWN", 0, "Page Down", "PgDown"},
	{ENDKEY, "END", 0, "End", ""},
	{0, "", 0, NULL, NULL},
	{MEDIAPLAY, "MEDIA_PLAY", 0, "Media Play/Pause", ">/||"},
	{MEDIASTOP, "MEDIA_STOP", 0, "Media Stop", "Stop"},
	{MEDIAFIRST, "MEDIA_FIRST", 0, "Media First", "|<<"},
	{MEDIALAST, "MEDIA_LAST", 0, "Media Last", ">>|"},
	{0, "", 0, NULL, NULL},
	{KM_TEXTINPUT, "TEXTINPUT", 0, "Text Input", "TxtIn"},
	{0, "", 0, NULL, NULL},
	{WINDEACTIVATE, "WINDOW_DEACTIVATE", 0, "Window Deactivate", ""},
	{TIMER, "TIMER", 0, "Timer", "Tmr"},
	{TIMER0, "TIMER0", 0, "Timer 0", "Tmr0"},
	{TIMER1, "TIMER1", 0, "Timer 1", "Tmr1"},
	{TIMER2, "TIMER2", 0, "Timer 2", "Tmr2"},
	{TIMERJOBS, "TIMER_JOBS", 0, "Timer Jobs", "TmrJob"},
	{TIMERAUTOSAVE, "TIMER_AUTOSAVE", 0, "Timer Autosave", "TmrSave"},
	{TIMERREPORT, "TIMER_REPORT", 0, "Timer Report", "TmrReport"},
	{TIMERREGION, "TIMERREGION", 0, "Timer Region", "TmrReg"},
	{0, "", 0, NULL, NULL},
	{NDOF_MOTION, "NDOF_MOTION", 0, "NDOF Motion", "NdofMov"},
	/* buttons on all 3dconnexion devices */
	{NDOF_BUTTON_MENU, "NDOF_BUTTON_MENU", 0, "NDOF Menu", "NdofMenu"},
	{NDOF_BUTTON_FIT, "NDOF_BUTTON_FIT", 0, "NDOF Fit", "NdofFit"},
	/* view buttons */
	{NDOF_BUTTON_TOP, "NDOF_BUTTON_TOP", 0, "NDOF Top", "Ndof↑"},
	{NDOF_BUTTON_BOTTOM, "NDOF_BUTTON_BOTTOM", 0, "NDOF Bottom", "Ndof↓"},
	{NDOF_BUTTON_LEFT, "NDOF_BUTTON_LEFT", 0, "NDOF Left", "Ndof←"},
	{NDOF_BUTTON_RIGHT, "NDOF_BUTTON_RIGHT", 0, "NDOF Right", "Ndof→"},
	{NDOF_BUTTON_FRONT, "NDOF_BUTTON_FRONT", 0, "NDOF Front", "NdofFront"},
	{NDOF_BUTTON_BACK, "NDOF_BUTTON_BACK", 0, "NDOF Back", "NdofBack"},
	/* more views */
	{NDOF_BUTTON_ISO1, "NDOF_BUTTON_ISO1", 0, "NDOF Isometric 1", "NdofIso1"},
	{NDOF_BUTTON_ISO2, "NDOF_BUTTON_ISO2", 0, "NDOF Isometric 2", "NdofIso2"},
	/* 90 degree rotations */
	{NDOF_BUTTON_ROLL_CW, "NDOF_BUTTON_ROLL_CW", 0, "NDOF Roll CW", "NdofRCW"},
	{NDOF_BUTTON_ROLL_CCW, "NDOF_BUTTON_ROLL_CCW", 0, "NDOF Roll CCW", "NdofRCCW"},
	{NDOF_BUTTON_SPIN_CW, "NDOF_BUTTON_SPIN_CW", 0, "NDOF Spin CW", "NdofSCW"},
	{NDOF_BUTTON_SPIN_CCW, "NDOF_BUTTON_SPIN_CCW", 0, "NDOF Spin CCW", "NdofSCCW"},
	{NDOF_BUTTON_TILT_CW, "NDOF_BUTTON_TILT_CW", 0, "NDOF Tilt CW", "NdofTCW"},
	{NDOF_BUTTON_TILT_CCW, "NDOF_BUTTON_TILT_CCW", 0, "NDOF Tilt CCW", "NdofTCCW"},
	/* device control */
	{NDOF_BUTTON_ROTATE, "NDOF_BUTTON_ROTATE", 0, "NDOF Rotate", "NdofRot"},
	{NDOF_BUTTON_PANZOOM, "NDOF_BUTTON_PANZOOM", 0, "NDOF Pan/Zoom", "NdofPanZoom"},
	{NDOF_BUTTON_DOMINANT, "NDOF_BUTTON_DOMINANT", 0, "NDOF Dominant", "NdofDom"},
	{NDOF_BUTTON_PLUS, "NDOF_BUTTON_PLUS", 0, "NDOF Plus", "Ndof+"},
	{NDOF_BUTTON_MINUS, "NDOF_BUTTON_MINUS", 0, "NDOF Minus", "Ndof-"},
	/* keyboard emulation */
	{NDOF_BUTTON_ESC, "NDOF_BUTTON_ESC", 0, "NDOF Esc", "NdofEsc"},
	{NDOF_BUTTON_ALT, "NDOF_BUTTON_ALT", 0, "NDOF Alt", "NdofAlt"},
	{NDOF_BUTTON_SHIFT, "NDOF_BUTTON_SHIFT", 0, "NDOF Shift", "NdofShift"},
	{NDOF_BUTTON_CTRL, "NDOF_BUTTON_CTRL", 0, "NDOF Ctrl", "NdofCtrl"},
	/* general-purpose buttons */
	{NDOF_BUTTON_1, "NDOF_BUTTON_1", 0, "NDOF Button 1", "NdofB1"},
	{NDOF_BUTTON_2, "NDOF_BUTTON_2", 0, "NDOF Button 2", "NdofB2"},
	{NDOF_BUTTON_3, "NDOF_BUTTON_3", 0, "NDOF Button 3", "NdofB3"},
	{NDOF_BUTTON_4, "NDOF_BUTTON_4", 0, "NDOF Button 4", "NdofB4"},
	{NDOF_BUTTON_5, "NDOF_BUTTON_5", 0, "NDOF Button 5", "NdofB5"},
	{NDOF_BUTTON_6, "NDOF_BUTTON_6", 0, "NDOF Button 6", "NdofB6"},
	{NDOF_BUTTON_7, "NDOF_BUTTON_7", 0, "NDOF Button 7", "NdofB7"},
	{NDOF_BUTTON_8, "NDOF_BUTTON_8", 0, "NDOF Button 8", "NdofB8"},
	{NDOF_BUTTON_9, "NDOF_BUTTON_9", 0, "NDOF Button 9", "NdofB9"},
	{NDOF_BUTTON_10, "NDOF_BUTTON_10", 0, "NDOF Button 10", "NdofB10"},
	{NDOF_BUTTON_A, "NDOF_BUTTON_A", 0, "NDOF Button A", "NdofBA"},
	{NDOF_BUTTON_B, "NDOF_BUTTON_B", 0, "NDOF Button B", "NdofBB"},
	{NDOF_BUTTON_C, "NDOF_BUTTON_C", 0, "NDOF Button C", "NdofBC"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem rna_enum_event_value_items[] = {
	{KM_ANY, "ANY", 0, "Any", ""},
	{KM_NOTHING, "NOTHING", 0, "Nothing", ""},
	{KM_PRESS, "PRESS", 0, "Press", ""},
	{KM_RELEASE, "RELEASE", 0, "Release", ""},
	{KM_CLICK, "CLICK", 0, "Click", ""},
	{KM_DBL_CLICK, "DOUBLE_CLICK", 0, "Double Click", ""},
	{EVT_GESTURE_N, "NORTH", 0, "North", ""},
	{EVT_GESTURE_NE, "NORTH_EAST", 0, "North-East", ""},
	{EVT_GESTURE_E, "EAST", 0, "East", ""},
	{EVT_GESTURE_SE, "SOUTH_EAST", 0, "South-East", ""},
	{EVT_GESTURE_S, "SOUTH", 0, "South", ""},
	{EVT_GESTURE_SW, "SOUTH_WEST", 0, "South-West", ""},
	{EVT_GESTURE_W, "WEST", 0, "West", ""},
	{EVT_GESTURE_NW, "NORTH_WEST", 0, "North-West", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem rna_enum_keymap_propvalue_items[] = {
	{0, "NONE", 0, "", ""},
	{0, NULL, 0, NULL, NULL}
};

#if 0
static EnumPropertyItem keymap_modifiers_items[] = {
	{KM_ANY, "ANY", 0, "Any", ""},
	{0, "NONE", 0, "None", ""},
	{1, "FIRST", 0, "First", ""},
	{2, "SECOND", 0, "Second", ""},
	{0, NULL, 0, NULL, NULL}
};
#endif


#ifndef RNA_RUNTIME
static EnumPropertyItem operator_flag_items[] = {
	{OPTYPE_REGISTER, "REGISTER", 0, "Register", "Display in the info window and support the redo toolbar panel"},
	{OPTYPE_UNDO, "UNDO", 0, "Undo", "Push an undo event (needed for operator redo)"},
	{OPTYPE_UNDO_GROUPED, "UNDO_GROUPED", 0, "Grouped Undo", "Push a single undo event for repetead instances of this operator"},
	{OPTYPE_BLOCKING, "BLOCKING", 0, "Blocking", "Block anything else from using the cursor"},
	{OPTYPE_MACRO, "MACRO", 0, "Macro", "Use to check if an operator is a macro"},
	{OPTYPE_GRAB_CURSOR, "GRAB_CURSOR", 0, "Grab Pointer",
	                      "Use so the operator grabs the mouse focus, enables wrapping when continuous grab "
	                      "is enabled"},
	{OPTYPE_PRESET, "PRESET", 0, "Preset", "Display a preset button with the operators settings"},
	{OPTYPE_INTERNAL, "INTERNAL", 0, "Internal", "Removes the operator from search results"},
	{0, NULL, 0, NULL, NULL}
};
#endif

EnumPropertyItem rna_enum_operator_return_items[] = {
	{OPERATOR_RUNNING_MODAL, "RUNNING_MODAL", 0, "Running Modal", "Keep the operator running with blender"},
	{OPERATOR_CANCELLED, "CANCELLED", 0, "Cancelled", "When no action has been taken, operator exits"},
	{OPERATOR_FINISHED, "FINISHED", 0, "Finished", "When the operator is complete, operator exits"},
	/* used as a flag */
	{OPERATOR_PASS_THROUGH, "PASS_THROUGH", 0, "Pass Through", "Do nothing and pass the event on"},
	{OPERATOR_INTERFACE, "INTERFACE", 0, "Interface", "Handled but not executed (popup menus)"},
	{0, NULL, 0, NULL, NULL}
};

/* flag/enum */
EnumPropertyItem rna_enum_wm_report_items[] = {
	{RPT_DEBUG, "DEBUG", 0, "Debug", ""},
	{RPT_INFO, "INFO", 0, "Info", ""},
	{RPT_OPERATOR, "OPERATOR", 0, "Operator", ""},
	{RPT_PROPERTY, "PROPERTY", 0, "Property", ""},
	{RPT_WARNING, "WARNING", 0, "Warning", ""},
	{RPT_ERROR, "ERROR", 0, "Error", ""},
	{RPT_ERROR_INVALID_INPUT, "ERROR_INVALID_INPUT", 0, "Invalid Input", ""},
	{RPT_ERROR_INVALID_CONTEXT, "ERROR_INVALID_CONTEXT", 0, "Invalid Context", ""},
	{RPT_ERROR_OUT_OF_MEMORY, "ERROR_OUT_OF_MEMORY", 0, "Out of Memory", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include <assert.h>

#include "WM_api.h"

#include "UI_interface.h"

#include "BKE_idprop.h"

#include "MEM_guardedalloc.h"

static wmOperator *rna_OperatorProperties_find_operator(PointerRNA *ptr)
{
	wmWindowManager *wm = ptr->id.data;

	if (wm) {
		IDProperty *properties = (IDProperty *)ptr->data;
		for (wmOperator *op = wm->operators.last; op; op = op->prev) {
			if (op->properties == properties) {
				return op;
			}
		}
	}
	
	return NULL;
}

static StructRNA *rna_OperatorProperties_refine(PointerRNA *ptr)
{
	wmOperator *op = rna_OperatorProperties_find_operator(ptr);

	if (op)
		return op->type->srna;
	else
		return ptr->type;
}

static IDProperty *rna_OperatorProperties_idprops(PointerRNA *ptr, bool create)
{
	if (create && !ptr->data) {
		IDPropertyTemplate val = {0};
		ptr->data = IDP_New(IDP_GROUP, &val, "RNA_OperatorProperties group");
	}

	return ptr->data;
}

static void rna_Operator_name_get(PointerRNA *ptr, char *value)
{
	wmOperator *op = (wmOperator *)ptr->data;
	strcpy(value, op->type->name);
}

static int rna_Operator_name_length(PointerRNA *ptr)
{
	wmOperator *op = (wmOperator *)ptr->data;
	return strlen(op->type->name);
}

static int rna_Operator_has_reports_get(PointerRNA *ptr)
{
	wmOperator *op = (wmOperator *)ptr->data;
	return (op->reports && op->reports->list.first);
}

static PointerRNA rna_Operator_options_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_OperatorOptions, ptr->data);
}

static PointerRNA rna_Operator_properties_get(PointerRNA *ptr)
{
	wmOperator *op = (wmOperator *)ptr->data;
	return rna_pointer_inherit_refine(ptr, op->type->srna, op->properties);
}

static PointerRNA rna_OperatorMacro_properties_get(PointerRNA *ptr)
{
	wmOperatorTypeMacro *otmacro = (wmOperatorTypeMacro *)ptr->data;
	wmOperatorType *ot = WM_operatortype_find(otmacro->idname, true);
	return rna_pointer_inherit_refine(ptr, ot->srna, otmacro->properties);
}

static void rna_Event_ascii_get(PointerRNA *ptr, char *value)
{
	const wmEvent *event = ptr->data;
	value[0] = event->ascii;
	value[1] = '\0';
}

static int rna_Event_ascii_length(PointerRNA *ptr)
{
	const wmEvent *event = ptr->data;
	return (event->ascii) ? 1 : 0;
}

static void rna_Event_unicode_get(PointerRNA *ptr, char *value)
{
	/* utf8 buf isn't \0 terminated */
	const wmEvent *event = ptr->data;
	size_t len = 0;

	if (event->utf8_buf[0]) {
		BLI_str_utf8_as_unicode_and_size(event->utf8_buf, &len);
		if (len > 0) {
			memcpy(value, event->utf8_buf, len);
		}
	}

	value[len] = '\0';
}

static int rna_Event_unicode_length(PointerRNA *ptr)
{

	const wmEvent *event = ptr->data;
	if (event->utf8_buf[0]) {
		/* invalid value is checked on assignment so we don't need to account for this */
		return BLI_str_utf8_size(event->utf8_buf);
	}
	else {
		return 0;
	}
}

static float rna_Event_pressure_get(PointerRNA *ptr)
{
	const wmEvent *event = ptr->data;
	return WM_event_tablet_data(event, NULL, NULL);
}

static int rna_Event_is_tablet_get(PointerRNA *ptr)
{
	const wmEvent *event = ptr->data;
	return WM_event_is_tablet(event);
}

static void rna_Event_tilt_get(PointerRNA *ptr, float *values)
{
	wmEvent *event = ptr->data;
	WM_event_tablet_data(event, NULL, values);
}

static PointerRNA rna_PopupMenu_layout_get(PointerRNA *ptr)
{
	struct uiPopupMenu *pup = ptr->data;
	uiLayout *layout = UI_popup_menu_layout(pup);

	PointerRNA rptr;
	RNA_pointer_create(ptr->id.data, &RNA_UILayout, layout, &rptr);

	return rptr;
}

static PointerRNA rna_PieMenu_layout_get(PointerRNA *ptr)
{
	struct uiPieMenu *pie = ptr->data;
	uiLayout *layout = UI_pie_menu_layout(pie);

	PointerRNA rptr;
	RNA_pointer_create(ptr->id.data, &RNA_UILayout, layout, &rptr);

	return rptr;
}

static void rna_Window_screen_set(PointerRNA *ptr, PointerRNA value)
{
	wmWindow *win = (wmWindow *)ptr->data;

	/* disallow ID-browsing away from temp screens */
	if (win->screen->temp) {
		return;
	}

	if (value.data == NULL)
		return;

	/* exception: can't set screens inside of area/region handlers */
	win->newscreen = value.data;
}

static int rna_Window_screen_assign_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	bScreen *screen = (bScreen *)value.id.data;

	return !screen->temp;
}


static void rna_Window_screen_update(bContext *C, PointerRNA *ptr)
{
	wmWindow *win = (wmWindow *)ptr->data;

	/* exception: can't set screens inside of area/region handlers,
	 * and must use context so notifier gets to the right window */
	if (win->newscreen) {
		WM_event_add_notifier(C, NC_SCREEN | ND_SCREENBROWSE, win->newscreen);
		win->newscreen = NULL;
	}
}

static PointerRNA rna_KeyMapItem_properties_get(PointerRNA *ptr)
{
	wmKeyMapItem *kmi = ptr->data;

	if (kmi->ptr)
		return *(kmi->ptr);
	
	/*return rna_pointer_inherit_refine(ptr, &RNA_OperatorProperties, op->properties); */
	return PointerRNA_NULL;
}

static int rna_wmKeyMapItem_map_type_get(PointerRNA *ptr)
{
	wmKeyMapItem *kmi = ptr->data;

	return WM_keymap_map_type_get(kmi);
}

static void rna_wmKeyMapItem_map_type_set(PointerRNA *ptr, int value)
{
	wmKeyMapItem *kmi = ptr->data;
	int map_type = rna_wmKeyMapItem_map_type_get(ptr);

	if (value != map_type) {
		switch (value) {
			case KMI_TYPE_KEYBOARD:
				kmi->type = AKEY;
				kmi->val = KM_PRESS;
				break;
			case KMI_TYPE_TWEAK:
				kmi->type = EVT_TWEAK_L;
				kmi->val = KM_ANY;
				break;
			case KMI_TYPE_MOUSE:
				kmi->type = LEFTMOUSE;
				kmi->val = KM_PRESS;
				break;
			case KMI_TYPE_TEXTINPUT:
				kmi->type = KM_TEXTINPUT;
				kmi->val = KM_NOTHING;
				break;
			case KMI_TYPE_TIMER:
				kmi->type = TIMER;
				kmi->val = KM_NOTHING;
				break;
			case KMI_TYPE_NDOF:
				kmi->type = NDOF_MOTION;
				kmi->val = KM_NOTHING;
				break;
		}
	}
}

/* assumes value to be an enum from rna_enum_event_type_items */
/* function makes sure keymodifiers are only valid keys, ESC keeps it unaltered */
static void rna_wmKeyMapItem_keymodifier_set(PointerRNA *ptr, int value)
{
	wmKeyMapItem *kmi = ptr->data;
	
	/* XXX, this should really be managed in an _itemf function,
	 * giving a list of valid enums, then silently changing them when they are set is not
	 * a good precedent, don't do this unless you have a good reason! */
	if (value == ESCKEY) {
		/* pass */
	}
	else if (value >= AKEY) {
		kmi->keymodifier = value;
	}
	else {
		kmi->keymodifier = 0;
	}
}


static EnumPropertyItem *rna_KeyMapItem_type_itemf(bContext *UNUSED(C), PointerRNA *ptr, PropertyRNA *UNUSED(prop),
                                                   bool *UNUSED(r_free))
{
	int map_type = rna_wmKeyMapItem_map_type_get(ptr);

	if (map_type == KMI_TYPE_MOUSE) return event_mouse_type_items;
	if (map_type == KMI_TYPE_TWEAK) return event_tweak_type_items;
	if (map_type == KMI_TYPE_TIMER) return event_timer_type_items;
	if (map_type == KMI_TYPE_NDOF) return event_ndof_type_items;
	if (map_type == KMI_TYPE_TEXTINPUT) return event_textinput_type_items;
	else return rna_enum_event_type_items;
}

static EnumPropertyItem *rna_KeyMapItem_value_itemf(bContext *UNUSED(C), PointerRNA *ptr, PropertyRNA *UNUSED(prop),
                                                    bool *UNUSED(r_free))
{
	int map_type = rna_wmKeyMapItem_map_type_get(ptr);

	if (map_type == KMI_TYPE_MOUSE || map_type == KMI_TYPE_KEYBOARD || map_type == KMI_TYPE_NDOF)
		return event_keymouse_value_items;
	if (map_type == KMI_TYPE_TWEAK)
		return event_tweak_value_items;
	else
		return rna_enum_event_value_items;
}

static EnumPropertyItem *rna_KeyMapItem_propvalue_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop),
                                                        bool *UNUSED(r_free))
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmKeyConfig *kc;
	wmKeyMap *km;

	for (kc = wm->keyconfigs.first; kc; kc = kc->next) {
		for (km = kc->keymaps.first; km; km = km->next) {
			/* only check if it's a modal keymap */
			if (km->modal_items) {
				wmKeyMapItem *kmi;
				for (kmi = km->items.first; kmi; kmi = kmi->next) {
					if (kmi == ptr->data) {
						return km->modal_items;
					}
				}
			}
		}
	}


	return rna_enum_keymap_propvalue_items; /* ERROR */
}

static int rna_KeyMapItem_any_get(PointerRNA *ptr)
{
	wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;

	if (kmi->shift == KM_ANY &&
	    kmi->ctrl == KM_ANY &&
	    kmi->alt == KM_ANY &&
	    kmi->oskey == KM_ANY)
	{
		return 1;
	}
	else {
		return 0;
	}
}

static void rna_KeyMapItem_any_set(PointerRNA *ptr, int value)
{
	wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;

	if (value) {
		kmi->shift = kmi->ctrl = kmi->alt = kmi->oskey = KM_ANY;
	}
	else {
		kmi->shift = kmi->ctrl = kmi->alt = kmi->oskey = 0;
	}
}

static int rna_KeyMapItem_shift_get(PointerRNA *ptr)
{
	wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;
	return kmi->shift != 0;
}

static int rna_KeyMapItem_ctrl_get(PointerRNA *ptr)
{
	wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;
	return kmi->ctrl != 0;
}

static int rna_KeyMapItem_alt_get(PointerRNA *ptr)
{
	wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;
	return kmi->alt != 0;
}

static int rna_KeyMapItem_oskey_get(PointerRNA *ptr)
{
	wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;
	return kmi->oskey != 0;
}

static PointerRNA rna_WindowManager_active_keyconfig_get(PointerRNA *ptr)
{
	wmWindowManager *wm = ptr->data;
	wmKeyConfig *kc;

	kc = BLI_findstring(&wm->keyconfigs, U.keyconfigstr, offsetof(wmKeyConfig, idname));

	if (!kc)
		kc = wm->defaultconf;
	
	return rna_pointer_inherit_refine(ptr, &RNA_KeyConfig, kc);
}

static void rna_WindowManager_active_keyconfig_set(PointerRNA *ptr, PointerRNA value)
{
	wmWindowManager *wm = ptr->data;
	wmKeyConfig *kc = value.data;

	if (kc)
		WM_keyconfig_set_active(wm, kc->idname);
}

static void rna_wmKeyMapItem_idname_get(PointerRNA *ptr, char *value)
{
	wmKeyMapItem *kmi = ptr->data;
	WM_operator_py_idname(value, kmi->idname);
}

static int rna_wmKeyMapItem_idname_length(PointerRNA *ptr)
{
	wmKeyMapItem *kmi = ptr->data;
	char pyname[OP_MAX_TYPENAME];

	WM_operator_py_idname(pyname, kmi->idname);
	return strlen(pyname);
}

static void rna_wmKeyMapItem_idname_set(PointerRNA *ptr, const char *value)
{
	wmKeyMapItem *kmi = ptr->data;
	char idname[OP_MAX_TYPENAME];

	WM_operator_bl_idname(idname, value);

	if (!STREQ(idname, kmi->idname)) {
		BLI_strncpy(kmi->idname, idname, sizeof(kmi->idname));

		WM_keymap_properties_reset(kmi, NULL);
	}
}

static void rna_wmKeyMapItem_name_get(PointerRNA *ptr, char *value)
{
	wmKeyMapItem *kmi = ptr->data;
	wmOperatorType *ot = WM_operatortype_find(kmi->idname, 1);
	strcpy(value, ot ? RNA_struct_ui_name(ot->srna) : kmi->idname);
}

static int rna_wmKeyMapItem_name_length(PointerRNA *ptr)
{
	wmKeyMapItem *kmi = ptr->data;
	wmOperatorType *ot = WM_operatortype_find(kmi->idname, 1);
	return strlen(ot ? RNA_struct_ui_name(ot->srna) : kmi->idname);
}

static int rna_KeyMapItem_userdefined_get(PointerRNA *ptr)
{
	wmKeyMapItem *kmi = ptr->data;
	return kmi->id < 0;
}

static void rna_wmClipboard_get(PointerRNA *UNUSED(ptr), char *value)
{
	char *pbuf;
	int pbuf_len;

	pbuf = WM_clipboard_text_get(false, &pbuf_len);
	if (pbuf) {
		memcpy(value, pbuf, pbuf_len + 1);
		MEM_freeN(pbuf);
	}
	else {
		value[0] = '\0';
	}
}

static int rna_wmClipboard_length(PointerRNA *UNUSED(ptr))
{
	char *pbuf;
	int pbuf_len;

	pbuf = WM_clipboard_text_get(false, &pbuf_len);
	if (pbuf) {
		MEM_freeN(pbuf);
	}

	return pbuf_len;
}

static void rna_wmClipboard_set(PointerRNA *UNUSED(ptr), const char *value)
{
	WM_clipboard_text_set((void *) value, false);
}

#ifdef WITH_PYTHON

static int rna_operator_poll_cb(bContext *C, wmOperatorType *ot)
{
	extern FunctionRNA rna_Operator_poll_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int visible;

	RNA_pointer_create(NULL, ot->ext.srna, NULL, &ptr); /* dummy */
	func = &rna_Operator_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	ot->ext.call(C, &ptr, func, &list);

	RNA_parameter_get_lookup(&list, "visible", &ret);
	visible = *(int *)ret;

	RNA_parameter_list_free(&list);

	return visible;
}

static int rna_operator_execute_cb(bContext *C, wmOperator *op)
{
	extern FunctionRNA rna_Operator_execute_func;

	PointerRNA opr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int result;

	RNA_pointer_create(NULL, op->type->ext.srna, op, &opr);
	func = &rna_Operator_execute_func; /* RNA_struct_find_function(&opr, "execute"); */

	RNA_parameter_list_create(&list, &opr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	op->type->ext.call(C, &opr, func, &list);

	RNA_parameter_get_lookup(&list, "result", &ret);
	result = *(int *)ret;

	RNA_parameter_list_free(&list);

	return result;
}

/* same as execute() but no return value */
static bool rna_operator_check_cb(bContext *C, wmOperator *op)
{
	extern FunctionRNA rna_Operator_check_func;

	PointerRNA opr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	bool result;

	RNA_pointer_create(NULL, op->type->ext.srna, op, &opr);
	func = &rna_Operator_check_func; /* RNA_struct_find_function(&opr, "check"); */

	RNA_parameter_list_create(&list, &opr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	op->type->ext.call(C, &opr, func, &list);

	RNA_parameter_get_lookup(&list, "result", &ret);
	result = (*(int *)ret) != 0;

	RNA_parameter_list_free(&list);

	return result;
}

static int rna_operator_invoke_cb(bContext *C, wmOperator *op, const wmEvent *event)
{
	extern FunctionRNA rna_Operator_invoke_func;

	PointerRNA opr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int result;

	RNA_pointer_create(NULL, op->type->ext.srna, op, &opr);
	func = &rna_Operator_invoke_func; /* RNA_struct_find_function(&opr, "invoke"); */

	RNA_parameter_list_create(&list, &opr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "event", &event);
	op->type->ext.call(C, &opr, func, &list);

	RNA_parameter_get_lookup(&list, "result", &ret);
	result = *(int *)ret;

	RNA_parameter_list_free(&list);

	return result;
}

/* same as invoke */
static int rna_operator_modal_cb(bContext *C, wmOperator *op, const wmEvent *event)
{
	extern FunctionRNA rna_Operator_modal_func;

	PointerRNA opr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int result;

	RNA_pointer_create(NULL, op->type->ext.srna, op, &opr);
	func = &rna_Operator_modal_func; /* RNA_struct_find_function(&opr, "modal"); */

	RNA_parameter_list_create(&list, &opr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "event", &event);
	op->type->ext.call(C, &opr, func, &list);

	RNA_parameter_get_lookup(&list, "result", &ret);
	result = *(int *)ret;

	RNA_parameter_list_free(&list);

	return result;
}

static void rna_operator_draw_cb(bContext *C, wmOperator *op)
{
	extern FunctionRNA rna_Operator_draw_func;

	PointerRNA opr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, op->type->ext.srna, op, &opr);
	func = &rna_Operator_draw_func; /* RNA_struct_find_function(&opr, "draw"); */

	RNA_parameter_list_create(&list, &opr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	op->type->ext.call(C, &opr, func, &list);

	RNA_parameter_list_free(&list);
}

/* same as exec(), but call cancel */
static void rna_operator_cancel_cb(bContext *C, wmOperator *op)
{
	extern FunctionRNA rna_Operator_cancel_func;

	PointerRNA opr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, op->type->ext.srna, op, &opr);
	func = &rna_Operator_cancel_func; /* RNA_struct_find_function(&opr, "cancel"); */

	RNA_parameter_list_create(&list, &opr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	op->type->ext.call(C, &opr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_Operator_unregister(struct Main *bmain, StructRNA *type);

/* bpy_operator_wrap.c */
extern void BPY_RNA_operator_wrapper(wmOperatorType *ot, void *userdata);
extern void BPY_RNA_operator_macro_wrapper(wmOperatorType *ot, void *userdata);

static StructRNA *rna_Operator_register(
        Main *bmain, ReportList *reports, void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	wmOperatorType dummyot = {NULL};
	wmOperator dummyop = {NULL};
	PointerRNA dummyotr;
	int have_function[7];

	struct {
		char idname[OP_MAX_TYPENAME];
		char name[OP_MAX_TYPENAME];
		char descr[RNA_DYN_DESCR_MAX];
		char ctxt[RNA_DYN_DESCR_MAX];
		char undo_group[OP_MAX_TYPENAME];
	} temp_buffers;

	/* setup dummy operator & operator type to store static properties in */
	dummyop.type = &dummyot;
	dummyot.idname = temp_buffers.idname; /* only assigne the pointer, string is NULL'd */
	dummyot.name = temp_buffers.name; /* only assigne the pointer, string is NULL'd */
	dummyot.description = temp_buffers.descr; /* only assigne the pointer, string is NULL'd */
	dummyot.translation_context = temp_buffers.ctxt; /* only assigne the pointer, string is NULL'd */
	dummyot.undo_group = temp_buffers.undo_group; /* only assigne the pointer, string is NULL'd */
	RNA_pointer_create(NULL, &RNA_Operator, &dummyop, &dummyotr);

	/* clear in case they are left unset */
	temp_buffers.idname[0] = temp_buffers.name[0] = temp_buffers.descr[0] = temp_buffers.undo_group[0] = '\0';
	/* We have to set default op context! */
	strcpy(temp_buffers.ctxt, BLT_I18NCONTEXT_OPERATOR_DEFAULT);

	/* validate the python class */
	if (validate(&dummyotr, data, have_function) != 0)
		return NULL;

	{   /* convert foo.bar to FOO_OT_bar
		 * allocate the description and the idname in 1 go */

		/* inconveniently long name sanity check */
		{
			char *ch = temp_buffers.idname;
			int i;
			int dot = 0;
			for (i = 0; *ch; i++) {
				if ((*ch >= 'a' && *ch <= 'z') || (*ch >= '0' && *ch <= '9') || *ch == '_') {
					/* pass */
				}
				else if (*ch == '.') {
					dot++;
				}
				else {
					BKE_reportf(reports, RPT_ERROR,
					            "Registering operator class: '%s', invalid bl_idname '%s', at position %d",
					            identifier, temp_buffers.idname, i);
					return NULL;
				}

				ch++;
			}

			if (i > ((int)sizeof(dummyop.idname)) - 3) {
				BKE_reportf(reports, RPT_ERROR, "Registering operator class: '%s', invalid bl_idname '%s', "
				            "is too long, maximum length is %d", identifier, temp_buffers.idname,
				            (int)sizeof(dummyop.idname) - 3);
				return NULL;
			}

			if (dot != 1) {
				BKE_reportf(reports, RPT_ERROR,
				            "Registering operator class: '%s', invalid bl_idname '%s', must contain 1 '.' character",
				            identifier, temp_buffers.idname);
				return NULL;
			}
		}
		/* end sanity check */

		{
			const uint idname_len = strlen(temp_buffers.idname) + 4;
			const uint name_len = strlen(temp_buffers.name) + 1;
			const uint desc_len = strlen(temp_buffers.descr) + 1;
			const uint ctxt_len = strlen(temp_buffers.ctxt) + 1;
			const uint undo_group_len = strlen(temp_buffers.undo_group) + 1;
			/* 2 terminators and 3 to convert a.b -> A_OT_b */
			char *ch = MEM_mallocN(
			        sizeof(char) * (idname_len + name_len + desc_len + ctxt_len + undo_group_len), __func__);
			WM_operator_bl_idname(ch, temp_buffers.idname); /* convert the idname from python */
			dummyot.idname = ch;
			ch += idname_len;
			memcpy(ch, temp_buffers.name, name_len);
			dummyot.name = ch;
			ch += name_len;
			memcpy(ch, temp_buffers.descr, desc_len);
			dummyot.description = ch;
			ch += desc_len;
			memcpy(ch, temp_buffers.ctxt, ctxt_len);
			dummyot.translation_context = ch;
			ch += ctxt_len;
			memcpy(ch, temp_buffers.undo_group, undo_group_len);
			dummyot.undo_group = ch;
		}
	}

	/* check if we have registered this operator type before, and remove it */
	{
		wmOperatorType *ot = WM_operatortype_find(dummyot.idname, true);
		if (ot && ot->ext.srna)
			rna_Operator_unregister(bmain, ot->ext.srna);
	}

	/* XXX, this doubles up with the operator name [#29666]
	 * for now just remove from dir(bpy.types) */

	/* create a new operator type */
	dummyot.ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, dummyot.idname, &RNA_Operator);
	RNA_def_struct_flag(dummyot.ext.srna, STRUCT_NO_IDPROPERTIES); /* operator properties are registered separately */
	RNA_def_struct_translation_context(dummyot.ext.srna, dummyot.translation_context);
	dummyot.ext.data = data;
	dummyot.ext.call = call;
	dummyot.ext.free = free;

	dummyot.pyop_poll = (have_function[0]) ? rna_operator_poll_cb : NULL;
	dummyot.exec =      (have_function[1]) ? rna_operator_execute_cb : NULL;
	dummyot.check =     (have_function[2]) ? rna_operator_check_cb : NULL;
	dummyot.invoke =    (have_function[3]) ? rna_operator_invoke_cb : NULL;
	dummyot.modal =     (have_function[4]) ? rna_operator_modal_cb : NULL;
	dummyot.ui =        (have_function[5]) ? rna_operator_draw_cb : NULL;
	dummyot.cancel =    (have_function[6]) ? rna_operator_cancel_cb : NULL;
	WM_operatortype_append_ptr(BPY_RNA_operator_wrapper, (void *)&dummyot);

	/* update while blender is running */
	WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

	return dummyot.ext.srna;
}

static void rna_Operator_unregister(struct Main *bmain, StructRNA *type)
{
	const char *idname;
	wmOperatorType *ot = RNA_struct_blender_type_get(type);
	wmWindowManager *wm;

	if (!ot)
		return;

	/* update while blender is running */
	wm = bmain->wm.first;
	if (wm) {
		WM_operator_stack_clear(wm);

		WM_operator_handlers_clear(wm, ot);
	}
	WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

	RNA_struct_free_extension(type, &ot->ext);

	idname = ot->idname;
	WM_operatortype_remove_ptr(ot);
	MEM_freeN((void *)idname);

	/* not to be confused with the RNA_struct_free that WM_operatortype_remove calls, they are 2 different srna's */
	RNA_struct_free(&BLENDER_RNA, type);
}

static void **rna_Operator_instance(PointerRNA *ptr)
{
	wmOperator *op = ptr->data;
	return &op->py_instance;
}

static StructRNA *rna_MacroOperator_register(
        Main *bmain, ReportList *reports, void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	wmOperatorType dummyot = {NULL};
	wmOperator dummyop = {NULL};
	PointerRNA dummyotr;
	int have_function[4];

	struct {
		char idname[OP_MAX_TYPENAME];
		char name[OP_MAX_TYPENAME];
		char descr[RNA_DYN_DESCR_MAX];
		char ctxt[RNA_DYN_DESCR_MAX];
		char undo_group[OP_MAX_TYPENAME];
	} temp_buffers;

	/* setup dummy operator & operator type to store static properties in */
	dummyop.type = &dummyot;
	dummyot.idname = temp_buffers.idname; /* only assigne the pointer, string is NULL'd */
	dummyot.name = temp_buffers.name; /* only assigne the pointer, string is NULL'd */
	dummyot.description = temp_buffers.descr; /* only assigne the pointer, string is NULL'd */
	dummyot.translation_context = temp_buffers.ctxt; /* only assigne the pointer, string is NULL'd */
	dummyot.undo_group = temp_buffers.undo_group; /* only assigne the pointer, string is NULL'd */
	RNA_pointer_create(NULL, &RNA_Macro, &dummyop, &dummyotr);

	/* clear in case they are left unset */
	temp_buffers.idname[0] = temp_buffers.name[0] = temp_buffers.descr[0] = temp_buffers.undo_group[0] = '\0';
	/* We have to set default op context! */
	strcpy(temp_buffers.ctxt, BLT_I18NCONTEXT_OPERATOR_DEFAULT);

	/* validate the python class */
	if (validate(&dummyotr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(dummyop.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering operator class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummyop.idname));
		return NULL;
	}

	{   /* convert foo.bar to FOO_OT_bar
		 * allocate the description and the idname in 1 go */
		const uint idname_len = strlen(temp_buffers.idname) + 4;
		const uint name_len = strlen(temp_buffers.name) + 1;
		const uint desc_len = strlen(temp_buffers.descr) + 1;
		const uint ctxt_len = strlen(temp_buffers.ctxt) + 1;
		const uint undo_group_len = strlen(temp_buffers.undo_group) + 1;
		/* 2 terminators and 3 to convert a.b -> A_OT_b */
		char *ch = MEM_mallocN(
		        sizeof(char) * (idname_len + name_len + desc_len + ctxt_len + undo_group_len), __func__);
		WM_operator_bl_idname(ch, temp_buffers.idname); /* convert the idname from python */
		dummyot.idname = ch;
		ch += idname_len;
		memcpy(ch, temp_buffers.name, name_len);
		dummyot.name = ch;
		ch += name_len;
		memcpy(ch, temp_buffers.descr, desc_len);
		dummyot.description = ch;
		ch += desc_len;
		memcpy(ch, temp_buffers.ctxt, ctxt_len);
		dummyot.translation_context = ch;
		ch += ctxt_len;
		memcpy(ch, temp_buffers.undo_group, undo_group_len);
		dummyot.undo_group = ch;
	}

	/* check if we have registered this operator type before, and remove it */
	{
		wmOperatorType *ot = WM_operatortype_find(dummyot.idname, true);
		if (ot && ot->ext.srna)
			rna_Operator_unregister(bmain, ot->ext.srna);
	}

	/* XXX, this doubles up with the operator name [#29666]
	 * for now just remove from dir(bpy.types) */

	/* create a new operator type */
	dummyot.ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, dummyot.idname, &RNA_Operator);
	RNA_def_struct_translation_context(dummyot.ext.srna, dummyot.translation_context);
	dummyot.ext.data = data;
	dummyot.ext.call = call;
	dummyot.ext.free = free;

	dummyot.pyop_poll = (have_function[0]) ? rna_operator_poll_cb : NULL;
	dummyot.ui =        (have_function[3]) ? rna_operator_draw_cb : NULL;

	WM_operatortype_append_macro_ptr(BPY_RNA_operator_macro_wrapper, (void *)&dummyot);

	/* update while blender is running */
	WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

	return dummyot.ext.srna;
}
#endif /* WITH_PYTHON */

static StructRNA *rna_Operator_refine(PointerRNA *opr)
{
	wmOperator *op = (wmOperator *)opr->data;
	return (op->type && op->type->ext.srna) ? op->type->ext.srna : &RNA_Operator;
}

static StructRNA *rna_MacroOperator_refine(PointerRNA *opr)
{
	wmOperator *op = (wmOperator *)opr->data;
	return (op->type && op->type->ext.srna) ? op->type->ext.srna : &RNA_Macro;
}

/* just to work around 'const char *' warning and to ensure this is a python op */
static void rna_Operator_bl_idname_set(PointerRNA *ptr, const char *value)
{
	wmOperator *data = (wmOperator *)(ptr->data);
	char *str = (char *)data->type->idname;
	if (!str[0])
		BLI_strncpy(str, value, OP_MAX_TYPENAME);    /* utf8 already ensured */
	else
		assert(!"setting the bl_idname on a non-builtin operator");
}

static void rna_Operator_bl_label_set(PointerRNA *ptr, const char *value)
{
	wmOperator *data = (wmOperator *)(ptr->data);
	char *str = (char *)data->type->name;
	if (!str[0])
		BLI_strncpy(str, value, OP_MAX_TYPENAME);    /* utf8 already ensured */
	else
		assert(!"setting the bl_label on a non-builtin operator");
}

static void rna_Operator_bl_translation_context_set(PointerRNA *ptr, const char *value)
{
	wmOperator *data = (wmOperator *)(ptr->data);
	char *str = (char *)data->type->translation_context;
	if (!str[0])
		BLI_strncpy(str, value, RNA_DYN_DESCR_MAX);    /* utf8 already ensured */
	else
		assert(!"setting the bl_translation_context on a non-builtin operator");
}

static void rna_Operator_bl_description_set(PointerRNA *ptr, const char *value)
{
	wmOperator *data = (wmOperator *)(ptr->data);
	char *str = (char *)data->type->description;
	if (!str[0])
		BLI_strncpy(str, value, RNA_DYN_DESCR_MAX);    /* utf8 already ensured */
	else
		assert(!"setting the bl_description on a non-builtin operator");
}

static void rna_Operator_bl_undo_group_set(PointerRNA *ptr, const char *value)
{
	wmOperator *data = (wmOperator *)(ptr->data);
	char *str = (char *)data->type->undo_group;
	if (!str[0])
		BLI_strncpy(str, value, OP_MAX_TYPENAME);    /* utf8 already ensured */
	else
		assert(!"setting the bl_undo_group on a non-builtin operator");
}

static void rna_KeyMapItem_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	wmKeyMapItem *kmi = ptr->data;
	WM_keyconfig_update_tag(NULL, kmi);
}

#else /* RNA_RUNTIME */

/**
 * expose ``Operator.options`` as its own type so we can control each flags use (some are read-only).
 */
static void rna_def_operator_options_runtime(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "OperatorOptions", NULL);
	RNA_def_struct_ui_text(srna, "Operator Options", "Runtime options");
	RNA_def_struct_sdna(srna, "wmOperator");

	prop = RNA_def_property(srna, "is_grab_cursor", PROP_BOOLEAN, PROP_BOOLEAN);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", OP_IS_MODAL_GRAB_CURSOR);
	RNA_def_property_ui_text(prop, "Grab Cursor", "True when the cursor is grabbed");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "is_invoke", PROP_BOOLEAN, PROP_BOOLEAN);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", OP_IS_INVOKE);
	RNA_def_property_ui_text(prop, "Invoke", "True when invoked (even if only the execute callbacks available)");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_cursor_region", PROP_BOOLEAN, PROP_BOOLEAN);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", OP_IS_MODAL_CURSOR_REGION);
	RNA_def_property_ui_text(prop, "Focus Region", "Enable to use the region under the cursor for modal execution");
}

static void rna_def_operator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Operator", NULL);
	RNA_def_struct_ui_text(srna, "Operator", "Storage of an operator being executed, or registered after execution");
	RNA_def_struct_sdna(srna, "wmOperator");
	RNA_def_struct_refine_func(srna, "rna_Operator_refine");
#ifdef WITH_PYTHON
	RNA_def_struct_register_funcs(srna, "rna_Operator_register", "rna_Operator_unregister", "rna_Operator_instance");
#endif
	RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Operator_name_get", "rna_Operator_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "");

	prop = RNA_def_property(srna, "properties", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "OperatorProperties");
	RNA_def_property_ui_text(prop, "Properties", "");
	RNA_def_property_pointer_funcs(prop, "rna_Operator_properties_get", NULL, NULL, NULL);
	
	prop = RNA_def_property(srna, "has_reports", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* this is 'virtual' property */
	RNA_def_property_boolean_funcs(prop, "rna_Operator_has_reports_get", NULL);
	RNA_def_property_ui_text(prop, "Has Reports",
	                         "Operator has a set of reports (warnings and errors) from last execution");
	
	prop = RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "UILayout");

	prop = RNA_def_property(srna, "options", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "OperatorOptions");
	RNA_def_property_pointer_funcs(prop, "rna_Operator_options_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Options", "Runtime options");

	/* Registration */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	/* else it uses the pointer size!. -3 because '.' -> '_OT_' */
	RNA_def_property_string_maxlength(prop, OP_MAX_TYPENAME - 3);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_idname_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->name");
	RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_label_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER);

	prop = RNA_def_property(srna, "bl_translation_context", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->translation_context");
	RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_translation_context_set");
	RNA_def_property_string_default(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_clear_flag(prop, PROP_NEVER_NULL); /* check for NULL */

	prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->description");
	RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_description_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_clear_flag(prop, PROP_NEVER_NULL); /* check for NULL */

	prop = RNA_def_property(srna, "bl_undo_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->undo_group");
	RNA_def_property_string_maxlength(prop, OP_MAX_TYPENAME); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_undo_group_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_clear_flag(prop, PROP_NEVER_NULL); /* check for NULL */

	prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->flag");
	RNA_def_property_enum_items(prop, operator_flag_items);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Options",  "Options for this operator type");

	prop = RNA_def_property(srna, "macros", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "macro", NULL);
	RNA_def_property_struct_type(prop, "Macro");
	RNA_def_property_ui_text(prop, "Macros", "");

	RNA_api_operator(srna);

	srna = RNA_def_struct(brna, "OperatorProperties", NULL);
	RNA_def_struct_ui_text(srna, "Operator Properties", "Input properties of an Operator");
	RNA_def_struct_refine_func(srna, "rna_OperatorProperties_refine");
	RNA_def_struct_idprops_func(srna, "rna_OperatorProperties_idprops");
	RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES);
}

static void rna_def_macro_operator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Macro", NULL);
	RNA_def_struct_ui_text(srna, "Macro Operator",
	                       "Storage of a macro operator being executed, or registered after execution");
	RNA_def_struct_sdna(srna, "wmOperator");
	RNA_def_struct_refine_func(srna, "rna_MacroOperator_refine");
#ifdef WITH_PYTHON
	RNA_def_struct_register_funcs(srna, "rna_MacroOperator_register", "rna_Operator_unregister",
	                              "rna_Operator_instance");
#endif
	RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Operator_name_get", "rna_Operator_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "");

	prop = RNA_def_property(srna, "properties", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "OperatorProperties");
	RNA_def_property_ui_text(prop, "Properties", "");
	RNA_def_property_pointer_funcs(prop, "rna_Operator_properties_get", NULL, NULL, NULL);

	/* Registration */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_string_maxlength(prop, OP_MAX_TYPENAME); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_idname_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->name");
	RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_label_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER);

	prop = RNA_def_property(srna, "bl_translation_context", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->translation_context");
	RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_translation_context_set");
	RNA_def_property_string_default(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_clear_flag(prop, PROP_NEVER_NULL); /* check for NULL */

	prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->description");
	RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_description_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_clear_flag(prop, PROP_NEVER_NULL); /* check for NULL */

	prop = RNA_def_property(srna, "bl_undo_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->undo_group");
	RNA_def_property_string_maxlength(prop, OP_MAX_TYPENAME); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_undo_group_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_clear_flag(prop, PROP_NEVER_NULL); /* check for NULL */

	prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->flag");
	RNA_def_property_enum_items(prop, operator_flag_items);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Options",  "Options for this operator type");

	RNA_api_macro(srna);
}

static void rna_def_operator_type_macro(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "OperatorMacro", NULL);
	RNA_def_struct_ui_text(srna, "Operator Macro", "Storage of a sub operator in a macro after it has been added");
	RNA_def_struct_sdna(srna, "wmOperatorTypeMacro");

/*	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE); */
/*	RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
/*	RNA_def_property_string_sdna(prop, NULL, "idname"); */
/*	RNA_def_property_ui_text(prop, "Name", "Name of the sub operator"); */
/*	RNA_def_struct_name_property(srna, prop); */

	prop = RNA_def_property(srna, "properties", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "OperatorProperties");
	RNA_def_property_ui_text(prop, "Properties", "");
	RNA_def_property_pointer_funcs(prop, "rna_OperatorMacro_properties_get", NULL, NULL, NULL);
}

static void rna_def_operator_utils(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "OperatorMousePath", "PropertyGroup");
	RNA_def_struct_ui_text(srna, "Operator Mouse Path", "Mouse path values for operators that record such paths");

	prop = RNA_def_property(srna, "loc", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "Location", "Mouse location");

	prop = RNA_def_property(srna, "time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Time", "Time of mouse location");
}

static void rna_def_operator_filelist_element(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "OperatorFileListElement", "PropertyGroup");
	RNA_def_struct_ui_text(srna, "Operator File List Element", "");


	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_FILENAME);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Name", "Name of a file or directory within a file list");
}
	
static void rna_def_event(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "Event", NULL);
	RNA_def_struct_ui_text(srna, "Event", "Window Manager Event");
	RNA_def_struct_sdna(srna, "wmEvent");

	RNA_define_verify_sdna(0); /* not in sdna */

	/* strings */
	prop = RNA_def_property(srna, "ascii", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Event_ascii_get", "rna_Event_ascii_length", NULL);
	RNA_def_property_ui_text(prop, "ASCII", "Single ASCII character for this event");


	prop = RNA_def_property(srna, "unicode", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Event_unicode_get", "rna_Event_unicode_length", NULL);
	RNA_def_property_ui_text(prop, "Unicode", "Single unicode character for this event");

	/* enums */
	prop = RNA_def_property(srna, "value", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "val");
	RNA_def_property_enum_items(prop, rna_enum_event_value_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Value",  "The type of event, only applies to some");
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, rna_enum_event_type_items);
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UI_EVENTS);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type",  "");


	/* mouse */
	prop = RNA_def_property(srna, "mouse_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "x");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mouse X Position", "The window relative horizontal location of the mouse");
	
	prop = RNA_def_property(srna, "mouse_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "y");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mouse Y Position", "The window relative vertical location of the mouse");

	prop = RNA_def_property(srna, "mouse_region_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "mval[0]");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mouse X Position", "The region relative horizontal location of the mouse");

	prop = RNA_def_property(srna, "mouse_region_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "mval[1]");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mouse Y Position", "The region relative vertical location of the mouse");
	
	prop = RNA_def_property(srna, "mouse_prev_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "prevx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mouse Previous X Position", "The window relative horizontal location of the mouse");
	
	prop = RNA_def_property(srna, "mouse_prev_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "prevy");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mouse Previous Y Position", "The window relative vertical location of the mouse");

	prop = RNA_def_property(srna, "pressure", PROP_FLOAT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_Event_pressure_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Tablet Pressure", "The pressure of the tablet or 1.0 if no tablet present");

	prop = RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_XYZ_LENGTH);
	RNA_def_property_array(prop, 2);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_Event_tilt_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Tablet Tilt", "The pressure of the tablet or zeroes if no tablet present");

	prop = RNA_def_property(srna, "is_tablet", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Event_is_tablet_get", NULL);
	RNA_def_property_ui_text(prop, "Tablet Pressure", "The pressure of the tablet or 1.0 if no tablet present");

	/* modifiers */
	prop = RNA_def_property(srna, "shift", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shift", 1);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Shift", "True when the Shift key is held");
	
	prop = RNA_def_property(srna, "ctrl", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ctrl", 1);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Ctrl", "True when the Ctrl key is held");
	
	prop = RNA_def_property(srna, "alt", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "alt", 1);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Alt", "True when the Alt/Option key is held");
	
	prop = RNA_def_property(srna, "oskey", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "oskey", 1);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "OS Key", "True when the Cmd key is held");

	RNA_define_verify_sdna(1); /* not in sdna */
}

static void rna_def_timer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Timer", NULL);
	RNA_def_struct_ui_text(srna, "Timer", "Window event timer");
	RNA_def_struct_sdna(srna, "wmTimer");

	RNA_define_verify_sdna(0); /* not in sdna */

	/* could wrap more, for now this is enough */
	prop = RNA_def_property(srna, "time_step", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "timestep");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Time Step", "");

	prop = RNA_def_property(srna, "time_delta", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "delta");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Delta", "Time since last step in seconds");

	prop = RNA_def_property(srna, "time_duration", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "duration");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Delta", "Time since last step in seconds");

	RNA_define_verify_sdna(1); /* not in sdna */
}

static void rna_def_popupmenu(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "UIPopupMenu", NULL);
	RNA_def_struct_ui_text(srna, "PopupMenu", "");
	RNA_def_struct_sdna(srna, "uiPopupMenu");

	RNA_define_verify_sdna(0); /* not in sdna */

	/* could wrap more, for now this is enough */
	prop = RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "UILayout");
	RNA_def_property_pointer_funcs(prop, "rna_PopupMenu_layout_get",
	                               NULL, NULL, NULL);

	RNA_define_verify_sdna(1); /* not in sdna */
}

static void rna_def_piemenu(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "UIPieMenu", NULL);
	RNA_def_struct_ui_text(srna, "PieMenu", "");
	RNA_def_struct_sdna(srna, "uiPieMenu");

	RNA_define_verify_sdna(0); /* not in sdna */

	/* could wrap more, for now this is enough */
	prop = RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "UILayout");
	RNA_def_property_pointer_funcs(prop, "rna_PieMenu_layout_get",
	                               NULL, NULL, NULL);

	RNA_define_verify_sdna(1); /* not in sdna */
}

static void rna_def_window_stereo3d(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Stereo3dDisplay", NULL);
	RNA_def_struct_sdna(srna, "Stereo3dFormat");
	RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
	RNA_def_struct_ui_text(srna, "Stereo 3D Display", "Settings for stereo 3D display");

	prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_stereo3d_display_items);
	RNA_def_property_ui_text(prop, "Display Mode", "");

	prop = RNA_def_property(srna, "anaglyph_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_stereo3d_anaglyph_type_items);
	RNA_def_property_ui_text(prop, "Anaglyph Type", "");

	prop = RNA_def_property(srna, "interlace_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_stereo3d_interlace_type_items);
	RNA_def_property_ui_text(prop, "Interlace Type", "");

	prop = RNA_def_property(srna, "use_interlace_swap", PROP_BOOLEAN, PROP_BOOLEAN);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", S3D_INTERLACE_SWAP);
	RNA_def_property_ui_text(prop, "Swap Left/Right", "Swap left and right stereo channels");

	prop = RNA_def_property(srna, "use_sidebyside_crosseyed", PROP_BOOLEAN, PROP_BOOLEAN);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", S3D_SIDEBYSIDE_CROSSEYED);
	RNA_def_property_ui_text(prop, "Cross-Eyed", "Right eye should see left image and vice-versa");
}

static void rna_def_window(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Window", NULL);
	RNA_def_struct_ui_text(srna, "Window", "Open window");
	RNA_def_struct_sdna(srna, "wmWindow");

	rna_def_window_stereo3d(brna);

	prop = RNA_def_property(srna, "screen", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "Screen");
	RNA_def_property_ui_text(prop, "Screen", "Active screen showing in the window");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Window_screen_set", NULL, "rna_Window_screen_assign_poll");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_Window_screen_update");

	prop = RNA_def_property(srna, "x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "posx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "X Position", "Horizontal location of the window");

	prop = RNA_def_property(srna, "y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "posy");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Y Position", "Vertical location of the window");

	prop = RNA_def_property(srna, "width", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "sizex");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Width", "Window width");

	prop = RNA_def_property(srna, "height", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "sizey");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Height", "Window height");

	prop = RNA_def_property(srna, "stereo_3d_display", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "stereo3d_format");
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "Stereo3dDisplay");
	RNA_def_property_ui_text(prop, "Stereo 3D Display", "Settings for stereo 3d display");

	RNA_api_window(srna);
}

/* curve.splines */
static void rna_def_wm_keyconfigs(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "KeyConfigurations");
	srna = RNA_def_struct(brna, "KeyConfigurations", NULL);
	RNA_def_struct_sdna(srna, "wmWindowManager");
	RNA_def_struct_ui_text(srna, "KeyConfigs", "Collection of KeyConfigs");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "KeyConfig");
	RNA_def_property_pointer_funcs(prop, "rna_WindowManager_active_keyconfig_get",
	                               "rna_WindowManager_active_keyconfig_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active KeyConfig", "Active key configuration (preset)");
	
	prop = RNA_def_property(srna, "default", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "defaultconf");
	RNA_def_property_struct_type(prop, "KeyConfig");
	RNA_def_property_ui_text(prop, "Default Key Configuration", "Default builtin key configuration");

	prop = RNA_def_property(srna, "addon", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "addonconf");
	RNA_def_property_struct_type(prop, "KeyConfig");
	RNA_def_property_ui_text(prop, "Add-on Key Configuration",
	                         "Key configuration that can be extended by add-ons, and is added to the active "
	                         "configuration when handling events");

	prop = RNA_def_property(srna, "user", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "userconf");
	RNA_def_property_struct_type(prop, "KeyConfig");
	RNA_def_property_ui_text(prop, "User Key Configuration",
	                         "Final key configuration that combines keymaps from the active and add-on configurations, "
	                         "and can be edited by the user");
	
	RNA_api_keyconfigs(srna);
}

static void rna_def_windowmanager(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "WindowManager", "ID");
	RNA_def_struct_ui_text(srna, "Window Manager",
	                       "Window manager data-block defining open windows and other user interface data");
	RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);
	RNA_def_struct_sdna(srna, "wmWindowManager");

	prop = RNA_def_property(srna, "operators", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Operator");
	RNA_def_property_ui_text(prop, "Operators", "Operator registry");

	prop = RNA_def_property(srna, "windows", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Window");
	RNA_def_property_ui_text(prop, "Windows", "Open windows");

	prop = RNA_def_property(srna, "keyconfigs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "KeyConfig");
	RNA_def_property_ui_text(prop, "Key Configurations", "Registered key configurations");
	rna_def_wm_keyconfigs(brna, prop);

	prop = RNA_def_property(srna, "clipboard", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_wmClipboard_get", "rna_wmClipboard_length", "rna_wmClipboard_set");
	RNA_def_property_ui_text(prop, "Text Clipboard", "");

	RNA_api_wm(srna);
}

/* keyconfig.items */
static void rna_def_keymap_items(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	
	RNA_def_property_srna(cprop, "KeyMapItems");
	srna = RNA_def_struct(brna, "KeyMapItems", NULL);
	RNA_def_struct_sdna(srna, "wmKeyMap");
	RNA_def_struct_ui_text(srna, "KeyMap Items", "Collection of keymap items");

	RNA_api_keymapitems(srna);
}

static void rna_def_wm_keymaps(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	RNA_def_property_srna(cprop, "KeyMaps");
	srna = RNA_def_struct(brna, "KeyMaps", NULL);
	RNA_def_struct_sdna(srna, "wmKeyConfig");
	RNA_def_struct_ui_text(srna, "Key Maps", "Collection of keymaps");

	RNA_api_keymaps(srna);
}

static void rna_def_keyconfig(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem map_type_items[] = {
		{KMI_TYPE_KEYBOARD, "KEYBOARD", 0, "Keyboard", ""},
		{KMI_TYPE_TWEAK, "TWEAK", 0, "Tweak", ""},
		{KMI_TYPE_MOUSE, "MOUSE", 0, "Mouse", ""},
		{KMI_TYPE_NDOF, "NDOF", 0, "NDOF", ""},
		{KMI_TYPE_TEXTINPUT, "TEXTINPUT", 0, "Text Input", ""},
		{KMI_TYPE_TIMER, "TIMER", 0, "Timer", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* KeyConfig */
	srna = RNA_def_struct(brna, "KeyConfig", NULL);
	RNA_def_struct_sdna(srna, "wmKeyConfig");
	RNA_def_struct_ui_text(srna, "Key Configuration", "Input configuration, including keymaps");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "idname");
	RNA_def_property_ui_text(prop, "Name", "Name of the key configuration");
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "keymaps", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "KeyMap");
	RNA_def_property_ui_text(prop, "Key Maps", "Key maps configured as part of this configuration");
	rna_def_wm_keymaps(brna, prop);

	prop = RNA_def_property(srna, "is_user_defined", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYCONF_USER);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "User Defined", "Indicates that a keyconfig was defined by the user");

	RNA_api_keyconfig(srna);

	/* KeyMap */
	srna = RNA_def_struct(brna, "KeyMap", NULL);
	RNA_def_struct_sdna(srna, "wmKeyMap");
	RNA_def_struct_ui_text(srna, "Key Map", "Input configuration, including keymaps");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "idname");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Name", "Name of the key map");
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "space_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "spaceid");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, rna_enum_space_type_items);
	RNA_def_property_ui_text(prop, "Space Type", "Optional space type keymap is associated with");

	prop = RNA_def_property(srna, "region_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "regionid");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, rna_enum_region_type_items);
	RNA_def_property_ui_text(prop, "Region Type", "Optional region type keymap is associated with");

	prop = RNA_def_property(srna, "keymap_items", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "items", NULL);
	RNA_def_property_struct_type(prop, "KeyMapItem");
	RNA_def_property_ui_text(prop, "Items", "Items in the keymap, linking an operator to an input event");
	rna_def_keymap_items(brna, prop);

	prop = RNA_def_property(srna, "is_user_modified", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYMAP_USER_MODIFIED);
	RNA_def_property_ui_text(prop, "User Defined", "Keymap is defined by the user");

	prop = RNA_def_property(srna, "is_modal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYMAP_MODAL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Modal Keymap",
	                         "Indicates that a keymap is used for translate modal events for an operator");

	prop = RNA_def_property(srna, "show_expanded_items", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYMAP_EXPANDED);
	RNA_def_property_ui_text(prop, "Items Expanded", "Expanded in the user interface");
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);
	
	prop = RNA_def_property(srna, "show_expanded_children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYMAP_CHILDREN_EXPANDED);
	RNA_def_property_ui_text(prop, "Children Expanded", "Children expanded in the user interface");
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);


	RNA_api_keymap(srna);

	/* KeyMapItem */
	srna = RNA_def_struct(brna, "KeyMapItem", NULL);
	RNA_def_struct_sdna(srna, "wmKeyMapItem");
	RNA_def_struct_ui_text(srna, "Key Map Item", "Item in a Key Map");

	prop = RNA_def_property(srna, "idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "idname");
	RNA_def_property_ui_text(prop, "Identifier", "Identifier of operator to call on input event");
	RNA_def_property_string_funcs(prop, "rna_wmKeyMapItem_idname_get", "rna_wmKeyMapItem_idname_length",
	                              "rna_wmKeyMapItem_idname_set");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	/* this is in fact the operator name, but if the operator can't be found we
	 * fallback on the operator ID */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Name", "Name of operator (translated) to call on input event");
	RNA_def_property_string_funcs(prop, "rna_wmKeyMapItem_name_get", "rna_wmKeyMapItem_name_length", NULL);
	
	prop = RNA_def_property(srna, "properties", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "OperatorProperties");
	RNA_def_property_pointer_funcs(prop, "rna_KeyMapItem_properties_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Properties", "Properties to set when the operator is called");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "map_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "maptype");
	RNA_def_property_enum_items(prop, map_type_items);
	RNA_def_property_enum_funcs(prop, "rna_wmKeyMapItem_map_type_get", "rna_wmKeyMapItem_map_type_set", NULL);
	RNA_def_property_ui_text(prop, "Map Type", "Type of event mapping");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, rna_enum_event_type_items);
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UI_EVENTS);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_KeyMapItem_type_itemf");
	RNA_def_property_ui_text(prop, "Type", "Type of event");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "value", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "val");
	RNA_def_property_enum_items(prop, rna_enum_event_value_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_KeyMapItem_value_itemf");
	RNA_def_property_ui_text(prop, "Value", "");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "id", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "id");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "ID", "ID of the item");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "any", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_KeyMapItem_any_get", "rna_KeyMapItem_any_set");
	RNA_def_property_ui_text(prop, "Any", "Any modifier keys pressed");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "shift", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shift", 0);
	RNA_def_property_boolean_funcs(prop, "rna_KeyMapItem_shift_get", NULL);
/*	RNA_def_property_enum_sdna(prop, NULL, "shift"); */
/*	RNA_def_property_enum_items(prop, keymap_modifiers_items); */
	RNA_def_property_ui_text(prop, "Shift", "Shift key pressed");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "ctrl", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ctrl", 0);
	RNA_def_property_boolean_funcs(prop, "rna_KeyMapItem_ctrl_get", NULL);
/*	RNA_def_property_enum_sdna(prop, NULL, "ctrl"); */
/*	RNA_def_property_enum_items(prop, keymap_modifiers_items); */
	RNA_def_property_ui_text(prop, "Ctrl", "Control key pressed");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "alt", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "alt", 0);
	RNA_def_property_boolean_funcs(prop, "rna_KeyMapItem_alt_get", NULL);
/*	RNA_def_property_enum_sdna(prop, NULL, "alt"); */
/*	RNA_def_property_enum_items(prop, keymap_modifiers_items); */
	RNA_def_property_ui_text(prop, "Alt", "Alt key pressed");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "oskey", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "oskey", 0);
	RNA_def_property_boolean_funcs(prop, "rna_KeyMapItem_oskey_get", NULL);
/*	RNA_def_property_enum_sdna(prop, NULL, "oskey"); */
/*	RNA_def_property_enum_items(prop, keymap_modifiers_items); */
	RNA_def_property_ui_text(prop, "OS Key", "Operating system key pressed");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "key_modifier", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "keymodifier");
	RNA_def_property_enum_items(prop, rna_enum_event_type_items);
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UI_EVENTS);
	RNA_def_property_enum_funcs(prop, NULL, "rna_wmKeyMapItem_keymodifier_set", NULL);
	RNA_def_property_ui_text(prop, "Key Modifier", "Regular key pressed as a modifier");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KMI_EXPANDED);
	RNA_def_property_ui_text(prop, "Expanded", "Show key map event and property details in the user interface");
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);

	prop = RNA_def_property(srna, "propvalue", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "propvalue");
	RNA_def_property_enum_items(prop, rna_enum_keymap_propvalue_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_KeyMapItem_propvalue_itemf");
	RNA_def_property_ui_text(prop, "Property Value", "The value this event translates to in a modal keymap");
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", KMI_INACTIVE);
	RNA_def_property_ui_text(prop, "Active", "Activate or deactivate item");
	RNA_def_property_ui_icon(prop, ICON_CHECKBOX_DEHLT, 1);
	RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

	prop = RNA_def_property(srna, "is_user_modified", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KMI_USER_MODIFIED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "User Modified", "Is this keymap item modified by the user");

	prop = RNA_def_property(srna, "is_user_defined", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "User Defined",
	                         "Is this keymap item user defined (doesn't just replace a builtin item)");
	RNA_def_property_boolean_funcs(prop, "rna_KeyMapItem_userdefined_get", NULL);

	RNA_api_keymapitem(srna);
}

void RNA_def_wm(BlenderRNA *brna)
{
	rna_def_operator(brna);
	rna_def_operator_options_runtime(brna);
	rna_def_operator_utils(brna);
	rna_def_operator_filelist_element(brna);
	rna_def_macro_operator(brna);
	rna_def_operator_type_macro(brna);
	rna_def_event(brna);
	rna_def_timer(brna);
	rna_def_popupmenu(brna);
	rna_def_piemenu(brna);
	rna_def_window(brna);
	rna_def_windowmanager(brna);
	rna_def_keyconfig(brna);
}

#endif /* RNA_RUNTIME */
