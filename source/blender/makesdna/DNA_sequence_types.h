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
#include "DNA_session_uid_types.h" /* for #SessionUID */
#include "DNA_vec_types.h"         /* for #rctf */

struct MovieClip;
struct Scene;
struct VFont;
struct bSound;

#ifdef __cplusplus
namespace blender::seq {
struct FinalImageCache;
struct IntraFrameCache;
struct MediaPresence;
struct PreviewCache;
struct ThumbnailCache;
struct TextVarsRuntime;
struct PrefetchJob;
struct SourceImageCache;
struct StripLookup;
}  // namespace blender::seq
using FinalImageCache = blender::seq::FinalImageCache;
using IntraFrameCache = blender::seq::IntraFrameCache;
using MediaPresence = blender::seq::MediaPresence;
using PreviewCache = blender::seq::PreviewCache;
using ThumbnailCache = blender::seq::ThumbnailCache;
using TextVarsRuntime = blender::seq::TextVarsRuntime;
using PrefetchJob = blender::seq::PrefetchJob;
using SourceImageCache = blender::seq::SourceImageCache;
using StripLookup = blender::seq::StripLookup;
#else
typedef struct FinalImageCache FinalImageCache;
typedef struct IntraFrameCache IntraFrameCache;
typedef struct MediaPresence MediaPresence;
typedef struct PreviewCache PreviewCache;
typedef struct ThumbnailCache ThumbnailCache;
typedef struct TextVarsRuntime TextVarsRuntime;
typedef struct PrefetchJob PrefetchJob;
typedef struct SourceImageCache SourceImageCache;
typedef struct StripLookup StripLookup;
#endif

/* -------------------------------------------------------------------- */
/** \name Strip & Editing Structs
 * \{ */

typedef struct StripAnim {
  struct StripAnim *next, *prev;
  struct MovieReader *anim;
} StripAnim;

typedef struct StripElem {
  /** File name concatenated onto #StripData::dirpath. */
  char filename[/*FILE_MAXFILE*/ 256];
  /** Ignore when zeroed. */
  int orig_width, orig_height;
  float orig_fps;
} StripElem;

typedef struct StripCrop {
  int top;
  int bottom;
  int left;
  int right;
} StripCrop;

typedef struct StripTransform {
  float xofs;
  float yofs;
  float scale_x;
  float scale_y;
  float rotation;
  /** 0-1 range, `seq::image_transform_origin_offset_pixelspace_get` to convert to pixelspace. */
  float origin[2];
  int filter; /* eStripTransformFilter */
} StripTransform;

typedef struct StripColorBalance {
  int method; /* eModColorBalanceMethod */
  float lift[3];
  float gamma[3];
  float gain[3];
  float slope[3];
  float offset[3];
  float power[3];
  int flag; /* eModColorBalanceInverseFlag */
  char _pad[4];
  // float exposure;
  // float saturation;
} StripColorBalance;

typedef struct StripProxy {
  /** Custom directory for index and proxy files (defaults to "BL_proxy"). */
  char dirpath[/*FILE_MAXDIR*/ 768];
  /** Custom file. */
  char filename[/*FILE_MAXFILE*/ 256];
  struct MovieReader *anim; /* Custom proxy anim file. */

  short tc; /* Time code in use. */

  short quality;          /* Proxy build quality. */
  short build_size_flags; /* eStripProxyBuildSize, which proxy sizes to build. */
  short build_tc_flags;   /* eStripProxyTimeCode, which time codes to build. */
  short build_flags;      /* eStripProxyBuildFlag */
  char storage;           /* eStripProxyStorageFlag */
  char _pad[5];
} StripProxy;

typedef struct StripData {
  struct StripData *next, *prev;
  int us, done;
  /**
   * Only used as an array in IMAGE sequences(!),
   * and as a 1-element array in MOVIE sequences,
   * NULL for all other strip-types.
   */
  StripElem *stripdata;
  char dirpath[/*FILE_MAXDIR*/ 768];
  StripProxy *proxy;
  StripCrop *crop;
  StripTransform *transform;
  /* Replaced by #ColorBalanceModifierData::color_balance in 2.64. */
  StripColorBalance *color_balance_legacy DNA_DEPRECATED;

  /* Color management */
  ColorManagedColorspaceSettings colorspace_settings;
} StripData;

