/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup DNA
 *
 * Structs for use by the 'Sequencer' (Video Editor)
 *
 * Note on terminology
 * - #Strip: video/effect/audio data you can select and manipulate in the sequencer.
 * - #StripData: The data referenced by the #Strip
 * - Meta Strip (STRIP_TYPE_META): Support for nesting strips.
 */

#pragma once

#include "DNA_color_types.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_vec_types.h" /* for #rctf */

namespace blender {

struct MovieClip;
struct Scene;
struct VFont;
struct bSound;

namespace seq {
struct FinalImageCache;
struct IntraFrameCache;
struct MediaPresence;
struct PreviewCache;
struct ThumbnailCache;
struct TextVarsRuntime;
struct PrefetchJob;
struct SourceImageCache;
struct StripLookup;
struct StripRuntime;
struct StripModifierDataRuntime;
}  // namespace seq

/** #Strip.flag */
enum eStripFlag {
  SEQ_SELECT = (1 << 0),
  SEQ_LEFTSEL = (1 << 1),
  SEQ_RIGHTSEL = (1 << 2),
  /* (1 << 3) unused, set to zero by versioning code. */
  SEQ_DEINTERLACE = (1 << 4),
  SEQ_MUTE = (1 << 5),
  SEQ_FLAG_TEXT_EDITING_ACTIVE = (1 << 6),
  SEQ_REVERSE_FRAMES = (1 << 7),
  /* (1 << 8) unused, set to zero by versioning code. */
  /* (1 << 9) unused, set to zero by versioning code. */
  /* (1 << 10) unused, set to zero by versioning code. */
  SEQ_FLIPX = (1 << 11),
  SEQ_FLIPY = (1 << 12),
  SEQ_MAKE_FLOAT = (1 << 13),
  SEQ_LOCK = (1 << 14),
  SEQ_USE_PROXY = (1 << 15),
  /* (1 << 16) unused, set to zero by versioning code. */
  SEQ_AUTO_PLAYBACK_RATE = (1 << 17),
  SEQ_SINGLE_FRAME_CONTENT = (1 << 18),
  SEQ_SHOW_RETIMING = (1 << 19),
  /* (1 << 20) unused, set to zero by versioning code. */
  SEQ_MULTIPLY_ALPHA = (1 << 21),

  SEQ_USE_EFFECT_DEFAULT_FADE = (1 << 22),
  SEQ_USE_LINEAR_MODIFIERS = (1 << 23),

  /* Flags for whether those properties are animated or not */
  SEQ_AUDIO_VOLUME_ANIMATED = (1 << 24),
  SEQ_AUDIO_PITCH_ANIMATED = (1 << 25),
  SEQ_AUDIO_PAN_ANIMATED = (1 << 26),
  SEQ_AUDIO_DRAW_WAVEFORM = (1 << 27),

  /* Don't include annotations in OpenGL previews of Scene strips. */
  SEQ_SCENE_NO_ANNOTATION = (1 << 28),
  SEQ_USE_VIEWS = (1 << 29),

  /* Access scene strips directly (like a meta-strip). */
  SEQ_SCENE_STRIPS = (1 << 30),

  SEQ_AUDIO_PITCH_CORRECTION = (1u << 31)
};

/** #StripProxy.storage */
enum eStripProxyStorageFlag {
  SEQ_STORAGE_PROXY_CUSTOM_FILE = (1 << 1), /* Store proxy in custom directory. */
  SEQ_STORAGE_PROXY_CUSTOM_DIR = (1 << 2),  /* Store proxy in custom file. */
};

/* Convenience define for all selection flags. */
#define STRIP_ALLSEL (SEQ_SELECT + SEQ_LEFTSEL + SEQ_RIGHTSEL)

/**
 * \warning has to be same as `IMB_imbuf.hh`: `IMB_PROXY_*` and `IMB_TC_*`.
 */
enum eStripProxyBuildSize {
  SEQ_PROXY_IMAGE_SIZE_25 = 1 << 0,
  SEQ_PROXY_IMAGE_SIZE_50 = 1 << 1,
  SEQ_PROXY_IMAGE_SIZE_75 = 1 << 2,
  SEQ_PROXY_IMAGE_SIZE_100 = 1 << 3,
};

/**
 * \warning has to be same as `IMB_imbuf.hh`: `IMB_TC_*`.
 */
enum eStripProxyTimeCode {
  SEQ_PROXY_TC_NONE = 0,
  SEQ_PROXY_TC_RECORD_RUN = 1 << 0,
  SEQ_PROXY_TC_RECORD_RUN_NO_GAPS = 1 << 1,
};

/** StripProxy.build_flags */
enum eStripProxyBuildFlag {
  SEQ_PROXY_SKIP_EXISTING = 1,
};

/** #Strip.alpha_mode */
enum eStripAlphaMode {
  SEQ_ALPHA_STRAIGHT = 0,
  SEQ_ALPHA_PREMUL = 1,
};

/**
 * #Strip.type
 *
 * Note: update #Strip::is_effect when adding new effect types.
 */
enum StripType {
  STRIP_TYPE_IMAGE = 0,
  STRIP_TYPE_META = 1,
  STRIP_TYPE_SCENE = 2,
  STRIP_TYPE_MOVIE = 3,
  STRIP_TYPE_SOUND = 4,
  STRIP_TYPE_SOUND_HD = 5, /* DEPRECATED */
  STRIP_TYPE_MOVIECLIP = 6,
  STRIP_TYPE_MASK = 7,

