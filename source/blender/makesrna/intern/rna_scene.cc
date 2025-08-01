/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_curve_types.h"
#include "DNA_layer_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "IMB_colormanagement.hh"

#include "MOV_enums.hh"

#include "BLI_math_rotation.h"
#include "BLI_string_utf8_symbols.h"

#include "BLT_translation.hh"

#include "BKE_paint.hh"

#include "ED_object.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

/* Include for Bake Options */
#include "RE_pipeline.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "BLI_threads.h"

#ifdef WITH_IMAGE_OPENEXR
const EnumPropertyItem rna_enum_exr_codec_items[] = {
    {R_IMF_EXR_CODEC_NONE, "NONE", 0, "None", "No compression"},
    {R_IMF_EXR_CODEC_ZIP, "ZIP", 0, "ZIP", "Lossless zip compression of 16 row image blocks"},
    {R_IMF_EXR_CODEC_PIZ,
     "PIZ",
     0,
     "PIZ",
     "Lossless wavelet compression, effective for noisy/grainy images"},
    {R_IMF_EXR_CODEC_DWAA,
     "DWAA",
     0,
     "DWAA (lossy)",
     "JPEG-like lossy compression on 32 row image blocks"},
    {R_IMF_EXR_CODEC_DWAB,
     "DWAB",
     0,
     "DWAB (lossy)",
     "JPEG-like lossy compression on 256 row image blocks"},
    {R_IMF_EXR_CODEC_ZIPS,
     "ZIPS",
     0,
     "ZIPS",
     "Lossless zip compression, each image row compressed separately"},
    {R_IMF_EXR_CODEC_RLE, "RLE", 0, "RLE", "Lossless run length encoding compression"},
    {R_IMF_EXR_CODEC_PXR24,
     "PXR24",
     0,
     "Pxr24 (lossy)",
     "Lossy compression for 32 bit float images (stores 24 bits of each float)"},
    {R_IMF_EXR_CODEC_B44,
     "B44",
     0,
     "B44 (lossy)",
     "Lossy compression for 16 bit float images, at fixed 2.3:1 ratio"},
    {R_IMF_EXR_CODEC_B44A,
     "B44A",
     0,
     "B44A (lossy)",
     "Lossy compression for 16 bit float images, at fixed 2.3:1 ratio"},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

const EnumPropertyItem rna_enum_snap_source_items[] = {
    {SCE_SNAP_SOURCE_CLOSEST, "CLOSEST", 0, "Closest", "Snap closest point onto target"},
    {SCE_SNAP_SOURCE_CENTER, "CENTER", 0, "Center", "Snap transformation center onto target"},
    {SCE_SNAP_SOURCE_MEDIAN, "MEDIAN", 0, "Median", "Snap median onto target"},
    {SCE_SNAP_SOURCE_ACTIVE, "ACTIVE", 0, "Active", "Snap active onto target"},
    {0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
};

/* subset of the enum - only curves, missing random and const */
const EnumPropertyItem rna_enum_proportional_falloff_curve_only_items[] = {
    {PROP_SMOOTH, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", "Smooth falloff"},
    {PROP_SPHERE, "SPHERE", ICON_SPHERECURVE, "Sphere", "Spherical falloff"},
    {PROP_ROOT, "ROOT", ICON_ROOTCURVE, "Root", "Root falloff"},
    {PROP_INVSQUARE, "INVERSE_SQUARE", ICON_ROOTCURVE, "Inverse Square", "Inverse Square falloff"},
    {PROP_SHARP, "SHARP", ICON_SHARPCURVE, "Sharp", "Sharp falloff"},
    {PROP_LIN, "LINEAR", ICON_LINCURVE, "Linear", "Linear falloff"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Keep for operators, not used here. */

const EnumPropertyItem rna_enum_mesh_select_mode_items[] = {
    {SCE_SELECT_VERTEX, "VERT", ICON_VERTEXSEL, "Vertex", "Vertex selection mode"},
    {SCE_SELECT_EDGE, "EDGE", ICON_EDGESEL, "Edge", "Edge selection mode"},
    {SCE_SELECT_FACE, "FACE", ICON_FACESEL, "Face", "Face selection mode"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_mesh_select_mode_uv_items[] = {
    {UV_SELECT_VERTEX, "VERTEX", ICON_UV_VERTEXSEL, "Vertex", "Vertex selection mode"},
    {UV_SELECT_EDGE, "EDGE", ICON_UV_EDGESEL, "Edge", "Edge selection mode"},
    {UV_SELECT_FACE, "FACE", ICON_UV_FACESEL, "Face", "Face selection mode"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* clang-format off */
#define RNA_SNAP_ELEMENTS_BASE \
  {SCE_SNAP_TO_INCREMENT, "INCREMENT", ICON_SNAP_INCREMENT, "Increment", "Snap to increments"}, \
  {SCE_SNAP_TO_GRID, "GRID", ICON_SNAP_GRID, "Grid", "Snap to grid"}, \
  {SCE_SNAP_TO_VERTEX, "VERTEX", ICON_SNAP_VERTEX, "Vertex", "Snap to vertices"}, \
  {SCE_SNAP_TO_EDGE, "EDGE", ICON_SNAP_EDGE, "Edge", "Snap to edges"}, \
  {SCE_SNAP_TO_FACE, "FACE", ICON_SNAP_FACE, "Face", "Snap by projecting onto faces"}, \
  {SCE_SNAP_TO_VOLUME, "VOLUME", ICON_SNAP_VOLUME, "Volume", "Snap to volume"}, \
  {SCE_SNAP_TO_EDGE_MIDPOINT, "EDGE_MIDPOINT", ICON_SNAP_MIDPOINT, "Edge Center", "Snap to the middle of edges"}, \
  {SCE_SNAP_TO_EDGE_PERPENDICULAR, "EDGE_PERPENDICULAR", ICON_SNAP_PERPENDICULAR, "Edge Perpendicular", "Snap to the nearest point on an edge"}
/* clang-format on */

const EnumPropertyItem rna_enum_snap_element_items[] = {
    RNA_SNAP_ELEMENTS_BASE,
    {SCE_SNAP_INDIVIDUAL_PROJECT,
     "FACE_PROJECT",
     ICON_SNAP_FACE,
     "Face Project",
     "Snap by projecting onto faces"},
    {SCE_SNAP_INDIVIDUAL_NEAREST,
     "FACE_NEAREST",
     ICON_SNAP_FACE_NEAREST,
     "Face Nearest",
     "Snap to nearest point on faces"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_snap_element_base_items[] = {
    RNA_SNAP_ELEMENTS_BASE,
    {0, nullptr, 0, nullptr, nullptr},
};

#ifndef RNA_RUNTIME
/* Last two snap elements from #rna_enum_snap_element_items. */
static const EnumPropertyItem *rna_enum_snap_element_individual_items =
    &rna_enum_snap_element_items[ARRAY_SIZE(rna_enum_snap_element_items) - 3];
#endif

const EnumPropertyItem rna_enum_snap_animation_element_items[] = {
    {SCE_SNAP_TO_FRAME, "FRAME", 0, "Frame", "Snap to frame"},
    {SCE_SNAP_TO_SECOND, "SECOND", 0, "Second", "Snap to seconds"},
    {SCE_SNAP_TO_MARKERS, "MARKER", 0, "Nearest Marker", "Snap to nearest marker"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem snap_uv_element_items[] = {
    {SCE_SNAP_TO_INCREMENT,
     "INCREMENT",
     ICON_SNAP_INCREMENT,
     "Increment",
     "Snap to increments of grid"},
    {SCE_SNAP_TO_GRID, "GRID", ICON_SNAP_GRID, "Grid", "Snap to grid"},
    {SCE_SNAP_TO_VERTEX, "VERTEX", ICON_SNAP_VERTEX, "Vertex", "Snap to vertices"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_snap_playhead_element_items[] = {
    {SCE_SNAP_TO_FRAME, "FRAME", 0, "Frames", "Snap to frame increments"},
    {SCE_SNAP_TO_SECOND, "SECOND", 0, "Seconds", "Snap to second increments"},
    {SCE_SNAP_TO_MARKERS, "MARKER", 0, "Markers", "Snap to markers"},
    {SCE_SNAP_TO_KEYS, "KEY", 0, "Keyframes", "Snap to keyframes"},
    {SCE_SNAP_TO_STRIPS, "Strip", 0, "Strips", "Snap to Strips"},
    {0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

const EnumPropertyItem rna_enum_curve_fit_method_items[] = {
    {CURVE_PAINT_FIT_METHOD_REFIT,
     "REFIT",
     0,
     "Refit",
     "Incrementally refit the curve (high quality)"},
    {CURVE_PAINT_FIT_METHOD_SPLIT,
     "SPLIT",
     0,
     "Split",
     "Split the curve until the tolerance is met (fast)"},
    {0, nullptr, 0, nullptr, nullptr},
};

#define MEDIA_TYPE_ENUM_IMAGE \
  { \
    MEDIA_TYPE_IMAGE, "IMAGE", ICON_NONE, "Image", "" \
  }
#define MEDIA_TYPE_ENUM_MULTI_LAYER_IMAGE \
  { \
    MEDIA_TYPE_MULTI_LAYER_IMAGE, "MULTI_LAYER_IMAGE", ICON_NONE, "Multi-Layer EXR", "" \
  }
#define MEDIA_TYPE_ENUM_VIDEO \
  { \
    MEDIA_TYPE_VIDEO, "VIDEO", ICON_NONE, "Video", "" \
  }

static const EnumPropertyItem rna_enum_media_type_all_items[] = {
    MEDIA_TYPE_ENUM_IMAGE,
    MEDIA_TYPE_ENUM_MULTI_LAYER_IMAGE,
    MEDIA_TYPE_ENUM_VIDEO,
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME
static const EnumPropertyItem rna_enum_media_type_image_items[] = {
    MEDIA_TYPE_ENUM_IMAGE,
    MEDIA_TYPE_ENUM_MULTI_LAYER_IMAGE,
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

/* workaround for duplicate enums,
 * have each enum line as a define then conditionally set it or not
 */

#define R_IMF_ENUM_BMP \
  {R_IMF_IMTYPE_BMP, "BMP", ICON_FILE_IMAGE, "BMP", "Output image in bitmap format"},
#define R_IMF_ENUM_IRIS \
  {R_IMF_IMTYPE_IRIS, "IRIS", ICON_FILE_IMAGE, "Iris", "Output image in SGI IRIS format"},
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
#  define R_IMF_ENUM_DDS \
    {R_IMF_IMTYPE_DDS, "DDS", ICON_FILE_IMAGE, "DDS", "Output image in DDS format"},
#endif

#ifdef WITH_IMAGE_OPENJPEG
#  define R_IMF_ENUM_JPEG2K \
    {R_IMF_IMTYPE_JP2, \
     "JPEG2000", \
     ICON_FILE_IMAGE, \
     "JPEG 2000", \
     "Output image in JPEG 2000 format"},
#else
#  define R_IMF_ENUM_JPEG2K
#endif

#ifdef WITH_IMAGE_CINEON
#  define R_IMF_ENUM_CINEON \
    {R_IMF_IMTYPE_CINEON, "CINEON", ICON_FILE_IMAGE, "Cineon", "Output image in Cineon format"},
#  define R_IMF_ENUM_DPX \
    {R_IMF_IMTYPE_DPX, "DPX", ICON_FILE_IMAGE, "DPX", "Output image in DPX format"},
#else
#  define R_IMF_ENUM_CINEON
#  define R_IMF_ENUM_DPX
#endif

#ifdef WITH_IMAGE_OPENEXR
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

#define R_IMF_ENUM_HDR \
  {R_IMF_IMTYPE_RADHDR, \
   "HDR", \
   ICON_FILE_IMAGE, \
   "Radiance HDR", \
   "Output image in Radiance HDR format"},

#define R_IMF_ENUM_TIFF \
  {R_IMF_IMTYPE_TIFF, "TIFF", ICON_FILE_IMAGE, "TIFF", "Output image in TIFF format"},

#ifdef WITH_IMAGE_WEBP
#  define R_IMF_ENUM_WEBP \
    {R_IMF_IMTYPE_WEBP, "WEBP", ICON_FILE_IMAGE, "WebP", "Output image in WebP format"},
#else
#  define R_IMF_ENUM_WEBP
#endif

#ifdef WITH_FFMPEG
#  define R_IMF_ENUM_FFMPEG {R_IMF_IMTYPE_FFMPEG, "FFMPEG", ICON_FILE_MOVIE, "FFmpeg Video", ""},
#else
#  define R_IMF_ENUM_FFMPEG
#endif

#define IMAGE_TYPE_ITEMS_IMAGE \
  R_IMF_ENUM_BMP \
  /* DDS save not supported yet R_IMF_ENUM_DDS */ \
  R_IMF_ENUM_IRIS \
  R_IMF_ENUM_PNG \
  R_IMF_ENUM_JPEG \
  R_IMF_ENUM_JPEG2K \
  R_IMF_ENUM_TAGA \
  R_IMF_ENUM_TAGA_RAW \
  RNA_ENUM_ITEM_SEPR_COLUMN, R_IMF_ENUM_CINEON R_IMF_ENUM_DPX R_IMF_ENUM_EXR R_IMF_ENUM_HDR \
                                 R_IMF_ENUM_TIFF R_IMF_ENUM_WEBP

#define IMAGE_TYPE_ITEMS_MULTI_LAYER_IMAGE R_IMF_ENUM_EXR_MULTILAYER

#define IMAGE_TYPE_ITEMS_VIDEO R_IMF_ENUM_FFMPEG

#ifdef RNA_RUNTIME
static const EnumPropertyItem image_type_items[] = {
    IMAGE_TYPE_ITEMS_IMAGE

    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem multi_layer_image_type_items[] = {
    IMAGE_TYPE_ITEMS_MULTI_LAYER_IMAGE

    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem video_image_type_items[] = {
    IMAGE_TYPE_ITEMS_VIDEO

    {0, nullptr, 0, nullptr, nullptr},
};
#endif

const EnumPropertyItem rna_enum_image_type_all_items[] = {
    IMAGE_TYPE_ITEMS_IMAGE IMAGE_TYPE_ITEMS_MULTI_LAYER_IMAGE IMAGE_TYPE_ITEMS_VIDEO

    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_image_color_mode_items[] = {
    {R_IMF_PLANES_BW,
     "BW",
     0,
     "BW",
     "Images get saved in 8-bit grayscale (only PNG, JPEG, TGA, TIF)"},
    {R_IMF_PLANES_RGB, "RGB", 0, "RGB", "Images are saved with RGB (color) data"},
    {R_IMF_PLANES_RGBA,
     "RGBA",
     0,
     "RGBA",
     "Images are saved with RGB and Alpha data (if supported)"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME
#  define IMAGE_COLOR_MODE_BW rna_enum_image_color_mode_items[0]
#  define IMAGE_COLOR_MODE_RGB rna_enum_image_color_mode_items[1]
#  define IMAGE_COLOR_MODE_RGBA rna_enum_image_color_mode_items[2]
#endif

const EnumPropertyItem rna_enum_image_color_depth_items[] = {
    /* 1 (monochrome) not used */
    {R_IMF_CHAN_DEPTH_8, "8", 0, "8", "8-bit color channels"},
    {R_IMF_CHAN_DEPTH_10, "10", 0, "10", "10-bit color channels"},
    {R_IMF_CHAN_DEPTH_12, "12", 0, "12", "12-bit color channels"},
    {R_IMF_CHAN_DEPTH_16, "16", 0, "16", "16-bit color channels"},
    /* 24 not used */
    {R_IMF_CHAN_DEPTH_32, "32", 0, "32", "32-bit color channels"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_normal_space_items[] = {
    {R_BAKE_SPACE_OBJECT, "OBJECT", 0, "Object", "Bake the normals in object space"},
    {R_BAKE_SPACE_TANGENT, "TANGENT", 0, "Tangent", "Bake the normals in tangent space"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_normal_swizzle_items[] = {
    {R_BAKE_POSX, "POS_X", 0, "+X", ""},
    {R_BAKE_POSY, "POS_Y", 0, "+Y", ""},
    {R_BAKE_POSZ, "POS_Z", 0, "+Z", ""},
    {R_BAKE_NEGX, "NEG_X", 0, "-X", ""},
    {R_BAKE_NEGY, "NEG_Y", 0, "-Y", ""},
    {R_BAKE_NEGZ, "NEG_Z", 0, "-Z", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_bake_margin_type_items[] = {
    {R_BAKE_ADJACENT_FACES,
     "ADJACENT_FACES",
     0,
     "Adjacent Faces",
     "Use pixels from adjacent faces across UV seams"},
    {R_BAKE_EXTEND, "EXTEND", 0, "Extend", "Extend border pixels outwards"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_bake_target_items[] = {
    {R_BAKE_TARGET_IMAGE_TEXTURES,
     "IMAGE_TEXTURES",
     0,
     "Image Textures",
     "Bake to image data-blocks associated with active image texture nodes in materials"},
    {R_BAKE_TARGET_VERTEX_COLORS,
     "VERTEX_COLORS",
     0,
     "Active Color Attribute",
     "Bake to the active color attribute on meshes"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_bake_save_mode_items[] = {
    {R_BAKE_SAVE_INTERNAL,
     "INTERNAL",
     0,
     "Internal",
     "Save the baking map in an internal image data-block"},
    {R_BAKE_SAVE_EXTERNAL, "EXTERNAL", 0, "External", "Save the baking map in an external file"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_bake_view_from_items[] = {
    {R_BAKE_VIEW_FROM_ABOVE_SURFACE,
     "ABOVE_SURFACE",
     0,
     "Above Surface",
     "Cast rays from above the surface"},
    {R_BAKE_VIEW_FROM_ACTIVE_CAMERA,
     "ACTIVE_CAMERA",
     0,
     "Active Camera",
     "Use the active camera's position to cast rays"},
    {0, nullptr, 0, nullptr, nullptr},
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
    R_IMF_VIEWS_ENUM_IND R_IMF_VIEWS_ENUM_S3D{0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_views_format_multilayer_items[] = {
    R_IMF_VIEWS_ENUM_IND R_IMF_VIEWS_ENUM_MV{0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_views_format_multiview_items[] = {
    R_IMF_VIEWS_ENUM_IND R_IMF_VIEWS_ENUM_S3D R_IMF_VIEWS_ENUM_MV{0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_stereo3d_anaglyph_type_items[] = {
    {S3D_ANAGLYPH_REDCYAN, "RED_CYAN", 0, "Red-Cyan", ""},
    {S3D_ANAGLYPH_GREENMAGENTA, "GREEN_MAGENTA", 0, "Green-Magenta", ""},
    {S3D_ANAGLYPH_YELLOWBLUE, "YELLOW_BLUE", 0, "Yellow-Blue", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_stereo3d_interlace_type_items[] = {
    {S3D_INTERLACE_ROW, "ROW_INTERLEAVED", 0, "Row Interleaved", ""},
    {S3D_INTERLACE_COLUMN, "COLUMN_INTERLEAVED", 0, "Column Interleaved", ""},
    {S3D_INTERLACE_CHECKERBOARD, "CHECKERBOARD_INTERLEAVED", 0, "Checkerboard Interleaved", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_bake_pass_filter_type_items[] = {
    {R_BAKE_PASS_FILTER_NONE, "NONE", 0, "None", ""},
    {R_BAKE_PASS_FILTER_EMIT, "EMIT", 0, "Emit", ""},
    {R_BAKE_PASS_FILTER_DIRECT, "DIRECT", 0, "Direct", ""},
    {R_BAKE_PASS_FILTER_INDIRECT, "INDIRECT", 0, "Indirect", ""},
    {R_BAKE_PASS_FILTER_COLOR, "COLOR", 0, "Color", ""},
    {R_BAKE_PASS_FILTER_DIFFUSE, "DIFFUSE", 0, "Diffuse", ""},
    {R_BAKE_PASS_FILTER_GLOSSY, "GLOSSY", 0, "Glossy", ""},
    {R_BAKE_PASS_FILTER_TRANSM, "TRANSMISSION", 0, "Transmission", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_view_layer_aov_type_items[] = {
    {AOV_TYPE_COLOR, "COLOR", 0, "Color", ""},
    {AOV_TYPE_VALUE, "VALUE", 0, "Value", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_transform_pivot_full_items[] = {
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
    {0, nullptr, 0, nullptr, nullptr},
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
    {V3D_ORIENT_PARENT,
     "PARENT",
     ICON_ORIENTATION_PARENT,
     "Parent",
     "Align the transformation axes to the object's parent space"},
    // {V3D_ORIENT_CUSTOM, "CUSTOM", 0, "Custom", "Use a custom transform orientation"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem plane_depth_items[] = {
    {V3D_PLACE_DEPTH_SURFACE,
     "SURFACE",
     0,
     "Surface",
     "Start placing on the surface, using the 3D cursor position as a fallback"},
    {V3D_PLACE_DEPTH_CURSOR_PLANE,
     "CURSOR_PLANE",
     0,
     "Cursor Plane",
     "Start placement using a point projected onto the orientation axis "
     "at the 3D cursor position"},
    {V3D_PLACE_DEPTH_CURSOR_VIEW,
     "CURSOR_VIEW",
     0,
     "Cursor View",
     "Start placement using a point projected onto the view plane at the 3D cursor position"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem plane_orientation_items[] = {
    {V3D_PLACE_ORIENT_SURFACE,
     "SURFACE",
     ICON_SNAP_NORMAL,
     "Surface",
     "Use the surface normal (using the transform orientation as a fallback)"},
    {V3D_PLACE_ORIENT_DEFAULT,
     "DEFAULT",
     ICON_ORIENTATION_GLOBAL,
     "Default",
     "Use the current transform orientation"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem snap_to_items[] = {
    {SCE_SNAP_TO_GEOM, "GEOMETRY", 0, "Geometry", "Snap to all geometry"},
    {SCE_SNAP_TO_NONE, "DEFAULT", 0, "Default", "Use the current snap settings"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_grease_pencil_selectmode_items[] = {
    {GP_SELECTMODE_POINT, "POINT", ICON_GP_SELECT_POINTS, "Point", "Select only points"},
    {GP_SELECTMODE_STROKE, "STROKE", ICON_GP_SELECT_STROKES, "Stroke", "Select all stroke points"},
    {GP_SELECTMODE_SEGMENT,
     "SEGMENT",
     ICON_GP_SELECT_BETWEEN_STROKES,
     "Segment",
     "Select all stroke points between other strokes"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem eevee_resolution_scale_items[] = {
    {1, "1", 0, "1:1", "Full resolution"},
    {2, "2", 0, "1:2", "Render this effect at 50% render resolution"},
    {4, "4", 0, "1:4", "Render this effect at 25% render resolution"},
    {8, "8", 0, "1:8", "Render this effect at 12.5% render resolution"},
    {16, "16", 0, "1:16", "Render this effect at 6.25% render resolution"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <algorithm>

#  include <fmt/format.h>

#  include "BLI_string_utils.hh"

#  include "DNA_anim_types.h"
#  include "DNA_cachefile_types.h"
#  include "DNA_color_types.h"
#  include "DNA_grease_pencil_types.h"
#  include "DNA_linestyle_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_node_types.h"
#  include "DNA_object_types.h"
#  include "DNA_particle_types.h"
#  include "DNA_text_types.h"
#  include "DNA_workspace_types.h"
#  include "DNA_world_types.h"

#  include "RNA_access.hh"

#  include "MEM_guardedalloc.h"

#  include "MOV_util.hh"

#  include "BKE_animsys.h"
#  include "BKE_armature.hh"
#  include "BKE_bake_geometry_nodes_modifier.hh"
#  include "BKE_brush.hh"
#  include "BKE_collection.hh"
#  include "BKE_context.hh"
#  include "BKE_editmesh.hh"
#  include "BKE_freestyle.h"
#  include "BKE_global.hh"
#  include "BKE_gpencil_legacy.h"
#  include "BKE_idprop.hh"
#  include "BKE_image.hh"
#  include "BKE_image_format.hh"
#  include "BKE_layer.hh"
#  include "BKE_main.hh"
#  include "BKE_main_invariants.hh"
#  include "BKE_mesh.hh"
#  include "BKE_node.hh"
#  include "BKE_node_legacy_types.hh"
#  include "BKE_node_runtime.hh"
#  include "BKE_pointcache.h"
#  include "BKE_scene.hh"
#  include "BKE_screen.hh"
#  include "BKE_unit.hh"

#  include "NOD_composite.hh"

#  include "ED_grease_pencil.hh"
#  include "ED_image.hh"
#  include "ED_info.hh"
#  include "ED_keyframing.hh"
#  include "ED_mesh.hh"
#  include "ED_node.hh"
#  include "ED_render.hh"
#  include "ED_scene.hh"
#  include "ED_uvedit.hh"
#  include "ED_view3d.hh"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"
#  include "DEG_depsgraph_query.hh"

#  include "SEQ_relations.hh"
#  include "SEQ_sequencer.hh"
#  include "SEQ_sound.hh"

#  ifdef WITH_FREESTYLE
#    include "FRS_freestyle.h"
#  endif

#  ifdef WITH_ALEMBIC
#    include "ABC_alembic.h"
#  endif

#  include "RE_engine.h"

#  include "ANIM_keyingsets.hh"

using blender::Vector;

static int rna_ToolSettings_snap_mode_get(PointerRNA *ptr)
{
  ToolSettings *ts = (ToolSettings *)(ptr->data);
  return ts->snap_mode;
}

static void rna_ToolSettings_snap_mode_set(PointerRNA *ptr, int value)
{
  ToolSettings *ts = (ToolSettings *)ptr->data;
  if (value != 0) {
    ts->snap_mode = value;
  }
}

static void rna_ToolSettings_snap_uv_mode_set(PointerRNA *ptr, int value)
{
  ToolSettings *ts = static_cast<ToolSettings *>(ptr->data);
  if (value != 0) {
    ts->snap_uv_mode = value;
  }
}

static void rna_Gpencil_mask_point_update(bContext *C, PointerRNA *ptr)
{
  ToolSettings *ts = (ToolSettings *)ptr->data;

  ts->gpencil_selectmode_sculpt &= ~GP_SCULPT_MASK_SELECTMODE_STROKE;
  ts->gpencil_selectmode_sculpt &= ~GP_SCULPT_MASK_SELECTMODE_SEGMENT;

  Object *ob = CTX_data_active_object(C);
  if (ob && ob->type == OB_GREASE_PENCIL) {
    blender::ed::greasepencil::ensure_selection_domain(ts, ob);
  }
}

static void rna_Gpencil_mask_stroke_update(bContext *C, PointerRNA *ptr)
{
  ToolSettings *ts = (ToolSettings *)ptr->data;

  ts->gpencil_selectmode_sculpt &= ~GP_SCULPT_MASK_SELECTMODE_POINT;
  ts->gpencil_selectmode_sculpt &= ~GP_SCULPT_MASK_SELECTMODE_SEGMENT;

  Object *ob = CTX_data_active_object(C);
  if (ob && ob->type == OB_GREASE_PENCIL) {
    blender::ed::greasepencil::ensure_selection_domain(ts, ob);
  }
}

static void rna_Gpencil_mask_segment_update(bContext *C, PointerRNA *ptr)
{
  ToolSettings *ts = (ToolSettings *)ptr->data;

  ts->gpencil_selectmode_sculpt &= ~GP_SCULPT_MASK_SELECTMODE_POINT;
  ts->gpencil_selectmode_sculpt &= ~GP_SCULPT_MASK_SELECTMODE_STROKE;

  Object *ob = CTX_data_active_object(C);
  if (ob && ob->type == OB_GREASE_PENCIL) {
    blender::ed::greasepencil::ensure_selection_domain(ts, ob);
  }
}

static void rna_Gpencil_vertex_mask_point_update(bContext *C, PointerRNA *ptr)
{
  ToolSettings *ts = (ToolSettings *)ptr->data;

  ts->gpencil_selectmode_vertex &= ~GP_VERTEX_MASK_SELECTMODE_STROKE;
  ts->gpencil_selectmode_vertex &= ~GP_VERTEX_MASK_SELECTMODE_SEGMENT;

  Object *ob = CTX_data_active_object(C);
  if (ob && ob->type == OB_GREASE_PENCIL) {
    blender::ed::greasepencil::ensure_selection_domain(ts, ob);
  }
}

static void rna_Gpencil_vertex_mask_stroke_update(bContext *C, PointerRNA *ptr)
{
  ToolSettings *ts = (ToolSettings *)ptr->data;

  ts->gpencil_selectmode_vertex &= ~GP_VERTEX_MASK_SELECTMODE_POINT;
  ts->gpencil_selectmode_vertex &= ~GP_VERTEX_MASK_SELECTMODE_SEGMENT;

  Object *ob = CTX_data_active_object(C);
  if (ob && ob->type == OB_GREASE_PENCIL) {
    blender::ed::greasepencil::ensure_selection_domain(ts, ob);
  }
}

static void rna_Gpencil_vertex_mask_segment_update(bContext *C, PointerRNA *ptr)
{
  ToolSettings *ts = (ToolSettings *)ptr->data;

  ts->gpencil_selectmode_vertex &= ~GP_VERTEX_MASK_SELECTMODE_POINT;
  ts->gpencil_selectmode_vertex &= ~GP_VERTEX_MASK_SELECTMODE_STROKE;

  Object *ob = CTX_data_active_object(C);
  if (ob && ob->type == OB_GREASE_PENCIL) {
    blender::ed::greasepencil::ensure_selection_domain(ts, ob);
  }
}

static void rna_all_grease_pencil_update(bContext *C, PointerRNA * /*ptr*/)
{
  /* FIXME: We shouldn't have to tag all the Grease Pencil IDs for an update! */
  Main *bmain = CTX_data_main(C);
  LISTBASE_FOREACH (GreasePencil *, grease_pencil, &bmain->grease_pencils) {
    DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  }
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
}

/* Read-only Iterator of all the scene objects. */

static void rna_Scene_objects_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;
  iter->internal.custom = MEM_callocN<BLI_Iterator>(__func__);

  BKE_scene_objects_iterator_begin(static_cast<BLI_Iterator *>(iter->internal.custom),
                                   (void *)scene);
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Scene_objects_next(CollectionPropertyIterator *iter)
{
  BKE_scene_objects_iterator_next(static_cast<BLI_Iterator *>(iter->internal.custom));
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Scene_objects_end(CollectionPropertyIterator *iter)
{
  BKE_scene_objects_iterator_end(static_cast<BLI_Iterator *>(iter->internal.custom));
  MEM_freeN(iter->internal.custom);
}

static PointerRNA rna_Scene_objects_get(CollectionPropertyIterator *iter)
{
  Object *ob = static_cast<Object *>(((BLI_Iterator *)iter->internal.custom)->current);
  return RNA_id_pointer_create(reinterpret_cast<ID *>(ob));
}

/* End of read-only Iterator of all the scene objects. */

static void rna_Scene_set_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
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

void rna_Scene_set_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update_ex(bmain, &scene->id, ID_RECALC_BASE_FLAGS);
  if (scene->set != nullptr) {
    /* Objects which are pulled into main scene's depsgraph needs to have
     * their base flags updated.
     */
    DEG_id_tag_update_ex(bmain, &scene->set->id, ID_RECALC_BASE_FLAGS);
  }
}

static void rna_Scene_camera_update(Main *bmain, Scene * /*scene_unused*/, PointerRNA *ptr)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  Scene *scene = (Scene *)ptr->data;

  WM_windows_scene_data_sync(&wm->windows, scene);
  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);
}

static void rna_Scene_fps_update(Main *bmain, Scene * /*active_scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_FPS | ID_RECALC_SEQUENCER_STRIPS);
  /* NOTE: Tag via dependency graph will take care of all the updates ion the evaluated domain,
   * however, changes in FPS actually modifies an original skip length,
   * so this we take care about here. */
  blender::seq::sound_update_length(bmain, scene);
  /* Reset simulation states because new frame interval doesn't apply anymore. */
  blender::bke::bake::scene_simulation_states_reset(*scene);
}

static void rna_Scene_listener_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_AUDIO_LISTENER);
}

static void rna_Scene_volume_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_VOLUME | ID_RECALC_SEQUENCER_STRIPS);
}

static const char *rna_Scene_statistics_string_get(Scene *scene,
                                                   Main *bmain,
                                                   ReportList *reports,
                                                   ViewLayer *view_layer)
{
  if (!BKE_scene_has_view_layer(scene, view_layer)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "View Layer '%s' not found in scene '%s'",
                view_layer->name,
                scene->id.name + 2);
    return "";
  }

  return ED_info_statistics_string(bmain, scene, view_layer);
}

static void rna_Scene_framelen_update(Main * /*bmain*/, Scene * /*active_scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  scene->r.framelen = float(scene->r.framapto) / float(scene->r.images);
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
  return float(data->r.cfra) + data->r.subframe;
}

static void rna_Scene_frame_float_set(PointerRNA *ptr, float value)
{
  Scene *data = (Scene *)ptr->data;
  /* if negative frames aren't allowed, then we can't use them */
  FRAMENUMBER_MIN_CLAMP(value);
  data->r.cfra = int(value);
  data->r.subframe = value - data->r.cfra;
}

static float rna_Scene_frame_current_final_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;

  return BKE_scene_frame_to_ctime(scene, float(scene->r.cfra));
}

static void rna_Scene_start_frame_set(PointerRNA *ptr, int value)
{
  Scene *data = (Scene *)ptr->data;
  /* MINFRAME not MINAFRAME, since some output formats can't taken negative frames */
  CLAMP(value, MINFRAME, MAXFRAME);
  data->r.sfra = value;

  if (value > data->r.efra) {
    data->r.efra = std::min(value, MAXFRAME);
  }
}

static void rna_Scene_end_frame_set(PointerRNA *ptr, int value)
{
  Scene *data = (Scene *)ptr->data;
  CLAMP(value, MINFRAME, MAXFRAME);
  data->r.efra = value;

  if (data->r.sfra > value) {
    data->r.sfra = std::max(value, MINFRAME);
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
  CLAMP(value, MINAFRAME, MAXFRAME);
  data->r.psfra = value;

  if (value > data->r.pefra) {
    data->r.pefra = std::min(value, MAXFRAME);
  }
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
  CLAMP(value, MINAFRAME, MAXFRAME);
  data->r.pefra = value;

  if (data->r.psfra > value) {
    data->r.psfra = std::max(value, MINAFRAME);
  }
}

static void rna_Scene_show_subframe_update(Main * /*bmain*/,
                                           Scene * /*current_scene*/,
                                           PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  scene->r.subframe = 0.0f;
}

static void rna_Scene_frame_update(Main * /*bmain*/, Scene * /*current_scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);
  WM_main_add_notifier(NC_SCENE | ND_FRAME, scene);
}

static PointerRNA rna_Scene_active_keying_set_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;
  return RNA_pointer_create_with_parent(
      *ptr, &RNA_KeyingSet, blender::animrig::scene_get_active_keyingset(scene));
}

static void rna_Scene_active_keying_set_set(PointerRNA *ptr,
                                            PointerRNA value,
                                            ReportList * /*reports*/)
{
  Scene *scene = (Scene *)ptr->data;
  KeyingSet *ks = (KeyingSet *)value.data;

  scene->active_keyingset = ANIM_scene_get_keyingset_index(scene, ks);
}

/* get KeyingSet index stuff for list of Keying Sets editing UI
 * - active_keyingset-1 since 0 is reserved for 'none'
 * - don't clamp, otherwise can never set builtin's types as active...
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

/* XXX: evil... builtin_keyingsets is defined in `blender::animrig::keyingsets.cc`! */
/* TODO: make API function to retrieve this... */
extern ListBase builtin_keyingsets;

static void rna_Scene_all_keyingsets_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;

  /* start going over the scene KeyingSets first, while we still have pointer to it
   * but only if we have any Keying Sets to use...
   */
  if (scene->keyingsets.first) {
    rna_iterator_listbase_begin(iter, ptr, &scene->keyingsets, nullptr);
  }
  else {
    rna_iterator_listbase_begin(iter, ptr, &builtin_keyingsets, nullptr);
  }
}

static void rna_Scene_all_keyingsets_next(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  KeyingSet *ks = (KeyingSet *)internal->link;

  /* If we've run out of links in Scene list,
   * jump over to the builtins list unless we're there already. */
  if ((ks->next == nullptr) && (ks != builtin_keyingsets.last)) {
    internal->link = (Link *)builtin_keyingsets.first;
  }
  else {
    internal->link = (Link *)ks->next;
  }

  iter->valid = (internal->link != nullptr);
}

static bool rna_Scene_compositing_node_group_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  bNodeTree *ntree = static_cast<bNodeTree *>(value.data);
  return ntree->type == NTREE_COMPOSIT;
}

static void rna_Scene_compositing_node_group_set(PointerRNA *ptr,
                                                 const PointerRNA value,
                                                 ReportList *reports)
{
  Scene *scene = static_cast<Scene *>(ptr->data);
  bNodeTree *ntree = static_cast<bNodeTree *>(value.data);
  if (ntree && ntree->type != NTREE_COMPOSIT) {
    BKE_reportf(
        reports, RPT_ERROR, "Node tree '%s' is not a compositing node group.", ntree->id.name + 2);
    return;
  }
  if (scene->compositing_node_group) {
    id_us_min(&scene->compositing_node_group->id);
  }
  scene->compositing_node_group = ntree;
  id_us_plus(&scene->compositing_node_group->id);
}

static std::optional<std::string> rna_SceneEEVEE_path(const PointerRNA * /*ptr*/)
{
  return "eevee";
}

static std::optional<std::string> rna_RaytraceEEVEE_path(const PointerRNA * /*ptr*/)
{
  return "eevee.ray_tracing_options";
}

static std::optional<std::string> rna_SceneGpencil_path(const PointerRNA * /*ptr*/)
{
  return "grease_pencil_settings";
}

static std::optional<std::string> rna_SceneHydra_path(const PointerRNA * /*ptr*/)
{
  return "hydra";
}

static bool rna_RenderSettings_stereoViews_skip(CollectionPropertyIterator *iter, void * /*data*/)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  SceneRenderView *srv = (SceneRenderView *)internal->link;

  if (STR_ELEM(srv->name, STEREO_LEFT_NAME, STEREO_RIGHT_NAME)) {
    return false;
  }

  return true;
};

static void rna_RenderSettings_stereoViews_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  RenderData *rd = (RenderData *)ptr->data;
  rna_iterator_listbase_begin(iter, ptr, &rd->views, rna_RenderSettings_stereoViews_skip);
}

static std::optional<std::string> rna_RenderSettings_path(const PointerRNA * /*ptr*/)
{
  return "render";
}

static std::optional<std::string> rna_BakeSettings_path(const PointerRNA * /*ptr*/)
{
  return "render.bake";
}

static std::optional<std::string> rna_ImageFormatSettings_path(
    const PointerRNA *ptr, blender::FunctionRef<bool(ImageFormatData *)> match)
{
  ID *id = ptr->owner_id;

  switch (GS(id->name)) {
    case ID_SCE: {
      Scene *scene = (Scene *)id;

      if (match(&scene->r.im_format)) {
        return "render.image_settings";
      }
      else if (match(&scene->r.bake.im_format)) {
        return "render.bake.image_settings";
      }
      return std::nullopt;
    }
    case ID_NT: {
      bNodeTree *ntree = (bNodeTree *)id;

      for (bNode *node : ntree->all_nodes()) {
        if (node->type_legacy == CMP_NODE_OUTPUT_FILE) {
          if (match(&((NodeImageMultiFile *)node->storage)->format)) {
            char node_name_esc[sizeof(node->name) * 2];
            BLI_str_escape(node_name_esc, node->name, sizeof(node_name_esc));
            return fmt::format("nodes[\"{}\"].format", node_name_esc);
          }
          else {
            LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
              NodeImageMultiFileSocket *sockdata = static_cast<NodeImageMultiFileSocket *>(
                  socket->storage);
              if (match(&sockdata->format)) {
                char node_name_esc[sizeof(node->name) * 2];
                BLI_str_escape(node_name_esc, node->name, sizeof(node_name_esc));

                char socketdata_path_esc[sizeof(sockdata->path) * 2];
                BLI_str_escape(socketdata_path_esc, sockdata->path, sizeof(socketdata_path_esc));

                return fmt::format(
                    "nodes[\"{}\"].file_slots[\"{}\"].format", node_name_esc, socketdata_path_esc);
              }
            }
          }
        }
      }
      return std::nullopt;
    }
    default:
      return std::nullopt;
  }
}

static std::optional<std::string> rna_ImageFormatSettings_path(const PointerRNA *ptr)
{
  ImageFormatData *data = static_cast<ImageFormatData *>(ptr->data);
  return rna_ImageFormatSettings_path(ptr, [&](ImageFormatData *imf) { return imf == data; });
}

std::optional<std::string> rna_ColorManagedDisplaySettings_path(const PointerRNA *ptr)
{
  ColorManagedDisplaySettings *data = static_cast<ColorManagedDisplaySettings *>(ptr->data);
  std::optional<std::string> path = rna_ImageFormatSettings_path(
      ptr, [&](ImageFormatData *imf) { return &imf->display_settings == data; });
  if (path) {
    return *path + ".display_settings";
  }
  if (GS(ptr->owner_id->name) == ID_SCE) {
    return "display_settings";
  }

  return std::nullopt;
}

std::optional<std::string> rna_ColorManagedViewSettings_path(const PointerRNA *ptr)
{
  ColorManagedViewSettings *data = static_cast<ColorManagedViewSettings *>(ptr->data);
  std::optional<std::string> path = rna_ImageFormatSettings_path(
      ptr, [&](ImageFormatData *imf) { return &imf->view_settings == data; });
  if (path) {
    return *path + ".view_settings";
  }
  if (GS(ptr->owner_id->name) == ID_SCE) {
    return "view_settings";
  }
  return std::nullopt;
}

std::optional<std::string> rna_ColorManagedInputColorspaceSettings_path(const PointerRNA *ptr)
{
  ColorManagedColorspaceSettings *data = static_cast<ColorManagedColorspaceSettings *>(ptr->data);
  std::optional<std::string> path = rna_ImageFormatSettings_path(
      ptr, [&](ImageFormatData *imf) { return &imf->linear_colorspace_settings == data; });
  if (path) {
    return *path + ".linear_colorspace_settings";
  }
  return std::nullopt;
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

static const EnumPropertyItem *rna_ImageFormatSettings_media_type_itemf(bContext * /*C*/,
                                                                        PointerRNA *ptr,
                                                                        PropertyRNA * /*prop*/,
                                                                        bool * /*r_free*/)
{
  ID *id = ptr->owner_id;
  /* Scene format setting include video, so we return all items. Otherwise, only image types are
   * returned. */
  if (id && GS(id->name) == ID_SCE) {
    return rna_enum_media_type_all_items;
  }
  else {
    return rna_enum_media_type_image_items;
  }
}

/* If the existing imtype does not match the new media type, assign an appropriate default media
 * type. */
static void rna_ImageFormatSettings_media_type_set(PointerRNA *ptr, int value)
{
  ImageFormatData *format = ptr->data_as<ImageFormatData>();
  BKE_image_format_media_type_set(format, ptr->owner_id, static_cast<MediaType>(value));
}

static void rna_ImageFormatSettings_file_format_set(PointerRNA *ptr, int value)
{
  BKE_image_format_set((ImageFormatData *)ptr->data, ptr->owner_id, value);
}

static const EnumPropertyItem *rna_ImageFormatSettings_file_format_itemf(bContext * /*C*/,
                                                                         PointerRNA *ptr,
                                                                         PropertyRNA * /*prop*/,
                                                                         bool * /*r_free*/)
{
  const ImageFormatData *format = ptr->data_as<ImageFormatData>();
  switch (static_cast<MediaType>(format->media_type)) {
    case MEDIA_TYPE_IMAGE:
      return image_type_items;
    case MEDIA_TYPE_MULTI_LAYER_IMAGE:
      return multi_layer_image_type_items;
    case MEDIA_TYPE_VIDEO:
      return video_image_type_items;
  }

  return rna_enum_image_type_all_items;
}

static const EnumPropertyItem *rna_ImageFormatSettings_color_mode_itemf(bContext * /*C*/,
                                                                        PointerRNA *ptr,
                                                                        PropertyRNA * /*prop*/,
                                                                        bool *r_free)
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;
  ID *id = ptr->owner_id;
  const bool is_render = (id && GS(id->name) == ID_SCE);

  /* NOTE(@ideasman42): we need to act differently for render
   * where 'BW' will force grayscale even if the output format writes
   * as RGBA, this is age old blender convention and not sure how useful
   * it really is but keep it for now. */
  char chan_flag = BKE_imtype_valid_channels(imf->imtype) | (is_render ? IMA_CHAN_FLAG_BW : 0);

  /* a WAY more crappy case than B&W flag: depending on codec, file format MIGHT support
   * alpha channel. for example MPEG format with h264 codec can't do alpha channel, but
   * the same MPEG format with QTRLE codec can easily handle alpha channel.
   * not sure how to deal with such cases in a nicer way (sergey) */
  if (is_render) {
    Scene *scene = (Scene *)ptr->owner_id;
    RenderData *rd = &scene->r;

    if (MOV_codec_supports_alpha(rd->ffcodecdata.codec_id_get(),
                                 rd->ffcodecdata.ffmpeg_prores_profile))
    {
      chan_flag |= IMA_CHAN_FLAG_RGBA;
    }
  }

  if (chan_flag == (IMA_CHAN_FLAG_BW | IMA_CHAN_FLAG_RGB | IMA_CHAN_FLAG_RGBA)) {
    return rna_enum_image_color_mode_items;
  }
  else {
    int totitem = 0;
    EnumPropertyItem *item = nullptr;

    if (chan_flag & IMA_CHAN_FLAG_BW) {
      RNA_enum_item_add(&item, &totitem, &IMAGE_COLOR_MODE_BW);
    }
    if (chan_flag & IMA_CHAN_FLAG_RGB) {
      RNA_enum_item_add(&item, &totitem, &IMAGE_COLOR_MODE_RGB);
    }
    if (chan_flag & IMA_CHAN_FLAG_RGBA) {
      RNA_enum_item_add(&item, &totitem, &IMAGE_COLOR_MODE_RGBA);
    }

    RNA_enum_item_end(&item, &totitem);
    *r_free = true;

    return item;
  }
}

static const EnumPropertyItem *rna_ImageFormatSettings_color_depth_itemf(bContext * /*C*/,
                                                                         PointerRNA *ptr,
                                                                         PropertyRNA * /*prop*/,
                                                                         bool *r_free)
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;

  if (imf == nullptr) {
    return rna_enum_image_color_depth_items;
  }
  else {
    const int depth_ok = BKE_imtype_valid_depths_with_video(imf->imtype, ptr->owner_id);
    const int is_float = ELEM(
        imf->imtype, R_IMF_IMTYPE_RADHDR, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER);

    const EnumPropertyItem *item_8bit = &rna_enum_image_color_depth_items[0];
    const EnumPropertyItem *item_10bit = &rna_enum_image_color_depth_items[1];
    const EnumPropertyItem *item_12bit = &rna_enum_image_color_depth_items[2];
    const EnumPropertyItem *item_16bit = &rna_enum_image_color_depth_items[3];
    const EnumPropertyItem *item_32bit = &rna_enum_image_color_depth_items[4];

    int totitem = 0;
    EnumPropertyItem *item = nullptr;
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
        tmp.name = N_("Float (Half)");
        if (ELEM(imf->imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
          tmp.description = N_(
              "16-bit color channels. Data passes like Depth will still be saved using full "
              "32-bit precision.");
        }
        RNA_enum_item_add(&item, &totitem, &tmp);
      }
      else {
        RNA_enum_item_add(&item, &totitem, item_16bit);
      }
    }

    if (depth_ok & R_IMF_CHAN_DEPTH_32) {
      if (is_float) {
        tmp = *item_32bit;
        tmp.name = N_("Float (Full)");
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

static const EnumPropertyItem *rna_ImageFormatSettings_views_format_itemf(bContext * /*C*/,
                                                                          PointerRNA *ptr,
                                                                          PropertyRNA * /*prop*/,
                                                                          bool * /*r_free*/)
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;

  if (imf == nullptr) {
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

#  ifdef WITH_IMAGE_OPENEXR
/* OpenEXR */

static const EnumPropertyItem *rna_ImageFormatSettings_exr_codec_itemf(bContext * /*C*/,
                                                                       PointerRNA *ptr,
                                                                       PropertyRNA * /*prop*/,
                                                                       bool *r_free)
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;

  EnumPropertyItem *item = nullptr;
  int i = 1, totitem = 0;

  if (imf->depth == 16) {
    return rna_enum_exr_codec_items; /* All compression types are defined for half-float. */
  }

  for (i = 0; i < R_IMF_EXR_CODEC_MAX; i++) {
    if (ELEM(rna_enum_exr_codec_items[i].value, R_IMF_EXR_CODEC_B44, R_IMF_EXR_CODEC_B44A)) {
      continue; /* B44 and B44A are not defined for 32 bit floats */
    }

    RNA_enum_item_add(&item, &totitem, &rna_enum_exr_codec_items[i]);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

#  endif

static bool rna_ImageFormatSettings_has_linear_colorspace_get(PointerRNA *ptr)
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;
  return BKE_imtype_requires_linear_float(imf->imtype);
}

static void rna_ImageFormatSettings_color_management_set(PointerRNA *ptr, int value)
{
  ImageFormatData *imf = (ImageFormatData *)ptr->data;

  if (imf->color_management != value) {
    imf->color_management = value;

    /* Copy from scene when enabling override. */
    if (imf->color_management == R_IMF_COLOR_MANAGEMENT_OVERRIDE) {
      ID *owner_id = ptr->owner_id;
      if (owner_id && GS(owner_id->name) == ID_NT) {
        /* For compositing nodes, find the corresponding scene. */
        owner_id = BKE_id_owner_get(owner_id);
      }
      if (owner_id && GS(owner_id->name) == ID_SCE) {
        BKE_image_format_color_management_copy_from_scene(imf, (Scene *)owner_id);
      }
    }
  }
}

static int rna_SceneRender_file_ext_length(PointerRNA *ptr)
{
  const RenderData *rd = (RenderData *)ptr->data;
  const char *ext_array[BKE_IMAGE_PATH_EXT_MAX];
  int ext_num = BKE_image_path_ext_from_imformat(&rd->im_format, ext_array);
  return ext_num ? strlen(ext_array[0]) : 0;
}

static void rna_SceneRender_file_ext_get(PointerRNA *ptr, char *value)
{
  const RenderData *rd = (RenderData *)ptr->data;
  const char *ext_array[BKE_IMAGE_PATH_EXT_MAX];
  int ext_num = BKE_image_path_ext_from_imformat(&rd->im_format, ext_array);
  strcpy(value, ext_num ? ext_array[0] : "");
}

#  ifdef WITH_FFMPEG
static void rna_FFmpegSettings_lossless_output_set(PointerRNA *ptr, bool value)
{
  Scene *scene = (Scene *)ptr->owner_id;
  RenderData *rd = &scene->r;

  if (value) {
    rd->ffcodecdata.flags |= FFMPEG_LOSSLESS_OUTPUT;
  }
  else {
    rd->ffcodecdata.flags &= ~FFMPEG_LOSSLESS_OUTPUT;
  }
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
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  RenderData *rd = (RenderData *)ptr->data;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&rd->views) - 1);
}

static PointerRNA rna_RenderSettings_active_view_get(PointerRNA *ptr)
{
  RenderData *rd = (RenderData *)ptr->data;
  SceneRenderView *srv = static_cast<SceneRenderView *>(BLI_findlink(&rd->views, rd->actview));

  return RNA_pointer_create_with_parent(*ptr, &RNA_SceneRenderView, srv);
}

static void rna_RenderSettings_active_view_set(PointerRNA *ptr,
                                               PointerRNA value,
                                               ReportList * /*reports*/)
{
  RenderData *rd = (RenderData *)ptr->data;
  SceneRenderView *srv = (SceneRenderView *)value.data;
  const int index = BLI_findindex(&rd->views, srv);
  if (index != -1) {
    rd->actview = index;
  }
}

static SceneRenderView *rna_RenderView_new(ID *id, RenderData * /*rd*/, const char *name)
{
  Scene *scene = (Scene *)id;
  SceneRenderView *srv = BKE_scene_add_render_view(scene, name);

  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  return srv;
}

static void rna_RenderView_remove(
    ID *id, RenderData * /*rd*/, Main * /*bmain*/, ReportList *reports, PointerRNA *srv_ptr)
{
  SceneRenderView *srv = static_cast<SceneRenderView *>(srv_ptr->data);
  Scene *scene = (Scene *)id;

  if (!BKE_scene_remove_render_view(scene, srv)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Render view '%s' could not be removed from scene '%s'",
                srv->name,
                scene->id.name + 2);
    return;
  }

  srv_ptr->invalidate();

  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, nullptr);
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
  RenderEngineType *type = static_cast<RenderEngineType *>(BLI_findlink(&R_engines, value));

  if (type) {
    STRNCPY_UTF8(rd->engine, type->idname);
    DEG_id_tag_update(ptr->owner_id, ID_RECALC_SYNC_TO_EVAL);
  }
}

static const EnumPropertyItem *rna_RenderSettings_engine_itemf(bContext * /*C*/,
                                                               PointerRNA * /*ptr*/,
                                                               PropertyRNA * /*prop*/,
                                                               bool *r_free)
{
  RenderEngineType *type;
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  int a = 0, totitem = 0;

  for (type = static_cast<RenderEngineType *>(R_engines.first); type; type = type->next, a++) {
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

  for (type = static_cast<RenderEngineType *>(R_engines.first); type; type = type->next, a++) {
    if (STREQ(type->idname, rd->engine)) {
      return a;
    }
  }

  return 0;
}

static void rna_RenderSettings_engine_update(Main *bmain, Scene * /*unused*/, PointerRNA * /*ptr*/)
{
  ED_render_engine_changed(bmain, true);
}

static void rna_Scene_update_render_engine(Main *bmain)
{
  ED_render_engine_changed(bmain, true);
}

static bool rna_RenderSettings_multiple_engines_get(PointerRNA * /*ptr*/)
{
  return (BLI_listbase_count(&R_engines) > 1);
}

static bool rna_RenderSettings_use_spherical_stereo_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  return BKE_scene_use_spherical_stereo(scene);
}

void rna_Scene_render_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
}

static void rna_Scene_world_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Scene *screen = (Scene *)ptr->owner_id;

  rna_Scene_render_update(bmain, scene, ptr);
  WM_main_add_notifier(NC_WORLD | ND_WORLD, &screen->id);
  DEG_relations_tag_update(bmain);
}

static void rna_Scene_mesh_quality_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;

  FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
    if (ELEM(ob->type, OB_MESH, OB_CURVES_LEGACY, OB_VOLUME, OB_MBALL)) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }
  FOREACH_SCENE_OBJECT_END;

  rna_Scene_render_update(bmain, scene, ptr);
}

void rna_Scene_freestyle_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
}

void rna_Scene_use_freestyle_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);

  if (scene->compositing_node_group) {
    ntreeCompositUpdateRLayers(scene->compositing_node_group);
  }
}

void rna_Scene_compositor_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;

  if (scene->compositing_node_group) {
    bNodeTree *ntree = reinterpret_cast<bNodeTree *>(scene->compositing_node_group);
    WM_main_add_notifier(NC_NODE | NA_EDITED, &ntree->id);
    WM_main_add_notifier(NC_SCENE | ND_NODES, &ntree->id);
    BKE_main_ensure_invariants(*bmain, ntree->id);
  }
}

void rna_Scene_use_view_map_cache_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA * /*ptr*/)
{
#  ifdef WITH_FREESTYLE
  FRS_free_view_map_cache();
#  endif
}

void rna_ViewLayer_name_set(PointerRNA *ptr, const char *value)
{
  Scene *scene = (Scene *)ptr->owner_id;
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  BLI_assert(BKE_id_is_in_global_main(&scene->id));
  BKE_view_layer_rename(G_MAIN, scene, view_layer, value);
}

static void rna_SceneRenderView_name_set(PointerRNA *ptr, const char *value)
{
  Scene *scene = (Scene *)ptr->owner_id;
  SceneRenderView *rv = (SceneRenderView *)ptr->data;
  STRNCPY_UTF8(rv->name, value);
  BLI_uniquename(&scene->r.views,
                 rv,
                 DATA_("RenderView"),
                 '.',
                 offsetof(SceneRenderView, name),
                 sizeof(rv->name));
}

void rna_ViewLayer_override_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  rna_Scene_render_update(bmain, scene, ptr);
  DEG_relations_tag_update(bmain);
}

void rna_ViewLayer_pass_update(Main *bmain, Scene *activescene, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;

  ViewLayer *view_layer = nullptr;
  if (ptr->type == &RNA_ViewLayer) {
    view_layer = (ViewLayer *)ptr->data;
  }
  else if (ptr->type == &RNA_AOV) {
    ViewLayerAOV *aov = (ViewLayerAOV *)ptr->data;
    view_layer = BKE_view_layer_find_with_aov(scene, aov);
  }
  else if (ptr->type == &RNA_Lightgroup) {
    ViewLayerLightgroup *lightgroup = (ViewLayerLightgroup *)ptr->data;
    view_layer = BKE_view_layer_find_with_lightgroup(scene, lightgroup);
  }

  if (view_layer) {
    RenderEngineType *engine_type = RE_engines_find(scene->r.engine);
    if (engine_type->update_render_passes) {
      RenderEngine *engine = RE_engine_create(engine_type);
      if (engine) {
        BKE_view_layer_verify_aov(engine, scene, view_layer);
      }
      RE_engine_free(engine);
      engine = nullptr;
    }
  }

  if (scene->compositing_node_group) {
    ntreeCompositUpdateRLayers(scene->compositing_node_group);
  }

  rna_Scene_render_update(bmain, activescene, ptr);
}

static std::optional<std::string> rna_ViewLayerEEVEE_path(const PointerRNA *ptr)
{
  const ViewLayerEEVEE *view_layer_eevee = (ViewLayerEEVEE *)ptr->data;
  const ViewLayer *view_layer = (ViewLayer *)((uint8_t *)view_layer_eevee -
                                              offsetof(ViewLayer, eevee));
  char rna_path[sizeof(view_layer->name) * 3];

  const size_t view_layer_path_len = rna_ViewLayer_path_buffer_get(
      view_layer, rna_path, sizeof(rna_path));

  BLI_strncpy(rna_path + view_layer_path_len, ".eevee", sizeof(rna_path) - view_layer_path_len);

  return rna_path;
}

static void rna_SceneEEVEE_gi_cubemap_resolution_update(Main * /*main*/,
                                                        Scene *scene,
                                                        PointerRNA * /*ptr*/)
{
  /* Tag all light probes to recalc transform. This signals EEVEE to update the light probes. */
  FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
    if (ob->type == OB_LIGHTPROBE) {
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
    }
  }
  FOREACH_SCENE_OBJECT_END;
}

static void rna_SceneEEVEE_clamp_surface_indirect_update(Main * /*main*/,
                                                         Scene *scene,
                                                         PointerRNA * /*ptr*/)
{
  /* Tag all light probes to recalc transform. This signals EEVEE to update the light probes. */
  FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
    if (ob->type == OB_LIGHTPROBE) {
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
    }
  }
  FOREACH_SCENE_OBJECT_END;

  /* Also tag the world. */
  DEG_id_tag_update(&scene->world->id, ID_RECALC_SHADING);
}

static void rna_SceneEEVEE_shadow_resolution_update(Main * /*bmain*/,
                                                    Scene *scene,
                                                    PointerRNA * /*ptr*/)
{
  FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
    if (ob->type == OB_LAMP) {
      DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);
    }
  }
  FOREACH_SCENE_OBJECT_END;

  WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
}

static std::optional<std::string> rna_SceneRenderView_path(const PointerRNA *ptr)
{
  const SceneRenderView *srv = (SceneRenderView *)ptr->data;
  char srv_name_esc[sizeof(srv->name) * 2];
  BLI_str_escape(srv_name_esc, srv->name, sizeof(srv_name_esc));
  return fmt::format("render.views[\"{}\"]", srv_name_esc);
}

static void rna_Physics_relations_update(Main *bmain, Scene * /*scene*/, PointerRNA * /*ptr*/)
{
  DEG_relations_tag_update(bmain);
}

static void rna_Physics_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
    BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_DEPSGRAPH);
  }
  FOREACH_SCENE_OBJECT_END;

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
}

static void rna_Scene_editmesh_select_mode_set(PointerRNA *ptr, const bool *value)
{
  ToolSettings *ts = (ToolSettings *)ptr->data;
  const int selectmode = (value[0] ? SCE_SELECT_VERTEX : 0) | (value[1] ? SCE_SELECT_EDGE : 0) |
                         (value[2] ? SCE_SELECT_FACE : 0);

  if (selectmode) {
    ts->selectmode = selectmode;

    /* Update select mode in all the workspaces in mesh edit mode. */
    wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      const Scene *scene = WM_window_get_active_scene(win);
      ViewLayer *view_layer = WM_window_get_active_view_layer(win);
      if (view_layer) {
        BKE_view_layer_synced_ensure(scene, view_layer);
        Object *object = BKE_view_layer_active_object_get(view_layer);
        if (object && object->type == OB_MESH) {
          if (BMEditMesh *em = BKE_editmesh_from_object(object)) {
            if (em->selectmode != selectmode) {
              EDBM_selectmode_set(em, selectmode);
            }
          }
        }
      }
    }
  }
}

static void rna_Scene_editmesh_select_mode_update(bContext *C, PointerRNA * /*ptr*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Mesh *mesh = nullptr;

  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *object = BKE_view_layer_active_object_get(view_layer);
  if (object) {
    mesh = BKE_mesh_from_object(object);
    if (mesh && mesh->runtime->edit_mesh == nullptr) {
      mesh = nullptr;
    }
  }

  if (mesh) {
    DEG_id_tag_update(&mesh->id, ID_RECALC_SELECT);
    WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);
  }
}

static void rna_Scene_uv_select_mode_update(bContext *C, PointerRNA * /*ptr*/)
{
  /* Makes sure that the UV selection states are consistent with the current UV select mode and
   * sticky mode. */
  ED_uvedit_selectmode_clean_multi(C);
}

static void rna_Scene_uv_sticky_select_mode_update(bContext *C, PointerRNA * /*ptr*/)
{
  /* Some changes to sticky select mode require rebuilding. */
  ED_uvedit_sticky_selectmode_update(C);
}

static void object_simplify_update(Scene *scene,
                                   Object *ob,
                                   bool update_normals,
                                   Depsgraph *depsgraph)
{
  ModifierData *md;
  ParticleSystem *psys;

  if ((ob->id.tag & ID_TAG_DOIT) == 0) {
    return;
  }

  ob->id.tag &= ~ID_TAG_DOIT;

  for (md = static_cast<ModifierData *>(ob->modifiers.first); md; md = md->next) {
    if (md->type == eModifierType_Nodes && depsgraph != nullptr) {
      Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
      const blender::bke::GeometrySet *geometry_set = ob_eval->runtime->geometry_set_eval;
      if (geometry_set != nullptr && geometry_set->has_volume()) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
      continue;
    }
    if (ELEM(
            md->type, eModifierType_Subsurf, eModifierType_Multires, eModifierType_ParticleSystem))
    {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }

  for (psys = static_cast<ParticleSystem *>(ob->particlesystem.first); psys; psys = psys->next) {
    psys->recalc |= ID_RECALC_PSYS_CHILD;
  }

  if (ob->instance_collection) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (ob->instance_collection, ob_collection) {
      object_simplify_update(scene, ob_collection, update_normals, depsgraph);
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }

  if (ob->type == OB_VOLUME) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  if (scene->r.mode & R_SIMPLIFY_NORMALS || update_normals) {
    if (OB_TYPE_IS_GEOMETRY(ob->type)) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }

  if (ob->type == OB_LAMP) {
    DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);
  }
}

static void rna_Scene_simplify_update_impl(Main *bmain,
                                           Scene *sce,
                                           bool update_normals,
                                           Depsgraph *depsgraph)
{
  Scene *sce_iter;
  Base *base;

  BKE_main_id_tag_listbase(&bmain->objects, ID_TAG_DOIT, true);
  FOREACH_SCENE_OBJECT_BEGIN (sce, ob) {
    object_simplify_update(sce, ob, update_normals, depsgraph);
  }
  FOREACH_SCENE_OBJECT_END;

  for (SETLOOPER_SET_ONLY(sce, sce_iter, base)) {
    object_simplify_update(sce, base->object, update_normals, depsgraph);
  }

  WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
  DEG_id_tag_update(&sce->id, ID_RECALC_SYNC_TO_EVAL);
}

static void rna_Scene_use_simplify_update(bContext *C, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  rna_Scene_simplify_update_impl(bmain, scene, false, depsgraph);
}

static void rna_Scene_simplify_volume_update(bContext *C, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  if (scene->r.mode & R_SIMPLIFY) {
    rna_Scene_simplify_update_impl(bmain, scene, false, depsgraph);
  }
}

static void rna_Scene_simplify_update(Main *bmain, Scene *scene, PointerRNA * /*ptr*/)
{
  if (scene->r.mode & R_SIMPLIFY) {
    rna_Scene_simplify_update_impl(bmain, scene, false, nullptr);
  }
}

static void rna_Scene_use_simplify_normals_update(Main *bmain, Scene *scene, PointerRNA * /*ptr*/)
{
  /* NOTE: Ideally this would just force recalculation of the draw batch cache normals.
   * That's complicated enough to not be worth it here. */
  if (scene->r.mode & R_SIMPLIFY) {
    rna_Scene_simplify_update_impl(bmain, scene, true, nullptr);
  }
}

static void rna_Scene_use_persistent_data_update(Main * /*bmain*/,
                                                 Scene * /*scene*/,
                                                 PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;

  if (!(scene->r.mode & R_PERSISTENT_DATA)) {
    RE_FreePersistentData(scene);
  }
}

/* Scene.transform_orientation_slots */
static void rna_Scene_transform_orientation_slots_begin(CollectionPropertyIterator *iter,
                                                        PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  TransformOrientationSlot *orient_slot = &scene->orientation_slots[0];
  rna_iterator_array_begin(iter,
                           ptr,
                           orient_slot,
                           sizeof(*orient_slot),
                           ARRAY_SIZE(scene->orientation_slots),
                           0,
                           nullptr);
}

static int rna_Scene_transform_orientation_slots_length(PointerRNA * /*ptr*/)
{
  return ARRAY_SIZE(((Scene *)nullptr)->orientation_slots);
}

static bool rna_Scene_use_audio_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->data;
  return (scene->audio.flag & AUDIO_MUTE) == 0;
}

static void rna_Scene_use_audio_set(PointerRNA *ptr, bool value)
{
  Scene *scene = (Scene *)ptr->data;

  if (!value) {
    scene->audio.flag |= AUDIO_MUTE;
  }
  else {
    scene->audio.flag &= ~AUDIO_MUTE;
  }
}

static void rna_Scene_use_audio_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_AUDIO_MUTE);
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
  View3DCursor *cursor = static_cast<View3DCursor *>(ptr->data);

  /* use API Method for conversions... */
  BKE_rotMode_change_values(cursor->rotation_quaternion,
                            cursor->rotation_euler,
                            cursor->rotation_axis,
                            &cursor->rotation_angle,
                            cursor->rotation_mode,
                            short(value));

  /* finally, set the new rotation type */
  cursor->rotation_mode = value;
}

static void rna_View3DCursor_rotation_axis_angle_get(PointerRNA *ptr, float *value)
{
  View3DCursor *cursor = static_cast<View3DCursor *>(ptr->data);
  value[0] = cursor->rotation_angle;
  copy_v3_v3(&value[1], cursor->rotation_axis);
}

static void rna_View3DCursor_rotation_axis_angle_set(PointerRNA *ptr, const float *value)
{
  View3DCursor *cursor = static_cast<View3DCursor *>(ptr->data);
  cursor->rotation_angle = value[0];
  copy_v3_v3(cursor->rotation_axis, &value[1]);
}

static void rna_View3DCursor_matrix_get(PointerRNA *ptr, float *values)
{
  const View3DCursor *cursor = static_cast<const View3DCursor *>(ptr->data);
  copy_m4_m4((float(*)[4])values, cursor->matrix<blender::float4x4>().ptr());
}

static void rna_View3DCursor_matrix_set(PointerRNA *ptr, const float *values)
{
  View3DCursor *cursor = static_cast<View3DCursor *>(ptr->data);
  float unit_mat[4][4];
  normalize_m4_m4(unit_mat, (const float(*)[4])values);
  cursor->set_matrix(blender::float4x4(unit_mat), false);
}

static std::optional<std::string> rna_TransformOrientationSlot_path(const PointerRNA *ptr)
{
  const Scene *scene = (Scene *)ptr->owner_id;
  const TransformOrientationSlot *orientation_slot = static_cast<const TransformOrientationSlot *>(
      ptr->data);

  if (!ELEM(nullptr, scene, orientation_slot)) {
    for (int i = 0; i < ARRAY_SIZE(scene->orientation_slots); i++) {
      if (&scene->orientation_slots[i] == orientation_slot) {
        return fmt::format("transform_orientation_slots[{}]", i);
      }
    }
  }

  /* Should not happen, but in case, just return default path. */
  BLI_assert_unreachable();
  return "transform_orientation_slots[0]";
}

static std::optional<std::string> rna_View3DCursor_path(const PointerRNA * /*ptr*/)
{
  return "cursor";
}

static TimeMarker *rna_TimeLine_add(Scene *scene, const char name[], int frame)
{
  TimeMarker *marker = MEM_callocN<TimeMarker>("TimeMarker");
  marker->flag = SELECT;
  marker->frame = frame;
  STRNCPY_UTF8(marker->name, name);
  BLI_addtail(&scene->markers, marker);

  WM_main_add_notifier(NC_SCENE | ND_MARKERS, nullptr);
  WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, nullptr);

  return marker;
}

static void rna_TimeLine_remove(Scene *scene, ReportList *reports, PointerRNA *marker_ptr)
{
  TimeMarker *marker = static_cast<TimeMarker *>(marker_ptr->data);
  if (BLI_remlink_safe(&scene->markers, marker) == false) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Timeline marker '%s' not found in scene '%s'",
                marker->name,
                scene->id.name + 2);
    return;
  }

  MEM_freeN(marker);
  marker_ptr->invalidate();

  WM_main_add_notifier(NC_SCENE | ND_MARKERS, nullptr);
  WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, nullptr);
}

static void rna_TimeLine_clear(Scene *scene)
{
  BLI_freelistN(&scene->markers);

  WM_main_add_notifier(NC_SCENE | ND_MARKERS, nullptr);
  WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, nullptr);
}

static std::optional<std::string> rna_Scene_KeyingsSetsAll_path(const PointerRNA * /*ptr*/)
{
  return "keying_sets_all";
}

static KeyingSet *rna_Scene_keying_set_new(Scene *sce,
                                           ReportList *reports,
                                           const char idname[],
                                           const char name[])
{
  KeyingSet *ks = nullptr;

  /* call the API func, and set the active keyingset index */
  ks = BKE_keyingset_add(&sce->keyingsets, idname, name, KEYINGSET_ABSOLUTE, 0);

  if (ks) {
    sce->active_keyingset = BLI_listbase_count(&sce->keyingsets);
    return ks;
  }
  else {
    BKE_report(reports, RPT_ERROR, "Keying set could not be added");
    return nullptr;
  }
}

static std::optional<std::string> rna_CurvePaintSettings_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.curve_paint_settings";
}

static std::optional<std::string> rna_SequencerToolSettings_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.sequencer_tool_settings";
}

/* generic function to recalc geometry */
static void rna_EditMesh_update(bContext *C, PointerRNA * /*ptr*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Mesh *mesh = BKE_mesh_from_object(obedit);

    DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, mesh);
  }
}

static std::optional<std::string> rna_MeshStatVis_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.statvis";
}

/* NOTE: without this, when Multi-Paint is activated/deactivated, the colors
 * will not change right away when multiple bones are selected, this function
 * is not for general use and only for the few cases where changing scene
 * settings and NOT for general purpose updates, possibly this should be
 * given its own notifier. */
static void rna_Scene_update_active_object_data(bContext *C, PointerRNA * /*ptr*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  if (ob) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_OBJECT | ND_DRAW, &ob->id);
  }
}

static void rna_SceneCamera_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Object *camera = scene->camera;

  blender::seq::cache_cleanup(scene);

  if (camera && (camera->type == OB_CAMERA)) {
    DEG_id_tag_update(&camera->id, ID_RECALC_GEOMETRY);
  }
}

static void rna_SceneSequencer_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  blender::seq::cache_cleanup((Scene *)ptr->owner_id);
}

static std::optional<std::string> rna_ToolSettings_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings";
}

PointerRNA rna_FreestyleLineSet_linestyle_get(PointerRNA *ptr)
{
  FreestyleLineSet *lineset = (FreestyleLineSet *)ptr->data;

  return RNA_id_pointer_create(reinterpret_cast<ID *>(lineset->linestyle));
}

void rna_FreestyleLineSet_linestyle_set(PointerRNA *ptr,
                                        PointerRNA value,
                                        ReportList * /*reports*/)
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

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  return lineset;
}

void rna_FreestyleSettings_lineset_remove(ID *id,
                                          FreestyleSettings *config,
                                          ReportList *reports,
                                          PointerRNA *lineset_ptr)
{
  FreestyleLineSet *lineset = static_cast<FreestyleLineSet *>(lineset_ptr->data);
  Scene *scene = (Scene *)id;

  if (!BKE_freestyle_lineset_delete((FreestyleConfig *)config, lineset)) {
    BKE_reportf(reports, RPT_ERROR, "Line set '%s' could not be removed", lineset->name);
    return;
  }

  lineset_ptr->invalidate();

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, nullptr);
}

PointerRNA rna_FreestyleSettings_active_lineset_get(PointerRNA *ptr)
{
  FreestyleConfig *config = (FreestyleConfig *)ptr->data;
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(config);
  return RNA_pointer_create_with_parent(*ptr, &RNA_FreestyleLineSet, lineset);
}

void rna_FreestyleSettings_active_lineset_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
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

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  return module;
}

void rna_FreestyleSettings_module_remove(ID *id,
                                         FreestyleSettings *config,
                                         ReportList *reports,
                                         PointerRNA *module_ptr)
{
  Scene *scene = (Scene *)id;
  FreestyleModuleConfig *module = static_cast<FreestyleModuleConfig *>(module_ptr->data);

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

  module_ptr->invalidate();

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, nullptr);
}

static void rna_Stereo3dFormat_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  if (id && GS(id->name) == ID_IM) {
    Image *ima = (Image *)id;
    ImBuf *ibuf;
    void *lock;

    if (!BKE_image_is_stereo(ima)) {
      return;
    }

    ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

    if (ibuf) {
      BKE_image_signal(bmain, ima, nullptr, IMA_SIGNAL_FREE);
    }
    BKE_image_release_ibuf(ima, ibuf, lock);
  }
}

static ViewLayer *rna_ViewLayer_new(ID *id, Scene * /*sce*/, Main *bmain, const char *name)
{
  Scene *scene = (Scene *)id;
  ViewLayer *view_layer = BKE_view_layer_add(scene, name, nullptr, VIEWLAYER_ADD_NEW);

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_LAYER, nullptr);

  return view_layer;
}

static void rna_ViewLayer_remove(
    ID *id, Scene * /*sce*/, Main *bmain, ReportList *reports, PointerRNA *sl_ptr)
{
  Scene *scene = (Scene *)id;
  ViewLayer *view_layer = static_cast<ViewLayer *>(sl_ptr->data);

  if (ED_scene_view_layer_delete(bmain, scene, view_layer, reports)) {
    sl_ptr->invalidate();
  }
}

static void rna_ViewLayer_move(
    ID *id, Scene * /*sce*/, Main *bmain, ReportList *reports, int from, int to)
{
  if (from == to) {
    return;
  }

  Scene *scene = (Scene *)id;

  if (!BLI_listbase_move_index(&scene->view_layers, from, to)) {
    BKE_reportf(reports, RPT_ERROR, "Could not move layer from index '%d' to '%d'", from, to);
    return;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_LAYER, nullptr);
}

void rna_ViewLayer_active_aov_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&view_layer->aovs) - 1);
}

int rna_ViewLayer_active_aov_index_get(PointerRNA *ptr)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  return BLI_findindex(&view_layer->aovs, view_layer->active_aov);
}

void rna_ViewLayer_active_aov_index_set(PointerRNA *ptr, int value)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  ViewLayerAOV *aov = static_cast<ViewLayerAOV *>(BLI_findlink(&view_layer->aovs, value));
  view_layer->active_aov = aov;
}

void rna_ViewLayer_active_lightgroup_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&view_layer->lightgroups) - 1);
}

int rna_ViewLayer_active_lightgroup_index_get(PointerRNA *ptr)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  return BLI_findindex(&view_layer->lightgroups, view_layer->active_lightgroup);
}

void rna_ViewLayer_active_lightgroup_index_set(PointerRNA *ptr, int value)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  ViewLayerLightgroup *lightgroup = static_cast<ViewLayerLightgroup *>(
      BLI_findlink(&view_layer->lightgroups, value));
  view_layer->active_lightgroup = lightgroup;
}

static void rna_ViewLayerLightgroup_name_get(PointerRNA *ptr, char *value)
{
  ViewLayerLightgroup *lightgroup = (ViewLayerLightgroup *)ptr->data;
  strcpy(value, lightgroup->name);
}

static int rna_ViewLayerLightgroup_name_length(PointerRNA *ptr)
{
  ViewLayerLightgroup *lightgroup = (ViewLayerLightgroup *)ptr->data;
  return strlen(lightgroup->name);
}

static void rna_ViewLayerLightgroup_name_set(PointerRNA *ptr, const char *value)
{
  ViewLayerLightgroup *lightgroup = (ViewLayerLightgroup *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;
  ViewLayer *view_layer = BKE_view_layer_find_with_lightgroup(scene, lightgroup);

  BKE_view_layer_rename_lightgroup(scene, view_layer, lightgroup, value);
}

/* Fake value, used internally (not saved to DNA). */
#  define V3D_ORIENT_DEFAULT -1

static int rna_TransformOrientationSlot_type_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  TransformOrientationSlot *orient_slot = static_cast<TransformOrientationSlot *>(ptr->data);
  if (orient_slot != &scene->orientation_slots[SCE_ORIENT_DEFAULT]) {
    if ((orient_slot->flag & SELECT) == 0) {
      return V3D_ORIENT_DEFAULT;
    }
  }
  return BKE_scene_orientation_slot_get_index(orient_slot);
}

void rna_TransformOrientationSlot_type_set(PointerRNA *ptr, int value)
{
  Scene *scene = (Scene *)ptr->owner_id;
  TransformOrientationSlot *orient_slot = static_cast<TransformOrientationSlot *>(ptr->data);

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
  Scene *scene = (Scene *)ptr->owner_id;
  TransformOrientationSlot *orient_slot = static_cast<TransformOrientationSlot *>(ptr->data);
  TransformOrientation *orientation;
  if (orient_slot->type < V3D_ORIENT_CUSTOM) {
    orientation = nullptr;
  }
  else {
    orientation = BKE_scene_transform_orientation_find(scene, orient_slot->index_custom);
  }
  return RNA_pointer_create_with_parent(*ptr, &RNA_TransformOrientation, orientation);
}

static const EnumPropertyItem *rna_TransformOrientation_impl_itemf(Scene *scene,
                                                                   const bool include_default,
                                                                   bool *r_free)
{
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  EnumPropertyItem *item = nullptr;
  int i = V3D_ORIENT_CUSTOM, totitem = 0;

  if (include_default) {
    tmp.identifier = "DEFAULT";
    tmp.name = N_("Default");
    tmp.description = N_("Use the scene orientation");
    tmp.value = V3D_ORIENT_DEFAULT;
    tmp.icon = ICON_OBJECT_ORIGIN;
    RNA_enum_item_add(&item, &totitem, &tmp);
    tmp.icon = 0;

    RNA_enum_item_add_separator(&item, &totitem);
  }

  RNA_enum_items_add(&item, &totitem, rna_enum_transform_orientation_items);

  const ListBase *transform_orientations = scene ? &scene->transform_spaces : nullptr;

  if (transform_orientations && (BLI_listbase_is_empty(transform_orientations) == false)) {
    RNA_enum_item_add_separator(&item, &totitem);

    LISTBASE_FOREACH (TransformOrientation *, ts, transform_orientations) {
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
                                                       PropertyRNA * /*prop*/,
                                                       bool *r_free)
{
  if (C == nullptr) {
    return rna_enum_transform_orientation_items;
  }

  Scene *scene;
  if (ptr->owner_id && (GS(ptr->owner_id->name) == ID_SCE)) {
    scene = (Scene *)ptr->owner_id;
  }
  else {
    scene = CTX_data_scene(C);
  }
  return rna_TransformOrientation_impl_itemf(scene, false, r_free);
}

const EnumPropertyItem *rna_TransformOrientation_with_scene_itemf(bContext *C,
                                                                  PointerRNA *ptr,
                                                                  PropertyRNA * /*prop*/,
                                                                  bool *r_free)
{
  if (C == nullptr) {
    return rna_enum_transform_orientation_items;
  }

  Scene *scene = (Scene *)ptr->owner_id;
  TransformOrientationSlot *orient_slot = static_cast<TransformOrientationSlot *>(ptr->data);
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
  BKE_unit_system_get(system, type, &usys, &len);

  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  EnumPropertyItem adaptive = {0};
  adaptive.identifier = "ADAPTIVE";
  adaptive.name = N_("Adaptive");
  adaptive.value = USER_UNIT_ADAPTIVE;
  RNA_enum_item_add(&items, &totitem, &adaptive);

  for (int i = 0; i < len; i++) {
    if (!BKE_unit_is_suppressed(usys, i)) {
      EnumPropertyItem tmp = {0};
      tmp.identifier = BKE_unit_identifier_get(usys, i);
      tmp.name = BKE_unit_display_name_get(usys, i);
      tmp.value = i;
      RNA_enum_item_add(&items, &totitem, &tmp);
    }
  }

  RNA_enum_item_end(&items, &totitem);
  *r_free = true;

  return items;
}

const EnumPropertyItem *rna_UnitSettings_length_unit_itemf(bContext * /*C*/,
                                                           PointerRNA *ptr,
                                                           PropertyRNA * /*prop*/,
                                                           bool *r_free)
{
  const UnitSettings *units = static_cast<const UnitSettings *>(ptr->data);
  return rna_UnitSettings_itemf_wrapper(units->system, B_UNIT_LENGTH, r_free);
}

const EnumPropertyItem *rna_UnitSettings_mass_unit_itemf(bContext * /*C*/,
                                                         PointerRNA *ptr,
                                                         PropertyRNA * /*prop*/,
                                                         bool *r_free)
{
  const UnitSettings *units = static_cast<const UnitSettings *>(ptr->data);
  return rna_UnitSettings_itemf_wrapper(units->system, B_UNIT_MASS, r_free);
}

const EnumPropertyItem *rna_UnitSettings_time_unit_itemf(bContext * /*C*/,
                                                         PointerRNA *ptr,
                                                         PropertyRNA * /*prop*/,
                                                         bool *r_free)
{
  const UnitSettings *units = static_cast<const UnitSettings *>(ptr->data);
  return rna_UnitSettings_itemf_wrapper(units->system, B_UNIT_TIME, r_free);
}

const EnumPropertyItem *rna_UnitSettings_temperature_unit_itemf(bContext * /*C*/,
                                                                PointerRNA *ptr,
                                                                PropertyRNA * /*prop*/,
                                                                bool *r_free)
{
  const UnitSettings *units = static_cast<const UnitSettings *>(ptr->data);
  return rna_UnitSettings_itemf_wrapper(units->system, B_UNIT_TEMPERATURE, r_free);
}

static void rna_UnitSettings_system_update(Main * /*bmain*/, Scene *scene, PointerRNA * /*ptr*/)
{
  UnitSettings *unit = &scene->unit;
  if (unit->system == USER_UNIT_NONE) {
    unit->length_unit = USER_UNIT_ADAPTIVE;
    unit->mass_unit = USER_UNIT_ADAPTIVE;
  }
  else {
    unit->length_unit = BKE_unit_base_of_type_get(unit->system, B_UNIT_LENGTH);
    unit->mass_unit = BKE_unit_base_of_type_get(unit->system, B_UNIT_MASS);
  }
}

static std::optional<std::string> rna_UnitSettings_path(const PointerRNA * /*ptr*/)
{
  return "unit_settings";
}

static std::optional<std::string> rna_FFmpegSettings_path(const PointerRNA * /*ptr*/)
{
  return "render.ffmpeg";
}

#  ifdef WITH_FFMPEG
/* FFMpeg Codec setting update hook. */
static void rna_FFmpegSettings_codec_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  FFMpegCodecData *codec_data = (FFMpegCodecData *)ptr->data;
  if (!MOV_codec_supports_crf(codec_data->codec_id_get())) {
    /* Constant Rate Factor (CRF) setting is only available for some codecs. Change encoder quality
     * mode to CBR for others. */
    codec_data->constant_rate_factor = FFM_CRF_NONE;
  }

  /* Ensure valid color depth when changing the codec. */
  const ID *id = ptr->owner_id;
  const bool is_render = (id && GS(id->name) == ID_SCE);
  if (is_render) {
    Scene *scene = (Scene *)ptr->owner_id;
    const int valid_depths = BKE_imtype_valid_depths_with_video(scene->r.im_format.imtype, id);
    if ((scene->r.im_format.depth & valid_depths) == 0) {
      scene->r.im_format.depth = BKE_imtype_first_valid_depth(valid_depths);
    }
  }
}
#  endif

#else

/* Grease Pencil Interpolation tool settings */
static void rna_def_gpencil_interpolate(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilInterpolateSettings", nullptr);
  RNA_def_struct_sdna(srna, "GP_Interpolate_Settings");
  RNA_def_struct_ui_text(srna,
                         "Grease Pencil Interpolate Settings",
                         "Settings for Grease Pencil interpolation tools");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  /* Custom curve-map. */
  prop = RNA_def_property(srna, "interpolation_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "custom_ipo");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(
      prop,
      "Interpolation Curve",
      "Custom curve to control 'sequence' interpolation between Grease Pencil frames");
}

static void rna_def_transform_orientation(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "TransformOrientation", nullptr);

  prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "mat");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_3x3);
  RNA_def_property_update(prop, NC_SCENE | ND_TRANSFORM, nullptr);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_ui_text(prop, "Name", "Name of the custom transform orientation");
  RNA_def_property_update(prop, NC_SCENE | ND_TRANSFORM, nullptr);
}

static void rna_def_transform_orientation_slot(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "TransformOrientationSlot", nullptr);
  RNA_def_struct_sdna(srna, "TransformOrientationSlot");
  RNA_def_struct_path_func(srna, "rna_TransformOrientationSlot_path");
  RNA_def_struct_ui_text(srna, "Orientation Slot", "");

  /* Orientations */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_transform_orientation_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_TransformOrientationSlot_type_get",
                              "rna_TransformOrientationSlot_type_set",
                              "rna_TransformOrientation_with_scene_itemf");
  RNA_def_property_ui_text(prop, "Orientation", "Transformation orientation");
  RNA_def_property_update(prop, NC_SCENE | ND_TRANSFORM, nullptr);

  prop = RNA_def_property(srna, "custom_orientation", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "TransformOrientation");
  RNA_def_property_pointer_funcs(
      prop, "rna_TransformOrientationSlot_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Current Transform Orientation", "");

  /* flag */
  prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SELECT);
  RNA_def_property_ui_text(prop, "Use", "Use scene orientation instead of a custom setting");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);
}