typedef struct SeqRetimingKey {
  double strip_frame_index;
  int flag;              /* eSeqRetimingKeyFlag */
  float retiming_factor; /* Value between 0-1 mapped to original content range. */

  double original_strip_frame_index; /* Used for transition keys only. */
  float original_retiming_factor;    /* Used for transition keys only. */
  char _pad[4];
} SeqRetimingKey;

typedef struct StripRuntime {
  SessionUID session_uid;
  /** eStripRuntimeFlag */
  uint32_t flag;
  char _pad[4];
} StripRuntime;

/**
 * `Strip` is the basic struct used by any strip.
 * Each strip uses a different `Strip` struct.
 *
 * \warning The first part identical to ID (for use in ipo's)
 * the comment above is historic, probably we can drop the ID compatibility,
 * but take care making this change.
 */
typedef struct Strip {
  struct Strip *next, *prev;
  void *_pad;
  /** Needed (to be like ipo), else it will raise libdata warnings, this should never be used. */
  void *lib;
  /** Name, set by default and needs to be unique, for RNA paths. */
  char name[/*STRIP_NAME_MAXSTR*/ 64];

  int flag; /* eStripFlag; flags bit mask. */
  int type; /* StripType; strip type. */
  /** The length of the contents of this strip before handles are applied. */
  int len;
  /**
   * Start frame of contents of strip in absolute frame coordinates.
   * For meta-strips start of first strip startdisp.
   */
  float start;
  /**
   * Frame distance from content start to left handle, and from right handle to content end,
   * meaning these can be negative if hold frames are visible.
   */
  float startofs, endofs;
  /** Replaced by `startofs` and `endofs` in 3.3. */
  float startstill_legacy DNA_DEPRECATED, endstill_legacy DNA_DEPRECATED;
  /** The current channel index of the strip in the timeline. */
  int channel;
  /** Starting and ending points of the effect strip. Undefined for other strip types. */
  int startdisp, enddisp;
  float sat;
  float mul;

  /** Stream-index for movie or sound files with several streams. */
  short streamindex;
  short _pad1;
  /** For multi-camera source selection. */
  int multicam_source;
  int clip_flag; /* eStripMovieClipFlag */

  StripData *data;

  /** These ID vars should never be NULL but can be when linked libraries fail to load,
   * so check on access. */
  /* For SCENE strips. */
  struct Scene *scene;
  /** Override scene camera. */
  struct Object *scene_camera;
  /** For MOVIECLIP strips. */
  struct MovieClip *clip;
  /** For MASK strips. */
  struct Mask *mask;
  /** For MOVIE strips. */
  ListBase anims; /* StripAnim */

  /** Only for transition effect strips. Allows keyframing custom fade progression over time. */
  float effect_fader;
  /** Moved to #SpeedControlVars::speed_fader in 3.0. */
  float speed_fader_legacy DNA_DEPRECATED;

  /** Effect strip inputs (`nullptr` if not an effect strip). */
  struct Strip *input1, *input2;

  /* This strange padding is needed for compatibility with older versions
   * that assumed `seqbasep` is at fixed offset. */
  void *_pad7;
  int _pad8[2];

  /** List of strips for meta-strips. */
  ListBase seqbase;
  /** List of channels for meta-strips. */
  ListBase channels; /* SeqTimelineChannel */

  /* List of strip connections (one-way, not bidirectional). */
  ListBase connections; /* StripConnection */

  /** The linked "bSound" object. */
  struct bSound *sound;
  /** Handle to #AUD_SequenceEntry. */
  void *scene_sound;
  float volume;

  /** Pitch ranges from -0.1 to 10, replaced in 3.3 with #Strip::speed_factor on sound strips.
   * Pan ranges from -2 to 2. */
  float pitch_legacy DNA_DEPRECATED, pan;
  float strobe;

  float sound_offset;
  char _pad4[4];

  /** Struct pointer for effect settings. */
  void *effectdata;

  /** Frame offset from start/end of video file content to be ignored and invisible to the VSE. */
  int anim_startofs, anim_endofs;

  int blend_mode; /* StripBlendMode */
  float blend_opacity;

  int8_t color_tag; /* StripColorTag */

  char alpha_mode; /* eStripAlphaMode */
  char _pad2[2];
  int _pad9;

  /* is sfra needed anymore? - it looks like its only used in one place */
  /** Starting frame according to the timeline of the scene. */
  int sfra;

  /* Multiview */
  char views_format;
  char _pad3[3];
  struct Stereo3dFormat *stereo3d_format;

  struct IDProperty *prop;
  /** System-defined custom properties storage. */
  struct IDProperty *system_properties;

  /* Modifiers */
  ListBase modifiers; /* StripModifierData */

  /* Playback rate of original video file in frames per second, for movie strips only. */
  float media_playback_rate;
  float speed_factor;

  struct SeqRetimingKey *retiming_keys;
  int retiming_keys_num;
  char _pad6[4];

  void *_pad10;

  StripRuntime runtime;

#ifdef __cplusplus
  bool is_effect() const;
#endif
} Strip;

