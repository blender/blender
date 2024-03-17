/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"

/* XXX(@ideasman42): temp feature. */
#define DURIAN_CAMERA_SWITCH

/**
 * Check for cyclic set-scene.
 * Libraries can cause this case which is normally prevented, see (#42009).
 */
#define USE_SETSCENE_CHECK

#include "DNA_ID.h"
#include "DNA_brush_types.h"      /* DynTopoSettings */
#include "DNA_color_types.h"      /* color management */
#include "DNA_customdata_types.h" /* Scene's runtime custom-data masks. */
#include "DNA_layer_types.h"
#include "DNA_listBase.h"
#include "DNA_scene_enums.h"
#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"

struct AnimData;
struct Brush;
struct Collection;
struct ColorSpace;
struct CurveMapping;
struct CurveProfile;
struct CustomData_MeshMasks;
struct Editing;
struct Image;
struct MovieClip;
struct Object;
struct Scene;
struct World;
struct bGPdata;
struct bNodeTree;

/** Workaround to forward-declare C++ type in C header. */
#ifdef __cplusplus
namespace blender::bke {
class SceneRuntime;
}
using SceneRuntimeHandle = blender::bke::SceneRuntime;
#else   // __cplusplus
typedef struct SceneRuntimeHandle SceneRuntimeHandle;
#endif  // __cplusplus

/* -------------------------------------------------------------------- */
/** \name FFMPEG
 * \{ */

typedef struct AviCodecData {
  /** Save format. */
  void *lpFormat;
  /** Compressor options. */
  void *lpParms;
  /** Size of lpFormat buffer. */
  unsigned int cbFormat;
  /** Size of lpParms buffer. */
  unsigned int cbParms;

  /** Stream type, for consistency. */
  unsigned int fccType;
  /** Compressor. */
  unsigned int fccHandler;
  /** Keyframe rate. */
  unsigned int dwKeyFrameEvery;
  /** Compress quality 0-10,000. */
  unsigned int dwQuality;
  /** Bytes per second. */
  unsigned int dwBytesPerSecond;
  /** Flags... see below. */
  unsigned int dwFlags;
  /** For non-video streams only. */
  unsigned int dwInterleaveEvery;
  char _pad[4];

  char avicodecname[128];
} AviCodecData;

typedef enum eFFMpegPreset {
  FFM_PRESET_NONE = 0,

#ifdef DNA_DEPRECATED_ALLOW
  /* Previously used by h.264 to control encoding speed vs. file size. */
  FFM_PRESET_ULTRAFAST = 1, /* DEPRECATED */
  FFM_PRESET_SUPERFAST = 2, /* DEPRECATED */
  FFM_PRESET_VERYFAST = 3,  /* DEPRECATED */
  FFM_PRESET_FASTER = 4,    /* DEPRECATED */
  FFM_PRESET_FAST = 5,      /* DEPRECATED */
  FFM_PRESET_MEDIUM = 6,    /* DEPRECATED */
  FFM_PRESET_SLOW = 7,      /* DEPRECATED */
  FFM_PRESET_SLOWER = 8,    /* DEPRECATED */
  FFM_PRESET_VERYSLOW = 9,  /* DEPRECATED */
#endif

  /* Used by WEBM/VP9 and h.264 to control encoding speed vs. file size.
   * WEBM/VP9 use these values directly, whereas h.264 map those to
   * respectively the MEDIUM, SLOWER, and SUPERFAST presets. */

  /** The default and recommended for most applications. */
  FFM_PRESET_GOOD = 10,
  /** Recommended if you have lots of time and want the best compression efficiency. */
  FFM_PRESET_BEST = 11,
  /** Recommended for live / fast encoding. */
  FFM_PRESET_REALTIME = 12,
} eFFMpegPreset;

/**
 * Mapping from easily-understandable descriptions to CRF values.
 * Assumes we output 8-bit video. Needs to be remapped if 10-bit
 * is output.
 * We use a slightly wider than "subjectively sane range" according
 * to https://trac.ffmpeg.org/wiki/Encode/H.264#a1.ChooseaCRFvalue
 */
typedef enum eFFMpegCrf {
  FFM_CRF_NONE = -1,
  FFM_CRF_LOSSLESS = 0,
  FFM_CRF_PERC_LOSSLESS = 17,
  FFM_CRF_HIGH = 20,
  FFM_CRF_MEDIUM = 23,
  FFM_CRF_LOW = 26,
  FFM_CRF_VERYLOW = 29,
  FFM_CRF_LOWEST = 32,
} eFFMpegCrf;

typedef enum eFFMpegAudioChannels {
  FFM_CHANNELS_MONO = 1,
  FFM_CHANNELS_STEREO = 2,
  FFM_CHANNELS_SURROUND4 = 4,
  FFM_CHANNELS_SURROUND51 = 6,
  FFM_CHANNELS_SURROUND71 = 8,
} eFFMpegAudioChannels;

