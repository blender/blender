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

#include "DNA_brush_types.h"
#include "DNA_collection_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_layer_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_userdef_types.h"
#include "DNA_world_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h" /* TransformOrientation */

#include "IMB_imbuf_types.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_armature.h"
#include "BKE_editmesh.h"
#include "BKE_paint.h"

#include "ED_object.h"
#include "ED_gpencil.h"

#include "GPU_extensions.h"

#include "DRW_engine.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

/* Include for Bake Options */
#include "RE_engine.h"
#include "RE_pipeline.h"

#ifdef WITH_FFMPEG
#  include "BKE_writeffmpeg.h"
#  include <libavcodec/avcodec.h>
#  include <libavformat/avformat.h>
#  include "ffmpeg_compat.h"
#endif

#include "ED_render.h"
#include "ED_transform.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLI_threads.h"

#include "DEG_depsgraph.h"

#ifdef WITH_OPENEXR
const EnumPropertyItem rna_enum_exr_codec_items[] = {
    {R_IMF_EXR_CODEC_NONE, "NONE", 0, "None", ""},
    {R_IMF_EXR_CODEC_PXR24, "PXR24", 0, "Pxr24 (lossy)", ""},
    {R_IMF_EXR_CODEC_ZIP, "ZIP", 0, "ZIP (lossless)", ""},
    {R_IMF_EXR_CODEC_PIZ, "PIZ", 0, "PIZ (lossless)", ""},
    {R_IMF_EXR_CODEC_RLE, "RLE", 0, "RLE (lossless)", ""},
    {R_IMF_EXR_CODEC_ZIPS, "ZIPS", 0, "ZIPS (lossless)", ""},
    {R_IMF_EXR_CODEC_B44, "B44", 0, "B44 (lossy)", ""},
    {R_IMF_EXR_CODEC_B44A, "B44A", 0, "B44A (lossy)", ""},
    {R_IMF_EXR_CODEC_DWAA, "DWAA", 0, "DWAA (lossy)", ""},
    /* NOTE: Commented out for until new OpenEXR is released, see T50673. */
    /* {R_IMF_EXR_CODEC_DWAB, "DWAB", 0, "DWAB (lossy)", ""}, */
    {0, NULL, 0, NULL, NULL},
};
#endif

#ifndef RNA_RUNTIME
static const EnumPropertyItem uv_sculpt_relaxation_items[] = {
    {UV_SCULPT_TOOL_RELAX_LAPLACIAN,
     "LAPLACIAN",
     0,
     "Laplacian",
     "Use Laplacian method for relaxation"},
    {UV_SCULPT_TOOL_RELAX_HC, "HC", 0, "HC", "Use HC method for relaxation"},
    {0, NULL, 0, NULL, NULL},
};
#endif