  STRIP_TYPE_CROSS = 8,
  STRIP_TYPE_ADD = 9,
  STRIP_TYPE_SUB = 10,
  STRIP_TYPE_ALPHAOVER = 11,
  STRIP_TYPE_ALPHAUNDER = 12,
  STRIP_TYPE_GAMCROSS = 13,
  STRIP_TYPE_MUL = 14,
  /* Removed (behavior was the same as alpha-over), only used when reading old files. */
  STRIP_TYPE_OVERDROP_REMOVED = 15,
  /* STRIP_TYPE_PLUGIN = 24, */ /* Removed. */
  STRIP_TYPE_WIPE = 25,
  STRIP_TYPE_GLOW = 26,
  /* Removed in 5.0, used only for versioning. */
  STRIP_TYPE_TRANSFORM_LEGACY = 27,
  STRIP_TYPE_COLOR = 28,
  STRIP_TYPE_SPEED = 29,
  STRIP_TYPE_MULTICAM = 30,
  STRIP_TYPE_ADJUSTMENT = 31,
  STRIP_TYPE_GAUSSIAN_BLUR = 40,
  STRIP_TYPE_TEXT = 41,
  STRIP_TYPE_COLORMIX = 42,
};

enum eStripMovieClipFlag {
  SEQ_MOVIECLIP_RENDER_UNDISTORTED = 1 << 0,
  SEQ_MOVIECLIP_RENDER_STABILIZED = 1 << 1,
};

enum StripBlendMode {
  STRIP_BLEND_REPLACE = 0,

  STRIP_BLEND_CROSS = 8,
  STRIP_BLEND_ADD = 9,
  STRIP_BLEND_SUB = 10,
  STRIP_BLEND_ALPHAOVER = 11,
  STRIP_BLEND_ALPHAUNDER = 12,
  STRIP_BLEND_GAMCROSS = 13,
  STRIP_BLEND_MUL = 14,
  /* Removed (behavior was the same as alpha-over), only used when reading old files. */
  STRIP_BLEND_OVERDROP_REMOVED = 15,

  STRIP_BLEND_SCREEN = 43,
  STRIP_BLEND_LIGHTEN = 44,
  STRIP_BLEND_DODGE = 45,
  STRIP_BLEND_DARKEN = 46,
  STRIP_BLEND_COLOR_BURN = 47,
  STRIP_BLEND_LINEAR_BURN = 48,
  STRIP_BLEND_OVERLAY = 49,
  STRIP_BLEND_HARD_LIGHT = 50,
  STRIP_BLEND_SOFT_LIGHT = 51,
  STRIP_BLEND_PIN_LIGHT = 52,
  STRIP_BLEND_LIN_LIGHT = 53,
  STRIP_BLEND_VIVID_LIGHT = 54,
  STRIP_BLEND_HUE = 55,
  STRIP_BLEND_SATURATION = 56,
  STRIP_BLEND_VALUE = 57,
  STRIP_BLEND_BLEND_COLOR = 58,
  STRIP_BLEND_DIFFERENCE = 59,
  STRIP_BLEND_EXCLUSION = 60,
};

#define STRIP_HAS_PATH(_strip) \
  (ELEM((_strip)->type, STRIP_TYPE_MOVIE, STRIP_TYPE_IMAGE, STRIP_TYPE_SOUND, STRIP_TYPE_SOUND_HD))

/** #Strip.color_tag. */
enum StripColorTag {
  STRIP_COLOR_NONE = -1,
  STRIP_COLOR_01,
  STRIP_COLOR_02,
  STRIP_COLOR_03,
  STRIP_COLOR_04,
  STRIP_COLOR_05,
  STRIP_COLOR_06,
  STRIP_COLOR_07,
  STRIP_COLOR_08,
  STRIP_COLOR_09,

