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

#include <limits.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BKE_text.h"

#include "ED_text.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_text_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

static void rna_Text_filename_get(PointerRNA *ptr, char *value)
{
  Text *text = (Text *)ptr->data;

  if (text->filepath) {
    strcpy(value, text->filepath);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_Text_filename_length(PointerRNA *ptr)
{
  Text *text = (Text *)ptr->data;
  return (text->filepath) ? strlen(text->filepath) : 0;
}

static void rna_Text_filename_set(PointerRNA *ptr, const char *value)
{
  Text *text = (Text *)ptr->data;

  if (text->filepath) {
    MEM_freeN(text->filepath);
  }

  if (value[0]) {
    text->filepath = BLI_strdup(value);
  }
  else {
    text->filepath = NULL;
  }
}

static bool rna_Text_modified_get(PointerRNA *ptr)
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
  Text *text = ptr->data;
  TextLine *line = BLI_findlink(&text->lines, value);
  if (line == NULL) {
    line = text->lines.last;
  }
  text->curl = line;
  text->curc = 0;
}

static int rna_Text_select_end_line_index_get(PointerRNA *ptr)
{
  Text *text = ptr->data;
  return BLI_findindex(&text->lines, text->sell);
}

static void rna_Text_select_end_line_index_set(PointerRNA *ptr, int value)
{
  Text *text = ptr->data;
  TextLine *line = BLI_findlink(&text->lines, value);
  if (line == NULL) {
    line = text->lines.last;
  }
  text->sell = line;
  text->selc = 0;
}

static int rna_Text_current_character_get(PointerRNA *ptr)
{
  Text *text = ptr->data;
  return BLI_str_utf8_offset_to_index(text->curl->line, text->curc);
}

static void rna_Text_current_character_set(PointerRNA *ptr, int index)
{
  Text *text = ptr->data;
  TextLine *line = text->curl;
  const int len_utf8 = BLI_strlen_utf8(line->line);
  CLAMP_MAX(index, len_utf8);
  text->curc = BLI_str_utf8_offset_from_index(line->line, index);
}

static int rna_Text_select_end_character_get(PointerRNA *ptr)
{
  Text *text = ptr->data;
  return BLI_str_utf8_offset_to_index(text->sell->line, text->selc);
}

static void rna_Text_select_end_character_set(PointerRNA *ptr, int index)
{
  Text *text = ptr->data;
  TextLine *line = text->sell;
  const int len_utf8 = BLI_strlen_utf8(line->line);
  CLAMP_MAX(index, len_utf8);
  text->selc = BLI_str_utf8_offset_from_index(line->line, index);
}

static void rna_TextLine_body_get(PointerRNA *ptr, char *value)
{
  TextLine *line = (TextLine *)ptr->data;

  if (line->line) {
    strcpy(value, line->line);
  }
  else {
    value[0] = '\0';
  }
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

  if (line->line) {
    MEM_freeN(line->line);
  }

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
  RNA_def_property_string_funcs(
      prop, "rna_TextLine_body_get", "rna_TextLine_body_length", "rna_TextLine_body_set");
  RNA_def_property_ui_text(prop, "Line", "Text in the line");
  RNA_def_property_update(prop, NC_TEXT | NA_EDITED, NULL);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);
}

static void rna_def_text(BlenderRNA *brna)
{

  static const EnumPropertyItem indentation_items[] = {
      {0, "TABS", 0, "Tabs", "Indent using tabs"},
      {TXT_TABSTOSPACES, "SPACES", 0, "Spaces", "Indent using spaces"},
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Text", "ID");
  RNA_def_struct_ui_text(
      srna, "Text", "Text data-block referencing an external or packed text file");
  RNA_def_struct_ui_icon(srna, ICON_TEXT);
  RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_Text_filename_get", "rna_Text_filename_length", "rna_Text_filename_set");
  RNA_def_property_ui_text(prop, "File Path", "Filename of the text file");

  prop = RNA_def_property(srna, "is_dirty", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", TXT_ISDIRTY);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Dirty", "Text file has been edited since last save");

  prop = RNA_def_property(srna, "is_modified", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Text_modified_get", NULL);
  RNA_def_property_ui_text(
      prop, "Modified", "Text file on disk is different than the one in memory");

  prop = RNA_def_property(srna, "is_in_memory", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", TXT_ISMEM);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Memory", "Text file is in memory, without a corresponding file on disk");

  prop = RNA_def_property(srna, "use_module", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", TXT_ISSCRIPT);
  RNA_def_property_ui_text(
      prop, "Register", "Run this text as a script on loading, Text name must end with \".py\"");

  prop = RNA_def_property(srna, "indentation", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
  RNA_def_property_enum_items(prop, indentation_items);
  RNA_def_property_ui_text(prop, "Indentation", "Use tabs or spaces for indentation");

  prop = RNA_def_property(srna, "lines", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "TextLine");
  RNA_def_property_ui_text(prop, "Lines", "Lines of text");

  prop = RNA_def_property(srna, "current_line", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "curl");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "TextLine");
  RNA_def_property_ui_text(
      prop, "Current Line", "Current line, and start line of selection if one exists");

  prop = RNA_def_property(srna, "current_character", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop,
                           "Current Character",
                           "Index of current character in current line, and also start index of "
                           "character in selection if one exists");
  RNA_def_property_int_funcs(
      prop, "rna_Text_current_character_get", "rna_Text_current_character_set", NULL);
  RNA_def_property_update(prop, NC_TEXT | ND_CURSOR, NULL);

  prop = RNA_def_property(srna, "current_line_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(
      prop, "rna_Text_current_line_index_get", "rna_Text_current_line_index_set", NULL);
  RNA_def_property_ui_text(
      prop, "Current Line Index", "Index of current TextLine in TextLine collection");
  RNA_def_property_update(prop, NC_TEXT | ND_CURSOR, NULL);

  prop = RNA_def_property(srna, "select_end_line", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "sell");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "TextLine");
  RNA_def_property_ui_text(prop, "Selection End Line", "End line of selection");

  prop = RNA_def_property(srna, "select_end_line_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(
      prop, "rna_Text_select_end_line_index_get", "rna_Text_select_end_line_index_set", NULL);
  RNA_def_property_ui_text(prop, "Select End Line Index", "Index of last TextLine in selection");
  RNA_def_property_update(prop, NC_TEXT | ND_CURSOR, NULL);

  prop = RNA_def_property(srna, "select_end_character", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop,
                           "Selection End Character",
                           "Index of character after end of selection in the selection end line");
  RNA_def_property_int_funcs(
      prop, "rna_Text_select_end_character_get", "rna_Text_select_end_character_set", NULL);
  RNA_def_property_update(prop, NC_TEXT | ND_CURSOR, NULL);

  RNA_api_text(srna);
}

void RNA_def_text(BlenderRNA *brna)
{
  rna_def_text_line(brna);
  rna_def_text(brna);
}

#endif
