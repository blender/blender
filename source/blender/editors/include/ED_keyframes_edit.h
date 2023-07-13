/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "ED_anim_api.h" /* for enum eAnimFilter_Flags */

#ifdef __cplusplus
extern "C" {
#endif

struct BezTriple;
struct FCurve;
struct Scene;
struct bAnimContext;
struct bAnimListElem;
struct bDopeSheet;

/* ************************************************ */
/* Common Macros and Defines */

/* -------------------------------------------------------------------- */
/** \name Tool Flags
 * \{ */

/* bezt validation */
typedef enum eEditKeyframes_Validate {
  /* Frame range */
  BEZT_OK_FRAME = 1,
  BEZT_OK_FRAMERANGE,
  /* Selection status */
  BEZT_OK_SELECTED,
  /* Values (y-val) only */
  BEZT_OK_VALUE,
  BEZT_OK_VALUERANGE,
  /* For graph editor keyframes (2D tests) */
  BEZT_OK_REGION,
  BEZT_OK_REGION_LASSO,
  BEZT_OK_REGION_CIRCLE,
  /* Only for keyframes a certain Dopesheet channel */
  BEZT_OK_CHANNEL_LASSO,
  BEZT_OK_CHANNEL_CIRCLE,
} eEditKeyframes_Validate;

/** \} */

/* select modes */
typedef enum eEditKeyframes_Select {
  /* SELECT_SUBTRACT for all, followed by SELECT_ADD for some */
  SELECT_REPLACE = (1 << 0),
  /* add ok keyframes to selection */
  SELECT_ADD = (1 << 1),
  /* remove ok keyframes from selection */
  SELECT_SUBTRACT = (1 << 2),
  /* flip ok status of keyframes based on key status */
  SELECT_INVERT = (1 << 3),
  SELECT_EXTEND_RANGE = (1 << 4),
} eEditKeyframes_Select;

/* "selection map" building modes */
typedef enum eEditKeyframes_SelMap {
  SELMAP_MORE = 0,
  SELMAP_LESS,
} eEditKeyframes_SelMap;

/* snapping tools */
typedef enum eEditKeyframes_Snap {
  SNAP_KEYS_CURFRAME = 1,
  SNAP_KEYS_NEARFRAME,
  SNAP_KEYS_NEARSEC,
  SNAP_KEYS_NEARMARKER,
  SNAP_KEYS_HORIZONTAL,
  SNAP_KEYS_VALUE,
  SNAP_KEYS_TIME,
} eEditKeyframes_Snap;

/* equalizing tools */
typedef enum eEditKeyframes_Equalize {
  EQUALIZE_HANDLES_LEFT = (1 << 0),
  EQUALIZE_HANDLES_RIGHT = (1 << 1),
  EQUALIZE_HANDLES_BOTH = (EQUALIZE_HANDLES_LEFT | EQUALIZE_HANDLES_RIGHT),
} eEditKeyframes_Equalize;

/* mirroring tools */
typedef enum eEditKeyframes_Mirror {
  MIRROR_KEYS_CURFRAME = 1,
  MIRROR_KEYS_YAXIS,
  MIRROR_KEYS_XAXIS,
  MIRROR_KEYS_MARKER,
  MIRROR_KEYS_VALUE,
  MIRROR_KEYS_TIME,
} eEditKeyframes_Mirror;

/* use with BEZT_OK_REGION_LASSO */
typedef struct KeyframeEdit_LassoData {
  rctf *rectf_scaled;
  const rctf *rectf_view;
  const int (*mcoords)[2];
  int mcoords_len;
} KeyframeEdit_LassoData;

/* use with BEZT_OK_REGION_CIRCLE */
typedef struct KeyframeEdit_CircleData {
  rctf *rectf_scaled;
  const rctf *rectf_view;
  float mval[2];
  float radius_squared;
} KeyframeEdit_CircleData;

/* ************************************************ */
/* Non-Destructive Editing API (keyframes_edit.cc) */

/* -------------------------------------------------------------------- */
/** \name Defines for 'OK' polls + KeyframeEditData Flags
 * \{ */

/* which verts of a keyframe is active (after polling) */
typedef enum eKeyframeVertOk {
  KEYFRAME_NONE = 0,
  /* 'key' itself is ok */
  KEYFRAME_OK_KEY = (1 << 0),
  /* 'handle 1' is ok */
  KEYFRAME_OK_H1 = (1 << 1),
  /* 'handle 2' is ok */
  KEYFRAME_OK_H2 = (1 << 2),
  /* all flags */
  KEYFRAME_OK_ALL = (KEYFRAME_OK_KEY | KEYFRAME_OK_H1 | KEYFRAME_OK_H2),
} eKeyframeVertOk;

/* Flags for use during iteration */
typedef enum eKeyframeIterFlags {
  /* consider handles in addition to key itself */
  KEYFRAME_ITER_INCL_HANDLES = (1 << 0),

  /* Perform NLA time remapping (global -> strip) for the "f1" parameter
   * (e.g. used for selection tools on summary tracks)
   */
  KED_F1_NLA_UNMAP = (1 << 1),

  /* Perform NLA time remapping (global -> strip) for the "f2" parameter */
  KED_F2_NLA_UNMAP = (1 << 2),

  /* Set this when handles aren't visible by default and you want to perform additional checks to
   * get the actual visibility state. E.g. in some cases handles are only drawn if either a handle
   * or their control point is selected. The selection state will have to be checked in the
   * iterator callbacks then. */
  KEYFRAME_ITER_HANDLES_DEFAULT_INVISIBLE = (1 << 3),
} eKeyframeIterFlags;
ENUM_OPERATORS(eKeyframeIterFlags, KEYFRAME_ITER_HANDLES_DEFAULT_INVISIBLE)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Properties for Keyframe Edit Tools
 * \{ */

typedef struct KeyframeEditData {
  /* generic properties/data access */
  /** temp list for storing custom list of data to check */
  ListBase list;
  /** pointer to current scene - many tools need access to cfra/etc. */
  struct Scene *scene;
  /** pointer to custom data - usually 'Object' but also 'rectf', but could be other types too */
  void *data;
  /** storage of times/values as 'decimals' */
  float f1, f2;
  /** storage of times/values/flags as 'whole' numbers */
  int i1, i2;

  /* current iteration data */
  /** F-Curve that is being iterated over */
  struct FCurve *fcu;
  /** index of current keyframe being iterated over */
  int curIndex;
  /** y-position of midpoint of the channel (for the dopesheet) */
  float channel_y;

  /* flags */
  /** current flags for the keyframe we're reached in the iteration process */
  eKeyframeVertOk curflags;
  /** settings for iteration process */
  eKeyframeIterFlags iterflags;
} KeyframeEditData;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Function Pointer Typedefs
 * \{ */

/* callback function that refreshes the F-Curve after use */
typedef void (*FcuEditFunc)(struct FCurve *fcu);
/* callback function that operates on the given BezTriple */
typedef short (*KeyframeEditFunc)(KeyframeEditData *ked, struct BezTriple *bezt);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom Data Type Defines
 * \{ */

/* Custom data for remapping one range to another in a fixed way */
typedef struct KeyframeEditCD_Remap {
  float oldMin, oldMax; /* old range */
  float newMin, newMax; /* new range */
} KeyframeEditCD_Remap;

/* Paste options */
typedef enum eKeyPasteOffset {
  /* paste keys starting at current frame */
  KEYFRAME_PASTE_OFFSET_CFRA_START,
  /* paste keys ending at current frame */
  KEYFRAME_PASTE_OFFSET_CFRA_END,
  /* paste keys relative to the current frame when copying */
  KEYFRAME_PASTE_OFFSET_CFRA_RELATIVE,
  /* paste keys from original time */
  KEYFRAME_PASTE_OFFSET_NONE,
} eKeyPasteOffset;

typedef enum eKeyPasteValueOffset {
  /* Paste keys with the first key matching the key left of the cursor. */
  KEYFRAME_PASTE_VALUE_OFFSET_LEFT_KEY,
  /* Paste keys with the last key matching the key right of the cursor. */
  KEYFRAME_PASTE_VALUE_OFFSET_RIGHT_KEY,
  /* Paste keys relative to the value of the curve under the cursor. */
  KEYFRAME_PASTE_VALUE_OFFSET_CFRA,
  /* Paste values relative to the cursor position. */
  KEYFRAME_PASTE_VALUE_OFFSET_CURSOR,
  /* Paste keys with the exact copied value. */
  KEYFRAME_PASTE_VALUE_OFFSET_NONE,
} eKeyPasteValueOffset;

typedef enum eKeyMergeMode {
  /* overlay existing with new keys */
  KEYFRAME_PASTE_MERGE_MIX,
  /* replace entire fcurve */
  KEYFRAME_PASTE_MERGE_OVER,
  /* overwrite keys in pasted range */
  KEYFRAME_PASTE_MERGE_OVER_RANGE,
  /* overwrite keys in pasted range (use all keyframe start & end for range) */
  KEYFRAME_PASTE_MERGE_OVER_RANGE_ALL,
} eKeyMergeMode;

/* Possible errors occurring while pasting keys. */
typedef enum eKeyPasteError {
  /* No errors occurred */
  KEYFRAME_PASTE_OK,
  /* Nothing was copied */
  KEYFRAME_PASTE_NOTHING_TO_PASTE,
  /* No F-curves was selected to paste into. */
  KEYFRAME_PASTE_NOWHERE_TO_PASTE
} eKeyPasteError;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Looping API
 *
 * Functions for looping over keyframes.
 * \{ */

/**
 * This function is used to loop over BezTriples in the given F-Curve, applying a given
 * operation on them, and optionally applies an F-Curve validation function afterwards.
 *
 * function for working with F-Curve data only
 * (i.e. when filters have been chosen to explicitly use this).
 */
short ANIM_fcurve_keyframes_loop(KeyframeEditData *ked,
                                 struct FCurve *fcu,
                                 KeyframeEditFunc key_ok,
                                 KeyframeEditFunc key_cb,
                                 FcuEditFunc fcu_cb);
/**
 * Sets selected keyframes' bezier handles to an equal length and optionally makes
 * the keyframes' handles horizontal.
 * \param handle_length: Desired handle length, must be positive.
 * \param flatten: Makes the keyframes' handles the same value as the keyframe,
 * flattening the curve at that point.
 */
void ANIM_fcurve_equalize_keyframes_loop(struct FCurve *fcu,
                                         eEditKeyframes_Equalize mode,
                                         float handle_length,
                                         bool flatten);

/**
 * Function for working with any type (i.e. one of the known types) of animation channel.
 */
short ANIM_animchannel_keyframes_loop(KeyframeEditData *ked,
                                      struct bDopeSheet *ads,
                                      struct bAnimListElem *ale,
                                      KeyframeEditFunc key_ok,
                                      KeyframeEditFunc key_cb,
                                      FcuEditFunc fcu_cb);
/**
 * Same as above, except bAnimListElem wrapper is not needed.
 * \param keytype: is #eAnim_KeyType.
 */
short ANIM_animchanneldata_keyframes_loop(KeyframeEditData *ked,
                                          struct bDopeSheet *ads,
                                          void *data,
                                          int keytype,
                                          KeyframeEditFunc key_ok,
                                          KeyframeEditFunc key_cb,
                                          FcuEditFunc fcu_cb);

/**
 * Calls callback_fn() for each keyframe in each fcurve in the filtered animation context.
 * Assumes the callback updates keys.
 */
void ANIM_animdata_keyframe_callback(struct bAnimContext *ac,
                                     eAnimFilter_Flags filter,
                                     KeyframeEditFunc callback_fn);

/**
 * Functions for making sure all keyframes are in good order.
 */
void ANIM_editkeyframes_refresh(struct bAnimContext *ac);

/** \} */

/* -------------------------------------------------------------------- */
/** \name BezTriple Callback Getters
 * \{ */

/* accessories */
KeyframeEditFunc ANIM_editkeyframes_ok(short mode);

/* edit */
KeyframeEditFunc ANIM_editkeyframes_snap(short mode);
/**
 * \note for markers and 'value', the values to use must be supplied as the first float value.
 */
KeyframeEditFunc ANIM_editkeyframes_mirror(short mode);
KeyframeEditFunc ANIM_editkeyframes_select(short mode);
/**
 * Set all selected Bezier Handles to a single type.
 */
KeyframeEditFunc ANIM_editkeyframes_handles(short mode);
/**
 * Set the interpolation type of the selected BezTriples in each F-Curve to the specified one.
 */
KeyframeEditFunc ANIM_editkeyframes_ipo(short mode);
KeyframeEditFunc ANIM_editkeyframes_keytype(short mode);
KeyframeEditFunc ANIM_editkeyframes_easing(short mode);

/** \} */

/* -------------------------------------------------------------------- */
/** \name BezTriple Callbacks (Selection Map)
 * \{ */

/**
 * Get a callback to populate the selection settings map
 * requires: `ked->custom = char[]` of length `fcurve->totvert`.
 */
KeyframeEditFunc ANIM_editkeyframes_buildselmap(short mode);

/**
 * Change the selection status of the keyframe based on the map entry for this vert
 * requires: `ked->custom = char[]` of length `fcurve->totvert`.
 */
short bezt_selmap_flush(KeyframeEditData *ked, struct BezTriple *bezt);

/** \} */

/* -------------------------------------------------------------------- */
/** \name BezTriple Callback (Assorted Utilities)
 * \{ */

/**
 * Used to calculate the average location of all relevant BezTriples by summing their locations.
 */
short bezt_calc_average(KeyframeEditData *ked, struct BezTriple *bezt);

/**
 * Used to extract a set of cfra-elems from the keyframes.
 */
short bezt_to_cfraelem(KeyframeEditData *ked, struct BezTriple *bezt);

/**
 * Used to remap times from one range to another.
 * requires: `ked->custom = KeyframeEditCD_Remap`.
 */
void bezt_remap_times(KeyframeEditData *ked, struct BezTriple *bezt);

/** \} */

/* -------------------------------------------------------------------- */
/** \name 1.5-D Region Testing Utilities (Lasso/Circle Select)
 * \{ */

/* XXX: These are temporary,
 * until we can unify GP/Mask Keyframe handling and standard FCurve Keyframe handling */

bool keyframe_region_lasso_test(const KeyframeEdit_LassoData *data_lasso, const float xy[2]);

bool keyframe_region_circle_test(const KeyframeEdit_CircleData *data_circle, const float xy[2]);

/* ************************************************ */
/* Destructive Editing API (keyframes_general.c) */

bool duplicate_fcurve_keys(struct FCurve *fcu);
float get_default_rna_value(struct FCurve *fcu, struct PropertyRNA *prop, struct PointerRNA *ptr);

typedef struct FCurveSegment {
  struct FCurveSegment *next, *prev;
  int start_index, length;
} FCurveSegment;

/**
 * Return a list of #FCurveSegment with a start index and a length.
 * A segment is a continuous selection of keyframes.
 * Keys that have BEZT_FLAG_IGNORE_TAG set are treated as unselected.
 * The caller is responsible for freeing the memory.
 */
ListBase find_fcurve_segments(struct FCurve *fcu);
void clean_fcurve(struct bAnimContext *ac,
                  struct bAnimListElem *ale,
                  float thresh,
                  bool cleardefault);
void blend_to_neighbor_fcurve_segment(struct FCurve *fcu,
                                      struct FCurveSegment *segment,
                                      float factor);
void breakdown_fcurve_segment(struct FCurve *fcu, struct FCurveSegment *segment, float factor);

/**
 * Get a 1D gauss kernel. Since the kernel is symmetrical, only calculates the positive side.
 * \param sigma: The shape of the gauss distribution.
 * \param kernel_size: How long the kernel array is.
 */
void ED_ANIM_get_1d_gauss_kernel(const float sigma, int kernel_size, double *r_kernel);

typedef struct ButterworthCoefficients {
  double *A, *d1, *d2;
  int filter_order;
} ButterworthCoefficients;

ButterworthCoefficients *ED_anim_allocate_butterworth_coefficients(const int filter_order);
void ED_anim_free_butterworth_coefficients(struct ButterworthCoefficients *bw_coeff);
void ED_anim_calculate_butterworth_coefficients(float cutoff,
                                                float sampling_frequency,
                                                struct ButterworthCoefficients *bw_coeff);
void butterworth_smooth_fcurve_segment(struct FCurve *fcu,
                                       struct FCurveSegment *segment,
                                       float *samples,
                                       int sample_count,
                                       float factor,
                                       int blend_in_out,
                                       int sample_rate,
                                       struct ButterworthCoefficients *bw_coeff);
void smooth_fcurve_segment(struct FCurve *fcu,
                           struct FCurveSegment *segment,
                           float *samples,
                           float factor,
                           int kernel_size,
                           double *kernel);
void ease_fcurve_segment(struct FCurve *fcu, struct FCurveSegment *segment, float factor);
bool decimate_fcurve(struct bAnimListElem *ale, float remove_ratio, float error_sq_max);

/**
 * Blends the selected keyframes to the default value of the property the F-curve drives.
 */
void blend_to_default_fcurve(struct PointerRNA *id_ptr, struct FCurve *fcu, float factor);
/**
 * Use a weighted moving-means method to reduce intensity of fluctuations.
 */
void smooth_fcurve(struct FCurve *fcu);
void sample_fcurve(struct FCurve *fcu);
/** \param sample_rate indicates how many samples per frame should be generated. */
void sample_fcurve_segment(
    struct FCurve *fcu, float start_frame, int sample_rate, float *r_samples, int sample_count);

/* ----------- */

void ANIM_fcurves_copybuf_free(void);
short copy_animedit_keys(struct bAnimContext *ac, ListBase *anim_data);
eKeyPasteError paste_animedit_keys(struct bAnimContext *ac,
                                   ListBase *anim_data,
                                   eKeyPasteOffset offset_mode,
                                   eKeyPasteValueOffset value_offset_mode,
                                   eKeyMergeMode merge_mode,
                                   bool flip);

/* ************************************************ */

/** \} */

#ifdef __cplusplus
}
#endif
