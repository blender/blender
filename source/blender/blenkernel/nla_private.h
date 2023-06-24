/* SPDX-FileCopyrightText: 2009 Blender Foundation, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_bitmap.h"
#include "BLI_ghash.h"
#include "RNA_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AnimationEvalContext;

/* --------------- NLA Evaluation DataTypes ----------------------- */

/* used for list of strips to accumulate at current time */
typedef struct NlaEvalStrip {
  struct NlaEvalStrip *next, *prev;

  NlaTrack *track; /* Track that this strip belongs to. */
  NlaStrip *strip; /* Strip that's being used. */

  short track_index; /* The index of the track within the list. */
  short strip_mode;  /* Which end of the strip are we looking at. */

  float strip_time; /* Time at which this strip is being evaluated. */
} NlaEvalStrip;

/* NlaEvalStrip->strip_mode */
enum eNlaEvalStrip_StripMode {
  /* standard evaluation */
  NES_TIME_BEFORE = -1,
  NES_TIME_WITHIN,
  NES_TIME_AFTER,

  /* transition-strip evaluations */
  NES_TIME_TRANSITION_START,
  NES_TIME_TRANSITION_END,
};

struct NlaEvalChannel;
struct NlaEvalData;

/* Unique channel key for GHash. */
typedef struct NlaEvalChannelKey {
  struct PointerRNA ptr;
  struct PropertyRNA *prop;
} NlaEvalChannelKey;

/* Bitmask of array indices touched by actions. */
typedef struct NlaValidMask {
  BLI_bitmap *ptr;
  BLI_bitmap buffer[sizeof(uint64_t) / sizeof(BLI_bitmap)];
} NlaValidMask;

/* Set of property values for blending. */
typedef struct NlaEvalChannelSnapshot {
  struct NlaEvalChannel *channel;

  /** For an upper snapshot channel, marks values that should be blended. */
  NlaValidMask blend_domain;

  /** Only used for keyframe remapping. Any values not in the \a remap_domain will not be used
   * for keyframe remapping. */
  NlaValidMask remap_domain;

  int length;   /* Number of values in the property. */
  bool is_base; /* Base snapshot of the channel. */

  float values[]; /* Item values. */
  /* Memory over-allocated to provide space for values. */
} NlaEvalChannelSnapshot;

/* NlaEvalChannel->mix_mode */
enum eNlaEvalChannel_MixMode {
  NEC_MIX_ADD,
  NEC_MIX_MULTIPLY,
  NEC_MIX_QUATERNION,
  NEC_MIX_AXIS_ANGLE,
};

/* Temp channel for accumulating data from NLA for a single property.
 * Handles array properties as a unit to allow intelligent blending. */
typedef struct NlaEvalChannel {
  struct NlaEvalChannel *next, *prev;
  struct NlaEvalData *owner;

  /* Original RNA path string and property key. */
  const char *rna_path;
  NlaEvalChannelKey key;

  int index;
  bool is_array;
  char mix_mode;

  /* Associated with the RNA property's value(s), marks which elements are affected by NLA. */
  NlaValidMask domain;

  /* Base set of values. */
  NlaEvalChannelSnapshot base_snapshot;
  /* Memory over-allocated to provide space for base_snapshot.values. */
} NlaEvalChannel;

/* Set of values for all channels. */
typedef struct NlaEvalSnapshot {
  /* Snapshot this one defaults to. */
  struct NlaEvalSnapshot *base;

  int size;
  NlaEvalChannelSnapshot **channels;
} NlaEvalSnapshot;

/* Set of all channels covered by NLA. */
typedef struct NlaEvalData {
  ListBase channels;

  /* Mapping of paths and NlaEvalChannelKeys to channels. */
  GHash *path_hash;
  GHash *key_hash;

  /* Base snapshot. */
  int num_channels;
  NlaEvalSnapshot base_snapshot;

  /* Evaluation result snapshot. */
  NlaEvalSnapshot eval_snapshot;
} NlaEvalData;

/* Information about the currently edited strip and ones below it for keyframing. */
typedef struct NlaKeyframingContext {
  struct NlaKeyframingContext *next, *prev;

  /* AnimData for which this context was built. */
  struct AnimData *adt;

  /* Data of the currently edited strip (copy, or fake strip for the main action). */
  NlaStrip strip;
  NlaEvalStrip *eval_strip;
  /* Storage for the action track as a strip. */
  NlaStrip action_track_strip;

  /* Strips above tweaked strip. */
  ListBase upper_estrips;
  /* Evaluated NLA stack below the tweak strip. */
  NlaEvalData lower_eval_data;
} NlaKeyframingContext;

/* --------------- NLA Functions (not to be used as a proper API) ----------------------- */

/**
 * Convert non clipped mapping for strip-time <-> global time:
 * `mode = eNlaTime_ConvertModes[] -> NLATIME_CONVERT_*`
 *
 * Only secure for 'internal' (i.e. within AnimSys evaluation) operations,
 * but should not be directly relied on for stuff which interacts with editors.
 */
float nlastrip_get_frame(NlaStrip *strip, float cframe, short mode);