  STRIP_COLOR_TOT,
};

/* #StripTransform.filter */
enum eStripTransformFilter {
  SEQ_TRANSFORM_FILTER_AUTO = -1,
  SEQ_TRANSFORM_FILTER_NEAREST = 0,
  SEQ_TRANSFORM_FILTER_BILINEAR = 1,
  SEQ_TRANSFORM_FILTER_BOX = 2,
  SEQ_TRANSFORM_FILTER_CUBIC_BSPLINE = 3,
  SEQ_TRANSFORM_FILTER_CUBIC_MITCHELL = 4,
};

enum eSeqChannelFlag {
  SEQ_CHANNEL_LOCK = (1 << 0),
  SEQ_CHANNEL_MUTE = (1 << 1),
};

/* -------------------------------------------------------------------- */
/** \name Strip & Editing Structs
 * \{ */

struct StripElem {
  /** File name concatenated onto #StripData::dirpath. */
  char filename[/*FILE_MAXFILE*/ 256] = "";
  /** Ignore when zeroed. */
  int orig_width = 0, orig_height = 0;
  float orig_fps = 0;
};

struct StripCrop {
  int top = 0;
  int bottom = 0;
  int left = 0;
  int right = 0;
};

struct StripTransform {
  float xofs = 0;
  float yofs = 0;
  float scale_x = 0;
  float scale_y = 0;
  float rotation = 0;
  /** 0-1 range, `seq::image_transform_origin_offset_pixelspace_get` to convert to pixel-space. */
  float origin[2] = {};
  int filter = 0; /* eStripTransformFilter */
};

struct StripColorBalance {
  int method = 0; /* eModColorBalanceMethod */
  float lift[3] = {};
  float gamma[3] = {};
  float gain[3] = {};
  float slope[3] = {};
  float offset[3] = {};
  float power[3] = {};
  int flag = 0; /* eModColorBalanceInverseFlag */
  char _pad[4] = {};
  // float exposure = {};
  // float saturation = {};
};

struct StripProxy {
  /** Custom directory for index and proxy files (defaults to "BL_proxy"). */
  char dirpath[/*FILE_MAXDIR*/ 768] = "";
  /** Custom file. */
  char filename[/*FILE_MAXFILE*/ 256] = "";
  struct MovieReader *anim = nullptr; /* Custom proxy anim file. */

  short tc = 0; /* Time code in use. */

  short quality = 0;          /* Proxy build quality. */
  short build_size_flags = 0; /* eStripProxyBuildSize, which proxy sizes to build. */
  short build_tc_flags = 0;   /* eStripProxyTimeCode, which time codes to build. */
  short build_flags = 0;      /* eStripProxyBuildFlag */
  char storage = 0;           /* eStripProxyStorageFlag */
  char _pad[5] = {};
};

struct StripData {
  struct StripData *next = nullptr, *prev = nullptr;
  /**
   * Only used as an array in IMAGE sequences(!),
   * and as a 1-element array in MOVIE sequences,
   * NULL for all other strip-types.
   */
  StripElem *stripdata = nullptr;
  char dirpath[/*FILE_MAXDIR*/ 768] = "";
  StripProxy *proxy = nullptr;
  StripCrop *crop = nullptr;
  StripTransform *transform = nullptr;
  /* Replaced by #ColorBalanceModifierData::color_balance in 2.64. */
  DNA_DEPRECATED StripColorBalance *color_balance_legacy = nullptr;

  /* Color management */
  ColorManagedColorspaceSettings colorspace_settings;
};

/** #SeqRetimingKey::flag */
enum eSeqRetimingKeyFlag {
  SEQ_SPEED_TRANSITION_IN = (1 << 0),
  SEQ_SPEED_TRANSITION_OUT = (1 << 1),
  SEQ_FREEZE_FRAME_IN = (1 << 2),
  SEQ_FREEZE_FRAME_OUT = (1 << 3),
  SEQ_KEY_SELECTED = (1 << 4),
};

struct SeqRetimingKey {
  double strip_frame_index = 0;
  int flag = 0;              /* eSeqRetimingKeyFlag */
  float retiming_factor = 0; /* Value between 0-1 mapped to original content range. */

  double original_strip_frame_index = 0; /* Used for transition keys only. */
  float original_retiming_factor = 0;    /* Used for transition keys only. */
  char _pad[4] = {};
};

#define STRIP_NAME_MAXSTR 64

/**
 * `Strip` is the basic struct used by any strip.
 * Each strip uses a different `Strip` struct.
 */
struct Strip {
  struct Strip *next = nullptr, *prev = nullptr;
  /** Name, set by default and needs to be unique, for RNA paths. */
  char name[/*STRIP_NAME_MAXSTR*/ 64] = "";

