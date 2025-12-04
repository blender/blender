/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_enum_flags.hh"

/** #ToolSettings.vgroupsubset */
enum eVGroupSelect {
  WT_VGROUP_ALL = 0,
  WT_VGROUP_ACTIVE = 1,
  WT_VGROUP_BONE_SELECT = 2,
  WT_VGROUP_BONE_DEFORM = 3,
  WT_VGROUP_BONE_DEFORM_OFF = 4,
};

#define WT_VGROUP_MASK_ALL \
  ((1 << WT_VGROUP_ACTIVE) | (1 << WT_VGROUP_BONE_SELECT) | (1 << WT_VGROUP_BONE_DEFORM) | \
   (1 << WT_VGROUP_BONE_DEFORM_OFF) | (1 << WT_VGROUP_ALL))

enum eSeqImageFitMethod {
  SEQ_SCALE_TO_FIT,
  SEQ_SCALE_TO_FILL,
  SEQ_STRETCH_TO_FILL,
  SEQ_USE_ORIGINAL_SIZE,
};

/**
 * #Paint::symmetry_flags
 * (for now just a duplicate of sculpt symmetry flags).
 */
enum ePaintSymmetryFlags {
  PAINT_SYMM_NONE = 0,
  PAINT_SYMM_X = (1 << 0),
  PAINT_SYMM_Y = (1 << 1),
  PAINT_SYMM_Z = (1 << 2),
  PAINT_SYMMETRY_FEATHER = (1 << 3),
  PAINT_TILE_X = (1 << 4),
  PAINT_TILE_Y = (1 << 5),
  PAINT_TILE_Z = (1 << 6),
};
ENUM_OPERATORS(ePaintSymmetryFlags);
#define PAINT_SYMM_AXIS_ALL (PAINT_SYMM_X | PAINT_SYMM_Y | PAINT_SYMM_Z)

#ifdef __cplusplus
inline ePaintSymmetryFlags operator++(ePaintSymmetryFlags &flags, int)
{
  flags = ePaintSymmetryFlags(char(flags) + 1);
  return flags;
}
#endif

/** #UnifiedPaintSettings::flag */
enum eUnifiedPaintSettingsFlags {
  UNIFIED_PAINT_SIZE = (1 << 0),
  UNIFIED_PAINT_ALPHA = (1 << 1),
  /** Only used if unified size is enabled, mirrors the brush flag #BRUSH_LOCK_SIZE. */
  UNIFIED_PAINT_BRUSH_LOCK_SIZE = (1 << 2),
  UNIFIED_PAINT_FLAG_UNUSED_0 = (1 << 3),
  UNIFIED_PAINT_FLAG_UNUSED_1 = (1 << 4),
  UNIFIED_PAINT_WEIGHT = (1 << 5),
  UNIFIED_PAINT_COLOR = (1 << 6),
  UNIFIED_PAINT_INPUT_SAMPLES = (1 << 7),
  UNIFIED_PAINT_COLOR_JITTER = (1 << 8),
};

/** Paint::curve_visibility_flag*/
enum PaintCurveVisibilityFlags {
  PAINT_CURVE_SHOW_STRENGTH = (1 << 0),
  PAINT_CURVE_SHOW_SIZE = (1 << 1),
  PAINT_CURVE_SHOW_JITTER = (1 << 2),
};

/** #SceneRenderLayer::passflag */
enum eScenePassType {
  SCE_PASS_COMBINED = (1 << 0),
  SCE_PASS_DEPTH = (1 << 1),
  SCE_PASS_UNUSED_1 = (1 << 2), /* RGBA */
  SCE_PASS_UNUSED_2 = (1 << 3), /* DIFFUSE */
  SCE_PASS_UNUSED_3 = (1 << 4), /* SPEC */
  SCE_PASS_SHADOW = (1 << 5),
  SCE_PASS_AO = (1 << 6),
  SCE_PASS_POSITION = (1 << 7),
  SCE_PASS_NORMAL = (1 << 8),
  SCE_PASS_VECTOR = (1 << 9),
  SCE_PASS_UNUSED_5 = (1 << 10), /* REFRACT */
  SCE_PASS_INDEXOB = (1 << 11),
  SCE_PASS_UV = (1 << 12),
  SCE_PASS_UNUSED_6 = (1 << 13), /* INDIRECT */
  SCE_PASS_MIST = (1 << 14),
  SCE_PASS_UNUSED_7 = (1 << 15), /* RAYHITS */
  SCE_PASS_EMIT = (1 << 16),
  SCE_PASS_ENVIRONMENT = (1 << 17),
  SCE_PASS_INDEXMA = (1 << 18),
  SCE_PASS_DIFFUSE_DIRECT = (1 << 19),
  SCE_PASS_DIFFUSE_INDIRECT = (1 << 20),
  SCE_PASS_DIFFUSE_COLOR = (1 << 21),
  SCE_PASS_GLOSSY_DIRECT = (1 << 22),
  SCE_PASS_GLOSSY_INDIRECT = (1 << 23),
  SCE_PASS_GLOSSY_COLOR = (1 << 24),
  SCE_PASS_TRANSM_DIRECT = (1 << 25),
  SCE_PASS_TRANSM_INDIRECT = (1 << 26),
  SCE_PASS_TRANSM_COLOR = (1 << 27),
  SCE_PASS_SUBSURFACE_DIRECT = (1 << 28),
  SCE_PASS_SUBSURFACE_INDIRECT = (1 << 29),
  SCE_PASS_SUBSURFACE_COLOR = (1 << 30),
  SCE_PASS_ROUGHNESS = (1u << 31u),
};

#define RE_PASSNAME_DEPRECATED "Deprecated"