const EnumPropertyItem rna_enum_snap_target_items[] = {
    {SCE_SNAP_TARGET_CLOSEST, "CLOSEST", 0, "Closest", "Snap closest point onto target"},
    {SCE_SNAP_TARGET_CENTER, "CENTER", 0, "Center", "Snap transformation center onto target"},
    {SCE_SNAP_TARGET_MEDIAN, "MEDIAN", 0, "Median", "Snap median onto target"},
    {SCE_SNAP_TARGET_ACTIVE, "ACTIVE", 0, "Active", "Snap active onto target"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_proportional_falloff_items[] = {
    {PROP_SMOOTH, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", "Smooth falloff"},
    {PROP_SPHERE, "SPHERE", ICON_SPHERECURVE, "Sphere", "Spherical falloff"},
    {PROP_ROOT, "ROOT", ICON_ROOTCURVE, "Root", "Root falloff"},
    {PROP_INVSQUARE,
     "INVERSE_SQUARE",
     ICON_INVERSESQUARECURVE,
     "Inverse Square",
     "Inverse Square falloff"},
    {PROP_SHARP, "SHARP", ICON_SHARPCURVE, "Sharp", "Sharp falloff"},
    {PROP_LIN, "LINEAR", ICON_LINCURVE, "Linear", "Linear falloff"},
    {PROP_CONST, "CONSTANT", ICON_NOCURVE, "Constant", "Constant falloff"},
    {PROP_RANDOM, "RANDOM", ICON_RNDCURVE, "Random", "Random falloff"},
    {0, NULL, 0, NULL, NULL},
};

/* subset of the enum - only curves, missing random and const */
const EnumPropertyItem rna_enum_proportional_falloff_curve_only_items[] = {
    {PROP_SMOOTH, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", "Smooth falloff"},
    {PROP_SPHERE, "SPHERE", ICON_SPHERECURVE, "Sphere", "Spherical falloff"},
    {PROP_ROOT, "ROOT", ICON_ROOTCURVE, "Root", "Root falloff"},
    {PROP_INVSQUARE, "INVERSE_SQUARE", ICON_ROOTCURVE, "Inverse Square", "Inverse Square falloff"},
    {PROP_SHARP, "SHARP", ICON_SHARPCURVE, "Sharp", "Sharp falloff"},
    {PROP_LIN, "LINEAR", ICON_LINCURVE, "Linear", "Linear falloff"},
    {0, NULL, 0, NULL, NULL},
};

/* keep for operators, not used here */
const EnumPropertyItem rna_enum_mesh_select_mode_items[] = {
    {SCE_SELECT_VERTEX, "VERTEX", ICON_VERTEXSEL, "Vertex", "Vertex selection mode"},
    {SCE_SELECT_EDGE, "EDGE", ICON_EDGESEL, "Edge", "Edge selection mode"},
    {SCE_SELECT_FACE, "FACE", ICON_FACESEL, "Face", "Face selection mode"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_mesh_select_mode_uv_items[] = {
    {UV_SELECT_VERTEX, "VERTEX", ICON_UV_VERTEXSEL, "Vertex", "Vertex selection mode"},
    {UV_SELECT_EDGE, "EDGE", ICON_UV_EDGESEL, "Edge", "Edge selection mode"},
    {UV_SELECT_FACE, "FACE", ICON_UV_FACESEL, "Face", "Face selection mode"},
    {UV_SELECT_ISLAND, "ISLAND", ICON_UV_ISLANDSEL, "Island", "Island selection mode"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_snap_element_items[] = {
    {SCE_SNAP_MODE_INCREMENT,
     "INCREMENT",
     ICON_SNAP_INCREMENT,
     "Increment",
     "Snap to increments of grid"},
    {SCE_SNAP_MODE_VERTEX, "VERTEX", ICON_SNAP_VERTEX, "Vertex", "Snap to vertices"},
    {SCE_SNAP_MODE_EDGE, "EDGE", ICON_SNAP_EDGE, "Edge", "Snap to edges"},
    {SCE_SNAP_MODE_FACE, "FACE", ICON_SNAP_FACE, "Face", "Snap to faces"},
    {SCE_SNAP_MODE_VOLUME, "VOLUME", ICON_SNAP_VOLUME, "Volume", "Snap to volume"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_snap_node_element_items[] = {
    {SCE_SNAP_MODE_GRID, "GRID", ICON_SNAP_GRID, "Grid", "Snap to grid"},
    {SCE_SNAP_MODE_NODE_X, "NODE_X", ICON_NODE_SIDE, "Node X", "Snap to left/right node border"},
    {SCE_SNAP_MODE_NODE_Y, "NODE_Y", ICON_NODE_TOP, "Node Y", "Snap to top/bottom node border"},
    {SCE_SNAP_MODE_NODE_X | SCE_SNAP_MODE_NODE_Y,
     "NODE_XY",
     ICON_NODE_CORNER,
     "Node X / Y",
     "Snap to any node border"},
    {0, NULL, 0, NULL, NULL},
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem snap_uv_element_items[] = {
    {SCE_SNAP_MODE_INCREMENT,
     "INCREMENT",
     ICON_SNAP_INCREMENT,
     "Increment",
     "Snap to increments of grid"},
    {SCE_SNAP_MODE_VERTEX, "VERTEX", ICON_SNAP_VERTEX, "Vertex", "Snap to vertices"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_scene_display_aa_methods[] = {
    {SCE_DISPLAY_AA_OFF,
     "OFF",
     0,
     "No Anti-Aliasing",
     "Scene will be rendering without any anti-aliasing"},
    {SCE_DISPLAY_AA_FXAA,
     "FXAA",
     0,
     "Single Pass Anti-Aliasing",
     "Scene will be rendered using a single pass anti-aliasing method (FXAA)"},
    {SCE_DISPLAY_AA_SAMPLES_5,
     "5",
     0,
     "5 Samples",
     "Scene will be rendered using 5 anti-aliasing samples"},
    {SCE_DISPLAY_AA_SAMPLES_8,
     "8",
     0,
     "8 Samples",
     "Scene will be rendered using 8 anti-aliasing samples"},
    {SCE_DISPLAY_AA_SAMPLES_11,
     "11",
     0,
     "11 Samples",
     "Scene will be rendered using 11 anti-aliasing samples"},
    {SCE_DISPLAY_AA_SAMPLES_16,
     "16",
     0,
     "16 Samples",
     "Scene will be rendered using 16 anti-aliasing samples"},
    {SCE_DISPLAY_AA_SAMPLES_32,
     "32",
     0,
     "32 Samples",
     "Scene will be rendered using 32 anti-aliasing samples"},
    {0, NULL, 0, NULL, NULL},
};
#endif

const EnumPropertyItem rna_enum_curve_fit_method_items[] = {
    {CURVE_PAINT_FIT_METHOD_REFIT,
     "REFIT",
     0,
     "Refit",
     "Incrementally re-fit the curve (high quality)"},
    {CURVE_PAINT_FIT_METHOD_SPLIT,
     "SPLIT",
     0,
     "Split",
     "Split the curve until the tolerance is met (fast)"},
    {0, NULL, 0, NULL, NULL},
};

/* workaround for duplicate enums,
 * have each enum line as a define then conditionally set it or not
 */

#define R_IMF_ENUM_BMP \
  {R_IMF_IMTYPE_BMP, "BMP", ICON_FILE_IMAGE, "BMP", "Output image in bitmap format"},
#define R_IMF_ENUM_IRIS \
  {R_IMF_IMTYPE_IRIS, "IRIS", ICON_FILE_IMAGE, "Iris", "Output image in (old!) SGI IRIS format"},
#define R_IMF_ENUM_PNG \
  {R_IMF_IMTYPE_PNG, "PNG", ICON_FILE_IMAGE, "PNG", "Output image in PNG format"},
#define R_IMF_ENUM_JPEG \
  {R_IMF_IMTYPE_JPEG90, "JPEG", ICON_FILE_IMAGE, "JPEG", "Output image in JPEG format"},
#define R_IMF_ENUM_TAGA \
  {R_IMF_IMTYPE_TARGA, "TARGA", ICON_FILE_IMAGE, "Targa", "Output image in Targa format"},
#define R_IMF_ENUM_TAGA_RAW \
  {R_IMF_IMTYPE_RAWTGA, \
   "TARGA_RAW", \
   ICON_FILE_IMAGE, \
   "Targa Raw", \
   "Output image in uncompressed Targa format"},

#if 0 /* UNUSED (so far) */
#  ifdef WITH_DDS
#    define R_IMF_ENUM_DDS \
      {R_IMF_IMTYPE_DDS, "DDS", ICON_FILE_IMAGE, "DDS", "Output image in DDS format"},
#  else
#    define R_IMF_ENUM_DDS
#  endif
#endif

#ifdef WITH_OPENJPEG
#  define R_IMF_ENUM_JPEG2K \
    {R_IMF_IMTYPE_JP2, \
     "JPEG2000", \
     ICON_FILE_IMAGE, \
     "JPEG 2000", \
     "Output image in JPEG 2000 format"},
#else
#  define R_IMF_ENUM_JPEG2K
#endif

#ifdef WITH_CINEON
#  define R_IMF_ENUM_CINEON \
    {R_IMF_IMTYPE_CINEON, "CINEON", ICON_FILE_IMAGE, "Cineon", "Output image in Cineon format"},
#  define R_IMF_ENUM_DPX \
    {R_IMF_IMTYPE_DPX, "DPX", ICON_FILE_IMAGE, "DPX", "Output image in DPX format"},
#else
#  define R_IMF_ENUM_CINEON
#  define R_IMF_ENUM_DPX
#endif

#ifdef WITH_OPENEXR
#  define R_IMF_ENUM_EXR_MULTILAYER \
    {R_IMF_IMTYPE_MULTILAYER, \
     "OPEN_EXR_MULTILAYER", \
     ICON_FILE_IMAGE, \
     "OpenEXR MultiLayer", \
     "Output image in multilayer OpenEXR format"},
#  define R_IMF_ENUM_EXR \
    {R_IMF_IMTYPE_OPENEXR, \
     "OPEN_EXR", \
     ICON_FILE_IMAGE, \
     "OpenEXR", \
     "Output image in OpenEXR format"},
#else
#  define R_IMF_ENUM_EXR_MULTILAYER
#  define R_IMF_ENUM_EXR
#endif

#ifdef WITH_HDR
#  define R_IMF_ENUM_HDR \
    {R_IMF_IMTYPE_RADHDR, \
     "HDR", \
     ICON_FILE_IMAGE, \
     "Radiance HDR", \
     "Output image in Radiance HDR format"},
#else
#  define R_IMF_ENUM_HDR
#endif

#ifdef WITH_TIFF
#  define R_IMF_ENUM_TIFF \
    {R_IMF_IMTYPE_TIFF, "TIFF", ICON_FILE_IMAGE, "TIFF", "Output image in TIFF format"},
#else
#  define R_IMF_ENUM_TIFF
#endif

#define IMAGE_TYPE_ITEMS_IMAGE_ONLY \
  R_IMF_ENUM_BMP \
  /* DDS save not supported yet R_IMF_ENUM_DDS */ \
  R_IMF_ENUM_IRIS \
  R_IMF_ENUM_PNG \
  R_IMF_ENUM_JPEG \
  R_IMF_ENUM_JPEG2K \
  R_IMF_ENUM_TAGA \
  R_IMF_ENUM_TAGA_RAW{0, "", 0, " ", NULL}, \
      R_IMF_ENUM_CINEON R_IMF_ENUM_DPX R_IMF_ENUM_EXR_MULTILAYER R_IMF_ENUM_EXR R_IMF_ENUM_HDR \
          R_IMF_ENUM_TIFF

#ifdef RNA_RUNTIME
static const EnumPropertyItem image_only_type_items[] = {

    IMAGE_TYPE_ITEMS_IMAGE_ONLY

    {0, NULL, 0, NULL, NULL},
};
#endif

const EnumPropertyItem rna_enum_image_type_items[] = {
    {0, "", 0, N_("Image"), NULL},

    IMAGE_TYPE_ITEMS_IMAGE_ONLY

    {0, "", 0, N_("Movie"), NULL},
    {R_IMF_IMTYPE_AVIJPEG,
     "AVI_JPEG",
     ICON_FILE_MOVIE,
     "AVI JPEG",
     "Output video in AVI JPEG format"},
    {R_IMF_IMTYPE_AVIRAW, "AVI_RAW", ICON_FILE_MOVIE, "AVI Raw", "Output video in AVI Raw format"},
#ifdef WITH_FFMPEG
    {R_IMF_IMTYPE_FFMPEG,
     "FFMPEG",
     ICON_FILE_MOVIE,
     "FFmpeg video",
     "The most versatile way to output video files"},
#endif
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_image_color_mode_items[] = {
    {R_IMF_PLANES_BW,
     "BW",
     0,
     "BW",
     "Images get saved in 8 bits grayscale (only PNG, JPEG, TGA, TIF)"},
    {R_IMF_PLANES_RGB, "RGB", 0, "RGB", "Images are saved with RGB (color) data"},
    {R_IMF_PLANES_RGBA,
     "RGBA",
     0,
     "RGBA",
     "Images are saved with RGB and Alpha data (if supported)"},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME
#  define IMAGE_COLOR_MODE_BW rna_enum_image_color_mode_items[0]
#  define IMAGE_COLOR_MODE_RGB rna_enum_image_color_mode_items[1]
#  define IMAGE_COLOR_MODE_RGBA rna_enum_image_color_mode_items[2]
#endif

const EnumPropertyItem rna_enum_image_color_depth_items[] = {
    /* 1 (monochrome) not used */
    {R_IMF_CHAN_DEPTH_8, "8", 0, "8", "8 bit color channels"},
    {R_IMF_CHAN_DEPTH_10, "10", 0, "10", "10 bit color channels"},
    {R_IMF_CHAN_DEPTH_12, "12", 0, "12", "12 bit color channels"},
    {R_IMF_CHAN_DEPTH_16, "16", 0, "16", "16 bit color channels"},
    /* 24 not used */
    {R_IMF_CHAN_DEPTH_32, "32", 0, "32", "32 bit color channels"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_normal_space_items[] = {
    {R_BAKE_SPACE_OBJECT, "OBJECT", 0, "Object", "Bake the normals in object space"},
    {R_BAKE_SPACE_TANGENT, "TANGENT", 0, "Tangent", "Bake the normals in tangent space"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_normal_swizzle_items[] = {
    {R_BAKE_POSX, "POS_X", 0, "+X", ""},
    {R_BAKE_POSY, "POS_Y", 0, "+Y", ""},
    {R_BAKE_POSZ, "POS_Z", 0, "+Z", ""},
    {R_BAKE_NEGX, "NEG_X", 0, "-X", ""},
    {R_BAKE_NEGY, "NEG_Y", 0, "-Y", ""},
    {R_BAKE_NEGZ, "NEG_Z", 0, "-Z", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_bake_save_mode_items[] = {
    {R_BAKE_SAVE_INTERNAL,
     "INTERNAL",
     0,
     "Internal",
     "Save the baking map in an internal image data-block"},
    {R_BAKE_SAVE_EXTERNAL, "EXTERNAL", 0, "External", "Save the baking map in an external file"},
    {0, NULL, 0, NULL, NULL},
};

#define R_IMF_VIEWS_ENUM_IND \
  {R_IMF_VIEWS_INDIVIDUAL, \
   "INDIVIDUAL", \
   0, \
   "Individual", \
   "Individual files for each view with the prefix as defined by the scene views"},
#define R_IMF_VIEWS_ENUM_S3D \
  {R_IMF_VIEWS_STEREO_3D, "STEREO_3D", 0, "Stereo 3D", "Single file with an encoded stereo pair"},
#define R_IMF_VIEWS_ENUM_MV \
  {R_IMF_VIEWS_MULTIVIEW, "MULTIVIEW", 0, "Multi-View", "Single file with all the views"},

const EnumPropertyItem rna_enum_views_format_items[] = {
    R_IMF_VIEWS_ENUM_IND R_IMF_VIEWS_ENUM_S3D{0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_views_format_multilayer_items[] = {
    R_IMF_VIEWS_ENUM_IND R_IMF_VIEWS_ENUM_MV{0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_views_format_multiview_items[] = {
    R_IMF_VIEWS_ENUM_IND R_IMF_VIEWS_ENUM_S3D R_IMF_VIEWS_ENUM_MV{0, NULL, 0, NULL, NULL},
};

#undef R_IMF_VIEWS_ENUM_IND
#undef R_IMF_VIEWS_ENUM_S3D
#undef R_IMF_VIEWS_ENUM_MV

const EnumPropertyItem rna_enum_stereo3d_display_items[] = {
    {S3D_DISPLAY_ANAGLYPH,
     "ANAGLYPH",
     0,
     "Anaglyph",
     "Render views for left and right eyes as two differently filtered colors in a single image "
     "(anaglyph glasses are required)"},
    {S3D_DISPLAY_INTERLACE,
     "INTERLACE",
     0,
     "Interlace",
     "Render views for left and right eyes interlaced in a single image (3D-ready monitor is "
     "required)"},
    {S3D_DISPLAY_PAGEFLIP,
     "TIMESEQUENTIAL",
     0,
     "Time Sequential",
     "Render alternate eyes (also known as page flip, quad buffer support in the graphic card is "
     "required)"},
    {S3D_DISPLAY_SIDEBYSIDE,
     "SIDEBYSIDE",
     0,
     "Side-by-Side",
     "Render views for left and right eyes side-by-side"},
    {S3D_DISPLAY_TOPBOTTOM,
     "TOPBOTTOM",
     0,
     "Top-Bottom",
     "Render views for left and right eyes one above another"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_stereo3d_anaglyph_type_items[] = {
    {S3D_ANAGLYPH_REDCYAN, "RED_CYAN", 0, "Red-Cyan", ""},
    {S3D_ANAGLYPH_GREENMAGENTA, "GREEN_MAGENTA", 0, "Green-Magenta", ""},
    {S3D_ANAGLYPH_YELLOWBLUE, "YELLOW_BLUE", 0, "Yellow-Blue", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_stereo3d_interlace_type_items[] = {
    {S3D_INTERLACE_ROW, "ROW_INTERLEAVED", 0, "Row Interleaved", ""},
    {S3D_INTERLACE_COLUMN, "COLUMN_INTERLEAVED", 0, "Column Interleaved", ""},
    {S3D_INTERLACE_CHECKERBOARD, "CHECKERBOARD_INTERLEAVED", 0, "Checkerboard Interleaved", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_bake_pass_filter_type_items[] = {
    {R_BAKE_PASS_FILTER_NONE, "NONE", 0, "None", ""},
    {R_BAKE_PASS_FILTER_AO, "AO", 0, "Ambient Occlusion", ""},
    {R_BAKE_PASS_FILTER_EMIT, "EMIT", 0, "Emit", ""},
    {R_BAKE_PASS_FILTER_DIRECT, "DIRECT", 0, "Direct", ""},
    {R_BAKE_PASS_FILTER_INDIRECT, "INDIRECT", 0, "Indirect", ""},
    {R_BAKE_PASS_FILTER_COLOR, "COLOR", 0, "Color", ""},
    {R_BAKE_PASS_FILTER_DIFFUSE, "DIFFUSE", 0, "Diffuse", ""},
    {R_BAKE_PASS_FILTER_GLOSSY, "GLOSSY", 0, "Glossy", ""},
    {R_BAKE_PASS_FILTER_TRANSM, "TRANSMISSION", 0, "Transmission", ""},
    {R_BAKE_PASS_FILTER_SUBSURFACE, "SUBSURFACE", 0, "Subsurface", ""},
    {0, NULL, 0, NULL, NULL},
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem rna_enum_gpencil_interpolation_mode_items[] = {
    /* interpolation */
    {0, "", 0, N_("Interpolation"), "Standard transitions between keyframes"},
    {GP_IPO_LINEAR,
     "LINEAR",
     ICON_IPO_LINEAR,
     "Linear",
     "Straight-line interpolation between A and B (i.e. no ease in/out)"},
    {GP_IPO_CURVEMAP,
     "CUSTOM",
     ICON_IPO_BEZIER,
     "Custom",
     "Custom interpolation defined using a curve map"},

    /* easing */
    {0,
     "",
     0,
     N_("Easing (by strength)"),
     "Predefined inertial transitions, useful for motion graphics (from least to most "
     "''dramatic'')"},
    {GP_IPO_SINE,
     "SINE",
     ICON_IPO_SINE,
     "Sinusoidal",
     "Sinusoidal easing (weakest, almost linear but with a slight curvature)"},
    {GP_IPO_QUAD, "QUAD", ICON_IPO_QUAD, "Quadratic", "Quadratic easing"},
    {GP_IPO_CUBIC, "CUBIC", ICON_IPO_CUBIC, "Cubic", "Cubic easing"},
    {GP_IPO_QUART, "QUART", ICON_IPO_QUART, "Quartic", "Quartic easing"},
    {GP_IPO_QUINT, "QUINT", ICON_IPO_QUINT, "Quintic", "Quintic easing"},
    {GP_IPO_EXPO, "EXPO", ICON_IPO_EXPO, "Exponential", "Exponential easing (dramatic)"},
    {GP_IPO_CIRC,
     "CIRC",
     ICON_IPO_CIRC,
     "Circular",
     "Circular easing (strongest and most dynamic)"},

    {0, "", 0, N_("Dynamic Effects"), "Simple physics-inspired easing effects"},
    {GP_IPO_BACK, "BACK", ICON_IPO_BACK, "Back", "Cubic easing with overshoot and settle"},
    {GP_IPO_BOUNCE,
     "BOUNCE",
     ICON_IPO_BOUNCE,
     "Bounce",
     "Exponentially decaying parabolic bounce, like when objects collide"},
    {GP_IPO_ELASTIC,
     "ELASTIC",
     ICON_IPO_ELASTIC,
     "Elastic",
     "Exponentially decaying sine wave, like an elastic band"},

    {0, NULL, 0, NULL, NULL},
};

#endif

const EnumPropertyItem rna_enum_transform_pivot_items_full[] = {
    {V3D_AROUND_CENTER_BOUNDS,
     "BOUNDING_BOX_CENTER",
     ICON_PIVOT_BOUNDBOX,
     "Bounding Box Center",
     "Pivot around bounding box center of selected object(s)"},
    {V3D_AROUND_CURSOR, "CURSOR", ICON_PIVOT_CURSOR, "3D Cursor", "Pivot around the 3D cursor"},
    {V3D_AROUND_LOCAL_ORIGINS,
     "INDIVIDUAL_ORIGINS",
     ICON_PIVOT_INDIVIDUAL,
     "Individual Origins",
     "Pivot around each object's own origin"},
    {V3D_AROUND_CENTER_MEDIAN,
     "MEDIAN_POINT",
     ICON_PIVOT_MEDIAN,
     "Median Point",
     "Pivot around the median point of selected objects"},
    {V3D_AROUND_ACTIVE,
     "ACTIVE_ELEMENT",
     ICON_PIVOT_ACTIVE,
     "Active Element",
     "Pivot around active object"},
    {0, NULL, 0, NULL, NULL},
};

/* Icons could be made a consistent set of images. */
const EnumPropertyItem rna_enum_transform_orientation_items[] = {
    {V3D_ORIENT_GLOBAL,
     "GLOBAL",
     ICON_ORIENTATION_GLOBAL,
     "Global",
     "Align the transformation axes to world space"},
    {V3D_ORIENT_LOCAL,
     "LOCAL",
     ICON_ORIENTATION_LOCAL,
     "Local",
     "Align the transformation axes to the selected objects' local space"},
    {V3D_ORIENT_NORMAL,
     "NORMAL",
     ICON_ORIENTATION_NORMAL,
     "Normal",
     "Align the transformation axes to average normal of selected elements "
     "(bone Y axis for pose mode)"},
    {V3D_ORIENT_GIMBAL,
     "GIMBAL",
     ICON_ORIENTATION_GIMBAL,
     "Gimbal",
     "Align each axis to the Euler rotation axis as used for input"},
    {V3D_ORIENT_VIEW,
     "VIEW",
     ICON_ORIENTATION_VIEW,
     "View",
     "Align the transformation axes to the window"},
    {V3D_ORIENT_CURSOR,
     "CURSOR",
     ICON_ORIENTATION_CURSOR,
     "Cursor",
     "Align the transformation axes to the 3D cursor"},
    // {V3D_ORIENT_CUSTOM, "CUSTOM", 0, "Custom", "Use a custom transform orientation"},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "BLI_string_utils.h"

#  include "DNA_anim_types.h"
#  include "DNA_color_types.h"
#  include "DNA_node_types.h"
#  include "DNA_object_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_text_types.h"
#  include "DNA_workspace_types.h"

#  include "RNA_access.h"

#  include "MEM_guardedalloc.h"

#  include "BKE_brush.h"
#  include "BKE_collection.h"
#  include "BKE_colortools.h"
#  include "BKE_context.h"
#  include "BKE_global.h"
#  include "BKE_idprop.h"
#  include "BKE_image.h"
#  include "BKE_layer.h"
#  include "BKE_main.h"
#  include "BKE_node.h"
#  include "BKE_pointcache.h"
#  include "BKE_scene.h"
#  include "BKE_mesh.h"
#  include "BKE_screen.h"
#  include "BKE_sequencer.h"
#  include "BKE_animsys.h"
#  include "BKE_freestyle.h"
#  include "BKE_gpencil.h"
#  include "BKE_unit.h"

#  include "ED_info.h"
#  include "ED_node.h"
#  include "ED_view3d.h"
#  include "ED_mesh.h"
#  include "ED_keyframing.h"
#  include "ED_image.h"
#  include "ED_scene.h"

#  include "DEG_depsgraph_build.h"
#  include "DEG_depsgraph_query.h"

#  ifdef WITH_FREESTYLE
#    include "FRS_freestyle.h"
#  endif

static void rna_ToolSettings_snap_mode_set(struct PointerRNA *ptr, int value)
{
  ToolSettings *ts = (ToolSettings *)ptr->data;
  if (value != 0) {
    ts->snap_mode = value;
  }
}

/* Grease Pencil update cache */
static void rna_GPencil_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
  /* mark all grease pencil datablocks of the scene */
  FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, ob) {
      if (ob->type == OB_GPENCIL) {
        bGPdata *gpd = (bGPdata *)ob->data;
        gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
        DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
  FOREACH_SCENE_COLLECTION_END;

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
}

/* Grease Pencil Interpolation settings */
static char *rna_GPencilInterpolateSettings_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("tool_settings.gpencil_interpolate");
}

static void rna_GPencilInterpolateSettings_type_set(PointerRNA *ptr, int value)
{
  GP_Interpolate_Settings *settings = (GP_Interpolate_Settings *)ptr->data;

  /* NOTE: This cast should be fine, as we have a small + finite set of values
   * (#eGP_Interpolate_Type) that should fit well within a char.
   */
  settings->type = (char)value;

  /* init custom interpolation curve here now the first time it's used */
  if ((settings->type == GP_IPO_CURVEMAP) && (settings->custom_ipo == NULL)) {
    settings->custom_ipo = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  }
}

/* Read-only Iterator of all the scene objects. */

static void rna_Scene_objects_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;
  iter->internal.custom = MEM_callocN(sizeof(BLI_Iterator), __func__);

  ((BLI_Iterator *)iter->internal.custom)->valid = true;
  BKE_scene_objects_iterator_begin(iter->internal.custom, (void *)scene);
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Scene_objects_next(CollectionPropertyIterator *iter)
{
  BKE_scene_objects_iterator_next(iter->internal.custom);
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Scene_objects_end(CollectionPropertyIterator *iter)
{
  BKE_scene_objects_iterator_end(iter->internal.custom);
  MEM_freeN(iter->internal.custom);
}

static PointerRNA rna_Scene_objects_get(CollectionPropertyIterator *iter)
{
  Object *ob = ((BLI_Iterator *)iter->internal.custom)->current;
  return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, ob);
}

/* End of read-only Iterator of all the scene objects. */

static void rna_Scene_set_set(PointerRNA *ptr,
                              PointerRNA value,
                              struct ReportList *UNUSED(reports))
{
  Scene *scene = (Scene *)ptr->data;
  Scene *set = (Scene *)value.data;
  Scene *nested_set;

  for (nested_set = set; nested_set; nested_set = nested_set->set) {
    if (nested_set == scene) {
      return;
    }
    /* prevent eternal loops, set can point to next, and next to set, without problems usually */
    if (nested_set->set == set) {
      return;
    }
  }

  id_lib_extern((ID *)set);
  scene->set = set;
}

void rna_Scene_set_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update_ex(bmain, &scene->id, 0);
  if (scene->set != NULL) {
    /* Objects which are pulled into main scene's depsgraph needs to have
     * their base flags updated.
     */
    DEG_id_tag_update_ex(bmain, &scene->set->id, 0);
  }
}

static void rna_Scene_camera_update(Main *bmain, Scene *UNUSED(scene_unused), PointerRNA *ptr)
{
  wmWindowManager *wm = bmain->wm.first;
  Scene *scene = (Scene *)ptr->data;

  WM_windows_scene_data_sync(&wm->windows, scene);
  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);
}

static void rna_Scene_fps_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_FPS | ID_RECALC_SEQUENCER_STRIPS);
}

static void rna_Scene_listener_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_LISTENER);
}

static void rna_Scene_volume_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_VOLUME);
}

static const char *rna_Scene_statistics_string_get(Scene *scene,
                                                   Main *bmain,
                                                   ViewLayer *view_layer)
{
  return ED_info_stats_string(bmain, scene, view_layer);
}

static void rna_Scene_framelen_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
  scene->r.framelen = (float)scene->r.framapto / (float)scene->r.images;
}

static void rna_Scene_frame_current_set(PointerRNA *ptr, int value)
{
  Scene *data = (Scene *)ptr->data;

  /* if negative frames aren't allowed, then we can't use them */
  FRAMENUMBER_MIN_CLAMP(value);
  data->r.cfra = value;
}

static float rna_Scene_frame_float_get(PointerRNA *ptr)
{
  Scene *data = (Scene *)ptr->data;
  return (float)data->r.cfra + data->r.subframe;
}

static void rna_Scene_frame_float_set(PointerRNA *ptr, float value)
{
  Scene *data = (Scene *)ptr->data;
  /* if negative frames aren't allowed, then we can't use them */
  FRAMENUMBER_MIN_CLAMP(value);
  data->r.cfra = (int)value;
  data->r.subframe = value - data->r.cfra;
}

static float rna_Scene_frame_current_final_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;

  return BKE_scene_frame_get_from_ctime(scene, (float)scene->r.cfra);
}

static void rna_Scene_start_frame_set(PointerRNA *ptr, int value)
{
  Scene *data = (Scene *)ptr->data;
  /* MINFRAME not MINAFRAME, since some output formats can't taken negative frames */
  CLAMP(value, MINFRAME, MAXFRAME);
  data->r.sfra = value;

  if (data->r.sfra >= data->r.efra) {
    data->r.efra = MIN2(data->r.sfra, MAXFRAME);
  }
}

static void rna_Scene_end_frame_set(PointerRNA *ptr, int value)
{
  Scene *data = (Scene *)ptr->data;
  CLAMP(value, MINFRAME, MAXFRAME);
  data->r.efra = value;

  if (data->r.sfra >= data->r.efra) {
    data->r.sfra = MAX2(data->r.efra, MINFRAME);
  }
}

static void rna_Scene_use_preview_range_set(PointerRNA *ptr, bool value)
{
  Scene *data = (Scene *)ptr->data;

  if (value) {
    /* copy range from scene if not set before */
    if ((data->r.psfra == data->r.pefra) && (data->r.psfra == 0)) {
      data->r.psfra = data->r.sfra;
      data->r.pefra = data->r.efra;
    }

    data->r.flag |= SCER_PRV_RANGE;
  }
  else {
    data->r.flag &= ~SCER_PRV_RANGE;
  }
}

static void rna_Scene_preview_range_start_frame_set(PointerRNA *ptr, int value)
{
  Scene *data = (Scene *)ptr->data;

  /* check if enabled already */
  if ((data->r.flag & SCER_PRV_RANGE) == 0) {
    /* set end of preview range to end frame, then clamp as per normal */
    /* TODO: or just refuse to set instead? */
    data->r.pefra = data->r.efra;
  }

  /* now set normally */
  CLAMP(value, MINAFRAME, data->r.pefra);
  data->r.psfra = value;
}

static void rna_Scene_preview_range_end_frame_set(PointerRNA *ptr, int value)
{
  Scene *data = (Scene *)ptr->data;

  /* check if enabled already */
  if ((data->r.flag & SCER_PRV_RANGE) == 0) {
    /* set start of preview range to start frame, then clamp as per normal */
    /* TODO: or just refuse to set instead? */
    data->r.psfra = data->r.sfra;
  }

  /* now set normally */
  CLAMP(value, data->r.psfra, MAXFRAME);
  data->r.pefra = value;
}

static void rna_Scene_show_subframe_update(Main *UNUSED(bmain),
                                           Scene *UNUSED(current_scene),
                                           PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  scene->r.subframe = 0.0f;
}

static void rna_Scene_frame_update(Main *UNUSED(bmain),
                                   Scene *UNUSED(current_scene),
                                   PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_SEEK);
  WM_main_add_notifier(NC_SCENE | ND_FRAME, scene);
}

static PointerRNA rna_Scene_active_keying_set_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;
  return rna_pointer_inherit_refine(ptr, &RNA_KeyingSet, ANIM_scene_get_active_keyingset(scene));
}

static void rna_Scene_active_keying_set_set(PointerRNA *ptr,
                                            PointerRNA value,
                                            struct ReportList *UNUSED(reports))
{
  Scene *scene = (Scene *)ptr->data;
  KeyingSet *ks = (KeyingSet *)value.data;

  scene->active_keyingset = ANIM_scene_get_keyingset_index(scene, ks);
}

/* get KeyingSet index stuff for list of Keying Sets editing UI
 * - active_keyingset-1 since 0 is reserved for 'none'
 * - don't clamp, otherwise can never set builtins types as active...
 */
static int rna_Scene_active_keying_set_index_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;
  return scene->active_keyingset - 1;
}

/* get KeyingSet index stuff for list of Keying Sets editing UI
 * - value+1 since 0 is reserved for 'none'
 */
static void rna_Scene_active_keying_set_index_set(PointerRNA *ptr, int value)
{
  Scene *scene = (Scene *)ptr->data;
  scene->active_keyingset = value + 1;
}

/* XXX: evil... builtin_keyingsets is defined in keyingsets.c! */
/* TODO: make API function to retrieve this... */
extern ListBase builtin_keyingsets;

static void rna_Scene_all_keyingsets_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;

  /* start going over the scene KeyingSets first, while we still have pointer to it
   * but only if we have any Keying Sets to use...
   */
  if (scene->keyingsets.first) {
    rna_iterator_listbase_begin(iter, &scene->keyingsets, NULL);
  }
  else {
    rna_iterator_listbase_begin(iter, &builtin_keyingsets, NULL);
  }
}

static void rna_Scene_all_keyingsets_next(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  KeyingSet *ks = (KeyingSet *)internal->link;

  /* If we've run out of links in Scene list,
   * jump over to the builtins list unless we're there already. */
  if ((ks->next == NULL) && (ks != builtin_keyingsets.last)) {
    internal->link = (Link *)builtin_keyingsets.first;
  }
  else {
    internal->link = (Link *)ks->next;
  }

  iter->valid = (internal->link != NULL);
}

static char *rna_SceneEEVEE_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("eevee");
}

static int rna_RenderSettings_stereoViews_skip(CollectionPropertyIterator *iter,
                                               void *UNUSED(data))
{
  ListBaseIterator *internal = &iter->internal.listbase;
  SceneRenderView *srv = (SceneRenderView *)internal->link;

  if ((STREQ(srv->name, STEREO_LEFT_NAME)) || (STREQ(srv->name, STEREO_RIGHT_NAME))) {
    return 0;
  }

  return 1;
};

static void rna_RenderSettings_stereoViews_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  RenderData *rd = (RenderData *)ptr->data;
  rna_iterator_listbase_begin(iter, &rd->views, rna_RenderSettings_stereoViews_skip);
}

static char *rna_RenderSettings_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("render");
}

static char *rna_BakeSettings_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("render.bake");
}

static char *rna_ImageFormatSettings_path(PointerRNA *ptr)
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;
  ID *id = ptr->id.data;

  switch (GS(id->name)) {
    case ID_SCE: {
      Scene *scene = (Scene *)id;

      if (&scene->r.im_format == imf) {
        return BLI_strdup("render.image_settings");
      }
      else if (&scene->r.bake.im_format == imf) {
        return BLI_strdup("render.bake.image_settings");
      }
      return BLI_strdup("..");
    }
    case ID_NT: {
      bNodeTree *ntree = (bNodeTree *)id;
      bNode *node;

      for (node = ntree->nodes.first; node; node = node->next) {
        if (node->type == CMP_NODE_OUTPUT_FILE) {
          if (&((NodeImageMultiFile *)node->storage)->format == imf) {
            return BLI_sprintfN("nodes['%s'].format", node->name);
          }
          else {
            bNodeSocket *sock;

            for (sock = node->inputs.first; sock; sock = sock->next) {
              NodeImageMultiFileSocket *sockdata = sock->storage;
              if (&sockdata->format == imf) {
                return BLI_sprintfN(
                    "nodes['%s'].file_slots['%s'].format", node->name, sockdata->path);
              }
            }
          }
        }
      }
      return BLI_strdup("..");
    }
    default:
      return BLI_strdup("..");
  }
}

static int rna_RenderSettings_threads_get(PointerRNA *ptr)
{
  RenderData *rd = (RenderData *)ptr->data;
  return BKE_render_num_threads(rd);
}

static int rna_RenderSettings_threads_mode_get(PointerRNA *ptr)
{
  RenderData *rd = (RenderData *)ptr->data;
  int override = BLI_system_num_threads_override_get();

  if (override > 0) {
    return R_FIXED_THREADS;
  }
  else {
    return (rd->mode & R_FIXED_THREADS);
  }
}

static bool rna_RenderSettings_is_movie_format_get(PointerRNA *ptr)
{
  RenderData *rd = (RenderData *)ptr->data;
  return BKE_imtype_is_movie(rd->im_format.imtype);
}

static void rna_ImageFormatSettings_file_format_set(PointerRNA *ptr, int value)
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;
  ID *id = ptr->id.data;
  const bool is_render = (id && GS(id->name) == ID_SCE);
  /* see note below on why this is */
  const char chan_flag = BKE_imtype_valid_channels(imf->imtype, true) |
                         (is_render ? IMA_CHAN_FLAG_BW : 0);

  imf->imtype = value;

  /* ensure depth and color settings match */
  if (((imf->planes == R_IMF_PLANES_BW) && !(chan_flag & IMA_CHAN_FLAG_BW)) ||
      ((imf->planes == R_IMF_PLANES_RGBA) && !(chan_flag & IMA_CHAN_FLAG_ALPHA))) {
    imf->planes = R_IMF_PLANES_RGB;
  }

  /* ensure usable depth */
  {
    const int depth_ok = BKE_imtype_valid_depths(imf->imtype);
    if ((imf->depth & depth_ok) == 0) {
      /* set first available depth */
      char depth_ls[] = {
          R_IMF_CHAN_DEPTH_32,
          R_IMF_CHAN_DEPTH_24,
          R_IMF_CHAN_DEPTH_16,
          R_IMF_CHAN_DEPTH_12,
          R_IMF_CHAN_DEPTH_10,
          R_IMF_CHAN_DEPTH_8,
          R_IMF_CHAN_DEPTH_1,
          0,
      };
      int i;

      for (i = 0; depth_ls[i]; i++) {
        if (depth_ok & depth_ls[i]) {
          imf->depth = depth_ls[i];
          break;
        }
      }
    }
  }

  if (id && GS(id->name) == ID_SCE) {
    Scene *scene = ptr->id.data;
    RenderData *rd = &scene->r;
#  ifdef WITH_FFMPEG
    BKE_ffmpeg_image_type_verify(rd, imf);
#  endif
    (void)rd;
  }
}

static const EnumPropertyItem *rna_ImageFormatSettings_file_format_itemf(bContext *UNUSED(C),
                                                                         PointerRNA *ptr,
                                                                         PropertyRNA *UNUSED(prop),
                                                                         bool *UNUSED(r_free))
{
  ID *id = ptr->id.data;
  if (id && GS(id->name) == ID_SCE) {
    return rna_enum_image_type_items;
  }
  else {
    return image_only_type_items;
  }
}

static const EnumPropertyItem *rna_ImageFormatSettings_color_mode_itemf(bContext *UNUSED(C),
                                                                        PointerRNA *ptr,
                                                                        PropertyRNA *UNUSED(prop),
                                                                        bool *r_free)
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;
  ID *id = ptr->id.data;
  const bool is_render = (id && GS(id->name) == ID_SCE);

  /* note, we need to act differently for render
   * where 'BW' will force grayscale even if the output format writes
   * as RGBA, this is age old blender convention and not sure how useful
   * it really is but keep it for now - campbell */
  char chan_flag = BKE_imtype_valid_channels(imf->imtype, true) |
                   (is_render ? IMA_CHAN_FLAG_BW : 0);

#  ifdef WITH_FFMPEG
  /* a WAY more crappy case than B&W flag: depending on codec, file format MIGHT support
   * alpha channel. for example MPEG format with h264 codec can't do alpha channel, but
   * the same MPEG format with QTRLE codec can easily handle alpha channel.
   * not sure how to deal with such cases in a nicer way (sergey) */
  if (is_render) {
    Scene *scene = ptr->id.data;
    RenderData *rd = &scene->r;

    if (BKE_ffmpeg_alpha_channel_is_supported(rd)) {
      chan_flag |= IMA_CHAN_FLAG_ALPHA;
    }
  }
#  endif

  if (chan_flag == (IMA_CHAN_FLAG_BW | IMA_CHAN_FLAG_RGB | IMA_CHAN_FLAG_ALPHA)) {
    return rna_enum_image_color_mode_items;
  }
  else {
    int totitem = 0;
    EnumPropertyItem *item = NULL;

    if (chan_flag & IMA_CHAN_FLAG_BW) {
      RNA_enum_item_add(&item, &totitem, &IMAGE_COLOR_MODE_BW);
    }
    if (chan_flag & IMA_CHAN_FLAG_RGB) {
      RNA_enum_item_add(&item, &totitem, &IMAGE_COLOR_MODE_RGB);
    }
    if (chan_flag & IMA_CHAN_FLAG_ALPHA) {
      RNA_enum_item_add(&item, &totitem, &IMAGE_COLOR_MODE_RGBA);
    }

    RNA_enum_item_end(&item, &totitem);
    *r_free = true;

    return item;
  }
}

static const EnumPropertyItem *rna_ImageFormatSettings_color_depth_itemf(bContext *UNUSED(C),
                                                                         PointerRNA *ptr,
                                                                         PropertyRNA *UNUSED(prop),
                                                                         bool *r_free)
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;

  if (imf == NULL) {
    return rna_enum_image_color_depth_items;
  }
  else {
    const int depth_ok = BKE_imtype_valid_depths(imf->imtype);
    const int is_float = ELEM(
        imf->imtype, R_IMF_IMTYPE_RADHDR, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER);

    const EnumPropertyItem *item_8bit = &rna_enum_image_color_depth_items[0];
    const EnumPropertyItem *item_10bit = &rna_enum_image_color_depth_items[1];
    const EnumPropertyItem *item_12bit = &rna_enum_image_color_depth_items[2];
    const EnumPropertyItem *item_16bit = &rna_enum_image_color_depth_items[3];
    const EnumPropertyItem *item_32bit = &rna_enum_image_color_depth_items[4];

    int totitem = 0;
    EnumPropertyItem *item = NULL;
    EnumPropertyItem tmp = {0, "", 0, "", ""};

    if (depth_ok & R_IMF_CHAN_DEPTH_8) {
      RNA_enum_item_add(&item, &totitem, item_8bit);
    }

    if (depth_ok & R_IMF_CHAN_DEPTH_10) {
      RNA_enum_item_add(&item, &totitem, item_10bit);
    }

    if (depth_ok & R_IMF_CHAN_DEPTH_12) {
      RNA_enum_item_add(&item, &totitem, item_12bit);
    }

    if (depth_ok & R_IMF_CHAN_DEPTH_16) {
      if (is_float) {
        tmp = *item_16bit;
        tmp.name = "Float (Half)";
        RNA_enum_item_add(&item, &totitem, &tmp);
      }
      else {
        RNA_enum_item_add(&item, &totitem, item_16bit);
      }
    }

    if (depth_ok & R_IMF_CHAN_DEPTH_32) {
      if (is_float) {
        tmp = *item_32bit;
        tmp.name = "Float (Full)";
        RNA_enum_item_add(&item, &totitem, &tmp);
      }
      else {
        RNA_enum_item_add(&item, &totitem, item_32bit);
      }
    }

    RNA_enum_item_end(&item, &totitem);
    *r_free = true;

    return item;
  }
}

static const EnumPropertyItem *rna_ImageFormatSettings_views_format_itemf(
    bContext *UNUSED(C), PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *UNUSED(r_free))
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;

  if (imf == NULL) {
    return rna_enum_views_format_items;
  }
  else if (imf->imtype == R_IMF_IMTYPE_OPENEXR) {
    return rna_enum_views_format_multiview_items;
  }
  else if (imf->imtype == R_IMF_IMTYPE_MULTILAYER) {
    return rna_enum_views_format_multilayer_items;
  }
  else {
    return rna_enum_views_format_items;
  }
}

#  ifdef WITH_OPENEXR
/* OpenEXR */

static const EnumPropertyItem *rna_ImageFormatSettings_exr_codec_itemf(bContext *UNUSED(C),
                                                                       PointerRNA *ptr,
                                                                       PropertyRNA *UNUSED(prop),
                                                                       bool *r_free)
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;

  EnumPropertyItem *item = NULL;
  int i = 1, totitem = 0;

  if (imf->depth == 16) {
    return rna_enum_exr_codec_items; /* All compression types are defined for halfs */
  }

  for (i = 0; i < R_IMF_EXR_CODEC_MAX; i++) {
    if ((i == R_IMF_EXR_CODEC_B44 || i == R_IMF_EXR_CODEC_B44A)) {
      continue; /* B44 and B44A are not defined for 32 bit floats */
    }

    RNA_enum_item_add(&item, &totitem, &rna_enum_exr_codec_items[i]);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

#  endif
static int rna_SceneRender_file_ext_length(PointerRNA *ptr)
{
  RenderData *rd = (RenderData *)ptr->data;
  char ext[8];
  ext[0] = '\0';
  BKE_image_path_ensure_ext_from_imformat(ext, &rd->im_format);
  return strlen(ext);
}

static void rna_SceneRender_file_ext_get(PointerRNA *ptr, char *str)
{
  RenderData *rd = (RenderData *)ptr->data;
  str[0] = '\0';
  BKE_image_path_ensure_ext_from_imformat(str, &rd->im_format);
}

#  ifdef WITH_FFMPEG
static void rna_FFmpegSettings_lossless_output_set(PointerRNA *ptr, bool value)
{
  Scene *scene = (Scene *)ptr->id.data;
  RenderData *rd = &scene->r;

  if (value) {
    rd->ffcodecdata.flags |= FFMPEG_LOSSLESS_OUTPUT;
  }
  else {
    rd->ffcodecdata.flags &= ~FFMPEG_LOSSLESS_OUTPUT;
  }

  BKE_ffmpeg_codec_settings_verify(rd);
}

static void rna_FFmpegSettings_codec_settings_update(Main *UNUSED(bmain),
                                                     Scene *UNUSED(scene_unused),
                                                     PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  RenderData *rd = &scene->r;

  BKE_ffmpeg_codec_settings_verify(rd);
}
#  endif

static int rna_RenderSettings_active_view_index_get(PointerRNA *ptr)
{
  RenderData *rd = (RenderData *)ptr->data;
  return rd->actview;
}

static void rna_RenderSettings_active_view_index_set(PointerRNA *ptr, int value)
{
  RenderData *rd = (RenderData *)ptr->data;
  rd->actview = value;
}

static void rna_RenderSettings_active_view_index_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  RenderData *rd = (RenderData *)ptr->data;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&rd->views) - 1);
}

static PointerRNA rna_RenderSettings_active_view_get(PointerRNA *ptr)
{
  RenderData *rd = (RenderData *)ptr->data;
  SceneRenderView *srv = BLI_findlink(&rd->views, rd->actview);

  return rna_pointer_inherit_refine(ptr, &RNA_SceneRenderView, srv);
}

static void rna_RenderSettings_active_view_set(PointerRNA *ptr,
                                               PointerRNA value,
                                               struct ReportList *UNUSED(reports))
{
  RenderData *rd = (RenderData *)ptr->data;
  SceneRenderView *srv = (SceneRenderView *)value.data;
  const int index = BLI_findindex(&rd->views, srv);
  if (index != -1) {
    rd->actview = index;
  }
}

static SceneRenderView *rna_RenderView_new(ID *id, RenderData *UNUSED(rd), const char *name)
{
  Scene *scene = (Scene *)id;
  SceneRenderView *srv = BKE_scene_add_render_view(scene, name);

  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);

  return srv;
}

static void rna_RenderView_remove(
    ID *id, RenderData *UNUSED(rd), Main *UNUSED(bmain), ReportList *reports, PointerRNA *srv_ptr)
{
  SceneRenderView *srv = srv_ptr->data;
  Scene *scene = (Scene *)id;

  if (!BKE_scene_remove_render_view(scene, srv)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Render view '%s' could not be removed from scene '%s'",
                srv->name,
                scene->id.name + 2);
    return;
  }

  RNA_POINTER_INVALIDATE(srv_ptr);

  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);
}

static void rna_RenderSettings_views_format_set(PointerRNA *ptr, int value)
{
  RenderData *rd = (RenderData *)ptr->data;

  if (rd->views_format == SCE_VIEWS_FORMAT_MULTIVIEW && value == SCE_VIEWS_FORMAT_STEREO_3D) {
    /* make sure the actview is visible */
    if (rd->actview > 1) {
      rd->actview = 1;
    }
  }

  rd->views_format = value;
}

static void rna_RenderSettings_engine_set(PointerRNA *ptr, int value)
{
  RenderData *rd = (RenderData *)ptr->data;
  RenderEngineType *type = BLI_findlink(&R_engines, value);

  if (type) {
    BLI_strncpy_utf8(rd->engine, type->idname, sizeof(rd->engine));
    DEG_id_tag_update(ptr->id.data, ID_RECALC_COPY_ON_WRITE);
  }
}

static const EnumPropertyItem *rna_RenderSettings_engine_itemf(bContext *UNUSED(C),
                                                               PointerRNA *UNUSED(ptr),
                                                               PropertyRNA *UNUSED(prop),
                                                               bool *r_free)
{
  RenderEngineType *type;
  EnumPropertyItem *item = NULL;
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  int a = 0, totitem = 0;

  for (type = R_engines.first; type; type = type->next, a++) {
    tmp.value = a;
    tmp.identifier = type->idname;
    tmp.name = type->name;
    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static int rna_RenderSettings_engine_get(PointerRNA *ptr)
{
  RenderData *rd = (RenderData *)ptr->data;
  RenderEngineType *type;
  int a = 0;

  for (type = R_engines.first; type; type = type->next, a++) {
    if (STREQ(type->idname, rd->engine)) {
      return a;
    }
  }

  return 0;
}

static void rna_RenderSettings_engine_update(Main *bmain,
                                             Scene *UNUSED(unused),
                                             PointerRNA *UNUSED(ptr))
{
  ED_render_engine_changed(bmain);
}

static bool rna_RenderSettings_multiple_engines_get(PointerRNA *UNUSED(ptr))
{
  return (BLI_listbase_count(&R_engines) > 1);
}

static bool rna_RenderSettings_use_spherical_stereo_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  return BKE_scene_use_spherical_stereo(scene);
}

void rna_Scene_glsl_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;

  DEG_id_tag_update(&scene->id, 0);
}

static void rna_Scene_world_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Scene *sc = (Scene *)ptr->id.data;

  rna_Scene_glsl_update(bmain, scene, ptr);
  WM_main_add_notifier(NC_WORLD | ND_WORLD, &sc->id);
  DEG_relations_tag_update(bmain);
}

void rna_Scene_freestyle_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;

  DEG_id_tag_update(&scene->id, 0);
}

void rna_Scene_use_view_map_cache_update(Main *UNUSED(bmain),
                                         Scene *UNUSED(scene),
                                         PointerRNA *UNUSED(ptr))
{
#  ifdef WITH_FREESTYLE
  FRS_free_view_map_cache();
#  endif
}

void rna_ViewLayer_name_set(PointerRNA *ptr, const char *value)
{
  Scene *scene = (Scene *)ptr->id.data;
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  BLI_assert(BKE_id_is_in_global_main(&scene->id));
  BKE_view_layer_rename(G_MAIN, scene, view_layer, value);
}

static void rna_SceneRenderView_name_set(PointerRNA *ptr, const char *value)
{
  Scene *scene = (Scene *)ptr->id.data;
  SceneRenderView *rv = (SceneRenderView *)ptr->data;
  BLI_strncpy_utf8(rv->name, value, sizeof(rv->name));
  BLI_uniquename(&scene->r.views,
                 rv,
                 DATA_("RenderView"),
                 '.',
                 offsetof(SceneRenderView, name),
                 sizeof(rv->name));
}

void rna_ViewLayer_material_override_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  rna_Scene_glsl_update(bmain, scene, ptr);
  DEG_relations_tag_update(bmain);
}

void rna_ViewLayer_pass_update(Main *bmain, Scene *activescene, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;

  if (scene->nodetree) {
    ntreeCompositUpdateRLayers(scene->nodetree);
  }

  rna_Scene_glsl_update(bmain, activescene, ptr);
}

static char *rna_SceneRenderView_path(PointerRNA *ptr)
{
  SceneRenderView *srv = (SceneRenderView *)ptr->data;
  return BLI_sprintfN("render.views[\"%s\"]", srv->name);
}

static void rna_Scene_use_nodes_update(bContext *C, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;
  if (scene->use_nodes && scene->nodetree == NULL) {
    ED_node_composit_default(C, scene);
  }
  DEG_relations_tag_update(CTX_data_main(C));
}

static void rna_Physics_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
    BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_DEPSGRAPH);
  }
  FOREACH_SCENE_OBJECT_END;

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
}

static void rna_Scene_editmesh_select_mode_set(PointerRNA *ptr, const bool *value)
{
  ToolSettings *ts = (ToolSettings *)ptr->data;
  int flag = (value[0] ? SCE_SELECT_VERTEX : 0) | (value[1] ? SCE_SELECT_EDGE : 0) |
             (value[2] ? SCE_SELECT_FACE : 0);

  if (flag) {
    ts->selectmode = flag;

    /* Update select mode in all the workspaces in mesh edit mode. */
    wmWindowManager *wm = G_MAIN->wm.first;
    for (wmWindow *win = wm->windows.first; win; win = win->next) {
      ViewLayer *view_layer = WM_window_get_active_view_layer(win);

      if (view_layer && view_layer->basact) {
        Mesh *me = BKE_mesh_from_object(view_layer->basact->object);
        if (me && me->edit_mesh && me->edit_mesh->selectmode != flag) {
          me->edit_mesh->selectmode = flag;
          EDBM_selectmode_set(me->edit_mesh);
        }
      }
    }
  }
}