static void rna_def_view3d_cursor(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "View3DCursor", nullptr);
  RNA_def_struct_sdna(srna, "View3DCursor");
  RNA_def_struct_path_func(srna, "rna_View3DCursor_path");
  RNA_def_struct_ui_text(srna, "3D Cursor", "");
  RNA_def_struct_ui_icon(srna, ICON_CURSOR);
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ_LENGTH);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "location");
  RNA_def_property_ui_text(prop, "Location", "");
  RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 4);
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "rotation_quaternion", PROP_FLOAT, PROP_QUATERNION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "rotation_quaternion");
  RNA_def_property_ui_text(
      prop, "Quaternion Rotation", "Rotation in quaternions (keep normalized)");
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "rotation_axis_angle", PROP_FLOAT, PROP_AXISANGLE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_funcs(prop,
                               "rna_View3DCursor_rotation_axis_angle_get",
                               "rna_View3DCursor_rotation_axis_angle_set",
                               nullptr);
  RNA_def_property_float_array_default(prop, rna_default_axis_angle);
  RNA_def_property_ui_text(
      prop, "Axis-Angle Rotation", "Angle of Rotation for Axis-Angle rotation representation");
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "rotation_euler", PROP_FLOAT, PROP_EULER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "rotation_euler");
  RNA_def_property_ui_text(prop, "Euler Rotation", "3D rotation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "rotation_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_sdna(prop, nullptr, "rotation_mode");
  RNA_def_property_enum_items(prop, rna_enum_object_rotation_mode_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_View3DCursor_rotation_mode_set", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Rotation Mode",
      /* This description is shared by other "rotation_mode" properties. */
      "The kind of rotation to apply, values from other rotation modes aren't used");
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  /* Matrix access to avoid having to check current rotation mode. */
  prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_flag(prop, PROP_THICK_WRAP); /* no reference to original data */
  RNA_def_property_ui_text(
      prop, "Transform Matrix", "Matrix combining location and rotation of the cursor");
  RNA_def_property_float_funcs(
      prop, "rna_View3DCursor_matrix_get", "rna_View3DCursor_matrix_set", nullptr);
  RNA_def_property_update(prop, NC_WINDOW, nullptr);
}