  int flag = 0; /* eStripFlag; flags bit mask. */
  int type = 0; /* StripType; strip type. */
  /** The length of the contents of this strip before handles are applied. */
  int len = 0;
  /**
   * Start frame of contents of strip in absolute frame coordinates.
   * For meta-strips start of first strip startdisp.
   */
  float start = 0;
  /**
   * Frame distance from content start to left handle, and from right handle to content end,
   * meaning these can be negative if hold frames are visible.
   */
  float startofs = 0, endofs = 0;
  /** Replaced by `startofs` and `endofs` in 3.3. */
  DNA_DEPRECATED float startstill_legacy = 0;
  DNA_DEPRECATED float endstill_legacy = 0;
  /** The current channel index of the strip in the timeline. */
  int channel = 0;
  /** Starting and ending points of the effect strip. Undefined for other strip types. */
  int startdisp = 0, enddisp = 0;
  float sat = 0;
  float mul = 0;

  /** Stream-index for movie or sound files with several streams. */
  short streamindex = 0;
  short _pad1 = {};
  /** For multi-camera source selection. */
  int multicam_source = 0;
  int clip_flag = 0; /* eStripMovieClipFlag */

  StripData *data = nullptr;

  /** These ID vars should never be NULL but can be when linked libraries fail to load,
   * so check on access. */
  /* For SCENE strips. */
  struct Scene *scene = nullptr;
  /** Override scene camera. */
  struct Object *scene_camera = nullptr;
  /** For MOVIECLIP strips. */
  struct MovieClip *clip = nullptr;
  /** For MASK strips. */
  struct Mask *mask = nullptr;

  /** Only for transition effect strips. Allows keyframing custom fade progression over time. */
  float effect_fader = 0;
  /** Moved to #SpeedControlVars::speed_fader in 3.0. */
  DNA_DEPRECATED float speed_fader_legacy = 0;

  /** Effect strip inputs (`nullptr` if not an effect strip). */
  struct Strip *input1 = nullptr, *input2 = nullptr;

  /** List of strips for meta-strips. */
  ListBaseT<Strip> seqbase = {nullptr, nullptr};
  /** List of channels for meta-strips. */
  ListBaseT<struct SeqTimelineChannel> channels = {nullptr, nullptr};

  /* List of strip connections (one-way, not bidirectional). */
  ListBaseT<struct StripConnection> connections = {nullptr, nullptr};

  /** The linked "bSound" object. */
  struct bSound *sound = nullptr;
  float volume = 0;

  /** Pitch ranges from -0.1 to 10, replaced in 3.3 with #Strip::speed_factor on sound strips.
   * Pan ranges from -2 to 2. */
  DNA_DEPRECATED float pitch_legacy = 0;
  float pan = 0;
  float strobe = 0;

  float sound_offset = 0;
  char _pad4[4] = {};

  /** Struct pointer for effect settings. */
  void *effectdata = nullptr;

  /** Frame offset from start/end of video file content to be ignored and invisible to the VSE. */
  int anim_startofs = 0, anim_endofs = 0;

  int blend_mode = 0; /* StripBlendMode */
  float blend_opacity = 0;

  int8_t color_tag = 0; /* StripColorTag */

  char alpha_mode = 0; /* eStripAlphaMode */
  char _pad2[2] = {};
  int _pad9 = {};

  /* is sfra needed anymore? - it looks like its only used in one place */
  /** Starting frame according to the timeline of the scene. */
  int sfra = 0;

  /* Multiview */
  char views_format = 0;
  char _pad3[3] = {};
  struct Stereo3dFormat *stereo3d_format = nullptr;

  struct IDProperty *prop = nullptr;
  /** System-defined custom properties storage. */
  struct IDProperty *system_properties = nullptr;

  /* Modifiers */
  ListBaseT<struct StripModifierData> modifiers = {nullptr, nullptr};

  /* Playback rate of original video file in frames per second, for movie strips only. */
  float media_playback_rate = 0;
  float speed_factor = 0;

  struct SeqRetimingKey *retiming_keys = nullptr;
  int retiming_keys_num = 0;
  char _pad6[4] = {};

  seq::StripRuntime *runtime = nullptr;

#ifdef __cplusplus
  bool is_effect() const;

