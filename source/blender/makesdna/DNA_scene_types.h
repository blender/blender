/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"

#include "BLI_enum_flags.hh"
#include "BLI_map.hh"
#include "BLI_math_constants.h"

/**
 * Check for cyclic set-scene.
 * Libraries can cause this case which is normally prevented, see (#42009).
 */
#define USE_SETSCENE_CHECK

#include "DNA_ID.h"
#include "DNA_brush_enums.h"
#include "DNA_color_types.h" /* color management */
#include "DNA_curve_enums.h"
#include "DNA_customdata_types.h" /* Scene's runtime custom-data masks. */
#include "DNA_freestyle_types.h"
#include "DNA_image_types.h"
#include "DNA_layer_types.h"
#include "DNA_listBase.h"
#include "DNA_scene_enums.h"
#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"

namespace blender {

struct AnimData;
struct Brush;
struct Collection;
struct CurveMapping;
struct CurveProfile;
struct CustomData_MeshMasks;
struct Depsgraph;
struct Editing;
struct Image;
struct MovieClip;
struct Object;
struct Scene;
struct World;
struct bGPdata;
struct bNodeTree;
struct KeyingSet;
struct TransformOrientation;

namespace bke {
struct PaintRuntime;
class SceneRuntime;
}  // namespace bke
namespace ocio {
class ColorSpace;
}
using SceneDepsgraphsMap = Map<struct DepsgraphKey, Depsgraph *, 4>;

/* -------------------------------------------------------------------- */
/** \name FFMPEG
 * \{ */

enum eFFMpegPreset {
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
};

/**
 * Mapping from easily-understandable quality (Constant Rate Factor - CRF) descriptions
 * to H.264 8-bit CRF values. https://trac.ffmpeg.org/wiki/Encode/H.264#a1.ChooseaCRFvalue
 * For other video codecs these values might need to be remapped.
 */
enum eFFMpegCrf {
  FFM_CRF_NONE = -1,
  FFM_CRF_LOSSLESS = 0,
  FFM_CRF_PERC_LOSSLESS = 17,
  FFM_CRF_HIGH = 20,
  FFM_CRF_MEDIUM = 23,
  FFM_CRF_LOW = 26,
  FFM_CRF_VERYLOW = 29,
  FFM_CRF_LOWEST = 32,
  FFM_CRF_CUSTOM = 128,
};

enum eFFMpegAudioChannels {
  FFM_CHANNELS_MONO = 1,
  FFM_CHANNELS_STEREO = 2,
  FFM_CHANNELS_SURROUND4 = 4,
  FFM_CHANNELS_SURROUND51 = 6,
  FFM_CHANNELS_SURROUND71 = 8,
};

enum eFFMpegProresProfile {
  FFM_PRORES_PROFILE_422_PROXY = 0, /* FF_PROFILE_PRORES_PROXY */
  FFM_PRORES_PROFILE_422_LT = 1,    /* FF_PROFILE_PRORES_LT */
  FFM_PRORES_PROFILE_422_STD = 2,   /* FF_PROFILE_PRORES_STANDARD */
  FFM_PRORES_PROFILE_422_HQ = 3,    /* FF_PROFILE_PRORES_HQ */
  FFM_PRORES_PROFILE_4444 = 4,      /* FF_PROFILE_PRORES_4444 */
  FFM_PRORES_PROFILE_4444_XQ = 5,   /* FF_PROFILE_PRORES_XQ */
};

/* Note: These used to match `AVCodecID` enum values. Kept old values to keep file compatibility.
 * Use `MOV_av_codec_id_get()` to get `AVCodecID` value. */
enum IMB_Ffmpeg_Codec_ID {
  FFMPEG_CODEC_ID_NONE = 0,
  FFMPEG_CODEC_ID_MPEG1VIDEO = 1,
  FFMPEG_CODEC_ID_MPEG2VIDEO = 2,
  FFMPEG_CODEC_ID_MPEG4 = 12,
  FFMPEG_CODEC_ID_FLV1 = 21,
  FFMPEG_CODEC_ID_DVVIDEO = 24,
  FFMPEG_CODEC_ID_HUFFYUV = 25,
  FFMPEG_CODEC_ID_H264 = 27,
  FFMPEG_CODEC_ID_THEORA = 30,
  FFMPEG_CODEC_ID_FFV1 = 33,
  FFMPEG_CODEC_ID_QTRLE = 55,
  FFMPEG_CODEC_ID_PNG = 61,
  FFMPEG_CODEC_ID_DNXHD = 99,
  FFMPEG_CODEC_ID_VP9 = 167,
  FFMPEG_CODEC_ID_H265 = 173,
  FFMPEG_CODEC_ID_AV1 = 226,
  FFMPEG_CODEC_ID_PRORES = 147,
  FFMPEG_CODEC_ID_PCM_S16LE = 65536,
  FFMPEG_CODEC_ID_MP2 = 86016,
  FFMPEG_CODEC_ID_MP3 = 86017,
  FFMPEG_CODEC_ID_AAC = 86018,
  FFMPEG_CODEC_ID_AC3 = 86019,
  FFMPEG_CODEC_ID_VORBIS = 86021,
  FFMPEG_CODEC_ID_FLAC = 86028,
  FFMPEG_CODEC_ID_OPUS = 86076,
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

struct FFMpegCodecData {
  int type = 0;
  int codec = 0;       /* Use `codec_id_get()` instead! IMB_Ffmpeg_Codec_ID */
  int audio_codec = 0; /* Use `audio_codec_id_get()` instead! IMB_Ffmpeg_Codec_ID */
  int video_bitrate = 0;
  int audio_bitrate = 192;
  int audio_mixrate = 48000;
  int audio_channels = 2;
  float audio_volume = 1.0f;
  int gop_size = 0;
  /** Only used if FFMPEG_USE_MAX_B_FRAMES flag is set. */
  int max_b_frames = 0;
  int flags = 0;
  int constant_rate_factor = 0;
  /** Only used if constant_rate_factor flag is set to FFM_CRF_CUSTOM. */
  int custom_constant_rate_factor = 23;
  /** See eFFMpegPreset. */
  int ffmpeg_preset = 0;
  int ffmpeg_prores_profile = 0;

