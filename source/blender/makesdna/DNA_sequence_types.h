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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
/** \file
 * \ingroup DNA
 *
 * Structs for use by the 'Sequencer' (Video Editor)
 *
 * Note on terminology
 * - #Sequence: video/effect/audio data you can select and manipulate in the sequencer.
 * - #Sequence.machine: Strange name for the channel.
 * - #Strip: The data referenced by the #Sequence
 * - Meta Strip (SEQ_TYPE_META): Support for nesting Sequences.
 */

#ifndef __DNA_SEQUENCE_TYPES_H__
#define __DNA_SEQUENCE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_color_types.h"
#include "DNA_listBase.h"
#include "DNA_vec_types.h"
#include "DNA_vfont_types.h"

struct Ipo;
struct MovieClip;
struct Scene;
struct bSound;

/* strlens; 256= FILE_MAXFILE, 768= FILE_MAXDIR */

typedef struct StripAnim {
  struct StripAnim *next, *prev;
  struct anim *anim;
} StripAnim;

typedef struct StripElem {
  char name[256];
  int orig_width, orig_height;
} StripElem;

typedef struct StripCrop {
  int top;
  int bottom;
  int left;
  int right;
} StripCrop;

typedef struct StripTransform {
  int xofs;
  int yofs;
} StripTransform;

typedef struct StripColorBalance {
  float lift[3];
  float gamma[3];
  float gain[3];
  int flag;
  char _pad[4];
  // float exposure;
  // float saturation;
} StripColorBalance;

typedef struct StripProxy {
  char dir[768];  // custom directory for index and proxy files
                  // (defaults to BL_proxy)

  char file[256];     // custom file
  struct anim *anim;  // custom proxy anim file

  short tc;  // time code in use

  short quality;           // proxy build quality
  short build_size_flags;  // size flags (see below) of all proxies
                           // to build
  short build_tc_flags;    // time code flags (see below) of all tc indices
                           // to build
  short build_flags;
  char storage;
  char _pad[5];
} StripProxy;

typedef struct Strip {
  struct Strip *next, *prev;
  int us, done;
  int startstill, endstill;
  /**
   * Only used as an array in IMAGE sequences(!),
   * and as a 1-element array in MOVIE sequences,
   * NULL for all other strip-types.
   */
  StripElem *stripdata;
  char dir[768];
  StripProxy *proxy;
  StripCrop *crop;
  StripTransform *transform;
  StripColorBalance *color_balance DNA_DEPRECATED;

  /* color management */
  ColorManagedColorspaceSettings colorspace_settings;
} Strip;

/**
 * The sequence structure is the basic struct used by any strip.
 * each of the strips uses a different sequence structure.
 *
 * \warning The first part identical to ID (for use in ipo's)
 * the comment above is historic, probably we can drop the ID compatibility,
 * but take care making this change.
 *
 * \warning This is really a 'Strip' in the UI!, name is highly confusing.
 */