  /**
   * Get timeline frame where strip content starts.
   */
  float content_start() const;
  /**
   * Set frame where strip content starts.
   * This function will also move strip handles.
   */
  void content_start_set(const Scene *scene, int timeline_frame);
  /**
   * Get timeline frame where strip content ends.
   */
  float content_end(const Scene *scene) const;
  /**
   * Get number of frames (in timeline) that can be rendered.
   * This can change depending on scene FPS or strip speed factor.
   */
  int length(const Scene *scene) const;
  /**
   * Get the sound offset (if any) and round it to the nearest integer.
   * This is mostly used in places where subframe data is not allowed (like re-timing key
   * positions). Returns zero if strip is not a sound strip or if there is no offset.
   */
  int rounded_sound_offset(float scene_fps) const;
  /**
   * Get timeline frame where strip boundary starts.
   */
  int left_handle() const;
  /**
   * Get timeline frame where strip boundary ends.
   */
  int right_handle(const Scene *scene) const;
  /**
   * Set frame where strip boundary starts. This function moves only handle, content is not moved.
   */
  void left_handle_set(const Scene *scene, int timeline_frame);
  /**
   * Set frame where strip boundary ends.
   * This function moves only handle, content is not moved.
   */
  void right_handle_set(const Scene *scene, int timeline_frame);
  /**
   * Set the left and right handles of the strip.
   * \note `left_frame` must be less than `right_frame`.
   */
  void handles_set(const Scene *scene, int left_frame, int right_frame);
  /**
   * Test if strip intersects with timeline frame.
   * \note This checks if strip would be rendered at this frame. For rendering it is assumed, that
   * timeline frame has width of 1 frame and therefore ends at timeline_frame + 1
   *
   * \param strip: Strip to be checked
   * \param timeline_frame: absolute frame position
   * \return true if strip intersects with timeline frame.
   */
  bool intersects_frame(const Scene *scene, int timeline_frame) const;
  /**
   * Get difference between scene and movie strip frame-rate.
   * Returns 1.0f for all other strip types.
   */
  float media_playback_rate_factor(float scene_fps) const;
  /**
   * Get FPS rate of source media. Movie, scene and movie-clip strips are supported.
   * Returns 0 for unsupported strip or if media can't be loaded.
   */
  float media_fps(Scene *scene);

#endif
};

struct MetaStack {
  struct MetaStack *next = nullptr, *prev = nullptr;
  /**
   * The meta-strip that contains `parent_strip`. May be null (that means it is the top-most
   * strips).
   */
  Strip *old_strip = nullptr;
  Strip *parent_strip = nullptr;
  /* The startdisp/enddisp when entering the metastrip. */
  int disp_range[2] = {};
};

struct SeqTimelineChannel {
  struct SeqTimelineChannel *next = nullptr, *prev = nullptr;
  char name[64] = "";
  int index = 0;
  int flag = 0; /* eSeqChannelFlag */
};

struct StripConnection {
  struct StripConnection *next = nullptr, *prev = nullptr;
  Strip *strip_ref = nullptr;
};

/** #Editing::overlay_frame_flag */
enum eEditingOverlayFrameFlag {
  SEQ_EDIT_OVERLAY_FRAME_SHOW = 1,
  SEQ_EDIT_OVERLAY_FRAME_ABS = 2,
};

/** #Editing::show_missing_media_flag */
enum eEditingShowMissingMediaFlag {
  SEQ_EDIT_SHOW_MISSING_MEDIA = 1 << 0,
};

#define STRIP_OFSBOTTOM 0.05f
#define STRIP_OFSTOP 0.95f

/** #Editing::proxy_storage */
enum eEditingProxyStorageMode {
  /** Store proxies in project directory. */
  SEQ_EDIT_PROXY_DIR_STORAGE = 1,
};

enum eEditingCacheFlag {
  SEQ_CACHE_STORE_RAW = (1 << 0),
  SEQ_CACHE_UNUSED_1 = (1 << 1), /* Was SEQ_CACHE_STORE_PREPROCESSED */
  SEQ_CACHE_UNUSED_2 = (1 << 2), /* Was SEQ_CACHE_STORE_COMPOSITE */
  SEQ_CACHE_STORE_FINAL_OUT = (1 << 3),

  /* For lookup purposes */
  SEQ_CACHE_ALL_TYPES = SEQ_CACHE_STORE_RAW | SEQ_CACHE_STORE_FINAL_OUT,

  SEQ_CACHE_UNUSED_4 = (1 << 4), /* Was SEQ_CACHE_OVERRIDE */
  SEQ_CACHE_UNUSED_5 = (1 << 5),
  SEQ_CACHE_UNUSED_6 = (1 << 6),
  SEQ_CACHE_UNUSED_7 = (1 << 7),
  SEQ_CACHE_UNUSED_8 = (1 << 8),
  SEQ_CACHE_UNUSED_9 = (1 << 9),