  int rc_min_rate = 0;
  int rc_max_rate = 0;
  int rc_buffer_size = 0;
  int mux_packet_size = 0;
  int mux_rate = 0;

#ifdef __cplusplus
  IMB_Ffmpeg_Codec_ID codec_id_get() const
  {
    return IMB_Ffmpeg_Codec_ID(codec);
  }
  IMB_Ffmpeg_Codec_ID audio_codec_id_get() const
  {
    return IMB_Ffmpeg_Codec_ID(audio_codec);
  }
  void codec_id_set(IMB_Ffmpeg_Codec_ID codec_id)
  {
    codec = codec_id;
  }
  void audio_codec_id_set(IMB_Ffmpeg_Codec_ID codec_id)
  {
    audio_codec = codec_id;
  }

#endif
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Audio
 * \{ */

/** #AudioData::flag */
enum {
  AUDIO_MUTE = 1 << 0,
  AUDIO_SYNC = 1 << 1,
  AUDIO_SCRUB = 1 << 2,
  AUDIO_VOLUME_ANIMATED = 1 << 3,
};

struct AudioData {
  int mixrate = 0; /* 2.5: now in FFMpegCodecData: audio_mixrate. */
  float main = 0;  /* 2.5: now in FFMpegCodecData: audio_volume. */
  float speed_of_sound = 343.3f;
  float doppler_factor = 1.0f;
  int distance_model = 2.0f;
  short flag = AUDIO_SYNC;
  char _pad[2] = {};
  float volume = 1.0f;
  char _pad2[4] = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Layers
 * \{ */

/** Render Layer. */
struct SceneRenderLayer {
  struct SceneRenderLayer *next = nullptr, *prev = nullptr;

  DNA_DEPRECATED char name[/*MAX_NAME*/ 64];

  /** Converted to ViewLayer setting. */
  DNA_DEPRECATED struct Material *mat_override = nullptr;
  DNA_DEPRECATED struct World *world_override = nullptr;

  /** Converted to LayerCollection cycles camera visibility override. */
  DNA_DEPRECATED unsigned int lay = 0;
  /** Converted to LayerCollection cycles holdout override. */
  DNA_DEPRECATED unsigned int lay_zmask = 0;
  DNA_DEPRECATED unsigned int lay_exclude = 0;
  /** Converted to ViewLayer layflag and flag. */
  DNA_DEPRECATED int layflag = 0;

  /* Pass_xor has to be after passflag. */
  /** Pass_xor has to be after passflag. */
  DNA_DEPRECATED int passflag = 0;
  /** Converted to ViewLayer passflag and flag. */
  DNA_DEPRECATED int pass_xor = 0;

  /** Converted to ViewLayer setting. */
  DNA_DEPRECATED int samples = 0;
  /** Converted to ViewLayer pass_alpha_threshold. */
  DNA_DEPRECATED float pass_alpha_threshold = 0;

  /** Converted to ViewLayer id_properties. */
  DNA_DEPRECATED IDProperty *prop = nullptr;

  /** Converted to ViewLayer freestyleConfig. */
  DNA_DEPRECATED struct FreestyleConfig freestyleConfig;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Multi-View
 * \{ */

/** View (Multi-view). */
struct SceneRenderView {
  struct SceneRenderView *next = nullptr, *prev = nullptr;

  char name[/*MAX_NAME*/ 64] = "";
  char suffix[/*MAX_NAME*/ 64] = "";

  int viewflag = 0;
  char _pad2[4] = {};
};

/* Stereo Flags. */
#define STEREO_RIGHT_NAME "right"
#define STEREO_LEFT_NAME "left"
#define STEREO_RIGHT_SUFFIX "_R"
#define STEREO_LEFT_SUFFIX "_L"

struct Stereo3dFormat {
  short flag = 0;
  /** Encoding mode. */
  char display_mode = 0;
  /** Anaglyph scheme for the user display. */
  char anaglyph_type = 0;
  /** Interlace type for the user display. */
  char interlace_type = 0;
  char _pad[3] = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Format Data
 * \{ */

/** #ImageFormatData::media_type */
enum MediaType {
  MEDIA_TYPE_IMAGE = 0,
  MEDIA_TYPE_MULTI_LAYER_IMAGE = 1,
  MEDIA_TYPE_VIDEO = 2,
};

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
  /* R_IMF_IMTYPE_AVIRAW = 15, DEPRECATED */
  /* R_IMF_IMTYPE_AVIJPEG = 16, DEPRECATED */
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
  /* R_IMF_IMTYPE_H264 = 31, DEPRECATED */
  /* R_IMF_IMTYPE_XVID = 32, DEPRECATED */
  /* R_IMF_IMTYPE_THEORA = 33, DEPRECATED */
  R_IMF_IMTYPE_PSD = 34,
  R_IMF_IMTYPE_WEBP = 35,
  /* R_IMF_IMTYPE_AV1 = 36, DEPRECATED */

  R_IMF_IMTYPE_INVALID = 255,
};

/** #ImageFormatData::flag */
enum {
  // R_IMF_FLAG_ZBUF = 1 << 0, /* DEPRECATED, and cleared. */
  R_IMF_FLAG_PREVIEW_JPG = 1 << 1,
};

/**
 * #ImageFormatData::depth
 *
 * Return values from #BKE_imtype_valid_depths, note this is depths per channel.
 */
enum eImageFormatDepth {
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
};

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

/** #ImageFormatData::exr_flag */
enum {
  R_IMF_EXR_FLAG_MULTIPART = 1 << 0,
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

/** #ImageFormatData::color_management */
enum {
  R_IMF_COLOR_MANAGEMENT_FOLLOW_SCENE = 0,
  R_IMF_COLOR_MANAGEMENT_OVERRIDE = 1,
};

/**
 * Generic image format settings,
 * this is used for #NodeImageFile and #IMAGE_OT_save_as operator too.
 *
 * NOTE: its a bit strange that even though this is an image format struct
 * the imtype can still be used to select video formats.
 * RNA ensures these enum's are only selectable for render output.
 */
struct ImageFormatData {
  /** MediaType. */
  char media_type = 0;
  /**
   * R_IMF_IMTYPE_PNG, R_...
   * \note Video types should only ever be set from this structure when used from #RenderData.
   */
  char imtype = R_IMF_IMTYPE_PNG;
  /**
   * bits per channel, R_IMF_CHAN_DEPTH_8 -> 32,
   * not a flag, only set 1 at a time. */
  char depth = R_IMF_CHAN_DEPTH_8;

  /** R_IMF_PLANES_BW, R_IMF_PLANES_RGB, R_IMF_PLANES_RGBA. */
  char planes = R_IMF_PLANES_RGBA;
  /** Generic options for all image types, alpha Z-buffer. */
  char flag = 0;

  /** (0 - 100), eg: JPEG quality. */
  char quality = 90;
  /** (0 - 100), eg: PNG compression. */
  char compress = 15;

  /* --- format specific --- */

  /** OpenEXR: R_IMF_EXR_CODEC_* values in low OPENEXR_CODEC_MASK bits. */
  char exr_codec = 0;
  char exr_flag = R_IMF_EXR_FLAG_MULTIPART;

  /** Jpeg2000. */
  char jp2_flag = 0;
  char jp2_codec = 0;

  /** TIFF. */
  char tiff_codec = 0;

  /** CINEON. */
  char cineon_flag = 0;
  char _pad[3] = {};
  short cineon_white = 0, cineon_black = 0;
  float cineon_gamma = 0;

  /** Multi-view. */
  Stereo3dFormat stereo3d_format;
  char views_format = 0;

  /* Color management members. */

  char color_management = 0;
  char _pad1[6] = {};
  ColorManagedViewSettings view_settings;
  ColorManagedDisplaySettings display_settings;
  ColorManagedColorspaceSettings linear_colorspace_settings;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Bake
 * \{ */

/** #BakeData::type */
enum eBakeType {
  R_BAKE_NORMALS = 0,
  R_BAKE_DISPLACEMENT = 1,
  R_BAKE_AO = 2,
  R_BAKE_VECTOR_DISPLACEMENT = 3,
};

/** #BakeData::flag */
enum {
  R_BAKE_CLEAR = 1 << 0,
  // R_BAKE_OSA = 1 << 1, /* Deprecated. */
  R_BAKE_TO_ACTIVE = 1 << 2,
  // R_BAKE_NORMALIZE = 1 << 3, /* Deprecated. */
  R_BAKE_MULTIRES = 1 << 4,
  R_BAKE_LORES_MESH = 1 << 5,
  // R_BAKE_VCOL = 1 << 6, /* Deprecated. */
  // R_BAKE_USERSCALE = 1 << 7, /* Deprecated. */
  R_BAKE_CAGE = 1 << 8,
  R_BAKE_SPLIT_MAT = 1 << 9,
  R_BAKE_AUTO_NAME = 1 << 10,
};

/** #BakeData::margin_type (char). */
enum eBakeMarginType {
  R_BAKE_ADJACENT_FACES = 0,
  R_BAKE_EXTEND = 1,
};

/** #BakeData::normal_swizzle (char). */
enum eBakeNormalSwizzle {
  R_BAKE_POSX = 0,
  R_BAKE_POSY = 1,
  R_BAKE_POSZ = 2,
  R_BAKE_NEGX = 3,
  R_BAKE_NEGY = 4,
  R_BAKE_NEGZ = 5,
};

/** #BakeData::target (char). */
enum eBakeTarget {
  R_BAKE_TARGET_IMAGE_TEXTURES = 0,
  R_BAKE_TARGET_VERTEX_COLORS = 1,
};

/** #BakeData::save_mode (char). */
enum eBakeSaveMode {
  R_BAKE_SAVE_INTERNAL = 0,
  R_BAKE_SAVE_EXTERNAL = 1,
};

/** #BakeData::view_from (char). */
enum eBakeViewFrom {
  R_BAKE_VIEW_FROM_ABOVE_SURFACE = 0,
  R_BAKE_VIEW_FROM_ACTIVE_CAMERA = 1,
};

/** #BakeData::pass_filter */
enum eBakePassFilter {
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
};

/** #BakeData::normal_space and #BakeData::displacement_space */
enum eBakeSpace {
  R_BAKE_SPACE_CAMERA = 0,
  R_BAKE_SPACE_WORLD = 1,
  R_BAKE_SPACE_OBJECT = 2,
  R_BAKE_SPACE_TANGENT = 3,
};

#define R_BAKE_PASS_FILTER_ALL (~0)

struct BakeData {
  struct ImageFormatData im_format;

  char filepath[/*FILE_MAX*/ 1024] = "//";

  int type = R_BAKE_NORMALS;

  short width = 512, height = 512;
  short margin = 16, flag = R_BAKE_CLEAR;

  float cage_extrusion = 0;
  float max_ray_distance = 0;
  int pass_filter = R_BAKE_PASS_FILTER_ALL;

  char normal_swizzle[3] = {R_BAKE_POSX, R_BAKE_POSY, R_BAKE_POSZ};
  char normal_space = R_BAKE_SPACE_TANGENT;

  char displacement_space = R_BAKE_SPACE_OBJECT;

  char target = 0;
  char save_mode = 0;
  char margin_type = R_BAKE_ADJACENT_FACES;
  char view_from = 0;

  char _pad[7] = {};

  struct Object *cage_object = nullptr;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Data
 * \{ */

/** #RenderData::quality_flag */
enum eQualityOption {
  SCE_PERF_HQ_NORMALS = (1 << 0),
};

/** #RenderData::hair_type */
enum eHairType {
  SCE_HAIR_SHAPE_STRAND = 0,
  SCE_HAIR_SHAPE_STRIP = 1,
  SCE_HAIR_SHAPE_CYLINDER = 2,
};

/** #RenderData::motion_blur_position */
enum {
  SCE_MB_CENTER = 0,
  SCE_MB_START = 1,
  SCE_MB_END = 2,
};

/** #RenderData::compositor_device */
enum eCompositorDevice {
  SCE_COMPOSITOR_DEVICE_CPU = 0,
  SCE_COMPOSITOR_DEVICE_GPU = 1,
};

/** #RenderData::compositor_precision */
enum eCompositorPrecision {
  SCE_COMPOSITOR_PRECISION_AUTO = 0,
  SCE_COMPOSITOR_PRECISION_FULL = 1,
};

/** #RenderData::compositor_denoise_device */
enum eCompositorDenoiseDevice {
  SCE_COMPOSITOR_DENOISE_DEVICE_AUTO = 0,
  SCE_COMPOSITOR_DENOISE_DEVICE_CPU = 1,
  SCE_COMPOSITOR_DENOISE_DEVICE_GPU = 2,
};

/** #RenderData::compositor_denoise_preview_quality */
/** #RenderData::compositor_denoise_final_quality */
enum eCompositorDenoiseQaulity {
  SCE_COMPOSITOR_DENOISE_HIGH = 0,
  SCE_COMPOSITOR_DENOISE_BALANCED = 1,
  SCE_COMPOSITOR_DENOISE_FAST = 2,
};

/** #RenderData::time_jump_unit */
enum {
  SCE_TIME_JUMP_FRAME = 0,
  SCE_TIME_JUMP_SECOND = 1,
};

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
  R_NO_CAMERA_SWITCH = 1 << 12, /* Disable cache switching */
  R_MODE_UNUSED_13 = 1 << 13,   /* cleared */
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

/** #RenderData::line_thickness_mode */
enum {
  R_LINE_THICKNESS_ABSOLUTE = 1,
  R_LINE_THICKNESS_RELATIVE = 2,
};

struct RenderData {
  DNA_DEFINE_CXX_METHODS(RenderData)

  struct ImageFormatData im_format;

  struct FFMpegCodecData ffcodecdata;

  /** Frames as in 'images'. */
  int cfra = 1, sfra = 1, efra = 250;
  /** Sub-frame offset from `cfra`, in 0.0-1.0. */
  float subframe = 0;
  /** Start+end frames of preview range. */
  int psfra = 0, pefra = 0;

  int images = 100, framapto = 100;
  short flag = 0, threads = 1;

  float framelen = 1.0;

  /** Frames to jump during render/playback. */
  int frame_step = 1;

  /** For the dimensions presets menu. */
  short dimensionspreset = 0;

  /** Size in %. */
  short size = 100;

  /* From buttons: */
  /**
   * The desired number of pixels in the x direction
   */
  int xsch = 1920;
  /**
   * The desired number of pixels in the y direction
   */
  int ysch = 1080;

  /**
   * render tile dimensions
   */
  DNA_DEPRECATED int tilex = 256;
  DNA_DEPRECATED int tiley = 256;

  DNA_DEPRECATED short planes = 0;
  DNA_DEPRECATED short imtype = 0;
  DNA_DEPRECATED short subimtype = 0;
  DNA_DEPRECATED short quality = 0;

  char use_lock_interface = 0;
  char _pad7[3] = {};

  /**
   * Flags for render settings. Use bit-masking to access the settings.
   */
  int scemode = R_DOCOMP | R_DOSEQ | R_EXTENSION;

  /**
   * Flags for render settings. Use bit-masking to access the settings.
   */
  int mode = 0;

  short frs_sec = 24;

  /**
   * What to do with the sky/background.
   * Picks sky/pre-multiply blending for the background.
   */
  char alphamode = 0;

  char _pad0[1] = {};

  /** Render border to render sub-regions. */
  rctf border = {0.0f, 1.0f, 0.0f, 1.0f};

  /* Information on different layers to be rendered. */
  /** Converted to Scene->view_layers. */
  ListBaseT<SceneRenderLayer> layers = {nullptr, nullptr};
  /** Converted to Scene->active_layer. */
  DNA_DEPRECATED short actlay = 0;
  char _pad1[2] = {};

  /**
   * Adjustment factors for the aspect ratio in the x direction, was a short in 2.45
   */
  float xasp = 1, yasp = 1;

  /**
   * Pixels per meter (factor of PPM base).
   * The final calculated PPM is stored as a pair of doubles,
   * taking the render aspect into support separate X/Y density.
   * Editing the final PPM directly isn't practical as common DPI
   * values often result the fractional part having many decimal places.
   * So expose the factor & base, where the base is used to set the "preset" in the GUI,
   * (Inch CM, MM... etc).
   *
   * Once calculated the final PPM is stored in the #ImBuf & #RenderResult
   * which are saved/loaded through #ImBuf API's or multi-layer EXR images
   * in the case of the render-result.
   *
   * Note that storing the X/Y density means it's possible know the aspect
   * used to render the image which may be useful.
   */
  float ppm_factor = 72.0f;
  /**
   * Pixels per meter base (0.0254 for DPI), a multiplier for `ppm_factor`.
   * Used to implement "presets".
   */
  float ppm_base = 0.0254f;

  float frs_sec_base = 1;

  /**
   * Value used to define filter size for all filter options.
   */
  float gauss = 1.5;

  /** Color management settings - color profiles, gamma correction, etc. */
  int color_mgt_flag = R_COLOR_MANAGEMENT;

  /** Dither noise intensity. */
  float dither_intensity = 1.0f;

  /** Legacy Bake Render options. */
  DNA_DEPRECATED short bake_mode = 0;
  DNA_DEPRECATED short bake_flag = 0;
  DNA_DEPRECATED short bake_margin = 0;
  DNA_DEPRECATED short bake_margin_type = 0;

  /**
   * Path to render output.
   * \note Excluded from `BKE_bpath_foreach_path_` / `scene_foreach_path` code.
   */
  char pic[/*FILE_MAX*/ 1024] = "//";

  /** Stamps flags. */
  int stamp = R_STAMP_TIME | R_STAMP_FRAME | R_STAMP_DATE | R_STAMP_CAMERA | R_STAMP_SCENE |
              R_STAMP_FILENAME | R_STAMP_RENDERTIME | R_STAMP_MEMORY;
  /** Select one of blenders bitmap fonts. */
  short stamp_font_id = 12;
  char _pad3[2] = {};

  /** Stamp info user data. */
  char stamp_udata[768] = "";

  /* Foreground/background color. */
  float fg_stamp[4] = {0.8f, 0.8f, 0.8f, 1.0f};
  float bg_stamp[4] = {0.0f, 0.0f, 0.0f, 0.25f};

  /** Sequencer options. */
  char seq_prev_type = OB_SOLID;
  /** Flag use for sequence render/draw. */
  char seq_flag = 0;
  char _pad5[4] = {};

  /* Render simplify. */
  short simplify_subsurf = 6;
  short simplify_subsurf_render = 0;
  short simplify_gpencil = 0;
  float simplify_particles = 1.0f;
  float simplify_particles_render = 0;
  float simplify_volumes = 1.0f;

  /** Freestyle line thickness options. */
  int line_thickness_mode = R_LINE_THICKNESS_ABSOLUTE;
  /** In pixels. */
  float unit_line_thickness = 1.0f;

  /** Render engine. */
  char engine[32] = "";
  char _pad2[2] = {};

  /** Performance Options. */
  short perf_flag = 0;

  /** Baking. */
  struct BakeData bake;

  int _pad8 = {};
  short preview_pixel_size = 0;

  short _pad4 = {};

  /* MultiView. */
  ListBaseT<SceneRenderView> views = {nullptr, nullptr};
  short actview = 0;
  short views_format = 0;

  /* Hair Display. */
  short hair_type = 0, hair_subdiv = 0;

  /** Motion blur */
  float motion_blur_shutter = 0.5f;
  int motion_blur_position = 0;
  struct CurveMapping mblur_shutter_curve;

  /** Device to use for compositor engine. */
  int compositor_device = 0; /* eCompositorDevice */

  /** Precision used by the GPU execution of the compositor tree. */
  int compositor_precision = 0; /* eCompositorPrecision */

  /** Device to use for denoise nodes in the compositor. */
  int compositor_denoise_device = 0; /* eCompositorDenoiseDevice */

  /** Global configuration for denoise compositor nodes. */
  int compositor_denoise_preview_quality =
      SCE_COMPOSITOR_DENOISE_BALANCED; /* eCompositorDenoiseQaulity */
  int compositor_denoise_final_quality =
      SCE_COMPOSITOR_DENOISE_HIGH; /* eCompositorDenoiseQaulity */

  /** Frames to jump manually. */
  float time_jump_delta = 1.0;
  int time_jump_unit = 1;
  char _pad10[4] = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Time Line Markers
 * \{ */

struct TimeMarker {
  struct TimeMarker *next = nullptr, *prev = nullptr;
  int frame = 0;
  char name[64] = "";
  unsigned int flag = 0;
  struct Object *camera = nullptr;
  struct IDProperty *prop = nullptr;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unified Paint Settings
 * \{ */

/**
 * These settings can override the equivalent fields in the active
 * Brush for any paint mode; the flag field controls whether these
 * values are used
 */
struct UnifiedPaintSettings {
  DNA_DEFINE_CXX_METHODS(UnifiedPaintSettings)

  /** Unified diameter of brush in pixels. */
  int size = 100;

  /** Unified diameter of brush in Blender units. */
  float unprojected_size = 0.58;

  /** Unified strength of brush. */
  float alpha = 0.5f;

  /** Unified brush weight, [0, 1]. */
  float weight = 0.5f;

  /** Unified brush color. */
  float color[3] = {0.0f, 0.0f, 0.0f};
  /** Unified brush secondary color. */
  float secondary_color[3] = {1.0f, 1.0f, 1.0f};

  /* Deprecated sRGB color for forward compatibility. */
  DNA_DEPRECATED float rgb[3] = {0.0f, 0.0f, 0.0f};
  DNA_DEPRECATED float secondary_rgb[3] = {1.0f, 1.0f, 1.0f};

  /** Unified color jitter settings */
  int color_jitter_flag = 0;
  float hsv_jitter[3] = {};

  /** Color jitter pressure curves. */
  struct CurveMapping *curve_rand_hue = nullptr;
  struct CurveMapping *curve_rand_saturation = nullptr;
  struct CurveMapping *curve_rand_value = nullptr;

  /** Unified brush stroke input samples. */
  int input_samples = 1;

  /** User preferences for sculpt and paint. */
  int flag = UNIFIED_PAINT_SIZE | UNIFIED_PAINT_COLOR;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Mode/Tool Data
 * \{ */

#define PAINT_MAX_INPUT_SAMPLES 64

struct NamedBrushAssetReference {
  struct NamedBrushAssetReference *next = nullptr, *prev = nullptr;

  const char *name = nullptr;
  struct AssetWeakReference *brush_asset_reference = nullptr;
};

/**
 * For the tool system: Storage to remember the last active brush for specific tools.
 *
 * This stores a "main" brush reference, which is used for any tool that uses brushes but isn't
 * limited to a specific brush type, and a list of brush references identified by the brush type,
 * for tools that are limited to a brush type.
 *
 * The tool system updates these fields as the active brush or active tool changes. It also
 * determines the brush to remember/restore on tool changes and activates it.
 */
struct ToolSystemBrushBindings {
  struct AssetWeakReference *main_brush_asset_reference = nullptr;

  /**
   * The tool system exposes tools for some brush types, like an eraser tool to access eraser
   * brushes. Switching between tools should remember the last used brush for a brush type, e.g.
   * which eraser was used last by the eraser tool.
   *
   * Note that multiple tools may use the same brush type, for example primitive draw tools (to
   * draw rectangles, circles, lines, etc.) all use a "DRAW" brush, which will then be shared
   * among them.
   */
  ListBaseT<NamedBrushAssetReference> active_brush_per_brush_type = {nullptr, nullptr};
};

/** #Paint::flags */
enum ePaintFlags {
  PAINT_SHOW_BRUSH = (1 << 0),
  PAINT_FAST_NAVIGATE = (1 << 1),
  PAINT_SHOW_BRUSH_ON_SURFACE = (1 << 2),
  PAINT_USE_CAVITY_MASK = (1 << 3),
  PAINT_SCULPT_DELAY_UPDATES = (1 << 4),
};

/** #Paint::debug_flags */
enum ePaintDebugFlags {
  PAINT_DEBUG_SHOW_BVH_NODES = (1 << 0),
};

/** #PaintModeSettings::mode */
enum ePaintCanvasSource {
  /** Paint on the active node of the active material slot. */
  PAINT_CANVAS_SOURCE_MATERIAL = 0,
  /** Paint on a selected image. */
  PAINT_CANVAS_SOURCE_IMAGE = 1,
  /** Paint on the active color attribute (vertex color) layer. */
  PAINT_CANVAS_SOURCE_COLOR_ATTRIBUTE = 2,
};

/** Paint Tool Base. */
struct Paint {
  DNA_DEFINE_CXX_METHODS(Paint)

  /**
   * The active brush. Possibly null. Possibly stored in a separate #Main data-base and not user-
   * counted.
   */
  struct Brush *brush = nullptr;

  /**
   * A weak asset reference to the #brush, if not NULL.
   * Used to attempt restoring the active brush from the AssetLibrary system, typically on
   * file load.
   */
  struct AssetWeakReference *brush_asset_reference = nullptr;

  /** Default eraser brush and associated weak reference. */
  struct Brush *eraser_brush = nullptr;
  struct AssetWeakReference *eraser_brush_asset_reference = nullptr;

  ToolSystemBrushBindings tool_brush_bindings;

  struct Palette *palette = nullptr;
  /** Cavity curve. */
  struct CurveMapping *cavity_curve = nullptr;

  /** Enum #ePaintFlags. */
  int flags = PAINT_SHOW_BRUSH;
  /** Enum #ePaintDebugFlags. */
  int debug_flags = 0;

  /**
   * Paint stroke can use up to #PAINT_MAX_INPUT_SAMPLES inputs to smooth the stroke.
   * This value is deprecated. Refer to the #Brush and #UnifiedPaintSetting values instead.
   */
  int num_input_samples_deprecated = 0;

  /** Flags used for symmetry. */
  int symmetry_flags = PAINT_SYMMETRY_FEATHER;
  /**
   * Collapsed state of a given pressure curve
   * See #PaintCurveVisibilityFlags
   */
  int curve_visibility_flags = 0;

  float tile_offset[3] = {1.0f, 1.0f, 1.0f};
  struct UnifiedPaintSettings unified_paint_settings;

  bke::PaintRuntime *runtime = nullptr;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Paint
 * \{ */

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

/** Texture/Image Editor. */
struct ImagePaintSettings {
  Paint paint;

  short flag = 0, missing_data = 0;

  /** For projection painting only. */
  short seam_bleed = 2, normal_angle = 80;
  /** Capture size for re-projection. */
  short screen_grab_size[2] = {};

  /** Mode used for texture painting. */
  int mode = 0;

  /** Workaround until we support true layer masks. */
  struct Image *stencil = nullptr;
  /** Clone layer for image mode for projective texture painting. */
  struct Image *clone = nullptr;
  /** Canvas when the explicit system is used for painting. */
  struct Image *canvas = nullptr;
  float stencil_col[3] = {};
  /** Dither amount used when painting on byte images. */
  float dither = 0;
  /** Display texture interpolation method. */
  int interp = 0;
  char _pad[4] = {};
  /** Offset of clone image from canvas in Image editor. */
  float clone_offset[2] = {};
  /** Transparency for drawing of clone image in Image editor. */
  float clone_alpha = 0.5f;
  char _pad2[4] = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Mode Settings
 * \{ */

struct PaintModeSettings {
  /** Source to select canvas from to paint on (#ePaintCanvasSource). */
  char canvas_source = 0;
  char _pad[7] = {};

  /** Selected image when canvas_source=PAINT_CANVAS_SOURCE_IMAGE. */
  Image *canvas_image = nullptr;
  ImageUser image_user;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle Edit
 * \{ */

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

/** Settings for a Particle Editing Brush. */
struct ParticleBrushData {
  /** Common setting. */
  short size = 50;
  /** For specific brushes only. */
  short step = 10, invert = 0, count = 10;
  int flag = 0;
  float strength = 0.5f;
};

/** #ParticleEditSettings::selectmode for particles */
enum {
  SCE_SELECT_PATH = 1 << 0,
  SCE_SELECT_POINT = 1 << 1,
  SCE_SELECT_END = 1 << 2,
};

/** Particle Edit Mode Settings. */
struct ParticleEditSettings {
  short flag = PE_KEEP_LENGTHS | PE_LOCK_FIRST | PE_DEFLECT_EMITTER | PE_AUTO_VELOCITY;
  short totrekey = 5;
  short totaddkey = 5;
  short brushtype = PE_BRUSH_COMB;

  ParticleBrushData brush[7];
  /** Runtime. */
  void *paintcursor = nullptr;

  float emitterdist = 0.25f;
  char _pad0[4] = {};

  int selectmode = SCE_SELECT_PATH;
  int edittype = 0;

  int draw_step = 2, fade_frames = 2;

  struct Scene *scene = nullptr;
  struct Object *object = nullptr;
  struct Object *shape_object = nullptr;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt
 * \{ */

/**
 * #Sculpt::flags
 * These can eventually be moved to paint flags?
 */
enum eSculptFlags {
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
  SCULPT_FLAG_UNUSED_8 = (1 << 10), /* deprecated */

  /** If set, dynamic-topology brushes will subdivide short edges. */
  SCULPT_DYNTOPO_SUBDIVIDE = (1 << 12),
  /** If set, dynamic-topology brushes will collapse short edges. */
  SCULPT_DYNTOPO_COLLAPSE = (1 << 11),

  /** If set, dynamic-topology detail size will be constant in object space. */
  SCULPT_DYNTOPO_DETAIL_CONSTANT = (1 << 13),
  SCULPT_DYNTOPO_DETAIL_BRUSH = (1 << 14),
  /* unused = (1 << 15), */
  SCULPT_DYNTOPO_DETAIL_MANUAL = (1 << 16),
};

/** #Sculpt::transform_mode */
enum eSculptTransformMode {
  SCULPT_TRANSFORM_MODE_ALL_VERTICES = 0,
  SCULPT_TRANSFORM_MODE_RADIUS_ELASTIC = 1,
};

/** Sculpt. */
struct Sculpt {
  DNA_DEFINE_CXX_METHODS(Sculpt)

  Paint paint;

  /** For rotating around a pivot point. */
  // float pivot[3] = {}; XXX not used?
  int flags = SCULPT_DYNTOPO_SUBDIVIDE | SCULPT_DYNTOPO_COLLAPSE;

  /** Transform tool. */
  int transform_mode = 0;

  int automasking_flags = 0;

  // /* Control tablet input. */
  // char tablet_size = {}, tablet_strength = {}; XXX not used?
  int radial_symm_legacy[3] = {};

  /** Maximum edge length for dynamic topology sculpting (in pixels). */
  float detail_size = 12;

  /** Direction used for `SCULPT_OT_symmetrize` operator. */
  int symmetrize_direction = 0;

  /** Gravity factor for sculpting. */
  float gravity_factor = 0;

  /* Scale for constant detail size. */
  /** Constant detail resolution (Blender unit / constant_detail). */
  float constant_detail = 3.0f;
  float detail_percent = 25;

  int automasking_boundary_edges_propagation_steps = 1;
  int automasking_cavity_blur_steps = 0;
  float automasking_cavity_factor = 0;

  float automasking_start_normal_limit = 0.34906585f; /* 20 / 180 * pi. */
  float automasking_start_normal_falloff = 0.25f;
  float automasking_view_normal_limit = 1.570796; /* 0.5 * pi. */
  float automasking_view_normal_falloff = 0.25f;

  struct CurveMapping *automasking_cavity_curve = nullptr;
  /** For use by operators. */
  struct CurveMapping *automasking_cavity_curve_op = nullptr;
  struct Object *gravity_object = nullptr;
};

struct CurvesSculpt {
  Paint paint;
};

struct UvSculpt {
  struct CurveMapping *curve_distance_falloff = nullptr;
  int size = 100;
  float strength = 1.0f;
  int8_t curve_distance_falloff_preset = BRUSH_CURVE_SMOOTH; /* #eBrushCurvePreset. */
  char _pad[7] = {};
};

/** Grease pencil drawing brushes. */
struct GpPaint {
  Paint paint;
  int flag = 0;
  /** Mode of paint (Materials or Vertex Color). */
  int mode = 0;
};

/** Grease pencil vertex paint. */
struct GpVertexPaint {
  Paint paint;
  int flag = 0;
  char _pad[4] = {};
};

/** Grease pencil sculpt paint. */
struct GpSculptPaint {
  Paint paint;
  int flag = 0;
  char _pad[4] = {};
};

/** Grease pencil weight paint. */
struct GpWeightPaint {
  Paint paint;
  int flag = 0;
  char _pad[4] = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Paint
 * \{ */

/** Vertex Paint. */
struct VPaint {
  Paint paint;
  char flag = 0;
  char _pad[7] = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease-Pencil Stroke Sculpting
 * \{ */

/** #GP_Sculpt_Settings::lock_axis */
enum eGP_Lockaxis_Types {
  GP_LOCKAXIS_VIEW = 0,
  GP_LOCKAXIS_X = 1,
  GP_LOCKAXIS_Y = 2,
  GP_LOCKAXIS_Z = 3,
  GP_LOCKAXIS_CURSOR = 4,
};

/** #GP_Sculpt_Settings::flag */
enum eGP_Sculpt_SettingsFlag {
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
};

/** #GP_Sculpt_Settings::gpencil_selectmode_sculpt */
enum eGP_Sculpt_SelectMaskFlag {
  /** Only affect selected points. */
  GP_SCULPT_MASK_SELECTMODE_POINT = (1 << 0),
  /** Only affect selected strokes. */
  GP_SCULPT_MASK_SELECTMODE_STROKE = (1 << 1),
  /** Only affect selected segments. */
  GP_SCULPT_MASK_SELECTMODE_SEGMENT = (1 << 2),
};

/** #GP_Sculpt_Settings::gpencil_selectmode_vertex */
enum eGP_Vertex_SelectMaskFlag {
  /** Only affect selected points. */
  GP_VERTEX_MASK_SELECTMODE_POINT = (1 << 0),
  /** Only affect selected strokes. */
  GP_VERTEX_MASK_SELECTMODE_STROKE = (1 << 1),
  /** Only affect selected segments. */
  GP_VERTEX_MASK_SELECTMODE_SEGMENT = (1 << 2),
};

/** #GP_Interpolate_Settings::flag */
enum eGP_Interpolate_SettingsFlag {
  /** Apply interpolation to all layers. */
  GP_TOOLFLAG_INTERPOLATE_ALL_LAYERS = (1 << 0),
  /** Apply interpolation to only selected. */
  GP_TOOLFLAG_INTERPOLATE_ONLY_SELECTED = (1 << 1),
  /** Exclude breakdown keyframe type as extreme. */
  GP_TOOLFLAG_INTERPOLATE_EXCLUDE_BREAKDOWNS = (1 << 2),
};

/** #GP_Interpolate_Settings::type */
enum eGP_Interpolate_Type {
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
};

/** Settings for a GPencil Speed Guide. */
struct GP_Sculpt_Guide {
  char use_guide = 0;
  char use_snapping = 0;
  char reference_point = 0;
  char type = 0;
  char _pad2[4] = {};
  float angle = 0;
  float angle_snap = 0;
  float spacing = 20.0f;
  float location[3] = {};
  struct Object *reference_object = nullptr;
};

/** GPencil Stroke Sculpting Settings. */
struct GP_Sculpt_Settings {
  /** Runtime. */
  void *paintcursor = nullptr;
  /** #eGP_Sculpt_SettingsFlag. */
  int flag = 0;
  /** #eGP_Lockaxis_Types lock drawing to one axis. */
  int lock_axis = 0;
  /** Threshold for intersections. */
  float isect_threshold = 0;
  char _pad[4] = {};
  /** Multi-frame edit falloff effect by frame. */
  struct CurveMapping *cur_falloff = nullptr;
  /** Curve used for primitive tools. */
  struct CurveMapping *cur_primitive = nullptr;
  /** Guides used for paint tools. */
  struct GP_Sculpt_Guide guide;
};

/** Settings for GP Interpolation Operators. */
struct GP_Interpolate_Settings {
  /** Custom interpolation curve (for use with GP_IPO_CURVEMAP). */
  struct CurveMapping *custom_ipo = nullptr;
};

/** #CurvePaintSettings::flag */
enum {
  CURVE_PAINT_FLAG_CORNERS_DETECT = (1 << 0),
  CURVE_PAINT_FLAG_PRESSURE_RADIUS = (1 << 1),
  CURVE_PAINT_FLAG_DEPTH_STROKE_ENDPOINTS = (1 << 2),
  CURVE_PAINT_FLAG_DEPTH_STROKE_OFFSET_ABS = (1 << 3),
  CURVE_PAINT_FLAG_DEPTH_ONLY_SELECTED = (1 << 4),
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

/** #CurvePaintSettings::surface_plane */
enum {
  AUTO_MERGE = 1 << 0,
  AUTO_MERGE_AND_SPLIT = 1 << 1,
};

struct CurvePaintSettings {
  char curve_type = CU_BEZIER;
  char flag = CURVE_PAINT_FLAG_CORNERS_DETECT;
  char depth_mode = 0;
  char surface_plane = 0;
  char fit_method = 0;
  char _pad = {};
  short error_threshold = 8;
  float radius_min = 0, radius_max = 1.0f;
  float radius_taper_start = 0, radius_taper_end = 0;
  float surface_offset = 0;
  float corner_angle = DEG2RADF(70.0f);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Visualization
 * \{ */

/** Stats for Meshes. */
struct MeshStatVis {
  char type = 0;
  char _pad1[2] = {};

  /* Overhang. */
  char overhang_axis = OB_NEGZ;
  float overhang_min = 0, overhang_max = DEG2RADF(45.0f);

  /* Thickness. */
  float thickness_min = 0, thickness_max = 0.1f;
  char thickness_samples = 1;
  char _pad2[3] = {};

  /* Distort. */
  float distort_min = DEG2RADF(5.0f), distort_max = DEG2RADF(45.0f);

  /* Sharp. */
  float sharp_min = DEG2RADF(90.0f), sharp_max = DEG2RADF(180.0f);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequencer Tool Settings
 * \{ */

/** #SequencerToolSettings::snap_mode */
enum eSequencerSnapMode {
  SEQ_SNAP_TO_STRIPS = 1 << 0,
  SEQ_SNAP_TO_CURRENT_FRAME = 1 << 1,
  SEQ_SNAP_TO_STRIP_HOLD = 1 << 2,
  SEQ_SNAP_TO_MARKERS = 1 << 3,

  /* Preview snapping. */
  SEQ_SNAP_TO_PREVIEW_BORDERS = 1 << 4,
  SEQ_SNAP_TO_PREVIEW_CENTER = 1 << 5,
  SEQ_SNAP_TO_STRIPS_PREVIEW = 1 << 6,

  SEQ_SNAP_TO_RETIMING = 1 << 7,
  SEQ_SNAP_TO_INCREMENT = 1 << 8, /* NOTE: Treated identically to `SCE_SNAP_TO_INCREMENT`. */
  SEQ_SNAP_TO_FRAME_RANGE = 1 << 9,
};

/** #SequencerToolSettings::snap_flag */
enum eSequencerSnapFlag {
  SEQ_SNAP_IGNORE_MUTED = 1 << 0,
  SEQ_SNAP_IGNORE_SOUND = 1 << 1,
  SEQ_SNAP_CURRENT_FRAME_TO_STRIPS = 1 << 2,
};

struct SequencerToolSettings {
  /** #eSeqImageFitMethod. */
  int fit_method = 0;
  short snap_mode = 0;
  short snap_flag = 0;
  /** #eSeqOverlapMode. */
  int overlap_mode = 0;
  /**
   * When there are many snap points,
   * 0-1 range corresponds to resolution from bound-box to all possible snap points.
   */
  int snap_distance = 0;
  int pivot_point = 0;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tool Settings
 * \{ */

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
enum eSnapFlag {
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
};
ENUM_OPERATORS(eSnapFlag)

/** See #ToolSettings::snap_target (to be renamed `snap_source`) and #TransSnap.source_operation */
enum eSnapSourceOP {
  SCE_SNAP_SOURCE_CLOSEST = 0,
  SCE_SNAP_SOURCE_CENTER = 1,
  SCE_SNAP_SOURCE_MEDIAN = 2,
  SCE_SNAP_SOURCE_ACTIVE = 3,
};
ENUM_OPERATORS(eSnapSourceOP)

/**
 * #TransSnap::target_operation and #ToolSettings::snap_flag
 * (#SCE_SNAP_NOT_TO_ACTIVE, #SCE_SNAP_TO_INCLUDE_EDITED, #SCE_SNAP_TO_INCLUDE_NONEDITED,
 * #SCE_SNAP_TO_ONLY_SELECTABLE).
 */
enum eSnapTargetOP {
  SCE_SNAP_TARGET_ALL = 0,
  SCE_SNAP_TARGET_NOT_SELECTED = (1 << 0),
  SCE_SNAP_TARGET_NOT_ACTIVE = (1 << 1),
  SCE_SNAP_TARGET_NOT_EDITED = (1 << 2),
  SCE_SNAP_TARGET_ONLY_SELECTABLE = (1 << 3),
  SCE_SNAP_TARGET_NOT_NONEDITED = (1 << 4),
};
ENUM_OPERATORS(eSnapTargetOP)

/** #ToolSettings::snap_mode */
enum eSnapMode {
  SCE_SNAP_TO_NONE = 0,

  /** #ToolSettings::snap_anim_mode and #ToolSettings::snap_playhead_mode. */
  SCE_SNAP_TO_FRAME = (1 << 0),
  SCE_SNAP_TO_SECOND = (1 << 1),
  SCE_SNAP_TO_MARKERS = (1 << 2),
  SCE_SNAP_TO_KEYS = (1 << 3),
  SCE_SNAP_TO_STRIPS = (1 << 4),

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

  SCE_SNAP_TO_FACE_MIDPOINT = (1 << 11)
};
ENUM_OPERATORS(eSnapMode)

#define SCE_SNAP_TO_VERTEX (SCE_SNAP_TO_POINT | SCE_SNAP_TO_EDGE_ENDPOINT)

#define SCE_SNAP_TO_GEOM \
  (SCE_SNAP_TO_VERTEX | SCE_SNAP_TO_EDGE | SCE_SNAP_TO_FACE | SCE_SNAP_TO_FACE_MIDPOINT | \
   SCE_SNAP_TO_EDGE_MIDPOINT | SCE_SNAP_TO_EDGE_PERPENDICULAR)

/* UV Paint. */
/** #ToolSettings::uv_sculpt_settings */
enum {
  UV_SCULPT_LOCK_BORDERS = 1,
  UV_SCULPT_ALL_ISLANDS = 2,
};

/** #GpPaint::flag */
enum {
  GPPAINT_FLAG_USE_MATERIAL = 0,
  GPPAINT_FLAG_USE_VERTEXCOLOR = 1,
};

/** #VPaint::flag */
enum {
  /** Weight paint only. */
  VP_FLAG_VGROUP_RESTRICT = (1 << 7),
};

enum eSeqOverlapMode {
  SEQ_OVERLAP_EXPAND,
  SEQ_OVERLAP_OVERWRITE,
  SEQ_OVERLAP_SHUFFLE,
};

/** #ToolSettings::snap_transform_mode_flag */
enum eSnapTransformMode {
  SCE_SNAP_TRANSFORM_MODE_TRANSLATE = (1 << 0),
  SCE_SNAP_TRANSFORM_MODE_ROTATE = (1 << 1),
  SCE_SNAP_TRANSFORM_MODE_SCALE = (1 << 2),
};

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

/** #ToolSettings::unwrapper */
enum {
  UVCALC_UNWRAP_METHOD_ANGLE = 0,
  UVCALC_UNWRAP_METHOD_CONFORMAL = 1,
  UVCALC_UNWRAP_METHOD_MINIMUM_STRETCH = 2,
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
  /** Prevent unwrap that flips. */
  UVCALC_UNWRAP_NO_FLIP = 1 << 6,
  /** Use importance weights. */
  UVCALC_UNWRAP_USE_WEIGHTS = 1 << 7,
};

/** #ToolSettings::uv_flag */
enum {
  UV_FLAG_SELECT_SYNC = 1 << 0,
  UV_FLAG_SHOW_SAME_IMAGE = 1 << 1,
  /**
   * \note In most cases #ED_uvedit_select_island_check should be used to check if island
   * selection should be used - since not all combinations of options support it.
   */
  UV_FLAG_SELECT_ISLAND = 1 << 2,
  UV_FLAG_CUSTOM_REGION = 1 << 3,
};

/** #ToolSettings::uv_selectmode */
enum {
  UV_SELECT_VERT = 1 << 0,
  UV_SELECT_EDGE = 1 << 1,
  UV_SELECT_FACE = 1 << 2,
};

/**
 * #ToolSettings::uv_sticky
 *
 * Control the behavior of selecting UV's in the UV editor.
 *
 * Internally UV's store selection for every face-corner,
 * however for the purpose of conveniently selecting & editing UV's it's often
 * preferable to use sticky selection (#UV_STICKY_LOCATION),
 * where selecting a UV also selects other UV's at the same location.
 *
 * \note This setting only affects subsequent selection operations.
 * It does not alter the current selection state.
 */
enum {
  /**
   * Treat all other UV's sharing the vertex at that location as a single UV.
   * This is the default behavior.
   *
   * \note Ripping UV's apart is still possible with "Split" & "Rip" operators.
   */
  UV_STICKY_LOCATION = 0,
  /**
   * Treat all UV's as individual face-corners, no matter where they are located.
   * This can be useful if the intention with UV editing is to manipulate each faces
   * UV's independently of one another.
   *
   * \note This is impractical for typical usage as it's impractical
   * to select and move a single UV connected to other UV chordates.
   */
  UV_STICKY_DISABLE = 1,
  /**
   * Selecting applies to all UV's sharing a vertex.
   * This can be useful to weld UV's that share a vertex but have become separated.
   *
   * \note This is impractical for typical usage since selecting UV's at island-boundaries
   * selects UV's of any other UV island-boundaries which share that vertex.
   */
  UV_STICKY_VERT = 2,
};

/** #ToolSettings::gpencil_flags */
enum eGPencil_Flags {
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
};

/** #Scene::r.simplify_gpencil */
enum eGPencil_SimplifyFlags {
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
};

/** `ToolSettings.gpencil_*_align` - Stroke Placement mode flags. */
enum eGPencil_Placement_Flags {
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

  /** Surface project, "Only project on selected objects". */
  GP_PROJECT_DEPTH_ONLY_SELECTED = (1 << 7),
};

/** #ToolSettings::gpencil_selectmode */
enum eGPencil_Selectmode_types {
  GP_SELECTMODE_POINT = 0,
  GP_SELECTMODE_STROKE = 1,
  GP_SELECTMODE_SEGMENT = 2,
};

/** #ToolSettings::gpencil_guide_types */
enum eGPencil_GuideTypes {
  GP_GUIDE_CIRCULAR = 0,
  GP_GUIDE_RADIAL = 1,
  GP_GUIDE_PARALLEL = 2,
  GP_GUIDE_GRID = 3,
  GP_GUIDE_ISO = 4,
};

/** #ToolSettings::gpencil_guide_references */
enum eGPencil_Guide_Reference {
  GP_GUIDE_REF_CURSOR = 0,
  GP_GUIDE_REF_CUSTOM = 1,
  GP_GUIDE_REF_OBJECT = 2,
};

struct ToolSettings {
  DNA_DEFINE_CXX_METHODS(ToolSettings)

  /** Vertex paint. */
  VPaint *vpaint = nullptr;
  /** Weight paint. */
  VPaint *wpaint = nullptr;
  Sculpt *sculpt = nullptr;
  /** UV smooth. */
  UvSculpt uvsculpt;
  /** Gpencil paint. */
  GpPaint *gp_paint = nullptr;
  /** Gpencil vertex paint. */
  GpVertexPaint *gp_vertexpaint = nullptr;
  /** Gpencil sculpt paint. */
  GpSculptPaint *gp_sculptpaint = nullptr;
  /** Gpencil weight paint. */
  GpWeightPaint *gp_weightpaint = nullptr;
  /** Curves sculpt. */
  CurvesSculpt *curves_sculpt = nullptr;

  /** Vertex group weight - used only for editmode, not weight paint. */
  float vgroup_weight = 1.0f;

  /** Remove doubles limit. */
  float doublimit = 0.001;
  char automerge = 0;
  char object_flag = SCE_OBJECT_MODE_LOCK;

  /** Selection Mode for Mesh. */
  char selectmode = SCE_SELECT_VERTEX;

  /* UV Calculation. */

  /* Use `UVCALC_UNWRAP_METHOD_*` values. */
  char unwrapper = UVCALC_UNWRAP_METHOD_CONFORMAL;
  char uvcalc_flag = UVCALC_TRANSFORM_CORRECT_SLIDE;
  char uv_flag = UV_FLAG_SELECT_SYNC;
  char uv_selectmode = UV_SELECT_VERT;
  char uv_sticky = 0;

  rctf uv_custom_region = {};

  float uvcalc_margin = 0.001f;

  int uvcalc_iterations = 10;
  float uvcalc_weight_factor = 1.0;

  /**
   * Regarding having a single vertex group for all meshes.
   * In most cases there is no expectation for the names used for vertex groups.
   * UV weights is a fairly specific feature for unwrapping and in this case
   * users are expected to use the name `uv_importance`.
   * While we could support setting a different group per mesh (similar to the active group).
   * This isn't all that useful in practice, so use a "default" name instead.
   * This approach may be reworked after gathering feedback from users.
   */
  char uvcalc_weight_group[/*MAX_VGROUP_NAME*/ 64] = "uv_importance";

  /* Auto-IK. */
  /** Runtime only. */
  short autoik_chainlen = 0;

  /* Grease Pencil. */
  /** Flags/options for how the tool works. */
  char gpencil_flags = 0;

  /** Stroke placement settings: 3D View. */
  char gpencil_v3d_align = GP_PROJECT_VIEWSPACE;
  /** General 2D Editor. */
  char gpencil_v2d_align = GP_PROJECT_VIEWSPACE;

  /* Annotations. */
  /** Stroke placement settings - 3D View. */
  char annotate_v3d_align = GP_PROJECT_VIEWSPACE | GP_PROJECT_CURSOR;
  /** Default stroke thickness for annotation strokes. */
  short annotate_thickness = 3;

  /** Normal offset used when drawing on surfaces. */
  float gpencil_surface_offset = 0;

  /** Stroke selection mode for Edit. */
  char gpencil_selectmode_edit = 0;
  /** Stroke selection mode for Sculpt. */
  char gpencil_selectmode_sculpt = 0;
  char _pad0[6] = {};

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
  float proportional_size = 1.0f;

  /** Select Group Threshold. */
  float select_thresh = 0.01f;

  /* Keying Settings. */
  /** Defines in DNA_userdef_types.h. */
  short keying_flag = 0;
  char autokey_mode = AUTOKEY_MODE_NORMAL;
  /** Keyframe type (see DNA_curve_types.h). */
  char keyframe_type = 0;

  /** Edge tagging, store operator settings (no UI access). */
  char edge_mode = 0;

  char edge_mode_live_unwrap = 0;

  /* Transform. */

  char transform_pivot_point = V3D_AROUND_CENTER_MEDIAN;
  char transform_flag = 0;
  /** Snap elements (per space-type), #eSnapMode. */
  char snap_node_mode = SCE_SNAP_TO_GRID;

  char _pad = {};

  short snap_mode = SCE_SNAP_TO_INCREMENT;
  short snap_uv_mode = SCE_SNAP_TO_INCREMENT;
  short snap_anim_mode = SCE_SNAP_TO_FRAME;
  short snap_playhead_mode = SCE_SNAP_TO_KEYS | SCE_SNAP_TO_STRIPS;
  /** Generic flags (per space-type), #eSnapFlag. */
  short snap_flag = SCE_SNAP_TO_INCLUDE_EDITED | SCE_SNAP_TO_INCLUDE_NONEDITED;
  short snap_flag_node = 0;
  short snap_flag_seq = SCE_SNAP;
  short snap_flag_anim = SCE_SNAP;
  short snap_flag_driver = 0;
  short snap_flag_playhead = 0;
  short snap_uv_flag = 0;
  /** Default snap source, #eSnapSourceOP. */
  /**
   * TODO(@gfxcoder): Rename `snap_target` to `snap_source` to avoid previous ambiguity of
   * "target" (now, "source" is geometry to be moved and "target" is geometry to which moved
   * geometry is snapped).
   */
  char snap_target = 0;
  /** Snap mask for transform modes, #eSnapTransformMode. */
  char snap_transform_mode_flag = SCE_SNAP_TRANSFORM_MODE_TRANSLATE;
  /** Steps to break transformation into with face nearest snapping. */
  short snap_face_nearest_steps = 1;

  char proportional_edit = 0, prop_mode = 0;
  /** Proportional edit, object mode. */
  char proportional_objects = 0;
  /** Proportional edit, mask editing. */
  char proportional_mask = 0;
  /** Proportional edit, action editor. */
  char proportional_action = 0;
  /** Proportional edit, graph editor. */
  char proportional_fcurve = 0;
  /** Lock marker editing. */
  char lock_markers = 0;

  /** Auto normalizing mode in wpaint. */
  char auto_normalize = 0;
  /** Present weights as if all locked vertex groups were
   *  deleted, and the remaining deform groups normalized. */
  char wpaint_lock_relative = 0;
  /** Paint multiple bones in wpaint. */
  char multipaint = 0;
  char weightuser = OB_DRAW_GROUPUSER_ACTIVE;
  /** Subset selection filter in wpaint. */
  char vgroupsubset = 0;

  /** Stroke selection mode for Vertex Paint. */
  char gpencil_selectmode_vertex = 0;

  /* UV painting. */
  char uv_sculpt_settings = 0;

  char workspace_tool_type = 0;

  char _pad5[7] = {};

  /**
   * XXX: these `sculpt_paint_*` fields are deprecated, use the
   * unified_paint_settings field instead!
   */
  DNA_DEPRECATED short sculpt_paint_settings = 0;
  DNA_DEPRECATED int sculpt_paint_unified_size = 0;
  DNA_DEPRECATED float sculpt_paint_unified_unprojected_radius = 0;
  DNA_DEPRECATED float sculpt_paint_unified_alpha = 0;

  /**
   * Unified Paint Settings.
   * \warning Deprecated, see the per-paint mode values on the `Paint` struct.
   */
  DNA_DEPRECATED struct UnifiedPaintSettings unified_paint_settings;

  struct CurvePaintSettings curve_paint_settings;

  struct MeshStatVis statvis;

  /** Normal Editing. */
  float normal_vector[3] = {};
  char _pad6[4] = {};

  /**
   * Custom Curve Profile for bevel tool:
   * Temporary until there is a proper preset system that stores the profiles or maybe stores
   * entire bevel configurations.
   */
  struct CurveProfile *custom_bevel_profile_preset = nullptr;

  struct SequencerToolSettings *sequencer_tool_settings = nullptr;

  short snap_mode_tools =
      SCE_SNAP_TO_GEOM;  /* If SCE_SNAP_TO_NONE, use #ToolSettings::snap_mode. #eSnapMode. */
  char plane_axis = 2;   /* X, Y or Z. */
  char plane_depth = 0;  /* #eV3DPlaceDepth. */
  char plane_orient = 0; /* #eV3DPlaceOrient. */
  char use_plane_axis_auto = 0;
  char _pad7[2] = {};

  /** Rotation Angle snapping amount */
  float snap_angle_increment_2d = DEG2RADF(5.0f);
  float snap_angle_increment_2d_precision = DEG2RADF(1.0f);
  float snap_angle_increment_3d = DEG2RADF(5.0f);
  float snap_angle_increment_3d_precision = DEG2RADF(1.0f);

  int16_t snap_step_seconds = 1;
  int16_t snap_step_frames = 2;
  /* Pixel threshold that needs to be crossed before the playhead is snapped to a point. */
  int playhead_snap_distance = 20;

  /* Animation settings, used by "Paste Global Transform" operator. */
  struct Object *anim_mirror_object = nullptr;
  struct Object *anim_relative_object = nullptr;
  char anim_mirror_bone[64] = "";

  /* Flags for "Fix to Camera" operator. */
  uint8_t fix_to_cam_flag = FIX_TO_CAM_FLAG_USE_LOC | FIX_TO_CAM_FLAG_USE_ROT |
                            FIX_TO_CAM_FLAG_USE_SCALE; /* eFixToCam_Flags */
  char _pad8[7] = {};
};

/** \} */

/* Assorted Scene Data. */

/* -------------------------------------------------------------------- */
/** \name Unit Settings
 * \{ */

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

/** Display/Editing unit options for each scene. */
struct UnitSettings {

  /* Maybe have other unit conversions? */
  /**
   * Spatial scale.
   * - This must not be used when `system == USER_UNIT_NONE`.
   * - Typically the scale should be applied using #BKE_unit_value_scale
   *   which supports different kinds of users and checks a none unit system.
   */
  float scale_length = 0;
  /** Imperial, metric etc. */
  char system = 0;
  /** Not implemented as a proper unit system yet. */
  char system_rotation = 0;
  short flag = 0;

  char length_unit = 0;
  char mass_unit = 0;
  char time_unit = 0;
  char temperature_unit = 0;

  char _pad[4] = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Global/Common Physics Settings
 * \{ */

/** #PhysicsSettings::flag */
enum {
  PHYS_GLOBAL_GRAVITY = 1,
};

struct PhysicsSettings {
  float gravity[3] = {0.0f, 0.0f, -9.81f};
  int flag = PHYS_GLOBAL_GRAVITY, quick_cache_step = 0;
  char _pad0[4] = {};
};

/**
 * Safe Area options used in Camera View & Sequencer.
 */
struct DisplaySafeAreas {
  /* Each value represents the (x,y) margins as a multiplier.
   * 'center' in this context is just the name for a different kind of safe-area. */

  /** Title Safe. */
  float title[2] = {10.0f / 100.0f, 5.0f / 100.0f};
  /** Image/Graphics Safe. */
  float action[2] = {3.5f / 100.0f, 3.5f / 100.0f};

  /* Use for alternate aspect ratio. */
  float title_center[2] = {17.5f / 100.0f, 5.0f / 100.0f};
  float action_center[2] = {15.0f / 100.0f, 5.0f / 100.0f};
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

/**
 * Scene Display - used for store scene specific display settings for the 3d view.
 */
struct SceneDisplay {
  /** Light direction for shadows/highlight. */
  float light_direction[3] = {M_SQRT1_3, M_SQRT1_3, M_SQRT1_3};
  float shadow_shift = 0.1f, shadow_focus = 0.0f;

  /** Settings for Cavity Shader. */
  float matcap_ssao_distance = 0.2f;
  float matcap_ssao_attenuation = 1.0f;
  int matcap_ssao_samples = 16;

  /** Method of AA for viewport rendering and image rendering. */
  char viewport_aa = SCE_DISPLAY_AA_FXAA;
  char render_aa = SCE_DISPLAY_AA_SAMPLES_8;
  char _pad[6] = {};

  /** OpenGL render engine settings. */
  View3DShading shading;
};

enum RaytraceEEVEE_Flag {
  RAYTRACE_EEVEE_USE_DENOISE = (1 << 0),
};

enum RaytraceEEVEE_DenoiseStages {
  RAYTRACE_EEVEE_DENOISE_SPATIAL = (1 << 0),
  RAYTRACE_EEVEE_DENOISE_TEMPORAL = (1 << 1),
  RAYTRACE_EEVEE_DENOISE_BILATERAL = (1 << 2),
};

enum RaytraceEEVEE_Method {
  /* NOTE: Each method contains the previous one. */
  RAYTRACE_EEVEE_METHOD_PROBE = 0,
  RAYTRACE_EEVEE_METHOD_SCREEN = 1,
  /* TODO(fclem): Hardware ray-tracing. */
  // RAYTRACE_EEVEE_METHOD_HARDWARE = 2,
};

/**
 * Ray-tracing parameters.
 */
struct RaytraceEEVEE {
  /** Higher values will take lower strides and have less blurry intersections. */
  float screen_trace_quality = 0.25f;
  /** Thickness in world space each surface will have during screen space tracing. */
  float screen_trace_thickness = 0.2f;
  /** Maximum roughness before using horizon scan. */
  float trace_max_roughness = 0.5f;
  /** Resolution downscale factor. */
  int resolution_scale = 2;
  /** #RaytraceEEVEE_Flag. */
  int flag = RAYTRACE_EEVEE_USE_DENOISE;
  /** #RaytraceEEVEE_DenoiseStages. */
  int denoise_stages = RAYTRACE_EEVEE_DENOISE_SPATIAL | RAYTRACE_EEVEE_DENOISE_TEMPORAL |
                       RAYTRACE_EEVEE_DENOISE_BILATERAL;
};

/** #SceneEEVEE::flag */
enum {
  // SCE_EEVEE_VOLUMETRIC_ENABLED = (1 << 0), /* Unused */
  // SCE_EEVEE_VOLUMETRIC_LIGHTS = (1 << 1), /* Unused. */
  SCE_EEVEE_VOLUMETRIC_SHADOWS = (1 << 2),
  //  SCE_EEVEE_VOLUMETRIC_COLORED    = (1 << 3), /* Unused */
  SCE_EEVEE_GTAO_ENABLED = (1 << 4),
  // SCE_EEVEE_GTAO_BENT_NORMALS = (1 << 5), /* Unused. */
  // SCE_EEVEE_GTAO_BOUNCE = (1 << 6), /* Unused. */
  // SCE_EEVEE_DOF_ENABLED = (1 << 7), /* Moved to camera->dof.flag */
  // SCE_EEVEE_BLOOM_ENABLED = (1 << 8), /* Unused */
  SCE_EEVEE_MOTION_BLUR_ENABLED_DEPRECATED = (1 << 9), /* Moved to scene->r.mode */
  // SCE_EEVEE_SHADOW_HIGH_BITDEPTH = (1 << 10), /* Unused. */
  SCE_EEVEE_TAA_REPROJECTION = (1 << 11),
  // SCE_EEVEE_SSS_ENABLED = (1 << 12), /* Unused */
  // SCE_EEVEE_SSS_SEPARATE_ALBEDO = (1 << 13), /* Unused */
  SCE_EEVEE_SSR_ENABLED = (1 << 14),
  // SCE_EEVEE_SSR_REFRACTION = (1 << 15), /* Unused. */
  // SCE_EEVEE_SSR_HALF_RESOLUTION = (1 << 16), /* Unused. */
  // SCE_EEVEE_SHOW_IRRADIANCE = (1 << 17), /* Unused. */
  // SCE_EEVEE_SHOW_CUBEMAPS = (1 << 18), /* Unused. */
  SCE_EEVEE_GI_AUTOBAKE = (1 << 19),
  // SCE_EEVEE_SHADOW_SOFT = (1 << 20), /* Unused. */
  SCE_EEVEE_OVERSCAN = (1 << 21),
  // SCE_EEVEE_DOF_HQ_SLIGHT_FOCUS = (1 << 22), /* Unused. */
  SCE_EEVEE_DOF_JITTER = (1 << 23),
  SCE_EEVEE_SHADOW_ENABLED = (1 << 24),
  SCE_EEVEE_RAYTRACE_OPTIONS_SPLIT = (1 << 25),
  SCE_EEVEE_SHADOW_JITTERED_VIEWPORT = (1 << 26),
  SCE_EEVEE_VOLUME_CUSTOM_RANGE = (1 << 27),
  SCE_EEVEE_FAST_GI_ENABLED = (1 << 28),
};

enum FastGI_Method {
  FAST_GI_FULL = 0,
  FAST_GI_AO_ONLY = 1,
};

struct SceneEEVEE {
  DNA_DEFINE_CXX_METHODS(SceneEEVEE)

  int flag = SCE_EEVEE_TAA_REPROJECTION | SCE_EEVEE_SHADOW_ENABLED;
  int gi_diffuse_bounces = 3;
  int gi_cubemap_resolution = 512;
  int gi_visibility_resolution = 32;
  float gi_glossy_clamp = 0;
  int gi_irradiance_pool_size = 16;
  char _pad0[4] = {};

  int taa_samples = 16;
  int taa_render_samples = 64;

  float volumetric_start = 0.1f;
  float volumetric_end = 100.0f;
  int volumetric_tile_size = 8;
  int volumetric_samples = 64;
  float volumetric_sample_distribution = 0.8f;
  float volumetric_light_clamp = 0.0f;
  int volumetric_shadow_samples = 16;
  int volumetric_ray_depth = 16;

  DNA_DEPRECATED float gtao_distance = 0;
  DNA_DEPRECATED float gtao_thickness = 0;

  float fast_gi_bias = 0.05f;
  int fast_gi_resolution = 2;
  int fast_gi_step_count = 8;
  int fast_gi_ray_count = 2;
  float fast_gi_quality = 0.25f;
  float fast_gi_distance = 0.0f;
  float fast_gi_thickness_near = 0.25f;
  float fast_gi_thickness_far = DEG2RAD(45);
  char fast_gi_method = FAST_GI_FULL;
  char _pad1[3] = {};

  float bokeh_overblur = 5.0f;
  float bokeh_max_size = 100.0f;
  float bokeh_threshold = 1.0f;
  float bokeh_neighbor_max = 10.0f;

  DNA_DEPRECATED int motion_blur_samples = 0;
  int motion_blur_max = 32;
  int motion_blur_steps = 1;
  DNA_DEPRECATED int motion_blur_position_deprecated = 0;
  DNA_DEPRECATED float motion_blur_shutter_deprecated = 0;
  float motion_blur_depth_scale = 100.0f;

  /* Only keep for versioning. */
  DNA_DEPRECATED int shadow_cube_size_deprecated = 0;
  int shadow_pool_size = 512;
  int shadow_ray_count = 1;
  int shadow_step_count = 6;
  float shadow_resolution_scale = 1.0f;

  float clamp_surface_direct = 0;
  float clamp_surface_indirect = 10.0f;
  float clamp_volume_direct = 0;
  float clamp_volume_indirect = 0;

  int ray_tracing_method = RAYTRACE_EEVEE_METHOD_SCREEN;

  struct RaytraceEEVEE ray_tracing_options;

  float overscan = 3.0f;
  float light_threshold = 0.01f;
};

struct SceneGpencil {
  float smaa_threshold = 1.0f;
  float smaa_threshold_render = 0.25f;
  int aa_samples = 8;
  int motion_blur_steps = 8;
};

/** #SceneHydra->export_method */
enum {
  SCE_HYDRA_EXPORT_HYDRA = 0,
  SCE_HYDRA_EXPORT_USD = 1,
};

struct SceneHydra {
  int export_method = SCE_HYDRA_EXPORT_HYDRA;
  int _pad0 = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Orientation
 * \{ */

/** Indices when used in #Scene::orientation_slots. */
enum {
  SCE_ORIENT_DEFAULT = 0,
  SCE_ORIENT_TRANSLATE = 1,
  SCE_ORIENT_ROTATE = 2,
  SCE_ORIENT_SCALE = 3,
};

struct TransformOrientationSlot {
  int type = 0;
  int index_custom = 0;
  char flag = 0;
  char _pad0[7] = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene ID-Block
 * \{ */

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

struct Scene {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(Scene)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_SCE;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt = nullptr;

  struct Object *camera = nullptr;
  struct World *world = nullptr;

  struct Scene *set = nullptr;

  ListBaseT<Base> base = {nullptr, nullptr};
  /** Active base. */
  DNA_DEPRECATED struct Base *basact = nullptr;

  /** 3d cursor location. */
  View3DCursor cursor;

  /** Bit-flags for layer visibility (deprecated). */
  DNA_DEPRECATED unsigned int lay = 0;
  /** Active layer (deprecated). */
  DNA_DEPRECATED int layact = 0;
  char _pad2[4] = {};

  /** Various settings. */
  short flag = 0;

  DNA_DEPRECATED char use_nodes = 0;
  char _pad3[1] = {};

  DNA_DEPRECATED struct bNodeTree *nodetree = nullptr;
  struct bNodeTree *compositing_node_group = nullptr;

  /** Sequence editor data is allocated here. */
  struct Editing *ed = nullptr;

  /** Default allocated now. */
  struct ToolSettings *toolsettings = nullptr;
  struct DisplaySafeAreas safe_areas;

  /* Migrate or replace? depends on some internal things... */
  /* No, is on the right place (ton). */
  struct RenderData r;
  struct AudioData audio;

  ListBaseT<TimeMarker> markers = {nullptr, nullptr};
  ListBaseT<TransformOrientation> transform_spaces = {nullptr, nullptr};

  /** First is the [scene, translate, rotate, scale]. */
  TransformOrientationSlot orientation_slots[4];

  /** (runtime) info/cache used for presenting playback frame-rate info to the user. */
  void *fps_info = nullptr;

  /** None of the dependency graph vars is mean to be saved. */
  SceneDepsgraphsMap *depsgraph_hash = nullptr;
  char _pad7[4] = {};

  /* User-Defined KeyingSets. */
  /**
   * Index of the active KeyingSet.
   * first KeyingSet has index 1, 'none' active is 0, 'add new' is -1
   */
  int active_keyingset = 0;
  /** KeyingSets for this scene. */
  ListBaseT<struct KeyingSet> keyingsets = {nullptr, nullptr};

  /* Units. */
  struct UnitSettings unit;

  /** Grease Pencil - Annotations. */
  struct bGPdata *gpd = nullptr;

  /* Movie Tracking. */
  /** Active movie clip. */
  struct MovieClip *clip = nullptr;

  /** Physics simulation settings. */
  struct PhysicsSettings physics_settings;

  /**
   * XXX: runtime flag for drawing, actually belongs in the window,
   * only used by #BKE_object_handle_update()
   */
  struct CustomData_MeshMasks customdata_mask;
  /** XXX: same as `customdata_mask` but for temp operator use (viewport renders). */
  struct CustomData_MeshMasks customdata_mask_modal;

  /* Color Management. */
  ColorManagedViewSettings view_settings;
  ColorManagedDisplaySettings display_settings;
  ColorManagedColorspaceSettings sequencer_colorspace_settings;

  /** RigidBody simulation world+settings. */
  struct RigidBodyWorld *rigidbody_world = nullptr;

  struct PreviewImage *preview = nullptr;

  /** ViewLayer, defined in DNA_layer_types.h */
  ListBaseT<ViewLayer> view_layers = {nullptr, nullptr};
  /** Not an actual data-block, but memory owned by scene. */
  struct Collection *master_collection = nullptr;

  /** Settings to be override by work-spaces. */
  IDProperty *layer_properties = nullptr;

  /**
   * Frame range used for simulations in geometry nodes by default, if SCE_CUSTOM_SIMULATION_RANGE
   * is set. Individual simulations can overwrite this though.
   */
  int simulation_frame_start = 1;
  int simulation_frame_end = 250;

  struct SceneDisplay display;
  struct SceneEEVEE eevee;
  struct SceneGpencil grease_pencil_settings;
  struct SceneHydra hydra;

  bke::SceneRuntime *runtime = nullptr;
#ifdef __cplusplus
  /* Return the frame rate of the scene. */
  double frames_per_second() const;
#endif
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Data Enum/Flags
 * \{ */

/** #RenderData::engine (scene.cc) */
extern const char *RE_engine_id_BLENDER_EEVEE;
extern const char *RE_engine_id_BLENDER_WORKBENCH;
extern const char *RE_engine_id_CYCLES;
/** Only used for versioning. Was used during the transition period between 4.2 and 5.0. */
extern const char *RE_engine_id_BLENDER_EEVEE_NEXT;

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
  (BASE_VISIBLE(v3d, base) && ID_IS_EDITABLE((base)->object) && \
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

/** \} */

}  // namespace blender
