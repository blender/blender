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

/** Language Directives */
static const char *text_format_pov_literals_keyword_data[]{
    /* Force single column, sorted list. */
    /* clang-format off */
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
    "patch",
    "persistent",
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
static const Span<const char *> text_format_pov_literals_keyword(
    text_format_pov_literals_keyword_data, ARRAY_SIZE(text_format_pov_literals_keyword_data));

/* POV-Ray Built-in Variables
 * list is from...
 * http://www.povray.org/documentation/view/3.7.0/212/
 */

/** Float Functions */
static const char *text_format_pov_literals_reserved_data[]{
    /* Force single column, sorted list. */
    /* clang-format on */
    "SRGB",
    "abs",
    "acos",
    "acosh",
    "albedo",
    "altitude",
    "angle",
    "asc",
    "asin",
    "asinh",
    "atan",
    "atan2",
    "atand",
    "atanh",
    "bitwise_and",
    "bitwise_or",
    "bitwise_xor",
    "blink",
    "blue",
    "ceil",
    "child",
    "chr",
    "clipped_by",
    "collect",
    "concat",
    "conserve_energy",
    "cos",
    "cosh",
    "crand",
    "datetime",
    "defined",
    "degrees",
    "dimension_size",
    "dimensions",
    "direction",
    "div",
    "evaluate",
    "exp",
    "file_exists",
    "file_time",
    "filter",
    "floor",
    "form",
    "function",
    "gamma",
    "gray",
    "green",
    "gts_load",
    "gts_save",
    "hsl",
    "hsv",
    "inside",
    "int",
    "inverse",
    "jitter",
    "ln",
    "load_file",
    "location",
    "log",
    "look_at",
    "matrix",
    "max",
    "max_extent",
    "max_intersections",
    "max_trace",
    "metallic",
    "min",
    "min_extent",
    "mod",
    "phong_size",
    "pov",
    "pow",
    "precompute",
    "prod",
    "pwr",
    "quaternion",
    "radians",
    "rand",
    "reciprocal",
    "red",
    "rgb",
    "rgbf",
    "rgbft",
    "rgbt",
    "right",
    "rotate",
    "roughness",
    "sRGB",
    "save_file",
    "scale",
    "seed",
    "select",
    "shadowless",
    "sin",
    "sinh",
    "sky",
    "sqr",
    "sqrt",
    "srgb",
    "srgbf",
    "srgbft",
    "srgbt",
    "str",
    "strcmp",
    "strlen",
    "strlwr",
    "strupr",
    "sturm",
    "substr",
    "sum",
    "tan",
    "tanh",
    "target",
    "tessel",
    "tesselate",
    "trace",
    "transform",
    "translate",
    "transmit",
    "turb_depth",
    "up",
    "val",
    "vaxis_rotate",
    "vcross",
    "vdot",
    "vlength",
    "vnormalize",
    "vrotate",
    "vstr",
    "vturbulence",
    "warp",
    "with",
    "xyl",
    "xyv",
    /* clang-format on */
};
static const Span<const char *> text_format_pov_literals_reserved(
    text_format_pov_literals_reserved_data, ARRAY_SIZE(text_format_pov_literals_reserved_data));

/* POV-Ray Built-in Variables
 * list is from...
 * http://www.povray.org/documentation/view/3.7.0/212/
 */

/* Language Keywords */
static const char *text_format_pov_literals_builtins_data[]{
    /* Force single column, sorted list. */
    /* clang-format off */
    "aa_threshold",
    "absorption",
    "agate",
    "akima_spline",
    "all",
    "all_intersections",
    "alpha",
    "ambient",
    "aoi",
    "arc_angle",
    "area_illumination",
    "array",
    "average",
    "b_spline",
    "background",
    "basic_x_spline",
    "bend",
    "bezier_spline",
    "bicubic_patch",
    "binary",
    "black_hole",
    "blob",
    "box",
    "boxed",
    "bozo",
    "brick",
    "brilliance",
    "bump_map",
    "bumps",
    "camera",
    "cells",
    "checker",
    "clock",
    "clock_delta",
    "clock_on",
    "color",
    "color_space",
    "colour",
    "colour_space",
    "component",
    "composite",
    "cone",
    "conic_sweep",
    "coords",
    "crackle",
    "cube",
    "cubic",
    "cubic",
    "cubic_spline",
    "cubic_spline",
    "cutaway_textures",
    "cylinder",
    "cylindrical",
    "density_file",
    "dents",
    "difference",
    "diffuse",
    "disc",
    "displace",
    "dist_exp",
    "emission",
    "extended_x_spline",
    "exterior",
    "facets",
    "falloff_angle",
    "file_gamma",
    "final_clock",
    "final_frame",
    "flatness",
    "flip",
    "fog",
    "frame_number",
    "galley",
    "general_x_spline",
    "global_settings",
    "gradient",
    "granite",
    "height_field",
    "hexagon",
    "hierarchy",
    "hypercomplex",
    "image_height",
    "image_map",
    "image_pattern",
    "image_width",
    "initial_clock",
    "initial_frame",
    "input_file_name",
    "interior",
    "intermerge",
    "internal",
    "intersection",
    "interunion",
    "irid",
    "iridescence",
    "isosurface",
    "julia",
    "julia_fractal",
    "keep",
    "lathe",
    "lemon",
    "leopard",
    "light_group",
    "light_source",
    "linear_spline",
    "linear_sweep",
    "lommel_seeliger",
    "look_at",
    "magnet",
    "major_radius",
    "mandel",
    "marble",
    "masonry",
    "material",
    "max_distance",
    "max_extent",
    "max_iteration",
    "media",
    "merge",
    "mesh",
    "mesh2",
    "metric",
    "minnaert",
    "move",
    "natural_spline",
    "now",
    "object",
    "offset",
    "onion",
    "oren_nayar",
    "orientation",
    "ovus",
    "parametric",
    "pattern",
    "pavement",
    "phong",
    "photons",
    "pigment_pattern",
    "planar",
    "plane",
    "planet",
    "poly",
    "polygon",
    "polynomial",
    "pot",
    "precision",
    "prism",
    "proportion",
    "proximity",
    "quadratic_spline",
    "quadric",
    "quartic",
    "quilted",
    "radial",
    "radiosity",
    "rainbow",
    "reflection",
    "reflection_exponent",
    "refraction",
    "repeat",
    "ripples",
    "roll",
    "scattering",
    "screw",
    "size",
    "sky_sphere",
    "slice",
    "slope",
    "smooth",
    "smooth_triangle",
    "solid",
    "sor",
    "sor_spline",
    "specular",
    "sphere",
    "sphere_sweep",
    "spherical",
    "spiral1",
    "spiral2",
    "spline",
    "spotted",
    "square",
    "subsurface",
    "superellipsoid",
    "t",
    "tcb_spline",
    "text",
    "texture",
    "tile2",
    "tiles",
    "tiling",
    "tolerance",
    "toroidal",
    "torus",
    "triangle",
    "triangular",
    "type",
    "u",
    "union",
    "v",
    "voronoi",
    "water_level",
    "waves",
    "width",
    "wood",
    "wrinkles",
    "x",
    "y",
    "z",
    /* clang-format on */
};
static const Span<const char *> text_format_pov_literals_builtins(
    text_format_pov_literals_builtins_data, ARRAY_SIZE(text_format_pov_literals_builtins_data));

/**
 * POV modifiers.
 * See:
 * http://www.povray.org/documentation/view/3.7.0/212/
 */
static const char *text_format_pov_literals_specialvar_data[]{
    /* Force single column, sorted list. */
    /* clang-format off */
    "aa_level",
    "accuracy",
    "accuracy",
    "adaptive",
    "adc_bailout",
    "agate_turb",
    "aitoff_hammer",
    "albinos",
    "always_sample",
    "ambient_light",
    "amount",
    "aperture",
    "area_light",
    "assumed_gamma",
    "autostop",
    "balthasart",
    "behrmann",
    "blur_samples",
    "bounded_by",
    "brick_size",
    "brightness",
    "bump_size",
    "camera_direction",
    "camera_location",
    "camera_right",
    "camera_type",
    "camera_up",
    "caustics",
    "charset",
    "circular",
    "color_map",
    "colour_map",
    "confidence",
    "contained_by",
    "control0",
    "control1",
    "count",
    "cubic",
    "cubic_wave",
    "density",
    "density_map",
    "dispersion",
    "dispersion_samples",
    "distance",
    "double_illuminate",
    "eccentricity",
    "eckert_iv",
    "eckert_vi",
    "edwards",
    "error_bound",
    "expand_thresholds",
    "exponent",
    "extinction",
    "face_indices",
    "fade_color",
    "fade_colour",
    "fade_distance",
    "fade_power",
    "fade_power",
    "falloff",
    "finish",
    "fisheye",
    "fixed",
    "focal_point",
    "fog_alt",
    "fog_offset",
    "fog_type",
    "frequency",
    "fresnel",
    "gall",
    "gather",
    "global_lights",
    "gray_threshold",
    "hf_gray_16",
    "hobo_dyer",
    "hollow",
    "icosa",
    "importance",
    "inbound",
    "inner",
    "inside_point",
    "inside_vector",
    "interior_texture",
    "interpolate",
    "intervals",
    "ior",
    "irid_wavelength",
    "lambda",
    "lambert_azimuthal",
    "lambert_cylindrical",
    "looks_like",
    "low_error_factor",
    "map_type",
    "material_map",
    "max_gradient",
    "max_sample",
    "max_trace_level",
    "maximal",
    "maximum_reuse",
    "media_attenuation",
    "media_interaction",
    "mercator",
    "mesh_camera",
    "method",
    "miller_cylindrical",
    "minimal",
    "minimum_reuse",
    "mm_per_unit",
    "modulation",
    "mollweide",
    "mortar",
    "nearest_count",
    "no_bump_scale",
    "no_cache",
    "no_image",
    "no_radiosity",
    "no_reflection",
    "no_shadow",
    "noise_generator",
    "normal",
    "normal_indices",
    "normal_map",
    "normal_vectors",
    "number_of_waves",
    "octa",
    "octaves",
    "offset",
    "omega",
    "omni_directional_stereo",
    "omnimax",
    "once",
    "open",
    "orient",
    "origin",
    "original",
    "orthographic",
    "outbound",
    "outside",
    "panoramic",
    "parallaxe",
    "parallel",
    "pass_through",
    "perspective",
    "peters",
    "phase",
    "pigment",
    "pigment_map",
    "plate_carree",
    "point_at",
    "polarity",
    "poly_wave",
    "precision",
    "pretrace_end",
    "pretrace_start",
    "projected_through",
    "quick_color",
    "quick_colour",
    "radius",
    "ramp_wave",
    "ratio",
    "recursion_limit",
    "samples",
    "scallop_wave",
    "sine_wave",
    "slope_map",
    "smyth_craster",
    "spacing",
    "split_union",
    "spotlight",
    "stereo",
    "strength",
    "tetra",
    "texture_list",
    "texture_map",
    "thickness",
    "threshold",
    "tightness",
    "translucency",
    "triangle_wave",
    "turbulence",
    "u_steps",
    "ultra_wide_angle",
    "use_alpha",
    "use_color",
    "use_colour",
    "use_index",
    "uv_indices",
    "uv_mapping",
    "uv_vectors",
    "v_steps",
    "van_der_grinten",
    "variance",
    "vertex_vectors",
    /* clang-format on */
};
static const Span<const char *> text_format_pov_literals_specialvar(
    text_format_pov_literals_specialvar_data,
    ARRAY_SIZE(text_format_pov_literals_specialvar_data));

/** POV Built-in Constants. */
static const char *text_format_pov_literals_bool_data[]{
    /* Force single column, sorted list. */
    /* clang-format off */
    "ascii",
    "bt2020",
    "bt709",
    "df3",
    "exr",
    "false",
    "gif",
    "hdr",
    "iff",
    "jpeg",
    "no",
    "off",
    "on",
    "pgm",
    "pi",
    "png",
    "ppm",
    "sint16be",
    "sint16le",
    "sint32be",
    "sint32le",
    "sint8",
    "sys",
    "tau",
    "tga",
    "tiff",
    "true",
    "ttf",
    "uint16be",
    "uint16le",
    "uint8",
    "unofficial",
    "utf8",
    "yes",
    /* clang-format on */
};
static const Span<const char *> text_format_pov_literals_bool(
    text_format_pov_literals_bool_data, ARRAY_SIZE(text_format_pov_literals_bool_data));

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Functions (for #TextFormatType::format_line)
 * \{ */

/**
 * POV keyword (minus boolean & 'nil').
 * See:
 * http://www.povray.org/documentation/view/3.7.0/212/
 */

static int txtfmt_pov_find_keyword(const char *string)
{

  const int i = text_format_string_literal_find(text_format_pov_literals_keyword, string);

  /* If next source char is an identifier (eg. 'i' in "definite") no match */
  return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static int txtfmt_pov_find_reserved_keywords(const char *string)
{
  const int i = text_format_string_literal_find(text_format_pov_literals_reserved, string);

  /* If next source char is an identifier (eg. 'i' in "definite") no match */
  return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static int txtfmt_pov_find_reserved_builtins(const char *string)
{
  const int i = text_format_string_literal_find(text_format_pov_literals_builtins, string);

  /* If next source char is an identifier (eg. 'i' in "definite") no match */
  return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static int txtfmt_pov_find_specialvar(const char *string)
{
  const int i = text_format_string_literal_find(text_format_pov_literals_specialvar, string);

  /* If next source char is an identifier (eg. 'i' in "definite") no match */
  return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static int txtfmt_pov_find_bool(const char *string)
{
  const int i = text_format_string_literal_find(text_format_pov_literals_bool, string);

  /* If next source char is an identifier (eg. 'i' in "Nonetheless") no match */
  return (i == 0 || text_check_identifier(string[i])) ? -1 : i;
}

static char txtfmt_pov_format_identifier(const char *str)
{
  char fmt;

  /* Keep aligned args for readability. */
  /* clang-format off */

  if        (txtfmt_pov_find_specialvar(str)        != -1) { fmt = FMT_TYPE_SPECIAL;
  } else if (txtfmt_pov_find_keyword(str)           != -1) { fmt = FMT_TYPE_KEYWORD;
  } else if (txtfmt_pov_find_reserved_keywords(str) != -1) { fmt = FMT_TYPE_RESERVED;
  } else if (txtfmt_pov_find_reserved_builtins(str) != -1) { fmt = FMT_TYPE_DIRECTIVE;
  } else                                                   { fmt = FMT_TYPE_DEFAULT;
  }

  /* clang-format on */

  return fmt;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format Line Implementation (#TextFormatType::format_line)
 * \{ */

static void txtfmt_pov_format_line(SpaceText *st, TextLine *line, const bool do_next)
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
      /* C-Style comments */
      if (cont & FMT_CONT_COMMENT_C) {
        if (*str == '*' && *(str + 1) == '/') {
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
      /* C-Style (multi-line) comments */
      if (*str == '/' && *(str + 1) == '*') {
        cont = FMT_CONT_COMMENT_C;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;
      }
      /* Single line comment */
      else if (*str == '/' && *(str + 1) == '/') {
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
      /* Identifiers and other text (no previous white-space/delimiters so text continues). */
      else if (prev == FMT_TYPE_DEFAULT) {
        str += BLI_str_utf8_size_safe(str) - 1;
        *fmt = FMT_TYPE_DEFAULT;
      }
      /* Not white-space, a digit, punctuation, or continuing text.
       * Must be new, check for special words. */
      else {
        /* Keep aligned arguments for readability. */
        /* clang-format off */

        /* Special vars(v) or built-in keywords(b) */
        /* keep in sync with `txtfmt_pov_format_identifier()`. */
        if        ((i = txtfmt_pov_find_specialvar(str))        != -1) { prev = FMT_TYPE_SPECIAL;
        } else if ((i = txtfmt_pov_find_keyword(str))           != -1) { prev = FMT_TYPE_KEYWORD;
        } else if ((i = txtfmt_pov_find_reserved_keywords(str)) != -1) { prev = FMT_TYPE_RESERVED;
        } else if ((i = txtfmt_pov_find_reserved_builtins(str)) != -1) { prev = FMT_TYPE_DIRECTIVE;
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
    txtfmt_pov_format_line(st, line->next, do_next);
  }

  flatten_string_free(&fs);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_text_format_register_pov()
{
  static TextFormatType tft = {nullptr};
  static const char *ext[] = {"pov", "inc", "mcr", "mac", nullptr};

  tft.format_identifier = txtfmt_pov_format_identifier;
  tft.format_line = txtfmt_pov_format_line;
  tft.ext = ext;
  tft.comment_line = "//";

  ED_text_format_register(&tft);

  BLI_assert(text_format_string_literals_check_sorted_array(text_format_pov_literals_keyword));
  BLI_assert(text_format_string_literals_check_sorted_array(text_format_pov_literals_reserved));
  BLI_assert(text_format_string_literals_check_sorted_array(text_format_pov_literals_builtins));
  BLI_assert(text_format_string_literals_check_sorted_array(text_format_pov_literals_specialvar));
  BLI_assert(text_format_string_literals_check_sorted_array(text_format_pov_literals_bool));
}

/** \} */