#define RE_PASSNAME_COMBINED "Combined"
#define RE_PASSNAME_DEPTH "Depth"
#define RE_PASSNAME_VECTOR "Vector"
#define RE_PASSNAME_POSITION "Position"
#define RE_PASSNAME_NORMAL "Normal"
#define RE_PASSNAME_UV "UV"
#define RE_PASSNAME_EMIT "Emission"
#define RE_PASSNAME_SHADOW "Shadow"

#define RE_PASSNAME_AO "Ambient Occlusion"
#define RE_PASSNAME_ENVIRONMENT "Environment"
#define RE_PASSNAME_INDEXOB "Object Index"
#define RE_PASSNAME_INDEXMA "Material Index"
#define RE_PASSNAME_MIST "Mist"

#define RE_PASSNAME_DIFFUSE_DIRECT "Diffuse Direct"
#define RE_PASSNAME_DIFFUSE_INDIRECT "Diffuse Indirect"
#define RE_PASSNAME_DIFFUSE_COLOR "Diffuse Color"
#define RE_PASSNAME_GLOSSY_DIRECT "Glossy Direct"
#define RE_PASSNAME_GLOSSY_INDIRECT "Glossy Indirect"
#define RE_PASSNAME_GLOSSY_COLOR "Glossy Color"
#define RE_PASSNAME_TRANSM_DIRECT "Transmission Direct"
#define RE_PASSNAME_TRANSM_INDIRECT "Transmission Indirect"
#define RE_PASSNAME_TRANSM_COLOR "Transmission Color"

#define RE_PASSNAME_SUBSURFACE_DIRECT "Subsurface Direct"
#define RE_PASSNAME_SUBSURFACE_INDIRECT "Subsurface Indirect"
#define RE_PASSNAME_SUBSURFACE_COLOR "Subsurface Color"

#define RE_PASSNAME_FREESTYLE "Freestyle"
#define RE_PASSNAME_VOLUME_LIGHT "Volume Direct"
#define RE_PASSNAME_TRANSPARENT "Transparent"

#define RE_PASSNAME_CRYPTOMATTE_OBJECT "CryptoObject"
#define RE_PASSNAME_CRYPTOMATTE_ASSET "CryptoAsset"
#define RE_PASSNAME_CRYPTOMATTE_MATERIAL "CryptoMaterial"

#define RE_PASSNAME_GREASE_PENCIL "Grease Pencil"

/** #SceneRenderLayer::layflag */
enum {
  SCE_LAY_SOLID = 1 << 0,
  SCE_LAY_UNUSED_1 = 1 << 1,
  SCE_LAY_UNUSED_2 = 1 << 2,
  SCE_LAY_UNUSED_3 = 1 << 3,
  SCE_LAY_SKY = 1 << 4,
  SCE_LAY_STRAND = 1 << 5,
  SCE_LAY_FRS = 1 << 6,
  SCE_LAY_AO = 1 << 7,
  SCE_LAY_VOLUMES = 1 << 8,
  SCE_LAY_MOTION_BLUR = 1 << 9,
  SCE_LAY_GREASE_PENCIL = 1 << 10,

  /* Flags between (1 << 9) and (1 << 15) are set to 1 already, for future options. */

  SCE_LAY_FLAG_DEFAULT = ((1 << 15) - 1),

  SCE_LAY_UNUSED_4 = 1 << 15,
  SCE_LAY_UNUSED_5 = 1 << 16,
  SCE_LAY_DISABLE = 1 << 17,
  SCE_LAY_UNUSED_6 = 1 << 18,
  SCE_LAY_UNUSED_7 = 1 << 19,
};

/** #SceneRenderView::viewflag */
enum {
  SCE_VIEW_DISABLE = 1 << 0,
};

/** #RenderData::views_format */
enum {
  SCE_VIEWS_FORMAT_STEREO_3D = 0,
  SCE_VIEWS_FORMAT_MULTIVIEW = 1,
};

/** #ImageFormatData::views_format (also used for #Strip::views_format). */
enum {
  R_IMF_VIEWS_INDIVIDUAL = 0,
  R_IMF_VIEWS_STEREO_3D = 1,
  R_IMF_VIEWS_MULTIVIEW = 2,
};

/** #Stereo3dFormat::display_mode */
enum eStereoDisplayMode {
  S3D_DISPLAY_ANAGLYPH = 0,
  S3D_DISPLAY_INTERLACE = 1,
  S3D_DISPLAY_PAGEFLIP = 2,
  S3D_DISPLAY_SIDEBYSIDE = 3,
  S3D_DISPLAY_TOPBOTTOM = 4,
};

/** #Stereo3dFormat::flag */
enum eStereo3dFlag {
  S3D_INTERLACE_SWAP = (1 << 0),
  S3D_SIDEBYSIDE_CROSSEYED = (1 << 1),
  S3D_SQUEEZED_FRAME = (1 << 2),
};

/** #Stereo3dFormat::anaglyph_type */
enum eStereo3dAnaglyphType {
  S3D_ANAGLYPH_REDCYAN = 0,
  S3D_ANAGLYPH_GREENMAGENTA = 1,
  S3D_ANAGLYPH_YELLOWBLUE = 2,
};

/** #Stereo3dFormat::interlace_type */
enum eStereo3dInterlaceType {
  S3D_INTERLACE_ROW = 0,
  S3D_INTERLACE_COLUMN = 1,
  S3D_INTERLACE_CHECKERBOARD = 2,
};

/** #View3D::stereo3d_camera / #View3D::multiview_eye / #ImageUser::multiview_eye */
enum eStereoViews {
  STEREO_LEFT_ID = 0,
  STEREO_RIGHT_ID = 1,
  STEREO_3D_ID = 2,
  STEREO_MONO_ID = 3,
};