  SEQ_CACHE_PREFETCH_ENABLE = (1 << 10),
  SEQ_CACHE_UNUSED_11 = (1 << 11), /* Was SEQ_CACHE_DISK_CACHE_ENABLE */
};

enum eEditingRuntimeFlag {
  SEQ_SHOW_TRANSFORM_PREVIEW = (1 << 0),
};

struct EditingRuntime {
  seq::StripLookup *strip_lookup = nullptr;
  seq::MediaPresence *media_presence = nullptr;
  seq::ThumbnailCache *thumbnail_cache = nullptr;
  seq::IntraFrameCache *intra_frame_cache = nullptr;
  seq::SourceImageCache *source_image_cache = nullptr;
  seq::FinalImageCache *final_image_cache = nullptr;
  seq::PreviewCache *preview_cache = nullptr;
  /** Used for rendering a different frame using sequencer_draw_get_transform_preview from the box
   * blade tool. */
  int transform_preview_frame = 0;
  /** Determines if transform_preview_frame should be used for transform preview. */
  uint32_t flag = 0; /* eEditingRuntimeFlag */
};

struct Editing {
  /**
   * The current meta-strip being edited and/or viewed, may be null, in which case the top-most
   * strips are used.
   */
  Strip *current_meta_strip = nullptr;

  /** Pointer to the top-most strips. */
  ListBaseT<Strip> seqbase = {nullptr, nullptr};
  ListBaseT<MetaStack> metastack = {nullptr, nullptr};
  ListBaseT<SeqTimelineChannel> channels = {nullptr, nullptr};

  Strip *act_strip = nullptr;
  char proxy_dir[/*FILE_MAX*/ 1024] = "";

  int proxy_storage = 0; /* eEditingProxyStorageMode */

  int overlay_frame_ofs = 0, overlay_frame_abs = 0;
  int overlay_frame_flag = 0; /* eEditingOverlayFrameFlag */
  rctf overlay_frame_rect = {};

  int show_missing_media_flag = 0; /* eEditingShowMissingMediaFlag */
  int cache_flag = 0;              /* eEditingCacheFlag */

  seq::PrefetchJob *prefetch_job = nullptr;

  EditingRuntime runtime;

#ifdef __cplusplus
  /** Access currently displayed strips, from root sequence or a meta-strip. */
  ListBaseT<Strip> *current_strips();
  ListBaseT<Strip> *current_strips() const;

