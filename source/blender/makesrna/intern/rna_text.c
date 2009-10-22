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
 * Contributor(s): Blender Foundation (2008)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BKE_text.h"

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_text_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

int text_file_modified(Text *text);

static void rna_Text_filename_get(PointerRNA *ptr, char *value)
{
	Text *text= (Text*)ptr->data;

	if(text->name)
		strcpy(value, text->name);
	else
		strcpy(value, "");
}

static int rna_Text_filename_length(PointerRNA *ptr)
{
	Text *text= (Text*)ptr->data;
	return (text->name)? strlen(text->name): 0;
}

static void rna_Text_filename_set(PointerRNA *ptr, const char *value)
{
	Text *text= (Text*)ptr->data;

	if(text->name)
		MEM_freeN(text->name);

	if(strlen(value))
		text->name= BLI_strdup(value);
	else
		text->name= NULL;
}

static int rna_Text_modified_get(PointerRNA *ptr)
{
	Text *text= (Text*)ptr->data;
	return text_file_modified(text);
}

static void rna_TextLine_line_get(PointerRNA *ptr, char *value)
{
	TextLine *line= (TextLine*)ptr->data;

	if(line->line)
		strcpy(value, line->line);
	else
		strcpy(value, "");
}

static int rna_TextLine_line_length(PointerRNA *ptr)
{
	TextLine *line= (TextLine*)ptr->data;
	return line->len;
}

static void rna_TextLine_line_set(PointerRNA *ptr, const char *value)
{
	TextLine *line= (TextLine*)ptr->data;

	if(line->line)
		MEM_freeN(line->line);
	
	line->line= BLI_strdup(value);
	line->len= strlen(line->line);

	if(line->format) {
		MEM_freeN(line->format);
		line->format= NULL;
	}
}

#else

static void rna_def_text_line(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "TextLine", NULL);
	RNA_def_struct_ui_text(srna, "Text Line", "Line of text in a Text datablock.");
	
	prop= RNA_def_property(srna, "line", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_TextLine_line_get", "rna_TextLine_line_length", "rna_TextLine_line_set");
	RNA_def_property_ui_text(prop, "Line", "Text in the line.");
	RNA_def_property_update(prop, NC_TEXT|NA_EDITED, NULL);
}

static void rna_def_text_marker(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "TextMarker", NULL);
	RNA_def_struct_ui_text(srna, "Text Marker", "Marker highlighting a portion of text in a Text datablock.");

	prop= RNA_def_property(srna, "line", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "lineno");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Line", "Line in which the marker is located.");
	
	prop= RNA_def_property(srna, "start", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Start", "Start position of the marker in the line.");

	prop= RNA_def_property(srna, "end", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "End", "Start position of the marker in the line.");
	
	prop= RNA_def_property(srna, "group", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_range(prop, 0, (int)0xFFFF);
	RNA_def_property_ui_text(prop, "Group", "");
	
	prop= RNA_def_property(srna, "temporary", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", TMARK_TEMP);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Temporary", "Marker is temporary.");

	prop= RNA_def_property(srna, "edit_all", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", TMARK_EDITALL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Edit All", "Edit all markers of the same group as one.");
	
	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_ui_text(prop, "Color", "Color to display the marker with.");
}

static void rna_def_text(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "Text", "ID");
	RNA_def_struct_ui_text(srna, "Text", "Text datablock referencing an external or packed text file.");
	RNA_def_struct_ui_icon(srna, ICON_TEXT);
	RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);
	
	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_Text_filename_get", "rna_Text_filename_length", "rna_Text_filename_set");
	RNA_def_property_ui_text(prop, "Filename", "Filename of the text file.");

	prop= RNA_def_property(srna, "dirty", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", TXT_ISDIRTY);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Dirty", "Text file has been edited since last save.");

	prop= RNA_def_property(srna, "modified", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Text_modified_get", NULL);
	RNA_def_property_ui_text(prop, "Modified", "Text file on disk is different than the one in memory.");

	prop= RNA_def_property(srna, "memory", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", TXT_ISMEM);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Memory", "Text file is in memory, without a corresponding file on disk.");
	
	prop= RNA_def_property(srna, "lines", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "TextLine");
	RNA_def_property_ui_text(prop, "Lines", "Lines of text.");
	
	prop= RNA_def_property(srna, "current_line", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "curl");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "TextLine");
	RNA_def_property_ui_text(prop, "Current Line", "Current line, and start line of selection if one exists.");

	prop= RNA_def_property(srna, "current_character", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "curc");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Current Character", "Index of current character in current line, and also start index of character in selection if one exists.");
	
	prop= RNA_def_property(srna, "selection_end_line", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "sell");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "TextLine");
	RNA_def_property_ui_text(prop, "Selection End Line", "End line of selection.");
	
	prop= RNA_def_property(srna, "selection_end_character", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "selc");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Selection End Character", "Index of character after end of selection in the selection end line.");
	
	prop= RNA_def_property(srna, "markers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "TextMarker");
	RNA_def_property_ui_text(prop, "Markers", "Text markers highlighting part of the text.");

	RNA_api_text(srna);
}

void RNA_def_text(BlenderRNA *brna)
{
	rna_def_text_line(brna);
	rna_def_text_marker(brna);
	rna_def_text(brna);
}

#endif