typedef struct MetaStack {
  struct MetaStack *next, *prev;
  /**
   * The meta-strip that contains `parent_strip`. May be null (that means it is the top-most
   * strips).
   */
  Strip *old_strip;
  Strip *parent_strip;
  /* The startdisp/enddisp when entering the metastrip. */
  int disp_range[2];
} MetaStack;

typedef struct SeqTimelineChannel {
  struct SeqTimelineChannel *next, *prev;
  char name[64];
  int index;
  int flag; /* eSeqChannelFlag */
} SeqTimelineChannel;

typedef struct StripConnection {
  struct StripConnection *next, *prev;
  Strip *strip_ref;
} StripConnection;

typedef struct EditingRuntime {
  StripLookup *strip_lookup;
  MediaPresence *media_presence;
  ThumbnailCache *thumbnail_cache;
  IntraFrameCache *intra_frame_cache;
  SourceImageCache *source_image_cache;
  FinalImageCache *final_image_cache;
  PreviewCache *preview_cache;
} EditingRuntime;

typedef struct Editing {
  /**
   * The current meta-strip being edited and/or viewed, may be null, in which case the top-most
   * strips are used.
   */
  Strip *current_meta_strip;

  /** Pointer to the top-most strips. */
  ListBase seqbase;
  ListBase metastack;
  ListBase channels; /* SeqTimelineChannel */

  Strip *act_strip;
  char proxy_dir[/*FILE_MAX*/ 1024];

  int proxy_storage; /* eEditingProxyStorageMode */

  int overlay_frame_ofs, overlay_frame_abs;
  int overlay_frame_flag; /* eEditingOverlayFrameFlag */
  rctf overlay_frame_rect;

  int show_missing_media_flag; /* eEditingShowMissingMediaFlag */
  int cache_flag;              /* eEditingCacheFlag */

  PrefetchJob *prefetch_job;

  EditingRuntime runtime;

#ifdef __cplusplus
  /** Access currently displayed strips, from root sequence or a meta-strip. */
  ListBase *current_strips();
  ListBase *current_strips() const;

  /** Access currently displayed channels, from root sequence or a meta-strip. */
  ListBase *current_channels();
  ListBase *current_channels() const;
#endif
} Editing;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Effect Variable Structs
 * \{ */

typedef enum eEffectWipeType {
  SEQ_WIPE_SINGLE,
  SEQ_WIPE_DOUBLE,
  /* SEQ_WIPE_BOX, */   /* UNUSED */
  /* SEQ_WIPE_CROSS, */ /* UNUSED */
  SEQ_WIPE_IRIS,
  SEQ_WIPE_CLOCK,
} eEffectWipeType;

typedef struct WipeVars {
  float edgeWidth, angle;
  short forward;
  short wipetype; /* eEffectWipeType */
} WipeVars;

typedef struct GlowVars {
  /** Minimum intensity to trigger a glow. */
  float fMini;
  float fClamp;
  /** Amount to multiply glow intensity. */
  float fBoost;
  /** Radius of glow blurring. */
  float dDist;
  int dQuality;
  /** SHOW/HIDE glow buffer. */
  int bNoComp;
} GlowVars;

/* Removed in 5.0. Only used in versioning and blend reading. */
typedef struct TransformVarsLegacy {
  float ScalexIni;
  float ScaleyIni;
  float xIni;
  float yIni;
  float rotIni;
  int percent;
  int interpolation;
  /** Preserve aspect/ratio when scaling. */
  int uniform_scale;
} TransformVarsLegacy;

typedef struct SolidColorVars {
  float col[3];
  char _pad[4];
} SolidColorVars;

typedef struct SpeedControlVars {
  float *frameMap;
  /** Replaced by `speed_fader_*` fields in 3.0. */
  float globalSpeed_legacy DNA_DEPRECATED;
  int flags; /* eEffectSpeedControlFlags */

  int speed_control_type; /* eEffectSpeedControlType */

  float speed_fader;
  float speed_fader_length;
  float speed_fader_frame_number;
} SpeedControlVars;

/** #SpeedControlVars.speed_control_type */
typedef enum eEffectSpeedControlType {
  SEQ_SPEED_STRETCH = 0,
  SEQ_SPEED_MULTIPLY = 1,
  SEQ_SPEED_LENGTH = 2,
  SEQ_SPEED_FRAME_NUMBER = 3,
} eEffectSpeedControlType;

typedef struct GaussianBlurVars {
  float size_x;
  float size_y;
} GaussianBlurVars;

typedef struct TextVars {
  char *text_ptr;
  /**
   * Text length in bytes, not including terminating zero
   * (The `strlen` of text).
   */
  int text_len_bytes;
  char _pad2[4];

  struct VFont *text_font;
  int text_blf_id;
  float text_size;
  float color[4], shadow_color[4], box_color[4], outline_color[4];
  float loc[2];
  float wrap_width;
  float box_margin;
  float box_roundness;
  float shadow_angle;
  float shadow_offset;
  float shadow_blur;
  float outline_width;
  char flag;  /* eEffectTextFlags */
  char align; /* eEffectTextAlignX */
  char _pad[2];

  /** Offsets in characters (unicode code-points) for #TextVars::text_ptr. */
  int cursor_offset;
  int selection_start_offset;
  int selection_end_offset;

  /** Replaced by `anchor_y` in 4.4. */
  char align_y_legacy DNA_DEPRECATED; /* eEffectTextAlignY */

  char anchor_x; /* eEffectTextAlignX */
  char anchor_y; /* eEffectTextAlignY */
  char _pad1;
  TextVarsRuntime *runtime;

  /* Fixed size text buffer, only exists for forward/backward compatibility.
   * #TextVars::text_ptr and #TextVars::text_len_bytes are used for full text. */
  char text_legacy[512];
} TextVars;

/** #TextVars.flag */
typedef enum eEffectTextFlags {
  SEQ_TEXT_SHADOW = (1 << 0),
  SEQ_TEXT_BOX = (1 << 1),
  SEQ_TEXT_BOLD = (1 << 2),
  SEQ_TEXT_ITALIC = (1 << 3),
  SEQ_TEXT_OUTLINE = (1 << 4),
} eEffectTextFlags;

/** #TextVars.anchor_x, #TextVars.align */
typedef enum eEffectTextAlignX {
  SEQ_TEXT_ALIGN_X_LEFT = 0,
  SEQ_TEXT_ALIGN_X_CENTER = 1,
  SEQ_TEXT_ALIGN_X_RIGHT = 2,
} eEffectTextAlignX;

/** #TextVars.anchor_y, formerly #TextVars.align_y */
typedef enum eEffectTextAlignY {
  SEQ_TEXT_ALIGN_Y_TOP = 0,
  SEQ_TEXT_ALIGN_Y_CENTER = 1,
  SEQ_TEXT_ALIGN_Y_BOTTOM = 2,
} eEffectTextAlignY;

#define STRIP_FONT_NOT_LOADED -2

typedef struct ColorMixVars {
  int blend_effect; /* StripBlendMode */
  /** Blend factor [0.0f, 1.0f]. */
  float factor;
} ColorMixVars;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Strip Modifiers
 * \{ */

typedef struct StripModifierDataRuntime {
  /* Reference parameters for optimizing updates. Sound modifiers can store parameters, sound
   * inputs and outputs. When all existing parameters do match new ones, the update can be skipped
   * and old sound handle may be returned. This is to prevent audio glitches, see #141595 */

  float *last_buf; /* Equalizer frequency/volume curve buffer */

  /* Reference sound handles (may be used by any sound modifier). */
  void *last_sound_in;
  void *last_sound_out;
} StripModifierDataRuntime;

typedef struct StripModifierData {
  struct StripModifierData *next, *prev;
  int type; /* eStripModifierType */
  int flag; /* eStripModifierFlag */
  char name[/*MAX_NAME*/ 64];

  /* Mask input, either strip or mask ID. */
  int mask_input_type; /* eModMaskInput */
  int mask_time;       /* eModMaskTime */

  struct Strip *mask_strip;
  struct Mask *mask_id;

  int persistent_uid;
  /**
   * Bits that can be used for open-states of layout panels in the modifier.
   */
  uint16_t layout_panel_open_flag;
  uint16_t ui_expand_flag;

  StripModifierDataRuntime runtime;
} StripModifierData;

typedef struct ColorBalanceModifierData {
  StripModifierData modifier;

  StripColorBalance color_balance;
  float color_multiply;
} ColorBalanceModifierData;

typedef enum eModColorBalanceMethod {
  SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN = 0,
  SEQ_COLOR_BALANCE_METHOD_SLOPEOFFSETPOWER = 1,
} eModColorBalanceMethod;

typedef struct CurvesModifierData {
  StripModifierData modifier;

  struct CurveMapping curve_mapping;
} CurvesModifierData;

typedef struct HueCorrectModifierData {
  StripModifierData modifier;

  struct CurveMapping curve_mapping;
} HueCorrectModifierData;

typedef struct BrightContrastModifierData {
  StripModifierData modifier;

  float bright;
  float contrast;
} BrightContrastModifierData;

typedef struct SequencerMaskModifierData {
  StripModifierData modifier;
} SequencerMaskModifierData;

typedef struct WhiteBalanceModifierData {
  StripModifierData modifier;

  float white_value[3];
  char _pad[4];
} WhiteBalanceModifierData;

typedef struct SequencerTonemapModifierData {
  StripModifierData modifier;

  float key, offset, gamma;
  float intensity, contrast, adaptation, correction;
  int type; /* eModTonemapType */
} SequencerTonemapModifierData;

typedef enum eModTonemapType {
  SEQ_TONEMAP_RH_SIMPLE = 0,
  SEQ_TONEMAP_RD_PHOTORECEPTOR = 1,
} eModTonemapType;

typedef struct SequencerCompositorModifierData {
  StripModifierData modifier;
  struct bNodeTree *node_group;
} SequencerCompositorModifierData;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sound Modifiers
 * \{ */

typedef struct EQCurveMappingData {
  struct EQCurveMappingData *next, *prev;
  struct CurveMapping curve_mapping;
} EQCurveMappingData;

typedef struct SoundEqualizerModifierData {
  StripModifierData modifier;
  /* EQCurveMappingData */
  ListBase graphics;
} SoundEqualizerModifierData;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flags & Types
 * \{ */

/** #Editing::overlay_frame_flag */
typedef enum eEditingOverlayFrameFlag {
  SEQ_EDIT_OVERLAY_FRAME_SHOW = 1,
  SEQ_EDIT_OVERLAY_FRAME_ABS = 2,
} eEditingOverlayFrameFlag;

/** #Editing::show_missing_media_flag */
typedef enum eEditingShowMissingMediaFlag {
  SEQ_EDIT_SHOW_MISSING_MEDIA = 1 << 0,
} eEditingShowMissingMediaFlag;

#define STRIP_OFSBOTTOM 0.05f
#define STRIP_OFSTOP 0.95f

/** #Editing::proxy_storage */
typedef enum eEditingProxyStorageMode {
  /** Store proxies in project directory. */
  SEQ_EDIT_PROXY_DIR_STORAGE = 1,
} eEditingProxyStorageMode;

/** #SpeedControlVars::flags */
typedef enum eEffectSpeedControlFlags {
  SEQ_SPEED_UNUSED_2 = 1 << 0, /* Cleared. */
  SEQ_SPEED_UNUSED_1 = 1 << 1, /* Cleared. */
  SEQ_SPEED_UNUSED_3 = 1 << 2, /* Cleared. */
  SEQ_SPEED_USE_INTERPOLATION = 1 << 3,
} eEffectSpeedControlFlags;

#define STRIP_NAME_MAXSTR 64

/** #SeqRetimingKey::flag */
typedef enum eSeqRetimingKeyFlag {
  SEQ_SPEED_TRANSITION_IN = (1 << 0),
  SEQ_SPEED_TRANSITION_OUT = (1 << 1),
  SEQ_FREEZE_FRAME_IN = (1 << 2),
  SEQ_FREEZE_FRAME_OUT = (1 << 3),
  SEQ_KEY_SELECTED = (1 << 4),
} eSeqRetimingKeyFlag;

/** #StripRuntime::flag */
typedef enum eStripRuntimeFlag {
  STRIP_CLAMPED_LH = (1 << 0),
  STRIP_CLAMPED_RH = (1 << 1),
  STRIP_OVERLAP = (1 << 2),
  STRIP_EFFECT_NOT_LOADED = (1 << 3), /* Set when reading blend file, cleared after. */
  STRIP_MARK_FOR_DELETE = (1 << 4),
  STRIP_IGNORE_CHANNEL_LOCK = (1 << 5), /* For #SEQUENCER_OT_duplicate_move macro. */
  STRIP_SHOW_OFFSETS = (1 << 6),        /* Set during #SEQUENCER_OT_slip. */
} eStripRuntimeFlag;

/* From: `DNA_object_types.h`, see it's doc-string there. */
#define SELECT 1

/** #Strip.flag */
typedef enum eStripFlag {
  /* `SELECT = (1 << 0)` */
  SEQ_LEFTSEL = (1 << 1),
  SEQ_RIGHTSEL = (1 << 2),
  SEQ_FLAG_UNUSED_3 = (1 << 3), /* Cleared. */
  SEQ_FILTERY = (1 << 4),
  SEQ_MUTE = (1 << 5),
  SEQ_FLAG_TEXT_EDITING_ACTIVE = (1 << 6),
  SEQ_REVERSE_FRAMES = (1 << 7),
  SEQ_IPO_FRAME_LOCKED = (1 << 8),
  SEQ_FLAG_UNUSED_9 = (1 << 9),   /* Cleared. */
  SEQ_FLAG_UNUSED_10 = (1 << 10), /* Potentially dirty, see #84057. */
  SEQ_FLIPX = (1 << 11),
  SEQ_FLIPY = (1 << 12),
  SEQ_MAKE_FLOAT = (1 << 13),
  SEQ_LOCK = (1 << 14),
  SEQ_USE_PROXY = (1 << 15),
  SEQ_FLAG_UNUSED_16 = (1 << 16), /* Cleared. */
  SEQ_AUTO_PLAYBACK_RATE = (1 << 17),
  SEQ_SINGLE_FRAME_CONTENT = (1 << 18),
  SEQ_SHOW_RETIMING = (1 << 19),
  SEQ_FLAG_UNUSED_20 = (1 << 20),
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
} eStripFlag;

/** #StripProxy.storage */
typedef enum eStripProxyStorageFlag {
  SEQ_STORAGE_PROXY_CUSTOM_FILE = (1 << 1), /* Store proxy in custom directory. */
  SEQ_STORAGE_PROXY_CUSTOM_DIR = (1 << 2),  /* Store proxy in custom file. */
} eStripProxyStorageFlag;

/* Convenience define for all selection flags. */
#define STRIP_ALLSEL (SELECT + SEQ_LEFTSEL + SEQ_RIGHTSEL)

typedef enum eModColorBalanceInverseFlag {
  SEQ_COLOR_BALANCE_INVERSE_GAIN = 1 << 0,
  SEQ_COLOR_BALANCE_INVERSE_GAMMA = 1 << 1,
  SEQ_COLOR_BALANCE_INVERSE_LIFT = 1 << 2,
  SEQ_COLOR_BALANCE_INVERSE_SLOPE = 1 << 3,
  SEQ_COLOR_BALANCE_INVERSE_OFFSET = 1 << 4,
  SEQ_COLOR_BALANCE_INVERSE_POWER = 1 << 5,
} eModColorBalanceInverseFlag;

/**
 * \warning has to be same as `IMB_imbuf.hh`: `IMB_PROXY_*` and `IMB_TC_*`.
 */
typedef enum eStripProxyBuildSize {
  SEQ_PROXY_IMAGE_SIZE_25 = 1 << 0,
  SEQ_PROXY_IMAGE_SIZE_50 = 1 << 1,
  SEQ_PROXY_IMAGE_SIZE_75 = 1 << 2,
  SEQ_PROXY_IMAGE_SIZE_100 = 1 << 3,
} eStripProxyBuildSize;

/**
 * \warning has to be same as `IMB_imbuf.hh`: `IMB_TC_*`.
 */
typedef enum eStripProxyTimeCode {
  SEQ_PROXY_TC_NONE = 0,
  SEQ_PROXY_TC_RECORD_RUN = 1 << 0,
  SEQ_PROXY_TC_RECORD_RUN_NO_GAPS = 1 << 1,
} eStripProxyTimeCode;

/** StripProxy.build_flags */
typedef enum eStripProxyBuildFlag {
  SEQ_PROXY_SKIP_EXISTING = 1,
} eStripProxyBuildFlag;

/** #Strip.alpha_mode */
typedef enum eStripAlphaMode {
  SEQ_ALPHA_STRAIGHT = 0,
  SEQ_ALPHA_PREMUL = 1,
} eStripAlphaMode;

/**
 * #Strip.type
 *
 * Note: update #Strip::is_effect when adding new effect types.
 */
typedef enum StripType {
  STRIP_TYPE_IMAGE = 0,
  STRIP_TYPE_META = 1,
  STRIP_TYPE_SCENE = 2,
  STRIP_TYPE_MOVIE = 3,
  STRIP_TYPE_SOUND_RAM = 4,
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
} StripType;

typedef enum eStripMovieClipFlag {
  SEQ_MOVIECLIP_RENDER_UNDISTORTED = 1 << 0,
  SEQ_MOVIECLIP_RENDER_STABILIZED = 1 << 1,
} eStripMovieClipFlag;

typedef enum StripBlendMode {
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
} StripBlendMode;

#define STRIP_HAS_PATH(_strip) \
  (ELEM((_strip)->type, \
        STRIP_TYPE_MOVIE, \
        STRIP_TYPE_IMAGE, \
        STRIP_TYPE_SOUND_RAM, \
        STRIP_TYPE_SOUND_HD))

/* Modifiers */

/** #StripModifierData.type */
typedef enum eStripModifierType {
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
  /* Keep last. */
  NUM_STRIP_MODIFIER_TYPES,
} eStripModifierType;

/** #StripModifierData.flag */
typedef enum eStripModifierFlag {
  STRIP_MODIFIER_FLAG_MUTE = (1 << 0),
  STRIP_MODIFIER_FLAG_EXPANDED = (1 << 1),
  STRIP_MODIFIER_FLAG_ACTIVE = (1 << 2),
} eStripModifierFlag;

typedef enum eModMaskInput {
  STRIP_MASK_INPUT_STRIP = 0,
  STRIP_MASK_INPUT_ID = 1,
} eModMaskInput;

typedef enum eModMaskTime {
  /* Mask animation will be remapped relative to the strip start frame. */
  STRIP_MASK_TIME_RELATIVE = 0,
  /* Global (scene) frame number will be used to access the mask. */
  STRIP_MASK_TIME_ABSOLUTE = 1,
} eModMaskTime;

typedef enum eEditingCacheFlag {
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
} eEditingCacheFlag;

/** #Strip.color_tag. */
typedef enum StripColorTag {
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
} StripColorTag;

/* #StripTransform.filter */
typedef enum eStripTransformFilter {
  SEQ_TRANSFORM_FILTER_AUTO = -1,
  SEQ_TRANSFORM_FILTER_NEAREST = 0,
  SEQ_TRANSFORM_FILTER_BILINEAR = 1,
  SEQ_TRANSFORM_FILTER_BOX = 2,
  SEQ_TRANSFORM_FILTER_CUBIC_BSPLINE = 3,
  SEQ_TRANSFORM_FILTER_CUBIC_MITCHELL = 4,
} eStripTransformFilter;

typedef enum eSeqChannelFlag {
  SEQ_CHANNEL_LOCK = (1 << 0),
  SEQ_CHANNEL_MUTE = (1 << 1),
} eSeqChannelFlag;

/** \} */