static void rna_def_tool_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* the construction of this enum is quite special - everything is stored as bitflags,
   * with 1st position only for on/off (and exposed as boolean), while others are mutually
   * exclusive options but which will only have any effect when autokey is enabled
   */
  static const EnumPropertyItem auto_key_items[] = {
      {AUTOKEY_MODE_NORMAL & ~AUTOKEY_ON, "ADD_REPLACE_KEYS", 0, "Add & Replace", ""},
      {AUTOKEY_MODE_EDITKEYS & ~AUTOKEY_ON, "REPLACE_KEYS", 0, "Replace", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem draw_groupuser_items[] = {
      {OB_DRAW_GROUPUSER_NONE, "NONE", 0, "None", ""},
      {OB_DRAW_GROUPUSER_ACTIVE,
       "ACTIVE",
       0,
       "Active",
       "Show vertices with no weights in the active group"},
      {OB_DRAW_GROUPUSER_ALL, "ALL", 0, "All", "Show vertices with no weights in any group"},
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem gpencil_stroke_snap_items[] = {
      {0, "NONE", 0, "All Points", "Snap to all points"},
      {GP_PROJECT_DEPTH_STROKE_ENDPOINTS,
       "ENDS",
       0,
       "End Points",
       "Snap to first and last points and interpolate"},
      {GP_PROJECT_DEPTH_STROKE_FIRST, "FIRST", 0, "First Point", "Snap to first point"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem annotation_stroke_placement_view2d_items[] = {
      {GP_PROJECT_VIEWSPACE | GP_PROJECT_CURSOR,
       "IMAGE",
       ICON_IMAGE_DATA,
       "Image",
       "Stick stroke to the image"},
      /* Weird, GP_PROJECT_VIEWALIGN is inverted. */
      {0, "VIEW", ICON_RESTRICT_VIEW_ON, "View", "Stick stroke to the view"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem annotation_stroke_placement_view3d_items[] = {
      {GP_PROJECT_VIEWSPACE | GP_PROJECT_CURSOR,
       "CURSOR",
       ICON_PIVOT_CURSOR,
       "3D Cursor",
       "Draw stroke at 3D cursor location"},
      /* Weird, GP_PROJECT_VIEWALIGN is inverted. */
      {0, "VIEW", ICON_RESTRICT_VIEW_ON, "View", "Stick stroke to the view"},
      {GP_PROJECT_VIEWSPACE | GP_PROJECT_DEPTH_VIEW,
       "SURFACE",
       ICON_FACESEL,
       "Surface",
       "Stick stroke to surfaces"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem uv_sticky_mode_items[] = {
      {SI_STICKY_DISABLE,
       "DISABLED",
       ICON_STICKY_UVS_DISABLE,
       "Disabled",
       "Sticky vertex selection disabled"},
      {SI_STICKY_LOC,
       "SHARED_LOCATION",
       ICON_STICKY_UVS_LOC,
       "Shared Location",
       "Select UVs that are at the same location and share a mesh vertex"},
      {SI_STICKY_VERTEX,
       "SHARED_VERTEX",
       ICON_STICKY_UVS_VERT,
       "Shared Vertex",
       "Select UVs that share a mesh vertex, whether or not they are at the same location"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ToolSettings", nullptr);
  RNA_def_struct_path_func(srna, "rna_ToolSettings_path");
  RNA_def_struct_ui_text(srna, "Tool Settings", "");
  /*
   * `STRUCT_UNDO` only applies to the top level attributes and not nested structs, any struct
   * contained within the `ToolSettings` struct should also clear this flag to avoid pushing empty
   * undo steps.
   */
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  prop = RNA_def_property(srna, "sculpt", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Sculpt");
  RNA_def_property_ui_text(prop, "Sculpt", "");

  prop = RNA_def_property(srna, "curves_sculpt", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "CurvesSculpt");
  RNA_def_property_ui_text(prop, "Curves Sculpt", "");

  prop = RNA_def_property(srna, "use_auto_normalize", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE | PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_sdna(prop, nullptr, "auto_normalize", 1);
  RNA_def_property_ui_text(prop,
                           "Weight Paint Auto-Normalize",
                           "Ensure all bone-deforming vertex groups add up "
                           "to 1.0 while weight painting or assigning to vertices");
  RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

  prop = RNA_def_property(srna, "use_lock_relative", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE | PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_sdna(prop, nullptr, "wpaint_lock_relative", 1);
  RNA_def_property_ui_text(prop,
                           "Weight Paint Lock-Relative",
                           "Display bone-deforming groups as if all locked deform groups "
                           "were deleted, and the remaining ones were re-normalized");
  RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

  prop = RNA_def_property(srna, "use_multipaint", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE | PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_sdna(prop, nullptr, "multipaint", 1);
  RNA_def_property_ui_text(prop,
                           "Weight Paint Multi-Paint",
                           "Paint across the weights of all selected bones, "
                           "maintaining their relative influence");
  RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

  prop = RNA_def_property(srna, "vertex_group_user", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE | PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_sdna(prop, nullptr, "weightuser");
  RNA_def_property_enum_items(prop, draw_groupuser_items);
  RNA_def_property_ui_text(prop, "Mask Non-Group Vertices", "Display unweighted vertices");
  RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

  prop = RNA_def_property(srna, "vertex_group_subset", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE | PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_sdna(prop, nullptr, "vgroupsubset");
  RNA_def_property_enum_items(prop, vertex_group_select_items);
  RNA_def_property_ui_text(prop, "Subset", "Filter Vertex groups for Display");
  RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

  prop = RNA_def_property(srna, "vertex_paint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "vpaint");
  RNA_def_property_ui_text(prop, "Vertex Paint", "");

  prop = RNA_def_property(srna, "weight_paint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "wpaint");
  RNA_def_property_ui_text(prop, "Weight Paint", "");

  prop = RNA_def_property(srna, "image_paint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "imapaint");
  RNA_def_property_ui_text(prop, "Image Paint", "");

  prop = RNA_def_property(srna, "paint_mode", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "paint_mode");
  RNA_def_property_ui_text(prop, "Paint Mode", "");

  prop = RNA_def_property(srna, "uv_sculpt", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "uvsculpt");
  RNA_def_property_ui_text(prop, "UV Sculpt", "");

  prop = RNA_def_property(srna, "gpencil_paint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gp_paint");
  RNA_def_property_ui_text(prop, "Grease Pencil Paint", "");

  prop = RNA_def_property(srna, "gpencil_vertex_paint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gp_vertexpaint");
  RNA_def_property_ui_text(prop, "Grease Pencil Vertex Paint", "");

  prop = RNA_def_property(srna, "gpencil_sculpt_paint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gp_sculptpaint");
  RNA_def_property_ui_text(prop, "Grease Pencil Sculpt Paint", "");

  prop = RNA_def_property(srna, "gpencil_weight_paint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gp_weightpaint");
  RNA_def_property_ui_text(prop, "Grease Pencil Weight Paint", "");

  prop = RNA_def_property(srna, "particle_edit", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "particle");
  RNA_def_property_ui_text(prop, "Particle Edit", "");

  prop = RNA_def_property(srna, "uv_sculpt_lock_borders", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "uv_sculpt_settings", UV_SCULPT_LOCK_BORDERS);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Lock Borders", "Disable editing of boundary edges");

  prop = RNA_def_property(srna, "uv_sculpt_all_islands", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "uv_sculpt_settings", UV_SCULPT_ALL_ISLANDS);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Sculpt All Islands", "Brush operates on all islands");

  prop = RNA_def_property(srna, "lock_object_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "object_flag", SCE_OBJECT_MODE_LOCK);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop,
                           "Lock Object Modes",
                           "Restrict selection to objects using the same mode as the active "
                           "object, to prevent accidental mode switch when selecting");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  static const EnumPropertyItem workspace_tool_items[] = {
      {SCE_WORKSPACE_TOOL_DEFAULT, "DEFAULT", 0, "Active Tool", ""},
      {SCE_WORKSPACE_TOOL_FALLBACK, "FALLBACK", 0, "Select", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "workspace_tool_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "workspace_tool_type");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, workspace_tool_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_VIEW3D);
  RNA_def_property_ui_text(prop, "Drag", "Action when dragging in the viewport");

  /* Transform */
  prop = RNA_def_property(srna, "use_proportional_edit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "proportional_edit", PROP_EDIT_USE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Proportional Editing", "Proportional edit mode");
  RNA_def_property_ui_icon(prop, ICON_PROP_ON, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_edit_objects", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "proportional_objects", 0);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Proportional Editing Objects", "Proportional editing object mode");
  RNA_def_property_ui_icon(prop, ICON_PROP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_projected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "proportional_edit", PROP_EDIT_PROJECTED);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Projected from View", "Proportional Editing using screen space locations");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_connected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "proportional_edit", PROP_EDIT_CONNECTED);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Connected Only", "Proportional Editing using connected geometry only");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_edit_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "proportional_mask", 0);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Proportional Editing Objects", "Proportional editing mask mode");
  RNA_def_property_ui_icon(prop, ICON_PROP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_action", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "proportional_action", 0);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Proportional Editing Actions", "Proportional editing in action editor");
  RNA_def_property_ui_icon(prop, ICON_PROP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_proportional_fcurve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "proportional_fcurve", 0);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Proportional Editing F-Curves", "Proportional editing in F-Curve editor");
  RNA_def_property_ui_icon(prop, ICON_PROP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "lock_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "lock_markers", 0);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Lock Markers", "Prevent marker editing");

  prop = RNA_def_property(srna, "proportional_edit_falloff", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "prop_mode");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, rna_enum_proportional_falloff_items);
  RNA_def_property_ui_text(
      prop, "Proportional Editing Falloff", "Falloff type for proportional editing mode");
  /* Abusing id_curve :/ */
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE_LEGACY);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "proportional_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "proportional_size");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Proportional Size", "Display size for proportional editing circle");
  RNA_def_property_range(prop, 0.00001, 5000.0);

  prop = RNA_def_property(srna, "proportional_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "proportional_size");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Proportional Size", "Display size for proportional editing circle");
  RNA_def_property_range(prop, 0.00001, 5000.0);

  prop = RNA_def_property(srna, "double_threshold", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "doublimit");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Merge Threshold", "Threshold distance for Auto Merge");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 0.1, 0.01, 6);

  /* Pivot Point */
  prop = RNA_def_property(srna, "transform_pivot_point", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "transform_pivot_point");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, rna_enum_transform_pivot_full_items);
  RNA_def_property_ui_text(prop, "Transform Pivot Point", "Pivot center for rotation/scaling");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_transform_pivot_point_align", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "transform_flag", SCE_XFORM_AXIS_ALIGN);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop,
      "Only Locations",
      "Only transform object locations, without affecting rotation or scaling");
  RNA_def_property_update(prop, NC_SCENE | ND_TRANSFORM, nullptr);

  prop = RNA_def_property(srna, "use_transform_data_origin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "transform_flag", SCE_XFORM_DATA_ORIGIN);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Transform Origins", "Transform object origins, while leaving the shape in place");
  RNA_def_property_update(prop, NC_SCENE | ND_TRANSFORM, nullptr);

  prop = RNA_def_property(srna, "use_transform_skip_children", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "transform_flag", SCE_XFORM_SKIP_CHILDREN);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Transform Parents", "Transform the parents, leaving the children in place");
  RNA_def_property_update(prop, NC_SCENE | ND_TRANSFORM, nullptr);

  prop = RNA_def_property(srna, "use_transform_correct_face_attributes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "uvcalc_flag", UVCALC_TRANSFORM_CORRECT);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop,
                           "Correct Face Attributes",
                           "Correct data such as UVs and color attributes when transforming");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_transform_correct_keep_connected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "uvcalc_flag", UVCALC_TRANSFORM_CORRECT_KEEP_CONNECTED);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop,
      "Keep Connected",
      "During the Face Attributes correction, merge attributes connected to the same vertex");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_mesh_automerge", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "automerge", AUTO_MERGE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Auto Merge Vertices", "Automatically merge vertices moved to the same location");
  RNA_def_property_ui_icon(prop, ICON_AUTOMERGE_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_mesh_automerge_and_split", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "automerge", AUTO_MERGE_AND_SPLIT);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Split Edges & Faces", "Automatically split edges and faces");
  RNA_def_property_ui_icon(prop, ICON_AUTOMERGE_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SCE_SNAP);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Snap", "Snap during transform");
  RNA_def_property_ui_icon(prop, ICON_SNAP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_node", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag_node", SCE_SNAP);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Snap", "Snap Node during transform");
  RNA_def_property_ui_icon(prop, ICON_SNAP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_sequencer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag_seq", SCE_SNAP);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Use Snapping", "Snap strips during transform");
  RNA_def_property_ui_icon(prop, ICON_SNAP_OFF, 1);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* Publish message-bus. */

  prop = RNA_def_property(srna, "use_snap_uv", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_uv_flag", SCE_SNAP);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Snap", "Snap UV during transform");
  RNA_def_property_ui_icon(prop, ICON_SNAP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_align_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SCE_SNAP_ROTATE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Align Rotation to Target", "Align rotation with the snapping target");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_grid_absolute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SCE_SNAP_ABS_GRID);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop,
      "Absolute Increment Snap",
      "Absolute grid alignment while translating (based on the pivot center)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_angle_increment_2d", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "snap_angle_increment_2d");
  RNA_def_property_ui_text(
      prop, "Rotation Increment", "Angle used for rotation increments in 2D editors");
  RNA_def_property_range(prop, 0, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, DEG2RADF(1.0f), DEG2RADF(180.0f), 100.0f, 2);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_angle_increment_2d_precision", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "snap_angle_increment_2d_precision");
  RNA_def_property_ui_text(prop,
                           "Rotation Precision Increment",
                           "Precision angle used for rotation increments in 2D editors");
  RNA_def_property_range(prop, 0, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, DEG2RADF(0.1f), DEG2RADF(180.0f), 10.0f, 3);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_angle_increment_3d", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "snap_angle_increment_3d");
  RNA_def_property_ui_text(
      prop, "Rotation Increment", "Angle used for rotation increments in 3D editors");
  RNA_def_property_range(prop, 0, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, DEG2RADF(1.0f), DEG2RADF(180.0f), 100.0f, 2);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_angle_increment_3d_precision", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "snap_angle_increment_3d_precision");
  RNA_def_property_ui_text(prop,
                           "Rotation Precision Increment",
                           "Precision angle used for rotation increments in 3D editors");
  RNA_def_property_range(prop, 0, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, DEG2RADF(0.1f), DEG2RADF(180.0f), 10.0f, 3);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_elements", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "snap_mode");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, rna_enum_snap_element_items);
  RNA_def_property_enum_funcs(
      prop, "rna_ToolSettings_snap_mode_get", "rna_ToolSettings_snap_mode_set", nullptr);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_ui_text(prop, "Snap Element", "Type of element to snap to");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_elements_base", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "snap_mode");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, rna_enum_snap_element_base_items);
  RNA_def_property_enum_funcs(
      prop, "rna_ToolSettings_snap_mode_get", "rna_ToolSettings_snap_mode_set", nullptr);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_ui_text(
      prop, "Snap Element", "Type of element for the \"Snap Base\" to snap to");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_elements_individual", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "snap_mode");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, rna_enum_snap_element_individual_items);
  RNA_def_property_enum_funcs(
      prop, "rna_ToolSettings_snap_mode_get", "rna_ToolSettings_snap_mode_set", nullptr);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_ui_text(
      prop, "Project Mode", "Type of element for individual transformed elements to snap to");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_face_nearest_steps", PROP_INT, PROP_FACTOR);
  RNA_def_property_int_sdna(prop, nullptr, "snap_face_nearest_steps");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_text(
      prop,
      "Face Nearest Steps",
      "Number of steps to break transformation into for face nearest snapping");

  prop = RNA_def_property(srna, "use_snap_to_same_target", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SCE_SNAP_KEEP_ON_SAME_OBJECT);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop,
      "Snap to Same Target",
      "Snap only to target that source was initially near (\"Face Nearest\" only)");

  prop = RNA_def_property(srna, "use_snap_anim", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag_anim", SCE_SNAP);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Snap", "Enable snapping when transforming keyframes");
  RNA_def_property_ui_icon(prop, ICON_SNAP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_driver", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag_driver", SCE_SNAP);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Snap", "Enable snapping when transforming keys in the Driver Editor");
  RNA_def_property_ui_icon(prop, ICON_SNAP_OFF, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_time_absolute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag_anim", SCE_SNAP_ABS_TIME_STEP);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Absolute Time Snap", "Absolute time alignment when transforming keyframes");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_driver_absolute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag_driver", SCE_SNAP_ABS_TIME_STEP);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Absolute Snap", "Snap to full values");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_anim_element", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "snap_anim_mode");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, rna_enum_snap_animation_element_items);
  RNA_def_property_ui_text(prop, "Snap Animation Element", "Type of element to snap to");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_playhead", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag_playhead", SCE_SNAP);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Use Snapping", "Snap playhead when scrubbing");
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "snap_playhead_element", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "snap_playhead_mode");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_enum_items(prop, rna_enum_snap_playhead_element_items);
  RNA_def_property_ui_text(prop, "Snap Playhead Element", "Type of element to snap to");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "snap_playhead_frame_step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "snap_step_frames");
  RNA_def_property_range(prop, 1, 32768);
  RNA_def_property_ui_text(prop, "Frame Step", "At which interval to snap to frames");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "snap_playhead_second_step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "snap_step_seconds");
  RNA_def_property_ui_text(prop, "Second Step", "At which interval to snap to seconds");
  RNA_def_property_range(prop, 1, 32768);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "playhead_snap_distance", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "playhead_snap_distance");
  RNA_def_property_ui_range(prop, 1, 100, 1, 1);
  RNA_def_property_ui_text(prop, "Snap Distance", "Maximum distance for snapping in pixels");

  /* image editor uses its own set of snap modes */
  prop = RNA_def_property(srna, "snap_uv_element", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "snap_uv_mode");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY | PROP_ENUM_FLAG);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_ToolSettings_snap_uv_mode_set", nullptr);
  RNA_def_property_enum_items(prop, snap_uv_element_items);
  RNA_def_property_ui_text(prop, "Snap UV Element", "Type of element to snap to");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  /* TODO(@gfxcoder): Rename `snap_target` to `snap_source` to avoid previous ambiguity of "target"
   * (now, "source" is geometry to be moved and "target" is geometry to which moved geometry is
   * snapped). */
  prop = RNA_def_property(srna, "snap_target", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "snap_target");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, rna_enum_snap_source_items);
  RNA_def_property_ui_text(prop, "Snap Target", "Which part to snap onto the target");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_peel_object", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SCE_SNAP_PEEL_OBJECT);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Snap Peel Object", "Consider objects as whole when finding volume center");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_backface_culling", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SCE_SNAP_BACKFACE_CULLING);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Backface Culling", "Exclude back facing geometry from snapping");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  /* TODO(@gfxcoder): Rename `use_snap_self` to `use_snap_active`, because active is correct but
   * self is not (breaks API). This only makes a difference when more than one mesh is edited. */
  prop = RNA_def_property(srna, "use_snap_self", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "snap_flag", SCE_SNAP_NOT_TO_ACTIVE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Snap onto Active", "Snap onto itself only if enabled (edit mode only)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_edit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SCE_SNAP_TO_INCLUDE_EDITED);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Snap onto Edited", "Snap onto non-active objects in edit mode (edit mode only)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_nonedit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SCE_SNAP_TO_INCLUDE_NONEDITED);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Snap onto Non-edited", "Snap onto objects not in edit mode (edit mode only)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_selectable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SCE_SNAP_TO_ONLY_SELECTABLE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Snap onto Selectable Only", "Snap only onto objects that are selectable");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_translate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "snap_transform_mode_flag", SCE_SNAP_TRANSFORM_MODE_TRANSLATE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Use Snap for Translation", "Move is affected by snapping settings");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_rotate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "snap_transform_mode_flag", SCE_SNAP_TRANSFORM_MODE_ROTATE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(
      prop, "Use Snap for Rotation", "Rotate is affected by the snapping settings");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "use_snap_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "snap_transform_mode_flag", SCE_SNAP_TRANSFORM_MODE_SCALE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(prop, "Use Snap for Scale", "Scale is affected by snapping settings");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "plane_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "plane_axis");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_enum_default(prop, 2);
  RNA_def_property_ui_text(prop, "Plane Axis", "The axis used for placing the base region");

  prop = RNA_def_property(srna, "plane_axis_auto", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_plane_axis_auto", 1);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop,
                           "Auto Axis",
                           "Select the closest axis when placing objects "
                           "(surface overrides)");

  prop = RNA_def_property(srna, "plane_depth", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "plane_depth");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, plane_depth_items);
  RNA_def_property_enum_default(prop, V3D_PLACE_DEPTH_SURFACE);
  RNA_def_property_ui_text(prop, "Position", "The initial depth used when placing the cursor");

  prop = RNA_def_property(srna, "plane_orientation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "plane_orient");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, plane_orientation_items);
  RNA_def_property_enum_default(prop, V3D_PLACE_ORIENT_SURFACE);
  RNA_def_property_ui_text(prop, "Orientation", "The initial depth used when placing the cursor");

  prop = RNA_def_property(srna, "snap_elements_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "snap_mode_tools");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, snap_to_items);
  RNA_def_property_enum_default(prop, SCE_SNAP_TO_GEOM);
  RNA_def_property_ui_text(prop, "Snap to", "The target to use while snapping");

  /* Grease Pencil */
  prop = RNA_def_property(srna, "use_gpencil_draw_additive", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gpencil_flags", GP_TOOL_FLAG_RETAIN_LAST);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop,
                           "Use Additive Drawing",
                           "When creating new frames, the strokes from the previous/active frame "
                           "are included as the basis for the new one");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_draw_onback", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gpencil_flags", GP_TOOL_FLAG_PAINT_ONBACK);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Draw Strokes on Back", "New strokes are drawn below of all strokes in the layer");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_thumbnail_list", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "gpencil_flags", GP_TOOL_FLAG_THUMBNAIL_LIST);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Compact List", "Show compact list of colors instead of thumbnails");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_weight_data_add", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gpencil_flags", GP_TOOL_FLAG_CREATE_WEIGHTS);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop,
                           "Add weight data for new strokes",
                           "Weight data for new strokes is added according to the current vertex "
                           "group and weight. If no vertex group selected, weight is not added.");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_automerge_strokes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gpencil_flags", GP_TOOL_FLAG_AUTOMERGE_STROKE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_icon(prop, ICON_AUTOMERGE_OFF, 1);
  RNA_def_property_ui_text(
      prop,
      "Automerge",
      "Join the last drawn stroke with previous strokes in the active layer by distance");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "gpencil_sculpt", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gp_sculpt");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_struct_type(prop, "GPencilSculptSettings");
  RNA_def_property_ui_text(
      prop, "Grease Pencil Sculpt", "Settings for stroke sculpting tools and brushes");

  prop = RNA_def_property(srna, "gpencil_interpolate", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gp_interpolate");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_struct_type(prop, "GPencilInterpolateSettings");
  RNA_def_property_ui_text(
      prop, "Grease Pencil Interpolate", "Settings for Grease Pencil interpolation tools");

  /* Grease Pencil - 3D View Stroke Placement */
  prop = RNA_def_property(srna, "gpencil_stroke_placement_view3d", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "gpencil_v3d_align");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, gpencil_stroke_placement_items);
  RNA_def_property_ui_text(prop, "Stroke Placement (3D View)", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "gpencil_stroke_snap_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "gpencil_v3d_align");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, gpencil_stroke_snap_items);
  RNA_def_property_ui_text(prop, "Stroke Snap", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "gpencil_surface_offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "gpencil_surface_offset");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Surface Offset", "Offset along the normal when drawing on surfaces");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 3);
  RNA_def_property_float_default(prop, 0.150f);

  prop = RNA_def_property(srna, "use_gpencil_project_only_selected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "gpencil_v3d_align", GP_PROJECT_DEPTH_ONLY_SELECTED);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Project Onto Selected", "Project the strokes only onto selected objects");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Grease Pencil - Select mode Edit */
  prop = RNA_def_property(srna, "gpencil_selectmode_edit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "gpencil_selectmode_edit");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, rna_enum_grease_pencil_selectmode_items);
  RNA_def_property_ui_text(prop, "Select Mode", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* Grease Pencil - Select mode Sculpt */
  prop = RNA_def_property(srna, "use_gpencil_select_mask_point", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "gpencil_selectmode_sculpt", GP_SCULPT_MASK_SELECTMODE_POINT);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Selection Mask", "Only sculpt selected stroke points");
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_POINTS, 0);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Gpencil_mask_point_update");

  prop = RNA_def_property(srna, "use_gpencil_select_mask_stroke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "gpencil_selectmode_sculpt", GP_SCULPT_MASK_SELECTMODE_STROKE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Selection Mask", "Only sculpt selected strokes");
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_STROKES, 0);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Gpencil_mask_stroke_update");

  prop = RNA_def_property(srna, "use_gpencil_select_mask_segment", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "gpencil_selectmode_sculpt", GP_SCULPT_MASK_SELECTMODE_SEGMENT);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Selection Mask", "Only sculpt selected stroke points between other strokes");
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_BETWEEN_STROKES, 0);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Gpencil_mask_segment_update");

  /* Grease Pencil - Select mode Vertex Paint */
  prop = RNA_def_property(srna, "use_gpencil_vertex_select_mask_point", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "gpencil_selectmode_vertex", GP_VERTEX_MASK_SELECTMODE_POINT);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Selection Mask", "Only paint selected stroke points");
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_POINTS, 0);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Gpencil_vertex_mask_point_update");

  prop = RNA_def_property(srna, "use_gpencil_vertex_select_mask_stroke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "gpencil_selectmode_vertex", GP_VERTEX_MASK_SELECTMODE_STROKE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Selection Mask", "Only paint selected strokes");
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_STROKES, 0);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Gpencil_vertex_mask_stroke_update");

  prop = RNA_def_property(srna, "use_gpencil_vertex_select_mask_segment", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "gpencil_selectmode_vertex", GP_VERTEX_MASK_SELECTMODE_SEGMENT);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Selection Mask", "Only paint selected stroke points between other strokes");
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_BETWEEN_STROKES, 0);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Gpencil_vertex_mask_segment_update");

  prop = RNA_def_property(srna, "use_grease_pencil_multi_frame_editing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gpencil_flags", GP_USE_MULTI_FRAME_EDITING);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Multi-frame Editing", "Enable multi-frame editing");
  RNA_def_property_ui_icon(prop, ICON_GP_MULTIFRAME_EDITING, 0);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  /* FIXME: We shouldn't have to tag all the Grease Pencil IDs for an update! */
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_all_grease_pencil_update");

  /* Annotations - 2D Views Stroke Placement */
  prop = RNA_def_property(srna, "annotation_stroke_placement_view2d", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "gpencil_v2d_align");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, annotation_stroke_placement_view2d_items);
  RNA_def_property_ui_text(prop, "Stroke Placement (2D View)", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Annotations - 3D View Stroke Placement */
  /* XXX: Do we need to decouple the stroke_endpoints setting too? */
  prop = RNA_def_property(srna, "annotation_stroke_placement_view3d", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "annotate_v3d_align");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, annotation_stroke_placement_view3d_items);
  RNA_def_property_enum_default(prop, GP_PROJECT_VIEWSPACE | GP_PROJECT_CURSOR);
  RNA_def_property_ui_text(prop,
                           "Annotation Stroke Placement (3D View)",
                           "How annotation strokes are orientated in 3D space");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_annotation_stroke_endpoints", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "annotate_v3d_align", GP_PROJECT_DEPTH_STROKE_ENDPOINTS);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Only Endpoints", "Only use the first and last parts of the stroke for snapping");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_annotation_project_only_selected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "annotate_v3d_align", GP_PROJECT_DEPTH_ONLY_SELECTED);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Project Onto Selected", "Project the strokes only onto selected objects");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Annotations - Stroke Thickness */
  prop = RNA_def_property(srna, "annotation_thickness", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "annotate_thickness");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_ui_text(prop, "Annotation Stroke Thickness", "Thickness of annotation strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Auto Keying */
  prop = RNA_def_property(srna, "use_keyframe_insert_auto", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "autokey_mode", AUTOKEY_ON);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "Auto Keying", "Automatic keyframe insertion for objects, bones and masks");
  RNA_def_property_ui_icon(prop, ICON_RECORD_OFF, 1);
  RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_AUTO, nullptr);

  prop = RNA_def_property(srna, "auto_keying_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "autokey_mode");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, auto_key_items);
  RNA_def_property_ui_text(prop,
                           "Auto-Keying Mode",
                           "Mode of automatic keyframe insertion for objects, bones and masks");

  prop = RNA_def_property(srna, "use_record_with_nla", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "keying_flag", AUTOKEY_FLAG_LAYERED_RECORD);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop,
      "Layered",
      "Add a new NLA Track + Strip for every loop/pass made over the animation "
      "to allow non-destructive tweaking");

  prop = RNA_def_property(srna, "use_keyframe_insert_keyingset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "keying_flag", AUTOKEY_FLAG_ONLYKEYINGSET);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop,
                           "Auto Keyframe Insert Keying Set",
                           "Automatic keyframe insertion using active Keying Set only");
  RNA_def_property_ui_icon(prop, ICON_KEYINGSET, 0);

  prop = RNA_def_property(srna, "use_keyframe_cycle_aware", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "keying_flag", KEYING_FLAG_CYCLEAWARE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop,
      "Cycle-Aware Keying",
      "For channels with cyclic extrapolation, keyframe insertion is automatically "
      "remapped inside the cycle time range, and keeps ends in sync. Curves newly added to "
      "actions with a Manual Frame Range and Cyclic Animation are automatically made cyclic.");

  /* Keyframing */
  prop = RNA_def_property(srna, "keyframe_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "keyframe_type");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, rna_enum_beztriple_keyframe_type_items);
  RNA_def_property_ui_text(
      prop, "New Keyframe Type", "Type of keyframes to create when inserting keyframes");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ACTION);

  /* UV */
  prop = RNA_def_property(srna, "uv_select_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "uv_selectmode");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, rna_enum_mesh_select_mode_uv_items);
  RNA_def_property_ui_text(prop, "UV Selection Mode", "UV selection and display mode");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_Scene_uv_select_mode_update");

  prop = RNA_def_property(srna, "uv_sticky_select_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "uv_sticky");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_items(prop, uv_sticky_mode_items);
  RNA_def_property_ui_text(
      prop, "Sticky Selection Mode", "Method for extending UV vertex selection");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_IMAGE, "rna_Scene_uv_sticky_select_mode_update");

  prop = RNA_def_property(srna, "use_uv_select_sync", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "uv_flag", UV_FLAG_SYNC_SELECT);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "UV Sync Selection", "Keep UV and edit mode mesh selection in sync");
  RNA_def_property_ui_icon(prop, ICON_UV_SYNC_SELECT, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "use_uv_select_island", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "uv_flag", UV_FLAG_ISLAND_SELECT);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "UV Island Selection", "Island selection");
  RNA_def_property_ui_icon(prop, ICON_UV_ISLANDSEL, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_uv_local_view", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "uv_flag", UV_FLAG_SHOW_SAME_IMAGE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(
      prop, "UV Local View", "Display only faces with the currently displayed image assigned");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  /* Mesh */
  prop = RNA_def_property(srna, "mesh_select_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_bitset_array_sdna(prop, nullptr, "selectmode", 1 << 0, 3);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Scene_editmesh_select_mode_set");
  RNA_def_property_ui_text(prop, "Mesh Selection Mode", "Which mesh elements selection works on");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_Scene_editmesh_select_mode_update");

  prop = RNA_def_property(srna, "vertex_group_weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "vgroup_weight");
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Vertex Group Weight", "Weight to assign in vertex groups");

  prop = RNA_def_property(srna, "use_edge_path_live_unwrap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_mode_live_unwrap", 1);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_ui_text(prop, "Live Unwrap", "Changing edge seams recalculates UV unwrap");

  prop = RNA_def_property(srna, "normal_vector", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_ui_text(prop, "Normal Vector", "Normal vector used to copy, add or multiply");
  RNA_def_property_ui_range(prop, -10000.0, 10000.0, 1, 3);

  /* Curve Paint Settings */
  prop = RNA_def_property(srna, "curve_paint_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "CurvePaintSettings");
  RNA_def_property_ui_text(prop, "Curve Paint Settings", nullptr);

  /* Mesh Statistics */
  prop = RNA_def_property(srna, "statvis", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "MeshStatVis");
  RNA_def_property_ui_text(prop, "Mesh Statistics Visualization", nullptr);

  /* CurveProfile */
  prop = RNA_def_property(srna, "custom_bevel_profile_preset", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "custom_bevel_profile_preset");
  RNA_def_property_struct_type(prop, "CurveProfile");
  RNA_def_property_ui_text(prop, "Curve Profile Widget", "Used for defining a profile's path");

  /* Sequencer tool settings */
  prop = RNA_def_property(srna, "sequencer_tool_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SequencerToolSettings");
  RNA_def_property_ui_text(prop, "Sequencer Tool Settings", nullptr);
}

static void rna_def_sequencer_tool_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem scale_fit_methods[] = {
      {SEQ_SCALE_TO_FIT, "FIT", 0, "Scale to Fit", "Scale image to fit within the canvas"},
      {SEQ_SCALE_TO_FILL, "FILL", 0, "Scale to Fill", "Scale image to completely fill the canvas"},
      {SEQ_STRETCH_TO_FILL, "STRETCH", 0, "Stretch to Fill", "Stretch image to fill the canvas"},
      {SEQ_USE_ORIGINAL_SIZE,
       "ORIGINAL",
       0,
       "Use Original Size",
       "Keep image at its original size"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem scale_overlap_modes[] = {
      {SEQ_OVERLAP_EXPAND, "EXPAND", 0, "Expand", "Move strips so transformed strips fit"},
      {SEQ_OVERLAP_OVERWRITE,
       "OVERWRITE",
       0,
       "Overwrite",
       "Trim or split strips to resolve overlap"},
      {SEQ_OVERLAP_SHUFFLE,
       "SHUFFLE",
       0,
       "Shuffle",
       "Move transformed strips to nearest free space to resolve overlap"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem pivot_points[] = {
      {V3D_AROUND_CENTER_BOUNDS, "CENTER", ICON_PIVOT_BOUNDBOX, "Bounding Box Center", ""},
      {V3D_AROUND_CENTER_MEDIAN, "MEDIAN", ICON_PIVOT_MEDIAN, "Median Point", ""},
      {V3D_AROUND_CURSOR, "CURSOR", ICON_PIVOT_CURSOR, "2D Cursor", "Pivot around the 2D cursor"},
      {V3D_AROUND_LOCAL_ORIGINS,
       "INDIVIDUAL_ORIGINS",
       ICON_PIVOT_INDIVIDUAL,
       "Individual Origins",
       "Pivot around each selected island's own median point"},
      {0, nullptr, 0, nullptr, nullptr},

  };
  srna = RNA_def_struct(brna, "SequencerToolSettings", nullptr);
  RNA_def_struct_path_func(srna, "rna_SequencerToolSettings_path");
  RNA_def_struct_ui_text(srna, "Sequencer Tool Settings", "");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  /* Add strip settings. */
  prop = RNA_def_property(srna, "fit_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, scale_fit_methods);
  RNA_def_property_ui_text(prop, "Fit Method", "Scale fit method");

  /* Transform snapping. */
  prop = RNA_def_property(srna, "snap_to_current_frame", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_mode", SEQ_SNAP_TO_CURRENT_FRAME);
  RNA_def_property_ui_text(prop, "Current Frame", "Snap to current frame");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_to_hold_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_mode", SEQ_SNAP_TO_STRIP_HOLD);
  RNA_def_property_ui_text(prop, "Hold Offset", "Snap to strip hold offsets");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_to_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_mode", SEQ_SNAP_TO_MARKERS);
  RNA_def_property_ui_text(prop, "Markers", "Snap to markers");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_to_retiming_keys", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_mode", SEQ_SNAP_TO_RETIMING);
  RNA_def_property_ui_text(prop, "Retiming Keys", "Snap to retiming keys");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_to_frame_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_mode", SEQ_SNAP_TO_FRAME_RANGE);
  RNA_def_property_ui_text(prop, "Frame Range", "Snap to preview or scene start and end frame");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_to_borders", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_mode", SEQ_SNAP_TO_PREVIEW_BORDERS);
  RNA_def_property_ui_text(prop, "Borders", "Snap to preview borders");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_to_center", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_mode", SEQ_SNAP_TO_PREVIEW_CENTER);
  RNA_def_property_ui_text(prop, "Center", "Snap to preview center");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_to_strips_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_mode", SEQ_SNAP_TO_STRIPS_PREVIEW);
  RNA_def_property_ui_text(
      prop, "Other Strips", "Snap to borders and origins of deselected, visible strips");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr); /* header redraw */

  prop = RNA_def_property(srna, "snap_ignore_muted", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SEQ_SNAP_IGNORE_MUTED);
  RNA_def_property_ui_text(prop, "Ignore Muted Strips", "Don't snap to hidden strips");

  prop = RNA_def_property(srna, "snap_ignore_sound", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SEQ_SNAP_IGNORE_SOUND);
  RNA_def_property_ui_text(prop, "Ignore Sound Strips", "Don't snap to sound strips");

  prop = RNA_def_property(srna, "use_snap_current_frame_to_strips", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "snap_flag", SEQ_SNAP_CURRENT_FRAME_TO_STRIPS);
  RNA_def_property_ui_text(
      prop, "Snap Current Frame to Strips", "Snap current frame to strip start or end");

  prop = RNA_def_property(srna, "snap_distance", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "snap_distance");
  RNA_def_property_int_default(prop, 15);
  RNA_def_property_ui_range(prop, 0, 50, 1, 1);
  RNA_def_property_ui_text(prop, "Snapping Distance", "Maximum distance for snapping in pixels");

  /* Transform overlap handling. */
  prop = RNA_def_property(srna, "overlap_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, scale_overlap_modes);
  RNA_def_property_ui_text(prop, "Overlap Mode", "How to resolve overlap after transformation");

  prop = RNA_def_property(srna, "pivot_point", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, pivot_points);
  RNA_def_property_ui_text(prop, "Pivot Point", "Rotation or scaling pivot point");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);
}

static void rna_def_curve_paint_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CurvePaintSettings", nullptr);
  RNA_def_struct_path_func(srna, "rna_CurvePaintSettings_path");
  RNA_def_struct_ui_text(srna, "Curve Paint Settings", "");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  static const EnumPropertyItem curve_type_items[] = {
      {CU_POLY, "POLY", 0, "Poly", ""},
      {CU_BEZIER, "BEZIER", 0, "Bézier", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "curve_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_sdna(prop, nullptr, "curve_type");
  RNA_def_property_enum_items(prop, curve_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of curve to use for new strokes");

  prop = RNA_def_property(srna, "use_corners_detect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CURVE_PAINT_FLAG_CORNERS_DETECT);
  RNA_def_property_ui_text(prop, "Detect Corners", "Detect corners and use non-aligned handles");

  prop = RNA_def_property(srna, "use_pressure_radius", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CURVE_PAINT_FLAG_PRESSURE_RADIUS);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Map tablet pressure to curve radius");

  prop = RNA_def_property(srna, "use_stroke_endpoints", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CURVE_PAINT_FLAG_DEPTH_STROKE_ENDPOINTS);
  RNA_def_property_ui_text(prop, "Only First", "Use the start of the stroke for the depth");

  prop = RNA_def_property(srna, "use_offset_absolute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CURVE_PAINT_FLAG_DEPTH_STROKE_OFFSET_ABS);
  RNA_def_property_ui_text(
      prop, "Absolute Offset", "Apply a fixed offset (don't scale by the radius)");

  prop = RNA_def_property(srna, "use_project_only_selected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CURVE_PAINT_FLAG_DEPTH_ONLY_SELECTED);
  RNA_def_property_ui_text(
      prop, "Project Onto Selected", "Project the strokes only onto selected objects");

  prop = RNA_def_property(srna, "error_threshold", PROP_INT, PROP_PIXEL);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_text(prop, "Tolerance", "Allow deviation for a smoother, less precise line");

  prop = RNA_def_property(srna, "fit_method", PROP_ENUM, PROP_PIXEL);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_sdna(prop, nullptr, "fit_method");
  RNA_def_property_enum_items(prop, rna_enum_curve_fit_method_items);
  RNA_def_property_ui_text(prop, "Method", "Curve fitting method");

  prop = RNA_def_property(srna, "corner_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_range(prop, 0, M_PI);
  RNA_def_property_ui_text(prop, "Corner Angle", "Angles above this are considered corners");

  prop = RNA_def_property(srna, "radius_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0f, 10.0, 10, 2);
  RNA_def_property_ui_text(
      prop,
      "Radius Min",
      "Minimum radius when the minimum pressure is applied (also the minimum when tapering)");

  prop = RNA_def_property(srna, "radius_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0f, 10.0, 10, 2);
  RNA_def_property_ui_text(
      prop,
      "Radius Max",
      "Radius to use when the maximum pressure is applied (or when a tablet isn't used)");

  prop = RNA_def_property(srna, "radius_taper_start", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0f, 1.0, 1, 2);
  RNA_def_property_ui_text(
      prop, "Radius Min", "Taper factor for the radius of each point along the curve");

  prop = RNA_def_property(srna, "radius_taper_end", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_range(prop, 0.0f, 1.0, 1, 2);
  RNA_def_property_ui_text(
      prop, "Radius Max", "Taper factor for the radius of each point along the curve");

  prop = RNA_def_property(srna, "surface_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_range(prop, -10.0, 10.0);
  RNA_def_property_ui_range(prop, -1.0f, 1.0, 1, 2);
  RNA_def_property_ui_text(prop, "Offset", "Offset the stroke from the surface");

  static const EnumPropertyItem depth_mode_items[] = {
      {CURVE_PAINT_PROJECT_CURSOR, "CURSOR", 0, "Cursor", ""},
      {CURVE_PAINT_PROJECT_SURFACE, "SURFACE", 0, "Surface", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "depth_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_sdna(prop, nullptr, "depth_mode");
  RNA_def_property_enum_items(prop, depth_mode_items);
  RNA_def_property_ui_text(prop, "Depth", "Method of projecting depth");

  static const EnumPropertyItem surface_plane_items[] = {
      {CURVE_PAINT_SURFACE_PLANE_NORMAL_VIEW,
       "NORMAL_VIEW",
       0,
       "Normal to Surface",
       "Draw in a plane perpendicular to the surface"},
      {CURVE_PAINT_SURFACE_PLANE_NORMAL_SURFACE,
       "NORMAL_SURFACE",
       0,
       "Tangent to Surface",
       "Draw in the surface plane"},
      {CURVE_PAINT_SURFACE_PLANE_VIEW,
       "VIEW",
       0,
       "View",
       "Draw in a plane aligned to the viewport"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "surface_plane", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DEG_SYNC_ONLY);
  RNA_def_property_enum_sdna(prop, nullptr, "surface_plane");
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "MeshStatVis", nullptr);
  RNA_def_struct_path_func(srna, "rna_MeshStatVis_path");
  RNA_def_struct_ui_text(srna, "Mesh Visualize Statistics", "");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, stat_type);
  RNA_def_property_ui_text(prop, "Type", "Type of data to visualize/check");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  /* overhang */
  prop = RNA_def_property(srna, "overhang_min", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "overhang_min");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "Overhang Min", "Minimum angle to display");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "overhang_max", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "overhang_max");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "Overhang Max", "Maximum angle to display");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "overhang_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "overhang_axis");
  RNA_def_property_enum_items(prop, rna_enum_object_axis_items);
  RNA_def_property_ui_text(prop, "Axis", "");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  /* thickness */
  prop = RNA_def_property(srna, "thickness_min", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "thickness_min");
  RNA_def_property_range(prop, 0.0f, 1000.0);
  RNA_def_property_ui_range(prop, 0.0f, 100.0, 0.001, 3);
  RNA_def_property_ui_text(prop, "Thickness Min", "Minimum for measuring thickness");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "thickness_max", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "thickness_max");
  RNA_def_property_range(prop, 0.0f, 1000.0);
  RNA_def_property_ui_range(prop, 0.0f, 100.0, 0.001, 3);
  RNA_def_property_ui_text(prop, "Thickness Max", "Maximum for measuring thickness");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "thickness_samples", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "thickness_samples");
  RNA_def_property_range(prop, 1, 32);
  RNA_def_property_ui_text(prop, "Samples", "Number of samples to test per face");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  /* distort */
  prop = RNA_def_property(srna, "distort_min", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "distort_min");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "Distort Min", "Minimum angle to display");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "distort_max", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "distort_max");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "Distort Max", "Maximum angle to display");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  /* sharp */
  prop = RNA_def_property(srna, "sharp_min", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "sharp_min");
  RNA_def_property_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "Sharpness Min", "Minimum angle to display");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_EditMesh_update");

  prop = RNA_def_property(srna, "sharp_max", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "sharp_max");
  RNA_def_property_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "Sharpness Max", "Maximum angle to display");
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem rotation_units[] = {
      {0, "DEGREES", 0, "Degrees", "Use degrees for measuring angles and rotations"},
      {USER_UNIT_ROT_RADIANS, "RADIANS", 0, "Radians", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "UnitSettings", nullptr);
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
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "scale_length", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_ui_text(
      prop,
      "Unit Scale",
      "Scale to use when converting between Blender units and dimensions."
      " When working at microscopic or astronomical scale, a small or large unit scale"
      " respectively can be used to avoid numerical precision problems");
  RNA_def_property_range(prop, 1e-9f, 1e+9f);
  RNA_def_property_ui_range(prop, 0.001, 100.0, 0.1, 6);
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "use_separate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", USER_UNIT_OPT_SPLIT);
  RNA_def_property_ui_text(prop, "Separate Units", "Display units in pairs (e.g. 1m 0cm)");
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "length_unit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_UnitSettings_length_unit_itemf");
  RNA_def_property_ui_text(prop, "Length Unit", "Unit that will be used to display length values");
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "mass_unit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_UnitSettings_mass_unit_itemf");
  RNA_def_property_ui_text(prop, "Mass Unit", "Unit that will be used to display mass values");
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "time_unit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_UnitSettings_time_unit_itemf");
  RNA_def_property_ui_text(prop, "Time Unit", "Unit that will be used to display time values");
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "temperature_unit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_UnitSettings_temperature_unit_itemf");
  RNA_def_property_ui_text(
      prop, "Temperature Unit", "Unit that will be used to display temperature values");
  RNA_def_property_update(prop, NC_WINDOW, nullptr);
}

