/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_windowmanager_types.h"
#include "WM_types.h" /* wmEvent */


EnumPropertyItem event_value_items[] = {
	{KM_ANY, "ANY", 0, "Any", ""},
	{KM_NOTHING, "NOTHING", 0, "Nothing", ""},
	{KM_PRESS, "PRESS", 0, "Press", ""},
	{KM_RELEASE, "RELEASE", 0, "Release", ""},
	{0, NULL, 0, NULL, NULL}};

/* not returned: CAPSLOCKKEY, UNKNOWNKEY, GRLESSKEY */
EnumPropertyItem event_type_items[] = {

	{LEFTMOUSE, "LEFTMOUSE", 0, "Left Mouse", ""},
	{MIDDLEMOUSE, "MIDDLEMOUSE", 0, "Middle Mouse", ""},
	{RIGHTMOUSE, "RIGHTMOUSE", 0, "Right Mouse", ""},
	{ACTIONMOUSE, "ACTIONMOUSE", 0, "Action Mouse", ""},
	{SELECTMOUSE, "SELECTMOUSE", 0, "Select Mouse", ""},

	{MOUSEMOVE, "MOUSEMOVE", 0, "Mouse Move", ""},

	{WHEELUPMOUSE, "WHEELUPMOUSE", 0, "Wheel Up", ""},
	{WHEELDOWNMOUSE, "WHEELDOWNMOUSE", 0, "Wheel Down", ""},
	{WHEELINMOUSE, "WHEELINMOUSE", 0, "Wheel In", ""},
	{WHEELOUTMOUSE, "WHEELOUTMOUSE", 0, "Wheel Out", ""},

	{EVT_TWEAK_L, "EVT_TWEAK_L", 0, "Tweak Left", ""},
	{EVT_TWEAK_M, "EVT_TWEAK_M", 0, "Tweak Middle", ""},
	{EVT_TWEAK_R, "EVT_TWEAK_R", 0, "Tweak Right", ""},
	{EVT_TWEAK_A, "EVT_TWEAK_A", 0, "Tweak Action", ""},
	{EVT_TWEAK_S, "EVT_TWEAK_S", 0, "Tweak Select", ""},

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
	
	{ZEROKEY, "ZERO",	0, "0", ""},
	{ONEKEY, "ONE",		0, "1", ""},
	{TWOKEY, "TWO",		0, "2", ""},
	{THREEKEY, "THREE",	0, "3", ""},
	{FOURKEY, "FOUR",	0, "4", ""},
	{FIVEKEY, "FIVE",	0, "5", ""},
	{SIXKEY, "SIX",		0, "6", ""},
	{SEVENKEY, "SEVEN",	0, "7", ""},
	{EIGHTKEY, "EIGHT",	0, "8", ""},
	{NINEKEY, "NINE",	0, "9", ""},
	
	{LEFTCTRLKEY,	"LEFT_CTRL",	0, "Left Ctrl", ""},
	{LEFTALTKEY,	"LEFT_ALT",		0, "Left Alt", ""},
	{LEFTSHIFTKEY,	"LEFT_SHIFT",	0, "Left Shift", ""},
	{RIGHTALTKEY,	"RIGHT_ALT",	0, "Right Alt", ""},
	{RIGHTCTRLKEY,	"RIGHT_CTRL",	0, "Right Ctrl", ""},
	{RIGHTSHIFTKEY,	"RIGHT_SHIFT",	0, "Right Shift", ""},

	{COMMANDKEY,	"COMMAND",	0, "Command", ""},
	
	{ESCKEY, "ESC", 0, "Esc", ""},
	{TABKEY, "TAB", 0, "Tab", ""},
	{RETKEY, "RET", 0, "Return", ""},
	{SPACEKEY, "SPACE", 0, "Spacebar", ""},
	{LINEFEEDKEY, "LINE_FEED", 0, "Line Feed", ""},
	{BACKSPACEKEY, "BACK_SPACE", 0, "Back Space", ""},
	{DELKEY, "DEL", 0, "Delete", ""},
	{SEMICOLONKEY, "SEMI_COLON", 0, ";", ""},
	{PERIODKEY, "PERIOD", 0, ".", ""},
	{COMMAKEY, "COMMA", 0, ",", ""},
	{QUOTEKEY, "QUOTE", 0, "\"", ""},
	{ACCENTGRAVEKEY, "ACCENT_GRAVE", 0, "`", ""},
	{MINUSKEY, "MINUS", 0, "-", ""},
	{SLASHKEY, "SLASH", 0, "/", ""},
	{BACKSLASHKEY, "BACK_SLASH", 0, "\\", ""},
	{EQUALKEY, "EQUAL", 0, "=", ""},
	{LEFTBRACKETKEY, "LEFT_BRACKET", 0, "]", ""},
	{RIGHTBRACKETKEY, "RIGHT_BRACKET", 0, "[", ""},
	{LEFTARROWKEY, "LEFT_ARROW", 0, "Left Arrow", ""},
	{DOWNARROWKEY, "DOWN_ARROW", 0, "Down Arrow", ""},
	{RIGHTARROWKEY, "RIGHT_ARROW", 0, "Right Arrow", ""},
	{UPARROWKEY, "UP_ARROW", 0, "Up Arrow", ""},
	{PAD2, "NUMPAD_2", 0, "Numpad 2", ""},
	{PAD4, "NUMPAD_4", 0, "Numpad 4", ""},
	{PAD6, "NUMPAD_6", 0, "Numpad 6", ""},
	{PAD8, "NUMPAD_8", 0, "Numpad 8", ""},
	{PAD1, "NUMPAD_1", 0, "Numpad 1", ""},
	{PAD3, "NUMPAD_3", 0, "Numpad 3", ""},
	{PAD5, "NUMPAD_5", 0, "Numpad 5", ""},
	{PAD7, "NUMPAD_7", 0, "Numpad 7", ""},
	{PAD9, "NUMPAD_9", 0, "Numpad 9", ""},
	{PADPERIOD, "NUMPAD_PERIOD", 0, "Numpad .", ""},
	{PADSLASHKEY, "NUMPAD_SLASH", 0, "Numpad /", ""},
	{PADASTERKEY, "NUMPAD_ASTERIX", 0, "Numpad *", ""},
	{PAD0, "NUMPAD_0", 0, "Numpad 0", ""},
	{PADMINUS, "NUMPAD_MINUS", 0, "Numpad -", ""},
	{PADENTER, "NUMPAD_ENTER", 0, "Numpad Enter", ""},
	{PADPLUSKEY, "NUMPAD_PLUS", 0, "Numpad +", ""},
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
	{PAUSEKEY, "PAUSE", 0, "Pause", ""},
	{INSERTKEY, "INSERT", 0, "Insert", ""},
	{HOMEKEY, "HOME", 0, "Home", ""},
	{PAGEUPKEY, "PAGE_UP", 0, "Page Up", ""},
	{PAGEDOWNKEY, "PAGE_DOWN", 0, "Page Down", ""},
	{ENDKEY, "END", 0, "End", ""},
	{0, NULL, 0, NULL, NULL}};	

