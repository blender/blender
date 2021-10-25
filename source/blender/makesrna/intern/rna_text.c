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
 * Contributor(s): Blender Foundation (2008)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_text.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BKE_text.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_text_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

static void rna_Text_filename_get(PointerRNA *ptr, char *value)
{
	Text *text = (Text *)ptr->data;

	if (text->name)
		strcpy(value, text->name);
	else
		value[0] = '\0';
}

static int rna_Text_filename_length(PointerRNA *ptr)
{
	Text *text = (Text *)ptr->data;
	return (text->name) ? strlen(text->name) : 0;
}

static void rna_Text_filename_set(PointerRNA *ptr, const char *value)
{
	Text *text = (Text *)ptr->data;

	if (text->name)
		MEM_freeN(text->name);

	if (value[0])
		text->name = BLI_strdup(value);
	else
		text->name = NULL;
}

static int rna_Text_modified_get(PointerRNA *ptr)
{
	Text *text = (Text *)ptr->data;
	return BKE_text_file_modified_check(text) != 0;
}

static int rna_Text_current_line_index_get(PointerRNA *ptr)
{
	Text *text = (Text *)ptr->data;
	return BLI_findindex(&text->lines, text->curl);
}

static void rna_Text_current_line_index_set(PointerRNA *ptr, int value)
{
	Text *text = (Text *)ptr->data;
	txt_move_toline(text, value, 0);
}

static void rna_TextLine_body_get(PointerRNA *ptr, char *value)
{
	TextLine *line = (TextLine *)ptr->data;

	if (line->line)
		strcpy(value, line->line);
	else
		value[0] = '\0';
}

static int rna_TextLine_body_length(PointerRNA *ptr)
{
	TextLine *line = (TextLine *)ptr->data;
	return line->len;
}

static void rna_TextLine_body_set(PointerRNA *ptr, const char *value)
{
	TextLine *line = (TextLine *)ptr->data;
	int len = strlen(value);

	if (line->line)
		MEM_freeN(line->line);

	line->line = MEM_mallocN((len + 1) * sizeof(char), "rna_text_body");
	line->len = len;
	memcpy(line->line, value, len + 1);

	if (line->format) {
		MEM_freeN(line->format);
		line->format = NULL;
	}
}

#else

static void rna_def_text_line(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "TextLine", NULL);
	RNA_def_struct_ui_text(srna, "Text Line", "Line of text in a Text data-block");
	
	prop = RNA_def_property(srna, "body", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_TextLine_body_get", "rna_TextLine_body_length", "rna_TextLine_body_set");
	RNA_def_property_ui_text(prop, "Line", "Text in the line");
	RNA_def_property_update(prop, NC_TEXT | NA_EDITED, NULL);
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);
}

static void rna_def_text(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "Text", "ID");
	RNA_def_struct_ui_text(srna, "Text", "Text data-block referencing an external or packed text file");
	RNA_def_struct_ui_icon(srna, ICON_TEXT);
	RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);
	
	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_Text_filename_get", "rna_Text_filename_length", "rna_Text_filename_set");
	RNA_def_property_ui_text(prop, "File Path", "Filename of the text file");

	prop = RNA_def_property(srna, "is_dirty", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", TXT_ISDIRTY);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Dirty", "Text file has been edited since last save");

	prop = RNA_def_property(srna, "is_modified", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Text_modified_get", NULL);
	RNA_def_property_ui_text(prop, "Modified", "Text file on disk is different than the one in memory");

	prop = RNA_def_property(srna, "is_in_memory", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", TXT_ISMEM);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Memory", "Text file is in memory, without a corresponding file on disk");
	
	prop = RNA_def_property(srna, "use_module", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", TXT_ISSCRIPT);
	RNA_def_property_ui_text(prop, "Register",
	                         "Register this text as a module on loading, Text name must end with \".py\"");

	prop = RNA_def_property(srna, "use_tabs_as_spaces", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", TXT_TABSTOSPACES);
	RNA_def_property_ui_text(prop, "Tabs as Spaces", "Automatically converts all new tabs into spaces");

	prop = RNA_def_property(srna, "lines", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "TextLine");
	RNA_def_property_ui_text(prop, "Lines", "Lines of text");
	
	prop = RNA_def_property(srna, "current_line", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "curl");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "TextLine");
	RNA_def_property_ui_text(prop, "Current Line", "Current line, and start line of selection if one exists");

	prop = RNA_def_property(srna, "current_character", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "curc");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Current Character",
	                         "Index of current character in current line, and also start index of "
	                         "character in selection if one exists");
	
	prop = RNA_def_property(srna, "current_line_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_Text_current_line_index_get", "rna_Text_current_line_index_set", NULL);
	RNA_def_property_ui_text(prop, "Current Line Index", "Index of current TextLine in TextLine collection");
	RNA_def_property_update(prop, NC_TEXT | ND_CURSOR, NULL);

	prop = RNA_def_property(srna, "select_end_line", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "sell");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "TextLine");
	RNA_def_property_ui_text(prop, "Selection End Line", "End line of selection");
	
	prop = RNA_def_property(srna, "select_end_character", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "selc");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Selection End Character",
	                         "Index of character after end of selection in the selection end line");
	
	RNA_api_text(srna);
}

void RNA_def_text(BlenderRNA *brna)
{
	rna_def_text_line(brna);
	rna_def_text(brna);
}

#endif