static void rna_def_view_layer_eevee(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  srna = RNA_def_struct(brna, "ViewLayerEEVEE", nullptr);
  RNA_def_struct_path_func(srna, "rna_ViewLayerEEVEE_path");
  RNA_def_struct_ui_text(srna, "EEVEE Settings", "View Layer settings for EEVEE");

  prop = RNA_def_property(srna, "use_pass_volume_direct", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "render_passes", EEVEE_RENDER_PASS_VOLUME_LIGHT);
  RNA_def_property_ui_text(prop, "Volume Light", "Deliver volume direct light pass");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");

#  if 1
  /* Bloom is deprecated since Blender 4.2, is kept for add-on compatibility reasons and needs to
   * be removed in a future release. */
  prop = RNA_def_property(srna, "use_pass_bloom", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "render_passes", 0 /*EEVEE_RENDER_PASS_BLOOM*/);
  RNA_def_property_ui_text(prop, "Bloom", "Deliver bloom pass (deprecated)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
#  endif

  prop = RNA_def_property(srna, "use_pass_transparent", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "render_passes", EEVEE_RENDER_PASS_TRANSPARENT);
  RNA_def_property_ui_text(
      prop, "Transparent", "Deliver alpha blended surfaces in a separate pass");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");

  prop = RNA_def_property(srna, "ambient_occlusion_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
  RNA_def_property_ui_text(
      prop, "Distance", "Distance of object that contribute to the ambient occlusion effect");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
}

static void rna_def_view_layer_aovs(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  // PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "AOVs");
  srna = RNA_def_struct(brna, "AOVs", nullptr);
  RNA_def_struct_sdna(srna, "ViewLayer");
  RNA_def_struct_ui_text(srna, "List of AOVs", "Collection of AOVs");

  func = RNA_def_function(srna, "add", "BKE_view_layer_add_aov");
  parm = RNA_def_pointer(func, "aov", "AOV", "", "Newly created AOV");
  RNA_def_function_return(func, parm);

  /* Defined in `rna_layer.cc`. */
  func = RNA_def_function(srna, "remove", "rna_ViewLayer_remove_aov");
  parm = RNA_def_pointer(func, "aov", "AOV", "", "AOV to remove");
  RNA_def_function_ui_description(func, "Remove an AOV");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_view_layer_aov(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  srna = RNA_def_struct(brna, "AOV", nullptr);
  RNA_def_struct_sdna(srna, "ViewLayerAOV");
  RNA_def_struct_ui_text(srna, "Shader AOV", "");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Name", "Name of the AOV");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", AOV_CONFLICT);
  RNA_def_property_ui_text(prop, "Valid", "Is the name of the AOV conflicting");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, rna_enum_view_layer_aov_type_items);
  RNA_def_property_enum_default(prop, AOV_TYPE_COLOR);
  RNA_def_property_ui_text(prop, "Type", "Data type of the AOV");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
}

static void rna_def_view_layer_lightgroups(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  // PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "Lightgroups");
  srna = RNA_def_struct(brna, "Lightgroups", nullptr);
  RNA_def_struct_sdna(srna, "ViewLayer");
  RNA_def_struct_ui_text(srna, "List of Lightgroups", "Collection of Lightgroups");

  func = RNA_def_function(srna, "add", "BKE_view_layer_add_lightgroup");
  parm = RNA_def_pointer(func, "lightgroup", "Lightgroup", "", "Newly created Lightgroup");
  RNA_def_function_return(func, parm);
  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "Name of newly created lightgroup");

  func = RNA_def_function(srna, "remove", "BKE_view_layer_remove_lightgroup");
  parm = RNA_def_pointer(func, "lightgroup", "Lightgroup", "", "Lightgroup to remove");
  RNA_def_function_ui_description(func, "Remove given light group");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_view_layer_lightgroup(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  srna = RNA_def_struct(brna, "Lightgroup", nullptr);
  RNA_def_struct_sdna(srna, "ViewLayerLightgroup");
  RNA_def_struct_ui_text(srna, "Light Group", "");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_string_funcs(prop,
                                "rna_ViewLayerLightgroup_name_get",
                                "rna_ViewLayerLightgroup_name_length",
                                "rna_ViewLayerLightgroup_name_set");
  RNA_def_property_ui_text(prop, "Name", "Name of the Lightgroup");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  RNA_def_struct_name_property(srna, prop);
}

void rna_def_view_layer_common(BlenderRNA *brna, StructRNA *srna, const bool scene)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  if (scene) {
    RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_ViewLayer_name_set");
  }
  else {
    RNA_def_property_string_sdna(prop, nullptr, "name");
  }
  RNA_def_property_ui_text(prop, "Name", "View layer name");
  RNA_def_struct_name_property(srna, prop);
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  if (scene) {
    prop = RNA_def_property(srna, "material_override", PROP_POINTER, PROP_NONE);
    RNA_def_property_pointer_sdna(prop, nullptr, "mat_override");
    RNA_def_property_struct_type(prop, "Material");
    RNA_def_property_flag(prop, PROP_EDITABLE);
    RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
    RNA_def_property_ui_text(
        prop, "Material Override", "Material to override all other materials in this view layer");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_override_update");

    prop = RNA_def_property(srna, "world_override", PROP_POINTER, PROP_NONE);
    RNA_def_property_pointer_sdna(prop, nullptr, "world_override");
    RNA_def_property_struct_type(prop, "World");
    RNA_def_property_flag(prop, PROP_EDITABLE);
    RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
    RNA_def_property_ui_text(prop, "World Override", "Override world in this view layer");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_override_update");

    prop = RNA_def_property(srna, "samples", PROP_INT, PROP_UNSIGNED);
    RNA_def_property_ui_text(prop,
                             "Samples",
                             "Override number of render samples for this view layer, "
                             "0 will use the scene setting");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

    prop = RNA_def_property(srna, "pass_alpha_threshold", PROP_FLOAT, PROP_FACTOR);
    RNA_def_property_ui_text(
        prop,
        "Alpha Threshold",
        "Z, Index, normal, UV and vector passes are only affected by surfaces with "
        "alpha transparency equal to or higher than this threshold");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

    prop = RNA_def_property(srna, "eevee", PROP_POINTER, PROP_NONE);
    RNA_def_property_flag(prop, PROP_NEVER_NULL);
    RNA_def_property_struct_type(prop, "ViewLayerEEVEE");
    RNA_def_property_ui_text(prop, "EEVEE Settings", "View layer settings for EEVEE");

    prop = RNA_def_property(srna, "aovs", PROP_COLLECTION, PROP_NONE);
    RNA_def_property_collection_sdna(prop, nullptr, "aovs", nullptr);
    RNA_def_property_struct_type(prop, "AOV");
    RNA_def_property_ui_text(prop, "Shader AOV", "");
    rna_def_view_layer_aovs(brna, prop);

    prop = RNA_def_property(srna, "active_aov", PROP_POINTER, PROP_NONE);
    RNA_def_property_struct_type(prop, "AOV");
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
    RNA_def_property_ui_text(prop, "Shader AOV", "Active AOV");

    prop = RNA_def_property(srna, "active_aov_index", PROP_INT, PROP_UNSIGNED);
    RNA_def_property_int_funcs(prop,
                               "rna_ViewLayer_active_aov_index_get",
                               "rna_ViewLayer_active_aov_index_set",
                               "rna_ViewLayer_active_aov_index_range");
    RNA_def_property_ui_text(prop, "Active AOV Index", "Index of active AOV");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

    prop = RNA_def_property(srna, "lightgroups", PROP_COLLECTION, PROP_NONE);
    RNA_def_property_collection_sdna(prop, nullptr, "lightgroups", nullptr);
    RNA_def_property_struct_type(prop, "Lightgroup");
    RNA_def_property_ui_text(prop, "Light Groups", "");
    rna_def_view_layer_lightgroups(brna, prop);

    prop = RNA_def_property(srna, "active_lightgroup", PROP_POINTER, PROP_NONE);
    RNA_def_property_struct_type(prop, "Lightgroup");
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
    RNA_def_property_ui_text(prop, "Light Groups", "Active Lightgroup");

    prop = RNA_def_property(srna, "active_lightgroup_index", PROP_INT, PROP_UNSIGNED);
    RNA_def_property_int_funcs(prop,
                               "rna_ViewLayer_active_lightgroup_index_get",
                               "rna_ViewLayer_active_lightgroup_index_set",
                               "rna_ViewLayer_active_lightgroup_index_range");
    RNA_def_property_ui_text(prop, "Active Lightgroup Index", "Index of active lightgroup");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

    prop = RNA_def_property(srna, "use_pass_cryptomatte_object", PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_boolean_sdna(
        prop, nullptr, "cryptomatte_flag", VIEW_LAYER_CRYPTOMATTE_OBJECT);
    RNA_def_property_ui_text(
        prop,
        "Cryptomatte Object",
        "Render cryptomatte object pass, for isolating objects in compositing");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");

    prop = RNA_def_property(srna, "use_pass_cryptomatte_material", PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_boolean_sdna(
        prop, nullptr, "cryptomatte_flag", VIEW_LAYER_CRYPTOMATTE_MATERIAL);
    RNA_def_property_ui_text(
        prop,
        "Cryptomatte Material",
        "Render cryptomatte material pass, for isolating materials in compositing");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");

    prop = RNA_def_property(srna, "use_pass_cryptomatte_asset", PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_boolean_sdna(prop, nullptr, "cryptomatte_flag", VIEW_LAYER_CRYPTOMATTE_ASSET);
    RNA_def_property_ui_text(
        prop,
        "Cryptomatte Asset",
        "Render cryptomatte asset pass, for isolating groups of objects with the same parent");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");

    prop = RNA_def_property(srna, "pass_cryptomatte_depth", PROP_INT, PROP_NONE);
    RNA_def_property_int_sdna(prop, nullptr, "cryptomatte_levels");
    RNA_def_property_range(prop, 2.0, 16.0);
    RNA_def_property_ui_text(
        prop, "Cryptomatte Levels", "Sets how many unique objects can be distinguished per pixel");
    RNA_def_property_ui_range(prop, 2.0, 16.0, 2.0, 0.0);
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");

    prop = RNA_def_property(srna, "use_pass_cryptomatte_accurate", PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_boolean_sdna(
        prop, nullptr, "cryptomatte_flag", VIEW_LAYER_CRYPTOMATTE_ACCURATE);
    RNA_def_property_ui_text(
        prop, "Cryptomatte Accurate", "Generate a more accurate cryptomatte pass");
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }

  prop = RNA_def_property(srna, "use_solid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "layflag", SCE_LAY_SOLID);
  RNA_def_property_ui_text(prop, "Solid", "Render Solid faces in this Layer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }
  prop = RNA_def_property(srna, "use_sky", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "layflag", SCE_LAY_SKY);
  RNA_def_property_ui_text(prop, "Sky", "Render Sky in this Layer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_render_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_ao", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "layflag", SCE_LAY_AO);
  RNA_def_property_ui_text(prop, "Ambient Occlusion", "Render Ambient Occlusion in this Layer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_render_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_strand", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "layflag", SCE_LAY_STRAND);
  RNA_def_property_ui_text(prop, "Strand", "Render Strands in this Layer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_volumes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "layflag", SCE_LAY_VOLUMES);
  RNA_def_property_ui_text(prop, "Volumes", "Render volumes in this Layer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_motion_blur", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "layflag", SCE_LAY_MOTION_BLUR);
  RNA_def_property_ui_text(
      prop, "Motion Blur", "Render motion blur in this Layer, if enabled in the scene");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_grease_pencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "layflag", SCE_LAY_GREASE_PENCIL);
  RNA_def_property_ui_text(prop, "Grease Pencil", "Render Grease Pencil on this layer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  /* passes */
  prop = RNA_def_property(srna, "use_pass_combined", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_COMBINED);
  RNA_def_property_ui_text(prop, "Combined", "Deliver full combined RGBA buffer");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_DEPTH);
  RNA_def_property_ui_text(prop, "Depth", "Deliver depth values pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_vector", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_VECTOR);
  RNA_def_property_ui_text(prop, "Vector", "Deliver speed vector pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_position", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_POSITION);
  RNA_def_property_ui_text(prop, "Position", "Deliver position pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_normal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_NORMAL);
  RNA_def_property_ui_text(prop, "Normal", "Deliver normal pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_uv", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_UV);
  RNA_def_property_ui_text(prop, "UV", "Deliver texture UV pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_mist", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_MIST);
  RNA_def_property_ui_text(prop, "Mist", "Deliver mist factor pass (0.0 to 1.0)");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_object_index", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_INDEXOB);
  RNA_def_property_ui_text(prop, "Object Index", "Deliver object index pass");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SCENE);
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_material_index", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_INDEXMA);
  RNA_def_property_ui_text(prop, "Material Index", "Deliver material index pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_SHADOW);
  RNA_def_property_ui_text(prop, "Shadow", "Deliver shadow pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_ambient_occlusion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_AO);
  RNA_def_property_ui_text(prop, "Ambient Occlusion", "Deliver Ambient Occlusion pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_emit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_EMIT);
  RNA_def_property_ui_text(prop, "Emit", "Deliver emission pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_environment", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_ENVIRONMENT);
  RNA_def_property_ui_text(prop, "Environment", "Deliver environment lighting pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_diffuse_direct", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_DIFFUSE_DIRECT);
  RNA_def_property_ui_text(prop, "Diffuse Direct", "Deliver diffuse direct pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_diffuse_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_DIFFUSE_INDIRECT);
  RNA_def_property_ui_text(prop, "Diffuse Indirect", "Deliver diffuse indirect pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_diffuse_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_DIFFUSE_COLOR);
  RNA_def_property_ui_text(prop, "Diffuse Color", "Deliver diffuse color pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_glossy_direct", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_GLOSSY_DIRECT);
  RNA_def_property_ui_text(prop, "Glossy Direct", "Deliver glossy direct pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_glossy_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_GLOSSY_INDIRECT);
  RNA_def_property_ui_text(prop, "Glossy Indirect", "Deliver glossy indirect pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_glossy_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_GLOSSY_COLOR);
  RNA_def_property_ui_text(prop, "Glossy Color", "Deliver glossy color pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_transmission_direct", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_TRANSM_DIRECT);
  RNA_def_property_ui_text(prop, "Transmission Direct", "Deliver transmission direct pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_transmission_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_TRANSM_INDIRECT);
  RNA_def_property_ui_text(prop, "Transmission Indirect", "Deliver transmission indirect pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_transmission_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_TRANSM_COLOR);
  RNA_def_property_ui_text(prop, "Transmission Color", "Deliver transmission color pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_subsurface_direct", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_SUBSURFACE_DIRECT);
  RNA_def_property_ui_text(prop, "Subsurface Direct", "Deliver subsurface direct pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_subsurface_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_SUBSURFACE_INDIRECT);
  RNA_def_property_ui_text(prop, "Subsurface Indirect", "Deliver subsurface indirect pass");
  if (scene) {
    RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  }

  prop = RNA_def_property(srna, "use_pass_subsurface_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "passflag", SCE_PASS_SUBSURFACE_COLOR);
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
  srna = RNA_def_struct(brna, "FreestyleModules", nullptr);
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
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_freestyle_linesets(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "Linesets");
  srna = RNA_def_struct(brna, "Linesets", nullptr);
  RNA_def_struct_sdna(srna, "FreestyleSettings");
  RNA_def_struct_ui_text(
      srna, "Line Sets", "Line sets for associating lines and style parameters");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "FreestyleLineSet");
  RNA_def_property_pointer_funcs(
      prop, "rna_FreestyleSettings_active_lineset_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Active Line Set", "Active line set being displayed");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_FreestyleSettings_active_lineset_index_get",
                             "rna_FreestyleSettings_active_lineset_index_set",
                             "rna_FreestyleSettings_active_lineset_index_range");
  RNA_def_property_ui_text(prop, "Active Line Set Index", "Index of active line set slot");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  func = RNA_def_function(srna, "new", "rna_FreestyleSettings_lineset_add");
  RNA_def_function_ui_description(func, "Add a line set to scene render layer Freestyle settings");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_SELF_ID);
  parm = RNA_def_string(func, "name", "LineSet", 0, "", "New name for the line set (not unique)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "lineset", "FreestyleLineSet", "", "Newly created line set");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_FreestyleSettings_lineset_remove");
  RNA_def_function_ui_description(func,
                                  "Remove a line set from scene render layer Freestyle settings");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "lineset", "FreestyleLineSet", "", "Line set to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
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
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem face_mark_condition_items[] = {
      {0, "ONE", 0, "One Face", "Select a feature edge if either of its adjacent faces is marked"},
      {FREESTYLE_LINESET_FM_BOTH,
       "BOTH",
       0,
       "Both Faces",
       "Select a feature edge if both of its adjacent faces are marked"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem freestyle_ui_mode_items[] = {
      {FREESTYLE_CONTROL_SCRIPT_MODE,
       "SCRIPT",
       0,
       "Python Scripting",
       "Advanced mode for using style modules written in Python"},
      {FREESTYLE_CONTROL_EDITOR_MODE,
       "EDITOR",
       0,
       "Parameter Editor",
       "Basic mode for interactive style parameter editing"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem visibility_items[] = {
      {FREESTYLE_QI_VISIBLE, "VISIBLE", 0, "Visible", "Select visible feature edges"},
      {FREESTYLE_QI_HIDDEN, "HIDDEN", 0, "Hidden", "Select hidden feature edges"},
      {FREESTYLE_QI_RANGE,
       "RANGE",
       0,
       "Quantitative Invisibility",
       "Select feature edges within a range of quantitative invisibility (QI) values"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* FreestyleLineSet */

  srna = RNA_def_struct(brna, "FreestyleLineSet", nullptr);
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
                                 nullptr,
                                 nullptr);
  RNA_def_property_ui_text(prop, "Line Style", "Line style settings");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Line Set Name", "Line set name");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FREESTYLE_LINESET_ENABLED);
  RNA_def_property_ui_text(
      prop, "Render", "Enable or disable this line set during stroke rendering");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_by_visibility", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "selection", FREESTYLE_SEL_VISIBILITY);
  RNA_def_property_ui_text(
      prop, "Selection by Visibility", "Select feature edges based on visibility");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_by_edge_types", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "selection", FREESTYLE_SEL_EDGE_TYPES);
  RNA_def_property_ui_text(
      prop, "Selection by Edge Types", "Select feature edges based on edge types");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_by_collection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "selection", FREESTYLE_SEL_GROUP);
  RNA_def_property_ui_text(
      prop, "Selection by Collection", "Select feature edges based on a collection of objects");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_by_image_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "selection", FREESTYLE_SEL_IMAGE_BORDER);
  RNA_def_property_ui_text(prop,
                           "Selection by Image Border",
                           "Select feature edges by image border (less memory consumption)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_by_face_marks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "selection", FREESTYLE_SEL_FACE_MARK);
  RNA_def_property_ui_text(prop, "Selection by Face Marks", "Select feature edges by face marks");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "edge_type_negation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flags");
  RNA_def_property_enum_items(prop, edge_type_negation_items);
  RNA_def_property_ui_text(
      prop,
      "Edge Type Negation",
      "Specify either inclusion or exclusion of feature edges selected by edge types");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "edge_type_combination", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flags");
  RNA_def_property_enum_items(prop, edge_type_combination_items);
  RNA_def_property_ui_text(
      prop,
      "Edge Type Combination",
      "Specify a logical combination of selection conditions on feature edge types");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "group");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Collection", "A collection of objects based on which feature edges are selected");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "collection_negation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flags");
  RNA_def_property_enum_items(prop, collection_negation_items);
  RNA_def_property_ui_text(prop,
                           "Collection Negation",
                           "Specify either inclusion or exclusion of feature edges belonging to a "
                           "collection of objects");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "face_mark_negation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flags");
  RNA_def_property_enum_items(prop, face_mark_negation_items);
  RNA_def_property_ui_text(
      prop,
      "Face Mark Negation",
      "Specify either inclusion or exclusion of feature edges selected by face marks");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "face_mark_condition", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flags");
  RNA_def_property_enum_items(prop, face_mark_condition_items);
  RNA_def_property_ui_text(prop,
                           "Face Mark Condition",
                           "Specify a feature edge selection condition based on face marks");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_silhouette", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", FREESTYLE_FE_SILHOUETTE);
  RNA_def_property_ui_text(
      prop,
      "Silhouette",
      "Select silhouettes (edges at the boundary of visible and hidden faces)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", FREESTYLE_FE_BORDER);
  RNA_def_property_ui_text(prop, "Border", "Select border edges (open mesh edges)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_crease", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", FREESTYLE_FE_CREASE);
  RNA_def_property_ui_text(prop,
                           "Crease",
                           "Select crease edges (those between two faces making an angle smaller "
                           "than the Crease Angle)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_ridge_valley", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", FREESTYLE_FE_RIDGE_VALLEY);
  RNA_def_property_ui_text(
      prop,
      "Ridge & Valley",
      "Select ridges and valleys (boundary lines between convex and concave areas of surface)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_suggestive_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", FREESTYLE_FE_SUGGESTIVE_CONTOUR);
  RNA_def_property_ui_text(
      prop, "Suggestive Contour", "Select suggestive contours (almost silhouette/contour edges)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_material_boundary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", FREESTYLE_FE_MATERIAL_BOUNDARY);
  RNA_def_property_ui_text(prop, "Material Boundary", "Select edges at material boundaries");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", FREESTYLE_FE_CONTOUR);
  RNA_def_property_ui_text(prop, "Contour", "Select contours (outer silhouettes of each object)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_external_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", FREESTYLE_FE_EXTERNAL_CONTOUR);
  RNA_def_property_ui_text(
      prop,
      "External Contour",
      "Select external contours (outer silhouettes of occluding and occluded objects)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "select_edge_mark", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", FREESTYLE_FE_EDGE_MARK);
  RNA_def_property_ui_text(
      prop, "Edge Mark", "Select edge marks (edges annotated by Freestyle edge marks)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_silhouette", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "exclude_edge_types", FREESTYLE_FE_SILHOUETTE);
  RNA_def_property_ui_text(prop, "Silhouette", "Exclude silhouette edges");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "exclude_edge_types", FREESTYLE_FE_BORDER);
  RNA_def_property_ui_text(prop, "Border", "Exclude border edges");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_crease", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "exclude_edge_types", FREESTYLE_FE_CREASE);
  RNA_def_property_ui_text(prop, "Crease", "Exclude crease edges");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_ridge_valley", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "exclude_edge_types", FREESTYLE_FE_RIDGE_VALLEY);
  RNA_def_property_ui_text(prop, "Ridge & Valley", "Exclude ridges and valleys");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_suggestive_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "exclude_edge_types", FREESTYLE_FE_SUGGESTIVE_CONTOUR);
  RNA_def_property_ui_text(prop, "Suggestive Contour", "Exclude suggestive contours");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_material_boundary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "exclude_edge_types", FREESTYLE_FE_MATERIAL_BOUNDARY);
  RNA_def_property_ui_text(prop, "Material Boundary", "Exclude edges at material boundaries");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "exclude_edge_types", FREESTYLE_FE_CONTOUR);
  RNA_def_property_ui_text(prop, "Contour", "Exclude contours");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_external_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "exclude_edge_types", FREESTYLE_FE_EXTERNAL_CONTOUR);
  RNA_def_property_ui_text(prop, "External Contour", "Exclude external contours");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "exclude_edge_mark", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "exclude_edge_types", FREESTYLE_FE_EDGE_MARK);
  RNA_def_property_ui_text(prop, "Edge Mark", "Exclude edge marks");
  RNA_def_property_ui_icon(prop, ICON_X, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "visibility", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "qi");
  RNA_def_property_enum_items(prop, visibility_items);
  RNA_def_property_ui_text(
      prop, "Visibility", "Determine how to use visibility for feature edge selection");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "qi_start", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "qi_start");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop, "Start", "First QI value of the QI range");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "qi_end", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "qi_end");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop, "End", "Last QI value of the QI range");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  /* FreestyleModuleSettings */

  srna = RNA_def_struct(brna, "FreestyleModuleSettings", nullptr);
  RNA_def_struct_sdna(srna, "FreestyleModuleConfig");
  RNA_def_struct_ui_text(
      srna, "Freestyle Module", "Style module configuration for specifying a style module");

  prop = RNA_def_property(srna, "script", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Text");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Style Module", "Python script to define a style module");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "is_displayed", 1);
  RNA_def_property_ui_text(
      prop, "Use", "Enable or disable this style module during stroke rendering");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  /* FreestyleSettings */

  srna = RNA_def_struct(brna, "FreestyleSettings", nullptr);
  RNA_def_struct_sdna(srna, "FreestyleConfig");
  RNA_def_struct_nested(brna, srna, "ViewLayer");
  RNA_def_struct_ui_text(
      srna, "Freestyle Settings", "Freestyle settings for a ViewLayer data-block");

  prop = RNA_def_property(srna, "modules", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "modules", nullptr);
  RNA_def_property_struct_type(prop, "FreestyleModuleSettings");
  RNA_def_property_ui_text(
      prop, "Style Modules", "A list of style modules (to be applied from top to bottom)");
  rna_def_freestyle_modules(brna, prop);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, freestyle_ui_mode_items);
  RNA_def_property_ui_text(prop, "Control Mode", "Select the Freestyle control mode");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_culling", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FREESTYLE_CULLING);
  RNA_def_property_ui_text(prop, "Culling", "If enabled, out-of-view edges are ignored");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_suggestive_contours", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FREESTYLE_SUGGESTIVE_CONTOURS_FLAG);
  RNA_def_property_ui_text(prop, "Suggestive Contours", "Enable suggestive contours");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_ridges_and_valleys", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FREESTYLE_RIDGES_AND_VALLEYS_FLAG);
  RNA_def_property_ui_text(prop, "Ridges and Valleys", "Enable ridges and valleys");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_material_boundaries", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FREESTYLE_MATERIAL_BOUNDARIES_FLAG);
  RNA_def_property_ui_text(prop, "Material Boundaries", "Enable material boundaries");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_smoothness", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FREESTYLE_FACE_SMOOTHNESS_FLAG);
  RNA_def_property_ui_text(
      prop, "Face Smoothness", "Take face smoothness into account in view map calculation");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "use_view_map_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FREESTYLE_VIEW_MAP_CACHE);
  RNA_def_property_ui_text(
      prop,
      "View Map Cache",
      "Keep the computed view map and avoid recalculating it if mesh geometry is unchanged");
  RNA_def_property_update(
      prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_use_view_map_cache_update");

  prop = RNA_def_property(srna, "as_render_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FREESTYLE_AS_RENDER_PASS);
  RNA_def_property_ui_text(
      prop,
      "As Render Pass",
      "Renders Freestyle output to a separate pass instead of overlaying it on the Combined pass");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");

  prop = RNA_def_property(srna, "sphere_radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sphere_radius");
  RNA_def_property_float_default(prop, 1.0);
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_text(prop, "Sphere Radius", "Sphere radius for computing curvatures");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "kr_derivative_epsilon", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_default(prop, 0.0);
  RNA_def_property_float_sdna(prop, nullptr, "dkr_epsilon");
  RNA_def_property_range(prop, -1000.0, 1000.0);
  RNA_def_property_ui_text(
      prop, "Kr Derivative Epsilon", "Kr derivative epsilon for computing suggestive contours");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "crease_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "crease_angle");
  RNA_def_property_range(prop, 0.0, DEG2RAD(180.0));
  RNA_def_property_ui_text(prop, "Crease Angle", "Angular threshold for detecting crease edges");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "linesets", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "linesets", nullptr);
  RNA_def_property_struct_type(prop, "FreestyleLineSet");
  RNA_def_property_ui_text(prop, "Line Sets", "");
  rna_def_freestyle_linesets(brna, prop);
}

static void rna_def_bake_data(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BakeSettings", nullptr);
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
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "File Path", "Image filepath to use when saving externally");
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "width", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 4, 10000);
  RNA_def_property_ui_text(prop, "Width", "Horizontal dimension of the baking map");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "height", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 4, 10000);
  RNA_def_property_ui_text(prop, "Height", "Vertical dimension of the baking map");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "margin", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_range(prop, 0, 64, 1, 1);
  RNA_def_property_ui_text(prop, "Margin", "Extends the baked result as a post process filter");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "margin_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_bake_margin_type_items);
  RNA_def_property_ui_text(prop, "Margin Type", "Algorithm to extend the baked result");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "max_ray_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 3);
  RNA_def_property_ui_text(prop,
                           "Max Ray Distance",
                           "The maximum ray distance for matching points between the active and "
                           "selected objects. If zero, there is no limit.");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "cage_extrusion", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 3);
  RNA_def_property_ui_text(
      prop,
      "Cage Extrusion",
      "Inflate the active object by the specified distance for baking. This helps matching to "
      "points nearer to the outside of the selected object meshes.");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "normal_space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "normal_space");
  RNA_def_property_enum_items(prop, rna_enum_normal_space_items);
  RNA_def_property_ui_text(prop, "Normal Space", "Choose normal space for baking");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "normal_r", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "normal_swizzle[0]");
  RNA_def_property_enum_items(prop, rna_enum_normal_swizzle_items);
  RNA_def_property_ui_text(prop, "Normal Space", "Axis to bake in red channel");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "normal_g", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "normal_swizzle[1]");
  RNA_def_property_enum_items(prop, rna_enum_normal_swizzle_items);
  RNA_def_property_ui_text(prop, "Normal Space", "Axis to bake in green channel");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "normal_b", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "normal_swizzle[2]");
  RNA_def_property_enum_items(prop, rna_enum_normal_swizzle_items);
  RNA_def_property_ui_text(prop, "Normal Space", "Axis to bake in blue channel");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "image_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "im_format");
  RNA_def_property_struct_type(prop, "ImageFormatSettings");
  RNA_def_property_ui_text(prop, "Image Format", "");

  prop = RNA_def_property(srna, "target", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_bake_target_items);
  RNA_def_property_ui_text(prop, "Target", "Where to output the baked map");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "save_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "save_mode");
  RNA_def_property_enum_items(prop, rna_enum_bake_save_mode_items);
  RNA_def_property_ui_text(prop, "Save Mode", "Where to save baked image textures");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "view_from", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_bake_view_from_items);
  RNA_def_property_ui_text(prop, "View From", "Source of reflection ray directions");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* flags */
  prop = RNA_def_property(srna, "use_selected_to_active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", R_BAKE_TO_ACTIVE);
  RNA_def_property_ui_text(prop,
                           "Selected to Active",
                           "Bake shading on the surface of selected objects to the active object");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_clear", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", R_BAKE_CLEAR);
  RNA_def_property_ui_text(prop, "Clear", "Clear Images before baking (internal only)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_split_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", R_BAKE_SPLIT_MAT);
  RNA_def_property_ui_text(
      prop, "Split Materials", "Split external images per material (external only)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_automatic_name", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", R_BAKE_AUTO_NAME);
  RNA_def_property_ui_text(
      prop,
      "Automatic Name",
      "Automatically name the output file with the pass type (external only)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_cage", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", R_BAKE_CAGE);
  RNA_def_property_ui_text(prop, "Cage", "Cast rays to active object from a cage");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* custom passes flags */
  prop = RNA_def_property(srna, "use_pass_emit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "pass_filter", R_BAKE_PASS_FILTER_EMIT);
  RNA_def_property_ui_text(prop, "Emit", "Add emission contribution");

  prop = RNA_def_property(srna, "use_pass_direct", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "pass_filter", R_BAKE_PASS_FILTER_DIRECT);
  RNA_def_property_ui_text(prop, "Direct", "Add direct lighting contribution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_pass_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "pass_filter", R_BAKE_PASS_FILTER_INDIRECT);
  RNA_def_property_ui_text(prop, "Indirect", "Add indirect lighting contribution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_pass_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "pass_filter", R_BAKE_PASS_FILTER_COLOR);
  RNA_def_property_ui_text(prop, "Color", "Color the pass");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_pass_diffuse", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "pass_filter", R_BAKE_PASS_FILTER_DIFFUSE);
  RNA_def_property_ui_text(prop, "Diffuse", "Add diffuse contribution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_pass_glossy", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "pass_filter", R_BAKE_PASS_FILTER_GLOSSY);
  RNA_def_property_ui_text(prop, "Glossy", "Add glossy contribution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_pass_transmission", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "pass_filter", R_BAKE_PASS_FILTER_TRANSM);
  RNA_def_property_ui_text(prop, "Transmission", "Add transmission contribution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "pass_filter", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "pass_filter");
  RNA_def_property_enum_items(prop, rna_enum_bake_pass_filter_type_items);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_ui_text(prop, "Pass Filter", "Passes to include in the active baking pass");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_view_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ViewLayers");
  srna = RNA_def_struct(brna, "ViewLayers", nullptr);
  RNA_def_struct_sdna(srna, "Scene");
  RNA_def_struct_ui_text(srna, "Render Layers", "Collection of render layers");

  func = RNA_def_function(srna, "new", "rna_ViewLayer_new");
  RNA_def_function_ui_description(func, "Add a view layer to scene");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_string(
      func, "name", "ViewLayer", 0, "", "New name for the view layer (not unique)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "result", "ViewLayer", "", "Newly created view layer");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_ViewLayer_remove");
  RNA_def_function_ui_description(func, "Remove a view layer");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "ViewLayer", "", "View layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "move", "rna_ViewLayer_move");
  RNA_def_function_ui_description(func, "Move a view layer");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "from_index", -1, INT_MIN, INT_MAX, "From Index", "Index to move", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "to_index", -1, INT_MIN, INT_MAX, "To Index", "Target index", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

/* Render Views - MultiView */
static void rna_def_scene_render_view(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SceneRenderView", nullptr);
  RNA_def_struct_ui_text(
      srna, "Scene Render View", "Render viewpoint for 3D stereo and multiview rendering");
  RNA_def_struct_ui_icon(srna, ICON_RESTRICT_RENDER_OFF);
  RNA_def_struct_path_func(srna, "rna_SceneRenderView_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_SceneRenderView_name_set");
  RNA_def_property_ui_text(prop, "Name", "Render view name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "file_suffix", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "suffix");
  RNA_def_property_ui_text(prop, "File Suffix", "Suffix added to the render images for this view");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "camera_suffix", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "suffix");
  RNA_def_property_ui_text(
      prop,
      "Camera Suffix",
      "Suffix to identify the cameras to use, and added to the render images for this view");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "viewflag", SCE_VIEW_DISABLE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Enabled", "Disable or enable the render view");
  RNA_def_property_update(
      prop, NC_SCENE | ND_RENDER_OPTIONS | NC_NODE | ND_DISPLAY, "rna_Scene_compositor_update");
}

static void rna_def_render_views(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "RenderViews");
  srna = RNA_def_struct(brna, "RenderViews", nullptr);
  RNA_def_struct_sdna(srna, "RenderData");
  RNA_def_struct_ui_text(srna, "Render Views", "Collection of render views");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "actview");
  RNA_def_property_int_funcs(prop,
                             "rna_RenderSettings_active_view_index_get",
                             "rna_RenderSettings_active_view_index_set",
                             "rna_RenderSettings_active_view_index_range");
  RNA_def_property_ui_text(prop, "Active View Index", "Active index in render view array");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SceneRenderView");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_RenderSettings_active_view_get",
                                 "rna_RenderSettings_active_view_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Active Render View", "Active Render View");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  func = RNA_def_function(srna, "new", "rna_RenderView_new");
  RNA_def_function_ui_description(func, "Add a render view to scene");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_string(func, "name", "RenderView", 0, "", "New name for the marker (not unique)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "result", "SceneRenderView", "", "Newly created render view");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_RenderView_remove");
  RNA_def_function_ui_description(func, "Remove a render view");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  parm = RNA_def_pointer(func, "view", "SceneRenderView", "", "Render view to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Stereo3dFormat", nullptr);
  RNA_def_struct_sdna(srna, "Stereo3dFormat");
  RNA_def_struct_ui_text(srna, "Stereo Output", "Settings for stereo output");

  prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "display_mode");
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
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", S3D_INTERLACE_SWAP);
  RNA_def_property_ui_text(prop, "Swap Left/Right", "Swap left and right stereo channels");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Stereo3dFormat_update");

  prop = RNA_def_property(srna, "use_sidebyside_crosseyed", PROP_BOOLEAN, PROP_BOOLEAN);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", S3D_SIDEBYSIDE_CROSSEYED);
  RNA_def_property_ui_text(prop, "Cross-Eyed", "Right eye should see left image and vice versa");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Stereo3dFormat_update");

  prop = RNA_def_property(srna, "use_squeezed_frame", PROP_BOOLEAN, PROP_BOOLEAN);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", S3D_SQUEEZED_FRAME);
  RNA_def_property_ui_text(prop, "Squeezed Frame", "Combine both views in a squeezed image");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Stereo3dFormat_update");
}

/* use for render output and image save operator,
 * NOTE: there are some cases where the members act differently when this is
 * used from a scene, video formats can only be selected for render output
 * for example, this is checked by seeing if the ptr->owner_id is a Scene id */

static void rna_def_scene_image_format_data(BlenderRNA *brna)
{
#  ifdef WITH_IMAGE_OPENJPEG
  static const EnumPropertyItem jp2_codec_items[] = {
      {R_IMF_JP2_CODEC_JP2, "JP2", 0, "JP2", ""},
      {R_IMF_JP2_CODEC_J2K, "J2K", 0, "J2K", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
#  endif

  static const EnumPropertyItem tiff_codec_items[] = {
      {R_IMF_TIFF_CODEC_NONE, "NONE", 0, "None", ""},
      {R_IMF_TIFF_CODEC_DEFLATE, "DEFLATE", 0, "Deflate", ""},
      {R_IMF_TIFF_CODEC_LZW, "LZW", 0, "LZW", ""},
      {R_IMF_TIFF_CODEC_PACKBITS, "PACKBITS", 0, "Pack Bits", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem color_management_items[] = {
      {R_IMF_COLOR_MANAGEMENT_FOLLOW_SCENE, "FOLLOW_SCENE", 0, "Follow Scene", ""},
      {R_IMF_COLOR_MANAGEMENT_OVERRIDE, "OVERRIDE", 0, "Override", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_image_format_stereo3d_format(brna);

  srna = RNA_def_struct(brna, "ImageFormatSettings", nullptr);
  RNA_def_struct_sdna(srna, "ImageFormatData");
  RNA_def_struct_nested(brna, srna, "Scene");
  RNA_def_struct_path_func(srna, "rna_ImageFormatSettings_path");
  RNA_def_struct_ui_text(srna, "Image Format", "Settings for image formats");

  prop = RNA_def_property(srna, "media_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "media_type");
  RNA_def_property_enum_items(prop, rna_enum_media_type_all_items);
  RNA_def_property_enum_funcs(prop,
                              nullptr,
                              "rna_ImageFormatSettings_media_type_set",
                              "rna_ImageFormatSettings_media_type_itemf");
  RNA_def_property_ui_text(prop, "Media Type", "The type of media to save");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "file_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "imtype");
  RNA_def_property_enum_items(prop, rna_enum_image_type_all_items);
  RNA_def_property_enum_funcs(prop,
                              nullptr,
                              "rna_ImageFormatSettings_file_format_set",
                              "rna_ImageFormatSettings_file_format_itemf");
  RNA_def_property_ui_text(prop, "File Format", "File format to save the rendered images as");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "color_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "planes");
  RNA_def_property_enum_items(prop, rna_enum_image_color_mode_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_ImageFormatSettings_color_mode_itemf");
  RNA_def_property_ui_text(
      prop,
      "Color Mode",
      "Choose BW for saving grayscale images, RGB for saving red, green and blue channels, "
      "and RGBA for saving red, green, blue and alpha channels");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "color_depth", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "depth");
  RNA_def_property_enum_items(prop, rna_enum_image_color_depth_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_ImageFormatSettings_color_depth_itemf");
  RNA_def_property_ui_text(prop, "Color Depth", "Bit depth per channel");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* was 'file_quality' */
  prop = RNA_def_property(srna, "quality", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, nullptr, "quality");
  RNA_def_property_range(prop, 0, 100); /* 0 is needed for compression. */
  RNA_def_property_ui_text(
      prop, "Quality", "Quality for image formats that support lossy compression");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* was shared with file_quality */
  prop = RNA_def_property(srna, "compression", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, nullptr, "compress");
  RNA_def_property_range(prop, 0, 100); /* 0 is needed for compression. */
  RNA_def_property_ui_text(prop,
                           "Compression",
                           "Amount of time to determine best compression: "
                           "0 = no compression with fast file output, "
                           "100 = maximum lossless compression with slow file output");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", R_IMF_FLAG_PREVIEW_JPG);
  RNA_def_property_ui_text(
      prop, "Preview", "When rendering animations, save JPG preview images in same directory");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* format specific */

#  ifdef WITH_IMAGE_OPENEXR
  /* OpenEXR */

  prop = RNA_def_property(srna, "exr_codec", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "exr_codec");
  RNA_def_property_enum_items(prop, rna_enum_exr_codec_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_ImageFormatSettings_exr_codec_itemf");
  RNA_def_property_ui_text(prop, "Codec", "Compression codec settings for OpenEXR");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
#  endif

#  ifdef WITH_IMAGE_OPENJPEG
  /* JPEG 2000 */
  prop = RNA_def_property(srna, "use_jpeg2k_ycc", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "jp2_flag", R_IMF_JP2_FLAG_YCC);
  RNA_def_property_ui_text(
      prop, "YCC", "Save luminance-chrominance-chrominance channels instead of RGB colors");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_jpeg2k_cinema_preset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "jp2_flag", R_IMF_JP2_FLAG_CINE_PRESET);
  RNA_def_property_ui_text(prop, "Cinema", "Use OpenJPEG Cinema Preset");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_jpeg2k_cinema_48", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "jp2_flag", R_IMF_JP2_FLAG_CINE_48);
  RNA_def_property_ui_text(prop, "Cinema (48)", "Use OpenJPEG Cinema Preset (48fps)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "jpeg2k_codec", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "jp2_codec");
  RNA_def_property_enum_items(prop, jp2_codec_items);
  RNA_def_property_ui_text(prop, "Codec", "Codec settings for JPEG 2000");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
#  endif

  /* TIFF */
  prop = RNA_def_property(srna, "tiff_codec", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "tiff_codec");
  RNA_def_property_enum_items(prop, tiff_codec_items);
  RNA_def_property_ui_text(prop, "Compression", "Compression mode for TIFF");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* Cineon and DPX */

  prop = RNA_def_property(srna, "use_cineon_log", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cineon_flag", R_IMF_CINEON_FLAG_LOG);
  RNA_def_property_ui_text(prop, "Log", "Convert to logarithmic color space");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "cineon_black", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "cineon_black");
  RNA_def_property_range(prop, 0, 1024);
  RNA_def_property_ui_text(prop, "Black", "Log conversion reference blackpoint");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "cineon_white", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "cineon_white");
  RNA_def_property_range(prop, 0, 1024);
  RNA_def_property_ui_text(prop, "White", "Log conversion reference whitepoint");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "cineon_gamma", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "cineon_gamma");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(prop, "Gamma", "Log conversion gamma");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* multiview */
  prop = RNA_def_property(srna, "views_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "views_format");
  RNA_def_property_enum_items(prop, rna_enum_views_format_multiview_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_ImageFormatSettings_views_format_itemf");
  RNA_def_property_ui_text(prop, "Views Format", "Format of multiview media");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "stereo_3d_format", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "stereo3d_format");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "Stereo3dFormat");
  RNA_def_property_ui_text(prop, "Stereo 3D Format", "Settings for stereo 3D");

  /* color management */
  prop = RNA_def_property(srna, "color_management", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, color_management_items);
  RNA_def_property_ui_text(
      prop, "Color Management", "Which color management settings to use for file saving");
  RNA_def_property_enum_funcs(
      prop, nullptr, "rna_ImageFormatSettings_color_management_set", nullptr);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "view_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ColorManagedViewSettings");
  RNA_def_property_ui_text(
      prop, "View Settings", "Color management settings applied on image before saving");

  prop = RNA_def_property(srna, "display_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ColorManagedDisplaySettings");
  RNA_def_property_ui_text(
      prop, "Display Settings", "Settings of device saved image would be displayed on");

  prop = RNA_def_property(srna, "linear_colorspace_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ColorManagedInputColorspaceSettings");
  RNA_def_property_ui_text(prop, "Color Space Settings", "Output color space settings");

  prop = RNA_def_property(srna, "has_linear_colorspace", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_ImageFormatSettings_has_linear_colorspace_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Has Linear Color Space", "File format expects linear color space");
}

static void rna_def_scene_ffmpeg_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

#  ifdef WITH_FFMPEG
  /* Container types */
  static const EnumPropertyItem ffmpeg_format_items[] = {
      {FFMPEG_MPEG4, "MPEG4", 0, "MPEG-4", ""},
      {FFMPEG_MKV, "MKV", 0, "Matroska", ""},
      {FFMPEG_WEBM, "WEBM", 0, "WebM", ""},
      /* Legacy containers. */
      RNA_ENUM_ITEM_SEPR,
      {FFMPEG_AVI, "AVI", 0, "AVI", ""},
      {FFMPEG_DV, "DV", 0, "DV", ""},
      {FFMPEG_FLV, "FLASH", 0, "Flash", ""},
      {FFMPEG_MPEG1, "MPEG1", 0, "MPEG-1", ""},
      {FFMPEG_MPEG2, "MPEG2", 0, "MPEG-2", ""},
      {FFMPEG_OGG, "OGG", 0, "Ogg", ""},
      {FFMPEG_MOV, "QUICKTIME", 0, "QuickTime", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem ffmpeg_codec_items[] = {
      {FFMPEG_CODEC_ID_NONE,
       "NONE",
       0,
       "No Video",
       "Disables video output, for audio-only renders"},
      {FFMPEG_CODEC_ID_AV1, "AV1", 0, "AV1", ""},
      {FFMPEG_CODEC_ID_H264, "H264", 0, "H.264", ""},
      {FFMPEG_CODEC_ID_H265, "H265", 0, "H.265 / HEVC", ""},
      {FFMPEG_CODEC_ID_VP9, "WEBM", 0, "WebM / VP9", ""},
      /* Legacy / rare codecs. */
      RNA_ENUM_ITEM_SEPR,
      {FFMPEG_CODEC_ID_DNXHD, "DNXHD", 0, "DNxHD", ""},
      {FFMPEG_CODEC_ID_DVVIDEO, "DV", 0, "DV", ""},
      {FFMPEG_CODEC_ID_FFV1, "FFV1", 0, "FFmpeg video codec #1", ""},
      {FFMPEG_CODEC_ID_FLV1, "FLASH", 0, "Flash Video", ""},
      {FFMPEG_CODEC_ID_HUFFYUV, "HUFFYUV", 0, "HuffYUV", ""},
      {FFMPEG_CODEC_ID_MPEG1VIDEO, "MPEG1", 0, "MPEG-1", ""},
      {FFMPEG_CODEC_ID_MPEG2VIDEO, "MPEG2", 0, "MPEG-2", ""},
      {FFMPEG_CODEC_ID_MPEG4, "MPEG4", 0, "MPEG-4 (divx)", ""},
      {FFMPEG_CODEC_ID_PNG, "PNG", 0, "PNG", ""},
      {FFMPEG_CODEC_ID_PRORES, "PRORES", 0, "ProRes", ""},
      {FFMPEG_CODEC_ID_QTRLE, "QTRLE", 0, "QuickTime Animation", ""},
      {FFMPEG_CODEC_ID_THEORA, "THEORA", 0, "Theora", ""},
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem ffmpeg_prores_profiles_items[] = {
      {FFM_PRORES_PROFILE_422_PROXY, "422_PROXY", 0, "ProRes 422 Proxy", ""},
      {FFM_PRORES_PROFILE_422_LT, "422_LT", 0, "ProRes 422 LT", ""},
      {FFM_PRORES_PROFILE_422_STD, "422_STD", 0, "ProRes 422", ""},
      {FFM_PRORES_PROFILE_422_HQ, "422_HQ", 0, "ProRes 422 HQ", ""},
      {FFM_PRORES_PROFILE_4444, "4444", 0, "ProRes 4444", ""},
      {FFM_PRORES_PROFILE_4444_XQ, "4444_XQ", 0, "ProRes 4444 XQ", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem ffmpeg_crf_items[] = {
      {FFM_CRF_NONE,
       "NONE",
       0,
       "Constant Bitrate",
       "Configure constant bit rate, rather than constant output quality"},
      {FFM_CRF_LOSSLESS, "LOSSLESS", 0, "Lossless", ""},
      {FFM_CRF_PERC_LOSSLESS, "PERC_LOSSLESS", 0, "Perceptually Lossless", ""},
      {FFM_CRF_HIGH, "HIGH", 0, "High Quality", ""},
      {FFM_CRF_MEDIUM, "MEDIUM", 0, "Medium Quality", ""},
      {FFM_CRF_LOW, "LOW", 0, "Low Quality", ""},
      {FFM_CRF_VERYLOW, "VERYLOW", 0, "Very Low Quality", ""},
      {FFM_CRF_LOWEST, "LOWEST", 0, "Lowest Quality", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem ffmpeg_hdr_items[] = {
      {FFM_VIDEO_HDR_NONE, "NONE", 0, "None", "No High Dynamic Range"},
      {FFM_VIDEO_HDR_REC2100_PQ,
       "REQ2100_PQ",
       0,
       "Rec.2100 PQ",
       "Rec.2100 color space with Perceptual Quantizer HDR encoding"},
      {FFM_VIDEO_HDR_REC2100_HLG,
       "REQ2100_HLG",
       0,
       "Rec.2100 HLG",
       "Rec.2100 color space with Hybrid-Log Gamma HDR encoding"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem ffmpeg_audio_codec_items[] = {
      {FFMPEG_CODEC_ID_NONE,
       "NONE",
       0,
       "No Audio",
       "Disables audio output, for video-only renders"},
      {FFMPEG_CODEC_ID_AAC, "AAC", 0, "AAC", ""},
      {FFMPEG_CODEC_ID_AC3, "AC3", 0, "AC3", ""},
      {FFMPEG_CODEC_ID_FLAC, "FLAC", 0, "FLAC", ""},
      {FFMPEG_CODEC_ID_MP2, "MP2", 0, "MP2", ""},
      {FFMPEG_CODEC_ID_MP3, "MP3", 0, "MP3", ""},
      {FFMPEG_CODEC_ID_OPUS, "OPUS", 0, "Opus", ""},
      {FFMPEG_CODEC_ID_PCM_S16LE, "PCM", 0, "PCM", ""},
      {FFMPEG_CODEC_ID_VORBIS, "VORBIS", 0, "Vorbis", ""},
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "FFmpegSettings", nullptr);
  RNA_def_struct_sdna(srna, "FFMpegCodecData");
  RNA_def_struct_path_func(srna, "rna_FFmpegSettings_path");
  RNA_def_struct_ui_text(srna, "FFmpeg Settings", "FFmpeg related settings for the scene");

#  ifdef WITH_FFMPEG
  prop = RNA_def_property(srna, "format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "type");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_format_items);
  RNA_def_property_enum_default(prop, FFMPEG_MKV);
  RNA_def_property_ui_text(prop, "Container", "Output file container");

  prop = RNA_def_property(srna, "codec", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "codec");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_codec_items);
  RNA_def_property_enum_default(prop, FFMPEG_CODEC_ID_H264);
  RNA_def_property_ui_text(prop, "Video Codec", "FFmpeg codec to use for video output");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_FFmpegSettings_codec_update");

  prop = RNA_def_property(srna, "video_bitrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "video_bitrate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Bitrate", "Video bitrate (kbit/s)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "video_hdr", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "video_hdr");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_hdr_items);
  RNA_def_property_enum_default(prop, FFM_VIDEO_HDR_NONE);
  RNA_def_property_ui_text(prop, "HDR", "High Dynamic Range options");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "minrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "rc_min_rate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Min Rate", "Rate control: min rate (kbit/s)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "maxrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "rc_max_rate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Max Rate", "Rate control: max rate (kbit/s)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "muxrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "mux_rate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 100000000);
  RNA_def_property_ui_text(prop, "Mux Rate", "Mux rate (bits/second)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "gopsize", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "gop_size");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 500);
  RNA_def_property_int_default(prop, 25);
  RNA_def_property_ui_text(prop,
                           "Keyframe Interval",
                           "Distance between key frames, also known as GOP size; "
                           "influences file size and seekability");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "max_b_frames", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "max_b_frames");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 16);
  RNA_def_property_ui_text(
      prop,
      "Max B-Frames",
      "Maximum number of B-frames between non-B-frames; influences file size and seekability");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_max_b_frames", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FFMPEG_USE_MAX_B_FRAMES);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Use Max B-Frames", "Set a maximum number of B-frames");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "buffersize", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "rc_buffer_size");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 2000);
  RNA_def_property_ui_text(prop, "Buffersize", "Rate control: buffer size (kb)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "packetsize", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "mux_packet_size");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 16384);
  RNA_def_property_ui_text(prop, "Mux Packet Size", "Mux packet size (byte)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "constant_rate_factor", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "constant_rate_factor");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_crf_items);
  RNA_def_property_enum_default(prop, FFM_CRF_MEDIUM);
  RNA_def_property_ui_text(
      prop,
      "Output Quality",
      "Constant Rate Factor (CRF); tradeoff between video quality and file size");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "ffmpeg_preset", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "ffmpeg_preset");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_preset_items);
  RNA_def_property_enum_default(prop, FFM_PRESET_GOOD);
  RNA_def_property_ui_text(
      prop, "Encoding Speed", "Tradeoff between encoding speed and compression ratio");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "ffmpeg_prores_profile", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "ffmpeg_prores_profile");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_prores_profiles_items);
  RNA_def_property_enum_default(prop, FFM_PRORES_PROFILE_422_STD);
  RNA_def_property_ui_text(prop, "Profile", "ProRes Profile");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_autosplit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FFMPEG_AUTOSPLIT_OUTPUT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Autosplit Output", "Autosplit output at 2GB boundary");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_lossless_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FFMPEG_LOSSLESS_OUTPUT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_FFmpegSettings_lossless_output_set");
  RNA_def_property_ui_text(prop, "Lossless Output", "Use lossless output for video streams");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* FFMPEG Audio. */
  prop = RNA_def_property(srna, "audio_codec", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "audio_codec");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ffmpeg_audio_codec_items);
  RNA_def_property_ui_text(prop, "Audio Codec", "FFmpeg audio codec to use");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "audio_bitrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "audio_bitrate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 32, 384);
  RNA_def_property_ui_text(prop, "Bitrate", "Audio bitrate (kb/s)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "audio_volume", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "audio_volume");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Volume", "Audio volume");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
#  endif

  /* the following two "ffmpeg" settings are general audio settings */
  prop = RNA_def_property(srna, "audio_mixrate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "audio_mixrate");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 8000, 192000);
  RNA_def_property_ui_text(prop, "Sample Rate", "Audio sample rate (samples/s)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "audio_channels", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "audio_channels");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, audio_channel_items);
  RNA_def_property_ui_text(prop, "Audio Channels", "Audio channel count");
}

static void rna_def_scene_render_data(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Bake */
  static const EnumPropertyItem bake_mode_items[] = {
      //{RE_BAKE_AO, "AO", 0, "Ambient Occlusion", "Bake ambient occlusion"},
      {RE_BAKE_NORMALS, "NORMALS", 0, "Normals", "Bake normals"},
      {RE_BAKE_DISPLACEMENT, "DISPLACEMENT", 0, "Displacement", "Bake displacement"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem bake_margin_type_items[] = {
      {R_BAKE_ADJACENT_FACES,
       "ADJACENT_FACES",
       0,
       "Adjacent Faces",
       "Use pixels from adjacent faces across UV seams"},
      {R_BAKE_EXTEND, "EXTEND", 0, "Extend", "Extend border pixels outwards"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem pixel_size_items[] = {
      {0, "AUTO", 0, "Automatic", "Automatic pixel size, depends on the user interface scale"},
      {1, "1", 0, "1" BLI_STR_UTF8_MULTIPLICATION_SIGN, "Render at full resolution"},
      {2, "2", 0, "2" BLI_STR_UTF8_MULTIPLICATION_SIGN, "Render at 50% resolution"},
      {4, "4", 0, "4" BLI_STR_UTF8_MULTIPLICATION_SIGN, "Render at 25% resolution"},
      {8, "8", 0, "8" BLI_STR_UTF8_MULTIPLICATION_SIGN, "Render at 12.5% resolution"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem threads_mode_items[] = {
      {0,
       "AUTO",
       0,
       "Auto-Detect",
       "Automatically determine the number of threads, based on CPUs"},
      {R_FIXED_THREADS, "FIXED", 0, "Fixed", "Manually determine the number of threads"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem engine_items[] = {
      {0, "BLENDER_EEVEE", 0, "EEVEE", ""},
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem motion_blur_position_items[] = {
      {SCE_MB_START, "START", 0, "Start on Frame", "The shutter opens at the current frame"},
      {SCE_MB_CENTER,
       "CENTER",
       0,
       "Center on Frame",
       "The shutter is open during the current frame"},
      {SCE_MB_END, "END", 0, "End on Frame", "The shutter closes at the current frame"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem hair_shape_type_items[] = {
      {SCE_HAIR_SHAPE_STRAND, "STRAND", 0, "Strand", ""},
      {SCE_HAIR_SHAPE_STRIP, "STRIP", 0, "Strip", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem meta_input_items[] = {
      {0, "SCENE", 0, "Scene", "Use metadata from the current scene"},
      {R_STAMP_STRIPMETA,
       "STRIPS",
       0,
       "Sequencer Strips",
       "Use metadata from the strips in the sequencer"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem compositor_device_items[] = {
      {SCE_COMPOSITOR_DEVICE_CPU, "CPU", 0, "CPU", ""},
      {SCE_COMPOSITOR_DEVICE_GPU, "GPU", 0, "GPU", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem compositor_precision_items[] = {
      {SCE_COMPOSITOR_PRECISION_AUTO,
       "AUTO",
       0,
       "Auto",
       "Full precision for final renders, half precision otherwise"},
      {SCE_COMPOSITOR_PRECISION_FULL, "FULL", 0, "Full", "Full precision"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem compositor_denoise_device_items[] = {
      {SCE_COMPOSITOR_DENOISE_DEVICE_AUTO,
       "AUTO",
       0,
       "Auto",
       "Use the same device used by the compositor to process the denoise node"},
      {SCE_COMPOSITOR_DENOISE_DEVICE_CPU,
       "CPU",
       0,
       "CPU",
       "Use the CPU to process the denoise node"},
      {SCE_COMPOSITOR_DENOISE_DEVICE_GPU,
       "GPU",
       0,
       "GPU",
       "Use the GPU to process the denoise node if available, otherwise fallback to CPU"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem compositor_denoise_quality_items[] = {
      {SCE_COMPOSITOR_DENOISE_HIGH, "HIGH", 0, "High", "High quality"},
      {SCE_COMPOSITOR_DENOISE_BALANCED,
       "BALANCED",
       0,
       "Balanced",
       "Balanced between performance and quality"},
      {SCE_COMPOSITOR_DENOISE_FAST, "FAST", 0, "Fast", "High perfomance"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  rna_def_scene_ffmpeg_settings(brna);

  srna = RNA_def_struct(brna, "RenderSettings", nullptr);
  RNA_def_struct_sdna(srna, "RenderData");
  RNA_def_struct_nested(brna, srna, "Scene");
  RNA_def_struct_path_func(srna, "rna_RenderSettings_path");
  RNA_def_struct_ui_text(srna, "Render Data", "Rendering settings for a Scene data-block");

  /* Render Data */
  prop = RNA_def_property(srna, "image_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "im_format");
  RNA_def_property_struct_type(prop, "ImageFormatSettings");
  RNA_def_property_ui_text(prop, "Image Format", "");

  prop = RNA_def_property(srna, "resolution_x", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "xsch");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 4, 65536);
  RNA_def_property_ui_text(
      prop, "Resolution X", "Number of horizontal pixels in the rendered image");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");

  prop = RNA_def_property(srna, "resolution_y", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "ysch");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 4, 65536);
  RNA_def_property_ui_text(
      prop, "Resolution Y", "Number of vertical pixels in the rendered image");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");

  prop = RNA_def_property(srna, "resolution_percentage", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, nullptr, "size");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, SHRT_MAX);
  RNA_def_property_ui_range(prop, 1, 100, 10, 1);
  RNA_def_property_ui_text(prop, "Resolution Scale", "Percentage scale for render resolution");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneSequencer_update");

  prop = RNA_def_property(srna, "preview_pixel_size", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "preview_pixel_size");
  RNA_def_property_enum_items(prop, pixel_size_items);
  RNA_def_property_ui_text(prop, "Pixel Size", "Pixel size for viewport rendering");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "pixel_aspect_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "xasp");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1.0f, 200.0f);
  RNA_def_property_ui_text(prop,
                           "Pixel Aspect X",
                           "Horizontal aspect ratio - for anamorphic or non-square pixel output");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");

  prop = RNA_def_property(srna, "pixel_aspect_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "yasp");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1.0f, 200.0f);
  RNA_def_property_ui_text(
      prop, "Pixel Aspect Y", "Vertical aspect ratio - for anamorphic or non-square pixel output");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");

  /* Pixels per meters (also DPI). */
  prop = RNA_def_property(srna, "ppm_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "ppm_factor");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1e-5f, 1e6f);
  RNA_def_property_ui_range(prop, 0.0001f, 10000.0f, 2, 2);
  RNA_def_property_ui_text(prop,
                           "PPM Factor",
                           "The pixel density meta-data written to supported image formats. "
                           "This value is multiplied by the PPM-base which defines the unit "
                           "(typically inches or meters)");

  prop = RNA_def_property(srna, "ppm_base", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "ppm_base");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1e-5f, 1e6f);
  /* Important to show at least 3 decimal points because multiple presets set this to 1.001. */
  RNA_def_property_ui_range(prop, 0.0001f, 10000.0f, 2, 4);
  RNA_def_property_ui_text(prop, "PPM Base", "The base unit for pixels per meter.");

  prop = RNA_def_property(srna, "ffmpeg", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "FFmpegSettings");
  RNA_def_property_pointer_sdna(prop, nullptr, "ffcodecdata");
  RNA_def_property_flag(prop, PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "FFmpeg Settings", "FFmpeg related settings for the scene");

  prop = RNA_def_property(srna, "fps", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "frs_sec");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, SHRT_MAX);
  RNA_def_property_ui_range(prop, 1, 240, 1, -1);
  RNA_def_property_ui_text(prop, "FPS", "Framerate, expressed in frames per second");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_fps_update");

  prop = RNA_def_property(srna, "fps_base", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "frs_sec_base");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1e-5f, 1e6f);
  /* Important to show at least 3 decimal points because multiple presets set this to 1.001. */
  RNA_def_property_ui_range(prop, 0.1f, 120.0f, 2, 3);
  RNA_def_property_ui_text(prop, "FPS Base", "Framerate base");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_fps_update");

  /* frame mapping */
  prop = RNA_def_property(srna, "frame_map_old", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "framapto");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, 900);
  RNA_def_property_ui_text(prop, "Frame Map Old", "Old mapping value in frames");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_framelen_update");

  prop = RNA_def_property(srna, "frame_map_new", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "images");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, 900);
  RNA_def_property_ui_text(prop, "Frame Map New", "How many frames the Map Old will last");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_framelen_update");

  prop = RNA_def_property(srna, "dither_intensity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "dither_intensity");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 2.0, 0.1, 2);
  RNA_def_property_ui_text(
      prop,
      "Dither Intensity",
      "Amount of dithering noise added to the rendered image to break up banding");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "filter_size", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_float_sdna(prop, nullptr, "gauss");
  RNA_def_property_range(prop, 0.0f, 500.0f);
  RNA_def_property_ui_range(prop, 0.01f, 10.0f, 1, 2);
  RNA_def_property_ui_text(
      prop, "Filter Size", "Width over which the reconstruction filter combines samples");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "film_transparent", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "alphamode", R_ALPHAPREMUL);
  RNA_def_property_ui_text(
      prop,
      "Transparent",
      "World background is transparent, for compositing the render over another background");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_render_update");

  prop = RNA_def_property(srna, "use_freestyle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", R_EDGE_FRS);
  RNA_def_property_ui_text(prop, "Use Freestyle", "Draw stylized strokes using Freestyle");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_use_freestyle_update");

  /* threads */
  prop = RNA_def_property(srna, "threads", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "threads");
  RNA_def_property_range(prop, 1, BLENDER_MAX_THREADS);
  RNA_def_property_int_funcs(prop, "rna_RenderSettings_threads_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop,
                           "Threads",
                           "Maximum number of CPU cores to use simultaneously while rendering "
                           "(for multi-core/CPU systems)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "threads_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, threads_mode_items);
  RNA_def_property_enum_funcs(prop, "rna_RenderSettings_threads_mode_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Threads Mode", "Determine the amount of render threads used");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* motion blur */
  prop = RNA_def_property(srna, "use_motion_blur", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", R_MBLUR);
  RNA_def_property_ui_text(prop, "Motion Blur", "Use multi-sampled 3D scene motion blur");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_render_update");

  prop = RNA_def_property(srna, "motion_blur_shutter", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.01f, 1.0f, 1, 2);
  RNA_def_property_ui_text(prop, "Shutter", "Time taken in frames between shutter open and close");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_render_update");

  prop = RNA_def_property(srna, "motion_blur_position", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, motion_blur_position_items);
  RNA_def_property_ui_text(prop,
                           "Motion Blur Position",
                           "Offset for the shutter's time interval, "
                           "allows to change the motion blur trails");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "motion_blur_shutter_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "mblur_shutter_curve");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(
      prop, "Shutter Curve", "Curve defining the shutter's openness over time");

  /* Hairs */
  prop = RNA_def_property(srna, "hair_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, hair_shape_type_items);
  RNA_def_property_ui_text(prop, "Curves Shape Type", "Curves shape type");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVES);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_render_update");

  prop = RNA_def_property(srna, "hair_subdiv", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 3);
  RNA_def_property_ui_text(
      prop, "Additional Subdivision", "Additional subdivision along the curves");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_render_update");

  /* Performance */
  prop = RNA_def_property(srna, "use_high_quality_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "perf_flag", SCE_PERF_HQ_NORMALS);
  RNA_def_property_ui_text(prop,
                           "High Quality Normals",
                           "Use high quality tangent space at the cost of lower performance");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_mesh_quality_update");

  /* border */
  prop = RNA_def_property(srna, "use_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", R_BORDER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Render Region", "Render a user-defined render region, within the frame size");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "border_min_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "border.xmin");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Region Minimum X", "Minimum X value for the render region");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "border_min_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "border.ymin");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Region Minimum Y", "Minimum Y value for the render region");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "border_max_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "border.xmax");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Region Maximum X", "Maximum X value for the render region");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "border_max_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "border.ymax");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Region Maximum Y", "Maximum Y value for the render region");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_crop_to_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", R_CROP);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Crop to Render Region", "Crop the rendered frame to the defined render region size");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_placeholder", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", R_TOUCH);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Placeholders",
      "Create empty placeholder files while rendering frames (similar to Unix 'touch')");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_overwrite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "mode", R_NO_OVERWRITE);
  RNA_def_property_ui_text(prop, "Overwrite", "Overwrite existing files while rendering");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_compositing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "scemode", R_DOCOMP);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Compositing",
                           "Process the render result through the compositing pipeline, "
                           "if a compositing node group is assigned to the scene");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_sequencer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "scemode", R_DOSEQ);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Sequencer",
                           "Process the render (and composited) result through the video sequence "
                           "editor pipeline, if sequencer strips exist");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_file_extension", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "scemode", R_EXTENSION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "File Extensions",
      "Add the file format extensions to the rendered file name (eg: filename + .jpg)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

#  if 0 /* moved */
  prop = RNA_def_property(srna, "file_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "imtype");
  RNA_def_property_enum_items(prop, rna_enum_image_type_all_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_RenderSettings_file_format_set", nullptr);
  RNA_def_property_ui_text(prop, "File Format", "File format to save the rendered images as");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
#  endif

  prop = RNA_def_property(srna, "file_extension", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_SceneRender_file_ext_get", "rna_SceneRender_file_ext_length", nullptr);
  RNA_def_property_ui_text(prop, "Extension", "The file extension used for saving renders");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_movie_format", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_is_movie_format_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Movie Format", "When true the format is a movie");

  prop = RNA_def_property(srna, "use_lock_interface", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_lock_interface", 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, true);
  RNA_def_property_ui_text(
      prop,
      "Lock Interface",
      "Lock interface during rendering in favor of giving more memory to the renderer");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, nullptr, "pic");
  RNA_def_property_flag(
      prop, PROP_PATH_OUTPUT | PROP_PATH_SUPPORTS_BLEND_RELATIVE | PROP_PATH_SUPPORTS_TEMPLATES);
  RNA_def_property_path_template_type(prop, PROP_VARIABLES_RENDER_OUTPUT);
  RNA_def_property_ui_text(prop,
                           "Output Path",
                           "Directory/name to save animations, # characters define the position "
                           "and padding of frame numbers");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* Render result EXR cache. */
  prop = RNA_def_property(srna, "use_render_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "scemode", R_EXR_CACHE_FILE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Cache Result",
                           "Save render cache to EXR files (useful for heavy compositing, "
                           "Note: affects indirectly rendered scenes)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* Bake */

  prop = RNA_def_property(srna, "bake_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "bake_mode");
  RNA_def_property_enum_items(prop, bake_mode_items);
  RNA_def_property_ui_text(prop, "Bake Type", "Choose shading information to bake into the image");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_bake_selected_to_active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bake_flag", R_BAKE_TO_ACTIVE);
  RNA_def_property_ui_text(prop,
                           "Selected to Active",
                           "Bake shading on the surface of selected objects to the active object");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_bake_clear", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bake_flag", R_BAKE_CLEAR);
  RNA_def_property_ui_text(prop, "Clear", "Clear Images before baking");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "bake_margin", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "bake_margin");
  RNA_def_property_range(prop, 0, 64);
  RNA_def_property_ui_text(prop, "Margin", "Extends the baked result as a post process filter");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "bake_margin_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "bake_margin_type");
  RNA_def_property_enum_items(prop, bake_margin_type_items);
  RNA_def_property_ui_text(prop, "Margin Type", "Algorithm to generate the margin");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "bake_bias", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "bake_biasdist");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_text(
      prop, "Bias", "Bias towards faces further away from the object (in Blender units)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_bake_multires", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bake_flag", R_BAKE_MULTIRES);
  RNA_def_property_ui_text(prop, "Bake from Multires", "Bake directly from multires object");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_bake_lores_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bake_flag", R_BAKE_LORES_MESH);
  RNA_def_property_ui_text(
      prop, "Low Resolution Mesh", "Calculate heights against unsubdivided low resolution mesh");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "bake_samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "bake_samples");
  RNA_def_property_range(prop, 64, 1024);
  RNA_def_property_ui_text(
      prop, "Samples", "Number of samples used for ambient occlusion baking from multires");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_bake_user_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bake_flag", R_BAKE_USERSCALE);
  RNA_def_property_ui_text(prop, "User Scale", "Use a user scale for the derivative map");

  prop = RNA_def_property(srna, "bake_user_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "bake_user_scale");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_text(prop,
                           "Scale",
                           "Instead of automatically normalizing to the range 0 to 1, "
                           "apply a user scale to the derivative map");

  /* stamp */

  prop = RNA_def_property(srna, "use_stamp_time", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_TIME);
  RNA_def_property_ui_text(
      prop, "Stamp Time", "Include the rendered frame timecode as HH:MM:SS.FF in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_date", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_DATE);
  RNA_def_property_ui_text(prop, "Stamp Date", "Include the current date in image/video metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_frame", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_FRAME);
  RNA_def_property_ui_text(prop, "Stamp Frame", "Include the frame number in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_frame_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_FRAME_RANGE);
  RNA_def_property_ui_text(
      prop, "Stamp Frame", "Include the rendered frame range in image/video metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_CAMERA);
  RNA_def_property_ui_text(
      prop, "Stamp Camera", "Include the name of the active camera in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_lens", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_CAMERALENS);
  RNA_def_property_ui_text(
      prop, "Stamp Lens", "Include the active camera's lens in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_scene", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_SCENE);
  RNA_def_property_ui_text(
      prop, "Stamp Scene", "Include the name of the active scene in image/video metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_note", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_NOTE);
  RNA_def_property_ui_text(prop, "Stamp Note", "Include a custom note in image/video metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_marker", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_MARKER);
  RNA_def_property_ui_text(
      prop, "Stamp Marker", "Include the name of the last marker in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_filename", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_FILENAME);
  RNA_def_property_ui_text(
      prop, "Stamp Filename", "Include the .blend filename in image/video metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_sequencer_strip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_SEQSTRIP);
  RNA_def_property_ui_text(prop,
                           "Stamp Sequence Strip",
                           "Include the name of the foreground sequence strip in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_render_time", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_RENDERTIME);
  RNA_def_property_ui_text(prop, "Stamp Render Time", "Include the render time in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "stamp_note_text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "stamp_udata");
  RNA_def_property_ui_text(prop, "Stamp Note Text", "Custom text to appear in the stamp note");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_DRAW);
  RNA_def_property_ui_text(
      prop, "Stamp Output", "Render the stamp info text in the rendered image");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_labels", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "stamp", R_STAMP_HIDE_LABELS);
  RNA_def_property_ui_text(
      prop, "Stamp Labels", "Display stamp labels (\"Camera\" in front of camera name, etc.)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "metadata_input", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "stamp");
  RNA_def_property_enum_items(prop, meta_input_items);
  RNA_def_property_ui_text(prop, "Metadata Input", "Where to take the metadata from");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_memory", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_MEMORY);
  RNA_def_property_ui_text(
      prop, "Stamp Peak Memory", "Include the peak memory usage in image metadata");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_stamp_hostname", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stamp", R_STAMP_HOSTNAME);
  RNA_def_property_ui_text(
      prop, "Stamp Hostname", "Include the hostname of the machine that rendered the frame");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "stamp_font_size", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "stamp_font_id");
  RNA_def_property_range(prop, 8, 64);
  RNA_def_property_ui_text(prop, "Font Size", "Size of the font used when rendering stamp text");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "stamp_foreground", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "fg_stamp");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Text Color", "Color to use for stamp text");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "stamp_background", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "bg_stamp");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Background", "Color to use behind stamp text");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* sequencer draw options */

  prop = RNA_def_property(srna, "sequencer_gl_preview", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "seq_prev_type");
  RNA_def_property_enum_items(prop, rna_enum_shading_type_items);
  RNA_def_property_ui_text(
      prop, "Sequencer Preview Shading", "Display method used in the sequencer view");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SceneSequencer_update");

  prop = RNA_def_property(srna, "use_sequencer_override_scene_strip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "seq_flag", R_SEQ_OVERRIDE_SCENE_SETTINGS);
  RNA_def_property_ui_text(prop,
                           "Override Scene Settings",
                           "Use workbench render settings from the sequencer scene, instead of "
                           "each individual scene used in the strip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SceneSequencer_update");

  prop = RNA_def_property(srna, "use_single_layer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "scemode", R_SINGLE_LAYER);
  RNA_def_property_ui_text(prop,
                           "Render Single Layer",
                           "Only render the active layer. Only affects rendering from the "
                           "interface, ignored for rendering from command line.");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* views (stereoscopy et al) */
  prop = RNA_def_property(srna, "views", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "SceneRenderView");
  RNA_def_property_ui_text(prop, "Render Views", "");
  rna_def_render_views(brna, prop);

  prop = RNA_def_property(srna, "stereo_views", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "views", nullptr);
  RNA_def_property_collection_funcs(prop,
                                    "rna_RenderSettings_stereoViews_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "SceneRenderView");
  RNA_def_property_ui_text(prop, "Render Views", "");

  prop = RNA_def_property(srna, "use_multiview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "scemode", R_MULTIVIEW);
  RNA_def_property_ui_text(prop, "Multiple Views", "Use multiple views in the scene");
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "views_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, views_format_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Setup Stereo Mode", "");
  RNA_def_property_enum_funcs(prop, nullptr, "rna_RenderSettings_views_format_set", nullptr);
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

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
  RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_multiple_engines_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Multiple Engines", "More than one rendering engine is available");

  prop = RNA_def_property(srna, "use_spherical_stereo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_use_spherical_stereo_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Use Spherical Stereo", "Active render engine supports spherical stereo rendering");

  /* simplify */
  prop = RNA_def_property(srna, "use_simplify", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", R_SIMPLIFY);
  RNA_def_property_ui_text(
      prop, "Use Simplify", "Enable simplification of scene for quicker preview renders");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_Scene_use_simplify_update");

  prop = RNA_def_property(srna, "simplify_subdivision", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "simplify_subsurf");
  RNA_def_property_ui_range(prop, 0, 6, 1, -1);
  RNA_def_property_ui_text(prop, "Simplify Subdivision", "Global maximum subdivision level");
  RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

  prop = RNA_def_property(srna, "simplify_child_particles", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "simplify_particles");
  RNA_def_property_ui_text(prop, "Simplify Child Particles", "Global child particles percentage");
  RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

  prop = RNA_def_property(srna, "simplify_subdivision_render", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "simplify_subsurf_render");
  RNA_def_property_ui_range(prop, 0, 6, 1, -1);
  RNA_def_property_ui_text(
      prop, "Simplify Subdivision", "Global maximum subdivision level during rendering");
  RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

  prop = RNA_def_property(srna, "simplify_child_particles_render", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "simplify_particles_render");
  RNA_def_property_ui_text(
      prop, "Simplify Child Particles", "Global child particles percentage during rendering");
  RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

  prop = RNA_def_property(srna, "simplify_volumes", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(
      prop, "Simplify Volumes", "Resolution percentage of volume objects in viewport");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_Scene_simplify_volume_update");

  prop = RNA_def_property(srna, "use_simplify_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", R_SIMPLIFY_NORMALS);
  RNA_def_property_ui_text(prop,
                           "Mesh Normals",
                           "Skip computing custom normals and face corner normals for displaying "
                           "meshes in the viewport");
  RNA_def_property_update(prop, 0, "rna_Scene_use_simplify_normals_update");

  /* Grease Pencil - Simplify Options */
  prop = RNA_def_property(srna, "simplify_gpencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "simplify_gpencil", SIMPLIFY_GPENCIL_ENABLE);
  RNA_def_property_ui_text(prop, "Simplify", "Simplify Grease Pencil drawing");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "simplify_gpencil_onplay", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "simplify_gpencil", SIMPLIFY_GPENCIL_ON_PLAY);
  RNA_def_property_ui_text(
      prop, "Playback Only", "Simplify Grease Pencil only during animation playback");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "simplify_gpencil_antialiasing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "simplify_gpencil", SIMPLIFY_GPENCIL_AA);
  RNA_def_property_ui_text(prop, "Antialiasing", "Use Antialiasing to smooth stroke edges");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "simplify_gpencil_view_fill", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "simplify_gpencil", SIMPLIFY_GPENCIL_FILL);
  RNA_def_property_ui_text(prop, "Fill", "Display fill strokes in the viewport");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "simplify_gpencil_modifier", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "simplify_gpencil", SIMPLIFY_GPENCIL_MODIFIER);
  RNA_def_property_ui_text(prop, "Modifiers", "Display modifiers");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "simplify_gpencil_shader_fx", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "simplify_gpencil", SIMPLIFY_GPENCIL_FX);
  RNA_def_property_ui_text(prop, "Shader Effects", "Display Shader Effects");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "simplify_gpencil_tint", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "simplify_gpencil", SIMPLIFY_GPENCIL_TINT);
  RNA_def_property_ui_text(prop, "Layers Tinting", "Display layer tint");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* persistent data */
  prop = RNA_def_property(srna, "use_persistent_data", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", R_PERSISTENT_DATA);
  RNA_def_property_ui_text(prop,
                           "Persistent Data",
                           "Keep render data around for faster re-renders and animation renders, "
                           "at the cost of increased memory usage");
  RNA_def_property_update(prop, 0, "rna_Scene_use_persistent_data_update");

  /* Freestyle line thickness options */
  prop = RNA_def_property(srna, "line_thickness_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "line_thickness_mode");
  RNA_def_property_enum_items(prop, freestyle_thickness_items);
  RNA_def_property_ui_text(
      prop, "Line Thickness Mode", "Line thickness mode for Freestyle line drawing");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  prop = RNA_def_property(srna, "line_thickness", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_float_sdna(prop, nullptr, "unit_line_thickness");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Line Thickness", "Line thickness in pixels");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

  /* Bake Settings */
  prop = RNA_def_property(srna, "bake", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "bake");
  RNA_def_property_struct_type(prop, "BakeSettings");
  RNA_def_property_ui_text(prop, "Bake Data", "");

  /* Compositor. */

  prop = RNA_def_property(srna, "compositor_device", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, compositor_device_items);
  RNA_def_property_ui_text(prop, "Compositor Device", "Set how compositing is executed");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_Scene_compositor_update");

  prop = RNA_def_property(srna, "compositor_precision", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "compositor_precision");
  RNA_def_property_enum_items(prop, compositor_precision_items);
  RNA_def_property_ui_text(
      prop, "Compositor Precision", "The precision of compositor intermediate result");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_Scene_compositor_update");

  prop = RNA_def_property(srna, "compositor_denoise_device", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "compositor_denoise_device");
  RNA_def_property_enum_items(prop, compositor_denoise_device_items);
  RNA_def_property_enum_default(prop, SCE_COMPOSITOR_DENOISE_DEVICE_AUTO);
  RNA_def_property_ui_text(prop,
                           "Compositor Denoise Node Device",
                           "The device to use to process the denoise nodes in the compositor");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_Scene_compositor_update");

  prop = RNA_def_property(srna, "compositor_denoise_preview_quality", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "compositor_denoise_preview_quality");
  RNA_def_property_enum_items(prop, compositor_denoise_quality_items);
  RNA_def_property_enum_default(prop, SCE_COMPOSITOR_DENOISE_BALANCED);
  RNA_def_property_ui_text(prop,
                           "Compositor Preview Denoise Quality",
                           "The quality used by denoise nodes during viewport and interactive "
                           "compositing if the nodes' quality option is set to Follow Scene");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_Scene_compositor_update");

  prop = RNA_def_property(srna, "compositor_denoise_final_quality", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "compositor_denoise_final_quality");
  RNA_def_property_enum_items(prop, compositor_denoise_quality_items);
  RNA_def_property_enum_default(prop, SCE_COMPOSITOR_DENOISE_HIGH);
  RNA_def_property_ui_text(prop,
                           "Compositor Final Denoise Quality",
                           "The quality used by denoise nodes during the compositing of final "
                           "renders if the nodes' quality option is set to Follow Scene");
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_Scene_compositor_update");

  /* Nestled Data. */
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
  srna = RNA_def_struct(brna, "SceneObjects", nullptr);
  RNA_def_struct_sdna(srna, "Scene");
  RNA_def_struct_ui_text(srna, "Scene Objects", "All of the scene objects");
}

/* scene.timeline_markers */
static void rna_def_timeline_markers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "TimelineMarkers");
  srna = RNA_def_struct(brna, "TimelineMarkers", nullptr);
  RNA_def_struct_sdna(srna, "Scene");
  RNA_def_struct_ui_text(srna, "Timeline Markers", "Collection of timeline markers");

  func = RNA_def_function(srna, "new", "rna_TimeLine_add");
  RNA_def_function_ui_description(func, "Add a timeline marker");
  parm = RNA_def_string(func, "name", "Marker", 0, "", "New name for the marker (not unique)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
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
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

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
  srna = RNA_def_struct(brna, "KeyingSets", nullptr);
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
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Scene_active_keying_set_get",
                                 "rna_Scene_active_keying_set_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_ui_text(
      prop, "Active Keying Set", "Active Keying Set used to insert/delete keyframes");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, nullptr);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "active_keyingset");
  RNA_def_property_int_funcs(prop,
                             "rna_Scene_active_keying_set_index_get",
                             "rna_Scene_active_keying_set_index_set",
                             nullptr);
  RNA_def_property_ui_text(
      prop,
      "Active Keying Set Index",
      "Current Keying Set index (negative for 'builtin' and positive for 'absolute')");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, nullptr);
}

static void rna_def_scene_keying_sets_all(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "KeyingSetsAll");
  srna = RNA_def_struct(brna, "KeyingSetsAll", nullptr);
  RNA_def_struct_sdna(srna, "Scene");
  RNA_def_struct_path_func(srna, "rna_Scene_KeyingsSetsAll_path");
  RNA_def_struct_ui_text(srna, "Keying Sets All", "All available keying sets");

  /* NOTE: no add/remove available here, without screwing up this amalgamated list... */

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "KeyingSet");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Scene_active_keying_set_get",
                                 "rna_Scene_active_keying_set_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_ui_text(
      prop, "Active Keying Set", "Active Keying Set used to insert/delete keyframes");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, nullptr);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "active_keyingset");
  RNA_def_property_int_funcs(prop,
                             "rna_Scene_active_keying_set_index_get",
                             "rna_Scene_active_keying_set_index_set",
                             nullptr);
  RNA_def_property_ui_text(
      prop,
      "Active Keying Set Index",
      "Current Keying Set index (negative for 'builtin' and positive for 'absolute')");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, nullptr);
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

  srna = RNA_def_struct(brna, "DisplaySafeAreas", nullptr);
  RNA_def_struct_ui_text(srna, "Safe Areas", "Safe areas used in 3D view and the sequencer");
  RNA_def_struct_sdna(srna, "DisplaySafeAreas");

  /* SAFE AREAS */
  prop = RNA_def_property(srna, "title", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "title");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Title Safe Margins", "Safe area for text and graphics");
  RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, nullptr);

  prop = RNA_def_property(srna, "action", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "action");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Action Safe Margins", "Safe area for general elements");
  RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, nullptr);

  prop = RNA_def_property(srna, "title_center", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "title_center");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop,
                           "Center Title Safe Margins",
                           "Safe area for text and graphics in a different aspect ratio");
  RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, nullptr);

  prop = RNA_def_property(srna, "action_center", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "action_center");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop,
                           "Center Action Safe Margins",
                           "Safe area for general elements in a different aspect ratio");
  RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, nullptr);
}

static void rna_def_scene_display(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SceneDisplay", nullptr);
  RNA_def_struct_ui_text(srna, "Scene Display", "Scene display settings for 3D viewport");
  RNA_def_struct_sdna(srna, "SceneDisplay");

  prop = RNA_def_property(srna, "light_direction", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_float_sdna(prop, nullptr, "light_direction");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Light Direction", "Direction of the light for shadows and highlights");
  RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_set_update");

  prop = RNA_def_property(srna, "shadow_shift", PROP_FLOAT, PROP_ANGLE);
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

  prop = RNA_def_property(srna, "matcap_ssao_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_ui_text(
      prop, "Distance", "Distance of object that contribute to the cavity/edge effect");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);

  prop = RNA_def_property(srna, "matcap_ssao_attenuation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Attenuation", "Attenuation constant");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);

  prop = RNA_def_property(srna, "matcap_ssao_samples", PROP_INT, PROP_NONE);
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

static void rna_def_raytrace_eevee(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "RaytraceEEVEE", nullptr);
  RNA_def_struct_path_func(srna, "rna_RaytraceEEVEE_path");
  RNA_def_struct_ui_text(
      srna, "EEVEE Raytrace Options", "Quality options for the raytracing pipeline");

  prop = RNA_def_property(srna, "resolution_scale", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, eevee_resolution_scale_items);
  RNA_def_property_ui_text(prop,
                           "Resolution",
                           "Determines the number of rays per pixel. "
                           "Higher resolution uses more memory.");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_denoise", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", RAYTRACE_EEVEE_USE_DENOISE);
  RNA_def_property_ui_text(
      prop, "Denoise", "Enable noise reduction techniques for raytraced effects");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "denoise_spatial", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "denoise_stages", RAYTRACE_EEVEE_DENOISE_SPATIAL);
  RNA_def_property_ui_text(prop, "Spatial Reuse", "Reuse neighbor pixels' rays");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "denoise_temporal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "denoise_stages", RAYTRACE_EEVEE_DENOISE_TEMPORAL);
  RNA_def_property_ui_text(
      prop, "Temporal Accumulation", "Accumulate samples by reprojecting last tracing results");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "denoise_bilateral", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "denoise_stages", RAYTRACE_EEVEE_DENOISE_BILATERAL);
  RNA_def_property_ui_text(
      prop, "Bilateral Filter", "Blur the resolved radiance using a bilateral filter");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "screen_trace_thickness", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_ui_text(
      prop,
      "Screen-Trace Thickness",
      "Surface thickness used to detect intersection when using screen-tracing");
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 5, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "trace_max_roughness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop,
                           "Raytrace Max Roughness",
                           "Maximum roughness to use the tracing pipeline for. Higher "
                           "roughness surfaces will use fast GI approximation. A value of 1 will "
                           "disable fast GI approximation.");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "screen_trace_quality", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(
      prop, "Screen-Trace Precision", "Precision of the screen space ray-tracing");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
}

