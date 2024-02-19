/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <climits>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLT_translation.hh"

#include "BKE_text.h"

#include "ED_text.hh"

#include "RNA_define.hh"

#include "rna_internal.hh"

#include "DNA_text_types.h"

#include "WM_types.hh"

#ifdef RNA_RUNTIME

static void rna_Text_filepath_get(PointerRNA *ptr, char *value)
{
  const Text *text = (const Text *)ptr->data;

  if (text->filepath) {
    strcpy(value, text->filepath);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_Text_filepath_length(PointerRNA *ptr)
{
  const Text *text = (const Text *)ptr->data;
  return (text->filepath) ? strlen(text->filepath) : 0;
}

static void rna_Text_filepath_set(PointerRNA *ptr, const char *value)
{
  Text *text = (Text *)ptr->data;

  if (text->filepath) {
    MEM_freeN(text->filepath);
  }

  if (value[0]) {
    text->filepath = BLI_strdup(value);
  }
  else {
    text->filepath = nullptr;
  }
}

static bool rna_Text_modified_get(PointerRNA *ptr)
{
  const Text *text = (const Text *)ptr->data;
  return BKE_text_file_modified_check(text) != 0;
}

static int rna_Text_current_line_index_get(PointerRNA *ptr)
{
  const Text *text = (const Text *)ptr->data;
  return BLI_findindex(&text->lines, text->curl);
}

static void rna_Text_current_line_index_set(PointerRNA *ptr, int value)
{
  Text *text = static_cast<Text *>(ptr->data);
  TextLine *line = static_cast<TextLine *>(BLI_findlink(&text->lines, value));
  if (line == nullptr) {
    line = static_cast<TextLine *>(text->lines.last);
  }
  text->curl = line;
  text->curc = 0;
}

static int rna_Text_select_end_line_index_get(PointerRNA *ptr)
{
  const Text *text = static_cast<Text *>(ptr->data);
  return BLI_findindex(&text->lines, text->sell);
}

static void rna_Text_select_end_line_index_set(PointerRNA *ptr, int value)
{
  Text *text = static_cast<Text *>(ptr->data);
  TextLine *line = static_cast<TextLine *>(BLI_findlink(&text->lines, value));
  if (line == nullptr) {
    line = static_cast<TextLine *>(text->lines.last);
  }
  text->sell = line;
  text->selc = 0;
}

static int rna_Text_current_character_get(PointerRNA *ptr)
{
  const Text *text = static_cast<const Text *>(ptr->data);
  const TextLine *line = text->curl;
  return BLI_str_utf8_offset_to_index(line->line, line->len, text->curc);
}

static void rna_Text_current_character_set(PointerRNA *ptr, const int index)
{
  Text *text = static_cast<Text *>(ptr->data);
  TextLine *line = text->curl;
  text->curc = BLI_str_utf8_offset_from_index(line->line, line->len, index);
}

static int rna_Text_select_end_character_get(PointerRNA *ptr)
{
  Text *text = static_cast<Text *>(ptr->data);
  TextLine *line = text->sell;
  return BLI_str_utf8_offset_to_index(line->line, line->len, text->selc);
}

static void rna_Text_select_end_character_set(PointerRNA *ptr, const int index)
{
  Text *text = static_cast<Text *>(ptr->data);
  TextLine *line = text->sell;
  text->selc = BLI_str_utf8_offset_from_index(line->line, line->len, index);
}

static void rna_TextLine_body_get(PointerRNA *ptr, char *value)
{
  const TextLine *line = (const TextLine *)ptr->data;

  if (line->line) {
    strcpy(value, line->line);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_TextLine_body_length(PointerRNA *ptr)
{
  const TextLine *line = (const TextLine *)ptr->data;
  return line->len;
}

static void rna_TextLine_body_set(PointerRNA *ptr, const char *value)
{
  TextLine *line = (TextLine *)ptr->data;
  int len = strlen(value);

  if (line->line) {
    MEM_freeN(line->line);
  }

  line->line = static_cast<char *>(MEM_mallocN((len + 1) * sizeof(char), "rna_text_body"));
  line->len = len;
  memcpy(line->line, value, len + 1);

  if (line->format) {
    MEM_freeN(line->format);
    line->format = nullptr;
  }
}

#else

static void rna_def_text_line(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "TextLine", nullptr);
  RNA_def_struct_ui_text(srna, "Text Line", "Line of text in a Text data-block");

  prop = RNA_def_property(srna, "body", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_TextLine_body_get", "rna_TextLine_body_length", "rna_TextLine_body_set");
  RNA_def_property_ui_text(prop, "Line", "Text in the line");
  RNA_def_property_update(prop, NC_TEXT | NA_EDITED, nullptr);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);
}

static void rna_def_text(BlenderRNA *brna)
{

  static const EnumPropertyItem indentation_items[] = {
      {0, "TABS", 0, "Tabs", "Indent using tabs"},
      {TXT_TABSTOSPACES, "SPACES", 0, "Spaces", "Indent using spaces"},
      {0, nullptr, 0, nullptr, nullptr},
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
      prop, "rna_Text_filepath_get", "rna_Text_filepath_length", "rna_Text_filepath_set");
  RNA_def_property_ui_text(prop, "File Path", "Filename of the text file");

  prop = RNA_def_property(srna, "is_dirty", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", TXT_ISDIRTY);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Dirty", "Text file has been edited since last save");

  prop = RNA_def_property(srna, "is_modified", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Text_modified_get", nullptr);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);
  RNA_def_property_ui_text(
      prop, "Modified", "Text file on disk is different than the one in memory");

  prop = RNA_def_property(srna, "is_in_memory", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", TXT_ISMEM);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Memory", "Text file is in memory, without a corresponding file on disk");

  prop = RNA_def_property(srna, "use_module", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", TXT_ISSCRIPT);
  RNA_def_property_ui_text(prop, "Register", "Run this text as a Python script on loading");

  prop = RNA_def_property(srna, "indentation", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flags");
  RNA_def_property_enum_items(prop, indentation_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);
  RNA_def_property_ui_text(prop, "Indentation", "Use tabs or spaces for indentation");

  prop = RNA_def_property(srna, "lines", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "TextLine");
  RNA_def_property_ui_text(prop, "Lines", "Lines of text");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);

  prop = RNA_def_property(srna, "current_line", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "curl");
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
      prop, "rna_Text_current_character_get", "rna_Text_current_character_set", nullptr);
  RNA_def_property_update(prop, NC_TEXT | ND_CURSOR, nullptr);

  prop = RNA_def_property(srna, "current_line_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(
      prop, "rna_Text_current_line_index_get", "rna_Text_current_line_index_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Current Line Index", "Index of current TextLine in TextLine collection");
  RNA_def_property_update(prop, NC_TEXT | ND_CURSOR, nullptr);

  prop = RNA_def_property(srna, "select_end_line", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "sell");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "TextLine");
  RNA_def_property_ui_text(prop, "Selection End Line", "End line of selection");

  prop = RNA_def_property(srna, "select_end_line_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(
      prop, "rna_Text_select_end_line_index_get", "rna_Text_select_end_line_index_set", nullptr);
  RNA_def_property_ui_text(prop, "Select End Line Index", "Index of last TextLine in selection");
  RNA_def_property_update(prop, NC_TEXT | ND_CURSOR, nullptr);

  prop = RNA_def_property(srna, "select_end_character", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop,
                           "Selection End Character",
                           "Index of character after end of selection in the selection end line");
  RNA_def_property_int_funcs(
      prop, "rna_Text_select_end_character_get", "rna_Text_select_end_character_set", nullptr);
  RNA_def_property_update(prop, NC_TEXT | ND_CURSOR, nullptr);

  RNA_api_text(srna);
}

void RNA_def_text(BlenderRNA *brna)
{
  rna_def_text_line(brna);
  rna_def_text(brna);
}

#endif