static void rna_Scene_editmesh_select_mode_update(bContext *C, PointerRNA *UNUSED(ptr))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Mesh *me = NULL;

  if (view_layer->basact) {
    me = BKE_mesh_from_object(view_layer->basact->object);
    if (me && me->edit_mesh == NULL) {
      me = NULL;
    }
  }

  if (me) {
    DEG_id_tag_update(&me->id, ID_RECALC_SELECT);
    WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);
  }
}

static void object_simplify_update(Object *ob)
{
  ModifierData *md;
  ParticleSystem *psys;

  if ((ob->id.tag & LIB_TAG_DOIT) == 0) {
    return;
  }

  ob->id.tag &= ~LIB_TAG_DOIT;

  for (md = ob->modifiers.first; md; md = md->next) {
    if (ELEM(md->type,
             eModifierType_Subsurf,
             eModifierType_Multires,
             eModifierType_ParticleSystem)) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }

  for (psys = ob->particlesystem.first; psys; psys = psys->next) {
    psys->recalc |= ID_RECALC_PSYS_CHILD;
  }

  if (ob->instance_collection) {
    CollectionObject *cob;

    for (cob = ob->instance_collection->gobject.first; cob; cob = cob->next) {
      object_simplify_update(cob->ob);
    }
  }
}

static void rna_Scene_use_simplify_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *sce = ptr->id.data;
  Scene *sce_iter;
  Base *base;

  BKE_main_id_tag_listbase(&bmain->objects, LIB_TAG_DOIT, true);
  FOREACH_SCENE_OBJECT_BEGIN (sce, ob) {
    object_simplify_update(ob);
  }
  FOREACH_SCENE_OBJECT_END;

  for (SETLOOPER_SET_ONLY(sce, sce_iter, base)) {
    object_simplify_update(base->object);
  }

  WM_main_add_notifier(NC_GEOM | ND_DATA, NULL);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, NULL);
  DEG_id_tag_update(&sce->id, 0);
}

static void rna_Scene_simplify_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Scene *sce = ptr->id.data;

  if (sce->r.mode & R_SIMPLIFY) {
    rna_Scene_use_simplify_update(bmain, scene, ptr);
  }
}

static void rna_Scene_use_persistent_data_update(Main *UNUSED(bmain),
                                                 Scene *UNUSED(scene),
                                                 PointerRNA *ptr)
{
  Scene *sce = ptr->id.data;

  if (!(sce->r.mode & R_PERSISTENT_DATA)) {
    RE_FreePersistentData();
  }
}

/* Scene.transform_orientation_slots */
static void rna_Scene_transform_orientation_slots_begin(CollectionPropertyIterator *iter,
                                                        PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  TransformOrientationSlot *orient_slot = &scene->orientation_slots[0];
  rna_iterator_array_begin(
      iter, orient_slot, sizeof(*orient_slot), ARRAY_SIZE(scene->orientation_slots), 0, NULL);
}

static int rna_Scene_transform_orientation_slots_length(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  return ARRAY_SIZE(scene->orientation_slots);
}

static bool rna_Scene_use_audio_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;
  return (scene->audio.flag & AUDIO_MUTE) != 0;
}

static void rna_Scene_use_audio_set(PointerRNA *ptr, bool value)
{
  Scene *scene = (Scene *)ptr->data;

  if (value) {
    scene->audio.flag |= AUDIO_MUTE;
  }
  else {
    scene->audio.flag &= ~AUDIO_MUTE;
  }
}

static void rna_Scene_use_audio_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_MUTE);
}

static int rna_Scene_sync_mode_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;
  if (scene->audio.flag & AUDIO_SYNC) {
    return AUDIO_SYNC;
  }
  return scene->flag & SCE_FRAME_DROP;
}

static void rna_Scene_sync_mode_set(PointerRNA *ptr, int value)
{
  Scene *scene = (Scene *)ptr->data;

  if (value == AUDIO_SYNC) {
    scene->audio.flag |= AUDIO_SYNC;
  }
  else if (value == SCE_FRAME_DROP) {
    scene->audio.flag &= ~AUDIO_SYNC;
    scene->flag |= SCE_FRAME_DROP;
  }
  else {
    scene->audio.flag &= ~AUDIO_SYNC;
    scene->flag &= ~SCE_FRAME_DROP;
  }
}

static void rna_View3DCursor_rotation_mode_set(PointerRNA *ptr, int value)
{
  View3DCursor *cursor = ptr->data;

  /* use API Method for conversions... */
  BKE_rotMode_change_values(cursor->rotation_quaternion,
                            cursor->rotation_euler,
                            cursor->rotation_axis,
                            &cursor->rotation_angle,
                            cursor->rotation_mode,
                            (short)value);

  /* finally, set the new rotation type */
  cursor->rotation_mode = value;
}

static void rna_View3DCursor_rotation_axis_angle_get(PointerRNA *ptr, float *value)
{
  View3DCursor *cursor = ptr->data;
  value[0] = cursor->rotation_angle;
  copy_v3_v3(&value[1], cursor->rotation_axis);
}

static void rna_View3DCursor_rotation_axis_angle_set(PointerRNA *ptr, const float *value)
{
  View3DCursor *cursor = ptr->data;
  cursor->rotation_angle = value[0];
  copy_v3_v3(cursor->rotation_axis, &value[1]);
}

static void rna_View3DCursor_matrix_get(PointerRNA *ptr, float *values)
{
  const View3DCursor *cursor = ptr->data;
  BKE_scene_cursor_to_mat4(cursor, (float(*)[4])values);
}

static void rna_View3DCursor_matrix_set(PointerRNA *ptr, const float *values)
{
  View3DCursor *cursor = ptr->data;
  float unit_mat[4][4];
  normalize_m4_m4(unit_mat, (const float(*)[4])values);
  BKE_scene_cursor_from_mat4(cursor, unit_mat, false);
}

static char *rna_View3DCursor_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("cursor");
}

static TimeMarker *rna_TimeLine_add(Scene *scene, const char name[], int frame)
{
  TimeMarker *marker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
  marker->flag = SELECT;
  marker->frame = frame;
  BLI_strncpy_utf8(marker->name, name, sizeof(marker->name));
  BLI_addtail(&scene->markers, marker);

  WM_main_add_notifier(NC_SCENE | ND_MARKERS, NULL);
  WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, NULL);

  return marker;
}

static void rna_TimeLine_remove(Scene *scene, ReportList *reports, PointerRNA *marker_ptr)
{
  TimeMarker *marker = marker_ptr->data;
  if (BLI_remlink_safe(&scene->markers, marker) == false) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Timeline marker '%s' not found in scene '%s'",
                marker->name,
                scene->id.name + 2);
    return;
  }

  MEM_freeN(marker);
  RNA_POINTER_INVALIDATE(marker_ptr);

  WM_main_add_notifier(NC_SCENE | ND_MARKERS, NULL);
  WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, NULL);
}

static void rna_TimeLine_clear(Scene *scene)
{
  BLI_freelistN(&scene->markers);

  WM_main_add_notifier(NC_SCENE | ND_MARKERS, NULL);
  WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, NULL);
}

static KeyingSet *rna_Scene_keying_set_new(Scene *sce,
                                           ReportList *reports,
                                           const char idname[],
                                           const char name[])
{
  KeyingSet *ks = NULL;

  /* call the API func, and set the active keyingset index */
  ks = BKE_keyingset_add(&sce->keyingsets, idname, name, KEYINGSET_ABSOLUTE, 0);

  if (ks) {
    sce->active_keyingset = BLI_listbase_count(&sce->keyingsets);
    return ks;
  }
  else {
    BKE_report(reports, RPT_ERROR, "Keying set could not be added");
    return NULL;
  }
}

static void rna_UnifiedPaintSettings_update(bContext *C, PointerRNA *UNUSED(ptr))
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Brush *br = BKE_paint_brush(BKE_paint_get_active(scene, view_layer));
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, br);
}

static void rna_UnifiedPaintSettings_size_set(PointerRNA *ptr, int value)
{
  UnifiedPaintSettings *ups = ptr->data;

  /* scale unprojected radius so it stays consistent with brush size */
  BKE_brush_scale_unprojected_radius(&ups->unprojected_radius, value, ups->size);
  ups->size = value;
}

static void rna_UnifiedPaintSettings_unprojected_radius_set(PointerRNA *ptr, float value)
{
  UnifiedPaintSettings *ups = ptr->data;

  /* scale brush size so it stays consistent with unprojected_radius */
  BKE_brush_scale_size(&ups->size, value, ups->unprojected_radius);
  ups->unprojected_radius = value;
}

static void rna_UnifiedPaintSettings_radius_update(bContext *C, PointerRNA *ptr)
{
  /* changing the unified size should invalidate the overlay but also update the brush */
  BKE_paint_invalidate_overlay_all();
  rna_UnifiedPaintSettings_update(C, ptr);
}

static char *rna_UnifiedPaintSettings_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("tool_settings.unified_paint_settings");
}

static char *rna_CurvePaintSettings_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("tool_settings.curve_paint_settings");
}

/* generic function to recalc geometry */
static void rna_EditMesh_update(bContext *C, PointerRNA *UNUSED(ptr))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Mesh *me = NULL;

  if (view_layer->basact) {
    me = BKE_mesh_from_object(view_layer->basact->object);
    if (me && me->edit_mesh == NULL) {
      me = NULL;
    }
  }

  if (me) {
    DEG_id_tag_update(&me->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, me);
  }
}

static char *rna_MeshStatVis_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("tool_settings.statvis");
}

/* note: without this, when Multi-Paint is activated/deactivated, the colors
 * will not change right away when multiple bones are selected, this function
 * is not for general use and only for the few cases where changing scene
 * settings and NOT for general purpose updates, possibly this should be
 * given its own notifier. */
static void rna_Scene_update_active_object_data(bContext *C, PointerRNA *UNUSED(ptr))
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);

  if (ob) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_OBJECT | ND_DRAW, &ob->id);
  }
}

static void rna_SceneCamera_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Object *camera = scene->camera;

  BKE_sequencer_cache_cleanup_all(bmain);

  if (camera && (camera->type == OB_CAMERA)) {
    DEG_id_tag_update(&camera->id, ID_RECALC_GEOMETRY);
  }
}

static void rna_SceneSequencer_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
  BKE_sequencer_cache_cleanup(scene);
}

static char *rna_ToolSettings_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("tool_settings");
}

PointerRNA rna_FreestyleLineSet_linestyle_get(PointerRNA *ptr)
{
  FreestyleLineSet *lineset = (FreestyleLineSet *)ptr->data;

  return rna_pointer_inherit_refine(ptr, &RNA_FreestyleLineStyle, lineset->linestyle);
}

void rna_FreestyleLineSet_linestyle_set(PointerRNA *ptr,
                                        PointerRNA value,
                                        struct ReportList *UNUSED(reports))
{
  FreestyleLineSet *lineset = (FreestyleLineSet *)ptr->data;

  if (lineset->linestyle) {
    id_us_min(&lineset->linestyle->id);
  }
  lineset->linestyle = (FreestyleLineStyle *)value.data;
  id_us_plus(&lineset->linestyle->id);
}

FreestyleLineSet *rna_FreestyleSettings_lineset_add(ID *id,
                                                    FreestyleSettings *config,
                                                    Main *bmain,
                                                    const char *name)
{
  Scene *scene = (Scene *)id;
  FreestyleLineSet *lineset = BKE_freestyle_lineset_add(bmain, (FreestyleConfig *)config, name);

  DEG_id_tag_update(&scene->id, 0);
  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);

  return lineset;
}

void rna_FreestyleSettings_lineset_remove(ID *id,
                                          FreestyleSettings *config,
                                          ReportList *reports,
                                          PointerRNA *lineset_ptr)
{
  FreestyleLineSet *lineset = lineset_ptr->data;
  Scene *scene = (Scene *)id;

  if (!BKE_freestyle_lineset_delete((FreestyleConfig *)config, lineset)) {
    BKE_reportf(reports, RPT_ERROR, "Line set '%s' could not be removed", lineset->name);
    return;
  }

  RNA_POINTER_INVALIDATE(lineset_ptr);

  DEG_id_tag_update(&scene->id, 0);
  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);
}

PointerRNA rna_FreestyleSettings_active_lineset_get(PointerRNA *ptr)
{
  FreestyleConfig *config = (FreestyleConfig *)ptr->data;
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(config);
  return rna_pointer_inherit_refine(ptr, &RNA_FreestyleLineSet, lineset);
}

void rna_FreestyleSettings_active_lineset_index_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  FreestyleConfig *config = (FreestyleConfig *)ptr->data;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&config->linesets) - 1);
}

int rna_FreestyleSettings_active_lineset_index_get(PointerRNA *ptr)
{
  FreestyleConfig *config = (FreestyleConfig *)ptr->data;
  return BKE_freestyle_lineset_get_active_index(config);
}

void rna_FreestyleSettings_active_lineset_index_set(PointerRNA *ptr, int value)
{
  FreestyleConfig *config = (FreestyleConfig *)ptr->data;
  BKE_freestyle_lineset_set_active_index(config, value);
}

FreestyleModuleConfig *rna_FreestyleSettings_module_add(ID *id, FreestyleSettings *config)
{
  Scene *scene = (Scene *)id;
  FreestyleModuleConfig *module = BKE_freestyle_module_add((FreestyleConfig *)config);

  DEG_id_tag_update(&scene->id, 0);
  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);

  return module;
}

void rna_FreestyleSettings_module_remove(ID *id,
                                         FreestyleSettings *config,
                                         ReportList *reports,
                                         PointerRNA *module_ptr)
{
  Scene *scene = (Scene *)id;
  FreestyleModuleConfig *module = module_ptr->data;

  if (!BKE_freestyle_module_delete((FreestyleConfig *)config, module)) {
    if (module->script) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Style module '%s' could not be removed",
                  module->script->id.name + 2);
    }
    else {
      BKE_report(reports, RPT_ERROR, "Style module could not be removed");
    }
    return;
  }

  RNA_POINTER_INVALIDATE(module_ptr);

  DEG_id_tag_update(&scene->id, 0);
  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);
}

static void rna_Stereo3dFormat_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  ID *id = ptr->id.data;

  if (id && GS(id->name) == ID_IM) {
    Image *ima = (Image *)id;
    ImBuf *ibuf;
    void *lock;

    if (!BKE_image_is_stereo(ima)) {
      return;
    }

    ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);

    if (ibuf) {
      BKE_image_signal(bmain, ima, NULL, IMA_SIGNAL_FREE);
    }
    BKE_image_release_ibuf(ima, ibuf, lock);
  }
}

static ViewLayer *rna_ViewLayer_new(ID *id, Scene *UNUSED(sce), Main *bmain, const char *name)
{
  Scene *scene = (Scene *)id;
  ViewLayer *view_layer = BKE_view_layer_add(scene, name);

  DEG_id_tag_update(&scene->id, 0);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_LAYER, NULL);

  return view_layer;
}

static void rna_ViewLayer_remove(
    ID *id, Scene *UNUSED(sce), Main *bmain, ReportList *reports, PointerRNA *sl_ptr)
{
  Scene *scene = (Scene *)id;
  ViewLayer *view_layer = sl_ptr->data;

  if (ED_scene_view_layer_delete(bmain, scene, view_layer, reports)) {
    RNA_POINTER_INVALIDATE(sl_ptr);
  }
}

/* Fake value, used internally (not saved to DNA). */
#  define V3D_ORIENT_DEFAULT -1

static int rna_TransformOrientationSlot_type_get(PointerRNA *ptr)
{
  Scene *scene = ptr->id.data;
  TransformOrientationSlot *orient_slot = ptr->data;
  if (orient_slot != &scene->orientation_slots[SCE_ORIENT_DEFAULT]) {
    if ((orient_slot->flag & SELECT) == 0) {
      return V3D_ORIENT_DEFAULT;
    }
  }
  return BKE_scene_orientation_slot_get_index(orient_slot);
}

void rna_TransformOrientationSlot_type_set(PointerRNA *ptr, int value)
{
  Scene *scene = ptr->id.data;
  TransformOrientationSlot *orient_slot = ptr->data;

  if (orient_slot != &scene->orientation_slots[SCE_ORIENT_DEFAULT]) {
    if (value == V3D_ORIENT_DEFAULT) {
      orient_slot->flag &= ~SELECT;
      return;
    }
    else {
      orient_slot->flag |= SELECT;
    }
  }

  BKE_scene_orientation_slot_set_index(orient_slot, value);
}

static PointerRNA rna_TransformOrientationSlot_get(PointerRNA *ptr)
{
  Scene *scene = ptr->id.data;
  TransformOrientationSlot *orient_slot = ptr->data;
  TransformOrientation *orientation;
  if (orient_slot->type < V3D_ORIENT_CUSTOM) {
    orientation = NULL;
  }
  else {
    orientation = BKE_scene_transform_orientation_find(scene, orient_slot->index_custom);
  }
  return rna_pointer_inherit_refine(ptr, &RNA_TransformOrientation, orientation);
}

static const EnumPropertyItem *rna_TransformOrientation_impl_itemf(Scene *scene,
                                                                   const bool include_default,
                                                                   bool *r_free)
{
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  EnumPropertyItem *item = NULL;
  int i = V3D_ORIENT_CUSTOM, totitem = 0;

  if (include_default) {
    tmp.identifier = "DEFAULT";
    tmp.name = "Default";
    tmp.description = "Use the scene orientation";
    tmp.value = V3D_ORIENT_DEFAULT;
    tmp.icon = ICON_OBJECT_ORIGIN;
    RNA_enum_item_add(&item, &totitem, &tmp);
    tmp.icon = 0;

    RNA_enum_item_add_separator(&item, &totitem);
  }

  RNA_enum_items_add(&item, &totitem, rna_enum_transform_orientation_items);

  const ListBase *transform_orientations = scene ? &scene->transform_spaces : NULL;

  if (transform_orientations && (BLI_listbase_is_empty(transform_orientations) == false)) {
    RNA_enum_item_add_separator(&item, &totitem);

    for (TransformOrientation *ts = transform_orientations->first; ts; ts = ts->next) {
      tmp.identifier = ts->name;
      tmp.name = ts->name;
      tmp.value = i++;
      RNA_enum_item_add(&item, &totitem, &tmp);
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}
const EnumPropertyItem *rna_TransformOrientation_itemf(bContext *C,
                                                       PointerRNA *ptr,
                                                       PropertyRNA *UNUSED(prop),
                                                       bool *r_free)
{
  Scene *scene;
  if (ptr->id.data && (GS(((ID *)ptr->id.data)->name) == ID_SCE)) {
    scene = ptr->id.data;
  }
  else {
    scene = CTX_data_scene(C);
  }
  return rna_TransformOrientation_impl_itemf(scene, false, r_free);
}

const EnumPropertyItem *rna_TransformOrientation_with_scene_itemf(bContext *UNUSED(C),
                                                                  PointerRNA *ptr,
                                                                  PropertyRNA *UNUSED(prop),
                                                                  bool *r_free)
{
  Scene *scene = ptr->id.data;
  TransformOrientationSlot *orient_slot = ptr->data;
  bool include_default = (orient_slot != &scene->orientation_slots[SCE_ORIENT_DEFAULT]);
  return rna_TransformOrientation_impl_itemf(scene, include_default, r_free);
}

#  undef V3D_ORIENT_DEFAULT

static const EnumPropertyItem *rna_UnitSettings_itemf_wrapper(const int system,
                                                              const int type,
                                                              bool *r_free)
{
  const void *usys;
  int len;
  bUnit_GetSystem(system, type, &usys, &len);

  EnumPropertyItem *items = NULL;
  int totitem = 0;

  EnumPropertyItem adaptive = {0};
  adaptive.identifier = "ADAPTIVE";
  adaptive.name = "Adaptive";
  adaptive.value = USER_UNIT_ADAPTIVE;
  RNA_enum_item_add(&items, &totitem, &adaptive);

  for (int i = 0; i < len; i++) {
    if (!bUnit_IsSuppressed(usys, i)) {
      EnumPropertyItem tmp = {0};
      tmp.identifier = bUnit_GetIdentifier(usys, i);
      tmp.name = bUnit_GetNameDisplay(usys, i);
      tmp.value = i;
      RNA_enum_item_add(&items, &totitem, &tmp);
    }
  }

  *r_free = true;
  return items;
}

const EnumPropertyItem *rna_UnitSettings_length_unit_itemf(bContext *UNUSED(C),
                                                           PointerRNA *ptr,
                                                           PropertyRNA *UNUSED(prop),
                                                           bool *r_free)
{
  UnitSettings *units = ptr->data;
  return rna_UnitSettings_itemf_wrapper(units->system, B_UNIT_LENGTH, r_free);
}

const EnumPropertyItem *rna_UnitSettings_mass_unit_itemf(bContext *UNUSED(C),
                                                         PointerRNA *ptr,
                                                         PropertyRNA *UNUSED(prop),
                                                         bool *r_free)
{
  UnitSettings *units = ptr->data;
  return rna_UnitSettings_itemf_wrapper(units->system, B_UNIT_MASS, r_free);
}

const EnumPropertyItem *rna_UnitSettings_time_unit_itemf(bContext *UNUSED(C),
                                                         PointerRNA *ptr,
                                                         PropertyRNA *UNUSED(prop),
                                                         bool *r_free)
{
  UnitSettings *units = ptr->data;
  return rna_UnitSettings_itemf_wrapper(units->system, B_UNIT_TIME, r_free);
}

static void rna_UnitSettings_system_update(Main *UNUSED(bmain),
                                           Scene *scene,
                                           PointerRNA *UNUSED(ptr))
{
  UnitSettings *unit = &scene->unit;
  if (unit->system == USER_UNIT_NONE) {
    unit->length_unit = USER_UNIT_ADAPTIVE;
    unit->mass_unit = USER_UNIT_ADAPTIVE;
  }
  else {
    unit->length_unit = bUnit_GetBaseUnitOfType(unit->system, B_UNIT_LENGTH);
    unit->mass_unit = bUnit_GetBaseUnitOfType(unit->system, B_UNIT_MASS);
  }
}

static char *rna_UnitSettings_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("unit_settings");
}

#else

/* Grease Pencil Interpolation tool settings */
static void rna_def_gpencil_interpolate(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilInterpolateSettings", NULL);
  RNA_def_struct_sdna(srna, "GP_Interpolate_Settings");
  RNA_def_struct_path_func(srna, "rna_GPencilInterpolateSettings_path");
  RNA_def_struct_ui_text(srna,
                         "Grease Pencil Interpolate Settings",
                         "Settings for Grease Pencil interpolation tools");

  /* flags */
  prop = RNA_def_property(srna, "interpolate_all_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TOOLFLAG_INTERPOLATE_ALL_LAYERS);
  RNA_def_property_ui_text(
      prop, "Interpolate All Layers", "Interpolate all layers, not only active");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  prop = RNA_def_property(srna, "interpolate_selected_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TOOLFLAG_INTERPOLATE_ONLY_SELECTED);
  RNA_def_property_ui_text(prop,
                           "Interpolate Selected Strokes",
                           "Interpolate only selected strokes in the original frame");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  /* interpolation type */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_interpolation_mode_items);
  RNA_def_property_enum_funcs(prop, NULL, "rna_GPencilInterpolateSettings_type_set", NULL);
  RNA_def_property_ui_text(
      prop, "Type", "Interpolation method to use the next time 'Interpolate Sequence' is run");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  /* easing */
  prop = RNA_def_property(srna, "easing", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "easing");
  RNA_def_property_enum_items(prop, rna_enum_beztriple_interpolation_easing_items);
  RNA_def_property_ui_text(
      prop,
      "Easing",
      "Which ends of the segment between the preceding and following grease pencil frames "
      "easing interpolation is applied to");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  /* easing options */
  prop = RNA_def_property(srna, "back", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "back");
  RNA_def_property_ui_text(prop, "Back", "Amount of overshoot for 'back' easing");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  prop = RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "amplitude");
  RNA_def_property_range(prop, 0.0f, FLT_MAX); /* only positive values... */
  RNA_def_property_ui_text(
      prop, "Amplitude", "Amount to boost elastic bounces for 'elastic' easing");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  prop = RNA_def_property(srna, "period", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "period");
  RNA_def_property_ui_text(prop, "Period", "Time between bounces for elastic easing");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  /* custom curvemap */
  prop = RNA_def_property(srna, "interpolation_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "custom_ipo");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(
      prop,
      "Interpolation Curve",
      "Custom curve to control 'sequence' interpolation between Grease Pencil frames");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);
}

static void rna_def_transform_orientation(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "TransformOrientation", NULL);

  prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, NULL, "mat");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_3x3);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_ui_text(prop, "Name", "Name of the custom transform orientation");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);
}

static void rna_def_transform_orientation_slot(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "TransformOrientationSlot", NULL);
  RNA_def_struct_sdna(srna, "TransformOrientationSlot");
  RNA_def_struct_ui_text(srna, "Orientation Slot", "");

  /* Orientations */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_transform_orientation_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_TransformOrientationSlot_type_get",
                              "rna_TransformOrientationSlot_type_set",
                              "rna_TransformOrientation_with_scene_itemf");
  RNA_def_property_ui_text(prop, "Orientation", "Transformation orientation");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "custom_orientation", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "TransformOrientation");
  RNA_def_property_pointer_funcs(prop, "rna_TransformOrientationSlot_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Current Transform Orientation", "");

  /* flag */
  prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
  RNA_def_property_ui_text(prop, "Use", "Use scene orientation instead of a custom setting");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);
}