static void rna_def_scene_eevee(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem eevee_shadow_size_items[] = {
      {128, "128", 0, "128 px", ""},
      {256, "256", 0, "256 px", ""},
      {512, "512", 0, "512 px", ""},
      {1024, "1024", 0, "1024 px", ""},
      {2048, "2048", 0, "2048 px", ""},
      {4096, "4096", 0, "4096 px", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem eevee_pool_size_items[] = {
      {16, "16", 0, "16 MB", ""},
      {32, "32", 0, "32 MB", ""},
      {64, "64", 0, "64 MB", ""},
      {128, "128", 0, "128 MB", ""},
      {256, "256", 0, "256 MB", ""},
      {512, "512", 0, "512 MB", ""},
      {1024, "1024", 0, "1 GB", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem eevee_gi_visibility_size_items[] = {
      {8, "8", 0, "8 px", ""},
      {16, "16", 0, "16 px", ""},
      {32, "32", 0, "32 px", ""},
      {64, "64", 0, "64 px", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem ray_tracing_method_items[] = {
      {RAYTRACE_EEVEE_METHOD_PROBE,
       "PROBE",
       0,
       "Light Probe",
       "Use light probes to find scene intersection"},
      {RAYTRACE_EEVEE_METHOD_SCREEN,
       "SCREEN",
       0,
       "Screen-Trace",
       "Raytrace against the depth buffer. Fallback to light probes for invalid rays."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem fast_gi_method_items[] = {
      {FAST_GI_AO_ONLY,
       "AMBIENT_OCCLUSION_ONLY",
       0,
       "Ambient Occlusion",
       "Use ambient occlusion instead of full global illumination"},
      {FAST_GI_FULL,
       "GLOBAL_ILLUMINATION",
       0,
       "Global Illumination",
       "Compute global illumination taking into account light bouncing off surrounding objects"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SceneEEVEE", nullptr);
  RNA_def_struct_path_func(srna, "rna_SceneEEVEE_path");
  RNA_def_struct_ui_text(srna, "Scene Display", "Scene display settings for 3D viewport");

  /* Indirect Lighting */
  prop = RNA_def_property(srna, "gi_diffuse_bounces", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Diffuse Bounces",
                           "Number of times the light is reinjected inside light grids, "
                           "0 disable indirect diffuse light");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "gi_cubemap_resolution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, eevee_shadow_size_items);
  RNA_def_property_ui_text(prop, "Cubemap Size", "Size of every cubemaps");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_SceneEEVEE_gi_cubemap_resolution_update");

  prop = RNA_def_property(srna, "gi_visibility_resolution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, eevee_gi_visibility_size_items);
  RNA_def_property_ui_text(prop,
                           "Irradiance Visibility Size",
                           "Size of the shadow map applied to each irradiance sample");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "gi_glossy_clamp", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Clamp Glossy",
                           "Clamp pixel intensity to reduce noise inside glossy reflections "
                           "from reflection cubemaps (0 to disable)");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "gi_irradiance_pool_size", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, eevee_pool_size_items);
  RNA_def_property_ui_text(prop,
                           "Irradiance Pool Size",
                           "Size of the irradiance pool, "
                           "a bigger pool size allows for more irradiance grid in the scene "
                           "but might not fit into GPU memory and decrease performance");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* Temporal Anti-Aliasing (super sampling) */
  prop = RNA_def_property(srna, "taa_samples", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Viewport Samples", "Number of samples, unlimited if 0");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  RNA_def_property_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "taa_render_samples", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Render Samples", "Number of samples per pixel for rendering");
  RNA_def_property_range(prop, 1, INT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  RNA_def_property_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_taa_reprojection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_EEVEE_TAA_REPROJECTION);
  RNA_def_property_ui_text(prop,
                           "Viewport Denoising",
                           "Denoise image using temporal reprojection "
                           "(can leave some ghosting)");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  RNA_def_property_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "ray_tracing_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, ray_tracing_method_items);
  RNA_def_property_ui_text(
      prop, "Tracing Method", "Select the tracing method used to find scene-ray intersections");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_shadow_jitter_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_EEVEE_SHADOW_JITTERED_VIEWPORT);
  RNA_def_property_ui_text(prop,
                           "Jittered Shadows (Viewport)",
                           "Enable jittered shadows on the viewport. (Jittered shadows are always "
                           "enabled for final renders).");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* Clamping */
  prop = RNA_def_property(srna, "clamp_surface_direct", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Clamp Surface Direct",
                           "If non-zero, the maximum value for lights contribution on a surface. "
                           "Higher values will be scaled down to avoid too "
                           "much noise and slow convergence at the cost of accuracy. "
                           "Used by light objects.");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "clamp_surface_indirect", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Clamp Surface Indirect",
                           "If non-zero, the maximum value for indirect lighting on surface. "
                           "Higher values will be scaled down to avoid too "
                           "much noise and slow convergence at the cost of accuracy. "
                           "Used by ray-tracing and light-probes.");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(
      prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneEEVEE_clamp_surface_indirect_update");

  prop = RNA_def_property(srna, "clamp_volume_direct", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Clamp Volume Direct",
                           "If non-zero, the maximum value for lights contribution in volumes. "
                           "Higher values will be scaled down to avoid too "
                           "much noise and slow convergence at the cost of accuracy. "
                           "Used by light objects.");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "clamp_volume_indirect", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Clamp Volume Indirect",
                           "If non-zero, the maximum value for indirect lighting in volumes. "
                           "Higher values will be scaled down to avoid too "
                           "much noise and slow convergence at the cost of accuracy. "
                           "Used by light-probes.");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* Volumetrics */
  prop = RNA_def_property(srna, "volumetric_start", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_ui_text(prop, "Start", "Start distance of the volumetric effect");
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "volumetric_end", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_ui_text(prop, "End", "End distance of the volumetric effect");
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "volumetric_tile_size", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, eevee_resolution_scale_items);
  RNA_def_property_ui_text(prop,
                           "Resolution",
                           "Control the quality of the volumetric effects. "
                           "Higher resolution uses more memory.");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "volumetric_samples", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Steps",
                           "Number of steps to compute volumetric effects. "
                           "Higher step count increase VRAM usage and quality.");
  RNA_def_property_range(prop, 1, 256);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "volumetric_sample_distribution", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(
      prop, "Exponential Sampling", "Distribute more samples closer to the camera");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "volumetric_ray_depth", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Volume Max Ray Depth",
                           "Maximum surface intersection count used by the accurate volume "
                           "intersection method. Will create artifact if it is exceeded. "
                           "Higher count increases VRAM usage.");
  RNA_def_property_range(prop, 1, 16);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "volumetric_light_clamp", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Clamp", "Maximum light contribution, reducing noise");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_volumetric_shadows", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_EEVEE_VOLUMETRIC_SHADOWS);
  RNA_def_property_ui_text(
      prop,
      "Volumetric Shadows",
      "Cast shadows from volumetric materials onto volumetric materials (Very expensive)");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "volumetric_shadow_samples", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 128);
  RNA_def_property_ui_text(
      prop, "Volumetric Shadow Samples", "Number of samples to compute volumetric shadowing");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_volume_custom_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_EEVEE_VOLUME_CUSTOM_RANGE);
  RNA_def_property_ui_text(prop,
                           "Volume Custom Range",
                           "Enable custom start and end clip distances for volume computation");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* Fast GI approximation */

  prop = RNA_def_property(srna, "use_fast_gi", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_EEVEE_FAST_GI_ENABLED);
  RNA_def_property_ui_text(prop,
                           "Fast GI Approximation",
                           "Use faster global illumination technique for high roughness surfaces");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "fast_gi_thickness_near", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_ui_text(
      prop,
      "Near Thickness",
      "Geometric thickness of the surfaces when computing fast GI and ambient occlusion. "
      "Reduces light leaking and missing contact occlusion.");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1.0f, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "fast_gi_thickness_far", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_ui_text(
      prop,
      "Far Thickness",
      "Angular thickness of the surfaces when computing fast GI and ambient occlusion. "
      "Reduces energy loss and missing occlusion of far geometry.");
  RNA_def_property_range(prop, DEG2RADF(1.0f), DEG2RADF(180.0f));
  RNA_def_property_ui_range(prop, DEG2RADF(1.0f), DEG2RADF(180.0f), 10.0f, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "fast_gi_quality", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop, "Trace Precision", "Precision of the fast GI ray marching");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "fast_gi_step_count", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 1, 64);
  RNA_def_property_ui_text(prop, "Step Count", "Amount of screen sample per GI ray");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "fast_gi_ray_count", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 1, 16);
  RNA_def_property_ui_text(prop, "Ray Count", "Amount of GI ray to trace for each pixel");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "fast_gi_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, fast_gi_method_items);
  RNA_def_property_ui_text(prop, "Method", "Fast GI approximation method");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "fast_gi_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
  RNA_def_property_ui_text(prop,
                           "Distance",
                           "If non-zero, the maximum distance at which other surfaces will "
                           "contribute to the fast GI approximation");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "fast_gi_bias", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(
      prop, "Bias", "Bias the shading normal to reduce self intersection artifacts");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 0.5f, 1.0f, 2);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "fast_gi_resolution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, eevee_resolution_scale_items);
  RNA_def_property_ui_text(prop,
                           "Resolution",
                           "Control the quality of the fast GI lighting. "
                           "Higher resolution uses more memory.");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* Depth of Field */

  prop = RNA_def_property(srna, "bokeh_max_size", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_ui_text(
      prop, "Max Size", "Max size of the bokeh shape for the depth of field (lower is faster)");
  RNA_def_property_range(prop, 0.0f, 2000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 200.0f, 100.0f, 1);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "bokeh_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(
      prop, "Sprite Threshold", "Brightness threshold for using sprite base depth of field");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 10, 2);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "bokeh_neighbor_max", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop,
                           "Neighbor Rejection",
                           "Maximum brightness to consider when rejecting bokeh sprites "
                           "based on neighborhood (lower is faster)");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 40.0f, 10, 2);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "use_bokeh_jittered", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_EEVEE_DOF_JITTER);
  RNA_def_property_ui_text(prop,
                           "Jitter Camera",
                           "Jitter camera position to create accurate blurring "
                           "using render samples (only for final render)");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "bokeh_overblur", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_ui_text(prop,
                           "Over-blur",
                           "Apply blur to each jittered sample to reduce "
                           "under-sampling artifacts");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_range(prop, 0.0f, 20.0f, 1, 1);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* Motion blur */
  prop = RNA_def_property(srna, "motion_blur_depth_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Bleeding Bias",
                           "Lower values will reduce background"
                           " bleeding onto foreground elements");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.01f, 1000.0f, 1, 2);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "motion_blur_max", PROP_INT, PROP_PIXEL);
  RNA_def_property_ui_text(prop, "Max Blur", "Maximum blur distance a pixel can spread over");
  RNA_def_property_range(prop, 0, 2048);
  RNA_def_property_ui_range(prop, 0, 512, 1, -1);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "motion_blur_steps", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Motion steps",
                           "Controls accuracy of motion blur, "
                           "more steps means longer render time");
  RNA_def_property_range(prop, 1, INT_MAX);
  RNA_def_property_ui_range(prop, 1, 64, 1, -1);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* Shadows */
  prop = RNA_def_property(srna, "use_shadows", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_EEVEE_SHADOW_ENABLED);
  RNA_def_property_ui_text(prop, "Shadows", "Enable shadow casting from lights");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "shadow_pool_size", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, eevee_pool_size_items);
  RNA_def_property_ui_text(prop,
                           "Shadow Pool Size",
                           "Size of the shadow pool, "
                           "a bigger pool size allows for more shadows in the scene "
                           "but might not fit into GPU memory");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "shadow_ray_count", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 1, 4);
  RNA_def_property_ui_text(
      prop, "Shadow Ray Count", "Amount of shadow ray to trace for each light");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "shadow_step_count", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 1, 16);
  RNA_def_property_ui_text(
      prop, "Shadow Step Count", "Amount of shadow map sample per shadow ray");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "light_threshold", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop,
                           "Light Threshold",
                           "Minimum light intensity for a light to contribute to the lighting");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* Overscan */
  prop = RNA_def_property(srna, "use_overscan", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_EEVEE_OVERSCAN);
  RNA_def_property_ui_text(prop,
                           "Overscan",
                           "Internally render past the image border to avoid "
                           "screen-space effects disappearing");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "overscan_size", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, nullptr, "overscan");
  RNA_def_property_ui_text(prop,
                           "Overscan Size",
                           "Percentage of render size to add as overscan to the "
                           "internal render buffers");
  RNA_def_property_range(prop, 0.0f, 50.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 2);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "ray_tracing_options", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "RaytraceEEVEE");
  RNA_def_property_ui_text(
      prop, "Reflection Trace Options", "EEVEE settings for tracing reflections");

  prop = RNA_def_property(srna, "use_raytracing", PROP_BOOLEAN, PROP_NONE);
  /* Reuse the same property as legacy EEVEE for compatibility. */
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_EEVEE_SSR_ENABLED);
  RNA_def_property_ui_text(prop, "Use Ray-Tracing", "Enable the ray-tracing module");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "shadow_resolution_scale", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Shadows Resolution Scale", "Resolution percentage of shadow maps");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_SceneEEVEE_shadow_resolution_update");
}

