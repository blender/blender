/*
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
 */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_keyconfig.h"
#include "BKE_workspace.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

static const EnumPropertyItem event_tweak_type_items[] = {
    {EVT_TWEAK_L, "EVT_TWEAK_L", 0, "Left", ""},
    {EVT_TWEAK_M, "EVT_TWEAK_M", 0, "Middle", ""},
    {EVT_TWEAK_R, "EVT_TWEAK_R", 0, "Right", ""},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem event_mouse_type_items[] = {
    {LEFTMOUSE, "LEFTMOUSE", 0, "Left", ""},
    {MIDDLEMOUSE, "MIDDLEMOUSE", 0, "Middle", ""},
    {RIGHTMOUSE, "RIGHTMOUSE", 0, "Right", ""},
    {BUTTON4MOUSE, "BUTTON4MOUSE", 0, "Button4", ""},
    {BUTTON5MOUSE, "BUTTON5MOUSE", 0, "Button5", ""},
    {BUTTON6MOUSE, "BUTTON6MOUSE", 0, "Button6", ""},
    {BUTTON7MOUSE, "BUTTON7MOUSE", 0, "Button7", ""},
    {0, "", 0, NULL, NULL},
    {TABLET_STYLUS, "PEN", 0, "Pen", ""},
    {TABLET_ERASER, "ERASER", 0, "Eraser", ""},
    {0, "", 0, NULL, NULL},
    {MOUSEMOVE, "MOUSEMOVE", 0, "Move", ""},
    {MOUSEPAN, "TRACKPADPAN", 0, "Mouse/Trackpad Pan", ""},
    {MOUSEZOOM, "TRACKPADZOOM", 0, "Mouse/Trackpad Zoom", ""},
    {MOUSEROTATE, "MOUSEROTATE", 0, "Mouse/Trackpad Rotate", ""},
    {MOUSESMARTZOOM, "MOUSESMARTZOOM", 0, "Mouse/Trackpad Smart Zoom", ""},
    {0, "", 0, NULL, NULL},
    {WHEELUPMOUSE, "WHEELUPMOUSE", 0, "Wheel Up", ""},
    {WHEELDOWNMOUSE, "WHEELDOWNMOUSE", 0, "Wheel Down", ""},
    {WHEELINMOUSE, "WHEELINMOUSE", 0, "Wheel In", ""},
    {WHEELOUTMOUSE, "WHEELOUTMOUSE", 0, "Wheel Out", ""},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem event_timer_type_items[] = {
    {TIMER, "TIMER", 0, "Timer", ""},
    {TIMER0, "TIMER0", 0, "Timer 0", ""},
    {TIMER1, "TIMER1", 0, "Timer 1", ""},
    {TIMER2, "TIMER2", 0, "Timer 2", ""},
    {TIMERJOBS, "TIMER_JOBS", 0, "Timer Jobs", ""},
    {TIMERAUTOSAVE, "TIMER_AUTOSAVE", 0, "Timer Autosave", ""},
    {TIMERREPORT, "TIMER_REPORT", 0, "Timer Report", ""},
    {TIMERREGION, "TIMERREGION", 0, "Timer Region", ""},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem event_textinput_type_items[] = {
    {KM_TEXTINPUT, "TEXTINPUT", 0, "Text Input", ""},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem event_ndof_type_items[] = {
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
    {0, NULL, 0, NULL, NULL},
};
#endif /* RNA_RUNTIME */

/* not returned: CAPSLOCKKEY, UNKNOWNKEY */
const EnumPropertyItem rna_enum_event_type_items[] = {
    /* Note we abuse 'tooltip' message here to store a 'compact' form of some (too) long names. */
    {0, "NONE", 0, "", ""},
    {LEFTMOUSE, "LEFTMOUSE", 0, "Left Mouse", "LMB"},
    {MIDDLEMOUSE, "MIDDLEMOUSE", 0, "Middle Mouse", "MMB"},
    {RIGHTMOUSE, "RIGHTMOUSE", 0, "Right Mouse", "RMB"},
    {BUTTON4MOUSE, "BUTTON4MOUSE", 0, "Button4 Mouse", "MB4"},
    {BUTTON5MOUSE, "BUTTON5MOUSE", 0, "Button5 Mouse", "MB5"},
    {BUTTON6MOUSE, "BUTTON6MOUSE", 0, "Button6 Mouse", "MB6"},
    {BUTTON7MOUSE, "BUTTON7MOUSE", 0, "Button7 Mouse", "MB7"},
    {0, "", 0, NULL, NULL},
    {TABLET_STYLUS, "PEN", 0, "Pen", ""},
    {TABLET_ERASER, "ERASER", 0, "Eraser", ""},
    {0, "", 0, NULL, NULL},
    {MOUSEMOVE, "MOUSEMOVE", 0, "Mouse Move", "MsMov"},
    {INBETWEEN_MOUSEMOVE, "INBETWEEN_MOUSEMOVE", 0, "In-between Move", "MsSubMov"},
    {MOUSEPAN, "TRACKPADPAN", 0, "Mouse/Trackpad Pan", "MsPan"},
    {MOUSEZOOM, "TRACKPADZOOM", 0, "Mouse/Trackpad Zoom", "MsZoom"},
    {MOUSEROTATE, "MOUSEROTATE", 0, "Mouse/Trackpad Rotate", "MsRot"},
    {MOUSESMARTZOOM, "MOUSESMARTZOOM", 0, "Mouse/Trackpad Smart Zoom", "MsSmartZoom"},
    {0, "", 0, NULL, NULL},
    {WHEELUPMOUSE, "WHEELUPMOUSE", 0, "Wheel Up", "WhUp"},
    {WHEELDOWNMOUSE, "WHEELDOWNMOUSE", 0, "Wheel Down", "WhDown"},
    {WHEELINMOUSE, "WHEELINMOUSE", 0, "Wheel In", "WhIn"},
    {WHEELOUTMOUSE, "WHEELOUTMOUSE", 0, "Wheel Out", "WhOut"},
    {0, "", 0, NULL, NULL},
    {EVT_TWEAK_L, "EVT_TWEAK_L", 0, "Tweak Left", "TwkL"},
    {EVT_TWEAK_M, "EVT_TWEAK_M", 0, "Tweak Middle", "TwkM"},
    {EVT_TWEAK_R, "EVT_TWEAK_R", 0, "Tweak Right", "TwkR"},
    {0, "", 0, NULL, NULL},
    {EVT_AKEY, "A", 0, "A", ""},
    {EVT_BKEY, "B", 0, "B", ""},
    {EVT_CKEY, "C", 0, "C", ""},
    {EVT_DKEY, "D", 0, "D", ""},
    {EVT_EKEY, "E", 0, "E", ""},
    {EVT_FKEY, "F", 0, "F", ""},
    {EVT_GKEY, "G", 0, "G", ""},
    {EVT_HKEY, "H", 0, "H", ""},
    {EVT_IKEY, "I", 0, "I", ""},
    {EVT_JKEY, "J", 0, "J", ""},
    {EVT_KKEY, "K", 0, "K", ""},
    {EVT_LKEY, "L", 0, "L", ""},
    {EVT_MKEY, "M", 0, "M", ""},
    {EVT_NKEY, "N", 0, "N", ""},
    {EVT_OKEY, "O", 0, "O", ""},
    {EVT_PKEY, "P", 0, "P", ""},
    {EVT_QKEY, "Q", 0, "Q", ""},
    {EVT_RKEY, "R", 0, "R", ""},
    {EVT_SKEY, "S", 0, "S", ""},
    {EVT_TKEY, "T", 0, "T", ""},
    {EVT_UKEY, "U", 0, "U", ""},
    {EVT_VKEY, "V", 0, "V", ""},
    {EVT_WKEY, "W", 0, "W", ""},
    {EVT_XKEY, "X", 0, "X", ""},
    {EVT_YKEY, "Y", 0, "Y", ""},
    {EVT_ZKEY, "Z", 0, "Z", ""},
    {0, "", 0, NULL, NULL},
    {EVT_ZEROKEY, "ZERO", 0, "0", ""},
    {EVT_ONEKEY, "ONE", 0, "1", ""},
    {EVT_TWOKEY, "TWO", 0, "2", ""},
    {EVT_THREEKEY, "THREE", 0, "3", ""},
    {EVT_FOURKEY, "FOUR", 0, "4", ""},
    {EVT_FIVEKEY, "FIVE", 0, "5", ""},
    {EVT_SIXKEY, "SIX", 0, "6", ""},
    {EVT_SEVENKEY, "SEVEN", 0, "7", ""},
    {EVT_EIGHTKEY, "EIGHT", 0, "8", ""},
    {EVT_NINEKEY, "NINE", 0, "9", ""},
    {0, "", 0, NULL, NULL},
    {EVT_LEFTCTRLKEY, "LEFT_CTRL", 0, "Left Ctrl", "CtrlL"},
    {EVT_LEFTALTKEY, "LEFT_ALT", 0, "Left Alt", "AltL"},
    {EVT_LEFTSHIFTKEY, "LEFT_SHIFT", 0, "Left Shift", "ShiftL"},
    {EVT_RIGHTALTKEY, "RIGHT_ALT", 0, "Right Alt", "AltR"},
    {EVT_RIGHTCTRLKEY, "RIGHT_CTRL", 0, "Right Ctrl", "CtrlR"},
    {EVT_RIGHTSHIFTKEY, "RIGHT_SHIFT", 0, "Right Shift", "ShiftR"},
    {0, "", 0, NULL, NULL},
    {EVT_OSKEY, "OSKEY", 0, "OS Key", "Cmd"},
    {EVT_APPKEY, "APP", 0, "Application", "App"},
    {EVT_GRLESSKEY, "GRLESS", 0, "Grless", ""},
    {EVT_ESCKEY, "ESC", 0, "Esc", ""},
    {EVT_TABKEY, "TAB", 0, "Tab", ""},
    {EVT_RETKEY, "RET", 0, "Return", "Enter"},
    {EVT_SPACEKEY, "SPACE", 0, "Spacebar", "Space"},
    {EVT_LINEFEEDKEY, "LINE_FEED", 0, "Line Feed", ""},
    {EVT_BACKSPACEKEY, "BACK_SPACE", 0, "Backspace", "BkSpace"},
    {EVT_DELKEY, "DEL", 0, "Delete", "Del"},
    {EVT_SEMICOLONKEY, "SEMI_COLON", 0, ";", ""},
    {EVT_PERIODKEY, "PERIOD", 0, ".", ""},
    {EVT_COMMAKEY, "COMMA", 0, ",", ""},
    {EVT_QUOTEKEY, "QUOTE", 0, "\"", ""},
    {EVT_ACCENTGRAVEKEY, "ACCENT_GRAVE", 0, "`", ""},
    {EVT_MINUSKEY, "MINUS", 0, "-", ""},
    {EVT_PLUSKEY, "PLUS", 0, "+", ""},
    {EVT_SLASHKEY, "SLASH", 0, "/", ""},
    {EVT_BACKSLASHKEY, "BACK_SLASH", 0, "\\", ""},
    {EVT_EQUALKEY, "EQUAL", 0, "=", ""},
    {EVT_LEFTBRACKETKEY, "LEFT_BRACKET", 0, "[", ""},
    {EVT_RIGHTBRACKETKEY, "RIGHT_BRACKET", 0, "]", ""},
    {EVT_LEFTARROWKEY, "LEFT_ARROW", 0, "Left Arrow", "←"},
    {EVT_DOWNARROWKEY, "DOWN_ARROW", 0, "Down Arrow", "↓"},
    {EVT_RIGHTARROWKEY, "RIGHT_ARROW", 0, "Right Arrow", "→"},
    {EVT_UPARROWKEY, "UP_ARROW", 0, "Up Arrow", "↑"},
    {EVT_PAD2, "NUMPAD_2", 0, "Numpad 2", "Pad2"},
    {EVT_PAD4, "NUMPAD_4", 0, "Numpad 4", "Pad4"},
    {EVT_PAD6, "NUMPAD_6", 0, "Numpad 6", "Pad6"},
    {EVT_PAD8, "NUMPAD_8", 0, "Numpad 8", "Pad8"},
    {EVT_PAD1, "NUMPAD_1", 0, "Numpad 1", "Pad1"},
    {EVT_PAD3, "NUMPAD_3", 0, "Numpad 3", "Pad3"},
    {EVT_PAD5, "NUMPAD_5", 0, "Numpad 5", "Pad5"},
    {EVT_PAD7, "NUMPAD_7", 0, "Numpad 7", "Pad7"},
    {EVT_PAD9, "NUMPAD_9", 0, "Numpad 9", "Pad9"},
    {EVT_PADPERIOD, "NUMPAD_PERIOD", 0, "Numpad .", "Pad."},
    {EVT_PADSLASHKEY, "NUMPAD_SLASH", 0, "Numpad /", "Pad/"},
    {EVT_PADASTERKEY, "NUMPAD_ASTERIX", 0, "Numpad *", "Pad*"},
    {EVT_PAD0, "NUMPAD_0", 0, "Numpad 0", "Pad0"},
    {EVT_PADMINUS, "NUMPAD_MINUS", 0, "Numpad -", "Pad-"},
    {EVT_PADENTER, "NUMPAD_ENTER", 0, "Numpad Enter", "PadEnter"},
    {EVT_PADPLUSKEY, "NUMPAD_PLUS", 0, "Numpad +", "Pad+"},
    {EVT_F1KEY, "F1", 0, "F1", ""},
    {EVT_F2KEY, "F2", 0, "F2", ""},
    {EVT_F3KEY, "F3", 0, "F3", ""},
    {EVT_F4KEY, "F4", 0, "F4", ""},
    {EVT_F5KEY, "F5", 0, "F5", ""},
    {EVT_F6KEY, "F6", 0, "F6", ""},
    {EVT_F7KEY, "F7", 0, "F7", ""},
    {EVT_F8KEY, "F8", 0, "F8", ""},
    {EVT_F9KEY, "F9", 0, "F9", ""},
    {EVT_F10KEY, "F10", 0, "F10", ""},
    {EVT_F11KEY, "F11", 0, "F11", ""},
    {EVT_F12KEY, "F12", 0, "F12", ""},
    {EVT_F13KEY, "F13", 0, "F13", ""},
    {EVT_F14KEY, "F14", 0, "F14", ""},
    {EVT_F15KEY, "F15", 0, "F15", ""},
    {EVT_F16KEY, "F16", 0, "F16", ""},
    {EVT_F17KEY, "F17", 0, "F17", ""},
    {EVT_F18KEY, "F18", 0, "F18", ""},
    {EVT_F19KEY, "F19", 0, "F19", ""},
    {EVT_F20KEY, "F20", 0, "F20", ""},
    {EVT_F21KEY, "F21", 0, "F21", ""},
    {EVT_F22KEY, "F22", 0, "F22", ""},
    {EVT_F23KEY, "F23", 0, "F23", ""},
    {EVT_F24KEY, "F24", 0, "F24", ""},
    {EVT_PAUSEKEY, "PAUSE", 0, "Pause", ""},
    {EVT_INSERTKEY, "INSERT", 0, "Insert", "Ins"},
    {EVT_HOMEKEY, "HOME", 0, "Home", ""},
    {EVT_PAGEUPKEY, "PAGE_UP", 0, "Page Up", "PgUp"},
    {EVT_PAGEDOWNKEY, "PAGE_DOWN", 0, "Page Down", "PgDown"},
    {EVT_ENDKEY, "END", 0, "End", ""},
    {0, "", 0, NULL, NULL},
    {EVT_MEDIAPLAY, "MEDIA_PLAY", 0, "Media Play/Pause", ">/||"},
    {EVT_MEDIASTOP, "MEDIA_STOP", 0, "Media Stop", "Stop"},
    {EVT_MEDIAFIRST, "MEDIA_FIRST", 0, "Media First", "|<<"},
    {EVT_MEDIALAST, "MEDIA_LAST", 0, "Media Last", ">>|"},
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
    /* Action Zones. */
    {EVT_ACTIONZONE_AREA, "ACTIONZONE_AREA", 0, "ActionZone Area", "AZone Area"},
    {EVT_ACTIONZONE_REGION, "ACTIONZONE_REGION", 0, "ActionZone Region", "AZone Region"},
    {EVT_ACTIONZONE_FULLSCREEN,
     "ACTIONZONE_FULLSCREEN",
     0,
     "ActionZone Fullscreen",
     "AZone FullScr"},
    /* xr */
    {EVT_XR_ACTION, "XR_ACTION", 0, "XR Action", ""},
    {0, NULL, 0, NULL, NULL},
};

/**
 * \note This contains overlapping items from:
 * - #rna_enum_event_value_keymouse_items
 * - #rna_enum_event_value_tweak_items
 *
 * This is needed for `km.keymap_items.new` value argument,
 * to accept values from different types.
 */
const EnumPropertyItem rna_enum_event_value_all_items[] = {
    {KM_ANY, "ANY", 0, "Any", ""},
    {KM_PRESS, "PRESS", 0, "Press", ""},
    {KM_RELEASE, "RELEASE", 0, "Release", ""},
    {KM_CLICK, "CLICK", 0, "Click", ""},
    {KM_DBL_CLICK, "DOUBLE_CLICK", 0, "Double Click", ""},
    {KM_CLICK_DRAG, "CLICK_DRAG", 0, "Click Drag", ""},
    {EVT_GESTURE_N, "NORTH", 0, "North", ""},
    {EVT_GESTURE_NE, "NORTH_EAST", 0, "North-East", ""},
    {EVT_GESTURE_E, "EAST", 0, "East", ""},
    {EVT_GESTURE_SE, "SOUTH_EAST", 0, "South-East", ""},
    {EVT_GESTURE_S, "SOUTH", 0, "South", ""},
    {EVT_GESTURE_SW, "SOUTH_WEST", 0, "South-West", ""},
    {EVT_GESTURE_W, "WEST", 0, "West", ""},
    {EVT_GESTURE_NW, "NORTH_WEST", 0, "North-West", ""},
    {KM_NOTHING, "NOTHING", 0, "Nothing", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_event_value_keymouse_items[] = {
    {KM_ANY, "ANY", 0, "Any", ""},
    {KM_PRESS, "PRESS", 0, "Press", ""},
    {KM_RELEASE, "RELEASE", 0, "Release", ""},
    {KM_CLICK, "CLICK", 0, "Click", ""},
    {KM_DBL_CLICK, "DOUBLE_CLICK", 0, "Double Click", ""},
    {KM_CLICK_DRAG, "CLICK_DRAG", 0, "Click Drag", ""},
    /* Used for NDOF and trackpad events. */
    {KM_NOTHING, "NOTHING", 0, "Nothing", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_event_value_tweak_items[] = {
    {KM_ANY, "ANY", 0, "Any", ""},
    {EVT_GESTURE_N, "NORTH", 0, "North", ""},
    {EVT_GESTURE_NE, "NORTH_EAST", 0, "North-East", ""},
    {EVT_GESTURE_E, "EAST", 0, "East", ""},
    {EVT_GESTURE_SE, "SOUTH_EAST", 0, "South-East", ""},
    {EVT_GESTURE_S, "SOUTH", 0, "South", ""},
    {EVT_GESTURE_SW, "SOUTH_WEST", 0, "South-West", ""},
    {EVT_GESTURE_W, "WEST", 0, "West", ""},
    {EVT_GESTURE_NW, "NORTH_WEST", 0, "North-West", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_keymap_propvalue_items[] = {
    {0, "NONE", 0, "", ""},
    {0, NULL, 0, NULL, NULL},
};

/* Mask event types used in keymap items. */
const EnumPropertyItem rna_enum_event_type_mask_items[] = {
    {EVT_TYPE_MASK_KEYBOARD_MODIFIER, "KEYBOARD_MODIFIER", 0, "Keyboard Modifier", ""},
    {EVT_TYPE_MASK_KEYBOARD, "KEYBOARD", 0, "Keyboard", ""},
    {EVT_TYPE_MASK_MOUSE_WHEEL, "MOUSE_WHEEL", 0, "Mouse Wheel", ""},
    {EVT_TYPE_MASK_MOUSE_GESTURE, "MOUSE_GESTURE", 0, "Mouse Gesture", ""},
    {EVT_TYPE_MASK_MOUSE_BUTTON, "MOUSE_BUTTON", 0, "Mouse Button", ""},
    {EVT_TYPE_MASK_MOUSE, "MOUSE", 0, "Mouse", ""},
    {EVT_TYPE_MASK_NDOF, "NDOF", 0, "NDOF", ""},
    {EVT_TYPE_MASK_TWEAK, "TWEAK", 0, "Tweak", ""},
    {EVT_TYPE_MASK_ACTIONZONE, "ACTIONZONE", 0, "Action Zone", ""},
    {0, NULL, 0, NULL, NULL},
};

#if 0
static const EnumPropertyItem keymap_modifiers_items[] = {
    {KM_ANY, "ANY", 0, "Any", ""},
    {0, "NONE", 0, "None", ""},
    {KM_MOD_HELD, "HELD", 0, "Held", ""},
    {0, NULL, 0, NULL, NULL},
};
#endif

const EnumPropertyItem rna_enum_operator_type_flag_items[] = {
    {OPTYPE_REGISTER,
     "REGISTER",
     0,
     "Register",
     "Display in the info window and support the redo toolbar panel"},
    {OPTYPE_UNDO, "UNDO", 0, "Undo", "Push an undo event (needed for operator redo)"},
    {OPTYPE_UNDO_GROUPED,
     "UNDO_GROUPED",
     0,
     "Grouped Undo",
     "Push a single undo event for repeated instances of this operator"},
    {OPTYPE_BLOCKING, "BLOCKING", 0, "Blocking", "Block anything else from using the cursor"},
    {OPTYPE_MACRO, "MACRO", 0, "Macro", "Use to check if an operator is a macro"},
    {OPTYPE_GRAB_CURSOR_XY,
     "GRAB_CURSOR",
     0,
     "Grab Pointer",
     "Use so the operator grabs the mouse focus, enables wrapping when continuous grab "
     "is enabled"},
    {OPTYPE_GRAB_CURSOR_X, "GRAB_CURSOR_X", 0, "Grab Pointer X", "Grab, only warping the X axis"},
    {OPTYPE_GRAB_CURSOR_Y, "GRAB_CURSOR_Y", 0, "Grab Pointer Y", "Grab, only warping the Y axis"},
    {OPTYPE_DEPENDS_ON_CURSOR,
     "DEPENDS_ON_CURSOR",
     0,
     "Depends on Cursor",
     "The initial cursor location is used, "
     "when running from a menus or buttons the user is prompted to place the cursor "
     "before beginning the operation"},
    {OPTYPE_PRESET, "PRESET", 0, "Preset", "Display a preset button with the operators settings"},
    {OPTYPE_INTERNAL, "INTERNAL", 0, "Internal", "Removes the operator from search results"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_operator_return_items[] = {
    {OPERATOR_RUNNING_MODAL,
     "RUNNING_MODAL",
     0,
     "Running Modal",
     "Keep the operator running with blender"},
    {OPERATOR_CANCELLED,
     "CANCELLED",
     0,
     "Cancelled",
     "The operator exited without doing anything, so no undo entry should be pushed"},
    {OPERATOR_FINISHED,
     "FINISHED",
     0,
     "Finished",
     "The operator exited after completing its action"},
    /* used as a flag */
    {OPERATOR_PASS_THROUGH, "PASS_THROUGH", 0, "Pass Through", "Do nothing and pass the event on"},
    {OPERATOR_INTERFACE, "INTERFACE", 0, "Interface", "Handled but not executed (popup menus)"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_operator_property_tags[] = {
    {OP_PROP_TAG_ADVANCED,
     "ADVANCED",
     0,
     "Advanced",
     "The property is advanced so UI is suggested to hide it"},
    {0, NULL, 0, NULL, NULL},
};

/* flag/enum */
const EnumPropertyItem rna_enum_wm_report_items[] = {
    {RPT_DEBUG, "DEBUG", 0, "Debug", ""},
    {RPT_INFO, "INFO", 0, "Info", ""},
    {RPT_OPERATOR, "OPERATOR", 0, "Operator", ""},
    {RPT_PROPERTY, "PROPERTY", 0, "Property", ""},
    {RPT_WARNING, "WARNING", 0, "Warning", ""},
    {RPT_ERROR, "ERROR", 0, "Error", ""},
    {RPT_ERROR_INVALID_INPUT, "ERROR_INVALID_INPUT", 0, "Invalid Input", ""},
    {RPT_ERROR_INVALID_CONTEXT, "ERROR_INVALID_CONTEXT", 0, "Invalid Context", ""},
    {RPT_ERROR_OUT_OF_MEMORY, "ERROR_OUT_OF_MEMORY", 0, "Out of Memory", ""},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "BLI_string_utils.h"

#  include "WM_api.h"

#  include "DNA_object_types.h"
#  include "DNA_workspace_types.h"

#  include "ED_screen.h"

#  include "UI_interface.h"

#  include "BKE_global.h"
#  include "BKE_idprop.h"

#  include "MEM_guardedalloc.h"

#  ifdef WITH_PYTHON
#    include "BPY_extern.h"
#  endif

static wmOperator *rna_OperatorProperties_find_operator(PointerRNA *ptr)
{
  wmWindowManager *wm = (wmWindowManager *)ptr->owner_id;

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

  if (op) {
    return op->type->srna;
  }
  else {
    return ptr->type;
  }
}

static IDProperty **rna_OperatorProperties_idprops(PointerRNA *ptr)
{
  return (IDProperty **)&ptr->data;
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

static bool rna_Operator_has_reports_get(PointerRNA *ptr)
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

  PointerRNA result;
  WM_operator_properties_create_ptr(&result, op->type);
  result.data = op->properties;
  return result;
}

static PointerRNA rna_OperatorMacro_properties_get(PointerRNA *ptr)
{
  wmOperatorTypeMacro *otmacro = (wmOperatorTypeMacro *)ptr->data;
  wmOperatorType *ot = WM_operatortype_find(otmacro->idname, true);

  PointerRNA result;
  WM_operator_properties_create_ptr(&result, ot);
  result.data = otmacro->properties;
  return result;
}

static const EnumPropertyItem *rna_Event_value_itemf(bContext *UNUSED(C),
                                                     PointerRNA *ptr,
                                                     PropertyRNA *UNUSED(prop),
                                                     bool *UNUSED(r_free))
{
  const wmEvent *event = ptr->data;
  if (ISTWEAK(event->type)) {
    return rna_enum_event_value_tweak_items;
  }
  return rna_enum_event_value_all_items;
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
    if (BLI_str_utf8_as_unicode_step_or_error(event->utf8_buf, sizeof(event->utf8_buf), &len) !=
        BLI_UTF8_ERR)
      memcpy(value, event->utf8_buf, len);
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

static bool rna_Event_is_repeat_get(PointerRNA *ptr)
{
  const wmEvent *event = ptr->data;
  return event->is_repeat;
}

static float rna_Event_pressure_get(PointerRNA *ptr)
{
  const wmEvent *event = ptr->data;
  return WM_event_tablet_data(event, NULL, NULL);
}

static bool rna_Event_is_tablet_get(PointerRNA *ptr)
{
  const wmEvent *event = ptr->data;
  return WM_event_is_tablet(event);
}

static void rna_Event_tilt_get(PointerRNA *ptr, float *values)
{
  wmEvent *event = ptr->data;
  WM_event_tablet_data(event, NULL, values);
}

static PointerRNA rna_Event_xr_get(PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  wmEvent *event = ptr->data;
  wmXrActionData *actiondata = WM_event_is_xr(event) ? event->customdata : NULL;
  return rna_pointer_inherit_refine(ptr, &RNA_XrEventData, actiondata);
#  else
  UNUSED_VARS(ptr);
  return PointerRNA_NULL;
#  endif
}

static PointerRNA rna_PopupMenu_layout_get(PointerRNA *ptr)
{
  struct uiPopupMenu *pup = ptr->data;
  uiLayout *layout = UI_popup_menu_layout(pup);

  PointerRNA rptr;
  RNA_pointer_create(ptr->owner_id, &RNA_UILayout, layout, &rptr);

  return rptr;
}

static PointerRNA rna_PopoverMenu_layout_get(PointerRNA *ptr)
{
  struct uiPopover *pup = ptr->data;
  uiLayout *layout = UI_popover_layout(pup);

  PointerRNA rptr;
  RNA_pointer_create(ptr->owner_id, &RNA_UILayout, layout, &rptr);

  return rptr;
}

static PointerRNA rna_PieMenu_layout_get(PointerRNA *ptr)
{
  struct uiPieMenu *pie = ptr->data;
  uiLayout *layout = UI_pie_menu_layout(pie);

  PointerRNA rptr;
  RNA_pointer_create(ptr->owner_id, &RNA_UILayout, layout, &rptr);

  return rptr;
}

static void rna_Window_scene_set(PointerRNA *ptr,
                                 PointerRNA value,
                                 struct ReportList *UNUSED(reports))
{
  wmWindow *win = ptr->data;

  if (value.data == NULL) {
    return;
  }

  win->new_scene = value.data;
}

static void rna_Window_scene_update(bContext *C, PointerRNA *ptr)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = ptr->data;

  /* Exception: must use context so notifier gets to the right window. */
  if (win->new_scene) {
#  ifdef WITH_PYTHON
    BPy_BEGIN_ALLOW_THREADS;
#  endif

    WM_window_set_active_scene(bmain, C, win, win->new_scene);

#  ifdef WITH_PYTHON
    BPy_END_ALLOW_THREADS;
#  endif

    wmWindowManager *wm = CTX_wm_manager(C);
    WM_event_add_notifier_ex(wm, win, NC_SCENE | ND_SCENEBROWSE, win->new_scene);

    if (G.debug & G_DEBUG) {
      printf("scene set %p\n", win->new_scene);
    }

    win->new_scene = NULL;
  }
}

static PointerRNA rna_Window_workspace_get(PointerRNA *ptr)
{
  wmWindow *win = ptr->data;
  return rna_pointer_inherit_refine(
      ptr, &RNA_WorkSpace, BKE_workspace_active_get(win->workspace_hook));
}

static void rna_Window_workspace_set(PointerRNA *ptr,
                                     PointerRNA value,
                                     struct ReportList *UNUSED(reports))
{
  wmWindow *win = (wmWindow *)ptr->data;

  /* disallow ID-browsing away from temp screens */
  if (WM_window_is_temp_screen(win)) {
    return;
  }
  if (value.data == NULL) {
    return;
  }

  /* exception: can't set workspaces inside of area/region handlers */
  win->workspace_hook->temp_workspace_store = value.data;
}

static void rna_Window_workspace_update(bContext *C, PointerRNA *ptr)
{
  wmWindow *win = ptr->data;
  WorkSpace *new_workspace = win->workspace_hook->temp_workspace_store;

  /* exception: can't set screens inside of area/region handlers,
   * and must use context so notifier gets to the right window */
  if (new_workspace) {
    wmWindowManager *wm = CTX_wm_manager(C);
    WM_event_add_notifier_ex(wm, win, NC_SCREEN | ND_WORKSPACE_SET, new_workspace);
    win->workspace_hook->temp_workspace_store = NULL;
  }
}

PointerRNA rna_Window_screen_get(PointerRNA *ptr)
{
  wmWindow *win = ptr->data;
  return rna_pointer_inherit_refine(
      ptr, &RNA_Screen, BKE_workspace_active_screen_get(win->workspace_hook));
}

static void rna_Window_screen_set(PointerRNA *ptr,
                                  PointerRNA value,
                                  struct ReportList *UNUSED(reports))
{
  wmWindow *win = ptr->data;
  WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
  WorkSpaceLayout *layout_new;
  const bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);

  /* disallow ID-browsing away from temp screens */
  if (screen->temp) {
    return;
  }
  if (value.data == NULL) {
    return;
  }

  /* exception: can't set screens inside of area/region handlers */
  layout_new = BKE_workspace_layout_find(workspace, value.data);
  win->workspace_hook->temp_layout_store = layout_new;
}

static bool rna_Window_screen_assign_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
  bScreen *screen = (bScreen *)value.owner_id;
  return !screen->temp;
}

static void rna_workspace_screen_update(bContext *C, PointerRNA *ptr)
{
  wmWindow *win = ptr->data;
  WorkSpaceLayout *layout_new = win->workspace_hook->temp_layout_store;

  /* exception: can't set screens inside of area/region handlers,
   * and must use context so notifier gets to the right window */
  if (layout_new) {
    wmWindowManager *wm = CTX_wm_manager(C);
    WM_event_add_notifier_ex(wm, win, NC_SCREEN | ND_LAYOUTBROWSE, layout_new);
    win->workspace_hook->temp_layout_store = NULL;
  }
}

static PointerRNA rna_Window_view_layer_get(PointerRNA *ptr)
{
  wmWindow *win = ptr->data;
  Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  PointerRNA scene_ptr;

  RNA_id_pointer_create(&scene->id, &scene_ptr);
  return rna_pointer_inherit_refine(&scene_ptr, &RNA_ViewLayer, view_layer);
}

static void rna_Window_view_layer_set(PointerRNA *ptr,
                                      PointerRNA value,
                                      struct ReportList *UNUSED(reports))
{
  wmWindow *win = ptr->data;
  ViewLayer *view_layer = value.data;

  WM_window_set_active_view_layer(win, view_layer);
}

static PointerRNA rna_KeyMapItem_properties_get(PointerRNA *ptr)
{
  wmKeyMapItem *kmi = ptr->data;

  if (kmi->ptr) {
    BLI_assert(kmi->ptr->owner_id == NULL);
    return *(kmi->ptr);
  }

  // return rna_pointer_inherit_refine(ptr, &RNA_OperatorProperties, op->properties);
  return PointerRNA_NULL;
}

static int rna_wmKeyMapItem_map_type_get(PointerRNA *ptr)
{
  wmKeyMapItem *kmi = ptr->data;

  return WM_keymap_item_map_type_get(kmi);
}

static void rna_wmKeyMapItem_map_type_set(PointerRNA *ptr, int value)
{
  wmKeyMapItem *kmi = ptr->data;
  int map_type = rna_wmKeyMapItem_map_type_get(ptr);

  if (value != map_type) {
    switch (value) {
      case KMI_TYPE_KEYBOARD:
        kmi->type = EVT_AKEY;
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
  if (value == EVT_ESCKEY) {
    /* pass */
  }
  else if (value >= EVT_AKEY) {
    kmi->keymodifier = value;
  }
  else {
    kmi->keymodifier = 0;
  }
}

static const EnumPropertyItem *rna_KeyMapItem_type_itemf(bContext *UNUSED(C),
                                                         PointerRNA *ptr,
                                                         PropertyRNA *UNUSED(prop),
                                                         bool *UNUSED(r_free))
{
  int map_type = rna_wmKeyMapItem_map_type_get(ptr);

  if (map_type == KMI_TYPE_MOUSE) {
    return event_mouse_type_items;
  }
  if (map_type == KMI_TYPE_TWEAK) {
    return event_tweak_type_items;
  }
  if (map_type == KMI_TYPE_TIMER) {
    return event_timer_type_items;
  }
  if (map_type == KMI_TYPE_NDOF) {
    return event_ndof_type_items;
  }
  if (map_type == KMI_TYPE_TEXTINPUT) {
    return event_textinput_type_items;
  }
  else {
    return rna_enum_event_type_items;
  }
}

static const EnumPropertyItem *rna_KeyMapItem_value_itemf(bContext *UNUSED(C),
                                                          PointerRNA *ptr,
                                                          PropertyRNA *UNUSED(prop),
                                                          bool *UNUSED(r_free))
{
  int map_type = rna_wmKeyMapItem_map_type_get(ptr);

  if (map_type == KMI_TYPE_MOUSE || map_type == KMI_TYPE_KEYBOARD || map_type == KMI_TYPE_NDOF) {
    return rna_enum_event_value_keymouse_items;
  }
  if (map_type == KMI_TYPE_TWEAK) {
    return rna_enum_event_value_tweak_items;
  }
  else {
    return rna_enum_event_value_all_items;
  }
}

static const EnumPropertyItem *rna_KeyMapItem_propvalue_itemf(bContext *C,
                                                              PointerRNA *ptr,
                                                              PropertyRNA *UNUSED(prop),
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

static bool rna_KeyMapItem_any_get(PointerRNA *ptr)
{
  wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;

  if (kmi->shift == KM_ANY && kmi->ctrl == KM_ANY && kmi->alt == KM_ANY && kmi->oskey == KM_ANY) {
    return 1;
  }
  else {
    return 0;
  }
}

static void rna_KeyMapItem_any_set(PointerRNA *ptr, bool value)
{
  wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;

  if (value) {
    kmi->shift = kmi->ctrl = kmi->alt = kmi->oskey = KM_ANY;
  }
  else {
    kmi->shift = kmi->ctrl = kmi->alt = kmi->oskey = 0;
  }
}

static bool rna_KeyMapItem_shift_get(PointerRNA *ptr)
{
  wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;
  return kmi->shift != 0;
}

static bool rna_KeyMapItem_ctrl_get(PointerRNA *ptr)
{
  wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;
  return kmi->ctrl != 0;
}

static bool rna_KeyMapItem_alt_get(PointerRNA *ptr)
{
  wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;
  return kmi->alt != 0;
}

static bool rna_KeyMapItem_oskey_get(PointerRNA *ptr)
{
  wmKeyMapItem *kmi = (wmKeyMapItem *)ptr->data;
  return kmi->oskey != 0;
}

static PointerRNA rna_WindowManager_active_keyconfig_get(PointerRNA *ptr)
{
  wmWindowManager *wm = ptr->data;
  wmKeyConfig *kc;

  kc = BLI_findstring(&wm->keyconfigs, U.keyconfigstr, offsetof(wmKeyConfig, idname));

  if (!kc) {
    kc = wm->defaultconf;
  }

  return rna_pointer_inherit_refine(ptr, &RNA_KeyConfig, kc);
}

static void rna_WindowManager_active_keyconfig_set(PointerRNA *ptr,
                                                   PointerRNA value,
                                                   struct ReportList *UNUSED(reports))
{
  wmWindowManager *wm = ptr->data;
  wmKeyConfig *kc = value.data;

  if (kc) {
    WM_keyconfig_set_active(wm, kc->idname);
  }
}

/* -------------------------------------------------------------------- */
/** \name Key Config Preferences
 * \{ */

static PointerRNA rna_wmKeyConfig_preferences_get(PointerRNA *ptr)
{
  wmKeyConfig *kc = ptr->data;
  wmKeyConfigPrefType_Runtime *kpt_rt = BKE_keyconfig_pref_type_find(kc->idname, true);
  if (kpt_rt) {
    wmKeyConfigPref *kpt = BKE_keyconfig_pref_ensure(&U, kc->idname);
    return rna_pointer_inherit_refine(ptr, kpt_rt->rna_ext.srna, kpt->prop);
  }
  else {
    return PointerRNA_NULL;
  }
}

static IDProperty **rna_wmKeyConfigPref_idprops(PointerRNA *ptr)
{
  return (IDProperty **)&ptr->data;
}

static void rna_wmKeyConfigPref_unregister(Main *UNUSED(bmain), StructRNA *type)
{
  wmKeyConfigPrefType_Runtime *kpt_rt = RNA_struct_blender_type_get(type);

  if (!kpt_rt) {
    return;
  }

  RNA_struct_free_extension(type, &kpt_rt->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  /* Possible we're not in the preferences if they have been reset. */
  BKE_keyconfig_pref_type_remove(kpt_rt);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, NULL);
}

static StructRNA *rna_wmKeyConfigPref_register(Main *bmain,
                                               ReportList *reports,
                                               void *data,
                                               const char *identifier,
                                               StructValidateFunc validate,
                                               StructCallbackFunc call,
                                               StructFreeFunc free)
{
  wmKeyConfigPrefType_Runtime *kpt_rt, dummy_kpt_rt = {{'\0'}};
  wmKeyConfigPref dummy_kpt = {NULL};
  PointerRNA dummy_ptr;
  // int have_function[1];

  /* setup dummy keyconf-prefs & keyconf-prefs type to store static properties in */
  RNA_pointer_create(NULL, &RNA_KeyConfigPreferences, &dummy_kpt, &dummy_ptr);

  /* validate the python class */
  if (validate(&dummy_ptr, data, NULL /* have_function */) != 0) {
    return NULL;
  }

  STRNCPY(dummy_kpt_rt.idname, dummy_kpt.idname);
  if (strlen(identifier) >= sizeof(dummy_kpt_rt.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering key-config preferences class: '%s' is too long, maximum length is %d",
                identifier,
                (int)sizeof(dummy_kpt_rt.idname));
    return NULL;
  }

  /* check if we have registered this keyconf-prefs type before, and remove it */
  kpt_rt = BKE_keyconfig_pref_type_find(dummy_kpt.idname, true);
  if (kpt_rt && kpt_rt->rna_ext.srna) {
    rna_wmKeyConfigPref_unregister(bmain, kpt_rt->rna_ext.srna);
  }

  /* create a new keyconf-prefs type */
  kpt_rt = MEM_mallocN(sizeof(wmKeyConfigPrefType_Runtime), "keyconfigpreftype");
  memcpy(kpt_rt, &dummy_kpt_rt, sizeof(dummy_kpt_rt));

  BKE_keyconfig_pref_type_add(kpt_rt);

  kpt_rt->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, identifier, &RNA_KeyConfigPreferences);
  kpt_rt->rna_ext.data = data;
  kpt_rt->rna_ext.call = call;
  kpt_rt->rna_ext.free = free;
  RNA_struct_blender_type_set(kpt_rt->rna_ext.srna, kpt_rt);

  //  kpt_rt->draw = (have_function[0]) ? header_draw : NULL;

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, NULL);

  return kpt_rt->rna_ext.srna;
}

/* placeholder, doesn't do anything useful yet */
static StructRNA *rna_wmKeyConfigPref_refine(PointerRNA *ptr)
{
  return (ptr->type) ? ptr->type : &RNA_KeyConfigPreferences;
}

/** \} */

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

    WM_keymap_item_properties_reset(kmi, NULL);
  }
}

static void rna_wmKeyMapItem_name_get(PointerRNA *ptr, char *value)
{
  wmKeyMapItem *kmi = ptr->data;
  wmOperatorType *ot = WM_operatortype_find(kmi->idname, 1);
  strcpy(value, ot ? WM_operatortype_name(ot, kmi->ptr) : kmi->idname);
}

static int rna_wmKeyMapItem_name_length(PointerRNA *ptr)
{
  wmKeyMapItem *kmi = ptr->data;
  wmOperatorType *ot = WM_operatortype_find(kmi->idname, 1);
  return strlen(ot ? WM_operatortype_name(ot, kmi->ptr) : kmi->idname);
}

static bool rna_KeyMapItem_userdefined_get(PointerRNA *ptr)
{
  wmKeyMapItem *kmi = ptr->data;
  return kmi->id < 0;
}

static PointerRNA rna_WindowManager_xr_session_state_get(PointerRNA *ptr)
{
  wmWindowManager *wm = ptr->data;
  struct wmXrSessionState *state =
#  ifdef WITH_XR_OPENXR
      WM_xr_session_state_handle_get(&wm->xr);
#  else
      NULL;
  UNUSED_VARS(wm);
#  endif

  return rna_pointer_inherit_refine(ptr, &RNA_XrSessionState, state);
}

#  ifdef WITH_PYTHON

static bool rna_operator_poll_cb(bContext *C, wmOperatorType *ot)
{
  extern FunctionRNA rna_Operator_poll_func;

  PointerRNA ptr;
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool visible;

  RNA_pointer_create(NULL, ot->rna_ext.srna, NULL, &ptr); /* dummy */
  func = &rna_Operator_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  ot->rna_ext.call(C, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "visible", &ret);
  visible = *(bool *)ret;

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

  RNA_pointer_create(NULL, op->type->rna_ext.srna, op, &opr);
  func = &rna_Operator_execute_func; /* RNA_struct_find_function(&opr, "execute"); */

  RNA_parameter_list_create(&list, &opr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  op->type->rna_ext.call(C, &opr, func, &list);

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

  RNA_pointer_create(NULL, op->type->rna_ext.srna, op, &opr);
  func = &rna_Operator_check_func; /* RNA_struct_find_function(&opr, "check"); */

  RNA_parameter_list_create(&list, &opr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  op->type->rna_ext.call(C, &opr, func, &list);

  RNA_parameter_get_lookup(&list, "result", &ret);
  result = (*(bool *)ret) != 0;

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

  RNA_pointer_create(NULL, op->type->rna_ext.srna, op, &opr);
  func = &rna_Operator_invoke_func; /* RNA_struct_find_function(&opr, "invoke"); */

  RNA_parameter_list_create(&list, &opr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "event", &event);
  op->type->rna_ext.call(C, &opr, func, &list);

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

  RNA_pointer_create(NULL, op->type->rna_ext.srna, op, &opr);
  func = &rna_Operator_modal_func; /* RNA_struct_find_function(&opr, "modal"); */

  RNA_parameter_list_create(&list, &opr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "event", &event);
  op->type->rna_ext.call(C, &opr, func, &list);

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

  RNA_pointer_create(NULL, op->type->rna_ext.srna, op, &opr);
  func = &rna_Operator_draw_func; /* RNA_struct_find_function(&opr, "draw"); */

  RNA_parameter_list_create(&list, &opr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  op->type->rna_ext.call(C, &opr, func, &list);

  RNA_parameter_list_free(&list);
}

/* same as exec(), but call cancel */
static void rna_operator_cancel_cb(bContext *C, wmOperator *op)
{
  extern FunctionRNA rna_Operator_cancel_func;

  PointerRNA opr;
  ParameterList list;
  FunctionRNA *func;

  RNA_pointer_create(NULL, op->type->rna_ext.srna, op, &opr);
  func = &rna_Operator_cancel_func; /* RNA_struct_find_function(&opr, "cancel"); */

  RNA_parameter_list_create(&list, &opr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  op->type->rna_ext.call(C, &opr, func, &list);

  RNA_parameter_list_free(&list);
}

static char *rna_operator_description_cb(bContext *C, wmOperatorType *ot, PointerRNA *prop_ptr)
{
  extern FunctionRNA rna_Operator_description_func;

  PointerRNA ptr;
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  char *result;

  RNA_pointer_create(NULL, ot->rna_ext.srna, NULL, &ptr); /* dummy */
  func = &rna_Operator_description_func; /* RNA_struct_find_function(&ptr, "description"); */

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "properties", prop_ptr);
  ot->rna_ext.call(C, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "result", &ret);
  result = (char *)ret;

  if (result && result[0]) {
    result = BLI_strdup(result);
  }
  else {
    result = NULL;
  }

  RNA_parameter_list_free(&list);

  return result;
}

static void rna_Operator_unregister(struct Main *bmain, StructRNA *type);

/* bpy_operator_wrap.c */
extern void BPY_RNA_operator_wrapper(wmOperatorType *ot, void *userdata);
extern void BPY_RNA_operator_macro_wrapper(wmOperatorType *ot, void *userdata);

static StructRNA *rna_Operator_register(Main *bmain,
                                        ReportList *reports,
                                        void *data,
                                        const char *identifier,
                                        StructValidateFunc validate,
                                        StructCallbackFunc call,
                                        StructFreeFunc free)
{
  wmOperatorType dummyot = {NULL};
  wmOperator dummyop = {NULL};
  PointerRNA dummyotr;
  int have_function[8];

  struct {
    char idname[OP_MAX_TYPENAME];
    char name[OP_MAX_TYPENAME];
    char description[RNA_DYN_DESCR_MAX];
    char translation_context[RNA_DYN_DESCR_MAX];
    char undo_group[OP_MAX_TYPENAME];
  } temp_buffers;

  /* setup dummy operator & operator type to store static properties in */
  dummyop.type = &dummyot;
  dummyot.idname = temp_buffers.idname;           /* only assign the pointer, string is NULL'd */
  dummyot.name = temp_buffers.name;               /* only assign the pointer, string is NULL'd */
  dummyot.description = temp_buffers.description; /* only assign the pointer, string is NULL'd */
  dummyot.translation_context =
      temp_buffers.translation_context;         /* only assign the pointer, string is NULL'd */
  dummyot.undo_group = temp_buffers.undo_group; /* only assign the pointer, string is NULL'd */
  RNA_pointer_create(NULL, &RNA_Operator, &dummyop, &dummyotr);

  /* clear in case they are left unset */
  temp_buffers.idname[0] = temp_buffers.name[0] = temp_buffers.description[0] =
      temp_buffers.undo_group[0] = temp_buffers.translation_context[0] = '\0';

  /* validate the python class */
  if (validate(&dummyotr, data, have_function) != 0) {
    return NULL;
  }

  /* check if we have registered this operator type before, and remove it */
  {
    wmOperatorType *ot = WM_operatortype_find(dummyot.idname, true);
    if (ot && ot->rna_ext.srna) {
      rna_Operator_unregister(bmain, ot->rna_ext.srna);
    }
  }

  if (!WM_operator_py_idname_ok_or_report(reports, identifier, dummyot.idname)) {
    return NULL;
  }

  char idname_conv[sizeof(dummyop.idname)];
  WM_operator_bl_idname(idname_conv, dummyot.idname); /* convert the idname from python */

  if (!RNA_struct_available_or_report(reports, idname_conv)) {
    return NULL;
  }

  /* We have to set default context if the class doesn't define it. */
  if (temp_buffers.translation_context[0] == '\0') {
    STRNCPY(temp_buffers.translation_context, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  }

  /* Convert foo.bar to FOO_OT_bar
   * allocate all strings at once. */
  {
    const char *strings[] = {
        idname_conv,
        temp_buffers.name,
        temp_buffers.description,
        temp_buffers.translation_context,
        temp_buffers.undo_group,
    };
    char *strings_table[ARRAY_SIZE(strings)];
    BLI_string_join_array_by_sep_char_with_tableN(
        '\0', strings_table, strings, ARRAY_SIZE(strings));

    dummyot.idname = strings_table[0]; /* allocated string stored here */
    dummyot.name = strings_table[1];
    dummyot.description = *strings_table[2] ? strings_table[2] : NULL;
    dummyot.translation_context = strings_table[3];
    dummyot.undo_group = strings_table[4];
    BLI_assert(ARRAY_SIZE(strings) == 5);
  }

  /* XXX, this doubles up with the operator name T29666.
   * for now just remove from dir(bpy.types) */

  /* create a new operator type */
  dummyot.rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, dummyot.idname, &RNA_Operator);

  /* Operator properties are registered separately. */
  RNA_def_struct_flag(dummyot.rna_ext.srna, STRUCT_NO_IDPROPERTIES);

  RNA_def_struct_property_tags(dummyot.rna_ext.srna, rna_enum_operator_property_tags);
  RNA_def_struct_translation_context(dummyot.rna_ext.srna, dummyot.translation_context);
  dummyot.rna_ext.data = data;
  dummyot.rna_ext.call = call;
  dummyot.rna_ext.free = free;

  dummyot.pyop_poll = (have_function[0]) ? rna_operator_poll_cb : NULL;
  dummyot.exec = (have_function[1]) ? rna_operator_execute_cb : NULL;
  dummyot.check = (have_function[2]) ? rna_operator_check_cb : NULL;
  dummyot.invoke = (have_function[3]) ? rna_operator_invoke_cb : NULL;
  dummyot.modal = (have_function[4]) ? rna_operator_modal_cb : NULL;
  dummyot.ui = (have_function[5]) ? rna_operator_draw_cb : NULL;
  dummyot.cancel = (have_function[6]) ? rna_operator_cancel_cb : NULL;
  dummyot.get_description = (have_function[7]) ? rna_operator_description_cb : NULL;
  WM_operatortype_append_ptr(BPY_RNA_operator_wrapper, (void *)&dummyot);

  /* update while blender is running */
  WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

  return dummyot.rna_ext.srna;
}

static void rna_Operator_unregister(struct Main *bmain, StructRNA *type)
{
  const char *idname;
  wmOperatorType *ot = RNA_struct_blender_type_get(type);
  wmWindowManager *wm;

  if (!ot) {
    return;
  }

  /* update while blender is running */
  wm = bmain->wm.first;
  if (wm) {
    WM_operator_stack_clear(wm);

    WM_operator_handlers_clear(wm, ot);
  }
  WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

  RNA_struct_free_extension(type, &ot->rna_ext);

  idname = ot->idname;
  WM_operatortype_remove_ptr(ot);

  /* Not to be confused with the RNA_struct_free that WM_operatortype_remove calls,
   * they are 2 different srna's. */
  RNA_struct_free(&BLENDER_RNA, type);

  MEM_freeN((void *)idname);
}

static void **rna_Operator_instance(PointerRNA *ptr)
{
  wmOperator *op = ptr->data;
  return &op->py_instance;
}

static StructRNA *rna_MacroOperator_register(Main *bmain,
                                             ReportList *reports,
                                             void *data,
                                             const char *identifier,
                                             StructValidateFunc validate,
                                             StructCallbackFunc call,
                                             StructFreeFunc free)
{
  wmOperatorType dummyot = {NULL};
  wmOperator dummyop = {NULL};
  PointerRNA dummyotr;
  int have_function[4];

  struct {
    char idname[OP_MAX_TYPENAME];
    char name[OP_MAX_TYPENAME];
    char description[RNA_DYN_DESCR_MAX];
    char translation_context[RNA_DYN_DESCR_MAX];
    char undo_group[OP_MAX_TYPENAME];
  } temp_buffers;

  /* setup dummy operator & operator type to store static properties in */
  dummyop.type = &dummyot;
  dummyot.idname = temp_buffers.idname;           /* only assign the pointer, string is NULL'd */
  dummyot.name = temp_buffers.name;               /* only assign the pointer, string is NULL'd */
  dummyot.description = temp_buffers.description; /* only assign the pointer, string is NULL'd */
  dummyot.translation_context =
      temp_buffers.translation_context;         /* only assign the pointer, string is NULL'd */
  dummyot.undo_group = temp_buffers.undo_group; /* only assign the pointer, string is NULL'd */
  RNA_pointer_create(NULL, &RNA_Macro, &dummyop, &dummyotr);

  /* clear in case they are left unset */
  temp_buffers.idname[0] = temp_buffers.name[0] = temp_buffers.description[0] =
      temp_buffers.undo_group[0] = temp_buffers.translation_context[0] = '\0';

  /* validate the python class */
  if (validate(&dummyotr, data, have_function) != 0) {
    return NULL;
  }

  if (strlen(identifier) >= sizeof(dummyop.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering operator class: '%s' is too long, maximum length is %d",
                identifier,
                (int)sizeof(dummyop.idname));
    return NULL;
  }

  /* check if we have registered this operator type before, and remove it */
  {
    wmOperatorType *ot = WM_operatortype_find(dummyot.idname, true);
    if (ot && ot->rna_ext.srna) {
      rna_Operator_unregister(bmain, ot->rna_ext.srna);
    }
  }

  if (!WM_operator_py_idname_ok_or_report(reports, identifier, dummyot.idname)) {
    return NULL;
  }

  char idname_conv[sizeof(dummyop.idname)];
  WM_operator_bl_idname(idname_conv, dummyot.idname); /* convert the idname from python */

  if (!RNA_struct_available_or_report(reports, idname_conv)) {
    return NULL;
  }

  /* We have to set default context if the class doesn't define it. */
  if (temp_buffers.translation_context[0] == '\0') {
    STRNCPY(temp_buffers.translation_context, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  }

  /* Convert foo.bar to FOO_OT_bar
   * allocate all strings at once. */
  {
    const char *strings[] = {
        idname_conv,
        temp_buffers.name,
        temp_buffers.description,
        temp_buffers.translation_context,
        temp_buffers.undo_group,
    };
    char *strings_table[ARRAY_SIZE(strings)];
    BLI_string_join_array_by_sep_char_with_tableN(
        '\0', strings_table, strings, ARRAY_SIZE(strings));

    dummyot.idname = strings_table[0]; /* allocated string stored here */
    dummyot.name = strings_table[1];
    dummyot.description = *strings_table[2] ? strings_table[2] : NULL;
    dummyot.translation_context = strings_table[3];
    dummyot.undo_group = strings_table[4];
    BLI_assert(ARRAY_SIZE(strings) == 5);
  }

  /* XXX, this doubles up with the operator name T29666.
   * for now just remove from dir(bpy.types) */

  /* create a new operator type */
  dummyot.rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, dummyot.idname, &RNA_Operator);
  RNA_def_struct_translation_context(dummyot.rna_ext.srna, dummyot.translation_context);
  dummyot.rna_ext.data = data;
  dummyot.rna_ext.call = call;
  dummyot.rna_ext.free = free;

  dummyot.pyop_poll = (have_function[0]) ? rna_operator_poll_cb : NULL;
  dummyot.ui = (have_function[3]) ? rna_operator_draw_cb : NULL;

  WM_operatortype_append_macro_ptr(BPY_RNA_operator_macro_wrapper, (void *)&dummyot);

  /* update while blender is running */
  WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL);

  return dummyot.rna_ext.srna;
}
#  endif /* WITH_PYTHON */

static StructRNA *rna_Operator_refine(PointerRNA *opr)
{
  wmOperator *op = (wmOperator *)opr->data;
  return (op->type && op->type->rna_ext.srna) ? op->type->rna_ext.srna : &RNA_Operator;
}

static StructRNA *rna_MacroOperator_refine(PointerRNA *opr)
{
  wmOperator *op = (wmOperator *)opr->data;
  return (op->type && op->type->rna_ext.srna) ? op->type->rna_ext.srna : &RNA_Macro;
}

/* just to work around 'const char *' warning and to ensure this is a python op */
static void rna_Operator_bl_idname_set(PointerRNA *ptr, const char *value)
{
  wmOperator *data = (wmOperator *)(ptr->data);
  char *str = (char *)data->type->idname;
  if (!str[0]) {
    BLI_strncpy(str, value, OP_MAX_TYPENAME); /* utf8 already ensured */
  }
  else {
    BLI_assert_msg(0, "setting the bl_idname on a non-builtin operator");
  }
}

static void rna_Operator_bl_label_set(PointerRNA *ptr, const char *value)
{
  wmOperator *data = (wmOperator *)(ptr->data);
  char *str = (char *)data->type->name;
  if (!str[0]) {
    BLI_strncpy(str, value, OP_MAX_TYPENAME); /* utf8 already ensured */
  }
  else {
    BLI_assert_msg(0, "setting the bl_label on a non-builtin operator");
  }
}

/**
 * Use callbacks that check for NULL instead of clearing #PROP_NEVER_NULL on the string property,
 * so the internal value may be NULL, without allowing Python to assign `None` which doesn't
 * make any sense in this case.
 */
#  define OPERATOR_STR_MAYBE_NULL_GETSET(attr, len) \
    static void rna_Operator_bl_##attr##_set(PointerRNA *ptr, const char *value) \
    { \
      wmOperator *data = (wmOperator *)(ptr->data); \
      char *str = (char *)data->type->attr; \
      if (str && !str[0]) { \
        BLI_strncpy(str, value, len); /* utf8 already ensured */ \
      } \
      else { \
        BLI_assert( \
            !"setting the bl_" STRINGIFY(translation_context) " on a non-builtin operator"); \
      } \
    } \
    static void rna_Operator_bl_##attr##_get(PointerRNA *ptr, char *value) \
    { \
      const wmOperator *data = (wmOperator *)(ptr->data); \
      const char *str = data->type->attr; \
      BLI_strncpy(value, str ? str : "", len); \
    } \
    static int rna_Operator_bl_##attr##_length(PointerRNA *ptr) \
    { \
      const wmOperator *data = (wmOperator *)(ptr->data); \
      const char *str = data->type->attr; \
      return BLI_strnlen(str ? str : "", len); \
    }

OPERATOR_STR_MAYBE_NULL_GETSET(translation_context, RNA_DYN_DESCR_MAX)
OPERATOR_STR_MAYBE_NULL_GETSET(description, RNA_DYN_DESCR_MAX)
OPERATOR_STR_MAYBE_NULL_GETSET(undo_group, OP_MAX_TYPENAME)

static void rna_KeyMapItem_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  wmKeyMapItem *kmi = ptr->data;
  WM_keyconfig_update_tag(NULL, kmi);
}

#else /* RNA_RUNTIME */

/**
 * expose `Operator.options` as its own type so we can control each flags use
 * (some are read-only).
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
  RNA_def_property_ui_text(
      prop, "Invoke", "True when invoked (even if only the execute callbacks available)");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_repeat", PROP_BOOLEAN, PROP_BOOLEAN);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", OP_IS_REPEAT);
  RNA_def_property_ui_text(prop, "Repeat", "True when run from the 'Adjust Last Operation' panel");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_repeat_last", PROP_BOOLEAN, PROP_BOOLEAN);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", OP_IS_REPEAT_LAST);
  RNA_def_property_ui_text(prop, "Repeat Call", "True when run from the operator 'Repeat Last'");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "use_cursor_region", PROP_BOOLEAN, PROP_BOOLEAN);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", OP_IS_MODAL_CURSOR_REGION);
  RNA_def_property_ui_text(
      prop, "Focus Region", "Enable to use the region under the cursor for modal execution");
}

static void rna_def_operator_common(StructRNA *srna)
{
  PropertyRNA *prop;

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
  RNA_def_property_ui_text(
      prop,
      "Has Reports",
      "Operator has a set of reports (warnings and errors) from last execution");

  /* Registration */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "type->idname");
  /* Without setting the length the pointer size would be used. -3 because `.` -> `_OT_`. */
  RNA_def_property_string_maxlength(prop, OP_MAX_TYPENAME - 3);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_idname_set");
  // RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "type->name");
  RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Operator_bl_label_set");
  // RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_REGISTER);

  prop = RNA_def_property(srna, "bl_translation_context", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "type->translation_context");
  RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
  RNA_def_property_string_funcs(prop,
                                "rna_Operator_bl_translation_context_get",
                                "rna_Operator_bl_translation_context_length",
                                "rna_Operator_bl_translation_context_set");
  RNA_def_property_string_default(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "type->description");
  RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
  RNA_def_property_string_funcs(prop,
                                "rna_Operator_bl_description_get",
                                "rna_Operator_bl_description_length",
                                "rna_Operator_bl_description_set");
  // RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_undo_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "type->undo_group");
  RNA_def_property_string_maxlength(prop, OP_MAX_TYPENAME); /* else it uses the pointer size! */
  RNA_def_property_string_funcs(prop,
                                "rna_Operator_bl_undo_group_get",
                                "rna_Operator_bl_undo_group_length",
                                "rna_Operator_bl_undo_group_set");
  // RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type->flag");
  RNA_def_property_enum_items(prop, rna_enum_operator_type_flag_items);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
  RNA_def_property_ui_text(prop, "Options", "Options for this operator type");

  prop = RNA_def_property(srna, "bl_cursor_pending", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type->cursor_pending");
  RNA_def_property_enum_items(prop, rna_enum_window_cursor_items);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Idle Cursor",
      "Cursor to use when waiting for the user to select a location to activate the operator "
      "(when ``bl_options`` has ``DEPENDS_ON_CURSOR`` set)");
}

static void rna_def_operator(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Operator", NULL);
  RNA_def_struct_ui_text(
      srna, "Operator", "Storage of an operator being executed, or registered after execution");
  RNA_def_struct_sdna(srna, "wmOperator");
  RNA_def_struct_refine_func(srna, "rna_Operator_refine");
#  ifdef WITH_PYTHON
  RNA_def_struct_register_funcs(
      srna, "rna_Operator_register", "rna_Operator_unregister", "rna_Operator_instance");
#  endif
  RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_struct_flag(srna, STRUCT_PUBLIC_NAMESPACE_INHERIT);

  rna_def_operator_common(srna);

  prop = RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "UILayout");

  prop = RNA_def_property(srna, "options", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "OperatorOptions");
  RNA_def_property_pointer_funcs(prop, "rna_Operator_options_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Options", "Runtime options");

  prop = RNA_def_property(srna, "macros", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "macro", NULL);
  RNA_def_property_struct_type(prop, "Macro");
  RNA_def_property_ui_text(prop, "Macros", "");

  RNA_api_operator(srna);

  srna = RNA_def_struct(brna, "OperatorProperties", NULL);
  RNA_def_struct_ui_text(srna, "Operator Properties", "Input properties of an operator");
  RNA_def_struct_refine_func(srna, "rna_OperatorProperties_refine");
  RNA_def_struct_idprops_func(srna, "rna_OperatorProperties_idprops");
  RNA_def_struct_property_tags(srna, rna_enum_operator_property_tags);
  RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES | STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID);
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
}

static void rna_def_macro_operator(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "Macro", NULL);
  RNA_def_struct_ui_text(
      srna,
      "Macro Operator",
      "Storage of a macro operator being executed, or registered after execution");
  RNA_def_struct_sdna(srna, "wmOperator");
  RNA_def_struct_refine_func(srna, "rna_MacroOperator_refine");
#  ifdef WITH_PYTHON
  RNA_def_struct_register_funcs(
      srna, "rna_MacroOperator_register", "rna_Operator_unregister", "rna_Operator_instance");
#  endif
  RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_struct_flag(srna, STRUCT_PUBLIC_NAMESPACE_INHERIT);

  rna_def_operator_common(srna);

  RNA_api_macro(srna);
}

static void rna_def_operator_type_macro(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "OperatorMacro", NULL);
  RNA_def_struct_ui_text(
      srna, "Operator Macro", "Storage of a sub operator in a macro after it has been added");
  RNA_def_struct_sdna(srna, "wmOperatorTypeMacro");

#  if 0
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_sdna(prop, NULL, "idname");
  RNA_def_property_ui_text(prop, "Name", "Name of the sub operator");
  RNA_def_struct_name_property(srna, prop);
#  endif

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
  RNA_def_struct_ui_text(
      srna, "Operator Mouse Path", "Mouse path values for operators that record such paths");

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
  RNA_def_property_enum_items(prop, rna_enum_event_value_all_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Event_value_itemf");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Value", "The type of event, only applies to some");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, rna_enum_event_type_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UI_EVENTS);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Type", "");

  /* keyboard */
  prop = RNA_def_property(srna, "is_repeat", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Event_is_repeat_get", NULL);
  RNA_def_property_ui_text(prop, "Is Repeat", "The event is generated by holding a key down");

  /* mouse */
  prop = RNA_def_property(srna, "mouse_x", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "xy[0]");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Mouse X Position", "The window relative horizontal location of the mouse");

  prop = RNA_def_property(srna, "mouse_y", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "xy[1]");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Mouse Y Position", "The window relative vertical location of the mouse");

  prop = RNA_def_property(srna, "mouse_region_x", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "mval[0]");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Mouse X Position", "The region relative horizontal location of the mouse");

  prop = RNA_def_property(srna, "mouse_region_y", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "mval[1]");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Mouse Y Position", "The region relative vertical location of the mouse");

  prop = RNA_def_property(srna, "mouse_prev_x", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "prev_xy[0]");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Mouse Previous X Position", "The window relative horizontal location of the mouse");

  prop = RNA_def_property(srna, "mouse_prev_y", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "prev_xy[1]");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Mouse Previous Y Position", "The window relative vertical location of the mouse");

  prop = RNA_def_property(srna, "pressure", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_Event_pressure_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Tablet Pressure", "The pressure of the tablet or 1.0 if no tablet present");

  prop = RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_XYZ_LENGTH);
  RNA_def_property_array(prop, 2);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_Event_tilt_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Tablet Tilt", "The pressure of the tablet or zeroes if no tablet present");

  prop = RNA_def_property(srna, "is_tablet", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Event_is_tablet_get", NULL);
  RNA_def_property_ui_text(prop, "Is Tablet", "The event has tablet data");

  prop = RNA_def_property(srna, "is_mouse_absolute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "tablet.is_motion_absolute", 1);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Absolute Motion", "The last motion event was an absolute input");

  /* xr */
  prop = RNA_def_property(srna, "xr", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "XrEventData");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, "rna_Event_xr_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "XR", "XR event data");

  /* modifiers */
  prop = RNA_def_property(srna, "shift", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "shift", 1);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Shift", "True when the Shift key is held");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_WINDOWMANAGER);

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

static void rna_def_popup_menu_wrapper(BlenderRNA *brna,
                                       const char *rna_type,
                                       const char *c_type,
                                       const char *layout_get_fn)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, rna_type, NULL);
  /* UI name isn't visible, name same as type. */
  RNA_def_struct_ui_text(srna, rna_type, "");
  RNA_def_struct_sdna(srna, c_type);

  RNA_define_verify_sdna(0); /* not in sdna */

  /* could wrap more, for now this is enough */
  prop = RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "UILayout");
  RNA_def_property_pointer_funcs(prop, layout_get_fn, NULL, NULL, NULL);

  RNA_define_verify_sdna(1); /* not in sdna */
}

static void rna_def_popupmenu(BlenderRNA *brna)
{
  rna_def_popup_menu_wrapper(brna, "UIPopupMenu", "uiPopupMenu", "rna_PopupMenu_layout_get");
}

static void rna_def_popovermenu(BlenderRNA *brna)
{
  rna_def_popup_menu_wrapper(brna, "UIPopover", "uiPopover", "rna_PopoverMenu_layout_get");
}

static void rna_def_piemenu(BlenderRNA *brna)
{
  rna_def_popup_menu_wrapper(brna, "UIPieMenu", "uiPieMenu", "rna_PieMenu_layout_get");
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
  RNA_def_property_ui_text(prop, "Cross-Eyed", "Right eye should see left image and vice versa");
}

static void rna_def_window(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Window", NULL);
  RNA_def_struct_ui_text(srna, "Window", "Open window");
  RNA_def_struct_sdna(srna, "wmWindow");

  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Parent Window", "Active workspace and scene follow this window");

  rna_def_window_stereo3d(brna);

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
  RNA_def_property_pointer_funcs(prop, NULL, "rna_Window_scene_set", NULL, NULL);
  RNA_def_property_ui_text(prop, "Scene", "Active scene to be edited in the window");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_Window_scene_update");

  prop = RNA_def_property(srna, "workspace", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "WorkSpace");
  RNA_def_property_ui_text(prop, "Workspace", "Active workspace showing in the window");
  RNA_def_property_pointer_funcs(
      prop, "rna_Window_workspace_get", "rna_Window_workspace_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_Window_workspace_update");

  prop = RNA_def_property(srna, "screen", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Screen");
  RNA_def_property_ui_text(prop, "Screen", "Active workspace screen showing in the window");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Window_screen_get",
                                 "rna_Window_screen_set",
                                 NULL,
                                 "rna_Window_screen_assign_poll");
  RNA_def_property_flag(prop, PROP_NEVER_NULL | PROP_EDITABLE | PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_workspace_screen_update");

  prop = RNA_def_property(srna, "view_layer", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ViewLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_Window_view_layer_get", "rna_Window_view_layer_set", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Active View Layer", "The active workspace view layer showing in the window");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
  RNA_def_property_update(prop, NC_SCREEN | ND_LAYER, NULL);

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
  RNA_def_property_ui_text(prop, "Stereo 3D Display", "Settings for stereo 3D display");

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
  RNA_def_property_pointer_funcs(prop,
                                 "rna_WindowManager_active_keyconfig_get",
                                 "rna_WindowManager_active_keyconfig_set",
                                 NULL,
                                 NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active KeyConfig", "Active key configuration (preset)");

  prop = RNA_def_property(srna, "default", PROP_POINTER, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "defaultconf");
  RNA_def_property_struct_type(prop, "KeyConfig");
  RNA_def_property_ui_text(prop, "Default Key Configuration", "Default builtin key configuration");

  prop = RNA_def_property(srna, "addon", PROP_POINTER, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "addonconf");
  RNA_def_property_struct_type(prop, "KeyConfig");
  RNA_def_property_ui_text(
      prop,
      "Add-on Key Configuration",
      "Key configuration that can be extended by add-ons, and is added to the active "
      "configuration when handling events");

  prop = RNA_def_property(srna, "user", PROP_POINTER, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "userconf");
  RNA_def_property_struct_type(prop, "KeyConfig");
  RNA_def_property_ui_text(
      prop,
      "User Key Configuration",
      "Final key configuration that combines keymaps from the active and add-on configurations, "
      "and can be edited by the user");

  RNA_api_keyconfigs(srna);
}

static void rna_def_windowmanager(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WindowManager", "ID");
  RNA_def_struct_ui_text(
      srna,
      "Window Manager",
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

  prop = RNA_def_property(srna, "xr_session_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "xr.session_settings");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "XR Session Settings", "");

  prop = RNA_def_property(srna, "xr_session_state", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "XrSessionState");
  RNA_def_property_pointer_funcs(prop, "rna_WindowManager_xr_session_state_get", NULL, NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "XR Session State", "Runtime state information about the VR session");

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

static void rna_def_keyconfig_prefs(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "KeyConfigPreferences", NULL);
  RNA_def_struct_ui_text(srna, "Key-Config Preferences", "");
  RNA_def_struct_sdna(srna, "wmKeyConfigPref"); /* WARNING: only a bAddon during registration */

  RNA_def_struct_refine_func(srna, "rna_wmKeyConfigPref_refine");
  RNA_def_struct_register_funcs(
      srna, "rna_wmKeyConfigPref_register", "rna_wmKeyConfigPref_unregister", NULL);
  RNA_def_struct_idprops_func(srna, "rna_wmKeyConfigPref_idprops");
  RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES); /* Mandatory! */

  /* registration */
  RNA_define_verify_sdna(0);
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_define_verify_sdna(1);
}

static void rna_def_keyconfig(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem map_type_items[] = {
      {KMI_TYPE_KEYBOARD, "KEYBOARD", 0, "Keyboard", ""},
      {KMI_TYPE_TWEAK, "TWEAK", 0, "Tweak", ""},
      {KMI_TYPE_MOUSE, "MOUSE", 0, "Mouse", ""},
      {KMI_TYPE_NDOF, "NDOF", 0, "NDOF", ""},
      {KMI_TYPE_TEXTINPUT, "TEXTINPUT", 0, "Text Input", ""},
      {KMI_TYPE_TIMER, "TIMER", 0, "Timer", ""},
      {0, NULL, 0, NULL, NULL},
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
  RNA_def_property_ui_text(
      prop, "User Defined", "Indicates that a keyconfig was defined by the user");

  /* Collection active property */
  prop = RNA_def_property(srna, "preferences", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "KeyConfigPreferences");
  RNA_def_property_pointer_funcs(prop, "rna_wmKeyConfig_preferences_get", NULL, NULL, NULL);

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

  prop = RNA_def_property(srna, "bl_owner_id", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "owner_id");
  RNA_def_property_ui_text(prop, "Owner", "Internal owner");

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
  RNA_def_property_ui_text(
      prop, "Items", "Items in the keymap, linking an operator to an input event");
  rna_def_keymap_items(brna, prop);

  prop = RNA_def_property(srna, "is_user_modified", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYMAP_USER_MODIFIED);
  RNA_def_property_ui_text(prop, "User Defined", "Keymap is defined by the user");

  prop = RNA_def_property(srna, "is_modal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYMAP_MODAL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Modal Keymap",
      "Indicates that a keymap is used for translate modal events for an operator");

  prop = RNA_def_property(srna, "show_expanded_items", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYMAP_EXPANDED);
  RNA_def_property_ui_text(prop, "Items Expanded", "Expanded in the user interface");
  RNA_def_property_ui_icon(prop, ICON_DISCLOSURE_TRI_RIGHT, 1);

  prop = RNA_def_property(srna, "show_expanded_children", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYMAP_CHILDREN_EXPANDED);
  RNA_def_property_ui_text(prop, "Children Expanded", "Children expanded in the user interface");
  RNA_def_property_ui_icon(prop, ICON_DISCLOSURE_TRI_RIGHT, 1);

  RNA_api_keymap(srna);

  /* KeyMapItem */
  srna = RNA_def_struct(brna, "KeyMapItem", NULL);
  RNA_def_struct_sdna(srna, "wmKeyMapItem");
  RNA_def_struct_ui_text(srna, "Key Map Item", "Item in a Key Map");

  prop = RNA_def_property(srna, "idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "idname");
  RNA_def_property_ui_text(prop, "Identifier", "Identifier of operator to call on input event");
  RNA_def_property_string_funcs(prop,
                                "rna_wmKeyMapItem_idname_get",
                                "rna_wmKeyMapItem_idname_length",
                                "rna_wmKeyMapItem_idname_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

  /* this is in fact the operator name, but if the operator can't be found we
   * fallback on the operator ID */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Name", "Name of operator (translated) to call on input event");
  RNA_def_property_string_funcs(
      prop, "rna_wmKeyMapItem_name_get", "rna_wmKeyMapItem_name_length", NULL);

  prop = RNA_def_property(srna, "properties", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "OperatorProperties");
  RNA_def_property_pointer_funcs(prop, "rna_KeyMapItem_properties_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Properties", "Properties to set when the operator is called");
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

  prop = RNA_def_property(srna, "map_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "maptype");
  RNA_def_property_enum_items(prop, map_type_items);
  RNA_def_property_enum_funcs(
      prop, "rna_wmKeyMapItem_map_type_get", "rna_wmKeyMapItem_map_type_set", NULL);
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
  RNA_def_property_enum_items(prop, rna_enum_event_value_all_items);
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

  prop = RNA_def_property(srna, "shift", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "shift");
  RNA_def_property_range(prop, KM_ANY, KM_MOD_HELD);
  RNA_def_property_ui_text(prop, "Shift", "Shift key pressed, -1 for any state");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_WINDOWMANAGER);
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

  prop = RNA_def_property(srna, "ctrl", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "ctrl");
  RNA_def_property_range(prop, KM_ANY, KM_MOD_HELD);
  RNA_def_property_ui_text(prop, "Ctrl", "Control key pressed, -1 for any state");
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

  prop = RNA_def_property(srna, "alt", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "alt");
  RNA_def_property_range(prop, KM_ANY, KM_MOD_HELD);
  RNA_def_property_ui_text(prop, "Alt", "Alt key pressed, -1 for any state");
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

  prop = RNA_def_property(srna, "oskey", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "oskey");
  RNA_def_property_range(prop, KM_ANY, KM_MOD_HELD);
  RNA_def_property_ui_text(prop, "OS Key", "Operating system key pressed, -1 for any state");
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

  /* XXX(@campbellbarton): the `*_ui` suffix is only for the UI, may be removed,
   * since this is only exposed so the UI can show these settings as toggle-buttons. */
  prop = RNA_def_property(srna, "shift_ui", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "shift", 0);
  RNA_def_property_boolean_funcs(prop, "rna_KeyMapItem_shift_get", NULL);
  /*  RNA_def_property_enum_items(prop, keymap_modifiers_items); */
  RNA_def_property_ui_text(prop, "Shift", "Shift key pressed");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_WINDOWMANAGER);
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

  prop = RNA_def_property(srna, "ctrl_ui", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ctrl", 0);
  RNA_def_property_boolean_funcs(prop, "rna_KeyMapItem_ctrl_get", NULL);
  /*  RNA_def_property_enum_items(prop, keymap_modifiers_items); */
  RNA_def_property_ui_text(prop, "Ctrl", "Control key pressed");
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

  prop = RNA_def_property(srna, "alt_ui", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "alt", 0);
  RNA_def_property_boolean_funcs(prop, "rna_KeyMapItem_alt_get", NULL);
  /*  RNA_def_property_enum_items(prop, keymap_modifiers_items); */
  RNA_def_property_ui_text(prop, "Alt", "Alt key pressed");
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

  prop = RNA_def_property(srna, "oskey_ui", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "oskey", 0);
  RNA_def_property_boolean_funcs(prop, "rna_KeyMapItem_oskey_get", NULL);
  /*  RNA_def_property_enum_items(prop, keymap_modifiers_items); */
  RNA_def_property_ui_text(prop, "OS Key", "Operating system key pressed");
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");
  /* End `_ui` modifiers. */

  prop = RNA_def_property(srna, "key_modifier", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "keymodifier");
  RNA_def_property_enum_items(prop, rna_enum_event_type_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UI_EVENTS);
  RNA_def_property_enum_funcs(prop, NULL, "rna_wmKeyMapItem_keymodifier_set", NULL);
  RNA_def_property_ui_text(prop, "Key Modifier", "Regular key pressed as a modifier");
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

  prop = RNA_def_property(srna, "repeat", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", KMI_REPEAT_IGNORE);
  RNA_def_property_ui_text(prop, "Repeat", "Active on key-repeat events (when a key is held)");
  RNA_def_property_update(prop, 0, "rna_KeyMapItem_update");

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", KMI_EXPANDED);
  RNA_def_property_ui_text(
      prop, "Expanded", "Show key map event and property details in the user interface");
  RNA_def_property_ui_icon(prop, ICON_DISCLOSURE_TRI_RIGHT, 1);

  prop = RNA_def_property(srna, "propvalue", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "propvalue");
  RNA_def_property_enum_items(prop, rna_enum_keymap_propvalue_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_KeyMapItem_propvalue_itemf");
  RNA_def_property_ui_text(
      prop, "Property Value", "The value this event translates to in a modal keymap");
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
  RNA_def_property_ui_text(
      prop,
      "User Defined",
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
  rna_def_popovermenu(brna);
  rna_def_piemenu(brna);
  rna_def_window(brna);
  rna_def_windowmanager(brna);
  rna_def_keyconfig_prefs(brna);
  rna_def_keyconfig(brna);
}

#endif /* RNA_RUNTIME */