typedef struct FFMpegCodecData {
  int type;
  int codec;
  int audio_codec;
  int video_bitrate;
  int audio_bitrate;
  int audio_mixrate;
  int audio_channels;
  float audio_volume;
  int gop_size;
  /** Only used if FFMPEG_USE_MAX_B_FRAMES flag is set. */
  int max_b_frames;
  int flags;
  int constant_rate_factor;
  /** See eFFMpegPreset. */
  int ffmpeg_preset;

  int rc_min_rate;
  int rc_max_rate;
  int rc_buffer_size;
  int mux_packet_size;
  int mux_rate;
  void *_pad1;
} FFMpegCodecData;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Audio
 * \{ */

typedef struct AudioData {
  int mixrate; /* 2.5: now in FFMpegCodecData: audio_mixrate. */
  float main;  /* 2.5: now in FFMpegCodecData: audio_volume. */
  float speed_of_sound;
  float doppler_factor;
  int distance_model;
  short flag;
  char _pad[2];
  float volume;
  char _pad2[4];
} AudioData;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Layers
 * \{ */

/** Render Layer. */
typedef struct SceneRenderLayer {
  struct SceneRenderLayer *next, *prev;

  /** MAX_NAME. */
  char name[64] DNA_DEPRECATED;

  /** Converted to ViewLayer setting. */
  struct Material *mat_override DNA_DEPRECATED;
  struct World *world_override DNA_DEPRECATED;

  /** Converted to LayerCollection cycles camera visibility override. */
  unsigned int lay DNA_DEPRECATED;
  /** Converted to LayerCollection cycles holdout override. */
  unsigned int lay_zmask DNA_DEPRECATED;
  unsigned int lay_exclude DNA_DEPRECATED;
  /** Converted to ViewLayer layflag and flag. */
  int layflag DNA_DEPRECATED;

  /* Pass_xor has to be after passflag. */
  /** Pass_xor has to be after passflag. */
  int passflag DNA_DEPRECATED;
  /** Converted to ViewLayer passflag and flag. */
  int pass_xor DNA_DEPRECATED;

  /** Converted to ViewLayer setting. */
  int samples DNA_DEPRECATED;
  /** Converted to ViewLayer pass_alpha_threshold. */
  float pass_alpha_threshold DNA_DEPRECATED;

  /** Converted to ViewLayer id_properties. */
  IDProperty *prop DNA_DEPRECATED;

  /** Converted to ViewLayer freestyleConfig. */
  struct FreestyleConfig freestyleConfig DNA_DEPRECATED;
} SceneRenderLayer;

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

  /* Flags between (1 << 9) and (1 << 15) are set to 1 already, for future options. */

  SCE_LAY_FLAG_DEFAULT = ((1 << 15) - 1),

  SCE_LAY_UNUSED_4 = 1 << 15,
  SCE_LAY_UNUSED_5 = 1 << 16,
  SCE_LAY_DISABLE = 1 << 17,
  SCE_LAY_UNUSED_6 = 1 << 18,
  SCE_LAY_UNUSED_7 = 1 << 19,
};

/** #SceneRenderLayer::passflag */
typedef enum eScenePassType {
  SCE_PASS_COMBINED = (1 << 0),
  SCE_PASS_Z = (1 << 1),
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
} eScenePassType;

#define RE_PASSNAME_DEPRECATED "Deprecated"

#define RE_PASSNAME_COMBINED "Combined"
#define RE_PASSNAME_Z "Depth"
#define RE_PASSNAME_VECTOR "Vector"
#define RE_PASSNAME_POSITION "Position"
#define RE_PASSNAME_NORMAL "Normal"
#define RE_PASSNAME_UV "UV"
#define RE_PASSNAME_EMIT "Emit"
#define RE_PASSNAME_SHADOW "Shadow"

#define RE_PASSNAME_AO "AO"
#define RE_PASSNAME_ENVIRONMENT "Env"
#define RE_PASSNAME_INDEXOB "IndexOB"
#define RE_PASSNAME_INDEXMA "IndexMA"
#define RE_PASSNAME_MIST "Mist"

#define RE_PASSNAME_DIFFUSE_DIRECT "DiffDir"
#define RE_PASSNAME_DIFFUSE_INDIRECT "DiffInd"
#define RE_PASSNAME_DIFFUSE_COLOR "DiffCol"
#define RE_PASSNAME_GLOSSY_DIRECT "GlossDir"
#define RE_PASSNAME_GLOSSY_INDIRECT "GlossInd"
#define RE_PASSNAME_GLOSSY_COLOR "GlossCol"
#define RE_PASSNAME_TRANSM_DIRECT "TransDir"
#define RE_PASSNAME_TRANSM_INDIRECT "TransInd"
#define RE_PASSNAME_TRANSM_COLOR "TransCol"

#define RE_PASSNAME_SUBSURFACE_DIRECT "SubsurfaceDir"
#define RE_PASSNAME_SUBSURFACE_INDIRECT "SubsurfaceInd"
#define RE_PASSNAME_SUBSURFACE_COLOR "SubsurfaceCol"

#define RE_PASSNAME_FREESTYLE "Freestyle"
#define RE_PASSNAME_BLOOM "BloomCol"
#define RE_PASSNAME_VOLUME_LIGHT "VolumeDir"
#define RE_PASSNAME_TRANSPARENT "Transp"

#define RE_PASSNAME_CRYPTOMATTE_OBJECT "CryptoObject"
#define RE_PASSNAME_CRYPTOMATTE_ASSET "CryptoAsset"
#define RE_PASSNAME_CRYPTOMATTE_MATERIAL "CryptoMaterial"

/** \} */

/* -------------------------------------------------------------------- */
/** \name Multi-View
 * \{ */

/** View (Multi-view). */
typedef struct SceneRenderView {
  struct SceneRenderView *next, *prev;

  /** MAX_NAME. */
  char name[64];
  /** MAX_NAME. */
  char suffix[64];

  int viewflag;
  char _pad2[4];

} SceneRenderView;

/** #SceneRenderView::viewflag */
enum {
  SCE_VIEW_DISABLE = 1 << 0,
};

/** #RenderData::views_format */
enum {
  SCE_VIEWS_FORMAT_STEREO_3D = 0,
  SCE_VIEWS_FORMAT_MULTIVIEW = 1,
};

/** #ImageFormatData::views_format (also used for #Sequence::views_format). */
enum {
  R_IMF_VIEWS_INDIVIDUAL = 0,
  R_IMF_VIEWS_STEREO_3D = 1,
  R_IMF_VIEWS_MULTIVIEW = 2,
};

typedef struct Stereo3dFormat {
  short flag;
  /** Encoding mode. */
  char display_mode;
  /** Anaglyph scheme for the user display. */
  char anaglyph_type;
  /** Interlace type for the user display. */
  char interlace_type;
  char _pad[3];
} Stereo3dFormat;

/** #Stereo3dFormat::display_mode */
typedef enum eStereoDisplayMode {
  S3D_DISPLAY_ANAGLYPH = 0,
  S3D_DISPLAY_INTERLACE = 1,
  S3D_DISPLAY_PAGEFLIP = 2,
  S3D_DISPLAY_SIDEBYSIDE = 3,
  S3D_DISPLAY_TOPBOTTOM = 4,
} eStereoDisplayMode;

/** #Stereo3dFormat::flag */
typedef enum eStereo3dFlag {
  S3D_INTERLACE_SWAP = (1 << 0),
  S3D_SIDEBYSIDE_CROSSEYED = (1 << 1),
  S3D_SQUEEZED_FRAME = (1 << 2),
} eStereo3dFlag;

/** #Stereo3dFormat::anaglyph_type */
typedef enum eStereo3dAnaglyphType {
  S3D_ANAGLYPH_REDCYAN = 0,
  S3D_ANAGLYPH_GREENMAGENTA = 1,
  S3D_ANAGLYPH_YELLOWBLUE = 2,
} eStereo3dAnaglyphType;

/** #Stereo3dFormat::interlace_type */
typedef enum eStereo3dInterlaceType {
  S3D_INTERLACE_ROW = 0,
  S3D_INTERLACE_COLUMN = 1,
  S3D_INTERLACE_CHECKERBOARD = 2,
} eStereo3dInterlaceType;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Format Data
 * \{ */

/**
 * Generic image format settings,
 * this is used for #NodeImageFile and IMAGE_OT_save_as operator too.
 *
 * NOTE: its a bit strange that even though this is an image format struct
 * the imtype can still be used to select video formats.
 * RNA ensures these enum's are only selectable for render output.
 */
typedef struct ImageFormatData {
  /**
   * R_IMF_IMTYPE_PNG, R_...
   * \note Video types should only ever be set from this structure when used from #RenderData.
   */
  char imtype;
  /**
   * bits per channel, R_IMF_CHAN_DEPTH_8 -> 32,
   * not a flag, only set 1 at a time. */
  char depth;

  /** R_IMF_PLANES_BW, R_IMF_PLANES_RGB, R_IMF_PLANES_RGBA. */
  char planes;
  /** Generic options for all image types, alpha Z-buffer. */
  char flag;

  /** (0 - 100), eg: JPEG quality. */
  char quality;
  /** (0 - 100), eg: PNG compression. */
  char compress;

  /* --- format specific --- */

  /** OpenEXR. */
  char exr_codec;

  /** CINEON. */
  char cineon_flag;
  short cineon_white, cineon_black;
  float cineon_gamma;

  /** Jpeg2000. */
  char jp2_flag;
  char jp2_codec;

  /** TIFF. */
  char tiff_codec;

  char _pad[4];

  /** Multi-view. */
  char views_format;
  Stereo3dFormat stereo3d_format;

  /* Color management members. */

  char color_management;
  char _pad1[7];
  ColorManagedViewSettings view_settings;
  ColorManagedDisplaySettings display_settings;
  ColorManagedColorspaceSettings linear_colorspace_settings;
} ImageFormatData;

/** #ImageFormatData::imtype */
enum {
  R_IMF_IMTYPE_TARGA = 0,
  R_IMF_IMTYPE_IRIS = 1,
  // R_HAMX = 2,  /* DEPRECATED */
  // R_FTYPE = 3, /* DEPRECATED */
  R_IMF_IMTYPE_JPEG90 = 4,
  // R_MOVIE = 5, /* DEPRECATED */
  R_IMF_IMTYPE_IRIZ = 7,
  R_IMF_IMTYPE_RAWTGA = 14,
  R_IMF_IMTYPE_AVIRAW = 15,
  R_IMF_IMTYPE_AVIJPEG = 16,
  R_IMF_IMTYPE_PNG = 17,
  // R_IMF_IMTYPE_AVICODEC = 18,  /* DEPRECATED */
  // R_IMF_IMTYPE_QUICKTIME = 19, /* DEPRECATED */
  R_IMF_IMTYPE_BMP = 20,
  R_IMF_IMTYPE_RADHDR = 21,
  R_IMF_IMTYPE_TIFF = 22,
  R_IMF_IMTYPE_OPENEXR = 23,
  R_IMF_IMTYPE_FFMPEG = 24,
  // R_IMF_IMTYPE_FRAMESERVER = 25, /* DEPRECATED */
  R_IMF_IMTYPE_CINEON = 26,
  R_IMF_IMTYPE_DPX = 27,
  R_IMF_IMTYPE_MULTILAYER = 28,
  R_IMF_IMTYPE_DDS = 29,
  R_IMF_IMTYPE_JP2 = 30,
  R_IMF_IMTYPE_H264 = 31,
  R_IMF_IMTYPE_XVID = 32,
  R_IMF_IMTYPE_THEORA = 33,
  R_IMF_IMTYPE_PSD = 34,
  R_IMF_IMTYPE_WEBP = 35,
  R_IMF_IMTYPE_AV1 = 36,

  R_IMF_IMTYPE_INVALID = 255,
};

/** #ImageFormatData::flag */
enum {
  // R_IMF_FLAG_ZBUF = 1 << 0, /* DEPRECATED, and cleared. */
  R_IMF_FLAG_PREVIEW_JPG = 1 << 1,
};

/*  */

/**
 * #ImageFormatData::depth
 *
 * Return values from #BKE_imtype_valid_depths, note this is depths per channel.
 */
typedef enum eImageFormatDepth {
  /** 1bits  (unused). */
  R_IMF_CHAN_DEPTH_1 = (1 << 0),
  /** 8bits  (default). */
  R_IMF_CHAN_DEPTH_8 = (1 << 1),
  /** 10bits (uncommon, Cineon/DPX support). */
  R_IMF_CHAN_DEPTH_10 = (1 << 2),
  /** 12bits (uncommon, jp2/DPX support). */
  R_IMF_CHAN_DEPTH_12 = (1 << 3),
  /** 16bits (TIFF, half float EXR). */
  R_IMF_CHAN_DEPTH_16 = (1 << 4),
  /** 24bits (unused). */
  R_IMF_CHAN_DEPTH_24 = (1 << 5),
  /** 32bits (full float EXR). */
  R_IMF_CHAN_DEPTH_32 = (1 << 6),
} eImageFormatDepth;

/** #ImageFormatData::planes */
enum {
  R_IMF_PLANES_RGB = 24,
  R_IMF_PLANES_RGBA = 32,
  R_IMF_PLANES_BW = 8,
};

/** #ImageFormatData::exr_codec */
enum {
  R_IMF_EXR_CODEC_NONE = 0,
  R_IMF_EXR_CODEC_PXR24 = 1,
  R_IMF_EXR_CODEC_ZIP = 2,
  R_IMF_EXR_CODEC_PIZ = 3,
  R_IMF_EXR_CODEC_RLE = 4,
  R_IMF_EXR_CODEC_ZIPS = 5,
  R_IMF_EXR_CODEC_B44 = 6,
  R_IMF_EXR_CODEC_B44A = 7,
  R_IMF_EXR_CODEC_DWAA = 8,
  R_IMF_EXR_CODEC_DWAB = 9,
  R_IMF_EXR_CODEC_MAX = 10,
};

/** #ImageFormatData::jp2_flag */
enum {
  /** When disabled use RGB. */
  R_IMF_JP2_FLAG_YCC = 1 << 0,         /* Was `R_JPEG2K_YCC`. */
  R_IMF_JP2_FLAG_CINE_PRESET = 1 << 1, /* Was `R_JPEG2K_CINE_PRESET`. */
  R_IMF_JP2_FLAG_CINE_48 = 1 << 2,     /* Was `R_JPEG2K_CINE_48FPS`. */
};

/** #ImageFormatData::jp2_codec */
enum {
  R_IMF_JP2_CODEC_JP2 = 0,
  R_IMF_JP2_CODEC_J2K = 1,
};

/** #ImageFormatData::cineon_flag */
enum {
  R_IMF_CINEON_FLAG_LOG = 1 << 0, /* Was `R_CINEON_LOG`. */
};

/** #ImageFormatData::tiff_codec */
enum {
  R_IMF_TIFF_CODEC_DEFLATE = 0,
  R_IMF_TIFF_CODEC_LZW = 1,
  R_IMF_TIFF_CODEC_PACKBITS = 2,
  R_IMF_TIFF_CODEC_NONE = 3,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Bake
 * \{ */

/** #ImageFormatData::color_management */
enum {
  R_IMF_COLOR_MANAGEMENT_FOLLOW_SCENE = 0,
  R_IMF_COLOR_MANAGEMENT_OVERRIDE = 1,
};

typedef struct BakeData {
  struct ImageFormatData im_format;

  /** FILE_MAX. */
  char filepath[1024];

  short width, height;
  short margin, flag;

  float cage_extrusion;
  float max_ray_distance;
  int pass_filter;

  char normal_swizzle[3];
  char normal_space;

  char target;
  char save_mode;
  char margin_type;
  char view_from;
  char _pad[4];

  struct Object *cage_object;
} BakeData;

/** #BakeData::margin_type (char). */
typedef enum eBakeMarginType {
  R_BAKE_ADJACENT_FACES = 0,
  R_BAKE_EXTEND = 1,
} eBakeMarginType;

/** #BakeData::normal_swizzle (char). */
typedef enum eBakeNormalSwizzle {
  R_BAKE_POSX = 0,
  R_BAKE_POSY = 1,
  R_BAKE_POSZ = 2,
  R_BAKE_NEGX = 3,
  R_BAKE_NEGY = 4,
  R_BAKE_NEGZ = 5,
} eBakeNormalSwizzle;

/** #BakeData::target (char). */
typedef enum eBakeTarget {
  R_BAKE_TARGET_IMAGE_TEXTURES = 0,
  R_BAKE_TARGET_VERTEX_COLORS = 1,
} eBakeTarget;

/** #BakeData::save_mode (char). */
typedef enum eBakeSaveMode {
  R_BAKE_SAVE_INTERNAL = 0,
  R_BAKE_SAVE_EXTERNAL = 1,
} eBakeSaveMode;

/** #BakeData::view_from (char). */
typedef enum eBakeViewFrom {
  R_BAKE_VIEW_FROM_ABOVE_SURFACE = 0,
  R_BAKE_VIEW_FROM_ACTIVE_CAMERA = 1,
} eBakeViewFrom;

/** #BakeData::pass_filter */
typedef enum eBakePassFilter {
  R_BAKE_PASS_FILTER_NONE = 0,
  R_BAKE_PASS_FILTER_UNUSED = (1 << 0),
  R_BAKE_PASS_FILTER_EMIT = (1 << 1),
  R_BAKE_PASS_FILTER_DIFFUSE = (1 << 2),
  R_BAKE_PASS_FILTER_GLOSSY = (1 << 3),
  R_BAKE_PASS_FILTER_TRANSM = (1 << 4),
  R_BAKE_PASS_FILTER_SUBSURFACE = (1 << 5),
  R_BAKE_PASS_FILTER_DIRECT = (1 << 6),
  R_BAKE_PASS_FILTER_INDIRECT = (1 << 7),
  R_BAKE_PASS_FILTER_COLOR = (1 << 8),
} eBakePassFilter;

#define R_BAKE_PASS_FILTER_ALL (~0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Data
 * \{ */

typedef struct RenderData {
  struct ImageFormatData im_format;

  struct AviCodecData *avicodecdata;
  struct FFMpegCodecData ffcodecdata;

  /** Frames as in 'images'. */
  int cfra, sfra, efra;
  /** Sub-frame offset from `cfra`, in 0.0-1.0. */
  float subframe;
  /** Start+end frames of preview range. */
  int psfra, pefra;

  int images, framapto;
  short flag, threads;

  float framelen;

  /** Frames to jump during render/playback. */
  int frame_step;

  /** For the dimensions presets menu. */
  short dimensionspreset;

  /** Size in %. */
  short size;

  /* From buttons: */
  /**
   * The desired number of pixels in the x direction
   */
  int xsch;
  /**
   * The desired number of pixels in the y direction
   */
  int ysch;

  /**
   * render tile dimensions
   */
  int tilex DNA_DEPRECATED;
  int tiley DNA_DEPRECATED;

  short planes DNA_DEPRECATED;
  short imtype DNA_DEPRECATED;
  short subimtype DNA_DEPRECATED;
  short quality DNA_DEPRECATED;

  char use_lock_interface;
  char _pad7[3];

  /**
   * Flags for render settings. Use bit-masking to access the settings.
   */
  int scemode;

  /**
   * Flags for render settings. Use bit-masking to access the settings.
   */
  int mode;

  short frs_sec;

  /**
   * What to do with the sky/background.
   * Picks sky/pre-multiply blending for the background.
   */
  char alphamode;

  char _pad0[1];

  /** Render border to render sub-regions. */
  rctf border;

  /* Information on different layers to be rendered. */
  /** Converted to Scene->view_layers. */
  ListBase layers DNA_DEPRECATED;
  /** Converted to Scene->active_layer. */
  short actlay DNA_DEPRECATED;
  char _pad1[2];

  /**
   * Adjustment factors for the aspect ratio in the x direction, was a short in 2.45
   */
  float xasp, yasp;

  float frs_sec_base;

  /**
   * Value used to define filter size for all filter options.
   */
  float gauss;

  /** Color management settings - color profiles, gamma correction, etc. */
  int color_mgt_flag;

  /** Dither noise intensity. */
  float dither_intensity;

  /* Bake Render options. */
  short bake_mode, bake_flag;
  short bake_margin, bake_samples;
  short bake_margin_type;
  char _pad9[6];
  float bake_biasdist, bake_user_scale;

  /* Path to render output. */
  /** 1024 = FILE_MAX. */
  /* NOTE: Excluded from `BKE_bpath_foreach_path_` / `scene_foreach_path` code. */
  char pic[1024];

  /** Stamps flags. */
  int stamp;
  /** Select one of blenders bitmap fonts. */
  short stamp_font_id;
  char _pad3[2];

  /** Stamp info user data. */
  char stamp_udata[768];

  /* Foreground/background color. */
  float fg_stamp[4];
  float bg_stamp[4];

  /** Sequencer options. */
  char seq_prev_type;
  /** UNUSED. */
  char seq_rend_type;
  /** Flag use for sequence render/draw. */
  char seq_flag;
  char _pad5[3];

  /* Render simplify. */
  short simplify_subsurf;
  short simplify_subsurf_render;
  short simplify_gpencil;
  float simplify_particles;
  float simplify_particles_render;
  float simplify_volumes;
  float simplify_shadows;
  float simplify_shadows_render;

  /** Freestyle line thickness options. */
  int line_thickness_mode;
  /** In pixels. */
  float unit_line_thickness;

  /** Render engine. */
  char engine[32];
  char _pad2[2];

  /** Performance Options. */
  short perf_flag;

  /** Cycles baking. */
  struct BakeData bake;

  int _pad8;
  short preview_pixel_size;

  short _pad4;

  /* MultiView. */
  /** SceneRenderView. */
  ListBase views;
  short actview;
  short views_format;

  /* Hair Display. */
  short hair_type, hair_subdiv;

  /** Motion blur */
  float motion_blur_shutter;
  int motion_blur_position;
  struct CurveMapping mblur_shutter_curve;
} RenderData;

/** #RenderData::quality_flag */
typedef enum eQualityOption {
  SCE_PERF_HQ_NORMALS = (1 << 0),
} eQualityOption;

/** #RenderData::hair_type */
typedef enum eHairType {
  SCE_HAIR_SHAPE_STRAND = 0,
  SCE_HAIR_SHAPE_STRIP = 1,
} eHairType;

/** #RenderData::motion_blur_position */
enum {
  SCE_MB_CENTER = 0,
  SCE_MB_START = 1,
  SCE_MB_END = 2,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Conversion/Simplification Settings
 * \{ */

/** Control render convert and shading engine. */
typedef struct RenderProfile {
  struct RenderProfile *next, *prev;
  char name[32];

  short particle_perc;
  short subsurf_max;
  short shadbufsample_max;
  char _pad1[2];

  float ao_error;
  char _pad2[4];

} RenderProfile;

/* UV Paint. */
/** #ToolSettings::uv_sculpt_settings */
enum {
  UV_SCULPT_LOCK_BORDERS = 1,
  UV_SCULPT_ALL_ISLANDS = 2,
};

/** #ToolSettings::uv_relax_method */
enum {
  UV_SCULPT_TOOL_RELAX_LAPLACIAN = 1,
  UV_SCULPT_TOOL_RELAX_HC = 2,
  UV_SCULPT_TOOL_RELAX_COTAN = 3,
};

/* Stereo Flags. */
#define STEREO_RIGHT_NAME "right"
#define STEREO_LEFT_NAME "left"
#define STEREO_RIGHT_SUFFIX "_R"
#define STEREO_LEFT_SUFFIX "_L"

/** #View3D::stereo3d_camera / #View3D::multiview_eye / #ImageUser::multiview_eye */
typedef enum eStereoViews {
  STEREO_LEFT_ID = 0,
  STEREO_RIGHT_ID = 1,
  STEREO_3D_ID = 2,
  STEREO_MONO_ID = 3,
} eStereoViews;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Time Line Markers
 * \{ */

typedef struct TimeMarker {
  struct TimeMarker *next, *prev;
  int frame;
  char name[64];
  unsigned int flag;
  struct Object *camera;
  struct IDProperty *prop;
} TimeMarker;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Mode/Tool Data
 * \{ */

#define PAINT_MAX_INPUT_SAMPLES 64

typedef struct Paint_Runtime {
  /** Avoid having to compare with scene pointer everywhere. */
  unsigned int tool_offset;
  unsigned short ob_mode;
  char _pad[2];
} Paint_Runtime;

/** We might want to store other things here. */
typedef struct PaintToolSlot {
  struct Brush *brush;
} PaintToolSlot;

/** Paint Tool Base. */
typedef struct Paint {
  struct Brush *brush;

  /**
   * Each tool has its own active brush,
   * The currently active tool is defined by the current 'brush'.
   */
  struct PaintToolSlot *tool_slots;
  int tool_slots_len;
  char _pad1[4];

  struct Palette *palette;
  /** Cavity curve. */
  struct CurveMapping *cavity_curve;

  /** WM Paint cursor. */
  void *paint_cursor;
  unsigned char paint_cursor_col[4];

  /** Enum #ePaintFlags. */
  int flags;

  /**
   * Paint stroke can use up to #PAINT_MAX_INPUT_SAMPLES inputs to smooth the stroke.
   * This value is deprecated. Refer to the #Brush and #UnifiedPaintSetting values instead.
   */
  int num_input_samples_deprecated;

  /** Flags used for symmetry. */
  int symmetry_flags;

  float tile_offset[3];
  char _pad2[4];

  struct Paint_Runtime runtime;
} Paint;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Paint
 * \{ */

/** Texture/Image Editor. */
typedef struct ImagePaintSettings {
  Paint paint;

  short flag, missing_data;

  /** For projection painting only. */
  short seam_bleed, normal_angle;
  /** Capture size for re-projection. */
  short screen_grab_size[2];

  /** Mode used for texture painting. */
  int mode;

  /** Workaround until we support true layer masks. */
  struct Image *stencil;
  /** Clone layer for image mode for projective texture painting. */
  struct Image *clone;
  /** Canvas when the explicit system is used for painting. */
  struct Image *canvas;
  float stencil_col[3];
  /** Dither amount used when painting on byte images. */
  float dither;
  /** Display texture interpolation method. */
  int interp;
  char _pad[4];
} ImagePaintSettings;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Mode Settings
 * \{ */

typedef struct PaintModeSettings {
  /** Source to select canvas from to paint on (#ePaintCanvasSource). */
  char canvas_source;
  char _pad[7];

  /** Selected image when canvas_source=PAINT_CANVAS_SOURCE_IMAGE. */
  Image *canvas_image;
  ImageUser image_user;

} PaintModeSettings;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle Edit
 * \{ */

/** Settings for a Particle Editing Brush. */
typedef struct ParticleBrushData {
  /** Common setting. */
  short size;
  /** For specific brushes only. */
  short step, invert, count;
  int flag;
  float strength;
} ParticleBrushData;

/** Particle Edit Mode Settings. */
typedef struct ParticleEditSettings {
  short flag;
  short totrekey;
  short totaddkey;
  short brushtype;

  ParticleBrushData brush[7];
  /** Runtime. */
  void *paintcursor;

  float emitterdist;
  char _pad0[4];

  int selectmode;
  int edittype;

  int draw_step, fade_frames;

  struct Scene *scene;
  struct Object *object;
  struct Object *shape_object;
} ParticleEditSettings;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt
 * \{ */

/* ------------------------------------------- */
/** Sculpt. */
typedef struct Sculpt {
  Paint paint;

  /** For rotating around a pivot point. */
  // float pivot[3]; XXX not used?
  int flags;

  /** Transform tool. */
  int transform_mode;

  int automasking_flags;

  // /* Control tablet input. */
  // char tablet_size, tablet_strength; XXX not used?
  int radial_symm[3];

  /** Maximum edge length for dynamic topology sculpting (in pixels). */
  float detail_size DNA_DEPRECATED;

  /** Direction used for `SCULPT_OT_symmetrize` operator. */
  int symmetrize_direction;

  /** Gravity factor for sculpting. */
  float gravity_factor;

  /* Scale for constant detail size. */
  /** Constant detail resolution (Blender unit / constant_detail). */
  float constant_detail DNA_DEPRECATED;
  float detail_percent DNA_DEPRECATED;

  int automasking_boundary_edges_propagation_steps;
  int automasking_cavity_blur_steps;
  float automasking_cavity_factor;

  float automasking_start_normal_limit, automasking_start_normal_falloff;
  float automasking_view_normal_limit, automasking_view_normal_falloff;

  struct CurveMapping *automasking_cavity_curve;
  /** For use by operators. */
  struct CurveMapping *automasking_cavity_curve_op;
  struct Object *gravity_object;

  DynTopoSettings dyntopo;
} Sculpt;

typedef struct CurvesSculpt {
  Paint paint;
} CurvesSculpt;

typedef struct UvSculpt {
  Paint paint;
} UvSculpt;

/** Grease pencil drawing brushes. */
typedef struct GpPaint {
  Paint paint;
  int flag;
  /** Mode of paint (Materials or Vertex Color). */
  int mode;
} GpPaint;

/** #GpPaint::flag */
enum {
  GPPAINT_FLAG_USE_MATERIAL = 0,
  GPPAINT_FLAG_USE_VERTEXCOLOR = 1,
};

/** Grease pencil vertex paint. */
typedef struct GpVertexPaint {
  Paint paint;
  int flag;
  char _pad[4];
} GpVertexPaint;

/** Grease pencil sculpt paint. */
typedef struct GpSculptPaint {
  Paint paint;
  int flag;
  char _pad[4];
} GpSculptPaint;

/** Grease pencil weight paint. */
typedef struct GpWeightPaint {
  Paint paint;
  int flag;
  char _pad[4];
} GpWeightPaint;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Paint
 * \{ */

/** Vertex Paint. */
typedef struct VPaint {
  Paint paint;
  char flag;
  char _pad[3];
  /** For mirrored painting. */
  int radial_symm[3];
} VPaint;

/** #VPaint::flag */
enum {
  /** Weight paint only. */
  VP_FLAG_VGROUP_RESTRICT = (1 << 7),
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease-Pencil Stroke Sculpting
 * \{ */

/** #GP_Sculpt_Settings::lock_axis */
typedef enum eGP_Lockaxis_Types {
  GP_LOCKAXIS_VIEW = 0,
  GP_LOCKAXIS_X = 1,
  GP_LOCKAXIS_Y = 2,
  GP_LOCKAXIS_Z = 3,
  GP_LOCKAXIS_CURSOR = 4,
} eGP_Lockaxis_Types;

/** Settings for a GPencil Speed Guide. */
typedef struct GP_Sculpt_Guide {
  char use_guide;
  char use_snapping;
  char reference_point;
  char type;
  char _pad2[4];
  float angle;
  float angle_snap;
  float spacing;
  float location[3];
  struct Object *reference_object;
} GP_Sculpt_Guide;

/** GPencil Stroke Sculpting Settings. */
typedef struct GP_Sculpt_Settings {
  /** Runtime. */
  void *paintcursor;
  /** #eGP_Sculpt_SettingsFlag. */
  int flag;
  /** #eGP_Lockaxis_Types lock drawing to one axis. */
  int lock_axis;
  /** Threshold for intersections. */
  float isect_threshold;
  char _pad[4];
  /** Multi-frame edit falloff effect by frame. */
  struct CurveMapping *cur_falloff;
  /** Curve used for primitive tools. */
  struct CurveMapping *cur_primitive;
  /** Guides used for paint tools. */
  struct GP_Sculpt_Guide guide;
} GP_Sculpt_Settings;

/** #GP_Sculpt_Settings::flag */
typedef enum eGP_Sculpt_SettingsFlag {
  /** Enable falloff for multi-frame editing. */
  GP_SCULPT_SETT_FLAG_FRAME_FALLOFF = (1 << 0),
  /** Apply primitive curve. */
  GP_SCULPT_SETT_FLAG_PRIMITIVE_CURVE = (1 << 1),
  /** Scale thickness. */
  GP_SCULPT_SETT_FLAG_SCALE_THICKNESS = (1 << 3),
  /** Stroke Auto-Masking for sculpt. */
  GP_SCULPT_SETT_FLAG_AUTOMASK_STROKE = (1 << 4),
  /** Stroke Layer Auto-Masking for sculpt. */
  GP_SCULPT_SETT_FLAG_AUTOMASK_LAYER_STROKE = (1 << 5),
  /** Stroke Material Auto-Masking for sculpt. */
  GP_SCULPT_SETT_FLAG_AUTOMASK_MATERIAL_STROKE = (1 << 6),
  /** Active Layer Auto-Masking for sculpt. */
  GP_SCULPT_SETT_FLAG_AUTOMASK_LAYER_ACTIVE = (1 << 7),
  /** Active Material Auto-Masking for sculpt. */
  GP_SCULPT_SETT_FLAG_AUTOMASK_MATERIAL_ACTIVE = (1 << 8),
} eGP_Sculpt_SettingsFlag;

/** #GP_Sculpt_Settings::gpencil_selectmode_sculpt */
typedef enum eGP_Sculpt_SelectMaskFlag {
  /** Only affect selected points. */
  GP_SCULPT_MASK_SELECTMODE_POINT = (1 << 0),
  /** Only affect selected strokes. */
  GP_SCULPT_MASK_SELECTMODE_STROKE = (1 << 1),
  /** Only affect selected segments. */
  GP_SCULPT_MASK_SELECTMODE_SEGMENT = (1 << 2),
} eGP_Sculpt_SelectMaskFlag;

/** #GP_Sculpt_Settings::gpencil_selectmode_vertex */
typedef enum eGP_vertex_SelectMaskFlag {
  /** Only affect selected points. */
  GP_VERTEX_MASK_SELECTMODE_POINT = (1 << 0),
  /** Only affect selected strokes. */
  GP_VERTEX_MASK_SELECTMODE_STROKE = (1 << 1),
  /** Only affect selected segments. */
  GP_VERTEX_MASK_SELECTMODE_SEGMENT = (1 << 2),
} eGP_Vertex_SelectMaskFlag;

/** Settings for GP Interpolation Operators. */
typedef struct GP_Interpolate_Settings {
  /** Custom interpolation curve (for use with GP_IPO_CURVEMAP). */
  struct CurveMapping *custom_ipo;
} GP_Interpolate_Settings;

/** #GP_Interpolate_Settings::flag */
typedef enum eGP_Interpolate_SettingsFlag {
  /** Apply interpolation to all layers. */
  GP_TOOLFLAG_INTERPOLATE_ALL_LAYERS = (1 << 0),
  /** Apply interpolation to only selected. */
  GP_TOOLFLAG_INTERPOLATE_ONLY_SELECTED = (1 << 1),
  /** Exclude breakdown keyframe type as extreme. */
  GP_TOOLFLAG_INTERPOLATE_EXCLUDE_BREAKDOWNS = (1 << 2),
} eGP_Interpolate_SettingsFlag;

/** #GP_Interpolate_Settings::type */
typedef enum eGP_Interpolate_Type {
  /** Traditional Linear Interpolation. */
  GP_IPO_LINEAR = 0,

  /** CurveMap Defined Interpolation. */
  GP_IPO_CURVEMAP = 1,

  /* Easing Equations. */
  GP_IPO_BACK = 3,
  GP_IPO_BOUNCE = 4,
  GP_IPO_CIRC = 5,
  GP_IPO_CUBIC = 6,
  GP_IPO_ELASTIC = 7,
  GP_IPO_EXPO = 8,
  GP_IPO_QUAD = 9,
  GP_IPO_QUART = 10,
  GP_IPO_QUINT = 11,
  GP_IPO_SINE = 12,
} eGP_Interpolate_Type;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unified Paint Settings
 * \{ */

/**
 * These settings can override the equivalent fields in the active
 * Brush for any paint mode; the flag field controls whether these
 * values are used
 */
typedef struct UnifiedPaintSettings {
  /** Unified radius of brush in pixels. */
  int size;

  /** Unified radius of brush in Blender units. */
  float unprojected_radius;

  /** Unified strength of brush. */
  float alpha;

  /** Unified brush weight, [0, 1]. */
  float weight;

  /** Unified brush color. */
  float rgb[3];
  /** Unified brush secondary color. */
  float secondary_rgb[3];

  /** Unified brush stroke input samples. */
  int input_samples;

  /** User preferences for sculpt and paint. */
  int flag;
  char _pad[4];

  /* Rake rotation. */

  /** Record movement of mouse so that rake can start at an intuitive angle. */
  float last_rake[2];
  float last_rake_angle;

  int last_stroke_valid;
  float average_stroke_accum[3];
  int average_stroke_counter;

  /* How much brush should be rotated in the view plane, 0 means x points right, y points up.
   * The convention is that the brush's _negative_ Y axis points in the tangent direction (of the
   * mouse curve, Bezier curve, etc.) */
  float brush_rotation;
  float brush_rotation_sec;

  /*******************************************************************************
   * all data below are used to communicate with cursor drawing and tex sampling *
   *******************************************************************************/
  int anchored_size;

  /**
   * Normalization factor due to accumulated value of curve along spacing.
   * Calculated when brush spacing changes to dampen strength of stroke
   * if space attenuation is used.
   */
  float overlap_factor;
  char draw_inverted;
  /** Check is there an ongoing stroke right now. */
  char stroke_active;

  char draw_anchored;
  char do_linear_conversion;

  /**
   * Store last location of stroke or whether the mesh was hit.
   * Valid only while stroke is active.
   */
  float last_location[3];
  int last_hit;

  float anchored_initial_mouse[2];

  /**
   * Radius of brush, pre-multiplied with pressure.
   * In case of anchored brushes contains the anchored radius.
   */
  float pixel_radius;
  float initial_pixel_radius;

  float hard_corner_pin;
  float sharp_angle_limit;
  char _pad2[2];

  char distort_correction_mode; /* eAttrCorrectMode bit mask. */
  char hard_edge_mode DNA_DEPRECATED;
  int smooth_boundary_flag;

  float start_pixel_radius;

  /** Drawing pressure. */
  float size_pressure_value;

  /** Position of mouse, used to sample the texture. */
  float tex_mouse[2];

  /** Position of mouse, used to sample the mask texture. */
  float mask_tex_mouse[2];

  /** ColorSpace cache to avoid locking up during sampling. */
  struct ColorSpace *colorspace;
} UnifiedPaintSettings;

/** #UnifiedPaintSettings::flag */
typedef enum {
  UNIFIED_PAINT_SIZE = (1 << 0),
  UNIFIED_PAINT_ALPHA = (1 << 1),

  /** Only used if unified size is enabled, mirrors the brush flag #BRUSH_LOCK_SIZE. */
  UNIFIED_PAINT_BRUSH_LOCK_SIZE = (1 << 2),
  UNIFIED_PAINT_FLAG_UNUSED_0 = (1 << 3),
  UNIFIED_PAINT_FLAG_UNUSED_1 = (1 << 4),
  UNIFIED_PAINT_WEIGHT = (1 << 5),
  UNIFIED_PAINT_COLOR = (1 << 6),
  UNIFIED_PAINT_INPUT_SAMPLES = (1 << 7),
  UNIFIED_PAINT_HARD_CORNER_PIN = (1 << 10),
  UNIFIED_PAINT_FLAG_HARD_EDGE_MODE = (1 << 11),
  UNIFIED_PAINT_FLAG_SHARP_ANGLE_LIMIT = (1 << 12),
} eUnifiedPaintSettingsFlags;

typedef struct CurvePaintSettings {
  char curve_type;
  char flag;
  char depth_mode;
  char surface_plane;
  char fit_method;
  char _pad;
  short error_threshold;
  float radius_min, radius_max;
  float radius_taper_start, radius_taper_end;
  float surface_offset;
  float corner_angle;
} CurvePaintSettings;

/** #CurvePaintSettings::flag */
enum {
  CURVE_PAINT_FLAG_CORNERS_DETECT = (1 << 0),
  CURVE_PAINT_FLAG_PRESSURE_RADIUS = (1 << 1),
  CURVE_PAINT_FLAG_DEPTH_STROKE_ENDPOINTS = (1 << 2),
  CURVE_PAINT_FLAG_DEPTH_STROKE_OFFSET_ABS = (1 << 3),
};

/** #CurvePaintSettings::fit_method */
enum {
  CURVE_PAINT_FIT_METHOD_REFIT = 0,
  CURVE_PAINT_FIT_METHOD_SPLIT = 1,
};

/** #CurvePaintSettings::depth_mode */
enum {
  CURVE_PAINT_PROJECT_CURSOR = 0,
  CURVE_PAINT_PROJECT_SURFACE = 1,
};

/** #CurvePaintSettings::surface_plane */
enum {
  CURVE_PAINT_SURFACE_PLANE_NORMAL_VIEW = 0,
  CURVE_PAINT_SURFACE_PLANE_NORMAL_SURFACE = 1,
  CURVE_PAINT_SURFACE_PLANE_VIEW = 2,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Visualization
 * \{ */

/** Stats for Meshes. */
typedef struct MeshStatVis {
  char type;
  char _pad1[2];

  /* Overhang. */
  char overhang_axis;
  float overhang_min, overhang_max;

  /* Thickness. */
  float thickness_min, thickness_max;
  char thickness_samples;
  char _pad2[3];

  /* Distort. */
  float distort_min, distort_max;

  /* Sharp. */
  float sharp_min, sharp_max;
} MeshStatVis;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequencer Tool Settings
 * \{ */

typedef struct SequencerToolSettings {
  /** #eSeqImageFitMethod. */
  int fit_method;
  short snap_mode;
  short snap_flag;
  /** #eSeqOverlapMode. */
  int overlap_mode;
  /**
   * When there are many snap points,
   * 0-1 range corresponds to resolution from bound-box to all possible snap points.
   */
  int snap_distance;
  int pivot_point;
} SequencerToolSettings;

typedef enum eSeqOverlapMode {
  SEQ_OVERLAP_EXPAND,
  SEQ_OVERLAP_OVERWRITE,
  SEQ_OVERLAP_SHUFFLE,
} eSeqOverlapMode;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tool Settings
 * \{ */

/** #CurvePaintSettings::surface_plane */
enum {
  AUTO_MERGE = 1 << 0,
  AUTO_MERGE_AND_SPLIT = 1 << 1,
};

typedef struct ToolSettings {
  /** Vertex paint. */
  VPaint *vpaint;
  /** Weight paint. */
  VPaint *wpaint;
  Sculpt *sculpt;
  /** UV smooth. */
  UvSculpt *uvsculpt;
  /** Gpencil paint. */
  GpPaint *gp_paint;
  /** Gpencil vertex paint. */
  GpVertexPaint *gp_vertexpaint;
  /** Gpencil sculpt paint. */
  GpSculptPaint *gp_sculptpaint;
  /** Gpencil weight paint. */
  GpWeightPaint *gp_weightpaint;
  /** Curves sculpt. */
  CurvesSculpt *curves_sculpt;

  /** Vertex group weight - used only for editmode, not weight paint. */
  float vgroup_weight;

  /** Remove doubles limit. */
  float doublimit;
  char automerge;
  char object_flag;

  /** Selection Mode for Mesh. */
  char selectmode;

  /* UV Calculation. */
  char unwrapper;
  char uvcalc_flag;
  char uv_flag;
  char uv_selectmode;
  char uv_sticky;

  float uvcalc_margin;

  /* Auto-IK. */
  /** Runtime only. */
  short autoik_chainlen;

  /* Grease Pencil. */
  /** Flags/options for how the tool works. */
  char gpencil_flags;

  /** Stroke placement settings: 3D View. */
  char gpencil_v3d_align;
  /** General 2D Editor. */
  char gpencil_v2d_align;

  /* Annotations. */
  /** Stroke placement settings - 3D View. */
  char annotate_v3d_align;
  /** Default stroke thickness for annotation strokes. */
  short annotate_thickness;

  /** Normal offset used when drawing on surfaces. */
  float gpencil_surface_offset;

  /** Stroke selection mode for Edit. */
  char gpencil_selectmode_edit;
  /** Stroke selection mode for Sculpt. */
  char gpencil_selectmode_sculpt;
  char _pad0[6];

  /** Grease Pencil Sculpt. */
  struct GP_Sculpt_Settings gp_sculpt;

  /** Grease Pencil Interpolation Tool(s). */
  struct GP_Interpolate_Settings gp_interpolate;

  /** Image Paint (8 bytes aligned please!). */
  struct ImagePaintSettings imapaint;

  /** Settings for paint mode. */
  struct PaintModeSettings paint_mode;

  /** Particle Editing. */
  struct ParticleEditSettings particle;

  /** Transform Proportional Area of Effect. */
  float proportional_size;

  /** Select Group Threshold. */
  float select_thresh;

  /* Keying Settings. */
  /** Defines in DNA_userdef_types.h. */
  short keying_flag;
  char autokey_mode;
  /** Keyframe type (see DNA_curve_types.h). */
  char keyframe_type;

  /** Multi-resolution meshes. */
  char multires_subdiv_type;

  /** Edge tagging, store operator settings (no UI access). */
  char edge_mode;

  char edge_mode_live_unwrap;

  /* Transform. */

  char transform_pivot_point;
  char transform_flag;
  /** Snap elements (per space-type), #eSnapMode. */
  char snap_node_mode;
  short snap_mode;
  short snap_uv_mode;
  short snap_anim_mode;
  /** Generic flags (per space-type), #eSnapFlag. */
  short snap_flag;
  short snap_flag_node;
  short snap_flag_seq;
  short snap_flag_anim;
  short snap_uv_flag;
  char _pad[4];
  /** Default snap source, #eSnapSourceOP. */
  /**
   * TODO(@gfxcoder): Rename `snap_target` to `snap_source` to avoid previous ambiguity of
   * "target" (now, "source" is geometry to be moved and "target" is geometry to which moved
   * geometry is snapped).
   */
  char snap_target;
  /** Snap mask for transform modes, #eSnapTransformMode. */
  char snap_transform_mode_flag;
  /** Steps to break transformation into with face nearest snapping. */
  short snap_face_nearest_steps;

  char proportional_edit, prop_mode;
  /** Proportional edit, object mode. */
  char proportional_objects;
  /** Proportional edit, mask editing. */
  char proportional_mask;
  /** Proportional edit, action editor. */
  char proportional_action;
  /** Proportional edit, graph editor. */
  char proportional_fcurve;
  /** Lock marker editing. */
  char lock_markers;

  /** Auto normalizing mode in wpaint. */
  char auto_normalize;
  /** Present weights as if all locked vertex groups were
   *  deleted, and the remaining deform groups normalized. */
  char wpaint_lock_relative;
  /** Paint multiple bones in wpaint. */
  char multipaint;
  char weightuser;
  /** Subset selection filter in wpaint. */
  char vgroupsubset;

  /** Stroke selection mode for Vertex Paint. */
  char gpencil_selectmode_vertex;

  /* UV painting. */
  char uv_sculpt_settings;
  char uv_relax_method;

  char workspace_tool_type;

  /**
   * XXX: these `sculpt_paint_*` fields are deprecated, use the
   * unified_paint_settings field instead!
   */
  short sculpt_paint_settings DNA_DEPRECATED;
  int sculpt_paint_unified_size DNA_DEPRECATED;
  float sculpt_paint_unified_unprojected_radius DNA_DEPRECATED;
  float sculpt_paint_unified_alpha DNA_DEPRECATED;

  /** Unified Paint Settings. */
  struct UnifiedPaintSettings unified_paint_settings;

  struct CurvePaintSettings curve_paint_settings;

  struct MeshStatVis statvis;

  /** Normal Editing. */
  float normal_vector[3];

  /* NotForPR: Show original coordinates from start of sculpt stroke.*/
  char show_origco;

  char _pad6[3];

  /**
   * Custom Curve Profile for bevel tool:
   * Temporary until there is a proper preset system that stores the profiles or maybe stores
   * entire bevel configurations.
   */
  struct CurveProfile *custom_bevel_profile_preset;

  struct SequencerToolSettings *sequencer_tool_settings;

  short snap_mode_tools; /* If SCE_SNAP_TO_NONE, use #ToolSettings::snap_mode. #eSnapMode. */
  char plane_axis;       /* X, Y or Z. */
  char plane_depth;      /* #eV3DPlaceDepth. */
  char plane_orient;     /* #eV3DPlaceOrient. */
  char use_plane_axis_auto;
  char _pad7[2];

  /** Rotation Angle snapping amount */
  float snap_angle_increment_2d;
  float snap_angle_increment_2d_precision;
  float snap_angle_increment_3d;
  float snap_angle_increment_3d_precision;

} ToolSettings;

/** \} */

/* Assorted Scene Data. */

/* -------------------------------------------------------------------- */
/** \name Unit Settings
 * \{ */

/** Display/Editing unit options for each scene. */
typedef struct UnitSettings {

  /** Maybe have other unit conversions? */
  float scale_length;
  /** Imperial, metric etc. */
  char system;
  /** Not implemented as a proper unit system yet. */
  char system_rotation;
  short flag;

  char length_unit;
  char mass_unit;
  char time_unit;
  char temperature_unit;

  char _pad[4];
} UnitSettings;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Global/Common Physics Settings
 * \{ */

typedef struct PhysicsSettings {
  float gravity[3];
  int flag, quick_cache_step;
  char _pad0[4];
} PhysicsSettings;

/**
 * Safe Area options used in Camera View & Sequencer.
 */
typedef struct DisplaySafeAreas {
  /* Each value represents the (x,y) margins as a multiplier.
   * 'center' in this context is just the name for a different kind of safe-area. */

  /** Title Safe. */
  float title[2];
  /** Image/Graphics Safe. */
  float action[2];

  /* Use for alternate aspect ratio. */
  float title_center[2];
  float action_center[2];
} DisplaySafeAreas;

/**
 * Scene Display - used for store scene specific display settings for the 3d view.
 */
typedef struct SceneDisplay {
  /** Light direction for shadows/highlight. */
  float light_direction[3];
  float shadow_shift, shadow_focus;

  /** Settings for Cavity Shader. */
  float matcap_ssao_distance;
  float matcap_ssao_attenuation;
  int matcap_ssao_samples;

  /** Method of AA for viewport rendering and image rendering. */
  char viewport_aa;
  char render_aa;
  char _pad[6];

  /** OpenGL render engine settings. */
  View3DShading shading;
} SceneDisplay;

/**
 * Ray-tracing parameters.
 */
typedef struct RaytraceEEVEE {
  /** Higher values will take lower strides and have less blurry intersections. */
  float screen_trace_quality;
  /** Thickness in world space each surface will have during screen space tracing. */
  float screen_trace_thickness;
  /** Maximum roughness before using horizon scan. */
  float screen_trace_max_roughness;
  /** Resolution downscale factor. */
  int resolution_scale;
  /** Maximum intensity a ray can have. */
  float sample_clamp;
  /** #RaytraceEEVEE_Flag. */
  int flag;
  /** #RaytraceEEVEE_DenoiseStages. */
  int denoise_stages;

  char _pad0[4];
} RaytraceEEVEE;

typedef struct SceneEEVEE {
  int flag;
  int gi_diffuse_bounces;
  int gi_cubemap_resolution;
  int gi_visibility_resolution;
  float gi_irradiance_smoothing;
  float gi_glossy_clamp;
  float gi_filter_quality;
  int gi_irradiance_pool_size;

  float gi_cubemap_draw_size;
  float gi_irradiance_draw_size;

  int taa_samples;
  int taa_render_samples;
  int sss_samples;
  float sss_jitter_threshold;

  float ssr_quality;
  float ssr_max_roughness;
  float ssr_thickness;
  float ssr_border_fade;
  float ssr_firefly_fac;

  float volumetric_start;
  float volumetric_end;
  int volumetric_tile_size;
  int volumetric_samples;
  float volumetric_sample_distribution;
  float volumetric_light_clamp;
  int volumetric_shadow_samples;
  int volumetric_ray_depth;

  float gtao_distance;
  float gtao_factor;
  float gtao_quality;
  float gtao_thickness;
  float gtao_focus;

  float bokeh_overblur;
  float bokeh_max_size;
  float bokeh_threshold;
  float bokeh_neighbor_max;
  float bokeh_denoise_fac;

  float bloom_color[3];
  float bloom_threshold;
  float bloom_knee;
  float bloom_intensity;
  float bloom_radius;
  float bloom_clamp;

  int motion_blur_samples DNA_DEPRECATED;
  int motion_blur_max;
  int motion_blur_steps;
  int motion_blur_position_deprecated DNA_DEPRECATED;
  float motion_blur_shutter_deprecated DNA_DEPRECATED;
  float motion_blur_depth_scale;

  int shadow_method DNA_DEPRECATED;
  int shadow_cube_size;
  int shadow_cascade_size;
  int shadow_pool_size;
  int shadow_ray_count;
  int shadow_step_count;
  float shadow_normal_bias;
  float _pad0;

  int ray_tracing_method;

  struct RaytraceEEVEE ray_tracing_options;

  struct LightCache *light_cache DNA_DEPRECATED;
  struct LightCache *light_cache_data;
  /* Need a 128 byte string for some translations of some messages. */
  char light_cache_info[128];

  float overscan;
  float light_threshold;
} SceneEEVEE;

typedef struct SceneGpencil {
  float smaa_threshold;
  char _pad[4];
} SceneGpencil;

typedef struct SceneHydra {
  int export_method;
  int _pad0;
} SceneHydra;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Orientation
 * \{ */

typedef struct TransformOrientationSlot {
  int type;
  int index_custom;
  char flag;
  char _pad0[7];
} TransformOrientationSlot;

/** Indices when used in #Scene::orientation_slots. */
enum {
  SCE_ORIENT_DEFAULT = 0,
  SCE_ORIENT_TRANSLATE = 1,
  SCE_ORIENT_ROTATE = 2,
  SCE_ORIENT_SCALE = 3,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene ID-Block
 * \{ */

typedef struct Scene {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;
  /**
   * Engines draw data, must be immediately after AnimData. See IdDdtTemplate and
   * DRW_drawdatalist_from_id to understand this requirement.
   */
  DrawDataList drawdata;

  struct Object *camera;
  struct World *world;

  struct Scene *set;

  ListBase base DNA_DEPRECATED;
  /** Active base. */
  struct Base *basact DNA_DEPRECATED;
  void *_pad1;

  /** 3d cursor location. */
  View3DCursor cursor;

  /** Bit-flags for layer visibility (deprecated). */
  unsigned int lay DNA_DEPRECATED;
  /** Active layer (deprecated). */
  int layact DNA_DEPRECATED;
  char _pad2[4];

  /** Various settings. */
  short flag;

  char use_nodes;
  char _pad3[1];

  struct bNodeTree *nodetree;

  /** Sequence editor data is allocated here. */
  struct Editing *ed;

  /** Default allocated now. */
  struct ToolSettings *toolsettings;
  void *_pad4;
  struct DisplaySafeAreas safe_areas;

  /* Migrate or replace? depends on some internal things... */
  /* No, is on the right place (ton). */
  struct RenderData r;
  struct AudioData audio;

  ListBase markers;
  ListBase transform_spaces;

  /** First is the [scene, translate, rotate, scale]. */
  TransformOrientationSlot orientation_slots[4];

  void *sound_scene;
  void *playback_handle;
  void *sound_scrub_handle;
  void *speaker_handles;

  /** (runtime) info/cache used for presenting playback frame-rate info to the user. */
  void *fps_info;

  /** None of the dependency graph vars is mean to be saved. */
  struct GHash *depsgraph_hash;
  char _pad7[4];

  /* User-Defined KeyingSets. */
  /**
   * Index of the active KeyingSet.
   * first KeyingSet has index 1, 'none' active is 0, 'add new' is -1
   */
  int active_keyingset;
  /** KeyingSets for this scene. */
  ListBase keyingsets;

  /* Units. */
  struct UnitSettings unit;

  /** Grease Pencil - Annotations. */
  struct bGPdata *gpd;

  /* Movie Tracking. */
  /** Active movie clip. */
  struct MovieClip *clip;

  /** Physics simulation settings. */
  struct PhysicsSettings physics_settings;

  void *_pad8;
  /**
   * XXX: runtime flag for drawing, actually belongs in the window,
   * only used by #BKE_object_handle_update()
   */
  struct CustomData_MeshMasks customdata_mask;
  /** XXX: same as above but for temp operator use (viewport renders). */
  struct CustomData_MeshMasks customdata_mask_modal;

  /* Color Management. */
  ColorManagedViewSettings view_settings;
  ColorManagedDisplaySettings display_settings;
  ColorManagedColorspaceSettings sequencer_colorspace_settings;

  /** RigidBody simulation world+settings. */
  struct RigidBodyWorld *rigidbody_world;

  struct PreviewImage *preview;

  /** ViewLayer, defined in DNA_layer_types.h */
  ListBase view_layers;
  /** Not an actual data-block, but memory owned by scene. */
  struct Collection *master_collection;

  /** Settings to be override by work-spaces. */
  IDProperty *layer_properties;

  /**
   * Frame range used for simulations in geometry nodes by default, if SCE_CUSTOM_SIMULATION_RANGE
   * is set. Individual simulations can overwrite this though.
   */
  int simulation_frame_start;
  int simulation_frame_end;

  struct SceneDisplay display;
  struct SceneEEVEE eevee;
  struct SceneGpencil grease_pencil_settings;
  struct SceneHydra hydra;

  SceneRuntimeHandle *runtime;
  void *_pad9;
} Scene;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Data Enum/Flags
 * \{ */

/** #RenderData::flag. */
enum {
  /** Use preview range. */
  SCER_PRV_RANGE = 1 << 0,
  SCER_LOCK_FRAME_SELECTION = 1 << 1,
  /** Show/use sub-frames (for checking motion blur). */
  SCER_SHOW_SUBFRAME = 1 << 3,
};

/** #RenderData::mode. */
enum {
  R_MODE_UNUSED_0 = 1 << 0, /* dirty */
  R_SIMPLIFY_NORMALS = 1 << 1,
  R_MODE_UNUSED_2 = 1 << 2, /* cleared */
  R_MODE_UNUSED_3 = 1 << 3, /* cleared */
  R_MODE_UNUSED_4 = 1 << 4, /* cleared */
  R_MODE_UNUSED_5 = 1 << 5, /* cleared */
  R_MODE_UNUSED_6 = 1 << 6, /* cleared */
  R_MODE_UNUSED_7 = 1 << 7, /* cleared */
  R_MODE_UNUSED_8 = 1 << 8, /* cleared */
  R_BORDER = 1 << 9,
  R_MODE_UNUSED_10 = 1 << 10, /* cleared */
  R_CROP = 1 << 11,
  /** Disable camera switching: runtime (DURIAN_CAMERA_SWITCH) */
  R_NO_CAMERA_SWITCH = 1 << 12,
  R_MODE_UNUSED_13 = 1 << 13, /* cleared */
  R_MBLUR = 1 << 14,
  /* unified was here */
  R_MODE_UNUSED_16 = 1 << 16, /* cleared */
  R_MODE_UNUSED_17 = 1 << 17, /* cleared */
  R_MODE_UNUSED_18 = 1 << 18, /* cleared */
  R_MODE_UNUSED_19 = 1 << 19, /* cleared */
  R_FIXED_THREADS = 1 << 19,

  R_MODE_UNUSED_20 = 1 << 20, /* cleared */
  R_MODE_UNUSED_21 = 1 << 21, /* cleared */
  R_NO_OVERWRITE = 1 << 22,   /* Skip existing files. */
  R_TOUCH = 1 << 23,          /* Touch files before rendering. */
  R_SIMPLIFY = 1 << 24,
  R_EDGE_FRS = 1 << 25,        /* R_EDGE reserved for Freestyle */
  R_PERSISTENT_DATA = 1 << 26, /* Keep data around for re-render. */
  R_MODE_UNUSED_27 = 1 << 27,  /* cleared */
};

/** #RenderData::seq_flag */
enum {
  R_SEQ_UNUSED_0 = (1 << 0), /* cleared */
  R_SEQ_UNUSED_1 = (1 << 1), /* cleared */
  R_SEQ_UNUSED_2 = (1 << 2), /* cleared */
  R_SEQ_UNUSED_3 = (1 << 3), /* cleared */
  R_SEQ_UNUSED_4 = (1 << 4), /* cleared */
  R_SEQ_OVERRIDE_SCENE_SETTINGS = (1 << 5),
};

/** #RenderData::filtertype (used for nodes) */
enum {
  R_FILTER_BOX = 0,
  R_FILTER_TENT = 1,
  R_FILTER_QUAD = 2,
  R_FILTER_CUBIC = 3,
  R_FILTER_CATROM = 4,
  R_FILTER_GAUSS = 5,
  R_FILTER_MITCH = 6,
  R_FILTER_FAST_GAUSS = 7,
};

/** #RenderData::scemode */
enum {
  R_DOSEQ = 1 << 0,
  R_BG_RENDER = 1 << 1,
  /* Passepartout is camera option now, keep this for backward compatibility. */
  R_PASSEPARTOUT = 1 << 2,
  R_BUTS_PREVIEW = 1 << 3,
  R_EXTENSION = 1 << 4,
  R_MATNODE_PREVIEW = 1 << 5,
  R_DOCOMP = 1 << 6,
  R_COMP_CROP = 1 << 7,
  R_SCEMODE_UNUSED_8 = 1 << 8, /* cleared */
  R_SINGLE_LAYER = 1 << 9,
  R_SCEMODE_UNUSED_10 = 1 << 10, /* cleared */
  R_SCEMODE_UNUSED_11 = 1 << 11, /* cleared */
  R_NO_IMAGE_LOAD = 1 << 12,
  R_SCEMODE_UNUSED_13 = 1 << 13, /* cleared */
  R_NO_FRAME_UPDATE = 1 << 14,
  R_SCEMODE_UNUSED_15 = 1 << 15, /* cleared */
  R_SCEMODE_UNUSED_16 = 1 << 16, /* cleared */
  R_SCEMODE_UNUSED_17 = 1 << 17, /* cleared */
  R_TEXNODE_PREVIEW = 1 << 18,
  R_SCEMODE_UNUSED_19 = 1 << 19, /* cleared */
  R_EXR_CACHE_FILE = 1 << 20,
  R_MULTIVIEW = 1 << 21,
};

/** #RenderData::stamp */
enum {
  R_STAMP_TIME = 1 << 0,
  R_STAMP_FRAME = 1 << 1,
  R_STAMP_DATE = 1 << 2,
  R_STAMP_CAMERA = 1 << 3,
  R_STAMP_SCENE = 1 << 4,
  R_STAMP_NOTE = 1 << 5,
  /** Draw in the image space. */
  R_STAMP_DRAW = 1 << 6,
  R_STAMP_MARKER = 1 << 7,
  R_STAMP_FILENAME = 1 << 8,
  R_STAMP_SEQSTRIP = 1 << 9,
  R_STAMP_RENDERTIME = 1 << 10,
  R_STAMP_CAMERALENS = 1 << 11,
  R_STAMP_STRIPMETA = 1 << 12,
  R_STAMP_MEMORY = 1 << 13,
  R_STAMP_HIDE_LABELS = 1 << 14,
  R_STAMP_FRAME_RANGE = 1 << 15,
  R_STAMP_HOSTNAME = 1 << 16,
};

#define R_STAMP_ALL \
  (R_STAMP_TIME | R_STAMP_FRAME | R_STAMP_DATE | R_STAMP_CAMERA | R_STAMP_SCENE | R_STAMP_NOTE | \
   R_STAMP_MARKER | R_STAMP_FILENAME | R_STAMP_SEQSTRIP | R_STAMP_RENDERTIME | \
   R_STAMP_CAMERALENS | R_STAMP_MEMORY | R_STAMP_HIDE_LABELS | R_STAMP_FRAME_RANGE | \
   R_STAMP_HOSTNAME)

/** #RenderData::alphamode */
enum {
  R_ADDSKY = 0,
  R_ALPHAPREMUL = 1,
};

/** #RenderData::color_mgt_flag */
enum {
  /** Deprecated, should only be used in versioning code only. */
  R_COLOR_MANAGEMENT = (1 << 0),
  R_COLOR_MANAGEMENT_UNUSED_1 = (1 << 1),
};

/* bake_mode: same as RE_BAKE_xxx defines. */
/** #RenderData::bake_flag */
enum {
  R_BAKE_CLEAR = 1 << 0,
  // R_BAKE_OSA = 1 << 1, /* Deprecated. */
  R_BAKE_TO_ACTIVE = 1 << 2,
  // R_BAKE_NORMALIZE = 1 << 3, /* Deprecated. */
  R_BAKE_MULTIRES = 1 << 4,
  R_BAKE_LORES_MESH = 1 << 5,
  // R_BAKE_VCOL = 1 << 6, /* Deprecated. */
  R_BAKE_USERSCALE = 1 << 7,
  R_BAKE_CAGE = 1 << 8,
  R_BAKE_SPLIT_MAT = 1 << 9,
  R_BAKE_AUTO_NAME = 1 << 10,
};

/** #RenderData::bake_normal_space */
enum {
  R_BAKE_SPACE_CAMERA = 0,
  R_BAKE_SPACE_WORLD = 1,
  R_BAKE_SPACE_OBJECT = 2,
  R_BAKE_SPACE_TANGENT = 3,
};

/** #RenderData::line_thickness_mode */
enum {
  R_LINE_THICKNESS_ABSOLUTE = 1,
  R_LINE_THICKNESS_RELATIVE = 2,
};

/* Sequencer seq_prev_type seq_rend_type. */

/** #RenderData::engine (scene.cc) */
extern const char *RE_engine_id_BLENDER_EEVEE;
extern const char *RE_engine_id_BLENDER_EEVEE_NEXT;
extern const char *RE_engine_id_BLENDER_WORKBENCH;
extern const char *RE_engine_id_CYCLES;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene Defines
 * \{ */

/* Note that much higher max-frames give imprecise sub-frames, see: #46859. */
/* Current precision is 16 for the sub-frames closer to MAXFRAME. */

/* For general use. */
#define MAXFRAME 1048574
#define MAXFRAMEF 1048574.0f

#define MINFRAME 0
#define MINFRAMEF 0.0f

/** (Minimum frame number for current-frame). */
#define MINAFRAME -1048574
#define MINAFRAMEF -1048574.0f

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene Related Macros
 * \{ */

#define BASE_VISIBLE(v3d, base) BKE_base_is_visible(v3d, base)
#define BASE_SELECTABLE(v3d, base) \
  (BASE_VISIBLE(v3d, base) && \
   ((v3d == NULL) || (((1 << (base)->object->type) & (v3d)->object_type_exclude_select) == 0)) && \
   (((base)->flag & BASE_SELECTABLE) != 0))
#define BASE_SELECTED(v3d, base) (BASE_VISIBLE(v3d, base) && (((base)->flag & BASE_SELECTED) != 0))
#define BASE_EDITABLE(v3d, base) \
  (BASE_VISIBLE(v3d, base) && !ID_IS_LINKED((base)->object) && \
   (!ID_IS_OVERRIDE_LIBRARY_REAL((base)->object) || \
    ((base)->object->id.override_library->flag & LIBOVERRIDE_FLAG_SYSTEM_DEFINED) == 0))
#define BASE_SELECTED_EDITABLE(v3d, base) \
  (BASE_EDITABLE(v3d, base) && (((base)->flag & BASE_SELECTED) != 0))

/* deprecate this! */
#define OBEDIT_FROM_OBACT(ob) ((ob) ? (((ob)->mode & OB_MODE_EDIT) ? ob : NULL) : NULL)
#define OBPOSE_FROM_OBACT(ob) ((ob) ? (((ob)->mode & OB_MODE_POSE) ? ob : NULL) : NULL)
#define OBWEIGHTPAINT_FROM_OBACT(ob) \
  ((ob) ? (((ob)->mode & OB_MODE_WEIGHT_PAINT) ? ob : NULL) : NULL)

#define V3D_CAMERA_LOCAL(v3d) ((!(v3d)->scenelock && (v3d)->camera) ? (v3d)->camera : NULL)
#define V3D_CAMERA_SCENE(scene, v3d) \
  ((!(v3d)->scenelock && (v3d)->camera) ? (v3d)->camera : (scene)->camera)

#define PRVRANGEON (scene->r.flag & SCER_PRV_RANGE)
#define PSFRA ((PRVRANGEON) ? (scene->r.psfra) : (scene->r.sfra))
#define PEFRA ((PRVRANGEON) ? (scene->r.pefra) : (scene->r.efra))
#define FRA2TIME(a) ((((double)scene->r.frs_sec_base) * (double)(a)) / (double)scene->r.frs_sec)
#define TIME2FRA(a) ((((double)scene->r.frs_sec) * (double)(a)) / (double)scene->r.frs_sec_base)
#define FPS (((double)scene->r.frs_sec) / (double)scene->r.frs_sec_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene Enum/Flags
 * \{ */

/* Base.flag is in `DNA_object_types.h`. */

/** #ToolSettings::transform_flag */
enum {
  SCE_XFORM_AXIS_ALIGN = (1 << 0),
  SCE_XFORM_DATA_ORIGIN = (1 << 1),
  SCE_XFORM_SKIP_CHILDREN = (1 << 2),
};

/** #ToolSettings::object_flag */
enum {
  SCE_OBJECT_MODE_LOCK = (1 << 0),
};

/** #ToolSettings::workspace_tool_flag */
enum {
  SCE_WORKSPACE_TOOL_FALLBACK = 0,
  SCE_WORKSPACE_TOOL_DEFAULT = 1,
};

/** #ToolSettings::snap_flag */
typedef enum eSnapFlag {
  SCE_SNAP = (1 << 0),
  SCE_SNAP_ROTATE = (1 << 1),
  SCE_SNAP_PEEL_OBJECT = (1 << 2),
  // SCE_SNAP_PROJECT = (1 << 3), /* DEPRECATED, see #SCE_SNAP_INDIVIDUAL_PROJECT. */
  /** Was `SCE_SNAP_NO_SELF`, but self should be active. */
  SCE_SNAP_NOT_TO_ACTIVE = (1 << 4),
  SCE_SNAP_ABS_GRID = (1 << 5),
  /* Same value with different name to make it easier to understand in time based code. */
  SCE_SNAP_ABS_TIME_STEP = (1 << 5),
  SCE_SNAP_BACKFACE_CULLING = (1 << 6),
  SCE_SNAP_KEEP_ON_SAME_OBJECT = (1 << 7),
  /** see #eSnapTargetOP */
  SCE_SNAP_TO_INCLUDE_EDITED = (1 << 8),
  SCE_SNAP_TO_INCLUDE_NONEDITED = (1 << 9),
  SCE_SNAP_TO_ONLY_SELECTABLE = (1 << 10),
} eSnapFlag;

ENUM_OPERATORS(eSnapFlag, SCE_SNAP_TO_ONLY_SELECTABLE)

/** See #ToolSettings::snap_target (to be renamed `snap_source`) and #TransSnap.source_operation */
typedef enum eSnapSourceOP {
  SCE_SNAP_SOURCE_CLOSEST = 0,
  SCE_SNAP_SOURCE_CENTER = 1,
  SCE_SNAP_SOURCE_MEDIAN = 2,
  SCE_SNAP_SOURCE_ACTIVE = 3,
} eSnapSourceOP;

ENUM_OPERATORS(eSnapSourceOP, SCE_SNAP_SOURCE_ACTIVE)

/**
 * #TransSnap::target_operation and #ToolSettings::snap_flag
 * (#SCE_SNAP_NOT_TO_ACTIVE, #SCE_SNAP_TO_INCLUDE_EDITED, #SCE_SNAP_TO_INCLUDE_NONEDITED,
 * #SCE_SNAP_TO_ONLY_SELECTABLE).
 */
typedef enum eSnapTargetOP {
  SCE_SNAP_TARGET_ALL = 0,
  SCE_SNAP_TARGET_NOT_SELECTED = (1 << 0),
  SCE_SNAP_TARGET_NOT_ACTIVE = (1 << 1),
  SCE_SNAP_TARGET_NOT_EDITED = (1 << 2),
  SCE_SNAP_TARGET_ONLY_SELECTABLE = (1 << 3),
  SCE_SNAP_TARGET_NOT_NONEDITED = (1 << 4),
} eSnapTargetOP;
ENUM_OPERATORS(eSnapTargetOP, SCE_SNAP_TARGET_NOT_NONEDITED)

/** #ToolSettings::snap_mode */
typedef enum eSnapMode {
  SCE_SNAP_TO_NONE = 0,

  /** #ToolSettings::snap_node_mode */
  SCE_SNAP_TO_NODE_X = (1 << 0),
  SCE_SNAP_TO_NODE_Y = (1 << 1),

  /** #ToolSettings::snap_anim_mode */
  SCE_SNAP_TO_FRAME = (1 << 0),
  SCE_SNAP_TO_SECOND = (1 << 1),
  SCE_SNAP_TO_MARKERS = (1 << 2),

  /** #ToolSettings::snap_mode and #ToolSettings::snap_node_mode and #ToolSettings.snap_uv_mode */
  SCE_SNAP_TO_POINT = (1 << 0),
  SCE_SNAP_TO_EDGE_MIDPOINT = (1 << 1),
  SCE_SNAP_TO_EDGE_ENDPOINT = (1 << 2),
  SCE_SNAP_TO_EDGE_PERPENDICULAR = (1 << 3),
  SCE_SNAP_TO_EDGE = (1 << 4),
  SCE_SNAP_TO_FACE = (1 << 5),
  SCE_SNAP_TO_VOLUME = (1 << 6),
  SCE_SNAP_TO_GRID = (1 << 7),
  SCE_SNAP_TO_INCREMENT = (1 << 8),

  /** For snap individual elements. */
  SCE_SNAP_INDIVIDUAL_NEAREST = (1 << 9),
  SCE_SNAP_INDIVIDUAL_PROJECT = (1 << 10),
} eSnapMode;

/* Due to dependency conflicts with Cycles, header cannot directly include `BLI_utildefines.h`. */
/* TODO: move this macro to a more general place. */
#ifdef ENUM_OPERATORS
ENUM_OPERATORS(eSnapMode, SCE_SNAP_INDIVIDUAL_PROJECT)
#endif

#define SCE_SNAP_TO_VERTEX (SCE_SNAP_TO_POINT | SCE_SNAP_TO_EDGE_ENDPOINT)

#define SCE_SNAP_TO_GEOM \
  (SCE_SNAP_TO_VERTEX | SCE_SNAP_TO_EDGE | SCE_SNAP_TO_FACE | SCE_SNAP_TO_EDGE_MIDPOINT | \
   SCE_SNAP_TO_EDGE_PERPENDICULAR)

/** #SequencerToolSettings::snap_mode */
enum {
  SEQ_SNAP_TO_STRIPS = 1 << 0,
  SEQ_SNAP_TO_CURRENT_FRAME = 1 << 1,
  SEQ_SNAP_TO_STRIP_HOLD = 1 << 2,
};

/** #SequencerToolSettings::snap_flag */
enum {
  SEQ_SNAP_IGNORE_MUTED = 1 << 0,
  SEQ_SNAP_IGNORE_SOUND = 1 << 1,
  SEQ_SNAP_CURRENT_FRAME_TO_STRIPS = 1 << 2,
};

/** #ToolSettings::snap_transform_mode_flag */
typedef enum eSnapTransformMode {
  SCE_SNAP_TRANSFORM_MODE_TRANSLATE = (1 << 0),
  SCE_SNAP_TRANSFORM_MODE_ROTATE = (1 << 1),
  SCE_SNAP_TRANSFORM_MODE_SCALE = (1 << 2),
} eSnapTransformMode;

/** #ToolSettings::selectmode */
enum {
  SCE_SELECT_VERTEX = 1 << 0, /* for mesh */
  SCE_SELECT_EDGE = 1 << 1,
  SCE_SELECT_FACE = 1 << 2,
};

/** #MeshStatVis::type */
enum {
  SCE_STATVIS_OVERHANG = 0,
  SCE_STATVIS_THICKNESS = 1,
  SCE_STATVIS_INTERSECT = 2,
  SCE_STATVIS_DISTORT = 3,
  SCE_STATVIS_SHARP = 4,
};

/** #ParticleEditSettings::selectmode for particles */
enum {
  SCE_SELECT_PATH = 1 << 0,
  SCE_SELECT_POINT = 1 << 1,
  SCE_SELECT_END = 1 << 2,
};

/** #ToolSettings::prop_mode (proportional falloff) */
enum {
  PROP_SMOOTH = 0,
  PROP_SPHERE = 1,
  PROP_ROOT = 2,
  PROP_SHARP = 3,
  PROP_LIN = 4,
  PROP_CONST = 5,
  PROP_RANDOM = 6,
  PROP_INVSQUARE = 7,
  PROP_MODE_MAX = 8,
};

/** #ToolSettings::proportional_edit & similarly named members. */
enum {
  PROP_EDIT_USE = (1 << 0),
  PROP_EDIT_CONNECTED = (1 << 1),
  PROP_EDIT_PROJECTED = (1 << 2),
};

/** #ToolSettings::weightuser */
enum {
  OB_DRAW_GROUPUSER_NONE = 0,
  OB_DRAW_GROUPUSER_ACTIVE = 1,
  OB_DRAW_GROUPUSER_ALL = 2,
};

/* object_vgroup.cc */

#define WT_VGROUP_MASK_ALL \
  ((1 << WT_VGROUP_ACTIVE) | (1 << WT_VGROUP_BONE_SELECT) | (1 << WT_VGROUP_BONE_DEFORM) | \
   (1 << WT_VGROUP_BONE_DEFORM_OFF) | (1 << WT_VGROUP_ALL))

/** #Scene::flag */
enum {
  SCE_DS_SELECTED = 1 << 0,
  SCE_DS_COLLAPSED = 1 << 1,
  SCE_NLA_EDIT_ON = 1 << 2,
  SCE_FRAME_DROP = 1 << 3,
  SCE_KEYS_NO_SELONLY = 1 << 4,
  SCE_READFILE_LIBLINK_NEED_SETSCENE_CHECK = 1 << 5,
  SCE_CUSTOM_SIMULATION_RANGE = 1 << 6,
};

/* Return flag BKE_scene_base_iter_next functions. */
enum {
  // F_ERROR = -1, /* UNUSED. */
  F_START = 0,
  F_SCENE = 1,
  F_DUPLI = 3,
};

/** #AudioData::flag */
enum {
  AUDIO_MUTE = 1 << 0,
  AUDIO_SYNC = 1 << 1,
  AUDIO_SCRUB = 1 << 2,
  AUDIO_VOLUME_ANIMATED = 1 << 3,
};

/** #FFMpegCodecData::flags */
enum {
#ifdef DNA_DEPRECATED_ALLOW
  /* DEPRECATED: you can choose none as audio-codec now. */
  FFMPEG_MULTIPLEX_AUDIO = (1 << 0),
#endif
  FFMPEG_AUTOSPLIT_OUTPUT = (1 << 1),
  FFMPEG_LOSSLESS_OUTPUT = (1 << 2),
  FFMPEG_USE_MAX_B_FRAMES = (1 << 3),
};

/** #Paint::flags */
typedef enum ePaintFlags {
  PAINT_SHOW_BRUSH = (1 << 0),
  PAINT_FAST_NAVIGATE = (1 << 1),
  PAINT_SHOW_BRUSH_ON_SURFACE = (1 << 2),
  PAINT_USE_CAVITY_MASK = (1 << 3),
  PAINT_SCULPT_DELAY_UPDATES = (1 << 4),
  PAINT_SCULPT_SHOW_PIVOT = (1 << 5),
} ePaintFlags;

/**
 * #Paint::symmetry_flags
 * (for now just a duplicate of sculpt symmetry flags).
 */
typedef enum ePaintSymmetryFlags {
  PAINT_SYMM_NONE = 0,
  PAINT_SYMM_X = (1 << 0),
  PAINT_SYMM_Y = (1 << 1),
  PAINT_SYMM_Z = (1 << 2),
  PAINT_SYMMETRY_FEATHER = (1 << 3),
  PAINT_TILE_X = (1 << 4),
  PAINT_TILE_Y = (1 << 5),
  PAINT_TILE_Z = (1 << 6),
} ePaintSymmetryFlags;
ENUM_OPERATORS(ePaintSymmetryFlags, PAINT_TILE_Z);
#define PAINT_SYMM_AXIS_ALL (PAINT_SYMM_X | PAINT_SYMM_Y | PAINT_SYMM_Z)

#ifdef __cplusplus
inline ePaintSymmetryFlags operator++(ePaintSymmetryFlags &flags, int)
{
  flags = ePaintSymmetryFlags(char(flags) + 1);
  return flags;
}
#endif

/**
 * #Sculpt::flags
 * These can eventually be moved to paint flags?
 */
typedef enum eSculptFlags {
  SCULPT_FLAG_UNUSED_0 = (1 << 0), /* cleared */
  SCULPT_FLAG_UNUSED_1 = (1 << 1), /* cleared */
  SCULPT_FLAG_UNUSED_2 = (1 << 2), /* cleared */

  SCULPT_LOCK_X = (1 << 3),
  SCULPT_LOCK_Y = (1 << 4),
  SCULPT_LOCK_Z = (1 << 5),

  SCULPT_FLAG_UNUSED_6 = (1 << 6), /* cleared */

  SCULPT_FLAG_UNUSED_7 = (1 << 7), /* cleared */
  SCULPT_ONLY_DEFORM = (1 << 8),
  // SCULPT_SHOW_DIFFUSE = (1 << 9), /* deprecated */

  /** If set, the mesh will be drawn with smooth-shading in dynamic-topology mode. */
  SCULPT_FLAG_UNUSED_10 = (1 << 10), /* deprecated */

  /** If set, dynamic-topology brushes will subdivide short edges. */
  SCULPT_DYNTOPO_SUBDIVIDE = (1 << 12), /* deprecated. */
  /** If set, dynamic-topology brushes will collapse short edges. */
  SCULPT_DYNTOPO_COLLAPSE = (1 << 11), /* deprecated. */

  /** If set, dynamic-topology detail size will be constant in object space. */
  SCULPT_DYNTOPO_DETAIL_CONSTANT = (1 << 13), /* deprecated. */
  SCULPT_DYNTOPO_DETAIL_BRUSH = (1 << 14),    /* deprecated. */
  /* Don't display mask in viewport, but still use it for strokes. */
  SCULPT_HIDE_MASK = (1 << 15),
  SCULPT_DYNTOPO_DETAIL_MANUAL = (1 << 16), /* deprecated. */

  /* Don't display face sets in viewport. */
  SCULPT_HIDE_FACE_SETS = (1 << 17),
  SCULPT_FLAG_UNUSED_8 = (1 << 18),

  /* Hides facesets/masks and forces indexed mode to save GPU bandwidth. */
  SCULPT_FLAG_UNUSED_20 = (1 << 20),
  SCULPT_DYNTOPO_ENABLED = (1 << 21),
} eSculptFlags;

/** #Sculpt::transform_mode */
typedef enum eSculptTransformMode {
  SCULPT_TRANSFORM_MODE_ALL_VERTICES = 0,
  SCULPT_TRANSFORM_MODE_RADIUS_ELASTIC = 1,
} eSculptTrasnformMode;

/** #PaintModeSettings::mode */
typedef enum ePaintCanvasSource {
  /** Paint on the active node of the active material slot. */
  PAINT_CANVAS_SOURCE_MATERIAL = 0,
  /** Paint on a selected image. */
  PAINT_CANVAS_SOURCE_IMAGE = 1,
  /** Paint on the active color attribute (vertex color) layer. */
  PAINT_CANVAS_SOURCE_COLOR_ATTRIBUTE = 2,
} ePaintCanvasSource;

/** #ImagePaintSettings::mode */
/* Defines to let old texture painting use the new enum. */
/* TODO(jbakker): rename usages. */
#define IMAGEPAINT_MODE_MATERIAL PAINT_CANVAS_SOURCE_MATERIAL
#define IMAGEPAINT_MODE_IMAGE PAINT_CANVAS_SOURCE_IMAGE

/** #ImagePaintSettings::interp */
enum {
  IMAGEPAINT_INTERP_LINEAR = 0,
  IMAGEPAINT_INTERP_CLOSEST = 1,
};

/** #ImagePaintSettings::flag */
enum {
  IMAGEPAINT_DRAWING = 1 << 0,
  // IMAGEPAINT_DRAW_TOOL = 1 << 1,         /* Deprecated. */
  // IMAGEPAINT_DRAW_TOOL_DRAWING = 1 << 2, /* Deprecated. */
};

/* Projection painting only. */
/** #ImagePaintSettings::flag */
enum {
  IMAGEPAINT_PROJECT_XRAY = 1 << 4,
  IMAGEPAINT_PROJECT_BACKFACE = 1 << 5,
  IMAGEPAINT_PROJECT_FLAT = 1 << 6,
  IMAGEPAINT_PROJECT_LAYER_CLONE = 1 << 7,
  IMAGEPAINT_PROJECT_LAYER_STENCIL = 1 << 8,
  IMAGEPAINT_PROJECT_LAYER_STENCIL_INV = 1 << 9,
};

/** #ImagePaintSettings::missing_data */
enum {
  IMAGEPAINT_MISSING_UVS = 1 << 0,
  IMAGEPAINT_MISSING_MATERIAL = 1 << 1,
  IMAGEPAINT_MISSING_TEX = 1 << 2,
  IMAGEPAINT_MISSING_STENCIL = 1 << 3,
};

/** #ToolSettings::uvcalc_flag */
enum {
  UVCALC_FILLHOLES = 1 << 0,
  /** Would call this UVCALC_ASPECT_CORRECT, except it should be default with old file. */
  UVCALC_NO_ASPECT_CORRECT = 1 << 1,
  /** Adjust UVs while transforming with Vert or Edge Slide. */
  UVCALC_TRANSFORM_CORRECT_SLIDE = 1 << 2,
  /** Use mesh data after subsurf to compute UVs. */
  UVCALC_USESUBSURF = 1 << 3,
  /** Adjust UVs while transforming to avoid distortion */
  UVCALC_TRANSFORM_CORRECT = 1 << 4,
  /** Keep equal values merged while correcting custom-data. */
  UVCALC_TRANSFORM_CORRECT_KEEP_CONNECTED = 1 << 5,
};

/** #ToolSettings::uv_flag */
enum {
  UV_SYNC_SELECTION = 1,
  UV_SHOW_SAME_IMAGE = 2,
};

/** #ToolSettings::uv_selectmode */
enum {
  UV_SELECT_VERTEX = 1 << 0,
  UV_SELECT_EDGE = 1 << 1,
  UV_SELECT_FACE = 1 << 2,
  UV_SELECT_ISLAND = 1 << 3,
};

/** #ToolSettings::uv_sticky */
enum {
  SI_STICKY_LOC = 0,
  SI_STICKY_DISABLE = 1,
  SI_STICKY_VERTEX = 2,
};

/** #ToolSettings::gpencil_flags */
typedef enum eGPencil_Flags {
  /** Enables multi-frame editing. */
  GP_USE_MULTI_FRAME_EDITING = (1 << 0),
  /** When creating new frames, the last frame gets used as the basis for the new one. */
  GP_TOOL_FLAG_RETAIN_LAST = (1 << 1),
  /** Add the strokes below all strokes in the layer. */
  GP_TOOL_FLAG_PAINT_ONBACK = (1 << 2),
  /** Show compact list of colors. */
  GP_TOOL_FLAG_THUMBNAIL_LIST = (1 << 3),
  /** Generate weight data for new strokes. */
  GP_TOOL_FLAG_CREATE_WEIGHTS = (1 << 4),
  /** Auto-merge with last stroke. */
  GP_TOOL_FLAG_AUTOMERGE_STROKE = (1 << 5),
} eGPencil_Flags;

/** #Scene::r.simplify_gpencil */
typedef enum eGPencil_SimplifyFlags {
  /** Simplify. */
  SIMPLIFY_GPENCIL_ENABLE = (1 << 0),
  /** Simplify on play. */
  SIMPLIFY_GPENCIL_ON_PLAY = (1 << 1),
  /** Simplify fill on viewport. */
  SIMPLIFY_GPENCIL_FILL = (1 << 2),
  /** Simplify modifier on viewport. */
  SIMPLIFY_GPENCIL_MODIFIER = (1 << 3),
  /** Simplify Shader FX. */
  SIMPLIFY_GPENCIL_FX = (1 << 5),
  /** Simplify layer tint. */
  SIMPLIFY_GPENCIL_TINT = (1 << 7),
  /** Simplify Anti-aliasing. */
  SIMPLIFY_GPENCIL_AA = (1 << 8),
} eGPencil_SimplifyFlags;

/** `ToolSettings.gpencil_*_align` - Stroke Placement mode flags. */
typedef enum eGPencil_Placement_Flags {
  /** New strokes are added in viewport/data space (i.e. not screen space). */
  GP_PROJECT_VIEWSPACE = (1 << 0),

  // /** Viewport space, but relative to render canvas (Sequencer Preview Only) */
  // GP_PROJECT_CANVAS = (1 << 1), /* UNUSED */

  /** Project into the screen's Z values. */
  GP_PROJECT_DEPTH_VIEW = (1 << 2),
  GP_PROJECT_DEPTH_STROKE = (1 << 3),

  /** "Use Endpoints". */
  GP_PROJECT_DEPTH_STROKE_ENDPOINTS = (1 << 4),
  GP_PROJECT_CURSOR = (1 << 5),
  GP_PROJECT_DEPTH_STROKE_FIRST = (1 << 6),
} eGPencil_Placement_Flags;

/** #ToolSettings::gpencil_selectmode */
typedef enum eGPencil_Selectmode_types {
  GP_SELECTMODE_POINT = 0,
  GP_SELECTMODE_STROKE = 1,
  GP_SELECTMODE_SEGMENT = 2,
} eGPencil_Selectmode_types;

/** #ToolSettings::gpencil_guide_types */
typedef enum eGPencil_GuideTypes {
  GP_GUIDE_CIRCULAR = 0,
  GP_GUIDE_RADIAL = 1,
  GP_GUIDE_PARALLEL = 2,
  GP_GUIDE_GRID = 3,
  GP_GUIDE_ISO = 4,
} eGPencil_GuideTypes;

/** #ToolSettings::gpencil_guide_references */
typedef enum eGPencil_Guide_Reference {
  GP_GUIDE_REF_CURSOR = 0,
  GP_GUIDE_REF_CUSTOM = 1,
  GP_GUIDE_REF_OBJECT = 2,
} eGPencil_Guide_Reference;

/** #ToolSettings::particle flag */
enum {
  PE_KEEP_LENGTHS = 1 << 0,
  PE_LOCK_FIRST = 1 << 1,
  PE_DEFLECT_EMITTER = 1 << 2,
  PE_INTERPOLATE_ADDED = 1 << 3,
  PE_DRAW_PART = 1 << 4,
  PE_UNUSED_6 = 1 << 6, /* cleared */
  PE_FADE_TIME = 1 << 7,
  PE_AUTO_VELOCITY = 1 << 8,
};

/** #ParticleEditSettings::brushtype */
enum {
  PE_BRUSH_NONE = -1,
  PE_BRUSH_COMB = 0,
  PE_BRUSH_CUT = 1,
  PE_BRUSH_LENGTH = 2,
  PE_BRUSH_PUFF = 3,
  PE_BRUSH_ADD = 4,
  PE_BRUSH_SMOOTH = 5,
  PE_BRUSH_WEIGHT = 6,
};

/** #ParticleBrushData::flag */
enum {
  PE_BRUSH_DATA_PUFF_VOLUME = 1 << 0,
};

/** #ParticleBrushData::edittype */
enum {
  PE_TYPE_PARTICLES = 0,
  PE_TYPE_SOFTBODY = 1,
  PE_TYPE_CLOTH = 2,
};

/** #PhysicsSettings::flag */
enum {
  PHYS_GLOBAL_GRAVITY = 1,
};

/* UnitSettings */

#define USER_UNIT_ADAPTIVE 0xFF
/** #UnitSettings::system */
enum {
  USER_UNIT_NONE = 0,
  USER_UNIT_METRIC = 1,
  USER_UNIT_IMPERIAL = 2,
};
/** #UnitSettings::flag */
enum {
  USER_UNIT_OPT_SPLIT = 1,
  USER_UNIT_ROT_RADIANS = 2,
};

/** #SceneEEVEE::flag */
enum {
  // SCE_EEVEE_VOLUMETRIC_ENABLED = (1 << 0), /* Unused */
  SCE_EEVEE_VOLUMETRIC_LIGHTS = (1 << 1),
  SCE_EEVEE_VOLUMETRIC_SHADOWS = (1 << 2),
  //  SCE_EEVEE_VOLUMETRIC_COLORED    = (1 << 3), /* Unused */
  SCE_EEVEE_GTAO_ENABLED = (1 << 4),
  SCE_EEVEE_GTAO_BENT_NORMALS = (1 << 5),
  SCE_EEVEE_GTAO_BOUNCE = (1 << 6),
  // SCE_EEVEE_DOF_ENABLED = (1 << 7), /* Moved to camera->dof.flag */
  SCE_EEVEE_BLOOM_ENABLED = (1 << 8),
  SCE_EEVEE_MOTION_BLUR_ENABLED_DEPRECATED = (1 << 9), /* Moved to scene->r.mode */
  SCE_EEVEE_SHADOW_HIGH_BITDEPTH = (1 << 10),
  SCE_EEVEE_TAA_REPROJECTION = (1 << 11),
  // SCE_EEVEE_SSS_ENABLED = (1 << 12), /* Unused */
  // SCE_EEVEE_SSS_SEPARATE_ALBEDO = (1 << 13), /* Unused */
  SCE_EEVEE_SSR_ENABLED = (1 << 14),
  SCE_EEVEE_SSR_REFRACTION = (1 << 15),
  SCE_EEVEE_SSR_HALF_RESOLUTION = (1 << 16),
  SCE_EEVEE_SHOW_IRRADIANCE = (1 << 17),
  SCE_EEVEE_SHOW_CUBEMAPS = (1 << 18),
  SCE_EEVEE_GI_AUTOBAKE = (1 << 19),
  SCE_EEVEE_SHADOW_SOFT = (1 << 20),
  SCE_EEVEE_OVERSCAN = (1 << 21),
  SCE_EEVEE_DOF_HQ_SLIGHT_FOCUS = (1 << 22),
  SCE_EEVEE_DOF_JITTER = (1 << 23),
  SCE_EEVEE_SHADOW_ENABLED = (1 << 24),
  SCE_EEVEE_RAYTRACE_OPTIONS_SPLIT = (1 << 25),
};

typedef enum RaytraceEEVEE_Flag {
  RAYTRACE_EEVEE_USE_DENOISE = (1 << 0),
} RaytraceEEVEE_Flag;

typedef enum RaytraceEEVEE_DenoiseStages {
  RAYTRACE_EEVEE_DENOISE_SPATIAL = (1 << 0),
  RAYTRACE_EEVEE_DENOISE_TEMPORAL = (1 << 1),
  RAYTRACE_EEVEE_DENOISE_BILATERAL = (1 << 2),
} RaytraceEEVEE_DenoiseStages;

typedef enum RaytraceEEVEE_Method {
  RAYTRACE_EEVEE_METHOD_NONE = 0,
  RAYTRACE_EEVEE_METHOD_SCREEN = 1,
  /* TODO(fclem): Hardware ray-tracing. */
  // RAYTRACE_EEVEE_METHOD_HARDWARE = 2,
} RaytraceEEVEE_Method;

/** #SceneEEVEE::shadow_method */
enum {
  SHADOW_ESM = 1,
  /* SHADOW_VSM = 2, */        /* UNUSED */
  /* SHADOW_METHOD_MAX = 3, */ /* UNUSED */
};

/** #SceneDisplay->render_aa and #SceneDisplay->viewport_aa */
enum {
  SCE_DISPLAY_AA_OFF = 0,
  SCE_DISPLAY_AA_FXAA = 1,
  SCE_DISPLAY_AA_SAMPLES_5 = 5,
  SCE_DISPLAY_AA_SAMPLES_8 = 8,
  SCE_DISPLAY_AA_SAMPLES_11 = 11,
  SCE_DISPLAY_AA_SAMPLES_16 = 16,
  SCE_DISPLAY_AA_SAMPLES_32 = 32,
};

/** #SceneHydra->export_method */

enum {
  SCE_HYDRA_EXPORT_HYDRA = 0,
  SCE_HYDRA_EXPORT_USD = 1,
};

/** \} */