#ifdef RNA_RUNTIME

#include "WM_api.h"

#include "BKE_idprop.h"

static wmOperator *rna_OperatorProperties_find_operator(PointerRNA *ptr)
{
	wmWindowManager *wm= ptr->id.data;
	IDProperty *properties= (IDProperty*)ptr->data;
	wmOperator *op;

	if(wm)
		for(op=wm->operators.first; op; op=op->next)
			if(op->properties == properties)
				return op;
	
	return NULL;
}

static StructRNA *rna_OperatorProperties_refine(PointerRNA *ptr)
{
	wmOperator *op= rna_OperatorProperties_find_operator(ptr);

	if(op)
		return op->type->srna;
	else
		return ptr->type;
}

IDProperty *rna_OperatorProperties_idproperties(PointerRNA *ptr, int create)
{
	if(create && !ptr->data) {
		IDPropertyTemplate val = {0};
		ptr->data= IDP_New(IDP_GROUP, val, "RNA_OperatorProperties group");
	}

	return ptr->data;
}

static void rna_Operator_name_get(PointerRNA *ptr, char *value)
{
	wmOperator *op= (wmOperator*)ptr->data;
	strcpy(value, op->type->name);
}

static int rna_Operator_name_length(PointerRNA *ptr)
{
	wmOperator *op= (wmOperator*)ptr->data;
	return strlen(op->type->name);
}

static PointerRNA rna_Operator_properties_get(PointerRNA *ptr)
{
	wmOperator *op= (wmOperator*)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_OperatorProperties, op->properties);
}


static void rna_Event_ascii_get(PointerRNA *ptr, char *value)
{
	wmEvent *event= (wmEvent*)ptr->id.data;
	value[0]= event->ascii;
	value[1]= '\0';
}

static int rna_Event_ascii_length(PointerRNA *ptr)
{
	wmEvent *event= (wmEvent*)ptr->id.data;
	return (event->ascii)? 1 : 0;
}

static void rna_Window_screen_set(PointerRNA *ptr, PointerRNA value)
{
	wmWindow *win= (wmWindow*)ptr->data;

	if(value.data == NULL)
		return;

	/* exception: can't set screens inside of area/region handers */
	win->newscreen= value.data;
}