/* --------------- NLA Evaluation (very-private stuff) ----------------------- */
/* these functions are only defined here to avoid problems with the order
 * in which they get defined. */

/**
 * Gets the strip active at the current time for a list of strips for evaluation purposes.
 */
NlaEvalStrip *nlastrips_ctime_get_strip(ListBase *list,
                                        ListBase *strips,
                                        short index,
                                        const struct AnimationEvalContext *anim_eval_context,
                                        bool flush_to_original);
/**
 * Evaluates the given evaluation strip.
 */

enum eNlaStripEvaluate_Mode {
  /* Blend upper strip with lower stack. */
  STRIP_EVAL_BLEND,
  /* Given upper strip and blended snapshot, solve for lower stack. */
  STRIP_EVAL_BLEND_GET_INVERTED_LOWER_SNAPSHOT,
  /* Store strip fcurve values in snapshot, properly marking blend_domain values.
   *
   * Currently only used for transitions to distinguish fcurve sampled values from default or lower
   * stack values.
   */
  STRIP_EVAL_NOBLEND,
};

void nlastrip_evaluate(const int evaluation_mode,
                       PointerRNA *ptr,
                       NlaEvalData *channels,
                       ListBase *modifiers,
                       NlaEvalStrip *nes,
                       NlaEvalSnapshot *snapshot,
                       const struct AnimationEvalContext *anim_eval_context,
                       bool flush_to_original);
/**
 * write the accumulated settings to.
 */
void nladata_flush_channels(PointerRNA *ptr,
                            NlaEvalData *channels,
                            NlaEvalSnapshot *snapshot,
                            bool flush_to_original);

void nlasnapshot_enable_all_blend_domain(NlaEvalSnapshot *snapshot);

void nlasnapshot_ensure_channels(NlaEvalData *eval_data, NlaEvalSnapshot *snapshot);

/**
 * Blends the \a lower_snapshot with the \a upper_snapshot into \a r_blended_snapshot according
 * to the given \a upper_blendmode and \a upper_influence.
 *
 * For \a upper_snapshot, blending limited to values in the \a blend_domain.
 * For Replace blend-mode, this allows the upper snapshot to have a location XYZ channel
 * where only a subset of values are blended.
 */
void nlasnapshot_blend(NlaEvalData *eval_data,
                       NlaEvalSnapshot *lower_snapshot,
                       NlaEvalSnapshot *upper_snapshot,
                       short upper_blendmode,
                       float upper_influence,
                       NlaEvalSnapshot *r_blended_snapshot);

/**
 * Using \a blended_snapshot and \a lower_snapshot, we can solve for the \a r_upper_snapshot.
 *
 * Only channels that exist within \a blended_snapshot are inverted.
 *
 * For \a r_upper_snapshot, disables \a NlaEvalChannelSnapshot->remap_domain for failed inversions.
 * Only values within the \a remap_domain are processed.
 */
void nlasnapshot_blend_get_inverted_upper_snapshot(NlaEvalData *eval_data,
                                                   NlaEvalSnapshot *lower_snapshot,
                                                   NlaEvalSnapshot *blended_snapshot,
                                                   short upper_blendmode,
                                                   float upper_influence,
                                                   NlaEvalSnapshot *r_upper_snapshot);

/**
 * Using \a blended_snapshot and \a upper_snapshot, we can solve for the \a r_lower_snapshot.
 *
 * Only channels that exist within \a blended_snapshot are processed.
 * Only blended values within the \a remap_domain are processed.
 *
 * Writes to \a r_upper_snapshot `NlaEvalChannelSnapshot->remap_domain` to match remapping success.
 *
 * Assumes caller marked upper values that are in the \a blend_domain. This determines whether the
 * blended value came directly from the lower snapshot or a result of blending.
 */
void nlasnapshot_blend_get_inverted_lower_snapshot(NlaEvalData *eval_data,
                                                   NlaEvalSnapshot *blended_snapshot,
                                                   NlaEvalSnapshot *upper_snapshot,
                                                   const short upper_blendmode,
                                                   const float upper_influence,
                                                   NlaEvalSnapshot *r_lower_snapshot);

void nlasnapshot_blend_strip(PointerRNA *ptr,
                             NlaEvalData *channels,
                             ListBase *modifiers,
                             NlaEvalStrip *nes,
                             NlaEvalSnapshot *snapshot,
                             const struct AnimationEvalContext *anim_eval_context,
                             const bool flush_to_original);

void nlasnapshot_blend_strip_get_inverted_lower_snapshot(
    PointerRNA *ptr,
    NlaEvalData *channels,
    ListBase *modifiers,
    NlaEvalStrip *nes,
    NlaEvalSnapshot *snapshot,
    const struct AnimationEvalContext *anim_eval_context);

void nlasnapshot_blend_strip_no_blend(PointerRNA *ptr,
                                      NlaEvalData *channels,
                                      ListBase *modifiers,
                                      NlaEvalStrip *nes,
                                      NlaEvalSnapshot *snapshot,
                                      const struct AnimationEvalContext *anim_eval_context);

#ifdef __cplusplus
}
#endif
