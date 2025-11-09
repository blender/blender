/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_curve_types.h"

#include "ANIM_action.hh"

struct AnimData;
struct CacheFile;
struct FCurve;
struct GreasePencil;
struct GreasePencilLayer;
struct GreasePencilLayerTreeGroup;
struct ListBase;
struct MaskLayer;
struct Object;
struct Scene;
struct Strip;
struct bAction;
struct bActionGroup;
struct bAnimContext;
struct bDopeSheet;
struct bGPDlayer;
struct bGPdata;

namespace blender::animrig {
class Action;
class Slot;
}  // namespace blender::animrig

/* ****************************** Base Structs ****************************** */

struct AnimKeylist;

/** Information about the stretch of time from current to the next column. */
struct ActKeyBlockInfo {
  /** Combination of flags from all curves. */
  short flag;
  /** Mask of flags that differ between curves. */
  short conflict;

  /** Selection flag. */
  char sel;
};

/** Keyframe Column Struct. */
struct ActKeyColumn {
  /* ListBase linkage */
  ActKeyColumn *next, *prev;

  /* sorting-tree linkage */
  /** 'children' of this node, less than and greater than it (respectively) */
  ActKeyColumn *left, *right;
  /** parent of this node in the tree */
  ActKeyColumn *parent;

  /* keyframe info */
  eBezTriple_KeyframeType key_type;
  /** eKeyframeHandleDrawOpts */
  char handle_type;
  /** eKeyframeExtremeDrawOpts */
  char extreme_type;
  short sel;
  float cfra;

  /* key-block info */
  ActKeyBlockInfo block;

  /* number of curves and keys in this column */
  short totcurve, totkey, totblock;
};

/** #ActKeyBlockInfo - Flag. */
enum eActKeyBlock_Hold {
  /** Key block represents a moving hold. */
  ACTKEYBLOCK_FLAG_MOVING_HOLD = (1 << 0),
  /** Key block represents a static hold */
  ACTKEYBLOCK_FLAG_STATIC_HOLD = (1 << 1),
  /** Key block represents any kind of hold. */
  ACTKEYBLOCK_FLAG_ANY_HOLD = (1 << 2),
  /** The block is grease pencil. */
  ACTKEYBLOCK_FLAG_GPENCIL = (1 << 4),
  /** The curve segment uses linear interpolation. */
  ACTKEYBLOCK_FLAG_IPO_LINEAR = (1 << 5),
  /** The curve segment uses constant interpolation. */
  ACTKEYBLOCK_FLAG_IPO_CONSTANT = (1 << 6),
  /** The curve segment uses easing or dynamic interpolation. */
  ACTKEYBLOCK_FLAG_IPO_OTHER = (1 << 7),
};

/* *********************** Keyframe Drawing ****************************** */

/** Options for keyframe shape drawing. */
enum eKeyframeShapeDrawOpts {
  /* only the border */
  KEYFRAME_SHAPE_FRAME = 0,
  /* only the inside filling */
  KEYFRAME_SHAPE_INSIDE,
  /* the whole thing */
  KEYFRAME_SHAPE_BOTH,
};

/** Handle type. */
enum eKeyframeHandleDrawOpts {
  /* Don't draw */
  KEYFRAME_HANDLE_NONE = 0,
  /* Various marks in order of increasing display priority. */
  KEYFRAME_HANDLE_AUTO_CLAMP,
  KEYFRAME_HANDLE_AUTO,
  KEYFRAME_HANDLE_VECTOR,
  KEYFRAME_HANDLE_ALIGNED,
  KEYFRAME_HANDLE_FREE,
};

/** Extreme type. */
enum eKeyframeExtremeDrawOpts {
  KEYFRAME_EXTREME_NONE = 0,
  /** Minimum present. */
  KEYFRAME_EXTREME_MIN = (1 << 0),
  /** Maximum present. */
  KEYFRAME_EXTREME_MAX = (1 << 1),
  /** Grouped keys have different states. */
  KEYFRAME_EXTREME_MIXED = (1 << 2),
  /** Both neighbors are equal to this key. */
  KEYFRAME_EXTREME_FLAT = (1 << 3),
};

/* ******************************* Methods ****************************** */

AnimKeylist *ED_keylist_create();
void ED_keylist_free(AnimKeylist *keylist);
void ED_keylist_prepare_for_direct_access(AnimKeylist *keylist);
const ActKeyColumn *ED_keylist_find_exact(const AnimKeylist *keylist, float cfra);
const ActKeyColumn *ED_keylist_find_next(const AnimKeylist *keylist, float cfra);
const ActKeyColumn *ED_keylist_find_prev(const AnimKeylist *keylist, float cfra);
const ActKeyColumn *ED_keylist_find_closest(const AnimKeylist *keylist, float cfra);
const ActKeyColumn *ED_keylist_find_any_between(const AnimKeylist *keylist,
                                                const blender::Bounds<float> frame_range);
bool ED_keylist_is_empty(const AnimKeylist *keylist);
const ListBase /*ActKeyColumn*/ *ED_keylist_listbase(const AnimKeylist *keylist);
bool ED_keylist_all_keys_frame_range(const AnimKeylist *keylist,
                                     blender::Bounds<float> *r_frame_range);
/**
 * Return the selected key-frame's range. If none are selected, return False and
 * do not affect the frame range.
 */
bool ED_keylist_selected_keys_frame_range(const AnimKeylist *keylist,
                                          blender::Bounds<float> *r_frame_range);