static void rna_Window_screen_update(bContext *C, PointerRNA *ptr)
{
	wmWindow *win= (wmWindow*)ptr->data;

	/* exception: can't set screens inside of area/region handers */
	if(win->newscreen) {
		WM_event_add_notifier(C, NC_SCREEN|ND_SCREENBROWSE, win->newscreen);
		win->newscreen= NULL;
	}
}

#else

static void rna_def_operator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Operator", NULL);
	RNA_def_struct_ui_text(srna, "Operator", "Storage of an operator being executed, or registered after execution.");
	RNA_def_struct_sdna(srna, "wmOperator");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Operator_name_get", "rna_Operator_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "properties", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "OperatorProperties");
	RNA_def_property_ui_text(prop, "Properties", "");
	RNA_def_property_pointer_funcs(prop, "rna_Operator_properties_get", NULL, NULL);

	srna= RNA_def_struct(brna, "OperatorProperties", NULL);
	RNA_def_struct_ui_text(srna, "Operator Properties", "Input properties of an Operator.");
	RNA_def_struct_refine_func(srna, "rna_OperatorProperties_refine");
	RNA_def_struct_idproperties_func(srna, "rna_OperatorProperties_idproperties");
}

static void rna_def_operator_utils(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "OperatorMousePath", "IDPropertyGroup");
	RNA_def_struct_ui_text(srna, "Operator Mouse Path", "Mouse path values for operators that record such paths.");

	prop= RNA_def_property(srna, "loc", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "Location", "Mouse location.");

	prop= RNA_def_property(srna, "time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Time", "Time of mouse location.");
}

static void rna_def_operator_filelist_element(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "OperatorFileListElement", "IDPropertyGroup");
	RNA_def_struct_ui_text(srna, "Operator File List Element", "");
	
	
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Name", "the name of a file or directory within a file list");
}
	
static void rna_def_event(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Event", NULL);
	RNA_def_struct_ui_text(srna, "Event", "Window Manager Event");
	RNA_def_struct_sdna(srna, "wmEvent");

	/* strings */
	prop= RNA_def_property(srna, "ascii", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Event_ascii_get", "rna_Event_ascii_length", NULL);
	RNA_def_property_ui_text(prop, "ASCII", "Single ASCII character for this event.");


	/* enums */
	prop= RNA_def_property(srna, "value", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "val");
	RNA_def_property_enum_items(prop, event_value_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Value",  "The type of event, only applies to some.");
	
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, event_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type",  "");


	/* mouse */
	prop= RNA_def_property(srna, "mouse_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "x");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mouse X Position", "The window relative vertical location of the mouse.");
	
	prop= RNA_def_property(srna, "mouse_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "y");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mouse Y Position", "The window relative horizontal location of the mouse.");
	
	prop= RNA_def_property(srna, "mouse_prev_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "prevx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mouse Previous X Position", "The window relative vertical location of the mouse.");
	
	prop= RNA_def_property(srna, "mouse_prev_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "prevy");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mouse Previous Y Position", "The window relative horizontal location of the mouse.");	


	/* modifiers */
	prop= RNA_def_property(srna, "shift", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shift", 1);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Shift", "True when the Shift key is held.");
	
	prop= RNA_def_property(srna, "ctrl", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ctrl", 1);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Ctrl", "True when the Ctrl key is held.");
	
	prop= RNA_def_property(srna, "alt", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "alt", 1);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Alt", "True when the Alt/Option key is held.");
	
	prop= RNA_def_property(srna, "oskey", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "oskey", 1);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "OS Key", "True when the Cmd key is held.");
}

static void rna_def_window(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Window", NULL);
	RNA_def_struct_ui_text(srna, "Window", "Open window.");
	RNA_def_struct_sdna(srna, "wmWindow");

	prop= RNA_def_property(srna, "screen", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "Screen");
	RNA_def_property_ui_text(prop, "Screen", "Active screen showing in the window.");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Window_screen_set", NULL);
	RNA_def_property_update(prop, 0, "rna_Window_screen_update");
}

static void rna_def_windowmanager(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "WindowManager", "ID");
	RNA_def_struct_ui_text(srna, "Window Manager", "Window manager datablock defining open windows and other user interface data.");
	RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);
	RNA_def_struct_sdna(srna, "wmWindowManager");

	prop= RNA_def_property(srna, "operators", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Operator");
	RNA_def_property_ui_text(prop, "Operators", "Operator registry.");

	prop= RNA_def_property(srna, "windows", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Window");
	RNA_def_property_ui_text(prop, "Windows", "Open windows.");

	RNA_api_wm(srna);
}

void RNA_def_wm(BlenderRNA *brna)
{
	rna_def_operator(brna);
	rna_def_operator_utils(brna);
	rna_def_operator_filelist_element(brna);
	rna_def_event(brna);
	rna_def_window(brna);
	rna_def_windowmanager(brna);
}

#endif

