/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptext
 */

#include <cstring>

#include "BLI_blenlib.h"

#include "DNA_space_types.h"
#include "DNA_text_types.h"

#include "BKE_text.h"

#include "text_format.hh"

/* -------------------------------------------------------------------- */
/** \name Local Literal Definitions
 * \{ */

/**
 * POV INI keyword (minus boolean & 'nil')
 * See:
 * http://www.povray.org/documentation/view/3.7.0/212/
 */

/* Language Directives */

static const char *text_format_pov_ini_literals_keyword_data[]{
    /* Force single column, sorted list. */
    /* clang-format off */
    "A",
    "C",
    "F",
    "I",
    "N",
    "P",
    "Q",
    "S",
    "T",
    "U",
    "append",
    "break",
    "case",
    "debug",
    "declare",
    "default",
    "deprecated",
    "else",
    "elseif",
    "end",
    "error",
    "fclose",
    "fopen",
    "for",
    "if",
    "ifdef",
    "ifndef",
    "include",
    "local",
    "macro",
    "range",
    "read",
    "render",
    "statistics",
    "switch",
    "undef",
    "version",
    "warning",
    "while",
    "write",
    /* clang-format on */
};
static const Span<const char *> text_format_pov_ini_literals_keyword(
    text_format_pov_ini_literals_keyword_data,
    ARRAY_SIZE(text_format_pov_ini_literals_keyword_data));

/**
 * POV-Ray Built-in INI Variables
 * list is from...
 * http://www.povray.org/documentation/view/3.7.0/212/
 */
static const char *text_format_pov_ini_literals_reserved_data[]{
    /* Force single column, sorted list. */
    /* clang-format off */
    "AlertOnCompletion",
    "AlertSound",
    "All_Console",
    "All_File",
    "Antialias",
    "Antialias_Confidence",
    "Antialias_Depth",
    "Antialias_Depth",
    "Antialias_Gamma",
    "Antialias_Threshold",
    "Append_File",
    "AutoClose",
    "AutoRender",
    "BackgroundColour",
    "BackgroundFile",
    "Band0Width",
    "Band1Width",
    "Band2Width",
    "Band3Width",
    "Band4Width",
    "BetaVersionNo64",
    "Bits_Per_Color",
    "Bounding",
    "Bounding_Method",
    "Bounding_Threshold",
    "CheckNewVersion",
    "CommandLine",
    "Completion",
    "Compression",
    "Continue_Trace",
    "Create_Continue_Trace_Log",
    "Create_Ini",
    "CurrentDirectory",
    "Cyclic_Animation",
    "Debug_Console",
    "Debug_File",
    "Display",
    "Display_Gamma",
    "Dither",
    "Dither_Method",
    "DropToEditor",
    "DutyCycle",
    "End_Column",
    "End_Row",
    "ErrorColour",
    "Fatal_Console",
    "Fatal_Error_Command",
    "Fatal_Error_Return",
    "Fatal_File",
    "Field_Render",
    "Flags",
    "Font",
    "FontSize",
    "FontWeight",
    "Frame_Step",
    "Glare_Desaturation",
    "Height",
    "HideNewUserHelp",
    "HideWhenMainMinimized",
    "Include_Header",
    "IniOutputFile",
    "Input_File_Name",
    "ItsAboutTime",
    "Jitter",
    "Jitter_Amount",
    "KeepAboveMain",
    "KeepMessages",
    "LastBitmapName",
    "LastBitmapPath",
    "LastINIPath",
    "LastPath",
    "LastQueuePath",
    "LastRenderName",
    "LastRenderPath",
    "Library_Path",
    "Light_Buffer",
    "MakeActive",
    "NoShellOuts",
    "NoShelloutWait",
    "NormalPositionBottom",
    "NormalPositionLeft",
    "NormalPositionRight",
    "NormalPositionTop",
    "NormalPositionX",
    "NormalPositionY",
    "Odd_Field",
    "OutputFile",
    "Output_Alpha",
    "Output_File_Name",
    "Output_File_Type",
    "Output_to_File",
    "ParseErrorSound",
    "ParseErrorSoundEnabled",
    "Pause_When_Done",
    "Post_Frame_Command",
    "Post_Frame_Return",
    "Post_Scene_Command",
    "Post_Scene_Return",
    "Pre_Frame_Command",
    "Pre_Frame_Return",
    "Pre_Scene_Command",
    "Pre_Scene_Return",
    "PreserveBitmap",
    "PreventSleep",
    "Preview_End_Size",
    "Preview_Start_Size",
    "Priority",
    "Quality",
    "ReadWriteSourceDir",
    "Remove_Bounds",
    "RenderCompleteSound",
    "RenderCompleteSoundEnabled",
    "RenderErrorSound",
    "RenderErrorSoundEnabled",
    "Render_Console",
    "Render_File",
    "Rendering",
    "RenderwinClose",
    "RunCount",
    "Sampling_Method",
    "SaveSettingsOnExit",
    "SceneFile",
    "SecondaryINIFile",
    "SecondaryINISection",
    "SendSystemInfo",
    "ShowCmd",
    "SourceFile",
    "Split_Unions",
    "Start_Column",
    "Start_Row",
    "Statistic_Console",
    "Statistic_File",
    "Stochastic_Seed",
    "Subset_End_Frame",
    "Subset_Start_Frame",
    "SystemNoActive",
    "Test_Abort",
    "Test_Abort_Count",
    "TextColour",
    "TileBackground",
    "Transparency",
    "Use8BitMode",
    "UseExtensions",
    "UseToolbar",
    "UseTooltips",
    "User_Abort_Command",
    "User_Abort_Return",
    "Verbose",
    "Version",
    "VideoSource",
    "Vista_Buffer",
    "Warning Level",
    "WarningColour",
    "Warning_Console",
    "Warning_File",
    "Warning_Level",
    "Width",
    "ascii",
    "clock",
    "clock_delta",
    "clock_on",
    "df3",
    "exr",
    "final_clock",
    "final_frame",
    "frame_number",
    "gif",
    "hdr",
    "iff",
    "image_height",
    "image_width",
    "initial_clock",
    "initial_frame",
    "input_file_name",
    "jpeg",
    "pgm",
    "png",
    "ppm",
    "sint16be",
    "sint16le",
    "sint32be",
    "sint32le",
    "sint8",
    "sys",
    "tga",
    "tiff",
    "uint16be",
    "uint16le",
    "uint8",
    "utf8",
    /* clang-format on */
};
static const Span<const char *> text_format_pov_ini_literals_reserved(
    text_format_pov_ini_literals_reserved_data,
    ARRAY_SIZE(text_format_pov_ini_literals_reserved_data));

