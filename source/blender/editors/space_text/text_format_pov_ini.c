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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_text/text_format_pov_ini.c
 *  \ingroup sptext
 */

#include <string.h>

#include "BLI_blenlib.h"

#include "DNA_text_types.h"
#include "DNA_space_types.h"

#include "BKE_text.h"

#include "text_format.h"

/* *** POV INI Keywords (for format_line) *** */

/* Checks the specified source string for a POV INI keyword (minus boolean & 'nil').
 * This name must start at the beginning of the source string and must be
 * followed by a non-identifier (see text_check_identifier(char)) or null char.
 *
 * If a keyword is found, the length of the matching word is returned.
 * Otherwise, -1 is returned.
 *
 * See:
 * http://www.povray.org/documentation/view/3.7.0/212/
 */

static int txtfmt_ini_find_keyword(const char *string)
{
	int i, len;
	/* Language Directives */
	if      (STR_LITERAL_STARTSWITH(string, "deprecated", len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "statistics", len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "declare",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "default",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "version",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "warning",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "include",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fclose",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ifndef",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "append",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "elseif",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "debug",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "error",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fopen",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ifdef",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "local",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "macro",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "range",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "render",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "break",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "switch",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "undef",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "while",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "write",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "case",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "else",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "read",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "end",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "for",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "if",         len)) i = len;

	else if (STR_LITERAL_STARTSWITH(string, "I",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "S",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "A",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Q",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "U",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "F",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "C",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "N",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "P",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "T",          len)) i = len;

	else                                                        i = 0;

	/* If next source char is an identifier (eg. 'i' in "definite") no match */
	return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static int txtfmt_ini_find_reserved(const char *string)
{
	int i, len;
	/* POV-Ray Built-in INI Variables
	 * list is from...
	 * http://www.povray.org/documentation/view/3.7.0/212/
	 */
	     if (STR_LITERAL_STARTSWITH(string, "RenderCompleteSoundEnabled",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Create_Continue_Trace_Log",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ParseErrorSoundEnabled",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "RenderErrorSoundEnabled",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "HideWhenMainMinimized",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Antialias_Confidence",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "RenderCompleteSound",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ParseErrorSound",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "RenderErrorSound",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "UseExtensions",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ReadWriteSourceDir",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "NormalPositionLeft",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "NormalPositionTop",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "NormalPositionRight",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "NormalPositionBottom",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Pre_Scene_Command",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Pre_Frame_Command",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Post_Scene_Command",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Post_Frame_Command",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "User_Abort_Command",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Fatal_Error_Command",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "NormalPositionX",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "NormalPositionY",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Pre_Scene_Return",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Pre_Frame_Return",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Post_Scene_Return",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Post_Frame_Return",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "User_Abort_Return",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Fatal_Error_Return",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Antialias_Threshold",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Antialias_Gamma",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Antialias_Depth",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "input_file_name",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Subset_Start_Frame",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Subset_End_Frame",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "UseToolbar",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "UseTooltips",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Frame_Step",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Cyclic_Animation",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Field_Render",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Odd_Field",                    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "final_clock",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "final_frame",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "frame_number",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "initial_clock",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "initial_frame",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "image_height",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "image_width",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Start_Column",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Start_Row",                    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "End_Column",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "End_Row",                      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Test_Abort_Count",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Test_Abort",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Continue_Trace",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Bounding_Method",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Create_Ini",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Display_Gamma",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Display",                      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Version",                      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Pause_When_Done",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Verbose",                      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Preview_Start_Size",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Preview_End_Size",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Output_to_File",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Input_File_Name",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Output_File_Name",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Output_File_Type",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Output_Alpha",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Bits_Per_Color",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Compression",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Dither_Method",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Include_Header",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Library_Path",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Debug_Console",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Fatal_Console",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Render_Console",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Statistic_Console",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Warning_Console",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Warning_Level",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "All_Console",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Debug_File",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Fatal_File",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Render_File",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Statistic_File",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Warning_File",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "All_File",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Quality",                      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Bounding_Threshold",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Bounding",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Light_Buffer",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Vista_Buffer",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Remove_Bounds",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Split_Unions",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Antialias",                    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Glare_Desaturation",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Sampling_Method",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Stochastic_Seed",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Jitter_Amount",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Jitter",                       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Antialias_Depth",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "CheckNewVersion",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "RunCount",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "CommandLine",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "TextColour",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "WarningColour",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ErrorColour",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "BackgroundColour",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "DropToEditor",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "LastRenderName",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "LastRenderPath",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "LastQueuePath",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "SecondaryINISection",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "BetaVersionNo64",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "LastBitmapName",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "LastBitmapPath",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "LastINIPath",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "SecondaryINIFile",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "BackgroundFile",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "SaveSettingsOnExit",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "TileBackground",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "HideNewUserHelp",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "SendSystemInfo",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ItsAboutTime",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "LastPath",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Band0Width",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Band1Width",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Band2Width",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Band3Width",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Band4Width",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ShowCmd",                      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Transparency",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Use8BitMode",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "MakeActive",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "KeepAboveMain",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "AutoClose",                    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "PreserveBitmap",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "FontSize",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "FontWeight",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "KeepMessages",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "AlertSound",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Completion",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Priority",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "DutyCycle",                    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "AlertOnCompletion",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "AutoRender",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "PreventSleep",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "NoShelloutWait",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "SystemNoActive",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "NoShellOuts",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "VideoSource",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "SceneFile",                    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "OutputFile",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "IniOutputFile",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "CurrentDirectory",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "SourceFile",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Rendering",                    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "RenderwinClose",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Append_File",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Warning Level",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "clock_delta",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "clock_on",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "clock",                        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Height",                       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Width",                        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Dither",                       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Flags",                        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "Font",                         len)) i = len;
	/* Filetypes */
	else if (STR_LITERAL_STARTSWITH(string, "df3",                          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "exr",                          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "gif",                          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "hdr",                          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "iff",                          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "jpeg",                         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pgm",                          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "png",                          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ppm",                          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sys",                          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tga",                          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tiff",                         len)) i = len;
	/* Encodings */
	else if (STR_LITERAL_STARTSWITH(string, "ascii",                        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "utf8",                         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "uint8",                        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "uint16be",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "uint16le",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sint8",                        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sint16be",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sint16le",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sint32be",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sint32le",                     len)) i = len;

	else                                                                          i = 0;

	/* If next source char is an identifier (eg. 'i' in "definite") no match */
	return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}




static int txtfmt_ini_find_bool(const char *string)
{
	int i, len;
	/* Built-in Constants */
	if      (STR_LITERAL_STARTSWITH(string, "false",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "no",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "off",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "true",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "yes",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "on",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pi",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tau",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "%o",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "%s",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "%n",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "%k",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "%h",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "%w",      len)) i = len;
	else                                                     i = 0;

	/* If next source char is an identifier (eg. 'i' in "Nonetheless") no match */
	return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static char txtfmt_pov_ini_format_identifier(const char *str)
{
	char fmt;
	if      ((txtfmt_ini_find_keyword(str))  != -1) fmt = FMT_TYPE_KEYWORD;
	else if ((txtfmt_ini_find_reserved(str)) != -1) fmt = FMT_TYPE_RESERVED;
	else                                            fmt = FMT_TYPE_DEFAULT;
	return fmt;
}

static void txtfmt_pov_ini_format_line(SpaceText *st, TextLine *line, const bool do_next)
{
	FlattenString fs;
	const char *str;
	char *fmt;
	char cont_orig, cont, find, prev = ' ';
	int len, i;

	/* Get continuation from previous line */
	if (line->prev && line->prev->format != NULL) {
		fmt = line->prev->format;
		cont = fmt[strlen(fmt) + 1]; /* Just after the null-terminator */
		BLI_assert((FMT_CONT_ALL & cont) == cont);
	}
	else {
		cont = FMT_CONT_NOP;
	}

	/* Get original continuation from this line */
	if (line->format != NULL) {
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
			*fmt = prev; fmt++; str++;
			if (*str == '\0') break;
			*fmt = prev; fmt++; str += BLI_str_utf8_size_safe(str);
			continue;
		}
		/* Handle continuations */
		else if (cont) {
			/* Multi-line comments */
			if (cont & FMT_CONT_COMMENT_C) {
				if (*str == ']' && *(str + 1) == ']') {
					*fmt = FMT_TYPE_COMMENT; fmt++; str++;
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
				if (*str == find) cont = 0;
				*fmt = FMT_TYPE_STRING;
			}

			str += BLI_str_utf8_size_safe(str) - 1;
		}
		/* Not in a string... */
		else {
			/* Multi-line comments not supported */
			/* Single line comment */
			if (*str == ';') {
				text_format_fill(&str, &fmt, FMT_TYPE_COMMENT, len - (int)(fmt - line->format));
			}
			else if (*str == '"' || *str == '\'') {
				/* Strings */
				find = *str;
				cont = (*str == '"') ? FMT_CONT_QUOTEDOUBLE : FMT_CONT_QUOTESINGLE;
				*fmt = FMT_TYPE_STRING;
			}
			/* Whitespace (all ws. has been converted to spaces) */
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
			/* Identifiers and other text (no previous ws. or delims. so text continues) */
			else if (prev == FMT_TYPE_DEFAULT) {
				str += BLI_str_utf8_size_safe(str) - 1;
				*fmt = FMT_TYPE_DEFAULT;
			}
			/* Not ws, a digit, punct, or continuing text. Must be new, check for special words */
			else {
				/* Special vars(v) or built-in keywords(b) */
				/* keep in sync with 'txtfmt_ini_format_identifier()' */
				if      ((i = txtfmt_ini_find_keyword(str))  != -1) prev = FMT_TYPE_KEYWORD;
				else if ((i = txtfmt_ini_find_reserved(str)) != -1) prev = FMT_TYPE_RESERVED;

				if (i > 0) {
					text_format_fill_ascii(&str, &fmt, prev, i);
				}
				else {
					str += BLI_str_utf8_size_safe(str) - 1;
					*fmt = FMT_TYPE_DEFAULT;
				}
			}
		}
		prev = *fmt; fmt++; str++;
	}

	/* Terminate and add continuation char */
	*fmt = '\0'; fmt++;
	*fmt = cont;

	/* If continuation has changed and we're allowed, process the next line */
	if (cont != cont_orig && do_next && line->next) {
		txtfmt_pov_ini_format_line(st, line->next, do_next);
	}

	flatten_string_free(&fs);
}

void ED_text_format_register_pov_ini(void)
{
	static TextFormatType tft = {NULL};
	static const char *ext[] = {"ini", NULL};

	tft.format_identifier = txtfmt_pov_ini_format_identifier;
	tft.format_line = txtfmt_pov_ini_format_line;
	tft.ext = ext;

	ED_text_format_register(&tft);
}