static void rna_def_scene_gpencil(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SceneGpencil", nullptr);
  RNA_def_struct_path_func(srna, "rna_SceneGpencil_path");
  RNA_def_struct_ui_text(srna, "Grease Pencil Render", "Render settings");

  prop = RNA_def_property(srna, "antialias_threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "smaa_threshold");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
  RNA_def_property_ui_text(prop,
                           "SMAA Threshold Viewport",
                           "Threshold for edge detection algorithm (higher values might over-blur "
                           "some part of the image)");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "antialias_threshold_render", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "smaa_threshold_render");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
  RNA_def_property_ui_text(prop,
                           "SMAA Threshold Render",
                           "Threshold for edge detection algorithm (higher values might over-blur "
                           "some part of the image). Only applies to final render");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  prop = RNA_def_property(srna, "aa_samples", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Anti-Aliasing Samples",
      "Number of supersampling anti-aliasing samples per pixel for final render");
  RNA_def_property_range(prop, 1, INT_MAX);
  RNA_def_property_ui_range(prop, 1, 256, 1, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  RNA_def_property_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "motion_blur_steps", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Motion Blur Steps",
                           "Controls accuracy of motion blur, more steps result in longer render "
                           "time. Only used when Motion Blur is enabled. Set to 0 to disable "
                           "motion blur for Grease Pencil");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_range(prop, 0, 64, 1, -1);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
  RNA_def_property_flag(prop, PROP_ANIMATABLE);

  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
}