/** POV INI Built-in Constants */
static const char *text_format_pov_ini_literals_bool_data[]{
    /* Force single column, sorted list. */
    /* clang-format off */
    "%h",
    "%k",
    "%n",
    "%o",
    "%s",
    "%w",
    "false",
    "no",
    "off",
    "on",
    "pi",
    "tau",
    "true",
    "yes",
    /* clang-format on */
};
static const Span<const char *> text_format_pov_ini_literals_bool(
    text_format_pov_ini_literals_bool_data, ARRAY_SIZE(text_format_pov_ini_literals_bool_data));

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Functions (for #TextFormatType::format_line)
 * \{ */

static int txtfmt_ini_find_keyword(const char *string)
{
  const int i = text_format_string_literal_find(text_format_pov_ini_literals_keyword, string);

  /* If next source char is an identifier (eg. 'i' in "definite") no match */
  return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static int txtfmt_ini_find_reserved(const char *string)
{
  const int i = text_format_string_literal_find(text_format_pov_ini_literals_reserved, string);

  /* If next source char is an identifier (eg. 'i' in "definite") no match */
  return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static int txtfmt_ini_find_bool(const char *string)
{
  const int i = text_format_string_literal_find(text_format_pov_ini_literals_bool, string);

  /* If next source char is an identifier (eg. 'i' in "Nonetheless") no match */
  return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static char txtfmt_pov_ini_format_identifier(const char *str)
{
  char fmt;
  if (txtfmt_ini_find_keyword(str) != -1) {
    fmt = FMT_TYPE_KEYWORD;
  }
  else if (txtfmt_ini_find_reserved(str) != -1) {
    fmt = FMT_TYPE_RESERVED;
  }
  else {
    fmt = FMT_TYPE_DEFAULT;
  }
  return fmt;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format Line Implementation (#TextFormatType::format_line)
 * \{ */

static void txtfmt_pov_ini_format_line(SpaceText *st, TextLine *line, const bool do_next)
{
  FlattenString fs;
  const char *str;
  char *fmt;
  char cont_orig, cont, find, prev = ' ';
  int len, i;

  /* Get continuation from previous line */
  if (line->prev && line->prev->format != nullptr) {
    fmt = line->prev->format;
    cont = fmt[strlen(fmt) + 1]; /* Just after the null-terminator */
    BLI_assert((FMT_CONT_ALL & cont) == cont);
  }
  else {
    cont = FMT_CONT_NOP;
  }

  /* Get original continuation from this line */
  if (line->format != nullptr) {
    fmt = line->format;
    cont_orig = fmt[strlen(fmt) + 1]; /* Just after the null-terminator */
    BLI_assert((FMT_CONT_ALL & cont_orig) == cont_orig);
  }
  else {
    cont_orig = 0xFF;
  }

  len = flatten_string(st, &fs, line->line);
  str = fs.buf;
  if (!text_check_format_len(line, len)) {
    flatten_string_free(&fs);
    return;
  }
  fmt = line->format;

  while (*str) {
    /* Handle escape sequences by skipping both \ and next char */
    if (*str == '\\') {
      *fmt = prev;
      fmt++;
      str++;
      if (*str == '\0') {
        break;
      }
      *fmt = prev;
      fmt++;
      str += BLI_str_utf8_size_safe(str);
      continue;
    }
    /* Handle continuations */
    if (cont) {
      /* Multi-line comments */
      if (cont & FMT_CONT_COMMENT_C) {
        if (*str == ']' && *(str + 1) == ']') {
          *fmt = FMT_TYPE_COMMENT;
          fmt++;
          str++;
          *fmt = FMT_TYPE_COMMENT;
          cont = FMT_CONT_NOP;
        }
        else {
          *fmt = FMT_TYPE_COMMENT;
        }
        /* Handle other comments */
      }
      else {
        find = (cont & FMT_CONT_QUOTEDOUBLE) ? '"' : '\'';
        if (*str == find) {
          cont = 0;
        }
        *fmt = FMT_TYPE_STRING;
      }

      str += BLI_str_utf8_size_safe(str) - 1;
    }
    /* Not in a string... */
    else {
      /* Multi-line comments not supported */
      /* Single line comment */
      if (*str == ';') {
        text_format_fill(&str, &fmt, FMT_TYPE_COMMENT, len - int(fmt - line->format));
      }
      else if (ELEM(*str, '"', '\'')) {
        /* Strings */
        find = *str;
        cont = (*str == '"') ? FMT_CONT_QUOTEDOUBLE : FMT_CONT_QUOTESINGLE;
        *fmt = FMT_TYPE_STRING;
      }
      /* White-space (all white-space has been converted to spaces). */
      else if (*str == ' ') {
        *fmt = FMT_TYPE_WHITESPACE;
      }
      /* Numbers (digits not part of an identifier and periods followed by digits) */
      else if ((prev != FMT_TYPE_DEFAULT && text_check_digit(*str)) ||
               (*str == '.' && text_check_digit(*(str + 1))))
      {
        *fmt = FMT_TYPE_NUMERAL;
      }
      /* Booleans */
      else if (prev != FMT_TYPE_DEFAULT && (i = txtfmt_ini_find_bool(str)) != -1) {
        if (i > 0) {
          text_format_fill_ascii(&str, &fmt, FMT_TYPE_NUMERAL, i);
        }
        else {
          str += BLI_str_utf8_size_safe(str) - 1;
          *fmt = FMT_TYPE_DEFAULT;
        }
      }
      /* Punctuation */
      else if ((*str != '#') && text_check_delim(*str)) {
        *fmt = FMT_TYPE_SYMBOL;
      }
      /* Identifiers and other text (no previous white-space/delimiters so text continues). */
      else if (prev == FMT_TYPE_DEFAULT) {
        str += BLI_str_utf8_size_safe(str) - 1;
        *fmt = FMT_TYPE_DEFAULT;
      }
      /* Not white-space, a digit, punctuation, or continuing text.
       * Must be new, check for special words */
      else {
        /* Keep aligned arguments for readability. */
        /* clang-format off */

        /* Special vars(v) or built-in keywords(b) */
        /* keep in sync with `txtfmt_ini_format_identifier()`. */
        if        ((i = txtfmt_ini_find_keyword(str))  != -1) { prev = FMT_TYPE_KEYWORD;
        } else if ((i = txtfmt_ini_find_reserved(str)) != -1) { prev = FMT_TYPE_RESERVED;
        }
        /* clang-format on */

        if (i > 0) {
          text_format_fill_ascii(&str, &fmt, prev, i);
        }
        else {
          str += BLI_str_utf8_size_safe(str) - 1;
          *fmt = FMT_TYPE_DEFAULT;
        }
      }
    }
    prev = *fmt;
    fmt++;
    str++;
  }

  /* Terminate and add continuation char */
  *fmt = '\0';
  fmt++;
  *fmt = cont;

  /* If continuation has changed and we're allowed, process the next line */
  if (cont != cont_orig && do_next && line->next) {
    txtfmt_pov_ini_format_line(st, line->next, do_next);
  }

  flatten_string_free(&fs);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_text_format_register_pov_ini()
{
  static TextFormatType tft = {nullptr};
  static const char *ext[] = {"ini", nullptr};

  tft.format_identifier = txtfmt_pov_ini_format_identifier;
  tft.format_line = txtfmt_pov_ini_format_line;
  tft.ext = ext;
  tft.comment_line = "//";

  ED_text_format_register(&tft);

  BLI_assert(text_format_string_literals_check_sorted_array(text_format_pov_ini_literals_keyword));
  BLI_assert(
      text_format_string_literals_check_sorted_array(text_format_pov_ini_literals_reserved));
  BLI_assert(text_format_string_literals_check_sorted_array(text_format_pov_ini_literals_bool));
}

/** \} */