typedef struct Sequence {
  struct Sequence *next, *prev;
  /** Tmp var for copying, and tagging for linked selection. */
  void *tmp;
  /** Needed (to be like ipo), else it will raise libdata warnings, this should never be used. */
  void *lib;
  /** SEQ_NAME_MAXSTR - name, set by default and needs to be unique, for RNA paths. */
  char name[64];

  /** Flags bitmap (see below) and the type of sequence. */
  int flag, type;
  /** The length of the contents of this strip - before handles are applied. */
  int len;
  /**
   * Start frame of contents of strip in absolute frame coordinates.
   * For metastrips start of first strip startdisp.
   */
  int start;
  /**
   * Frames after the first frame where display starts,
   * frames before the last frame where display ends.
   */
  int startofs, endofs;
  /**
   * Frames that use the first frame before data begins,
   * frames that use the last frame after data ends.
   */
  int startstill, endstill;
  /** Machine: the strip channel, depth the depth in the sequence when dealing with metastrips. */
  int machine, depth;
  /** Starting and ending points of the strip in the sequence. */
  int startdisp, enddisp;
  float sat;
  float mul, handsize;

  short anim_preseek;
  /** Streamindex for movie or sound files with several streams. */
  short streamindex;
  /** For multicam source selection. */
  int multicam_source;
  /** MOVIECLIP render flags. */
  int clip_flag;

  Strip *strip;

  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;

  /** these ID vars should never be NULL but can be when linked libs fail to load,
   * so check on access */
  struct Scene *scene;
  /** Override scene camera. */
  struct Object *scene_camera;
  /** For MOVIECLIP strips. */
  struct MovieClip *clip;
  /** For MASK strips. */
  struct Mask *mask;
  /** For MOVIE strips. */
  ListBase anims;

  float effect_fader;
  float speed_fader;

  /* pointers for effects: */
  struct Sequence *seq1, *seq2, *seq3;

  /** List of strips for metastrips. */
  ListBase seqbase;

  /** The linked "bSound" object. */
  struct bSound *sound;
  void *scene_sound;
  float volume;

  /** Pitch (-0.1..10), pan -2..2. */
  float pitch, pan;
  float strobe;

  /** Struct pointer for effect settings. */
  void *effectdata;

  /** Only use part of animation file. */
  int anim_startofs;
  /** Is subtle different to startofs / endofs. */
  int anim_endofs;

  int blend_mode;
  float blend_opacity;

  /* is sfra needed anymore? - it looks like its only used in one place */
  /** Starting frame according to the timeline of the scene. */
  int sfra;

  char alpha_mode;
  char _pad[2];

  /* Multiview */
  char views_format;
  struct Stereo3dFormat *stereo3d_format;

  struct IDProperty *prop;

  /* modifiers */
  ListBase modifiers;

  int cache_flag;
  int _pad2[3];

  struct Sequence *orig_sequence;
  void *_pad3;
} Sequence;

typedef struct MetaStack {
  struct MetaStack *next, *prev;
  ListBase *oldbasep;
  Sequence *parseq;
  /* the startdisp/enddisp when entering the meta */
  int disp_range[2];
} MetaStack;

typedef struct Editing {
  /** Pointer to the current list of seq's being edited (can be within a meta strip). */
  ListBase *seqbasep;
  /** Pointer to the top-most seq's. */
  ListBase seqbase;
  ListBase metastack;

  /* Context vars, used to be static */
  Sequence *act_seq;
  /** 1024 = FILE_MAX. */
  char act_imagedir[1024];
  /** 1024 = FILE_MAX. */
  char act_sounddir[1024];
  /** 1024 = FILE_MAX. */
  char proxy_dir[1024];

  int over_ofs, over_cfra;
  int over_flag, proxy_storage;
  rctf over_border;

  struct SeqCache *cache;

  /* Cache control */
  float recycle_max_cost;
  int cache_flag;
} Editing;