static void rna_def_scene_hydra(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem hydra_export_method_items[] = {
      {SCE_HYDRA_EXPORT_HYDRA,
       "HYDRA",
       0,
       "Hydra",
       "Fast interactive editing through native Hydra integration"},
      {SCE_HYDRA_EXPORT_USD,
       "USD",
       0,
       "USD",
       "Export scene through USD file, for accurate comparison with USD file export"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SceneHydra", nullptr);
  RNA_def_struct_path_func(srna, "rna_SceneHydra_path");
  RNA_def_struct_ui_text(srna, "Scene Hydra", "Scene Hydra render engine settings");

  prop = RNA_def_property(srna, "export_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, hydra_export_method_items);
  RNA_def_property_ui_text(
      prop, "Export Method", "How to export the Blender scene to the Hydra render engine");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);
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
      {5, "EXPONENT", 0, "Exponential", "Exponential distance model"},
      {6,
       "EXPONENT_CLAMPED",
       0,
       "Exponential Clamped",
       "Exponential distance model with clamping"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem sync_mode_items[] = {
      {0, "NONE", 0, "Play Every Frame", "Do not sync, play every frame"},
      {SCE_FRAME_DROP, "FRAME_DROP", 0, "Frame Dropping", "Drop frames if playback is too slow"},
      {AUDIO_SYNC, "AUDIO_SYNC", 0, "Sync to Audio", "Sync to audio playback, dropping frames"},
      {0, nullptr, 0, nullptr, nullptr},
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
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_Camera_object_poll");
  RNA_def_property_ui_text(prop, "Camera", "Active camera, used for rendering the scene");
  RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_camera_update");

  prop = RNA_def_property(srna, "background_set", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "set");
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_Scene_set_set", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Background Scene", "Background set scene");
  RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_set_update");

  prop = RNA_def_property(srna, "world", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "World", "World used for rendering the scene");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_WORLD);
  RNA_def_property_update(prop, NC_SCENE | ND_WORLD, "rna_Scene_world_update");

  prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(prop, "Objects", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Scene_objects_begin",
                                    "rna_Scene_objects_next",
                                    "rna_Scene_objects_end",
                                    "rna_Scene_objects_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  rna_def_scene_objects(brna, prop);

  /* Frame Range Stuff */
  prop = RNA_def_property(srna, "frame_current", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "r.cfra");
  RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
  RNA_def_property_int_funcs(prop, nullptr, "rna_Scene_frame_current_set", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Current Frame",
      "Current frame, to update animation data from Python frame_set() instead");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_frame_update");

  prop = RNA_def_property(srna, "frame_subframe", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "r.subframe");
  RNA_def_property_ui_text(prop, "Current Subframe", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 2);
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_frame_update");

  prop = RNA_def_property(srna, "frame_float", PROP_FLOAT, PROP_TIME);
  RNA_def_property_ui_text(prop, "Current Subframe", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
  RNA_def_property_ui_range(prop, MINAFRAME, MAXFRAME, 0.1, 2);
  RNA_def_property_float_funcs(
      prop, "rna_Scene_frame_float_get", "rna_Scene_frame_float_set", nullptr);
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_frame_update");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "r.sfra");
  RNA_def_property_int_funcs(prop, nullptr, "rna_Scene_start_frame_set", nullptr);
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "Start Frame", "First frame of the playback/rendering range");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME_RANGE, nullptr);

  prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "r.efra");
  RNA_def_property_int_funcs(prop, nullptr, "rna_Scene_end_frame_set", nullptr);
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "End Frame", "Final frame of the playback/rendering range");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME_RANGE, nullptr);

  prop = RNA_def_property(srna, "frame_step", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "r.frame_step");
  RNA_def_property_range(prop, 0, MAXFRAME);
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);
  RNA_def_property_ui_text(
      prop,
      "Frame Step",
      "Number of frames to skip forward while rendering/playing back each frame");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, nullptr);

  prop = RNA_def_property(srna, "frame_current_final", PROP_FLOAT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
  RNA_def_property_float_funcs(prop, "rna_Scene_frame_current_final_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Current Frame Final", "Current frame with subframe and time remapping applied");

  prop = RNA_def_property(srna, "lock_frame_selection_to_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "r.flag", SCER_LOCK_FRAME_SELECTION);
  RNA_def_property_ui_text(prop,
                           "Lock Frame Selection",
                           "Don't allow frame to be selected with mouse outside of frame range");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, nullptr);

  /* Preview Range (frame-range for UI playback) */
  prop = RNA_def_property(srna, "use_preview_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "r.flag", SCER_PRV_RANGE);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Scene_use_preview_range_set");
  RNA_def_property_ui_text(
      prop,
      "Use Preview Range",
      "Use an alternative start/end frame range for animation playback and view renders");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, nullptr);
  RNA_def_property_ui_icon(prop, ICON_PREVIEW_RANGE, 0);

  prop = RNA_def_property(srna, "frame_preview_start", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "r.psfra");
  RNA_def_property_int_funcs(prop, nullptr, "rna_Scene_preview_range_start_frame_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Preview Range Start Frame", "Alternative start frame for UI playback");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, nullptr);

  prop = RNA_def_property(srna, "frame_preview_end", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "r.pefra");
  RNA_def_property_int_funcs(prop, nullptr, "rna_Scene_preview_range_end_frame_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Preview Range End Frame", "Alternative end frame for UI playback");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, nullptr);

  /* Sub-frame for motion-blur debug. */
  prop = RNA_def_property(srna, "show_subframe", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "r.flag", SCER_SHOW_SUBFRAME);
  RNA_def_property_ui_text(
      prop,
      "Show Subframe",
      "Display and allow setting fractional frame values for the current frame");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_show_subframe_update");

  /* Timeline / Time Navigation settings */
  prop = RNA_def_property(srna, "show_keys_from_selected_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SCE_KEYS_NO_SELONLY);
  RNA_def_property_ui_text(
      prop, "Only Show Selected", "Only include channels relating to selected objects and data");
  RNA_def_property_update(prop, NC_SCENE | ND_FRAME, nullptr);

  /* Stamp */
  prop = RNA_def_property(srna, "use_stamp_note", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "r.stamp_udata");
  RNA_def_property_ui_text(prop, "Stamp Note", "User defined note for the render stamping");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, nullptr);

  /* Animation Data (for Scene) */
  rna_def_animdata_common(srna);

  /* Readonly Properties */
  prop = RNA_def_property(srna, "is_nla_tweakmode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_NLA_EDIT_ON);
  RNA_def_property_clear_flag(prop,
                              PROP_EDITABLE); /* DO NOT MAKE THIS EDITABLE, OR NLA EDITOR BREAKS */
  RNA_def_property_ui_text(
      prop,
      "NLA Tweak Mode",
      "Whether there is any action referenced by NLA being edited (strictly read-only)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  /* Frame dropping flag for playback and sync enum */
#  if 0 /* XXX: Is this actually needed? */
  prop = RNA_def_property(srna, "use_frame_drop", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_FRAME_DROP);
  RNA_def_property_ui_text(
      prop, "Frame Dropping", "Play back dropping frames if frame display is too slow");
  RNA_def_property_update(prop, NC_SCENE, nullptr);
#  endif

  prop = RNA_def_property(srna, "use_custom_simulation_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SCE_CUSTOM_SIMULATION_RANGE);
  RNA_def_property_ui_text(prop,
                           "Custom Simulation Range",
                           "Use a simulation range that is different from the scene range for "
                           "simulation nodes that don't override the frame range themselves");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_set_update");

  prop = RNA_def_property(srna, "simulation_frame_start", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Simulation Frame Start", "Frame at which simulations start");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_set_update");

  prop = RNA_def_property(srna, "simulation_frame_end", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Simulation Frame End", "Frame at which simulations end");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_set_update");

  prop = RNA_def_property(srna, "sync_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop, "rna_Scene_sync_mode_get", "rna_Scene_sync_mode_set", nullptr);
  RNA_def_property_enum_items(prop, sync_mode_items);
  RNA_def_property_enum_default(prop, AUDIO_SYNC);
  RNA_def_property_ui_text(prop, "Sync Mode", "How to sync playback");
  RNA_def_property_update(prop, NC_SCENE, nullptr);

  /* Nodes (Compositing) */
  prop = RNA_def_property(srna, "compositing_node_group", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "compositing_node_group");
  RNA_def_property_struct_type(prop, "NodeTree");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Node Tree", "Compositor Nodes");
  RNA_def_property_update(prop, 0, "rna_Scene_compositor_update");
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_Scene_compositing_node_group_set",
                                 nullptr,
                                 "rna_Scene_compositing_node_group_poll");

  /* Sequencer */
  prop = RNA_def_property(srna, "sequence_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "ed");
  RNA_def_property_struct_type(prop, "SequenceEditor");
  RNA_def_property_ui_text(prop, "Sequence Editor", "");

  /* Keying Sets */
  prop = RNA_def_property(srna, "keying_sets", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "keyingsets", nullptr);
  RNA_def_property_struct_type(prop, "KeyingSet");
  RNA_def_property_ui_text(prop, "Absolute Keying Sets", "Absolute Keying Sets for this Scene");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, nullptr);
  rna_def_scene_keying_sets(brna, prop);

  prop = RNA_def_property(srna, "keying_sets_all", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Scene_all_keyingsets_begin",
                                    "rna_Scene_all_keyingsets_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "KeyingSet");
  RNA_def_property_ui_text(
      prop,
      "All Keying Sets",
      "All Keying Sets available for use (Builtins and Absolute Keying Sets for this Scene)");
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, nullptr);
  rna_def_scene_keying_sets_all(brna, prop);

  /* Rigid Body Simulation */
  prop = RNA_def_property(srna, "rigidbody_world", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "rigidbody_world");
  RNA_def_property_struct_type(prop, "RigidBodyWorld");
  RNA_def_property_ui_text(prop, "Rigid Body World", "");
  RNA_def_property_update(prop, NC_SCENE, "rna_Physics_relations_update");

  /* Tool Settings */
  prop = RNA_def_property(srna, "tool_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_pointer_sdna(prop, nullptr, "toolsettings");
  RNA_def_property_struct_type(prop, "ToolSettings");
  RNA_def_property_ui_text(prop, "Tool Settings", "");

  /* Unit Settings */
  prop = RNA_def_property(srna, "unit_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "unit");
  RNA_def_property_struct_type(prop, "UnitSettings");
  RNA_def_property_ui_text(prop, "Unit Settings", "Unit editing settings");

  /* Physics Settings */
  prop = RNA_def_property(srna, "gravity", PROP_FLOAT, PROP_ACCELERATION);
  RNA_def_property_float_sdna(prop, nullptr, "physics_settings.gravity");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_range(prop, -200.0f, 200.0f, 1, 2);
  RNA_def_property_ui_text(prop, "Gravity", "Constant acceleration in a given direction");
  RNA_def_property_update(prop, 0, "rna_Physics_update");

  prop = RNA_def_property(srna, "use_gravity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "physics_settings.flag", PHYS_GLOBAL_GRAVITY);
  RNA_def_property_ui_text(prop, "Global Gravity", "Use global gravity for all dynamics");
  RNA_def_property_update(prop, 0, "rna_Physics_update");

  /* Render Data */
  prop = RNA_def_property(srna, "render", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "r");
  RNA_def_property_struct_type(prop, "RenderSettings");
  RNA_def_property_ui_text(prop, "Render Data", "");

  /* Safe Areas */
  prop = RNA_def_property(srna, "safe_areas", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "safe_areas");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "DisplaySafeAreas");
  RNA_def_property_ui_text(prop, "Safe Areas", "");

  /* Markers */
  prop = RNA_def_property(srna, "timeline_markers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "markers", nullptr);
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
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "TransformOrientationSlot");
  RNA_def_property_ui_text(prop, "Transform Orientation Slots", "");

  /* 3D View Cursor */
  prop = RNA_def_property(srna, "cursor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "cursor");
  RNA_def_property_struct_type(prop, "View3DCursor");
  RNA_def_property_ui_text(prop, "3D Cursor", "");

  /* Audio Settings */
  prop = RNA_def_property(srna, "use_audio", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Scene_use_audio_get", "rna_Scene_use_audio_set");
  RNA_def_property_ui_text(
      prop, "Play Audio", "Play back of audio from Sequence Editor, otherwise mute audio");
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_use_audio_update");

#  if 0 /* XXX: Is this actually needed? */
  prop = RNA_def_property(srna, "use_audio_sync", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "audio.flag", AUDIO_SYNC);
  RNA_def_property_ui_text(
      prop,
      "Audio Sync",
      "Play back and sync with audio clock, dropping frames if frame display is too slow");
  RNA_def_property_update(prop, NC_SCENE, nullptr);
#  endif

  prop = RNA_def_property(srna, "use_audio_scrub", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "audio.flag", AUDIO_SCRUB);
  RNA_def_property_ui_text(
      prop, "Audio Scrubbing", "Play audio from Sequence Editor while scrubbing");
  RNA_def_property_update(prop, NC_SCENE, nullptr);

  prop = RNA_def_property(srna, "audio_doppler_speed", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "audio.speed_of_sound");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.01f, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Speed of Sound", "Speed of sound for Doppler effect calculation");
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_listener_update");

  prop = RNA_def_property(srna, "audio_doppler_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "audio.doppler_factor");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_text(prop, "Doppler Factor", "Pitch factor for Doppler effect calculation");
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_listener_update");

  prop = RNA_def_property(srna, "audio_distance_model", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "audio.distance_model");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, audio_distance_model_items);
  RNA_def_property_ui_text(
      prop, "Distance Model", "Distance model for distance attenuation calculation");
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_listener_update");

  prop = RNA_def_property(srna, "audio_volume", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "audio.volume");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Volume", "Audio volume");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
  RNA_def_property_update(prop, NC_SCENE, nullptr);
  RNA_def_property_update(prop, NC_SCENE, "rna_Scene_volume_update");

  func = RNA_def_function(srna, "update_render_engine", "rna_Scene_update_render_engine");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Trigger a render engine update");

  /* Statistics */
  func = RNA_def_function(srna, "statistics", "rna_Scene_statistics_string_get");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "view_layer", "ViewLayer", "View Layer", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_string(func, "statistics", nullptr, 0, "Statistics", "");
  RNA_def_function_return(func, parm);

  /* Grease Pencil */
  prop = RNA_def_property(srna, "annotation", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gpd");
  RNA_def_property_struct_type(prop, "Annotation");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_GPencil_datablocks_annotations_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Annotations", "Data-block used for annotations in the 3D view");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

  /* active MovieClip */
  prop = RNA_def_property(srna, "active_clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "clip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_ui_text(prop,
                           "Active Movie Clip",
                           "Active Movie Clip that can be used by motion tracking constraints "
                           "or as a camera's background image");
  RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, nullptr);

  /* color management */
  prop = RNA_def_property(srna, "view_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "view_settings");
  RNA_def_property_struct_type(prop, "ColorManagedViewSettings");
  RNA_def_property_ui_text(
      prop, "View Settings", "Color management settings applied on image before saving");

  prop = RNA_def_property(srna, "display_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "display_settings");
  RNA_def_property_struct_type(prop, "ColorManagedDisplaySettings");
  RNA_def_property_ui_text(
      prop, "Display Settings", "Settings of device saved image would be displayed on");

  prop = RNA_def_property(srna, "sequencer_colorspace_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "sequencer_colorspace_settings");
  RNA_def_property_struct_type(prop, "ColorManagedSequencerColorspaceSettings");
  RNA_def_property_ui_text(
      prop, "Sequencer Color Space Settings", "Settings of color space sequencer is working in");

  /* Layer and Collections */
  prop = RNA_def_property(srna, "view_layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "view_layers", nullptr);
  RNA_def_property_struct_type(prop, "ViewLayer");
  RNA_def_property_ui_text(prop, "View Layers", "");
  rna_def_view_layers(brna, prop);

  prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "master_collection");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_clear_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop,
                           "Collection",
                           "Scene root collection that owns all the objects and other collections "
                           "instantiated in the scene");

  /* Scene Display */
  prop = RNA_def_property(srna, "display", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "display");
  RNA_def_property_struct_type(prop, "SceneDisplay");
  RNA_def_property_ui_text(prop, "Scene Display", "Scene display settings for 3D viewport");

  /* EEVEE */
  prop = RNA_def_property(srna, "eevee", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SceneEEVEE");
  RNA_def_property_ui_text(prop, "EEVEE", "EEVEE settings for the scene");

  /* Grease Pencil */
  prop = RNA_def_property(srna, "grease_pencil_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SceneGpencil");
  RNA_def_property_ui_text(prop, "Grease Pencil", "Grease Pencil settings for the scene");

  /* Hydra */
  prop = RNA_def_property(srna, "hydra", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SceneHydra");
  RNA_def_property_ui_text(prop, "Hydra", "Hydra settings for the scene");

  /* Nestled Data. */
  /* *** Non-Animated *** */
  RNA_define_animate_sdna(false);
  rna_def_tool_settings(brna);
  rna_def_gpencil_interpolate(brna);
  rna_def_curve_paint_settings(brna);
  rna_def_sequencer_tool_settings(brna);
  rna_def_statvis(brna);
  rna_def_unit_settings(brna);
  rna_def_scene_image_format_data(brna);
  rna_def_transform_orientation(brna);
  rna_def_transform_orientation_slot(brna);
  rna_def_view3d_cursor(brna);
  rna_def_selected_uv_element(brna);
  rna_def_display_safe_areas(brna);
  rna_def_scene_display(brna);
  rna_def_raytrace_eevee(brna);
  rna_def_scene_eevee(brna);
  rna_def_scene_hydra(brna);
  rna_def_view_layer_aov(brna);
  rna_def_view_layer_lightgroup(brna);
  rna_def_view_layer_eevee(brna);
  rna_def_scene_gpencil(brna);
  RNA_define_animate_sdna(true);
  /* *** Animated *** */
  rna_def_scene_render_data(brna);
  rna_def_scene_render_view(brna);

  /* Scene API */
  RNA_api_scene(srna);
}

#endif