const ActKeyColumn *ED_keylist_array(const AnimKeylist *keylist);
int64_t ED_keylist_array_len(const AnimKeylist *keylist);

/* Key-data Generation --------------- */

/**
 * Add the keyframes of the F-Curve to the keylist.
 *
 * \param adt: the AnimData associated with the FCurve, if any.  Must be
 * non-null if `use_nla_remapping` is true, because it's needed for that
 * remapping.
 * \param range: adds keys in the given range to the keylist plus 1 extra on
 * each side if available.
 * \param use_nla_remapping: whether to allow NLA remapping or not. `true` by
 * default, basically only `false` when this F-Curve is an NLA control curve
 * (like animated influence) or a driver.
 */
void fcurve_to_keylist(AnimData *adt,
                       FCurve *fcu,
                       AnimKeylist *keylist,
                       int saction_flag,
                       blender::float2 range,
                       bool use_nla_remapping);
/* Action Group */
void action_group_to_keylist(AnimData *adt,
                             bActionGroup *agrp,
                             AnimKeylist *keylist,
                             int saction_flag,
                             blender::float2 range);
/* Action */

/**
 * Generate a full list of the keys in `dna_action` that are within the frame
 * range `range`.
 *
 * For layered actions, this is limited to the keys that are for the slot
 * assigned to `adt`.
 *
 * Note: this should only be used in places that need or want the *full* list of
 * keys, without any filtering by e.g. channel selection/visibility, etc. For
 * use cases that need such filtering, use `action_slot_summary_to_keylist()`
 * instead.
 *
 * \see action_slot_summary_to_keylist()
 */
void action_to_keylist(AnimData *adt,
                       bAction *dna_action,
                       AnimKeylist *keylist,
                       int saction_flag,
                       blender::float2 range);

/* Object */
void ob_to_keylist(
    bDopeSheet *ads, Object *ob, AnimKeylist *keylist, int saction_flag, blender::float2 range);
/* Cache File */
void cachefile_to_keylist(bDopeSheet *ads,
                          CacheFile *cache_file,
                          AnimKeylist *keylist,
                          int saction_flag);
/* Scene */
void scene_to_keylist(
    bDopeSheet *ads, Scene *sce, AnimKeylist *keylist, int saction_flag, blender::float2 range);
/* DopeSheet Summary */
void summary_to_keylist(bAnimContext *ac,
                        AnimKeylist *keylist,
                        int saction_flag,
                        blender::float2 range);

/**
 * Generate a summary channel keylist for the specified slot, merging it into
 * `keylist`.
 *
 * This filters the keys to be consistent with the visible channels in the
 * editor indicated by `ac`
 *
 * \param animated_id: the particular animated ID that the slot summary is being
 * generated for. This is needed for filtering channels based on bone selection,
 * etc. NOTE: despite being passed as a pointer, this should never be null. It's
 * currently passed as a pointer to be defensive because I (Nathan) am not 100%
 * confident at the time of writing (PR #134922) that the callers of this
 * actually guarantee a non-null pointer (they should, but bugs). This way we
 * can assert internally to catch if that ever happens.
 *
 * \param action: the action containing the slot to generate the summary for.
 *
 * \param slot_handle: the handle of the slot to generate the summary for.
 *
 * \param keylist: the keylist that the generated summary will be merged into.
 *
 * \param saction_flag: needed for the `SACTION_SHOW_EXTREMES` flag, to
 * determine whether to compute and store the data needed to determine which
 * keys are "extremes" (local maxima/minima).
 *
 * \param range: only keys within this time range will be included in the
 * summary.
 */
void action_slot_summary_to_keylist(bAnimContext *ac,
                                    ID *animated_id,
                                    blender::animrig::Action &action,
                                    blender::animrig::slot_handle_t slot_handle,
                                    AnimKeylist *keylist,
                                    int /* eSAction_Flag */ saction_flag,
                                    blender::float2 range);

/* Grease Pencil datablock summary (Legacy) */
void gpencil_to_keylist(bDopeSheet *ads, bGPdata *gpd, AnimKeylist *keylist, bool active);

/* Grease Pencil Cels. */
void grease_pencil_cels_to_keylist(AnimData *adt,
                                   const GreasePencilLayer *gpl,
                                   AnimKeylist *keylist,
                                   int saction_flag);

/* Grease Pencil Layer Group. */
void grease_pencil_layer_group_to_keylist(AnimData *adt,
                                          const GreasePencilLayerTreeGroup *layer_group,
                                          AnimKeylist *keylist,
                                          const int saction_flag);
/* Grease Pencil Data-Block. */
void grease_pencil_data_block_to_keylist(AnimData *adt,
                                         const GreasePencil *grease_pencil,
                                         AnimKeylist *keylist,
                                         const int saction_flag,
                                         bool active_layer_only);
/* Grease Pencil Layer (Legacy) */
void gpl_to_keylist(bDopeSheet *ads, bGPDlayer *gpl, AnimKeylist *keylist);

/* Mask */
void mask_to_keylist(bDopeSheet *ads, MaskLayer *masklay, AnimKeylist *keylist);

/* Sequencer strip data. */
void sequencer_strip_to_keylist(const Strip &strip, AnimKeylist &keylist, Scene &scene);

/* ActKeyColumn API ---------------- */

/** Checks if #ActKeyColumn has any block data. */
bool actkeyblock_is_valid(const ActKeyColumn *ac);

/** Checks if #ActKeyColumn can be used as a block (i.e. drawn/used to detect "holds"). */
int actkeyblock_get_valid_hold(const ActKeyColumn *ac);