static void rna_def_view3d_cursor(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "View3DCursor", NULL);
  RNA_def_struct_sdna(srna, "View3DCursor");
  RNA_def_struct_path_func(srna, "rna_View3DCursor_path");
  RNA_def_struct_ui_text(srna, "3D Cursor", "");

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ_LENGTH);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, NULL, "location");
  RNA_def_property_ui_text(prop, "Location", "");
  RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 4);
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  prop = RNA_def_property(srna, "rotation_quaternion", PROP_FLOAT, PROP_QUATERNION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, NULL, "rotation_quaternion");
  RNA_def_property_float_array_default(prop, rna_default_quaternion);
  RNA_def_property_ui_text(
      prop, "Quaternion Rotation", "Rotation in quaternions (keep normalized)");
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  prop = RNA_def_property(srna, "rotation_axis_angle", PROP_FLOAT, PROP_AXISANGLE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_funcs(prop,
                               "rna_View3DCursor_rotation_axis_angle_get",
                               "rna_View3DCursor_rotation_axis_angle_set",
                               NULL);
  RNA_def_property_float_array_default(prop, rna_default_axis_angle);
  RNA_def_property_ui_text(
      prop, "Axis-Angle Rotation", "Angle of Rotation for Axis-Angle rotation representation");
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  prop = RNA_def_property(srna, "rotation_euler", PROP_FLOAT, PROP_EULER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, NULL, "rotation_euler");
  RNA_def_property_ui_text(prop, "Euler Rotation", "3D rotation");
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  prop = RNA_def_property(srna, "rotation_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_sdna(prop, NULL, "rotation_mode");
  RNA_def_property_enum_items(prop, rna_enum_object_rotation_mode_items);
  RNA_def_property_enum_funcs(prop, NULL, "rna_View3DCursor_rotation_mode_set", NULL);
  RNA_def_property_ui_text(prop, "Rotation Mode", "");
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  /* Matrix access to avoid having to check current rotation mode. */
  prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_flag(prop, PROP_THICK_WRAP); /* no reference to original data */
  RNA_def_property_ui_text(prop, "Transform Matrix", "Matrix combining loc/rot of the cursor");
  RNA_def_property_float_funcs(
      prop, "rna_View3DCursor_matrix_get", "rna_View3DCursor_matrix_set", NULL);
}

static void rna_def_tool_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* the construction of this enum is quite special - everything is stored as bitflags,
   * with 1st position only for for on/off (and exposed as boolean), while others are mutually
   * exclusive options but which will only have any effect when autokey is enabled
   */
  static const EnumPropertyItem auto_key_items[] = {
      {AUTOKEY_MODE_NORMAL & ~AUTOKEY_ON, "ADD_REPLACE_KEYS", 0, "Add & Replace", ""},
      {AUTOKEY_MODE_EDITKEYS & ~AUTOKEY_ON, "REPLACE_KEYS", 0, "Replace", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem draw_groupuser_items[] = {
      {OB_DRAW_GROUPUSER_NONE, "NONE", 0, "None", ""},
      {OB_DRAW_GROUPUSER_ACTIVE,
       "ACTIVE",
       0,
       "Active",
       "Show vertices with no weights in the active group"},
      {OB_DRAW_GROUPUSER_ALL, "ALL", 0, "All", "Show vertices with no weights in any group"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem vertex_group_select_items[] = {
      {WT_VGROUP_ALL, "ALL", 0, "All", "All Vertex Groups"},
      {WT_VGROUP_BONE_DEFORM,
       "BONE_DEFORM",
       0,
       "Deform",
       "Vertex Groups assigned to Deform Bones"},
      {WT_VGROUP_BONE_DEFORM_OFF,
       "OTHER_DEFORM",
       0,
       "Other",
       "Vertex Groups assigned to non Deform Bones"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem gpencil_stroke_placement_items[] = {
      {GP_PROJECT_VIEWSPACE,
       "ORIGIN",
       ICON_OBJECT_ORIGIN,
       "Origin",
       "Draw stroke at Object origin"},
      {GP_PROJECT_VIEWSPACE | GP_PROJECT_CURSOR,
       "CURSOR",
       ICON_PIVOT_CURSOR,
       "3D Cursor",
       "Draw stroke at 3D cursor location"},
      {GP_PROJECT_VIEWSPACE | GP_PROJECT_DEPTH_VIEW,
       "SURFACE",
       ICON_SNAP_FACE,
       "Surface",
       "Stick stroke to surfaces"},
      {GP_PROJECT_VIEWSPACE | GP_PROJECT_DEPTH_STROKE,
       "STROKE",
       ICON_STROKE,
       "Stroke",
       "Stick stroke to other strokes"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem gpencil_stroke_snap_items[] = {
      {0, "NONE", 0, "All points", "Snap to all points"},
      {GP_PROJECT_DEPTH_STROKE_ENDPOINTS,
       "ENDS",
       0,
       "End points",
       "Snap to first and last points and interpolate"},
      {GP_PROJECT_DEPTH_STROKE_FIRST, "FIRST", 0, "First point", "Snap to first point"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem gpencil_selectmode_items[] = {
      {GP_SELECTMODE_POINT, "POINT", ICON_GP_SELECT_POINTS, "Point", "Select only points"},
      {GP_SELECTMODE_STROKE,
       "STROKE",
       ICON_GP_SELECT_STROKES,
       "Stroke",
       "Select all stroke points"},
      {GP_SELECTMODE_SEGMENT,
       "SEGMENT",
       ICON_GP_SELECT_BETWEEN_STROKES,
       "Segment",
       "Select all stroke points between other strokes"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem annotation_stroke_placement_items[] = {
      {GP_PROJECT_VIEWSPACE | GP_PROJECT_CURSOR,
       "CURSOR",
       ICON_PIVOT_CURSOR,
       "3D Cursor",
       "Draw stroke at 3D cursor location"},
      /* Weird, GP_PROJECT_VIEWALIGN is inverted. */
      {0, "VIEW", ICON_RESTRICT_VIEW_ON, "View", "Stick stroke to the view "},
      {GP_PROJECT_VIEWSPACE | GP_PROJECT_DEPTH_VIEW,
       "SURFACE",
       ICON_FACESEL,
       "Surface",
       "Stick stroke to surfaces"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "ToolSettings", NULL);
  RNA_def_struct_path_func(srna, "rna_ToolSettings_path");
  RNA_def_struct_ui_text(srna, "Tool Settings", "");

  prop = RNA_def_property(srna, "sculpt", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Sculpt");
  RNA_def_property_ui_text(prop, "Sculpt", "");

  prop = RNA_def_property(srna, "use_auto_normalize", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_boolean_sdna(prop, NULL, "auto_normalize", 1);
  RNA_def_property_ui_text(prop,
                           "WPaint Auto-Normalize",
                           "Ensure all bone-deforming vertex groups add up "
                           "to 1.0 while weight painting");
  RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

  prop = RNA_def_property(srna, "use_multipaint", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_boolean_sdna(prop, NULL, "multipaint", 1);
  RNA_def_property_ui_text(prop,
                           "WPaint Multi-Paint",
                           "Paint across the weights of all selected bones, "
                           "maintaining their relative influence");
  RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

  prop = RNA_def_property(srna, "vertex_group_user", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_enum_sdna(prop, NULL, "weightuser");
  RNA_def_property_enum_items(prop, draw_groupuser_items);
  RNA_def_property_ui_text(prop, "Mask Non-Group Vertices", "Display unweighted vertices");
  RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

  prop = RNA_def_property(srna, "vertex_group_subset", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_enum_sdna(prop, NULL, "vgroupsubset");
  RNA_def_property_enum_items(prop, vertex_group_select_items);
  RNA_def_property_ui_text(prop, "Subset", "Filter Vertex groups for Display");
  RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

  prop = RNA_def_property(srna, "vertex_paint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "vpaint");
  RNA_def_property_ui_text(prop, "Vertex Paint", "");

  prop = RNA_def_property(srna, "weight_paint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "wpaint");
  RNA_def_property_ui_text(prop, "Weight Paint", "");

  prop = RNA_def_property(srna, "image_paint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "imapaint");
  RNA_def_property_ui_text(prop, "Image Paint", "");

  prop = RNA_def_property(srna, "uv_sculpt", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "uvsculpt");
  RNA_def_property_ui_text(prop, "UV Sculpt", "");

  prop = RNA_def_property(srna, "gpencil_paint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "gp_paint");
  RNA_def_property_ui_text(prop, "Grease Pencil Paint", "");

  prop = RNA_def_property(srna, "particle_edit", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "particle");
  RNA_def_property_ui_text(prop, "Particle Edit", "");

  prop = RNA_def_property(srna, "uv_sculpt_lock_borders", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uv_sculpt_settings", UV_SCULPT_LOCK_BORDERS);
  RNA_def_property_ui_text(prop, "Lock Borders", "Disable editing of boundary edges");

  prop = RNA_def_property(srna, "uv_sculpt_all_islands", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uv_sculpt_settings", UV_SCULPT_ALL_ISLANDS);
  RNA_def_property_ui_text(prop, "Sculpt All Islands", "Brush operates on all islands");

  prop = RNA_def_property(srna, "uv_relax_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "uv_relax_method");
  RNA_def_property_enum_items(prop, uv_sculpt_relaxation_items);
  RNA_def_property_ui_text(prop, "Relaxation Method", "Algorithm used for UV relaxation");

  prop = RNA_def_property(srna, "lock_object_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "object_flag", SCE_OBJECT_MODE_LOCK);
  RNA_def_property_ui_text(prop, "Lock Object Modes", "Restrict select to the current mode");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  /* Transform */
  prop = RNA_def_property(srna, "use_proportional_edit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "proportional_edit", PROP_EDIT_USE);
  RNA_def_property_ui_text(prop, "Proportional Editing", "Proportional edit mode");
  RNA_def_property_ui_icon(prop, ICON_PROP_ON, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_edit_objects", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "proportional_objects", 0);
  RNA_def_property_ui_text(
      prop, "Proportional Editing Objects", "Proportional editing object mode");
  RNA_def_property_ui_icon(prop, ICON_PROP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_projected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "proportional_edit", PROP_EDIT_PROJECTED);
  RNA_def_property_ui_text(
      prop, "Projected from View", "Proportional Editing using screen space locations");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_connected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "proportional_edit", PROP_EDIT_CONNECTED);
  RNA_def_property_ui_text(
      prop, "Connected Only", "Proportional Editing using connected geometry only");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_edit_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "proportional_mask", 0);
  RNA_def_property_ui_text(prop, "Proportional Editing Objects", "Proportional editing mask mode");
  RNA_def_property_ui_icon(prop, ICON_PROP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_action", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "proportional_action", 0);
  RNA_def_property_ui_text(
      prop, "Proportional Editing Actions", "Proportional editing in action editor");
  RNA_def_property_ui_icon(prop, ICON_PROP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_fcurve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "proportional_fcurve", 0);
  RNA_def_property_ui_text(
      prop, "Proportional Editing FCurves", "Proportional editing in FCurve editor");
  RNA_def_property_ui_icon(prop, ICON_PROP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "lock_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "lock_markers", 0);
  RNA_def_property_ui_text(prop, "Lock Markers", "Prevent marker editing");

  prop = RNA_def_property(srna, "proportional_edit_falloff", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "prop_mode");
  RNA_def_property_enum_items(prop, rna_enum_proportional_falloff_items);
  RNA_def_property_ui_text(
      prop, "Proportional Editing Falloff", "Falloff type for proportional editing mode");
  /* Abusing id_curve :/ */
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "proportional_size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "proportional_size");
  RNA_def_property_ui_text(
      prop, "Proportional Size", "Display size for proportional editing circle");
  RNA_def_property_range(prop, 0.00001, 5000.0);

  prop = RNA_def_property(srna, "double_threshold", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "doublimit");
  RNA_def_property_ui_text(prop, "Merge Threshold", "Threshold distance for Auto Merge");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 0.1, 0.01, 6);

  /* Pivot Point */
  prop = RNA_def_property(srna, "transform_pivot_point", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "transform_pivot_point");
  RNA_def_property_enum_items(prop, rna_enum_transform_pivot_items_full);
  RNA_def_property_ui_text(prop, "Pivot Point", "Pivot center for rotation/scaling");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  prop = RNA_def_property(srna, "use_transform_pivot_point_align", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "transform_flag", SCE_XFORM_AXIS_ALIGN);
  RNA_def_property_ui_text(
      prop, "Only Origins", "Manipulate center points (object, pose and weight paint mode only)");
  RNA_def_property_ui_icon(prop, ICON_CENTER_ONLY, 0);
  RNA_def_property_update(prop, NC_SCENE, NULL);

  prop = RNA_def_property(srna, "use_mesh_automerge", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "automerge", 0);
  RNA_def_property_ui_text(
      prop, "Auto Merge", "Automatically merge vertices moved to the same location");
  RNA_def_property_ui_icon(prop, ICON_AUTOMERGE_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_snap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "snap_flag", SCE_SNAP);
  RNA_def_property_ui_text(prop, "Snap", "Snap during transform");
  RNA_def_property_ui_icon(prop, ICON_SNAP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_align_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "snap_flag", SCE_SNAP_ROTATE);
  RNA_def_property_ui_text(
      prop, "Align Rotation to Target", "Align rotation with the snapping target");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_grid_absolute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "snap_flag", SCE_SNAP_ABS_GRID);
  RNA_def_property_ui_text(
      prop,
      "Absolute Grid Snap",
      "Absolute grid alignment while translating (based on the pivot center)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "snap_elements", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "snap_mode");
  RNA_def_property_enum_items(prop, rna_enum_snap_element_items);
  RNA_def_property_enum_funcs(prop, NULL, "rna_ToolSettings_snap_mode_set", NULL);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_ui_text(prop, "Snap Element", "Type of element to snap to");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  /* node editor uses own set of snap modes */
  prop = RNA_def_property(srna, "snap_node_element", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "snap_node_mode");
  RNA_def_property_enum_items(prop, rna_enum_snap_node_element_items);
  RNA_def_property_ui_text(prop, "Snap Node Element", "Type of element to snap to");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  /* image editor uses own set of snap modes */
  prop = RNA_def_property(srna, "snap_uv_element", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "snap_uv_mode");
  RNA_def_property_enum_items(prop, snap_uv_element_items);
  RNA_def_property_ui_text(prop, "Snap UV Element", "Type of element to snap to");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "snap_target", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "snap_target");
  RNA_def_property_enum_items(prop, rna_enum_snap_target_items);
  RNA_def_property_ui_text(prop, "Snap Target", "Which part to snap onto the target");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_peel_object", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "snap_flag", SCE_SNAP_PEEL_OBJECT);
  RNA_def_property_ui_text(
      prop, "Snap Peel Object", "Consider objects as whole when finding volume center");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_project", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "snap_flag", SCE_SNAP_PROJECT);
  RNA_def_property_ui_text(prop,
                           "Project Individual Elements",
                           "Project individual elements on the surface of other objects");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_self", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "snap_flag", SCE_SNAP_NO_SELF);
  RNA_def_property_ui_text(prop, "Project onto Self", "Snap onto itself (Edit Mode Only)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_translate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, NULL, "snap_transform_mode_flag", SCE_SNAP_TRANSFORM_MODE_TRANSLATE);
  RNA_def_property_ui_text(
      prop, "Use Snap for Translation", "Move is affected by snapping settings");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_rotate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, NULL, "snap_transform_mode_flag", SCE_SNAP_TRANSFORM_MODE_ROTATE);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(
      prop, "Use Snap for Rotation", "Rotate is affected by the snapping settings");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, NULL, "snap_transform_mode_flag", SCE_SNAP_TRANSFORM_MODE_SCALE);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(prop, "Use Snap for Scale", "Scale is affected by snapping settings");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

  /* Grease Pencil */
  prop = RNA_def_property(srna, "use_gpencil_draw_additive", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gpencil_flags", GP_TOOL_FLAG_RETAIN_LAST);
  RNA_def_property_ui_text(prop,
                           "Use Additive Drawing",
                           "When creating new frames, the strokes from the previous/active frame "
                           "are included as the basis for the new one");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  prop = RNA_def_property(srna, "use_gpencil_draw_onback", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gpencil_flags", GP_TOOL_FLAG_PAINT_ONBACK);
  RNA_def_property_ui_text(
      prop,
      "Draw Strokes on Back",
      "When draw new strokes, the new stroke is drawn below of all strokes in the layer");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  prop = RNA_def_property(srna, "use_gpencil_thumbnail_list", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "gpencil_flags", GP_TOOL_FLAG_THUMBNAIL_LIST);
  RNA_def_property_ui_text(
      prop, "Compact List", "Show compact list of color instead of thumbnails");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  prop = RNA_def_property(srna, "use_gpencil_weight_data_add", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gpencil_flags", GP_TOOL_FLAG_CREATE_WEIGHTS);
  RNA_def_property_ui_text(prop,
                           "Add weight data for new strokes",
                           "When creating new strokes, the weight data is added according to the "
                           "current vertex group and weight, "
                           "if no vertex group selected, weight is not added");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

  prop = RNA_def_property(srna, "gpencil_sculpt", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "gp_sculpt");
  RNA_def_property_struct_type(prop, "GPencilSculptSettings");
  RNA_def_property_ui_text(
      prop, "Grease Pencil Sculpt", "Settings for stroke sculpting tools and brushes");

  prop = RNA_def_property(srna, "gpencil_interpolate", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "gp_interpolate");
  RNA_def_property_struct_type(prop, "GPencilInterpolateSettings");
  RNA_def_property_ui_text(
      prop, "Grease Pencil Interpolate", "Settings for Grease Pencil Interpolation tools");

  /* Grease Pencil - 3D View Stroke Placement */
  prop = RNA_def_property(srna, "gpencil_stroke_placement_view3d", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "gpencil_v3d_align");
  RNA_def_property_enum_items(prop, gpencil_stroke_placement_items);
  RNA_def_property_ui_text(prop, "Stroke Placement (3D View)", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);

  prop = RNA_def_property(srna, "gpencil_stroke_snap_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "gpencil_v3d_align");
  RNA_def_property_enum_items(prop, gpencil_stroke_snap_items);
  RNA_def_property_ui_text(prop, "Stroke Snap", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);

  prop = RNA_def_property(srna, "use_gpencil_stroke_endpoints", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, NULL, "gpencil_v3d_align", GP_PROJECT_DEPTH_STROKE_ENDPOINTS);
  RNA_def_property_ui_text(
      prop, "Only Endpoints", "Only use the first and last parts of the stroke for snapping");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);

  /* Grease Pencil - Select mode */
  prop = RNA_def_property(srna, "gpencil_selectmode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "gpencil_selectmode");
  RNA_def_property_enum_items(prop, gpencil_selectmode_items);
  RNA_def_property_ui_text(prop, "Select Mode", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  /* Annotations - 2D Views Stroke Placement */
  prop = RNA_def_property(srna, "annotation_stroke_placement_view2d", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "gpencil_v2d_align");
  RNA_def_property_enum_items(prop, annotation_stroke_placement_items);
  RNA_def_property_ui_text(prop, "Stroke Placement (2D View)", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);

  /* Annotations - Sequencer Preview Stroke Placement */
  prop = RNA_def_property(
      srna, "annotation_stroke_placement_sequencer_preview", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "gpencil_seq_align");
  RNA_def_property_enum_items(prop, annotation_stroke_placement_items);
  RNA_def_property_ui_text(prop, "Stroke Placement (Sequencer Preview)", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);

  /* Annotations - Image Editor Stroke Placement */
  prop = RNA_def_property(srna, "annotation_stroke_placement_image_editor", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "gpencil_ima_align");
  RNA_def_property_enum_items(prop, annotation_stroke_placement_items);
  RNA_def_property_ui_text(prop, "Stroke Placement (Image Editor)", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);

  /* Annotations - 3D View Stroke Placement */
  /* XXX: Do we need to decouple the stroke_endpoints setting too?  */
  prop = RNA_def_property(srna, "annotation_stroke_placement_view3d", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "annotate_v3d_align");
  RNA_def_property_enum_items(prop, annotation_stroke_placement_items);
  RNA_def_property_enum_default(prop, GP_PROJECT_VIEWSPACE | GP_PROJECT_CURSOR);
  RNA_def_property_ui_text(prop,
                           "Annotation Stroke Placement (3D View)",
                           "How annotation strokes are orientated in 3D space");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);

  /* Annotations - Stroke Thickness */
  prop = RNA_def_property(srna, "annotation_thickness", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "annotate_thickness");
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_ui_text(prop, "Annotation Stroke Thickness", "Thickness of annotation strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* Auto Keying */
  prop = RNA_def_property(srna, "use_keyframe_insert_auto", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "autokey_mode", AUTOKEY_ON);
  RNA_def_property_ui_text(
      prop, "Auto Keying", "Automatic keyframe insertion for Objects and Bones");
  RNA_def_property_ui_icon(prop, ICON_REC, 0);

  prop = RNA_def_property(srna, "auto_keying_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "autokey_mode");
  RNA_def_property_enum_items(prop, auto_key_items);
  RNA_def_property_ui_text(
      prop, "Auto-Keying Mode", "Mode of automatic keyframe insertion for Objects and Bones");

  prop = RNA_def_property(srna, "use_record_with_nla", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "autokey_flag", ANIMRECORD_FLAG_WITHNLA);
  RNA_def_property_ui_text(
      prop,
      "Layered",
      "Add a new NLA Track + Strip for every loop/pass made over the animation "
      "to allow non-destructive tweaking");

  prop = RNA_def_property(srna, "use_keyframe_insert_keyingset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "autokey_flag", AUTOKEY_FLAG_ONLYKEYINGSET);
  RNA_def_property_ui_text(prop,
                           "Auto Keyframe Insert Keying Set",
                           "Automatic keyframe insertion using active Keying Set only");
  RNA_def_property_ui_icon(prop, ICON_KEYINGSET, 0);

  prop = RNA_def_property(srna, "use_keyframe_cycle_aware", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "autokey_flag", AUTOKEY_FLAG_CYCLEAWARE);
  RNA_def_property_ui_text(
      prop,
      "Cycle-Aware Keying",
      "For channels with cyclic extrapolation, keyframe insertion is automatically "
      "remapped inside the cycle time range, and keeps ends in sync");

  /* Keyframing */
  prop = RNA_def_property(srna, "keyframe_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "keyframe_type");
  RNA_def_property_enum_items(prop, rna_enum_beztriple_keyframe_type_items);
  RNA_def_property_ui_text(
      prop, "New Keyframe Type", "Type of keyframes to create when inserting keyframes");

  /* UV */
  prop = RNA_def_property(srna, "uv_select_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "uv_selectmode");
  RNA_def_property_enum_items(prop, rna_enum_mesh_select_mode_uv_items);
  RNA_def_property_ui_text(prop, "UV Selection Mode", "UV selection and display mode");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "use_uv_select_sync", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uv_flag", UV_SYNC_SELECTION);
  RNA_def_property_ui_text(
      prop, "UV Sync Selection", "Keep UV and edit mode mesh selection in sync");
  RNA_def_property_ui_icon(prop, ICON_UV_SYNC_SELECT, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "show_uv_local_view", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uv_flag", UV_SHOW_SAME_IMAGE);
  RNA_def_property_ui_text(
      prop, "UV Local View", "Display only faces with the currently displayed image assigned");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  /* Mesh */
  prop = RNA_def_property(srna, "mesh_select_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "selectmode", 1);
  RNA_def_property_array(prop, 3);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_Scene_editmesh_select_mode_set");
  RNA_def_property_ui_text(prop, "Mesh Selection Mode", "Which mesh elements selection works on");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_Scene_editmesh_select_mode_update");

  prop = RNA_def_property(srna, "vertex_group_weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "vgroup_weight");
  RNA_def_property_ui_text(prop, "Vertex Group Weight", "Weight to assign in vertex groups");

  prop = RNA_def_property(srna, "use_edge_path_live_unwrap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_mode_live_unwrap", 1);
  RNA_def_property_ui_text(prop, "Live Unwrap", "Changing edges seam re-calculates UV unwrap");

  prop = RNA_def_property(srna, "normal_vector", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_ui_text(prop, "Normal Vector", "Normal Vector used to copy, add or multiply");
  RNA_def_property_ui_range(prop, -10000.0, 10000.0, 1, 3);

  /* Unified Paint Settings */
  prop = RNA_def_property(srna, "unified_paint_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "UnifiedPaintSettings");
  RNA_def_property_ui_text(prop, "Unified Paint Settings", NULL);

  /* Curve Paint Settings */
  prop = RNA_def_property(srna, "curve_paint_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "CurvePaintSettings");
  RNA_def_property_ui_text(prop, "Curve Paint Settings", NULL);

  /* Mesh Statistics */
  prop = RNA_def_property(srna, "statvis", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "MeshStatVis");
  RNA_def_property_ui_text(prop, "Mesh Statistics Visualization", NULL);
}

static void rna_def_unified_paint_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem brush_size_unit_items[] = {
      {0, "VIEW", 0, "View", "Measure brush size relateve to the view"},
      {UNIFIED_PAINT_BRUSH_LOCK_SIZE,
       "SCENE",
       0,
       "Scene",
       "Measure brush size relateve to the scene"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "UnifiedPaintSettings", NULL);
  RNA_def_struct_path_func(srna, "rna_UnifiedPaintSettings_path");
  RNA_def_struct_ui_text(
      srna, "Unified Paint Settings", "Overrides for some of the active brush's settings");

  /* high-level flags to enable or disable unified paint settings */
  prop = RNA_def_property(srna, "use_unified_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_SIZE);
  RNA_def_property_ui_text(prop,
                           "Use Unified Radius",
                           "Instead of per-brush radius, the radius is shared across brushes");

  prop = RNA_def_property(srna, "use_unified_strength", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_ALPHA);
  RNA_def_property_ui_text(prop,
                           "Use Unified Strength",
                           "Instead of per-brush strength, the strength is shared across brushes");

  prop = RNA_def_property(srna, "use_unified_weight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_WEIGHT);
  RNA_def_property_ui_text(prop,
                           "Use Unified Weight",
                           "Instead of per-brush weight, the weight is shared across brushes");

  prop = RNA_def_property(srna, "use_unified_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_COLOR);
  RNA_def_property_ui_text(
      prop, "Use Unified Color", "Instead of per-brush color, the color is shared across brushes");

  /* unified paint settings that override the equivalent settings
   * from the active brush */
  prop = RNA_def_property(srna, "size", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_funcs(prop, NULL, "rna_UnifiedPaintSettings_size_set", NULL);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 1, MAX_BRUSH_PIXEL_RADIUS * 10);
  RNA_def_property_ui_range(prop, 1, MAX_BRUSH_PIXEL_RADIUS, 1, -1);
  RNA_def_property_ui_text(prop, "Radius", "Radius of the brush");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_radius_update");

  prop = RNA_def_property(srna, "unprojected_radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_funcs(
      prop, NULL, "rna_UnifiedPaintSettings_unprojected_radius_set", NULL);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 0.001, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001, 1, 0, -1);
  RNA_def_property_ui_text(prop, "Unprojected Radius", "Radius of brush in Blender units");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_radius_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "alpha");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(
      prop, "Strength", "How powerful the effect of the brush is when applied");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "weight");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(prop, "Weight", "Weight to assign in vertex groups");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, NULL, "rgb");
  RNA_def_property_ui_text(prop, "Color", "");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "secondary_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, NULL, "secondary_rgb");
  RNA_def_property_ui_text(prop, "Secondary Color", "");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "use_pressure_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_BRUSH_SIZE_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Size Pressure", "Enable tablet pressure sensitivity for size");

  prop = RNA_def_property(srna, "use_pressure_strength", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_BRUSH_ALPHA_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(
      prop, "Strength Pressure", "Enable tablet pressure sensitivity for strength");

  prop = RNA_def_property(srna, "use_locked_size", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, brush_size_unit_items);
  RNA_def_property_ui_text(
      prop, "Radius Unit", "Measure brush size relative to the view or the scene ");
}

static void rna_def_curve_paint_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CurvePaintSettings", NULL);
  RNA_def_struct_path_func(srna, "rna_CurvePaintSettings_path");
  RNA_def_struct_ui_text(srna, "Curve Paint Settings", "");

  static const EnumPropertyItem curve_type_items[] = {
      {CU_POLY, "POLY", 0, "Poly", ""},
      {CU_BEZIER, "BEZIER", 0, "Bezier", ""},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "curve_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "curve_type");
  RNA_def_property_enum_items(prop, curve_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of curve to use for new strokes");

  prop = RNA_def_property(srna, "use_corners_detect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", CURVE_PAINT_FLAG_CORNERS_DETECT);
  RNA_def_property_ui_text(prop, "Detect Corners", "Detect corners and use non-aligned handles");

  prop = RNA_def_property(srna, "use_pressure_radius", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", CURVE_PAINT_FLAG_PRESSURE_RADIUS);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Map tablet pressure to curve radius");

  prop = RNA_def_property(srna, "use_stroke_endpoints", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", CURVE_PAINT_FLAG_DEPTH_STROKE_ENDPOINTS);
  RNA_def_property_ui_text(prop, "Only First", "Use the start of the stroke for the depth");

  prop = RNA_def_property(srna, "use_offset_absolute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", CURVE_PAINT_FLAG_DEPTH_STROKE_OFFSET_ABS);
  RNA_def_property_ui_text(
      prop, "Absolute Offset", "Apply a fixed offset (don't scale by the radius)");

  prop = RNA_def_property(srna, "error_threshold", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_text(prop, "Tolerance", "Allow deviation for a smoother, less precise line");

  prop = RNA_def_property(srna, "fit_method", PROP_ENUM, PROP_PIXEL);
  RNA_def_property_enum_sdna(prop, NULL, "fit_method");
  RNA_def_property_enum_items(prop, rna_enum_curve_fit_method_items);
  RNA_def_property_ui_text(prop, "Method", "Curve fitting method");

  prop = RNA_def_property(srna, "corner_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, 0, M_PI);
  RNA_def_property_ui_text(prop, "Corner Angle", "Angles above this are considered corners");

  prop = RNA_def_property(srna, "radius_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0f, 10.0, 10, 2);
  RNA_def_property_ui_text(
      prop,
      "Radius Min",
      "Minimum radius when the minimum pressure is applied (also the minimum when tapering)");

  prop = RNA_def_property(srna, "radius_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0f, 10.0, 10, 2);
  RNA_def_property_ui_text(
      prop,
      "Radius Max",
      "Radius to use when the maximum pressure is applied (or when a tablet isn't used)");

  prop = RNA_def_property(srna, "radius_taper_start", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0f, 1.0, 1, 2);
  RNA_def_property_ui_text(
      prop, "Radius Min", "Taper factor for the radius of each point along the curve");

  prop = RNA_def_property(srna, "radius_taper_end", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_range(prop, 0.0f, 1.0, 1, 2);
  RNA_def_property_ui_text(
      prop, "Radius Max", "Taper factor for the radius of each point along the curve");

  prop = RNA_def_property(srna, "surface_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -10.0, 10.0);
  RNA_def_property_ui_range(prop, -1.0f, 1.0, 1, 2);
  RNA_def_property_ui_text(prop, "Offset", "Offset the stroke from the surface");

  static const EnumPropertyItem depth_mode_items[] = {
      {CURVE_PAINT_PROJECT_CURSOR, "CURSOR", 0, "Cursor", ""},
      {CURVE_PAINT_PROJECT_SURFACE, "SURFACE", 0, "Surface", ""},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "depth_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "depth_mode");
  RNA_def_property_enum_items(prop, depth_mode_items);
  RNA_def_property_ui_text(prop, "Depth", "Method of projecting depth");

  static const EnumPropertyItem surface_plane_items[] = {
      {CURVE_PAINT_SURFACE_PLANE_NORMAL_VIEW,
       "NORMAL_VIEW",
       0,
       "Normal/View",
       "Display perpendicular to the surface"},
      {CURVE_PAINT_SURFACE_PLANE_NORMAL_SURFACE,
       "NORMAL_SURFACE",
       0,
       "Normal/Surface",
       "Display aligned to the surface"},
      {CURVE_PAINT_SURFACE_PLANE_VIEW, "VIEW", 0, "View", "Display aligned to the viewport"},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "surface_plane", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "surface_plane");
  RNA_def_property_enum_items(prop, surface_plane_items);
  RNA_def_property_ui_text(prop, "Plane", "Plane for projected stroke");
}

static void rna_def_statvis(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem stat_type[] = {
      {SCE_STATVIS_OVERHANG, "OVERHANG", 0, "Overhang", ""},
      {SCE_STATVIS_THICKNESS, "THICKNESS", 0, "Thickness", ""},
      {SCE_STATVIS_INTERSECT, "INTERSECT", 0, "Intersect", ""},
      {SCE_STATVIS_DISTORT, "DISTORT", 0, "Distortion", ""},
      {SCE_STATVIS_SHARP, "SHARP", 0, "Sharp", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "MeshStatVis", NULL);
  RNA_def_struct_path_func(srna, "rna_MeshStatVis_path");
  RNA_def_struct_ui_text(srna, "Mesh Visualize Statistics", "");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, stat_type);
  RNA_def_property_ui_text(prop, "Type", "Type of data to visualize/check");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  /* overhang */
  prop = RNA_def_property(srna, "overhang_min", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "overhang_min");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 0.001, 3);
  RNA_def_property_ui_text(prop, "Overhang Min", "Minimum angle to display");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "overhang_max", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "overhang_max");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "Overhang Max", "Maximum angle to display");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "overhang_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "overhang_axis");
  RNA_def_property_enum_items(prop, rna_enum_object_axis_items);
  RNA_def_property_ui_text(prop, "Axis", "");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  /* thickness */
  prop = RNA_def_property(srna, "thickness_min", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "thickness_min");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 1000.0);
  RNA_def_property_ui_range(prop, 0.0f, 100.0, 0.001, 3);
  RNA_def_property_ui_text(prop, "Thickness Min", "Minimum for measuring thickness");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "thickness_max", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "thickness_max");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 1000.0);
  RNA_def_property_ui_range(prop, 0.0f, 100.0, 0.001, 3);
  RNA_def_property_ui_text(prop, "Thickness Max", "Maximum for measuring thickness");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "thickness_samples", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "thickness_samples");
  RNA_def_property_range(prop, 1, 32);
  RNA_def_property_ui_text(prop, "Samples", "Number of samples to test per face");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  /* distort */
  prop = RNA_def_property(srna, "distort_min", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "distort_min");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "Distort Min", "Minimum angle to display");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "distort_max", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "distort_max");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "Distort Max", "Maximum angle to display");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  /* sharp */
  prop = RNA_def_property(srna, "sharp_min", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "sharp_min");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "Distort Min", "Minimum angle to display");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "sharp_max", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "sharp_max");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "Distort Max", "Maximum angle to display");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");
}

static void rna_def_unit_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem unit_systems[] = {
      {USER_UNIT_NONE, "NONE", 0, "None", ""},
      {USER_UNIT_METRIC, "METRIC", 0, "Metric", ""},
      {USER_UNIT_IMPERIAL, "IMPERIAL", 0, "Imperial", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem rotation_units[] = {
      {0, "DEGREES", 0, "Degrees", "Use degrees for measuring angles and rotations"},
      {USER_UNIT_ROT_RADIANS, "RADIANS", 0, "Radians", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "UnitSettings", NULL);
  RNA_def_struct_ui_text(srna, "Unit Settings", "");
  RNA_def_struct_nested(brna, srna, "Scene");
  RNA_def_struct_path_func(srna, "rna_UnitSettings_path");

  /* Units */
  prop = RNA_def_property(srna, "system", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, unit_systems);
  RNA_def_property_ui_text(
      prop, "Unit System", "The unit system to use for user interface controls");
  RNA_def_property_update(prop, NC_WINDOW, "rna_UnitSettings_system_update");

  prop = RNA_def_property(srna, "system_rotation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rotation_units);
  RNA_def_property_ui_text(
      prop, "Rotation Units", "Unit to use for displaying/editing rotation values");
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  prop = RNA_def_property(srna, "scale_length", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_ui_text(
      prop,
      "Unit Scale",
      "Scale to use when converting between blender units and dimensions."
      " When working at microscopic or astronomical scale, a small or large unit scale"
      " respectively can be used to avoid numerical precision problems");
  RNA_def_property_range(prop, 0.00001, 100000.0);
  RNA_def_property_ui_range(prop, 0.001, 100.0, 0.1, 6);
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  prop = RNA_def_property(srna, "use_separate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_UNIT_OPT_SPLIT);
  RNA_def_property_ui_text(prop, "Separate Units", "Display units in pairs (e.g. 1m 0cm)");
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  prop = RNA_def_property(srna, "length_unit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, DummyRNA_DEFAULT_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_UnitSettings_length_unit_itemf");
  RNA_def_property_ui_text(prop, "Length Unit", "Unit that will be used to display length values");
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  prop = RNA_def_property(srna, "mass_unit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, DummyRNA_DEFAULT_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_UnitSettings_mass_unit_itemf");
  RNA_def_property_ui_text(prop, "Mass Unit", "Unit that will be used to display mass values");
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  prop = RNA_def_property(srna, "time_unit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, DummyRNA_DEFAULT_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_UnitSettings_time_unit_itemf");
  RNA_def_property_ui_text(prop, "Time Unit", "Unit that will be used to display time values");
  RNA_def_property_update(prop, NC_WINDOW, NULL);
}

void rna_def_view_layer_common(StructRNA *srna, const bool scene)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  if (scene) {
    RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ViewLayer_name_set");
  }
  else {
    RNA_def_property_string_sdna(prop, NULL, "name");
  }
  RNA_def_property_ui_text(prop, "Name", "View layer name");
  RNA_def_struct_name_property(srna, prop);
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  if (scene) {
    prop = RNA_def_property(srna, "material_override", PROP_POINTER, PROP_NONE);
    RNA_def_property_pointer_sdna(prop, NULL, "mat_override");
    RNA_def_property_struct_type(prop, "Material");
    RNA_def_property_flag(prop, PROP_EDITABLE);
    RNA_def_property_ui_text(
        prop, "Material Override", "Material to override all other materials in this view layer");
    RNA_def_property_update(
        prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_material_override_update");

    prop = RNA_def_property(srna, "samples", PROP_INT, PROP_UNSIGNED);
    RNA_def_property_ui_text(prop,
                             "Samples",
                             "Override number of render samples for this view layer, "
                             "0 will use the scene setting");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

    prop = RNA_def_property(srna, "pass_alpha_threshold", PROP_FLOAT, PROP_FACTOR);
    RNA_def_property_ui_text(
        prop,
        "Alpha Threshold",
        "Z, Index, normal, UV and vector passes are only affected by surfaces with "
        "alpha transparency equal to or higher than this threshold");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }

  /* layer options */
  prop = RNA_def_property(srna, "use_zmask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_ZMASK);
  RNA_def_property_ui_text(prop, "Zmask", "Only render what's in front of the solid z values");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "invert_zmask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_NEG_ZMASK);
  RNA_def_property_ui_text(
      prop,
      "Zmask Negate",
      "For Zmask, only render what is behind solid z values instead of in front");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_all_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_ALL_Z);
  RNA_def_property_ui_text(
      prop, "All Z", "Fill in Z values for solid faces in invisible layers, for masking");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_solid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_SOLID);
  RNA_def_property_ui_text(prop, "Solid", "Render Solid faces in this Layer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_halo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_HALO);
  RNA_def_property_ui_text(prop, "Halo", "Render Halos in this Layer (on top of Solid)");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_ztransp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_ZTRA);
  RNA_def_property_ui_text(
      prop, "ZTransp", "Render Z-Transparent faces in this Layer (on top of Solid and Halos)");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_sky", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_SKY);
  RNA_def_property_ui_text(prop, "Sky", "Render Sky in this Layer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_ao", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_AO);
  RNA_def_property_ui_text(prop, "Ambient Occlusion", "Render Ambient Occlusion in this Layer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_edge_enhance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_EDGE);
  RNA_def_property_ui_text(
      prop, "Edge", "Render Edge-enhance in this Layer (only works for Solid faces)");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_strand", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_STRAND);
  RNA_def_property_ui_text(prop, "Strand", "Render Strands in this Layer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  /* passes */
  prop = RNA_def_property(srna, "use_pass_combined", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_COMBINED);
  RNA_def_property_ui_text(prop, "Combined", "Deliver full combined RGBA buffer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_Z);
  RNA_def_property_ui_text(prop, "Z", "Deliver Z values pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_vector", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_VECTOR);
  RNA_def_property_ui_text(prop, "Vector", "Deliver speed vector pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_normal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_NORMAL);
  RNA_def_property_ui_text(prop, "Normal", "Deliver normal pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_uv", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_UV);
  RNA_def_property_ui_text(prop, "UV", "Deliver texture UV pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_mist", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_MIST);
  RNA_def_property_ui_text(prop, "Mist", "Deliver mist factor pass (0.0-1.0)");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_object_index", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_INDEXOB);
  RNA_def_property_ui_text(prop, "Object Index", "Deliver object index pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_material_index", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_INDEXMA);
  RNA_def_property_ui_text(prop, "Material Index", "Deliver material index pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_SHADOW);
  RNA_def_property_ui_text(prop, "Shadow", "Deliver shadow pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_ambient_occlusion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_AO);
  RNA_def_property_ui_text(prop, "Ambient Occlusion", "Deliver Ambient Occlusion pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_emit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_EMIT);
  RNA_def_property_ui_text(prop, "Emit", "Deliver emission pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_environment", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_ENVIRONMENT);
  RNA_def_property_ui_text(prop, "Environment", "Deliver environment lighting pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_diffuse_direct", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_DIFFUSE_DIRECT);
  RNA_def_property_ui_text(prop, "Diffuse Direct", "Deliver diffuse direct pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_diffuse_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_DIFFUSE_INDIRECT);
  RNA_def_property_ui_text(prop, "Diffuse Indirect", "Deliver diffuse indirect pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_diffuse_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_DIFFUSE_COLOR);
  RNA_def_property_ui_text(prop, "Diffuse Color", "Deliver diffuse color pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_glossy_direct", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_GLOSSY_DIRECT);
  RNA_def_property_ui_text(prop, "Glossy Direct", "Deliver glossy direct pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_glossy_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_GLOSSY_INDIRECT);
  RNA_def_property_ui_text(prop, "Glossy Indirect", "Deliver glossy indirect pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_glossy_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_GLOSSY_COLOR);
  RNA_def_property_ui_text(prop, "Glossy Color", "Deliver glossy color pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_transmission_direct", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_TRANSM_DIRECT);
  RNA_def_property_ui_text(prop, "Transmission Direct", "Deliver transmission direct pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_transmission_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_TRANSM_INDIRECT);
  RNA_def_property_ui_text(prop, "Transmission Indirect", "Deliver transmission indirect pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_transmission_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_TRANSM_COLOR);
  RNA_def_property_ui_text(prop, "Transmission Color", "Deliver transmission color pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_subsurface_direct", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_SUBSURFACE_DIRECT);
  RNA_def_property_ui_text(prop, "Subsurface Direct", "Deliver subsurface direct pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_subsurface_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_SUBSURFACE_INDIRECT);
  RNA_def_property_ui_text(prop, "Subsurface Indirect", "Deliver subsurface indirect pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_subsurface_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_SUBSURFACE_COLOR);
  RNA_def_property_ui_text(prop, "Subsurface Color", "Deliver subsurface color pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }
}

static void rna_def_freestyle_modules(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "FreestyleModules");
  srna = RNA_def_struct(brna, "FreestyleModules", NULL);
  RNA_def_struct_sdna(srna, "FreestyleSettings");
  RNA_def_struct_ui_text(
      srna, "Style Modules", "A list of style modules (to be applied from top to bottom)");

  func = RNA_def_function(srna, "new", "rna_FreestyleSettings_module_add");
  RNA_def_function_ui_description(func,
                                  "Add a style module to scene render layer Freestyle settings");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_pointer(
      func, "module", "FreestyleModuleSettings", "", "Newly created style module");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_FreestyleSettings_module_remove");
  RNA_def_function_ui_description(
      func, "Remove a style module from scene render layer Freestyle settings");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "module", "FreestyleModuleSettings", "", "Style module to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_freestyle_linesets(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "Linesets");
  srna = RNA_def_struct(brna, "Linesets", NULL);
  RNA_def_struct_sdna(srna, "FreestyleSettings");
  RNA_def_struct_ui_text(
      srna, "Line Sets", "Line sets for associating lines and style parameters");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "FreestyleLineSet");
  RNA_def_property_pointer_funcs(
      prop, "rna_FreestyleSettings_active_lineset_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Active Line Set", "Active line set being displayed");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_FreestyleSettings_active_lineset_index_get",
                             "rna_FreestyleSettings_active_lineset_index_set",
                             "rna_FreestyleSettings_active_lineset_index_range");
  RNA_def_property_ui_text(prop, "Active Line Set Index", "Index of active line set slot");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  func = RNA_def_function(srna, "new", "rna_FreestyleSettings_lineset_add");
  RNA_def_function_ui_description(func, "Add a line set to scene render layer Freestyle settings");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_SELF_ID);
  parm = RNA_def_string(func, "name", "LineSet", 0, "", "New name for the line set (not unique)");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "lineset", "FreestyleLineSet", "", "Newly created line set");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_FreestyleSettings_lineset_remove");
  RNA_def_function_ui_description(func,
                                  "Remove a line set from scene render layer Freestyle settings");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "lineset", "FreestyleLineSet", "", "Line set to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

void rna_def_freestyle_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem edge_type_negation_items[] = {
      {0,
       "INCLUSIVE",
       0,
       "Inclusive",
       "Select feature edges satisfying the given edge type conditions"},
      {FREESTYLE_LINESET_FE_NOT,
       "EXCLUSIVE",
       0,
       "Exclusive",
       "Select feature edges not satisfying the given edge type conditions"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem edge_type_combination_items[] = {
      {0,
       "OR",
       0,
       "Logical OR",
       "Select feature edges satisfying at least one of edge type conditions"},
      {FREESTYLE_LINESET_FE_AND,
       "AND",
       0,
       "Logical AND",
       "Select feature edges satisfying all edge type conditions"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem collection_negation_items[] = {
      {0,
       "INCLUSIVE",
       0,
       "Inclusive",
       "Select feature edges belonging to some object in the group"},
      {FREESTYLE_LINESET_GR_NOT,
       "EXCLUSIVE",
       0,
       "Exclusive",
       "Select feature edges not belonging to any object in the group"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem face_mark_negation_items[] = {
      {0,
       "INCLUSIVE",
       0,
       "Inclusive",
       "Select feature edges satisfying the given face mark conditions"},
      {FREESTYLE_LINESET_FM_NOT,
       "EXCLUSIVE",
       0,
       "Exclusive",
       "Select feature edges not satisfying the given face mark conditions"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem face_mark_condition_items[] = {
      {0, "ONE", 0, "One Face", "Select a feature edge if either of its adjacent faces is marked"},
      {FREESTYLE_LINESET_FM_BOTH,
       "BOTH",
       0,
       "Both Faces",
       "Select a feature edge if both of its adjacent faces are marked"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem freestyle_ui_mode_items[] = {
      {FREESTYLE_CONTROL_SCRIPT_MODE,
       "SCRIPT",
       0,
       "Python Scripting Mode",
       "Advanced mode for using style modules written in Python"},
      {FREESTYLE_CONTROL_EDITOR_MODE,
       "EDITOR",
       0,
       "Parameter Editor Mode",
       "Basic mode for interactive style parameter editing"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem visibility_items[] = {
      {FREESTYLE_QI_VISIBLE, "VISIBLE", 0, "Visible", "Select visible feature edges"},
      {FREESTYLE_QI_HIDDEN, "HIDDEN", 0, "Hidden", "Select hidden feature edges"},
      {FREESTYLE_QI_RANGE,
       "RANGE",
       0,
       "QI Range",
       "Select feature edges within a range of quantitative invisibility (QI) values"},
      {0, NULL, 0, NULL, NULL},
  };

  /* FreestyleLineSet */

  srna = RNA_def_struct(brna, "FreestyleLineSet", NULL);
  RNA_def_struct_ui_text(
      srna, "Freestyle Line Set", "Line set for associating lines and style parameters");

  /* access to line style settings is redirected through functions
   * to allow proper id-buttons functionality
   */
  prop = RNA_def_property(srna, "linestyle", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "FreestyleLineStyle");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
  RNA_def_property_pointer_funcs(prop,
                                 "rna_FreestyleLineSet_linestyle_get",
                                 "rna_FreestyleLineSet_linestyle_set",
                                 NULL,
                                 NULL);
  RNA_def_property_ui_text(prop, "Line Style", "Line style settings");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "name");
  RNA_def_property_ui_text(prop, "Line Set Name", "Line set name");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_LINESET_ENABLED);
  RNA_def_property_ui_text(
      prop, "Render", "Enable or disable this line set during stroke rendering");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_by_visibility", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "selection", FREESTYLE_SEL_VISIBILITY);
  RNA_def_property_ui_text(
      prop, "Selection by Visibility", "Select feature edges based on visibility");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_by_edge_types", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "selection", FREESTYLE_SEL_EDGE_TYPES);
  RNA_def_property_ui_text(
      prop, "Selection by Edge Types", "Select feature edges based on edge types");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_by_collection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "selection", FREESTYLE_SEL_GROUP);
  RNA_def_property_ui_text(
      prop, "Selection by Collection", "Select feature edges based on a collection of objects");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_by_image_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "selection", FREESTYLE_SEL_IMAGE_BORDER);
  RNA_def_property_ui_text(prop,
                           "Selection by Image Border",
                           "Select feature edges by image border (less memory consumption)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_by_face_marks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "selection", FREESTYLE_SEL_FACE_MARK);
  RNA_def_property_ui_text(prop, "Selection by Face Marks", "Select feature edges by face marks");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "edge_type_negation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
  RNA_def_property_enum_items(prop, edge_type_negation_items);
  RNA_def_property_ui_text(
      prop,
      "Edge Type Negation",
      "Specify either inclusion or exclusion of feature edges selected by edge types");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "edge_type_combination", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
  RNA_def_property_enum_items(prop, edge_type_combination_items);
  RNA_def_property_ui_text(
      prop,
      "Edge Type Combination",
      "Specify a logical combination of selection conditions on feature edge types");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "group");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Collection", "A collection of objects based on which feature edges are selected");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "collection_negation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
  RNA_def_property_enum_items(prop, collection_negation_items);
  RNA_def_property_ui_text(prop,
                           "Collection Negation",
                           "Specify either inclusion or exclusion of feature edges belonging to a "
                           "collection of objects");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "face_mark_negation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
  RNA_def_property_enum_items(prop, face_mark_negation_items);
  RNA_def_property_ui_text(
      prop,
      "Face Mark Negation",
      "Specify either inclusion or exclusion of feature edges selected by face marks");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "face_mark_condition", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
  RNA_def_property_enum_items(prop, face_mark_condition_items);
  RNA_def_property_ui_text(prop,
                           "Face Mark Condition",
                           "Specify a feature edge selection condition based on face marks");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_silhouette", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_SILHOUETTE);
  RNA_def_property_ui_text(
      prop,
      "Silhouette",
      "Select silhouettes (edges at the boundary of visible and hidden faces)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_BORDER);
  RNA_def_property_ui_text(prop, "Border", "Select border edges (open mesh edges)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_crease", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_CREASE);
  RNA_def_property_ui_text(prop,
                           "Crease",
                           "Select crease edges (those between two faces making an angle smaller "
                           "than the Crease Angle)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_ridge_valley", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_RIDGE_VALLEY);
  RNA_def_property_ui_text(
      prop,
      "Ridge & Valley",
      "Select ridges and valleys (boundary lines between convex and concave areas of surface)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_suggestive_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_SUGGESTIVE_CONTOUR);
  RNA_def_property_ui_text(
      prop, "Suggestive Contour", "Select suggestive contours (almost silhouette/contour edges)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_material_boundary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_MATERIAL_BOUNDARY);
  RNA_def_property_ui_text(prop, "Material Boundary", "Select edges at material boundaries");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_CONTOUR);
  RNA_def_property_ui_text(prop, "Contour", "Select contours (outer silhouettes of each object)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_external_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_EXTERNAL_CONTOUR);
  RNA_def_property_ui_text(
      prop,
      "External Contour",
      "Select external contours (outer silhouettes of occluding and occluded objects)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_edge_mark", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_EDGE_MARK);
  RNA_def_property_ui_text(
      prop, "Edge Mark", "Select edge marks (edges annotated by Freestyle edge marks)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_silhouette", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_SILHOUETTE);
  RNA_def_property_ui_text(prop, "Silhouette", "Exclude silhouette edges");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_BORDER);
  RNA_def_property_ui_text(prop, "Border", "Exclude border edges");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_crease", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_CREASE);
  RNA_def_property_ui_text(prop, "Crease", "Exclude crease edges");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_ridge_valley", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_RIDGE_VALLEY);
  RNA_def_property_ui_text(prop, "Ridge & Valley", "Exclude ridges and valleys");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_suggestive_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_SUGGESTIVE_CONTOUR);
  RNA_def_property_ui_text(prop, "Suggestive Contour", "Exclude suggestive contours");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_material_boundary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_MATERIAL_BOUNDARY);
  RNA_def_property_ui_text(prop, "Material Boundary", "Exclude edges at material boundaries");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_CONTOUR);
  RNA_def_property_ui_text(prop, "Contour", "Exclude contours");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_external_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_EXTERNAL_CONTOUR);
  RNA_def_property_ui_text(prop, "External Contour", "Exclude external contours");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_edge_mark", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_EDGE_MARK);
  RNA_def_property_ui_text(prop, "Edge Mark", "Exclude edge marks");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "visibility", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "qi");
  RNA_def_property_enum_items(prop, visibility_items);
  RNA_def_property_ui_text(
      prop, "Visibility", "Determine how to use visibility for feature edge selection");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "qi_start", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "qi_start");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop, "Start", "First QI value of the QI range");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "qi_end", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "qi_end");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop, "End", "Last QI value of the QI range");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  /* FreestyleModuleSettings */

  srna = RNA_def_struct(brna, "FreestyleModuleSettings", NULL);
  RNA_def_struct_sdna(srna, "FreestyleModuleConfig");
  RNA_def_struct_ui_text(
      srna, "Freestyle Module", "Style module configuration for specifying a style module");

  prop = RNA_def_property(srna, "script", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Text");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Style Module", "Python script to define a style module");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "is_displayed", 1);
  RNA_def_property_ui_text(
      prop, "Use", "Enable or disable this style module during stroke rendering");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  /* FreestyleSettings */

  srna = RNA_def_struct(brna, "FreestyleSettings", NULL);
  RNA_def_struct_sdna(srna, "FreestyleConfig");
  RNA_def_struct_nested(brna, srna, "ViewLayer");
  RNA_def_struct_ui_text(
      srna, "Freestyle Settings", "Freestyle settings for a ViewLayer data-block");

  prop = RNA_def_property(srna, "modules", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "modules", NULL);
  RNA_def_property_struct_type(prop, "FreestyleModuleSettings");
  RNA_def_property_ui_text(
      prop, "Style Modules", "A list of style modules (to be applied from top to bottom)");
  rna_def_freestyle_modules(brna, prop);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, freestyle_ui_mode_items);
  RNA_def_property_ui_text(prop, "Control Mode", "Select the Freestyle control mode");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_culling", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_CULLING);
  RNA_def_property_ui_text(prop, "Culling", "If enabled, out-of-view edges are ignored");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_suggestive_contours", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_SUGGESTIVE_CONTOURS_FLAG);
  RNA_def_property_ui_text(prop, "Suggestive Contours", "Enable suggestive contours");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_ridges_and_valleys", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_RIDGES_AND_VALLEYS_FLAG);
  RNA_def_property_ui_text(prop, "Ridges and Valleys", "Enable ridges and valleys");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_material_boundaries", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_MATERIAL_BOUNDARIES_FLAG);
  RNA_def_property_ui_text(prop, "Material Boundaries", "Enable material boundaries");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_smoothness", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_FACE_SMOOTHNESS_FLAG);
  RNA_def_property_ui_text(
      prop, "Face Smoothness", "Take face smoothness into account in view map calculation");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_advanced_options", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_ADVANCED_OPTIONS_FLAG);
  RNA_def_property_ui_text(
      prop,
      "Advanced Options",
      "Enable advanced edge detection options (sphere radius and Kr derivative epsilon)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_view_map_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_VIEW_MAP_CACHE);
  RNA_def_property_ui_text(
      prop,
      "View Map Cache",
      "Keep the computed view map and avoid re-calculating it if mesh geometry is unchanged");
  RNA_def_property_update(
      prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_use_view_map_cache_update");

  prop = RNA_def_property(srna, "sphere_radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "sphere_radius");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_text(prop, "Sphere Radius", "Sphere radius for computing curvatures");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "kr_derivative_epsilon", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "dkr_epsilon");
  RNA_def_property_range(prop, -1000.0, 1000.0);
  RNA_def_property_ui_text(
      prop, "Kr Derivative Epsilon", "Kr derivative epsilon for computing suggestive contours");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "crease_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "crease_angle");
  RNA_def_property_range(prop, 0.0, DEG2RAD(180.0));
  RNA_def_property_ui_text(prop, "Crease Angle", "Angular threshold for detecting crease edges");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "linesets", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "linesets", NULL);
  RNA_def_property_struct_type(prop, "FreestyleLineSet");
  RNA_def_property_ui_text(prop, "Line Sets", "");
  rna_def_freestyle_linesets(brna, prop);
}

static void rna_def_bake_data(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BakeSettings", NULL);
  RNA_def_struct_sdna(srna, "BakeData");
  RNA_def_struct_nested(brna, srna, "RenderSettings");
  RNA_def_struct_ui_text(srna, "Bake Data", "Bake data for a Scene data-block");
  RNA_def_struct_path_func(srna, "rna_BakeSettings_path");

  prop = RNA_def_property(srna, "cage_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Cage Object",
      "Object to use as cage "
      "instead of calculating the cage from the active object with cage extrusion");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "File Path", "Image filepath to use when saving externally");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "width", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 4, 10000);
  RNA_def_property_ui_text(prop, "Width", "Horizontal dimension of the baking map");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "height", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 4, 10000);
  RNA_def_property_ui_text(prop, "Height", "Vertical dimension of the baking map");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "margin", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_range(prop, 0, 64, 1, 1);
  RNA_def_property_ui_text(prop, "Margin", "Extends the baked result as a post process filter");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "cage_extrusion", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 3);
  RNA_def_property_ui_text(
      prop,
      "Cage Extrusion",
      "Distance to use for the inward ray cast when using selected to active");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "normal_space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "normal_space");
  RNA_def_property_enum_items(prop, rna_enum_normal_space_items);
  RNA_def_property_ui_text(prop, "Normal Space", "Choose normal space for baking");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "normal_r", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "normal_swizzle[0]");
  RNA_def_property_enum_items(prop, rna_enum_normal_swizzle_items);
  RNA_def_property_ui_text(prop, "Normal Space", "Axis to bake in red channel");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "normal_g", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "normal_swizzle[1]");
  RNA_def_property_enum_items(prop, rna_enum_normal_swizzle_items);
  RNA_def_property_ui_text(prop, "Normal Space", "Axis to bake in green channel");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "normal_b", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "normal_swizzle[2]");
  RNA_def_property_enum_items(prop, rna_enum_normal_swizzle_items);
  RNA_def_property_ui_text(prop, "Normal Space", "Axis to bake in blue channel");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "image_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "im_format");
  RNA_def_property_struct_type(prop, "ImageFormatSettings");
  RNA_def_property_ui_text(prop, "Image Format", "");

  prop = RNA_def_property(srna, "save_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "save_mode");
  RNA_def_property_enum_items(prop, rna_enum_bake_save_mode_items);
  RNA_def_property_ui_text(prop, "Save Mode", "Choose how to save the baking map");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* flags */
  prop = RNA_def_property(srna, "use_selected_to_active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", R_BAKE_TO_ACTIVE);
  RNA_def_property_ui_text(prop,
                           "Selected to Active",
                           "Bake shading on the surface of selected objects to the active object");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_clear", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", R_BAKE_CLEAR);
  RNA_def_property_ui_text(prop, "Clear", "Clear Images before baking (internal only)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_split_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", R_BAKE_SPLIT_MAT);
  RNA_def_property_ui_text(
      prop, "Split Materials", "Split external images per material (external only)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_automatic_name", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", R_BAKE_AUTO_NAME);
  RNA_def_property_ui_text(
      prop,
      "Automatic Name",
      "Automatically name the output file with the pass type (external only)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_cage", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", R_BAKE_CAGE);
  RNA_def_property_ui_text(prop, "Cage", "Cast rays to active object from a cage");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* custom passes flags */
  prop = RNA_def_property(srna, "use_pass_ambient_occlusion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pass_filter", R_BAKE_PASS_FILTER_AO);
  RNA_def_property_ui_text(prop, "Ambient Occlusion", "Add ambient occlusion contribution");

  prop = RNA_def_property(srna, "use_pass_emit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pass_filter", R_BAKE_PASS_FILTER_EMIT);
  RNA_def_property_ui_text(prop, "Emit", "Add emission contribution");

  prop = RNA_def_property(srna, "use_pass_direct", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pass_filter", R_BAKE_PASS_FILTER_DIRECT);
  RNA_def_property_ui_text(prop, "Direct", "Add direct lighting contribution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_pass_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pass_filter", R_BAKE_PASS_FILTER_INDIRECT);
  RNA_def_property_ui_text(prop, "Indirect", "Add indirect lighting contribution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_pass_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pass_filter", R_BAKE_PASS_FILTER_COLOR);
  RNA_def_property_ui_text(prop, "Color", "Color the pass");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_pass_diffuse", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pass_filter", R_BAKE_PASS_FILTER_DIFFUSE);
  RNA_def_property_ui_text(prop, "Diffuse", "Add diffuse contribution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_pass_glossy", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pass_filter", R_BAKE_PASS_FILTER_GLOSSY);
  RNA_def_property_ui_text(prop, "Glossy", "Add glossy contribution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_pass_transmission", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pass_filter", R_BAKE_PASS_FILTER_TRANSM);
  RNA_def_property_ui_text(prop, "Transmission", "Add transmission contribution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_pass_subsurface", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pass_filter", R_BAKE_PASS_FILTER_SUBSURFACE);
  RNA_def_property_ui_text(prop, "Subsurface", "Add subsurface contribution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "pass_filter", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "pass_filter");
  RNA_def_property_enum_items(prop, rna_enum_bake_pass_filter_type_items);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_ui_text(prop, "Pass Filter", "Passes to include in the active baking pass");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_gpu_ssao_fx(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPUSSAOSettings", NULL);
  RNA_def_struct_ui_text(
      srna, "GPU SSAO", "Settings for GPU based screen space ambient occlusion");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Strength", "Strength of the SSAO effect");
  RNA_def_property_range(prop, 0.0f, 250.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "distance_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Distance", "Distance of object that contribute to the SSAO effect");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "attenuation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Attenuation", "Attenuation constant");
  RNA_def_property_range(prop, 1.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 1.0f, 100.0f, 1, 3);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Samples", "Number of samples");
  RNA_def_property_range(prop, 1, 500);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Color", "Color for screen space ambient occlusion effect");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);
}

static void rna_def_gpu_fx(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_gpu_ssao_fx(brna);

  srna = RNA_def_struct(brna, "GPUFXSettings", NULL);
  RNA_def_struct_ui_text(srna, "GPU FX Settings", "Settings for GPU based compositing");

  prop = RNA_def_property(srna, "ssao", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "GPUSSAOSettings");
  RNA_def_property_ui_text(prop, "Screen Space Ambient Occlusion settings", "");

  prop = RNA_def_property(srna, "use_ssao", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "fx_flag", GPU_FX_FLAG_SSAO);
  RNA_def_property_ui_text(
      prop, "SSAO", "Use screen space ambient occlusion of field on viewport");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);
}

static void rna_def_view_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ViewLayers");
  srna = RNA_def_struct(brna, "ViewLayers", NULL);
  RNA_def_struct_sdna(srna, "Scene");
  RNA_def_struct_ui_text(srna, "Render Layers", "Collection of render layers");

  func = RNA_def_function(srna, "new", "rna_ViewLayer_new");
  RNA_def_function_ui_description(func, "Add a view layer to scene");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_string(
      func, "name", "ViewLayer", 0, "", "New name for the view layer (not unique)");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "result", "ViewLayer", "", "Newly created view layer");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_ViewLayer_remove");
  RNA_def_function_ui_description(func, "Remove a view layer");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "ViewLayer", "", "View layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

/* Render Views - MultiView */
static void rna_def_scene_render_view(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SceneRenderView", NULL);
  RNA_def_struct_ui_text(
      srna, "Scene Render View", "Render viewpoint for 3D stereo and multiview rendering");
  RNA_def_struct_ui_icon(srna, ICON_RESTRICT_RENDER_OFF);
  RNA_def_struct_path_func(srna, "rna_SceneRenderView_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SceneRenderView_name_set");
  RNA_def_property_ui_text(prop, "Name", "Render view name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "file_suffix", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "suffix");
  RNA_def_property_ui_text(prop, "File Suffix", "Suffix added to the render images for this view");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "camera_suffix", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "suffix");
  RNA_def_property_ui_text(
      prop,
      "Camera Suffix",
      "Suffix to identify the cameras to use, and added to the render images for this view");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "viewflag", SCE_VIEW_DISABLE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Enabled", "Disable or enable the render view");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
}

static void rna_def_render_views(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "RenderViews");
  srna = RNA_def_struct(brna, "RenderViews", NULL);
  RNA_def_struct_sdna(srna, "RenderData");
  RNA_def_struct_ui_text(srna, "Render Views", "Collection of render views");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "actview");
  RNA_def_property_int_funcs(prop,
                             "rna_RenderSettings_active_view_index_get",
                             "rna_RenderSettings_active_view_index_set",
                             "rna_RenderSettings_active_view_index_range");
  RNA_def_property_ui_text(prop, "Active View Index", "Active index in render view array");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SceneRenderView");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_RenderSettings_active_view_get",
                                 "rna_RenderSettings_active_view_set",
                                 NULL,
                                 NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Active Render View", "Active Render View");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  func = RNA_def_function(srna, "new", "rna_RenderView_new");
  RNA_def_function_ui_description(func, "Add a render view to scene");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_string(func, "name", "RenderView", 0, "", "New name for the marker (not unique)");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "result", "SceneRenderView", "", "Newly created render view");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_RenderView_remove");
  RNA_def_function_ui_description(func, "Remove a render view");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  parm = RNA_def_pointer(func, "view", "SceneRenderView", "", "Render view to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_image_format_stereo3d_format(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* rna_enum_stereo3d_display_items, without (S3D_DISPLAY_PAGEFLIP) */
  static const EnumPropertyItem stereo3d_display_items[] = {
      {S3D_DISPLAY_ANAGLYPH,
       "ANAGLYPH",
       0,
       "Anaglyph",
       "Render views for left and right eyes as two differently filtered colors in a single image "
       "(anaglyph glasses are required)"},
      {S3D_DISPLAY_INTERLACE,
       "INTERLACE",
       0,
       "Interlace",
       "Render views for left and right eyes interlaced in a single image (3D-ready monitor is "
       "required)"},
      {S3D_DISPLAY_SIDEBYSIDE,
       "SIDEBYSIDE",
       0,
       "Side-by-Side",
       "Render views for left and right eyes side-by-side"},
      {S3D_DISPLAY_TOPBOTTOM,
       "TOPBOTTOM",
       0,
       "Top-Bottom",
       "Render views for left and right eyes one above another"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "Stereo3dFormat", NULL);
  RNA_def_struct_sdna(srna, "Stereo3dFormat");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Stereo Output", "Settings for stereo output");

  prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "display_mode");
  RNA_def_property_enum_items(prop, stereo3d_display_items);
  RNA_def_property_ui_text(prop, "Stereo Mode", "");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Stereo3dFormat_update");

  prop = RNA_def_property(srna, "anaglyph_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_stereo3d_anaglyph_type_items);
  RNA_def_property_ui_text(prop, "Anaglyph Type", "");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Stereo3dFormat_update");

  prop = RNA_def_property(srna, "interlace_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_stereo3d_interlace_type_items);
  RNA_def_property_ui_text(prop, "Interlace Type", "");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Stereo3dFormat_update");

  prop = RNA_def_property(srna, "use_interlace_swap", PROP_BOOLEAN, PROP_BOOLEAN);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", S3D_INTERLACE_SWAP);
  RNA_def_property_ui_text(prop, "Swap Left/Right", "Swap left and right stereo channels");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Stereo3dFormat_update");

  prop = RNA_def_property(srna, "use_sidebyside_crosseyed", PROP_BOOLEAN, PROP_BOOLEAN);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", S3D_SIDEBYSIDE_CROSSEYED);
  RNA_def_property_ui_text(prop, "Cross-Eyed", "Right eye should see left image and vice-versa");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Stereo3dFormat_update");

  prop = RNA_def_property(srna, "use_squeezed_frame", PROP_BOOLEAN, PROP_BOOLEAN);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", S3D_SQUEEZED_FRAME);
  RNA_def_property_ui_text(prop, "Squeezed Frame", "Combine both views in a squeezed image");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Stereo3dFormat_update");
}

/* use for render output and image save operator,
 * note: there are some cases where the members act differently when this is
 * used from a scene, video formats can only be selected for render output
 * for example, this is checked by seeing if the ptr->id.data is a Scene id */

static void rna_def_scene_image_format_data(BlenderRNA *brna)
{

#  ifdef WITH_OPENJPEG
  static const EnumPropertyItem jp2_codec_items[] = {
      {R_IMF_JP2_CODEC_JP2, "JP2", 0, "JP2", ""},
      {R_IMF_JP2_CODEC_J2K, "J2K", 0, "J2K", ""},
      {0, NULL, 0, NULL, NULL},
  };
#  endif

#  ifdef WITH_TIFF
  static const EnumPropertyItem tiff_codec_items[] = {
      {R_IMF_TIFF_CODEC_NONE, "NONE", 0, "None", ""},
      {R_IMF_TIFF_CODEC_DEFLATE, "DEFLATE", 0, "Deflate", ""},
      {R_IMF_TIFF_CODEC_LZW, "LZW", 0, "LZW", ""},
      {R_IMF_TIFF_CODEC_PACKBITS, "PACKBITS", 0, "Pack Bits", ""},
      {0, NULL, 0, NULL, NULL},
  };
#  endif

  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_image_format_stereo3d_format(brna);

  srna = RNA_def_struct(brna, "ImageFormatSettings", NULL);
  RNA_def_struct_sdna(srna, "ImageFormatData");
  RNA_def_struct_nested(brna, srna, "Scene");
  RNA_def_struct_path_func(srna, "rna_ImageFormatSettings_path");
  RNA_def_struct_ui_text(srna, "Image Format", "Settings for image formats");

  prop = RNA_def_property(srna, "file_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "imtype");
  RNA_def_property_enum_items(prop, rna_enum_image_type_items);
  RNA_def_property_enum_funcs(prop,
                              NULL,
                              "rna_ImageFormatSettings_file_format_set",
                              "rna_ImageFormatSettings_file_format_itemf");
  RNA_def_property_ui_text(prop, "File Format", "File format to save the rendered images as");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "color_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "planes");
  RNA_def_property_enum_items(prop, rna_enum_image_color_mode_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_ImageFormatSettings_color_mode_itemf");
  RNA_def_property_ui_text(
      prop,
      "Color Mode",
      "Choose BW for saving grayscale images, RGB for saving red, green and blue channels, "
      "and RGBA for saving red, green, blue and alpha channels");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "color_depth", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "depth");
  RNA_def_property_enum_items(prop, rna_enum_image_color_depth_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_ImageFormatSettings_color_depth_itemf");
  RNA_def_property_ui_text(prop, "Color Depth", "Bit depth per channel");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* was 'file_quality' */
  prop = RNA_def_property(srna, "quality", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, NULL, "quality");
  RNA_def_property_range(prop, 0, 100); /* 0 is needed for compression. */
  RNA_def_property_ui_text(
      prop, "Quality", "Quality for image formats that support lossy compression");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* was shared with file_quality */
  prop = RNA_def_property(srna, "compression", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, NULL, "compress");
  RNA_def_property_range(prop, 0, 100); /* 0 is needed for compression. */
  RNA_def_property_ui_text(prop,
                           "Compression",
                           "Amount of time to determine best compression: "
                           "0 = no compression with fast file output, "
                           "100 = maximum lossless compression with slow file output");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* flag */
  prop = RNA_def_property(srna, "use_zbuffer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", R_IMF_FLAG_ZBUF);
  RNA_def_property_ui_text(
      prop, "Z Buffer", "Save the z-depth per pixel (32 bit unsigned int z-buffer)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", R_IMF_FLAG_PREVIEW_JPG);
  RNA_def_property_ui_text(
      prop, "Preview", "When rendering animations, save JPG preview images in same directory");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* format specific */

#  ifdef WITH_OPENEXR
  /* OpenEXR */

  prop = RNA_def_property(srna, "exr_codec", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "exr_codec");
  RNA_def_property_enum_items(prop, rna_enum_exr_codec_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_ImageFormatSettings_exr_codec_itemf");
  RNA_def_property_ui_text(prop, "Codec", "Codec settings for OpenEXR");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
#  endif

#  ifdef WITH_OPENJPEG
  /* Jpeg 2000 */
  prop = RNA_def_property(srna, "use_jpeg2k_ycc", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "jp2_flag", R_IMF_JP2_FLAG_YCC);
  RNA_def_property_ui_text(
      prop, "YCC", "Save luminance-chrominance-chrominance channels instead of RGB colors");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_jpeg2k_cinema_preset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "jp2_flag", R_IMF_JP2_FLAG_CINE_PRESET);
  RNA_def_property_ui_text(prop, "Cinema", "Use Openjpeg Cinema Preset");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_jpeg2k_cinema_48", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "jp2_flag", R_IMF_JP2_FLAG_CINE_48);
  RNA_def_property_ui_text(prop, "Cinema (48)", "Use Openjpeg Cinema Preset (48fps)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "jpeg2k_codec", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "jp2_codec");
  RNA_def_property_enum_items(prop, jp2_codec_items);
  RNA_def_property_ui_text(prop, "Codec", "Codec settings for Jpeg2000");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
#  endif

#  ifdef WITH_TIFF
  /* TIFF */
  prop = RNA_def_property(srna, "tiff_codec", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "tiff_codec");
  RNA_def_property_enum_items(prop, tiff_codec_items);
  RNA_def_property_ui_text(prop, "Compression", "Compression mode for TIFF");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
#  endif

  /* Cineon and DPX */

  prop = RNA_def_property(srna, "use_cineon_log", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cineon_flag", R_IMF_CINEON_FLAG_LOG);
  RNA_def_property_ui_text(prop, "Log", "Convert to logarithmic color space");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "cineon_black", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "cineon_black");
  RNA_def_property_range(prop, 0, 1024);
  RNA_def_property_ui_text(prop, "B", "Log conversion reference blackpoint");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "cineon_white", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "cineon_white");
  RNA_def_property_range(prop, 0, 1024);
  RNA_def_property_ui_text(prop, "W", "Log conversion reference whitepoint");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "cineon_gamma", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "cineon_gamma");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(prop, "G", "Log conversion gamma");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* multiview */
  prop = RNA_def_property(srna, "views_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "views_format");
  RNA_def_property_enum_items(prop, rna_enum_views_format_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_ImageFormatSettings_views_format_itemf");
  RNA_def_property_ui_text(prop, "Views Format", "Format of multiview media");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "stereo_3d_format", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "stereo3d_format");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "Stereo3dFormat");
  RNA_def_property_ui_text(prop, "Stereo 3D Format", "Settings for stereo 3d");

  /* color management */
  prop = RNA_def_property(srna, "view_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "view_settings");
  RNA_def_property_struct_type(prop, "ColorManagedViewSettings");
  RNA_def_property_ui_text(
      prop, "View Settings", "Color management settings applied on image before saving");

  prop = RNA_def_property(srna, "display_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "display_settings");
  RNA_def_property_struct_type(prop, "ColorManagedDisplaySettings");
  RNA_def_property_ui_text(
      prop, "Display Settings", "Settings of device saved image would be displayed on");
}

static void rna_def_scene_ffmpeg_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

#  ifdef WITH_FFMPEG
  /* Container types */
  static const EnumPropertyItem ffmpeg_format_items[] = {
      {FFMPEG_MPEG1, "MPEG1", 0, "MPEG-1", ""},
      {FFMPEG_MPEG2, "MPEG2", 0, "MPEG-2", ""},
      {FFMPEG_MPEG4, "MPEG4", 0, "MPEG-4", ""},
      {FFMPEG_AVI, "AVI", 0, "AVI", ""},
      {FFMPEG_MOV, "QUICKTIME", 0, "Quicktime", ""},
      {FFMPEG_DV, "DV", 0, "DV", ""},
      {FFMPEG_OGG, "OGG", 0, "Ogg", ""},
      {FFMPEG_MKV, "MKV", 0, "Matroska", ""},
      {FFMPEG_FLV, "FLASH", 0, "Flash", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem ffmpeg_codec_items[] = {
      {AV_CODEC_ID_NONE, "NONE", 0, "No Video", "Disables video output, for audio-only renders"},
      {AV_CODEC_ID_DNXHD, "DNXHD", 0, "DNxHD", ""},
      {AV_CODEC_ID_DVVIDEO, "DV", 0, "DV", ""},
      {AV_CODEC_ID_FFV1, "FFV1", 0, "FFmpeg video codec #1", ""},
      {AV_CODEC_ID_FLV1, "FLASH", 0, "Flash Video", ""},
      {AV_CODEC_ID_H264, "H264", 0, "H.264", ""},
      {AV_CODEC_ID_HUFFYUV, "HUFFYUV", 0, "HuffYUV", ""},
      {AV_CODEC_ID_MPEG1VIDEO, "MPEG1", 0, "MPEG-1", ""},
      {AV_CODEC_ID_MPEG2VIDEO, "MPEG2", 0, "MPEG-2", ""},
      {AV_CODEC_ID_MPEG4, "MPEG4", 0, "MPEG-4 (divx)", ""},
      {AV_CODEC_ID_PNG, "PNG", 0, "PNG", ""},
      {AV_CODEC_ID_QTRLE, "QTRLE", 0, "QT rle / QT Animation", ""},
      {AV_CODEC_ID_THEORA, "THEORA", 0, "Theora", ""},
      {AV_CODEC_ID_VP9, "WEBM", 0, "WEBM / VP9", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* Recommendations come from the FFmpeg wiki, https://trac.ffmpeg.org/wiki/Encode/VP9.
   * The label for BEST has been changed to "Slowest" so that it fits the "Encoding Speed"
   * property label in the UI. */
  static const EnumPropertyItem ffmpeg_preset_items[] = {
      {FFM_PRESET_BEST,
       "BEST",
       0,
       "Slowest",
       "Recommended if you have lots of time and want the best compression efficiency"},
      {FFM_PRESET_GOOD, "GOOD", 0, "Good", "The default and recommended for most applications"},
      {FFM_PRESET_REALTIME, "REALTIME", 0, "Realtime", "Recommended for fast encoding"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem ffmpeg_crf_items[] = {
      {FFM_CRF_NONE,
       "NONE",
       0,
       "Constant Bitrate",
       "Configure constant bit rate, rather than constant output quality"},
      {FFM_CRF_LOSSLESS, "LOSSLESS", 0, "Lossless", ""},
      {FFM_CRF_PERC_LOSSLESS, "PERC_LOSSLESS", 0, "Perceptually lossless", ""},
      {FFM_CRF_HIGH, "HIGH", 0, "High quality", ""},
      {FFM_CRF_MEDIUM, "MEDIUM", 0, "Medium quality", ""},
      {FFM_CRF_LOW, "LOW", 0, "Low quality", ""},
      {FFM_CRF_VERYLOW, "VERYLOW", 0, "Very low quality", ""},
      {FFM_CRF_LOWEST, "LOWEST", 0, "Lowest quality", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem ffmpeg_audio_codec_items[] = {
      {AV_CODEC_ID_NONE, "NONE", 0, "No Audio", "Disables audio output, for video-only renders"},
      {AV_CODEC_ID_AAC, "AAC", 0, "AAC", ""},
      {AV_CODEC_ID_AC3, "AC3", 0, "AC3", ""},
      {AV_CODEC_ID_FLAC, "FLAC", 0, "FLAC", ""},
      {AV_CODEC_ID_MP2, "MP2", 0, "MP2", ""},
      {AV_CODEC_ID_MP3, "MP3", 0, "MP3", ""},
      {AV_CODEC_ID_PCM_S16LE, "PCM", 0, "PCM", ""},
      {AV_CODEC_ID_VORBIS, "VORBIS", 0, "Vorbis", ""},
      {0, NULL, 0, NULL, NULL},
  };
#  endif

  static const EnumPropertyItem audio_channel_items[] = {
      {FFM_CHANNELS_MONO, "MONO", 0, "Mono", "Set audio channels to mono"},
      {FFM_CHANNELS_STEREO, "STEREO", 0, "Stereo", "Set audio channels to stereo"},
      {FFM_CHANNELS_SURROUND4, "SURROUND4", 0, "4 Channels", "Set audio channels to 4 channels"},
      {FFM_CHANNELS_SURROUND51,
       "SURROUND51",
       0,
       "5.1 Surround",
       "Set audio channels to 5.1 surround sound"},
      {FFM_CHANNELS_SURROUND71,
       "SURROUND71",
       0,
       "7.1 Surround",
       "Set audio channels to 7.1 surround sound"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "FFmpegSettings", NULL);
  RNA_def_struct_sdna(srna, "FFMpegCodecData");
  RNA_def_struct_ui_text(srna, "FFmpeg Settings", "FFmpeg related settings for the scene");

#  ifdef WITH_FFMPEG
  prop = RNA_def_property(srna, "format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "type");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_format_items);
  RNA_def_property_enum_default(prop, FFMPEG_MKV);
  RNA_def_property_ui_text(prop, "Container", "Output file container");
  RNA_def_property_update(
      prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_FFmpegSettings_codec_settings_update");

  prop = RNA_def_property(srna, "codec", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "codec");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_codec_items);
  RNA_def_property_enum_default(prop, AV_CODEC_ID_H264);
  RNA_def_property_ui_text(prop, "Video Codec", "FFmpeg codec to use for video output");
  RNA_def_property_update(
      prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_FFmpegSettings_codec_settings_update");

  prop = RNA_def_property(srna, "video_bitrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "video_bitrate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Bitrate", "Video bitrate (kb/s)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "minrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "rc_min_rate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Min Rate", "Rate control: min rate (kb/s)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "maxrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "rc_max_rate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Max Rate", "Rate control: max rate (kb/s)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "muxrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "mux_rate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 100000000);
  RNA_def_property_ui_text(prop, "Mux Rate", "Mux rate (bits/s(!))");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "gopsize", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "gop_size");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 500);
  RNA_def_property_int_default(prop, 25);
  RNA_def_property_ui_text(prop,
                           "Keyframe interval",
                           "Distance between key frames, also known as GOP size; "
                           "influences file size and seekability");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "max_b_frames", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "max_b_frames");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 16);
  RNA_def_property_ui_text(
      prop,
      "Max B-frames",
      "Maximum number of B-frames between non-B-frames; influences file size and seekability");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_max_b_frames", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", FFMPEG_USE_MAX_B_FRAMES);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Use max B-frames", "Set a maximum number of B-frames");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "buffersize", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "rc_buffer_size");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 2000);
  RNA_def_property_ui_text(prop, "Buffersize", "Rate control: buffer size (kb)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "packetsize", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "mux_packet_size");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 16384);
  RNA_def_property_ui_text(prop, "Mux Packet Size", "Mux packet size (byte)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "constant_rate_factor", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "constant_rate_factor");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_crf_items);
  RNA_def_property_enum_default(prop, FFM_CRF_MEDIUM);
  RNA_def_property_ui_text(
      prop,
      "Output quality",
      "Constant Rate Factor (CRF); tradeoff between video quality and file size");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "ffmpeg_preset", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "ffmpeg_preset");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_preset_items);
  RNA_def_property_enum_default(prop, FFM_PRESET_GOOD);
  RNA_def_property_ui_text(
      prop, "Encoding speed", "Tradeoff between encoding speed and compression ratio");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_autosplit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", FFMPEG_AUTOSPLIT_OUTPUT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Autosplit Output", "Autosplit output at 2GB boundary");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_lossless_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", FFMPEG_LOSSLESS_OUTPUT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_FFmpegSettings_lossless_output_set");
  RNA_def_property_ui_text(prop, "Lossless Output", "Use lossless output for video streams");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* FFMPEG Audio*/
  prop = RNA_def_property(srna, "audio_codec", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "audio_codec");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_audio_codec_items);
  RNA_def_property_ui_text(prop, "Audio Codec", "FFmpeg audio codec to use");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "audio_bitrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "audio_bitrate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 32, 384);
  RNA_def_property_ui_text(prop, "Bitrate", "Audio bitrate (kb/s)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "audio_volume", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "audio_volume");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Volume", "Audio volume");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
#  endif

  /* the following two "ffmpeg" settings are general audio settings */
  prop = RNA_def_property(srna, "audio_mixrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "audio_mixrate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 8000, 192000);
  RNA_def_property_ui_text(prop, "Samplerate", "Audio samplerate(samples/s)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "audio_channels", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "audio_channels");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, audio_channel_items);
  RNA_def_property_ui_text(prop, "Audio Channels", "Audio channel count");
}

static void rna_def_scene_render_data(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem display_mode_items[] = {
      {R_OUTPUT_SCREEN,
       "SCREEN",
       0,
       "Full Screen",
       "Images are rendered in a maximized Image Editor"},
      {R_OUTPUT_AREA, "AREA", 0, "Image Editor", "Images are rendered in an Image Editor"},
      {R_OUTPUT_WINDOW, "WINDOW", 0, "New Window", "Images are rendered in a new window"},
      {R_OUTPUT_NONE,
       "NONE",
       0,
       "Keep User Interface",
       "Images are rendered without changing the user interface"},
      {0, NULL, 0, NULL, NULL},
  };

  /* Bake */
  static const EnumPropertyItem bake_mode_items[] = {
      //{RE_BAKE_AO, "AO", 0, "Ambient Occlusion", "Bake ambient occlusion"},
      {RE_BAKE_NORMALS, "NORMALS", 0, "Normals", "Bake normals"},
      {RE_BAKE_DISPLACEMENT, "DISPLACEMENT", 0, "Displacement", "Bake displacement"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem pixel_size_items[] = {
      {0, "AUTO", 0, "Automatic", "Automatic pixel size, depends on the user interface scale"},
      {1, "1", 0, "1x", "Render at full resolution"},
      {2, "2", 0, "2x", "Render at 50% resolution"},
      {4, "4", 0, "4x", "Render at 25% resolution"},
      {8, "8", 0, "8x", "Render at 12.5% resolution"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem threads_mode_items[] = {
      {0,
       "AUTO",
       0,
       "Auto-detect",
       "Automatically determine the number of threads, based on CPUs"},
      {R_FIXED_THREADS, "FIXED", 0, "Fixed", "Manually determine the number of threads"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem engine_items[] = {
      {0, "BLENDER_EEVEE", 0, "Eevee", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem freestyle_thickness_items[] = {
      {R_LINE_THICKNESS_ABSOLUTE,
       "ABSOLUTE",
       0,
       "Absolute",
       "Specify unit line thickness in pixels"},
      {R_LINE_THICKNESS_RELATIVE,
       "RELATIVE",
       0,
       "Relative",
       "Unit line thickness is scaled by the proportion of the present vertical image "
       "resolution to 480 pixels"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem views_format_items[] = {
      {SCE_VIEWS_FORMAT_STEREO_3D,
       "STEREO_3D",
       0,
       "Stereo 3D",
       "Single stereo camera system, adjust the stereo settings in the camera panel"},
      {SCE_VIEWS_FORMAT_MULTIVIEW,
       "MULTIVIEW",
       0,
       "Multi-View",
       "Multi camera system, adjust the cameras individually"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem hair_shape_type_items[] = {
      {SCE_HAIR_SHAPE_STRAND, "STRAND", 0, "Strand", ""},
      {SCE_HAIR_SHAPE_STRIP, "STRIP", 0, "Strip", ""},
      {0, NULL, 0, NULL, NULL},
  };

  rna_def_scene_ffmpeg_settings(brna);

  srna = RNA_def_struct(brna, "RenderSettings", NULL);
  RNA_def_struct_sdna(srna, "RenderData");
  RNA_def_struct_nested(brna, srna, "Scene");
  RNA_def_struct_path_func(srna, "rna_RenderSettings_path");
  RNA_def_struct_ui_text(srna, "Render Data", "Rendering settings for a Scene data-block");

  /* Render Data */
  prop = RNA_def_property(srna, "image_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "im_format");
  RNA_def_property_struct_type(prop, "ImageFormatSettings");
  RNA_def_property_ui_text(prop, "Image Format", "");

  prop = RNA_def_property(srna, "resolution_x", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "xsch");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 4, 65536);
  RNA_def_property_ui_text(
      prop, "Resolution X", "Number of horizontal pixels in the rendered image");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");

  prop = RNA_def_property(srna, "resolution_y", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "ysch");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 4, 65536);
  RNA_def_property_ui_text(
      prop, "Resolution Y", "Number of vertical pixels in the rendered image");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");

  prop = RNA_def_property(srna, "resolution_percentage", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, NULL, "size");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, SHRT_MAX);
  RNA_def_property_ui_range(prop, 1, 100, 10, 1);
  RNA_def_property_ui_text(prop, "Resolution %", "Percentage scale for render resolution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneSequencer_update");

  prop = RNA_def_property(srna, "tile_x", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "tilex");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 8, 65536);
  RNA_def_property_ui_text(prop, "Tile X", "Horizontal tile size to use while rendering");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "tile_y", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "tiley");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 8, 65536);
  RNA_def_property_ui_text(prop, "Tile Y", "Vertical tile size to use while rendering");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "preview_start_resolution", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 8, 16384);
  RNA_def_property_int_default(prop, 64);
  RNA_def_property_ui_text(prop,
                           "Start Resolution",
                           "Resolution to start rendering preview at, "
                           "progressively increasing it to the full viewport size");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "preview_pixel_size", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "preview_pixel_size");
  RNA_def_property_enum_items(prop, pixel_size_items);
  RNA_def_property_ui_text(prop, "Pixel Size", "Pixel size for viewport rendering");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "pixel_aspect_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "xasp");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1.0f, 200.0f);
  RNA_def_property_ui_text(prop,
                           "Pixel Aspect X",
                           "Horizontal aspect ratio - for anamorphic or non-square pixel output");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");

  prop = RNA_def_property(srna, "pixel_aspect_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "yasp");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1.0f, 200.0f);
  RNA_def_property_ui_text(
      prop, "Pixel Aspect Y", "Vertical aspect ratio - for anamorphic or non-square pixel output");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");

  prop = RNA_def_property(srna, "ffmpeg", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "FFmpegSettings");
  RNA_def_property_pointer_sdna(prop, NULL, "ffcodecdata");
  RNA_def_property_flag(prop, PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "FFmpeg Settings", "FFmpeg related settings for the scene");

  prop = RNA_def_property(srna, "fps", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "frs_sec");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, SHRT_MAX);
  RNA_def_property_ui_range(prop, 1, 120, 1, -1);
  RNA_def_property_ui_text(prop, "FPS", "Framerate, expressed in frames per second");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_fps_update");

  prop = RNA_def_property(srna, "fps_base", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "frs_sec_base");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1e-5f, 1e6f);
  RNA_def_property_ui_range(prop, 0.1f, 120.0f, 2, -1);
  RNA_def_property_ui_text(prop, "FPS Base", "Framerate base");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_fps_update");

  /* frame mapping */
  prop = RNA_def_property(srna, "frame_map_old", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "framapto");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, 900);
  RNA_def_property_ui_text(prop, "Frame Map Old", "Old mapping value in frames");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_framelen_update");

  prop = RNA_def_property(srna, "frame_map_new", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "images");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, 900);
  RNA_def_property_ui_text(prop, "Frame Map New", "How many frames the Map Old will last");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_framelen_update");

  prop = RNA_def_property(srna, "dither_intensity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "dither_intensity");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_text(
      prop,
      "Dither Intensity",
      "Amount of dithering noise added to the rendered image to break up banding");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "filter_size", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_float_sdna(prop, NULL, "gauss");
  RNA_def_property_range(prop, 0.0f, 500.0f);
  RNA_def_property_ui_range(prop, 0.01f, 10.0f, 1, 2);
  RNA_def_property_ui_text(
      prop, "Filter Size", "Width over which the reconstruction filter combines samples");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "film_transparent", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "alphamode", R_ALPHAPREMUL);
  RNA_def_property_ui_text(
      prop,
      "Transparent",
      "World background is transparent, for compositing the render over another background");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");

  prop = RNA_def_property(srna, "use_freestyle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", R_EDGE_FRS);
  RNA_def_property_ui_text(prop, "Edge", "Draw stylized strokes using Freestyle");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  /* threads */
  prop = RNA_def_property(srna, "threads", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "threads");
  RNA_def_property_range(prop, 1, BLENDER_MAX_THREADS);
  RNA_def_property_int_funcs(prop, "rna_RenderSettings_threads_get", NULL, NULL);
  RNA_def_property_ui_text(prop,
                           "Threads",
                           "Number of CPU threads to use simultaneously while rendering "
                           "(for multi-core/CPU systems)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "threads_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, threads_mode_items);
  RNA_def_property_enum_funcs(prop, "rna_RenderSettings_threads_mode_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Threads Mode", "Determine the amount of render threads used");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* motion blur */
  prop = RNA_def_property(srna, "use_motion_blur", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", R_MBLUR);
  RNA_def_property_ui_text(prop, "Motion Blur", "Use multi-sampled 3D scene motion blur");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");

  prop = RNA_def_property(srna, "motion_blur_shutter", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "blurfac");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.01f, 1.0f, 1, 2);
  RNA_def_property_ui_text(prop, "Shutter", "Time taken in frames between shutter open and close");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");

  prop = RNA_def_property(srna, "motion_blur_shutter_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "mblur_shutter_curve");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(
      prop, "Shutter Curve", "Curve defining the shutter's openness over time");

  /* Hairs */
  prop = RNA_def_property(srna, "hair_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, hair_shape_type_items);
  RNA_def_property_ui_text(prop, "Hair Shape Type", "Hair shape type");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");

  prop = RNA_def_property(srna, "hair_subdiv", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 3);
  RNA_def_property_ui_text(prop, "Additional Subdiv", "Additional subdivision along the hair");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");

  /* border */
  prop = RNA_def_property(srna, "use_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", R_BORDER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Render Region", "Render a user-defined render region, within the frame size");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "border_min_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "border.xmin");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Region Minimum X", "Minimum X value for the render region");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "border_min_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "border.ymin");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Region Minimum Y", "Minimum Y value for the render region");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "border_max_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "border.xmax");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Region Maximum X", "Maximum X value for the render region");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "border_max_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "border.ymax");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Region Maximum Y", "Maximum Y value for the render region");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_crop_to_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", R_CROP);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Crop to Render Region", "Crop the rendered frame to the defined render region size");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_placeholder", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", R_TOUCH);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Placeholders",
      "Create empty placeholder files while rendering frames (similar to Unix 'touch')");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_overwrite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "mode", R_NO_OVERWRITE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Overwrite", "Overwrite existing files while rendering");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_compositing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_DOCOMP);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Compositing",
                           "Process the render result through the compositing pipeline, "
                           "if compositing nodes are enabled");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_sequencer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_DOSEQ);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Sequencer",
                           "Process the render (and composited) result through the video sequence "
                           "editor pipeline, if sequencer strips exist");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_file_extension", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_EXTENSION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "File Extensions",
      "Add the file format extensions to the rendered file name (eg: filename + .jpg)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

#  if 0 /* moved */
  prop = RNA_def_property(srna, "file_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "imtype");
  RNA_def_property_enum_items(prop, rna_enum_image_type_items);
  RNA_def_property_enum_funcs(prop, NULL, "rna_RenderSettings_file_format_set", NULL);
  RNA_def_property_ui_text(prop, "File Format", "File format to save the rendered images as");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
#  endif

  prop = RNA_def_property(srna, "file_extension", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_SceneRender_file_ext_get", "rna_SceneRender_file_ext_length", NULL);
  RNA_def_property_ui_text(prop, "Extension", "The file extension used for saving renders");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_movie_format", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_is_movie_format_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Movie Format", "When true the format is a movie");

  prop = RNA_def_property(srna, "use_save_buffers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_EXR_TILE_FILE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Save Buffers",
      "Save tiles for all RenderLayers and SceneNodes to files in the temp directory "
      "(saves memory, required for Full Sample)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_full_sample", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_FULL_SAMPLE);
  RNA_def_property_ui_text(prop,
                           "Full Sample",
                           "Save for every anti-aliasing sample the entire RenderLayer results "
                           "(this solves anti-aliasing issues with compositing)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "displaymode");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, display_mode_items);
  RNA_def_property_ui_text(prop, "Display", "Select where rendered images will be displayed");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_lock_interface", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_lock_interface", 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, true);
  RNA_def_property_ui_text(
      prop,
      "Lock Interface",
      "Lock interface during rendering in favor of giving more memory to the renderer");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, NULL, "pic");
  RNA_def_property_ui_text(prop,
                           "Output Path",
                           "Directory/name to save animations, # characters defines the position "
                           "and length of frame numbers");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Render result EXR cache. */
  prop = RNA_def_property(srna, "use_render_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_EXR_CACHE_FILE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Cache Result",
                           "Save render cache to EXR files (useful for heavy compositing, "
                           "Note: affects indirectly rendered scenes)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Bake */

  prop = RNA_def_property(srna, "bake_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "bake_mode");
  RNA_def_property_enum_items(prop, bake_mode_items);
  RNA_def_property_ui_text(prop, "Bake Type", "Choose shading information to bake into the image");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_bake_selected_to_active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_TO_ACTIVE);
  RNA_def_property_ui_text(prop,
                           "Selected to Active",
                           "Bake shading on the surface of selected objects to the active object");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_bake_clear", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_CLEAR);
  RNA_def_property_ui_text(prop, "Clear", "Clear Images before baking");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "bake_margin", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "bake_filter");
  RNA_def_property_range(prop, 0, 64);
  RNA_def_property_ui_text(prop, "Margin", "Extends the baked result as a post process filter");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "bake_bias", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "bake_biasdist");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_text(
      prop, "Bias", "Bias towards faces further away from the object (in blender units)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_bake_multires", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_MULTIRES);
  RNA_def_property_ui_text(prop, "Bake from Multires", "Bake directly from multires object");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_bake_lores_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_LORES_MESH);
  RNA_def_property_ui_text(
      prop, "Low Resolution Mesh", "Calculate heights against unsubdivided low resolution mesh");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "bake_samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "bake_samples");
  RNA_def_property_range(prop, 64, 1024);
  RNA_def_property_int_default(prop, 256);
  RNA_def_property_ui_text(
      prop, "Samples", "Number of samples used for ambient occlusion baking from multires");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_bake_user_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_USERSCALE);
  RNA_def_property_ui_text(prop, "User scale", "Use a user scale for the derivative map");

  prop = RNA_def_property(srna, "bake_user_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "bake_user_scale");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_text(prop,
                           "Scale",
                           "Instead of automatically normalizing to 0..1, "
                           "apply a user scale to the derivative map");

  /* stamp */

  prop = RNA_def_property(srna, "use_stamp_time", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_TIME);
  RNA_def_property_ui_text(
      prop, "Stamp Time", "Include the rendered frame timecode as HH:MM:SS.FF in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_date", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_DATE);
  RNA_def_property_ui_text(prop, "Stamp Date", "Include the current date in image/video metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_frame", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_FRAME);
  RNA_def_property_ui_text(prop, "Stamp Frame", "Include the frame number in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_frame_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_FRAME_RANGE);
  RNA_def_property_ui_text(
      prop, "Stamp Frame", "Include the rendered frame range in image/video metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_CAMERA);
  RNA_def_property_ui_text(
      prop, "Stamp Camera", "Include the name of the active camera in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_lens", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_CAMERALENS);
  RNA_def_property_ui_text(
      prop, "Stamp Lens", "Include the active camera's lens in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_scene", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_SCENE);
  RNA_def_property_ui_text(
      prop, "Stamp Scene", "Include the name of the active scene in image/video metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_note", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_NOTE);
  RNA_def_property_ui_text(prop, "Stamp Note", "Include a custom note in image/video metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_marker", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_MARKER);
  RNA_def_property_ui_text(
      prop, "Stamp Marker", "Include the name of the last marker in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_filename", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_FILENAME);
  RNA_def_property_ui_text(
      prop, "Stamp Filename", "Include the .blend filename in image/video metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_sequencer_strip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_SEQSTRIP);
  RNA_def_property_ui_text(prop,
                           "Stamp Sequence Strip",
                           "Include the name of the foreground sequence strip in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_render_time", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_RENDERTIME);
  RNA_def_property_ui_text(prop, "Stamp Render Time", "Include the render time in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "stamp_note_text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "stamp_udata");
  RNA_def_property_ui_text(prop, "Stamp Note Text", "Custom text to appear in the stamp note");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_DRAW);
  RNA_def_property_ui_text(
      prop, "Stamp Output", "Render the stamp info text in the rendered image");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_labels", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "stamp", R_STAMP_HIDE_LABELS);
  RNA_def_property_ui_text(
      prop, "Stamp Labels", "Display stamp labels (\"Camera\" in front of camera name, etc.)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_strip_meta", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_STRIPMETA);
  RNA_def_property_ui_text(
      prop, "Strip Metadata", "Use metadata from the strips in the sequencer");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_memory", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_MEMORY);
  RNA_def_property_ui_text(
      prop, "Stamp Peak Memory", "Include the peak memory usage in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_stamp_hostname", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_HOSTNAME);
  RNA_def_property_ui_text(
      prop, "Stamp Hostname", "Include the hostname of the machine that rendered the frame");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "stamp_font_size", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "stamp_font_id");
  RNA_def_property_range(prop, 8, 64);
  RNA_def_property_ui_text(prop, "Font Size", "Size of the font used when rendering stamp text");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "stamp_foreground", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "fg_stamp");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Text Color", "Color to use for stamp text");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "stamp_background", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "bg_stamp");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Background", "Color to use behind stamp text");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* sequencer draw options */

#  if 0 /* see R_SEQ_GL_REND comment */
  prop = RNA_def_property(srna, "use_sequencer_gl_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "seq_flag", R_SEQ_GL_REND);
  RNA_def_property_ui_text(prop, "Sequencer OpenGL", "");
#  endif

  prop = RNA_def_property(srna, "sequencer_gl_preview", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "seq_prev_type");
  RNA_def_property_enum_items(prop, rna_enum_shading_type_items);
  RNA_def_property_ui_text(
      prop, "Sequencer Preview Shading", "Method to draw in the sequencer view");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SceneSequencer_update");

#  if 0 /* UNUSED, see R_SEQ_GL_REND comment */
  prop = RNA_def_property(srna, "sequencer_gl_render", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "seq_rend_type");
  RNA_def_property_enum_items(prop, rna_enum_shading_type_items);
  /* XXX Label and tooltips are obviously wrong! */
  RNA_def_property_ui_text(
      prop, "Sequencer Preview Shading", "Method to draw in the sequencer view");
#  endif

  prop = RNA_def_property(srna, "use_sequencer_override_scene_strip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "seq_flag", R_SEQ_OVERRIDE_SCENE_SETTINGS);
  RNA_def_property_ui_text(prop,
                           "Override Scene Settings",
                           "Use workbench render settings from the sequencer scene, instead of "
                           "each individual scene used in the strip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SceneSequencer_update");

  prop = RNA_def_property(srna, "use_single_layer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_SINGLE_LAYER);
  RNA_def_property_ui_text(prop,
                           "Render Single Layer",
                           "Only render the active layer. Only affects rendering from the "
                           "interface, ignored for rendering from command line");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* views (stereoscopy et al) */
  prop = RNA_def_property(srna, "views", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "SceneRenderView");
  RNA_def_property_ui_text(prop, "Render Views", "");
  rna_def_render_views(brna, prop);

  prop = RNA_def_property(srna, "stereo_views", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "views", NULL);
  RNA_def_property_collection_funcs(prop,
                                    "rna_RenderSettings_stereoViews_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "SceneRenderView");
  RNA_def_property_ui_text(prop, "Render Views", "");

  prop = RNA_def_property(srna, "use_multiview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_MULTIVIEW);
  RNA_def_property_ui_text(prop, "Multiple Views", "Use multiple views in the scene");
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  prop = RNA_def_property(srna, "views_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, views_format_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Setup Stereo Mode", "");
  RNA_def_property_enum_funcs(prop, NULL, "rna_RenderSettings_views_format_set", NULL);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  /* engine */
  prop = RNA_def_property(srna, "engine", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, engine_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_RenderSettings_engine_get",
                              "rna_RenderSettings_engine_set",
                              "rna_RenderSettings_engine_itemf");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Engine", "Engine to use for rendering");
  RNA_def_property_update(prop, NC_WINDOW, "rna_RenderSettings_engine_update");

  prop = RNA_def_property(srna, "has_multiple_engines", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_multiple_engines_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Multiple Engines", "More than one rendering engine is available");

  prop = RNA_def_property(srna, "use_spherical_stereo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_use_spherical_stereo_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Use Spherical Stereo", "Active render engine supports spherical stereo rendering");

  /* simplify */
  prop = RNA_def_property(srna, "use_simplify", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", R_SIMPLIFY);
  RNA_def_property_ui_text(
      prop, "Use Simplify", "Enable simplification of scene for quicker preview renders");
  RNA_def_property_update(prop, 0, "rna_Scene_use_simplify_update");

  prop = RNA_def_property(srna, "simplify_subdivision", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "simplify_subsurf");
  RNA_def_property_ui_range(prop, 0, 6, 1, -1);
  RNA_def_property_ui_text(prop, "Simplify Subdivision", "Global maximum subdivision level");
  RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

  prop = RNA_def_property(srna, "simplify_child_particles", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "simplify_particles");
  RNA_def_property_ui_text(prop, "Simplify Child Particles", "Global child particles percentage");
  RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

  prop = RNA_def_property(srna, "simplify_subdivision_render", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "simplify_subsurf_render");
  RNA_def_property_ui_range(prop, 0, 6, 1, -1);
  RNA_def_property_ui_text(
      prop, "Simplify Subdivision", "Global maximum subdivision level during rendering");
  RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

  prop = RNA_def_property(srna, "simplify_child_particles_render", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "simplify_particles_render");
  RNA_def_property_ui_text(
      prop, "Simplify Child Particles", "Global child particles percentage during rendering");
  RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

  prop = RNA_def_property(srna, "use_simplify_smoke_highres", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "simplify_smoke_ignore_highres", 1);
  RNA_def_property_ui_text(
      prop, "Use High-resolution Smoke", "Display high-resolution smoke in the viewport");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  /* Grease Pencil - Simplify Options */
  prop = RNA_def_property(srna, "simplify_gpencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "simplify_gpencil", SIMPLIFY_GPENCIL_ENABLE);
  RNA_def_property_ui_text(prop, "Simplify", "Simplify Grease Pencil drawing");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "simplify_gpencil_onplay", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "simplify_gpencil", SIMPLIFY_GPENCIL_ON_PLAY);
  RNA_def_property_ui_text(
      prop, "Simplify Playback", "Simplify Grease Pencil only during animation playback");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "simplify_gpencil_view_fill", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "simplify_gpencil", SIMPLIFY_GPENCIL_FILL);
  RNA_def_property_ui_text(prop, "Disable Fill", "Disable fill strokes in the viewport");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "simplify_gpencil_remove_lines", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "simplify_gpencil", SIMPLIFY_GPENCIL_REMOVE_FILL_LINE);
  RNA_def_property_ui_text(prop, "Disable Lines", "Disable external lines of fill strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "simplify_gpencil_view_modifier", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "simplify_gpencil", SIMPLIFY_GPENCIL_MODIFIER);
  RNA_def_property_ui_text(prop, "Disable Modifiers", "Do not apply modifiers in the viewport");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "simplify_gpencil_shader_fx", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "simplify_gpencil", SIMPLIFY_GPENCIL_FX);
  RNA_def_property_ui_text(prop, "Simplify Shaders", "Do not apply shader fx");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  prop = RNA_def_property(srna, "simplify_gpencil_blend", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "simplify_gpencil", SIMPLIFY_GPENCIL_BLEND);
  RNA_def_property_ui_text(prop, "Layers Blending", "Do not display blend layers");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

  /* persistent data */
  prop = RNA_def_property(srna, "use_persistent_data", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", R_PERSISTENT_DATA);
  RNA_def_property_ui_text(
      prop, "Persistent Data", "Keep render data around for faster re-renders");
  RNA_def_property_update(prop, 0, "rna_Scene_use_persistent_data_update");

  /* Freestyle line thickness options */
  prop = RNA_def_property(srna, "line_thickness_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "line_thickness_mode");
  RNA_def_property_enum_items(prop, freestyle_thickness_items);
  RNA_def_property_ui_text(
      prop, "Line Thickness Mode", "Line thickness mode for Freestyle line drawing");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "line_thickness", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_float_sdna(prop, NULL, "unit_line_thickness");
  RNA_def_property_range(prop, 0.f, 10000.f);
  RNA_def_property_ui_text(prop, "Line Thickness", "Line thickness in pixels");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  /* Bake Settings */
  prop = RNA_def_property(srna, "bake", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "bake");
  RNA_def_property_struct_type(prop, "BakeSettings");
  RNA_def_property_ui_text(prop, "Bake Data", "");

  /* Nestled Data  */
  /* *** Non-Animated *** */
  RNA_define_animate_sdna(false);
  rna_def_bake_data(brna);
  RNA_define_animate_sdna(true);

  /* *** Animated *** */

  /* Scene API */
  RNA_api_scene_render(srna);
}

/* scene.objects */
static void rna_def_scene_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  RNA_def_property_srna(cprop, "SceneObjects");
  srna = RNA_def_struct(brna, "SceneObjects", NULL);
  RNA_def_struct_sdna(srna, "Scene");
  RNA_def_struct_ui_text(srna, "Scene Objects", "All the of scene objects");
}

/* scene.timeline_markers */
static void rna_def_timeline_markers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "TimelineMarkers");
  srna = RNA_def_struct(brna, "TimelineMarkers", NULL);
  RNA_def_struct_sdna(srna, "Scene");
  RNA_def_struct_ui_text(srna, "Timeline Markers", "Collection of timeline markers");

  func = RNA_def_function(srna, "new", "rna_TimeLine_add");
  RNA_def_function_ui_description(func, "Add a keyframe to the curve");
  parm = RNA_def_string(func, "name", "Marker", 0, "", "New name for the marker (not unique)");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame",
                     1,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The frame for the new marker",
                     -MAXFRAME,
                     MAXFRAME);
  parm = RNA_def_pointer(func, "marker", "TimelineMarker", "", "Newly created timeline marker");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_TimeLine_remove");
  RNA_def_function_ui_description(func, "Remove a timeline marker");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "marker", "TimelineMarker", "", "Timeline marker to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  func = RNA_def_function(srna, "clear", "rna_TimeLine_clear");
  RNA_def_function_ui_description(func, "Remove all timeline markers");
}

/* scene.keying_sets */
static void rna_def_scene_keying_sets(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "KeyingSets");
  srna = RNA_def_struct(brna, "KeyingSets", NULL);
  RNA_def_struct_sdna(srna, "Scene");
  RNA_def_struct_ui_text(srna, "Keying Sets", "Scene keying sets");

  /* Add Keying Set */
  func = RNA_def_function(srna, "new", "rna_Scene_keying_set_new");
  RNA_def_function_ui_description(func, "Add a new Keying Set to Scene");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  /* name */
  RNA_def_string(func, "idname", "KeyingSet", 64, "IDName", "Internal identifier of Keying Set");
  RNA_def_string(func, "name", "KeyingSet", 64, "Name", "User visible name of Keying Set");
  /* returns the new KeyingSet */
  parm = RNA_def_pointer(func, "keyingset", "KeyingSet", "", "Newly created Keying Set");
  RNA_def_function_return(func, parm);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "KeyingSet");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(
      prop, "rna_Scene_active_keying_set_get", "rna_Scene_active_keying_set_set", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Active Keying Set", "Active Keying Set used to insert/delete keyframes");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "active_keyingset");
  RNA_def_property_int_funcs(prop,
                             "rna_Scene_active_keying_set_index_get",
                             "rna_Scene_active_keying_set_index_set",
                             NULL);
  RNA_def_property_ui_text(
      prop,
      "Active Keying Set Index",
      "Current Keying Set index (negative for 'builtin' and positive for 'absolute')");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);
}

static void rna_def_scene_keying_sets_all(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "KeyingSetsAll");
  srna = RNA_def_struct(brna, "KeyingSetsAll", NULL);
  RNA_def_struct_sdna(srna, "Scene");
  RNA_def_struct_ui_text(srna, "Keying Sets All", "All available keying sets");

  /* NOTE: no add/remove available here, without screwing up this amalgamated list... */

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "KeyingSet");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(
      prop, "rna_Scene_active_keying_set_get", "rna_Scene_active_keying_set_set", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Active Keying Set", "Active Keying Set used to insert/delete keyframes");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "active_keyingset");
  RNA_def_property_int_funcs(prop,
                             "rna_Scene_active_keying_set_index_get",
                             "rna_Scene_active_keying_set_index_set",
                             NULL);
  RNA_def_property_ui_text(
      prop,
      "Active Keying Set Index",
      "Current Keying Set index (negative for 'builtin' and positive for 'absolute')");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);
}

/* Runtime property, used to remember uv indices, used only in UV stitch for now.
 */
static void rna_def_selected_uv_element(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SelectedUvElement", "PropertyGroup");
  RNA_def_struct_ui_text(srna, "Selected UV Element", "");

  /* store the index to the UV element selected */
  prop = RNA_def_property(srna, "element_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_ui_text(prop, "Element Index", "");

  prop = RNA_def_property(srna, "face_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_ui_text(prop, "Face Index", "");
}

static void rna_def_display_safe_areas(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static float default_title[2] = {0.1f, 0.05f};
  static float default_action[2] = {0.035f, 0.035f};

  static float default_title_center[2] = {0.175f, 0.05f};
  static float default_action_center[2] = {0.15f, 0.05f};

  srna = RNA_def_struct(brna, "DisplaySafeAreas", NULL);
  RNA_def_struct_ui_text(srna, "Safe Areas", "Safe areas used in 3D view and the sequencer");
  RNA_def_struct_sdna(srna, "DisplaySafeAreas");

  /* SAFE AREAS */
  prop = RNA_def_property(srna, "title", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "title");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, default_title);
  RNA_def_property_ui_text(prop, "Title Safe Margins", "Safe area for text and graphics");
  RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, NULL);

  prop = RNA_def_property(srna, "action", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "action");
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_array_default(prop, default_action);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Action Safe Margins", "Safe area for general elements");
  RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, NULL);

  prop = RNA_def_property(srna, "title_center", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "title_center");
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_array_default(prop, default_title_center);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop,
                           "Center Title Safe Margins",
                           "Safe area for text and graphics in a different aspect ratio");
  RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, NULL);

  prop = RNA_def_property(srna, "action_center", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "action_center");
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_array_default(prop, default_action_center);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop,
                           "Center Action Safe Margins",
                           "Safe area for general elements in a different aspect ratio");
  RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, NULL);
}

static void rna_def_scene_display(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static float default_light_direction[3] = {-M_SQRT1_3, -M_SQRT1_3, M_SQRT1_3};

  srna = RNA_def_struct(brna, "SceneDisplay", NULL);
  RNA_def_struct_ui_text(srna, "Scene Display", "Scene display settings for 3d viewport");
  RNA_def_struct_sdna(srna, "SceneDisplay");

  prop = RNA_def_property(srna, "light_direction", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_float_sdna(prop, NULL, "light_direction");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_light_direction);
  RNA_def_property_ui_text(
      prop, "Light Direction", "Direction of the light for shadows and highlights");
  RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_set_update");

  prop = RNA_def_property(srna, "shadow_shift", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_default(prop, 0.1);
  RNA_def_property_ui_text(prop, "Shadow Shift", "Shadow termination angle");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.00f, 1.0f, 1, 2);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_set_update");

  prop = RNA_def_property(srna, "shadow_focus", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.0);
  RNA_def_property_ui_text(prop, "Shadow Focus", "Shadow factor hardness");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 2);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_set_update");

  prop = RNA_def_property(srna, "matcap_ssao_distance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_default(prop, 0.2f);
  RNA_def_property_ui_text(
      prop, "Distance", "Distance of object that contribute to the Cavity/Edge effect");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);

  prop = RNA_def_property(srna, "matcap_ssao_attenuation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Attenuation", "Attenuation constant");
  RNA_def_property_range(prop, 1.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 1.0f, 100.0f, 1, 3);

  prop = RNA_def_property(srna, "matcap_ssao_samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_default(prop, 16);
  RNA_def_property_ui_text(prop, "Samples", "Number of samples");
  RNA_def_property_range(prop, 1, 500);

  prop = RNA_def_property(srna, "render_aa", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_scene_display_aa_methods);
  RNA_def_property_ui_text(
      prop, "Render Anti-Aliasing", "Method of anti-aliasing when rendering final image");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "viewport_aa", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_scene_display_aa_methods);
  RNA_def_property_ui_text(
      prop, "Viewport Anti-Aliasing", "Method of anti-aliasing when rendering 3d viewport");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  /* OpenGL render engine settings. */
  prop = RNA_def_property(srna, "shading", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Shading Settings", "Shading settings for OpenGL render engine");
}

static void rna_def_scene_eevee(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem eevee_shadow_method_items[] = {
      {SHADOW_ESM, "ESM", 0, "ESM", "Exponential Shadow Mapping"},
      {SHADOW_VSM, "VSM", 0, "VSM", "Variance Shadow Mapping"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem eevee_shadow_size_items[] = {
      {64, "64", 0, "64px", ""},
      {128, "128", 0, "128px", ""},
      {256, "256", 0, "256px", ""},
      {512, "512", 0, "512px", ""},
      {1024, "1024", 0, "1024px", ""},
      {2048, "2048", 0, "2048px", ""},
      {4096, "4096", 0, "4096px", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem eevee_gi_visibility_size_items[] = {
      {8, "8", 0, "8px", ""},
      {16, "16", 0, "16px", ""},
      {32, "32", 0, "32px", ""},
      {64, "64", 0, "64px", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem eevee_volumetric_tile_size_items[] = {
      {2, "2", 0, "2px", ""},
      {4, "4", 0, "4px", ""},
      {8, "8", 0, "8px", ""},
      {16, "16", 0, "16px", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static float default_bloom_color[3] = {1.0f, 1.0f, 1.0f};

  srna = RNA_def_struct(brna, "SceneEEVEE", NULL);
  RNA_def_struct_path_func(srna, "rna_SceneEEVEE_path");
  RNA_def_struct_ui_text(srna, "Scene Display", "Scene display settings for 3d viewport");

  /* Indirect Lighting */
  prop = RNA_def_property(srna, "gi_diffuse_bounces", PROP_INT, PROP_NONE);
  RNA_def_property_int_default(prop, 3);
  RNA_def_property_ui_text(prop,
                           "Diffuse Bounces",
                           "Number of time the light is reinjected inside light grids, "
                           "0 disable indirect diffuse light");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);

  prop = RNA_def_property(srna, "gi_cubemap_resolution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, eevee_shadow_size_items);
  RNA_def_property_enum_default(prop, 512);
  RNA_def_property_ui_text(prop, "Cubemap Size", "Size of every cubemaps");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);

  prop = RNA_def_property(srna, "gi_visibility_resolution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, eevee_gi_visibility_size_items);
  RNA_def_property_enum_default(prop, 32);
  RNA_def_property_ui_text(prop,
                           "Irradiance Visibility Size",
                           "Size of the shadow map applied to each irradiance sample");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);

  prop = RNA_def_property(srna, "gi_irradiance_smoothing", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 5, 2);
  RNA_def_property_float_default(prop, 0.1f);
  RNA_def_property_ui_text(prop,
                           "Irradiance Smoothing",
                           "Smoother irradiance interpolation but introduce light bleeding");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "gi_glossy_clamp", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(prop,
                           "Clamp Glossy",
                           "Clamp pixel intensity to reduce noise inside glossy reflections "
                           "from reflection cubemaps (0 to disabled)");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);

  prop = RNA_def_property(srna, "gi_filter_quality", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_default(prop, 3.0f);
  RNA_def_property_ui_text(
      prop, "Filter Quality", "Take more samples during cubemap filtering to remove artifacts");
  RNA_def_property_range(prop, 1.0f, 8.0f);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);

  prop = RNA_def_property(srna, "gi_show_irradiance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_SHOW_IRRADIANCE);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_icon(prop, ICON_HIDE_ON, 1);
  RNA_def_property_ui_text(
      prop, "Show Irradiance Cache", "Display irradiance samples in the viewport");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "gi_show_cubemaps", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_SHOW_CUBEMAPS);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_icon(prop, ICON_HIDE_ON, 1);
  RNA_def_property_ui_text(
      prop, "Show Cubemap Cache", "Display captured cubemaps in the viewport");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "gi_irradiance_display_size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "gi_irradiance_draw_size");
  RNA_def_property_range(prop, 0.05f, 10.0f);
  RNA_def_property_float_default(prop, 0.1f);
  RNA_def_property_ui_text(prop,
                           "Irradiance Display Size",
                           "Size of the irradiance sample spheres to debug captured light");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "gi_cubemap_display_size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "gi_cubemap_draw_size");
  RNA_def_property_range(prop, 0.05f, 10.0f);
  RNA_def_property_float_default(prop, 0.3f);
  RNA_def_property_ui_text(
      prop, "Cubemap Display Size", "Size of the cubemap spheres to debug captured light");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "gi_auto_bake", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_GI_AUTOBAKE);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_text(prop, "Auto Bake", "Auto bake indirect lighting when editing probes");

  prop = RNA_def_property(srna, "gi_cache_info", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "light_cache_info");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Light Cache Info", "Info on current cache status");

  /* Temporal Anti-Aliasing (super sampling) */
  prop = RNA_def_property(srna, "taa_samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_default(prop, 16);
  RNA_def_property_ui_text(prop, "Viewport Samples", "Number of samples, unlimited if 0");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "taa_render_samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_default(prop, 64);
  RNA_def_property_ui_text(prop, "Render Samples", "Number of samples per pixels for rendering");
  RNA_def_property_range(prop, 1, INT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_taa_reprojection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_TAA_REPROJECTION);
  RNA_def_property_boolean_default(prop, 1);
  RNA_def_property_ui_text(prop,
                           "Viewport Denoising",
                           "Denoise image using temporal reprojection "
                           "(can leave some ghosting)");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Screen Space Subsurface Scattering */
  prop = RNA_def_property(srna, "sss_samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_default(prop, 7);
  RNA_def_property_ui_text(prop, "Samples", "Number of samples to compute the scattering effect");
  RNA_def_property_range(prop, 1, 32);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "sss_jitter_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.3f);
  RNA_def_property_ui_text(
      prop, "Jitter Threshold", "Rotate samples that are below this threshold");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_sss_separate_albedo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_SSS_SEPARATE_ALBEDO);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_text(prop,
                           "Separate Albedo",
                           "Avoid albedo being blurred by the subsurface scattering "
                           "but uses more video memory");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Screen Space Reflection */
  prop = RNA_def_property(srna, "use_ssr", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_SSR_ENABLED);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_text(prop, "Screen Space Reflections", "Enable screen space reflection");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_ssr_refraction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_SSR_REFRACTION);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_text(prop, "Screen Space Refractions", "Enable screen space Refractions");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_ssr_halfres", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_SSR_HALF_RESOLUTION);
  RNA_def_property_boolean_default(prop, 1);
  RNA_def_property_ui_text(prop, "Half Res Trace", "Raytrace at a lower resolution");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "ssr_quality", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.25f);
  RNA_def_property_ui_text(prop, "Trace Precision", "Precision of the screen space raytracing");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "ssr_max_roughness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_ui_text(
      prop, "Max Roughness", "Do not raytrace reflections for roughness above this value");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "ssr_thickness", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_default(prop, 0.2f);
  RNA_def_property_ui_text(prop, "Thickness", "Pixel thickness used to detect intersection");
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 5, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "ssr_border_fade", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.075f);
  RNA_def_property_ui_text(prop, "Edge Fading", "Screen percentage used to fade the SSR");
  RNA_def_property_range(prop, 0.0f, 0.5f);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "ssr_firefly_fac", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_default(prop, 10.0f);
  RNA_def_property_ui_text(prop, "Clamp", "Clamp pixel intensity to remove noise (0 to disabled)");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Volumetrics */
  prop = RNA_def_property(srna, "volumetric_start", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_default(prop, 0.1f);
  RNA_def_property_ui_text(prop, "Start", "Start distance of the volumetric effect");
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "volumetric_end", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_default(prop, 100.0f);
  RNA_def_property_ui_text(prop, "End", "End distance of the volumetric effect");
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "volumetric_tile_size", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_default(prop, 8);
  RNA_def_property_enum_items(prop, eevee_volumetric_tile_size_items);
  RNA_def_property_ui_text(prop,
                           "Tile Size",
                           "Control the quality of the volumetric effects "
                           "(lower size increase vram usage and quality)");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "volumetric_samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_default(prop, 64);
  RNA_def_property_ui_text(prop, "Samples", "Number of samples to compute volumetric effects");
  RNA_def_property_range(prop, 1, 256);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "volumetric_sample_distribution", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.8f);
  RNA_def_property_ui_text(
      prop, "Exponential Sampling", "Distribute more samples closer to the camera");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_volumetric_lights", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_VOLUMETRIC_LIGHTS);
  RNA_def_property_boolean_default(prop, 1);
  RNA_def_property_ui_text(
      prop, "Volumetric Lighting", "Enable scene light interactions with volumetrics");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "volumetric_light_clamp", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Clamp", "Maximum light contribution, reducing noise");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_volumetric_shadows", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_VOLUMETRIC_SHADOWS);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_text(
      prop, "Volumetric Shadows", "Generate shadows from volumetric material (Very expensive)");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "volumetric_shadow_samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_default(prop, 16);
  RNA_def_property_range(prop, 1, 128);
  RNA_def_property_ui_text(
      prop, "Volumetric Shadow Samples", "Number of samples to compute volumetric shadowing");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Ambient Occlusion */
  prop = RNA_def_property(srna, "use_gtao", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_GTAO_ENABLED);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_text(prop,
                           "Ambient Occlusion",
                           "Enable ambient occlusion to simulate medium scale indirect shadowing");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_gtao_bent_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_GTAO_BENT_NORMALS);
  RNA_def_property_boolean_default(prop, 1);
  RNA_def_property_ui_text(
      prop, "Bent Normals", "Compute main non occluded direction to sample the environment");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_gtao_bounce", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_GTAO_BOUNCE);
  RNA_def_property_boolean_default(prop, 1);
  RNA_def_property_ui_text(prop,
                           "Bounces Approximation",
                           "An approximation to simulate light bounces "
                           "giving less occlusion on brighter objects");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "gtao_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Factor", "Factor for ambient occlusion blending");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 2);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "gtao_quality", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.25f);
  RNA_def_property_ui_text(prop, "Trace Precision", "Precision of the horizon search");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "gtao_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_default(prop, 0.2f);
  RNA_def_property_ui_text(
      prop, "Distance", "Distance of object that contribute to the ambient occlusion effect");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Depth of Field */
  prop = RNA_def_property(srna, "bokeh_max_size", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_float_default(prop, 100.0f);
  RNA_def_property_ui_text(
      prop, "Max Size", "Max size of the bokeh shape for the depth of field (lower is faster)");
  RNA_def_property_range(prop, 0.0f, 2000.0f);
  RNA_def_property_ui_range(prop, 2.0f, 200.0f, 1, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);

  prop = RNA_def_property(srna, "bokeh_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(
      prop, "Sprite Threshold", "Brightness threshold for using sprite base depth of field");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Bloom */
  prop = RNA_def_property(srna, "use_bloom", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_BLOOM_ENABLED);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_text(prop, "Bloom", "High brightness pixels generate a glowing effect");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "bloom_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.8f);
  RNA_def_property_ui_text(prop, "Threshold", "Filters out pixels under this level of brightness");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "bloom_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_array_default(prop, default_bloom_color);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Color", "Color applied to the bloom effect");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "bloom_knee", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_ui_text(prop, "Knee", "Makes transition between under/over-threshold gradual");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "bloom_radius", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 6.5f);
  RNA_def_property_ui_text(prop, "Radius", "Bloom spread distance");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "bloom_clamp", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(
      prop, "Clamp", "Maximum intensity a bloom pixel can have (0 to disabled)");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1000.0f, 1, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "bloom_intensity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 0.05f);
  RNA_def_property_ui_text(prop, "Intensity", "Blend factor");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 0.1f, 1, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Motion blur */
  prop = RNA_def_property(srna, "use_motion_blur", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_MOTION_BLUR_ENABLED);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_text(prop, "Motion Blur", "Enable motion blur effect (only in camera view)");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "motion_blur_samples", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_default(prop, 8);
  RNA_def_property_ui_text(prop, "Samples", "Number of samples to take with motion blur");
  RNA_def_property_range(prop, 1, 64);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "motion_blur_shutter", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Shutter", "Time taken in frames between shutter open and close");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.01f, 1.0f, 1, 2);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Shadows */
  prop = RNA_def_property(srna, "shadow_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_default(prop, SHADOW_ESM);
  RNA_def_property_enum_items(prop, eevee_shadow_method_items);
  RNA_def_property_ui_text(prop, "Method", "Technique use to compute the shadows");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "shadow_cube_size", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_default(prop, 512);
  RNA_def_property_enum_items(prop, eevee_shadow_size_items);
  RNA_def_property_ui_text(
      prop, "Cube Shadows Resolution", "Size of point and area light shadow maps");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "shadow_cascade_size", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_default(prop, 1024);
  RNA_def_property_enum_items(prop, eevee_shadow_size_items);
  RNA_def_property_ui_text(
      prop, "Directional Shadows Resolution", "Size of sun light shadow maps");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_shadow_high_bitdepth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_SHADOW_HIGH_BITDEPTH);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_text(prop, "High Bitdepth", "Use 32bit shadows");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "use_soft_shadows", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_SHADOW_SOFT);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_text(
      prop, "Soft Shadows", "Randomize shadowmaps origin to create soft shadows");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  prop = RNA_def_property(srna, "light_threshold", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_default(prop, 0.01f);
  RNA_def_property_ui_text(prop,
                           "Light Threshold",
                           "Minimum light intensity for a light to contribute to the lighting");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Overscan */
  prop = RNA_def_property(srna, "use_overscan", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_EEVEE_OVERSCAN);
  RNA_def_property_boolean_default(prop, 0);
  RNA_def_property_ui_text(prop,
                           "Overscan",
                           "Internally render past the image border to avoid "
                           "screen-space effects disappearing");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);

  prop = RNA_def_property(srna, "overscan_size", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, NULL, "overscan");
  RNA_def_property_float_default(prop, 3.0f);
  RNA_def_property_ui_text(prop,
                           "Overscan Size",
                           "Percentage of render size to add as overscan to the "
                           "internal render buffers");
  RNA_def_property_range(prop, 0.0f, 50.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 2);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
}

void RNA_def_scene(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem audio_distance_model_items[] = {
      {0, "NONE", 0, "None", "No distance attenuation"},
      {1, "INVERSE", 0, "Inverse", "Inverse distance model"},
      {2, "INVERSE_CLAMPED", 0, "Inverse Clamped", "Inverse distance model with clamping"},
      {3, "LINEAR", 0, "Linear", "Linear distance model"},
      {4, "LINEAR_CLAMPED", 0, "Linear Clamped", "Linear distance model with clamping"},
      {5, "EXPONENT", 0, "Exponent", "Exponent distance model"},
      {6, "EXPONENT_CLAMPED", 0, "Exponent Clamped", "Exponent distance model with clamping"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem sync_mode_items[] = {
      {0, "NONE", 0, "No Sync", "Do not sync, play every frame"},
      {SCE_FRAME_DROP, "FRAME_DROP", 0, "Frame Dropping", "Drop frames if playback is too slow"},
      {AUDIO_SYNC, "AUDIO_SYNC", 0, "AV-sync", "Sync to audio playback, dropping frames"},
      {0, NULL, 0, NULL, NULL},
  };

  /* Struct definition */
  srna = RNA_def_struct(brna, "Scene", "ID");
  RNA_def_struct_ui_text(srna,
                         "Scene",
                         "Scene data-block, consisting in objects and "
                         "defining time and render related settings");
  RNA_def_struct_ui_icon(srna, ICON_SCENE_DATA);
  RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);

  /* Global Settings */
  prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Camera_object_poll");
  RNA_def_property_ui_text(prop, "Camera", "Active camera, used for rendering the scene");
  RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_camera_update");

  prop = RNA_def_property(srna, "background_set", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "set");
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_pointer_funcs(prop, NULL, "rna_Scene_set_set", NULL, NULL);
  RNA_def_property_ui_text(prop, "Background Scene", "Background set scene");
  RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_set_update");

  prop = RNA_def_property(srna, "world", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "World", "World used for rendering the scene");
  RNA_def_property_update(prop, NC_SCENE | ND_WORLD, "rna_Scene_world_update");

  prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(prop, "Objects", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Scene_objects_begin",
                                    "rna_Scene_objects_next",
                                    "rna_Scene_objects_end",
                                    "rna_Scene_objects_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  rna_def_scene_objects(brna, prop);

  /* Frame Range Stuff */
  prop = RNA_def_property(srna, "frame_current", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, NULL, "r.cfra");
  RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
  RNA_def_property_int_funcs(prop, NULL, "rna_Scene_frame_current_set", NULL);
  RNA_def_property_ui_text(
      prop,
      "Current Frame",
      "Current Frame, to update animation data from python frame_set() instead");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_frame_update");

  prop = RNA_def_property(srna, "frame_subframe", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, NULL, "r.subframe");
  RNA_def_property_ui_text(prop, "Current Sub-Frame", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 2);
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_frame_update");

  prop = RNA_def_property(srna, "frame_float", PROP_FLOAT, PROP_TIME);
  RNA_def_property_ui_text(prop, "Current Sub-Frame", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
  RNA_def_property_ui_range(prop, MINAFRAME, MAXFRAME, 0.1, 2);
  RNA_def_property_float_funcs(
      prop, "rna_Scene_frame_float_get", "rna_Scene_frame_float_set", NULL);
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_frame_update");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, NULL, "r.sfra");
  RNA_def_property_int_funcs(prop, NULL, "rna_Scene_start_frame_set", NULL);
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_int_default(prop, 1);
  RNA_def_property_ui_text(prop, "Start Frame", "First frame of the playback/rendering range");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME_RANGE, NULL);

  prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, NULL, "r.efra");
  RNA_def_property_int_funcs(prop, NULL, "rna_Scene_end_frame_set", NULL);
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_int_default(prop, 250);
  RNA_def_property_ui_text(prop, "End Frame", "Final frame of the playback/rendering range");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME_RANGE, NULL);

  prop = RNA_def_property(srna, "frame_step", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, NULL, "r.frame_step");
  RNA_def_property_range(prop, 0, MAXFRAME);
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);
  RNA_def_property_ui_text(
      prop,
      "Frame Step",
      "Number of frames to skip forward while rendering/playing back each frame");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);

  prop = RNA_def_property(srna, "frame_current_final", PROP_FLOAT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
  RNA_def_property_float_funcs(prop, "rna_Scene_frame_current_final_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Current Frame Final", "Current frame with subframe and time remapping applied");

  prop = RNA_def_property(srna, "lock_frame_selection_to_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, NULL, "r.flag", SCER_LOCK_FRAME_SELECTION);
  RNA_def_property_ui_text(prop,
                           "Lock Frame Selection",
                           "Don't allow frame to be selected with mouse outside of frame range");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);

  /* Preview Range (frame-range for UI playback) */
  prop = RNA_def_property(srna, "use_preview_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, NULL, "r.flag", SCER_PRV_RANGE);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_Scene_use_preview_range_set");
  RNA_def_property_ui_text(
      prop,
      "Use Preview Range",
      "Use an alternative start/end frame range for animation playback and "
      "OpenGL renders instead of the Render properties start/end frame range");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);
  RNA_def_property_ui_icon(prop, ICON_PREVIEW_RANGE, 0);

  prop = RNA_def_property(srna, "frame_preview_start", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, NULL, "r.psfra");
  RNA_def_property_int_funcs(prop, NULL, "rna_Scene_preview_range_start_frame_set", NULL);
  RNA_def_property_ui_text(
      prop, "Preview Range Start Frame", "Alternative start frame for UI playback");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);

  prop = RNA_def_property(srna, "frame_preview_end", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, NULL, "r.pefra");
  RNA_def_property_int_funcs(prop, NULL, "rna_Scene_preview_range_end_frame_set", NULL);
  RNA_def_property_ui_text(
      prop, "Preview Range End Frame", "Alternative end frame for UI playback");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);

  /* Subframe for moblur debug. */
  prop = RNA_def_property(srna, "show_subframe", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, NULL, "r.flag", SCER_SHOW_SUBFRAME);
  RNA_def_property_ui_text(
      prop, "Show Subframe", "Show current scene subframe and allow set it using interface tools");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_show_subframe_update");

  /* Timeline / Time Navigation settings */
  prop = RNA_def_property(srna, "show_keys_from_selected_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SCE_KEYS_NO_SELONLY);
  RNA_def_property_ui_text(prop,
                           "Only Keyframes from Selected Channels",
                           "Consider keyframes for active Object and/or its selected bones only "
                           "(in timeline and when jumping between keyframes)");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);

  /* Stamp */
  prop = RNA_def_property(srna, "use_stamp_note", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "r.stamp_udata");
  RNA_def_property_ui_text(prop, "Stamp Note", "User defined note for the render stamping");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

  /* Animation Data (for Scene) */
  rna_def_animdata_common(srna);

  /* Readonly Properties */
  prop = RNA_def_property(srna, "is_nla_tweakmode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_NLA_EDIT_ON);
  RNA_def_property_clear_flag(prop,
                              PROP_EDITABLE); /* DO NOT MAKE THIS EDITABLE, OR NLA EDITOR BREAKS */
  RNA_def_property_ui_text(
      prop,
      "NLA TweakMode",
      "Whether there is any action referenced by NLA being edited (strictly read-only)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  /* Frame dropping flag for playback and sync enum */
#  if 0 /* XXX: Is this actually needed? */
  prop = RNA_def_property(srna, "use_frame_drop", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_FRAME_DROP);
  RNA_def_property_ui_text(
      prop, "Frame Dropping", "Play back dropping frames if frame display is too slow");
  RNA_def_property_update(prop, NC_SCENE, NULL);
#  endif

  prop = RNA_def_property(srna, "sync_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop, "rna_Scene_sync_mode_get", "rna_Scene_sync_mode_set", NULL);
  RNA_def_property_enum_items(prop, sync_mode_items);
  RNA_def_property_enum_default(prop, AUDIO_SYNC);
  RNA_def_property_ui_text(prop, "Sync Mode", "How to sync playback");
  RNA_def_property_update(prop, NC_SCENE, NULL);

  /* Nodes (Compositing) */
  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
  RNA_def_property_ui_text(prop, "Node Tree", "Compositing node tree");

  prop = RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_nodes", 1);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Use Nodes", "Enable the compositing node tree");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_use_nodes_update");

  /* Sequencer */
  prop = RNA_def_property(srna, "sequence_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "ed");
  RNA_def_property_struct_type(prop, "SequenceEditor");
  RNA_def_property_ui_text(prop, "Sequence Editor", "");

  /* Keying Sets */
  prop = RNA_def_property(srna, "keying_sets", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "keyingsets", NULL);
  RNA_def_property_struct_type(prop, "KeyingSet");
  RNA_def_property_ui_text(prop, "Absolute Keying Sets", "Absolute Keying Sets for this Scene");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);
  rna_def_scene_keying_sets(brna, prop);

  prop = RNA_def_property(srna, "keying_sets_all", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Scene_all_keyingsets_begin",
                                    "rna_Scene_all_keyingsets_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "KeyingSet");
  RNA_def_property_ui_text(
      prop,
      "All Keying Sets",
      "All Keying Sets available for use (Builtins and Absolute Keying Sets for this Scene)");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);
  rna_def_scene_keying_sets_all(brna, prop);

  /* Rigid Body Simulation */
  prop = RNA_def_property(srna, "rigidbody_world", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "rigidbody_world");
  RNA_def_property_struct_type(prop, "RigidBodyWorld");
  RNA_def_property_ui_text(prop, "Rigid Body World", "");
  RNA_def_property_update(prop, NC_SCENE, NULL);

  /* Tool Settings */
  prop = RNA_def_property(srna, "tool_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "toolsettings");
  RNA_def_property_struct_type(prop, "ToolSettings");
  RNA_def_property_ui_text(prop, "Tool Settings", "");

  /* Unit Settings */
  prop = RNA_def_property(srna, "unit_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "unit");
  RNA_def_property_struct_type(prop, "UnitSettings");
  RNA_def_property_ui_text(prop, "Unit Settings", "Unit editing settings");

  /* Physics Settings */
  prop = RNA_def_property(srna, "gravity", PROP_FLOAT, PROP_ACCELERATION);
  RNA_def_property_float_sdna(prop, NULL, "physics_settings.gravity");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_range(prop, -200.0f, 200.0f, 1, 2);
  RNA_def_property_ui_text(prop, "Gravity", "Constant acceleration in a given direction");
  RNA_def_property_update(prop, 0, "rna_Physics_update");

  prop = RNA_def_property(srna, "use_gravity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "physics_settings.flag", PHYS_GLOBAL_GRAVITY);
  RNA_def_property_ui_text(prop, "Global Gravity", "Use global gravity for all dynamics");
  RNA_def_property_update(prop, 0, "rna_Physics_update");

  /* Render Data */
  prop = RNA_def_property(srna, "render", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "r");
  RNA_def_property_struct_type(prop, "RenderSettings");
  RNA_def_property_ui_text(prop, "Render Data", "");

  /* Safe Areas */
  prop = RNA_def_property(srna, "safe_areas", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "safe_areas");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "DisplaySafeAreas");
  RNA_def_property_ui_text(prop, "Safe Areas", "");

  /* Markers */
  prop = RNA_def_property(srna, "timeline_markers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "markers", NULL);
  RNA_def_property_struct_type(prop, "TimelineMarker");
  RNA_def_property_ui_text(
      prop, "Timeline Markers", "Markers used in all timelines for the current scene");
  rna_def_timeline_markers(brna, prop);

  /* Transform Orientations */
  prop = RNA_def_property(srna, "transform_orientation_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Scene_transform_orientation_slots_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Scene_transform_orientation_slots_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "TransformOrientationSlot");
  RNA_def_property_ui_text(prop, "Transform Orientation Slots", "");

  /* 3D View Cursor */
  prop = RNA_def_property(srna, "cursor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "cursor");
  RNA_def_property_struct_type(prop, "View3DCursor");
  RNA_def_property_ui_text(prop, "3D Cursor", "");

  /* Audio Settings */
  prop = RNA_def_property(srna, "use_audio", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Scene_use_audio_get", "rna_Scene_use_audio_set");
  RNA_def_property_ui_text(
      prop, "Audio Muted", "Play back of audio from Sequence Editor will be muted");
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_use_audio_update");

#  if 0 /* XXX: Is this actually needed? */
  prop = RNA_def_property(srna, "use_audio_sync", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "audio.flag", AUDIO_SYNC);
  RNA_def_property_ui_text(
      prop,
      "Audio Sync",
      "Play back and sync with audio clock, dropping frames if frame display is too slow");
  RNA_def_property_update(prop, NC_SCENE, NULL);
#  endif

  prop = RNA_def_property(srna, "use_audio_scrub", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "audio.flag", AUDIO_SCRUB);
  RNA_def_property_ui_text(
      prop, "Audio Scrubbing", "Play audio from Sequence Editor while scrubbing");
  RNA_def_property_update(prop, NC_SCENE, NULL);

  prop = RNA_def_property(srna, "audio_doppler_speed", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "audio.speed_of_sound");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.01f, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Speed of Sound", "Speed of sound for Doppler effect calculation");
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_listener_update");

  prop = RNA_def_property(srna, "audio_doppler_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "audio.doppler_factor");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_text(prop, "Doppler Factor", "Pitch factor for Doppler effect calculation");
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_listener_update");

  prop = RNA_def_property(srna, "audio_distance_model", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "audio.distance_model");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, audio_distance_model_items);
  RNA_def_property_ui_text(
      prop, "Distance Model", "Distance model for distance attenuation calculation");
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_listener_update");

  prop = RNA_def_property(srna, "audio_volume", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "audio.volume");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Volume", "Audio volume");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
  RNA_def_property_update(prop, NC_SCENE, NULL);
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_volume_update");

  /* Statistics */
  func = RNA_def_function(srna, "statistics", "rna_Scene_statistics_string_get");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "view_layer", "ViewLayer", "", "Active layer");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "statistics", NULL, 0, "Statistics", "");
  RNA_def_function_return(func, parm);

  /* Grease Pencil */
  prop = RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "gpd");
  RNA_def_property_struct_type(prop, "GreasePencil");
  RNA_def_property_pointer_funcs(
      prop, NULL, NULL, NULL, "rna_GPencil_datablocks_annotations_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_ui_text(
      prop, "Annotations", "Grease Pencil data-block used for annotations in the 3D view");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  /* active MovieClip */
  prop = RNA_def_property(srna, "active_clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "clip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_ui_text(
      prop, "Active Movie Clip", "Active movie clip used for constraints and viewport drawing");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  /* color management */
  prop = RNA_def_property(srna, "view_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "view_settings");
  RNA_def_property_struct_type(prop, "ColorManagedViewSettings");
  RNA_def_property_ui_text(
      prop, "View Settings", "Color management settings applied on image before saving");

  prop = RNA_def_property(srna, "display_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "display_settings");
  RNA_def_property_struct_type(prop, "ColorManagedDisplaySettings");
  RNA_def_property_ui_text(
      prop, "Display Settings", "Settings of device saved image would be displayed on");

  prop = RNA_def_property(srna, "sequencer_colorspace_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "sequencer_colorspace_settings");
  RNA_def_property_struct_type(prop, "ColorManagedSequencerColorspaceSettings");
  RNA_def_property_ui_text(
      prop, "Sequencer Color Space Settings", "Settings of color space sequencer is working in");

  /* Layer and Collections */
  prop = RNA_def_property(srna, "view_layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "view_layers", NULL);
  RNA_def_property_struct_type(prop, "ViewLayer");
  RNA_def_property_ui_text(prop, "View Layers", "");
  rna_def_view_layers(brna, prop);

  prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "master_collection");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_ui_text(
      prop,
      "Collection",
      "Scene master collection that objects and other collections in the scene");

  /* Scene Display */
  prop = RNA_def_property(srna, "display", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "display");
  RNA_def_property_struct_type(prop, "SceneDisplay");
  RNA_def_property_ui_text(prop, "Scene Display", "Scene display settings for 3d viewport");

  /* EEVEE */
  prop = RNA_def_property(srna, "eevee", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SceneEEVEE");
  RNA_def_property_ui_text(prop, "EEVEE", "EEVEE settings for the scene");

  /* Nestled Data  */
  /* *** Non-Animated *** */
  RNA_define_animate_sdna(false);
  rna_def_tool_settings(brna);
  rna_def_gpencil_interpolate(brna);
  rna_def_unified_paint_settings(brna);
  rna_def_curve_paint_settings(brna);
  rna_def_statvis(brna);
  rna_def_unit_settings(brna);
  rna_def_scene_image_format_data(brna);
  rna_def_transform_orientation(brna);
  rna_def_transform_orientation_slot(brna);
  rna_def_view3d_cursor(brna);
  rna_def_selected_uv_element(brna);
  rna_def_display_safe_areas(brna);
  rna_def_scene_display(brna);
  rna_def_scene_eevee(brna);
  RNA_define_animate_sdna(true);
  /* *** Animated *** */
  rna_def_scene_render_data(brna);
  rna_def_gpu_fx(brna);
  rna_def_scene_render_view(brna);

  /* Scene API */
  RNA_api_scene(srna);
}

#endif