  /** Access currently displayed channels, from root sequence or a meta-strip. */
  ListBaseT<SeqTimelineChannel> *current_channels();
  ListBaseT<SeqTimelineChannel> *current_channels() const;
#endif
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Effect Variable Structs
 * \{ */

enum eEffectWipeType {
  SEQ_WIPE_SINGLE,
  SEQ_WIPE_DOUBLE,
  /* SEQ_WIPE_BOX, */   /* UNUSED */
  /* SEQ_WIPE_CROSS, */ /* UNUSED */
  SEQ_WIPE_IRIS,
  SEQ_WIPE_CLOCK,
};

/** #SpeedControlVars::flags */
enum eEffectSpeedControlFlags {
  SEQ_SPEED_UNUSED_2 = 1 << 0, /* Cleared. */
  SEQ_SPEED_UNUSED_1 = 1 << 1, /* Cleared. */
  SEQ_SPEED_UNUSED_3 = 1 << 2, /* Cleared. */
  SEQ_SPEED_USE_INTERPOLATION = 1 << 3,
};

/** #SpeedControlVars.speed_control_type */
enum eEffectSpeedControlType {
  SEQ_SPEED_STRETCH = 0,
  SEQ_SPEED_MULTIPLY = 1,
  SEQ_SPEED_LENGTH = 2,
  SEQ_SPEED_FRAME_NUMBER = 3,
};

/** #TextVars.flag */
enum eEffectTextFlags {
  SEQ_TEXT_SHADOW = (1 << 0),
  SEQ_TEXT_BOX = (1 << 1),
  SEQ_TEXT_BOLD = (1 << 2),
  SEQ_TEXT_ITALIC = (1 << 3),
  SEQ_TEXT_OUTLINE = (1 << 4),
};

/** #TextVars.anchor_x, #TextVars.align */
enum eEffectTextAlignX {
  SEQ_TEXT_ALIGN_X_LEFT = 0,
  SEQ_TEXT_ALIGN_X_CENTER = 1,
  SEQ_TEXT_ALIGN_X_RIGHT = 2,
};

/** #TextVars.anchor_y, formerly #TextVars.align_y */
enum eEffectTextAlignY {
  SEQ_TEXT_ALIGN_Y_TOP = 0,
  SEQ_TEXT_ALIGN_Y_CENTER = 1,
  SEQ_TEXT_ALIGN_Y_BOTTOM = 2,
};

enum eModColorBalanceMethod {
  SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN = 0,
  SEQ_COLOR_BALANCE_METHOD_SLOPEOFFSETPOWER = 1,
};

enum eModColorBalanceInverseFlag {
  SEQ_COLOR_BALANCE_INVERSE_GAIN = 1 << 0,
  SEQ_COLOR_BALANCE_INVERSE_GAMMA = 1 << 1,
  SEQ_COLOR_BALANCE_INVERSE_LIFT = 1 << 2,
  SEQ_COLOR_BALANCE_INVERSE_SLOPE = 1 << 3,
  SEQ_COLOR_BALANCE_INVERSE_OFFSET = 1 << 4,
  SEQ_COLOR_BALANCE_INVERSE_POWER = 1 << 5,
};

enum eModTonemapType {
  SEQ_TONEMAP_RH_SIMPLE = 0,
  SEQ_TONEMAP_RD_PHOTORECEPTOR = 1,
};

enum ePitchMode {
  PITCH_MODE_SEMITONES = 0,
  PITCH_MODE_RATIO = 1,
};

enum ePitchQuality {
  PITCH_QUALITY_HIGH = 0,
  PITCH_QUALITY_FAST = 1,
  PITCH_QUALITY_CONSISTENT = 2,
};

struct WipeVars {
  float edgeWidth = 0, angle = 0;
  short forward = 0;
  short wipetype = 0; /* eEffectWipeType */
};

struct GlowVars {
  /** Minimum intensity to trigger a glow. */
  float fMini = 0;
  float fClamp = 0;
  /** Amount to multiply glow intensity. */
  float fBoost = 0;
  /** Radius of glow blurring. */
  float dDist = 0;
  int dQuality = 0;
  /** SHOW/HIDE glow buffer. */
  int bNoComp = 0;
};

/* Removed in 5.0. Only used in versioning and blend reading. */
struct TransformVarsLegacy {
  float ScalexIni = {};
  float ScaleyIni = {};
  float xIni = 0;
  float yIni = 0;
  float rotIni = 0;
  int percent = 0;
  int interpolation = 0;
  /** Preserve aspect/ratio when scaling. */
  int uniform_scale = 0;
};

struct SolidColorVars {
  float col[3] = {};
  char _pad[4] = {};
};

struct SpeedControlVars {
  DNA_DEFINE_CXX_METHODS(SpeedControlVars)

  float *frameMap = nullptr;
  /** Replaced by `speed_fader_*` fields in 3.0. */
  DNA_DEPRECATED float globalSpeed_legacy = 0;
  int flags = 0; /* eEffectSpeedControlFlags */

  int speed_control_type = 0; /* eEffectSpeedControlType */

  float speed_fader = 0;
  float speed_fader_length = 0;
  float speed_fader_frame_number = 0;
};

struct GaussianBlurVars {
  float size_x = 0;
  float size_y = 0;
};

struct TextVars {
  DNA_DEFINE_CXX_METHODS(TextVars)

  char *text_ptr = nullptr;
  /**
   * Text length in bytes, not including terminating zero
   * (The `strlen` of text).
   */
  int text_len_bytes = 0;
  char _pad2[4] = {};

  struct VFont *text_font = nullptr;
  int text_blf_id = 0;
  float text_size = 0;
  float color[4] = {}, shadow_color[4] = {}, box_color[4] = {}, outline_color[4] = {};
  float loc[2] = {};
  float wrap_width = 0;
  float box_margin = 0;
  float box_roundness = 0;
  float shadow_angle = 0;
  float shadow_offset = 0;
  float shadow_blur = 0;
  float outline_width = 0;
  char flag = 0;  /* eEffectTextFlags */
  char align = 0; /* eEffectTextAlignX */
  char _pad[2] = {};

  /** Offsets in characters (unicode code-points) for #TextVars::text_ptr. */
  int cursor_offset = 0;
  int selection_start_offset = 0;
  int selection_end_offset = 0;

  /** Replaced by `anchor_y` in 4.4. */
  DNA_DEPRECATED char align_y_legacy = 0; /* eEffectTextAlignY */

  char anchor_x = 0; /* eEffectTextAlignX */
  char anchor_y = 0; /* eEffectTextAlignY */
  char _pad1 = {};
  seq::TextVarsRuntime *runtime = nullptr;

  /* Fixed size text buffer, only exists for forward/backward compatibility.
   * #TextVars::text_ptr and #TextVars::text_len_bytes are used for full text. */
  char text_legacy[512] = "";
};

#define STRIP_FONT_NOT_LOADED -2

struct ColorMixVars {
  int blend_effect = 0; /* StripBlendMode */
  /** Blend factor [0.0f, 1.0f]. */
  float factor = 0;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Strip Modifiers
 * \{ */

/** #StripModifierData.type */
enum eStripModifierType {
  eSeqModifierType_None = 0,
  eSeqModifierType_ColorBalance = 1,
  eSeqModifierType_Curves = 2,
  eSeqModifierType_HueCorrect = 3,
  eSeqModifierType_BrightContrast = 4,
  eSeqModifierType_Mask = 5,
  eSeqModifierType_WhiteBalance = 6,
  eSeqModifierType_Tonemap = 7,
  eSeqModifierType_SoundEqualizer = 8,
  eSeqModifierType_Compositor = 9,
  eSeqModifierType_Pitch = 10,
  eSeqModifierType_Echo = 11,
  /* Keep last. */
  NUM_STRIP_MODIFIER_TYPES,
};

/** #StripModifierData.flag */
enum eStripModifierFlag {
  STRIP_MODIFIER_FLAG_NONE = 0,
  STRIP_MODIFIER_FLAG_MUTE = (1 << 0),
  STRIP_MODIFIER_FLAG_EXPANDED = (1 << 1),
  STRIP_MODIFIER_FLAG_ACTIVE = (1 << 2),
};

enum eModMaskInput {
  STRIP_MASK_INPUT_STRIP = 0,
  STRIP_MASK_INPUT_ID = 1,
};

enum eModMaskTime {
  /* Mask animation will be remapped relative to the strip start frame. */
  STRIP_MASK_TIME_RELATIVE = 0,
  /* Global (scene) frame number will be used to access the mask. */
  STRIP_MASK_TIME_ABSOLUTE = 1,
};

struct StripModifierData {
  struct StripModifierData *next = nullptr, *prev = nullptr;
  int type = 0; /* eStripModifierType */
  int flag = 0; /* eStripModifierFlag */
  char name[/*MAX_NAME*/ 64] = "";

  /* Mask input, either strip or mask ID. */
  int mask_input_type = 0; /* eModMaskInput */
  int mask_time = 0;       /* eModMaskTime */

  struct Strip *mask_strip = nullptr;
  struct Mask *mask_id = nullptr;

  int persistent_uid = 0;
  /**
   * Bits that can be used for open-states of layout panels in the modifier.
   */
  uint16_t layout_panel_open_flag = 0;
  uint16_t ui_expand_flag = 0;

  blender::seq::StripModifierDataRuntime *runtime = nullptr;
};

struct ColorBalanceModifierData {
  StripModifierData modifier;

  StripColorBalance color_balance;
  float color_multiply = 0;
};

struct CurvesModifierData {
  StripModifierData modifier;

  struct CurveMapping curve_mapping;
};

struct HueCorrectModifierData {
  StripModifierData modifier;

  struct CurveMapping curve_mapping;
};

struct BrightContrastModifierData {
  StripModifierData modifier;

  float bright = 0;
  float contrast = 0;
};

struct SequencerMaskModifierData {
  StripModifierData modifier;
};

struct WhiteBalanceModifierData {
  StripModifierData modifier;

  float white_value[3] = {};
  char _pad[4] = {};
};

struct SequencerTonemapModifierData {
  StripModifierData modifier;

  float key = 0, offset = 0, gamma = 0;
  float intensity = 0, contrast = 0, adaptation = 0, correction = 0;
  int type = 0; /* eModTonemapType */
};

struct SequencerCompositorModifierData {
  StripModifierData modifier;
  struct bNodeTree *node_group = nullptr;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sound Modifiers
 * \{ */

struct EQCurveMappingData {
  struct EQCurveMappingData *next = nullptr, *prev = nullptr;
  struct CurveMapping curve_mapping;
};

struct SoundEqualizerModifierData {
  StripModifierData modifier;
  ListBaseT<EQCurveMappingData> graphics = {nullptr, nullptr};
};

struct PitchModifierData {
  StripModifierData modifier;
  int mode = 0; /*ePitchMode*/
  int semitones = 0;
  int cents = 0;
  float ratio = 0;
  char preserve_formant = 0;
  char _pad[3] = {};
  int quality = 0; /*ePitchQuality*/
};

struct EchoModifierData {
  StripModifierData modifier;
  float delay = 0;
  float feedback = 0;
  float mix = 0;
  char _pad[4] = {};
};

/** \} */

}  // namespace blender