/* ************* Effect Variable Structs ********* */
typedef struct WipeVars {
  float edgeWidth, angle;
  short forward, wipetype;
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

typedef struct TransformVars {
  float ScalexIni;
  float ScaleyIni;
  float xIni;
  float yIni;
  float rotIni;
  int percent;
  int interpolation;
  /** Preserve aspect/ratio when scaling. */
  int uniform_scale;
} TransformVars;

typedef struct SolidColorVars {
  float col[3];
  char _pad[4];
} SolidColorVars;

typedef struct SpeedControlVars {
  float *frameMap;
  float globalSpeed;
  int flags;
  int length;
  int lastValidFrame;
} SpeedControlVars;

typedef struct GaussianBlurVars {
  float size_x;
  float size_y;
} GaussianBlurVars;

typedef struct TextVars {
  char text[512];
  VFont *text_font;
  int text_blf_id;
  int text_size;
  float color[4], shadow_color[4];
  float loc[2];
  float wrap_width;
  char flag;
  char align, align_y;
  char _pad[1];
} TextVars;

/* TextVars.flag */
enum {
  SEQ_TEXT_SHADOW = (1 << 0),
};

/* TextVars.align */
enum {
  SEQ_TEXT_ALIGN_X_LEFT = 0,
  SEQ_TEXT_ALIGN_X_CENTER = 1,
  SEQ_TEXT_ALIGN_X_RIGHT = 2,
};

/* TextVars.align_y */
enum {
  SEQ_TEXT_ALIGN_Y_TOP = 0,
  SEQ_TEXT_ALIGN_Y_CENTER = 1,
  SEQ_TEXT_ALIGN_Y_BOTTOM = 2,
};

#define SEQ_FONT_NOT_LOADED -2

typedef struct ColorMixVars {
  /** Value from SEQ_TYPE_XXX enumeration. */
  int blend_effect;
  /** Blend factor [0.0f, 1.0f]. */
  float factor;
} ColorMixVars;

/* ***************** Sequence modifiers ****************** */

typedef struct SequenceModifierData {
  struct SequenceModifierData *next, *prev;
  int type, flag;
  /** MAX_NAME. */
  char name[64];

  /* mask input, either sequence or mask ID */
  int mask_input_type;
  int mask_time;

  struct Sequence *mask_sequence;
  struct Mask *mask_id;
} SequenceModifierData;

typedef struct ColorBalanceModifierData {
  SequenceModifierData modifier;

  StripColorBalance color_balance;
  float color_multiply;
} ColorBalanceModifierData;

typedef struct CurvesModifierData {
  SequenceModifierData modifier;

  struct CurveMapping curve_mapping;
} CurvesModifierData;

typedef struct HueCorrectModifierData {
  SequenceModifierData modifier;

  struct CurveMapping curve_mapping;
} HueCorrectModifierData;

typedef struct BrightContrastModifierData {
  SequenceModifierData modifier;

  float bright;
  float contrast;
} BrightContrastModifierData;

typedef struct SequencerMaskModifierData {
  SequenceModifierData modifier;
} SequencerMaskModifierData;

typedef struct WhiteBalanceModifierData {
  SequenceModifierData modifier;

  float white_value[3];
  char _pad[4];
} WhiteBalanceModifierData;

typedef struct SequencerTonemapModifierData {
  SequenceModifierData modifier;

  float key, offset, gamma;
  float intensity, contrast, adaptation, correction;
  int type;
} SequencerTonemapModifierData;

enum {
  SEQ_TONEMAP_RH_SIMPLE = 0,
  SEQ_TONEMAP_RD_PHOTORECEPTOR = 1,
};

/* ***************** Scopes ****************** */

typedef struct SequencerScopes {
  struct ImBuf *reference_ibuf;

  struct ImBuf *zebra_ibuf;
  struct ImBuf *waveform_ibuf;
  struct ImBuf *sep_waveform_ibuf;
  struct ImBuf *vector_ibuf;
  struct ImBuf *histogram_ibuf;
} SequencerScopes;

#define MAXSEQ 32

#define SELECT 1

/* Editor->over_flag */
#define SEQ_EDIT_OVERLAY_SHOW 1
#define SEQ_EDIT_OVERLAY_ABS 2

#define SEQ_STRIP_OFSBOTTOM 0.05f
#define SEQ_STRIP_OFSTOP 0.95f

/* Editor->proxy_storage */
/* store proxies in project directory */
#define SEQ_EDIT_PROXY_DIR_STORAGE 1

/* SpeedControlVars->flags */
#define SEQ_SPEED_INTEGRATE (1 << 0)
#define SEQ_SPEED_UNUSED_1 (1 << 1) /* cleared */
#define SEQ_SPEED_COMPRESS_IPO_Y (1 << 2)

/* ***************** SEQUENCE ****************** */
#define SEQ_NAME_MAXSTR 64

/* seq->flag */
enum {
  /* SELECT */
  SEQ_LEFTSEL = (1 << 1),
  SEQ_RIGHTSEL = (1 << 2),
  SEQ_OVERLAP = (1 << 3),
  SEQ_FILTERY = (1 << 4),
  SEQ_MUTE = (1 << 5),
  SEQ_FLAG_UNUSED_6 = (1 << 6), /* cleared */
  SEQ_REVERSE_FRAMES = (1 << 7),
  SEQ_IPO_FRAME_LOCKED = (1 << 8),
  SEQ_EFFECT_NOT_LOADED = (1 << 9),
  SEQ_FLAG_DELETE = (1 << 10),
  SEQ_FLIPX = (1 << 11),
  SEQ_FLIPY = (1 << 12),
  SEQ_MAKE_FLOAT = (1 << 13),
  SEQ_LOCK = (1 << 14),
  SEQ_USE_PROXY = (1 << 15),
  SEQ_USE_TRANSFORM = (1 << 16),
  SEQ_USE_CROP = (1 << 17),
  SEQ_FLAG_UNUSED_18 = (1 << 18), /* cleared */
  SEQ_FLAG_UNUSED_19 = (1 << 19), /* cleared */
  SEQ_FLAG_UNUSED_21 = (1 << 21), /* cleared */

  SEQ_USE_EFFECT_DEFAULT_FADE = (1 << 22),
  SEQ_USE_LINEAR_MODIFIERS = (1 << 23),

  /* flags for whether those properties are animated or not */
  SEQ_AUDIO_VOLUME_ANIMATED = (1 << 24),
  SEQ_AUDIO_PITCH_ANIMATED = (1 << 25),
  SEQ_AUDIO_PAN_ANIMATED = (1 << 26),
  SEQ_AUDIO_DRAW_WAVEFORM = (1 << 27),

  /* don't include Grease Pencil in OpenGL previews of Scene strips */
  SEQ_SCENE_NO_GPENCIL = (1 << 28),
  SEQ_USE_VIEWS = (1 << 29),

  /* access scene strips directly (like a metastrip) */
  SEQ_SCENE_STRIPS = (1 << 30),

  SEQ_INVALID_EFFECT = (1u << 31),
};

/* StripProxy->storage */
enum {
  SEQ_STORAGE_PROXY_CUSTOM_FILE = (1 << 1), /* store proxy in custom directory */
  SEQ_STORAGE_PROXY_CUSTOM_DIR = (1 << 2),  /* store proxy in custom file */
};

/* convenience define for all selection flags */
#define SEQ_ALLSEL (SELECT + SEQ_LEFTSEL + SEQ_RIGHTSEL)

/* deprecated, don't use a flag anymore*/
/*#define SEQ_ACTIVE                            1048576*/

#define SEQ_COLOR_BALANCE_INVERSE_GAIN 1
#define SEQ_COLOR_BALANCE_INVERSE_GAMMA 2
#define SEQ_COLOR_BALANCE_INVERSE_LIFT 4

/* !!! has to be same as IMB_imbuf.h IMB_PROXY_... and IMB_TC_... */

#define SEQ_PROXY_IMAGE_SIZE_25 1
#define SEQ_PROXY_IMAGE_SIZE_50 2
#define SEQ_PROXY_IMAGE_SIZE_75 4
#define SEQ_PROXY_IMAGE_SIZE_100 8

#define SEQ_PROXY_TC_NONE 0
#define SEQ_PROXY_TC_RECORD_RUN 1
#define SEQ_PROXY_TC_FREE_RUN 2
#define SEQ_PROXY_TC_INTERP_REC_DATE_FREE_RUN 4
#define SEQ_PROXY_TC_RECORD_RUN_NO_GAPS 8
#define SEQ_PROXY_TC_ALL 15

/* SeqProxy->build_flags */
enum {
  SEQ_PROXY_SKIP_EXISTING = 1,
};

/* seq->alpha_mode */
enum {
  SEQ_ALPHA_STRAIGHT = 0,
  SEQ_ALPHA_PREMUL = 1,
};

/* seq->type WATCH IT: SEQ_TYPE_EFFECT BIT is used to determine if this is an effect strip!!! */
enum {
  SEQ_TYPE_IMAGE = 0,
  SEQ_TYPE_META = 1,
  SEQ_TYPE_SCENE = 2,
  SEQ_TYPE_MOVIE = 3,
  SEQ_TYPE_SOUND_RAM = 4,
  SEQ_TYPE_SOUND_HD = 5,
  SEQ_TYPE_MOVIECLIP = 6,
  SEQ_TYPE_MASK = 7,

  SEQ_TYPE_EFFECT = 8,
  SEQ_TYPE_CROSS = 8,
  SEQ_TYPE_ADD = 9,
  SEQ_TYPE_SUB = 10,
  SEQ_TYPE_ALPHAOVER = 11,
  SEQ_TYPE_ALPHAUNDER = 12,
  SEQ_TYPE_GAMCROSS = 13,
  SEQ_TYPE_MUL = 14,
  SEQ_TYPE_OVERDROP = 15,
  /* SEQ_TYPE_PLUGIN      = 24, */ /* Deprecated */
  SEQ_TYPE_WIPE = 25,
  SEQ_TYPE_GLOW = 26,
  SEQ_TYPE_TRANSFORM = 27,
  SEQ_TYPE_COLOR = 28,
  SEQ_TYPE_SPEED = 29,
  SEQ_TYPE_MULTICAM = 30,
  SEQ_TYPE_ADJUSTMENT = 31,
  SEQ_TYPE_GAUSSIAN_BLUR = 40,
  SEQ_TYPE_TEXT = 41,
  SEQ_TYPE_COLORMIX = 42,

  /* Blend modes */
  SEQ_TYPE_SCREEN = 43,
  SEQ_TYPE_LIGHTEN = 44,
  SEQ_TYPE_DODGE = 45,
  SEQ_TYPE_DARKEN = 46,
  SEQ_TYPE_BURN = 47,
  SEQ_TYPE_LINEAR_BURN = 48,
  SEQ_TYPE_OVERLAY = 49,
  SEQ_TYPE_HARD_LIGHT = 50,
  SEQ_TYPE_SOFT_LIGHT = 51,
  SEQ_TYPE_PIN_LIGHT = 52,
  SEQ_TYPE_LIN_LIGHT = 53,
  SEQ_TYPE_VIVID_LIGHT = 54,
  SEQ_TYPE_HUE = 55,
  SEQ_TYPE_SATURATION = 56,
  SEQ_TYPE_VALUE = 57,
  SEQ_TYPE_BLEND_COLOR = 58,
  SEQ_TYPE_DIFFERENCE = 59,
  SEQ_TYPE_EXCLUSION = 60,

  SEQ_TYPE_MAX = 60,
};

#define SEQ_MOVIECLIP_RENDER_UNDISTORTED (1 << 0)
#define SEQ_MOVIECLIP_RENDER_STABILIZED (1 << 1)

#define SEQ_BLEND_REPLACE 0
/* all other BLEND_MODEs are simple SEQ_TYPE_EFFECT ids and therefore identical
 * to the table above. (Only those effects that handle _exactly_ two inputs,
 * otherwise, you can't really blend, right :) !)
 */

#define SEQ_HAS_PATH(_seq) \
  (ELEM((_seq)->type, SEQ_TYPE_MOVIE, SEQ_TYPE_IMAGE, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD))

/* modifiers */

/* SequenceModifierData->type */
enum {
  seqModifierType_ColorBalance = 1,
  seqModifierType_Curves = 2,
  seqModifierType_HueCorrect = 3,
  seqModifierType_BrightContrast = 4,
  seqModifierType_Mask = 5,
  seqModifierType_WhiteBalance = 6,
  seqModifierType_Tonemap = 7,

  NUM_SEQUENCE_MODIFIER_TYPES,
};

/* SequenceModifierData->flag */
enum {
  SEQUENCE_MODIFIER_MUTE = (1 << 0),
  SEQUENCE_MODIFIER_EXPANDED = (1 << 1),
};

enum {
  SEQUENCE_MASK_INPUT_STRIP = 0,
  SEQUENCE_MASK_INPUT_ID = 1,
};

enum {
  /* Mask animation will be remapped relative to the strip start frame. */
  SEQUENCE_MASK_TIME_RELATIVE = 0,
  /* Global (scene) frame number will be used to access the mask. */
  SEQUENCE_MASK_TIME_ABSOLUTE = 1,
};

/* Sequence->cache_flag
 * SEQ_CACHE_STORE_RAW
 * SEQ_CACHE_STORE_PREPROCESSED
 * SEQ_CACHE_STORE_COMPOSITE
 * FINAL_OUT is ignored
 *
 * Editing->cache_flag
 * all entries
 */
enum {
  SEQ_CACHE_STORE_RAW = (1 << 0),
  SEQ_CACHE_STORE_PREPROCESSED = (1 << 1),
  SEQ_CACHE_STORE_COMPOSITE = (1 << 2),
  SEQ_CACHE_STORE_FINAL_OUT = (1 << 3),

  /* For lookup purposes */
  SEQ_CACHE_ALL_TYPES = SEQ_CACHE_STORE_RAW | SEQ_CACHE_STORE_PREPROCESSED |
                        SEQ_CACHE_STORE_COMPOSITE | SEQ_CACHE_STORE_FINAL_OUT,

  SEQ_CACHE_OVERRIDE = (1 << 4),

  /* enable cache visualization overlay in timeline UI */
  SEQ_CACHE_VIEW_ENABLE = (1 << 5),
  SEQ_CACHE_VIEW_RAW = (1 << 6),
  SEQ_CACHE_VIEW_PREPROCESSED = (1 << 7),
  SEQ_CACHE_VIEW_COMPOSITE = (1 << 8),
  SEQ_CACHE_VIEW_FINAL_OUT = (1 << 9),
};

#endif /* __DNA_SEQUENCE_TYPES_H__ */
