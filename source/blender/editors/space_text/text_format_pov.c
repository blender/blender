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

/** \file blender/editors/space_text/text_format_pov.c
 *  \ingroup sptext
 */

#include <string.h>

#include "BLI_blenlib.h"

#include "DNA_text_types.h"
#include "DNA_space_types.h"

#include "BKE_text.h"

#include "text_format.h"

/* *** POV Keywords (for format_line) *** */

/* Checks the specified source string for a POV keyword (minus boolean & 'nil').
 * This name must start at the beginning of the source string and must be
 * followed by a non-identifier (see text_check_identifier(char)) or null char.
 *
 * If a keyword is found, the length of the matching word is returned.
 * Otherwise, -1 is returned.
 *
 * See:
 * http://www.povray.org/documentation/view/3.7.0/212/
 */

static int txtfmt_pov_find_keyword(const char *string)
{
	int i, len;
	/* Language Directives */
	if      (STR_LITERAL_STARTSWITH(string, "deprecated",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "persistent",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "statistics",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "version",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "warning",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "declare",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "default",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "include",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "append",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "elseif",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "debug",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "break",  	   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "else",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "error",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fclose",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fopen",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ifndef",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ifdef",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "patch",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "local",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "macro",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "range",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "read",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "render",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "switch",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "undef",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "while",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "write",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "case",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "end",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "for",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "if",          len)) i = len;
	else                                                         i = 0;

	/* If next source char is an identifier (eg. 'i' in "definite") no match */
	return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static int txtfmt_pov_find_reserved_keywords(const char *string)
{
	int i, len;
	/* POV-Ray Built-in Variables
	 * list is from...
	 * http://www.povray.org/documentation/view/3.7.0/212/
	 */

	/* Float Functions */
	if 		(STR_LITERAL_STARTSWITH(string, "conserve_energy",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "max_intersections",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "dimension_size",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bitwise_and",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bitwise_or",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bitwise_xor",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "file_exists",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "precompute",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "dimensions",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "clipped_by",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "shadowless",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "turb_depth",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "reciprocal",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "quaternion",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "phong_size",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tesselate",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "save_file",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "load_file",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "max_trace",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "transform",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "translate",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "direction",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "roughness",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "metallic",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "gts_load",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "gts_save",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "location",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "altitude",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "function",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "evaluate",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "inverse",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "collect",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "target",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "albedo",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "rotate",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "matrix",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "look_at",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "jitter",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "angle",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "right",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "scale",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "child",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "crand",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "blink",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "defined",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "degrees",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "inside",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "radians",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "vlength",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "select",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "floor",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "strcmp",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "strlen",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tessel",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sturm",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "abs",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "acosh",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "prod",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "with",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "acos",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "asc",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "asinh",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "asin",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "atan2",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "atand",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "atanh",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "atan",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ceil",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "warp",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cosh",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "log",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "max",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "min",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "mod",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pow",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "rand",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "seed",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "form",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sinh",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sqrt",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tanh",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "vdot",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sin",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sqr",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sum",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pwr",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tan",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "val",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cos",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "div",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "exp",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "int",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sky",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "up",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ln",                 len)) i = len;
	/* Color Identifiers */
	else if (STR_LITERAL_STARTSWITH(string, "transmit",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "filter",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "srgbft",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "srgbf",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "srgbt",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "rgbft",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "gamma",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "green",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "blue",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "gray",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "srgb",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sRGB",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "SRGB",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "rgbf",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "rgbt",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "rgb",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "red",                 len)) i = len;
	/* Color Spaces */
	else if (STR_LITERAL_STARTSWITH(string, "pov",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "hsl",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "hsv",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "xyl",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "xyv",                 len)) i = len;
	/* Vector Functions */
	else if (STR_LITERAL_STARTSWITH(string, "vaxis_rotate",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "vturbulence",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "min_extent",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "vnormalize",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "max_extent",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "vrotate",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "vcross",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "trace",              len)) i = len;
	/* String Functions */
	else if (STR_LITERAL_STARTSWITH(string, "file_time",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "datetime",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "concat",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "strlwr",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "strupr",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "substr",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "vstr",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "chr",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "str",                len)) i = len;
	else                                                                i = 0;

	/* If next source char is an identifier (eg. 'i' in "definite") no match */
	return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}


static int txtfmt_pov_find_reserved_builtins(const char *string)
{
	int i, len;

	/* POV-Ray Built-in Variables
	 * list is from...
	 * http://www.povray.org/documentation/view/3.7.0/212/
	 */
	/* Language Keywords */
	if      (STR_LITERAL_STARTSWITH(string, "reflection_exponent", len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "area_illumination",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "all_intersections",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cutaway_textures",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "smooth_triangle",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "lommel_seeliger",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "falloff_angle",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "aa_threshold",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "hypercomplex",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "major_radius",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "max_distance",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "max_iteration",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "colour_space",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "color_space",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "iridescence",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "subsurface",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "scattering",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "absorption",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "water_level",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "reflection",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "max_extent",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "oren_nayar",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "refraction",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "hierarchy",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "radiosity",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tolerance",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "interior",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "toroidal",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "emission",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "material",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "internal",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "photons",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "arc_angle",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "minnaert",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "texture",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "array",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "black_hole",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "component",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "composite",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "coords",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cube",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "dist_exp",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "exterior",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "file_gamma",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "flatness",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "planet",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "screw",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "keep",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "flip",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "move",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "roll",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "look_at",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "metric",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "offset",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "orientation",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pattern",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "precision",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "width",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "repeat",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bend",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "size",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "alpha",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "slice",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "smooth",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "solid",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "all",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "now",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pot",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "type",                len)) i = len;
	/* Animation Options */
	else if (STR_LITERAL_STARTSWITH(string, "global_settings",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "input_file_name",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "initial_clock",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "initial_frame",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "frame_number",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "image_height",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "image_width",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "final_clock",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "final_frame",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "clock_delta",     	   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "clock_on",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "clock",               len)) i = len;
	/* Spline Identifiers */
	else if (STR_LITERAL_STARTSWITH(string, "extended_x_spline",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "general_x_spline",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "quadratic_spline",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "basic_x_spline",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "natural_spline",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "linear_spline",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bezier_spline",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "akima_spline",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cubic_spline",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sor_spline",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tcb_spline",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "linear_sweep",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "conic_sweep",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "b_spline",            len)) i = len;
	/* Patterns */
	else if (STR_LITERAL_STARTSWITH(string, "pigment_pattern",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "image_pattern",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "density_file",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cylindrical",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "proportion",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "triangular",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "image_map",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "proximity",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "spherical",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bump_map",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "wrinkles",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "average",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "voronoi",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "masonry",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "binary",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "boxed",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bozo",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "brick",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bumps",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cells",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "checker",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "crackle",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cubic",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "dents",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "facets",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "gradient",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "granite",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "hexagon",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "julia",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "leopard",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "magnet",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "mandel",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "marble",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "onion",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pavement",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "planar",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "quilted",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "radial",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ripples",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "slope",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "spiral1",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "spiral2",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "spotted",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "square",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tile2",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tiling",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tiles",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "waves",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "wood",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "agate",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "aoi",                 len)) i = len;
	/* Objects */
	else if (STR_LITERAL_STARTSWITH(string, "superellipsoid",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bicubic_patch",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "julia_fractal",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "height_field",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cubic_spline",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sphere_sweep",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "light_group",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "light_source",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "intersection",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "isosurface",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "background",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sky_sphere",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cylinder",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "difference",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "brilliance",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "parametric",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "interunion",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "intermerge",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "polynomial",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "displace",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "specular",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ambient",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "diffuse",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "polygon",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "quadric",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "quartic",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "rainbow",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sphere",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "spline",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "prism",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "camera",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "galley",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cubic",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "phong",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cone",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "blob",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "box",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "disc",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fog",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "lathe",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "merge",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "mesh2",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "mesh",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "object",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ovus",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "lemon",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "plane",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "poly",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "irid",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sor",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "text",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "torus",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "triangle",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "union",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "colour",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "color",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "media",               len)) i = len;
	/* Built-in Vectors */
	else if (STR_LITERAL_STARTSWITH(string, "t",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "u",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "v",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "x",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "y",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "z",                   len)) i = len;
	else                                                                 i = 0;

	/* If next source char is an identifier (eg. 'i' in "definite") no match */
	return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}


/* Checks the specified source string for a POV modifiers. This
 * name must start at the beginning of the source string and must be followed
 * by a non-identifier (see text_check_identifier(char)) or null character.
 *
 * If a special name is found, the length of the matching name is returned.
 * Otherwise, -1 is returned.
 *
 * See:
 * http://www.povray.org/documentation/view/3.7.0/212/
 */

static int txtfmt_pov_find_specialvar(const char *string)
{
	int i, len;
	/* Modifiers */
	if      (STR_LITERAL_STARTSWITH(string, "dispersion_samples", len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "projected_through",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "double_illuminate",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "expand_thresholds",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "media_interaction",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "media_attenuation",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "low_error_factor",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "recursion_limit",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "interior_texture",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "max_trace_level",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "gray_threshold",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pretrace_start",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "normal_indices",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "normal_vectors",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "vertex_vectors",     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "noise_generator",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "irid_wavelength",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "number_of_waves",    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ambient_light",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "inside_vector",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "face_indices",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "texture_list",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "max_gradient",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "uv_indices",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "uv_vectors",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fade_distance",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "global_lights",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "no_bump_scale",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pretrace_end",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "no_radiosity",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "no_reflection",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "assumed_gamma",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "scallop_wave",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "triangle_wave",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "nearest_count",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "maximum_reuse",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "minimum_reuse",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "always_sample",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "translucency",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "eccentricity",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "contained_by",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "inside_point",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "adc_bailout",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "density_map",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "split_union",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "mm_per_unit",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "agate_turb",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bounded_by",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "brick_size",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "hf_gray_16",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "dispersion",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "extinction",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "thickness",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "color_map",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "colour_map",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cubic_wave",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fade_colour",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fade_power",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fade_color",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "normal_map",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pigment_map",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "quick_color",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "quick_colour",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "material_map",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pass_through",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "interpolate",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "texture_map",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "error_bound",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "brightness",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "use_color",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "use_alpha",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "use_colour",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "use_index",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "uv_mapping",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "importance",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "max_sample",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "intervals",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sine_wave",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "slope_map",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "poly_wave",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "no_shadow",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ramp_wave",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "precision",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "original",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "accuracy",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "map_type",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "no_image",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "distance",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "autostop",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "caustics",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "octaves",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "aa_level",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "frequency",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fog_offset",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "modulation",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "outbound",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "no_cache",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pigment",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "charset",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "inbound",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "outside",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "inner",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "turbulence",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "threshold",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "accuracy",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "polarity",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bump_size",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "circular",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "control0",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "control1",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "maximal",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "minimal",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fog_type",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fog_alt",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "samples",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "origin",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "amount",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "adaptive",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "exponent",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "strength",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "density",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fresnel",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "albinos",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "finish",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "method",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "omega",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fixed",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "spacing",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "u_steps",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "v_steps",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "offset",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "hollow",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "gather",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "lambda",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "mortar",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "cubic",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "count",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "once",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "orient",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "normal",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "phase",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ratio",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "open",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ior",                len)) i = len;
	/* Light Types and options*/
	else if (STR_LITERAL_STARTSWITH(string, "area_light",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "looks_like",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fade_power",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tightness",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "spotlight",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "parallel",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "point_at",           len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "falloff",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "radius",             len)) i = len;
	/* Camera Types and options*/
	else if (STR_LITERAL_STARTSWITH(string, "omni_directional_stereo",  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "lambert_cylindrical",      len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "miller_cylindrical",       len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "lambert_azimuthal",        len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ultra_wide_angle",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "camera_direction",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "camera_location ",         len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "van_der_grinten",          len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "aitoff_hammer",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "smyth_craster",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "orthographic",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "camera_right",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "blur_samples",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "plate_carree",             len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "camera_type",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "perspective",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "mesh_camera",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "focal_point",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "balthasart",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "confidence",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "parallaxe",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "hobo_dyer",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "camera_up",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "panoramic",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "eckert_vi",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "eckert_iv",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "mollweide",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "aperture",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "behrmann",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "variance",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "stereo",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "icosa",                    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tetra",                    len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "octa",                     len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "mercator",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "omnimax",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "fisheye",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "edwards",                  len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "peters",                   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "gall",                     len)) i = len;
	else                                                                i = 0;

	/* If next source char is an identifier (eg. 'i' in "definite") no match */
	return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static int txtfmt_pov_find_bool(const char *string)
{
	int i, len;
	/*Built-in Constants*/
	if      (STR_LITERAL_STARTSWITH(string, "unofficial",   len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "false",   		len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "no",      		len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "off",     		len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "true",    		len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "yes",     		len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "on",      		len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pi",      		len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tau",     		len)) i = len;
	/* Encodings */
	else if (STR_LITERAL_STARTSWITH(string, "sint16be",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sint16le",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sint32be",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sint32le",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "uint16be",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "uint16le",            len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bt2020",              len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "bt709",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sint8",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "uint8",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ascii",               len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "utf8",                len)) i = len;
	/* Filetypes */
	else if (STR_LITERAL_STARTSWITH(string, "tiff",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "df3",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "exr",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "gif",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "hdr",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "iff",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "jpeg",                len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "pgm",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "png",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ppm",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "sys",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "tga",                 len)) i = len;
	else if (STR_LITERAL_STARTSWITH(string, "ttf",                 len)) i = len;
	else                                                                 i = 0;

	/* If next source char is an identifier (eg. 'i' in "Nonetheless") no match */
	return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static char txtfmt_pov_format_identifier(const char *str)
{
	char fmt;
	if      ((txtfmt_pov_find_specialvar(str))        != -1) fmt = FMT_TYPE_SPECIAL;
	else if ((txtfmt_pov_find_keyword(str))           != -1) fmt = FMT_TYPE_KEYWORD;
	else if ((txtfmt_pov_find_reserved_keywords(str)) != -1) fmt = FMT_TYPE_RESERVED;
	else if ((txtfmt_pov_find_reserved_builtins(str)) != -1) fmt = FMT_TYPE_DIRECTIVE;
	else                                                     fmt = FMT_TYPE_DEFAULT;
	return fmt;
}

static void txtfmt_pov_format_line(SpaceText *st, TextLine *line, const bool do_next)
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
			/* C-Style comments */
			if (cont & FMT_CONT_COMMENT_C) {
				if (*str == '*' && *(str + 1) == '/') {
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
			/* C-Style (multi-line) comments */
			if (*str == '/' && *(str + 1) == '*') {
				cont = FMT_CONT_COMMENT_C;
				*fmt = FMT_TYPE_COMMENT; fmt++; str++;
				*fmt = FMT_TYPE_COMMENT;
			}
			/* Single line comment */
			else if (*str == '/' && *(str + 1) == '/') {
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
			else if (prev != FMT_TYPE_DEFAULT && (i = txtfmt_pov_find_bool(str)) != -1) {
				if (i > 0) {
					text_format_fill_ascii(&str, &fmt, FMT_TYPE_NUMERAL, i);
				}
				else {
					str += BLI_str_utf8_size_safe(str) - 1;
					*fmt = FMT_TYPE_DEFAULT;
				}
			}
			/* Punctuation */
			else if (text_check_delim(*str)) {
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
				/* keep in sync with 'txtfmt_pov_format_identifier()' */
				if      ((i = txtfmt_pov_find_specialvar(str))        != -1) prev = FMT_TYPE_SPECIAL;
				else if ((i = txtfmt_pov_find_keyword(str))           != -1) prev = FMT_TYPE_KEYWORD;
				else if ((i = txtfmt_pov_find_reserved_keywords(str)) != -1) prev = FMT_TYPE_RESERVED;
				else if ((i = txtfmt_pov_find_reserved_builtins(str)) != -1) prev = FMT_TYPE_DIRECTIVE;

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
		txtfmt_pov_format_line(st, line->next, do_next);
	}

	flatten_string_free(&fs);
}

void ED_text_format_register_pov(void)
{
	static TextFormatType tft = {NULL};
	static const char *ext[] = {"pov", "inc", "mcr", "mac", NULL};

	tft.format_identifier = txtfmt_pov_format_identifier;
	tft.format_line = txtfmt_pov_format_line;
	tft.ext = ext;

	ED_text_format_register(&tft);
}
