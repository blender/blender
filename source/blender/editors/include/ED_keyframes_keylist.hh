/* SPDX-FileCopyrightText: 2009 Blender Foundation, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_range.h"

struct AnimData;
struct CacheFile;
struct FCurve;
struct MaskLayer;
struct Object;
struct Scene;
struct bAnimContext;
struct bDopeSheet;
struct bGPDlayer;
struct GreasePencilLayer;

/* ****************************** Base Structs ****************************** */

struct AnimKeylist;

/* Information about the stretch of time from current to the next column */
typedef struct ActKeyBlockInfo {
  /* Combination of flags from all curves. */
  short flag;
  /* Mask of flags that differ between curves. */
  short conflict;

  /* Selection flag. */
  char sel;
} ActKeyBlockInfo;

/* Keyframe Column Struct */
typedef struct ActKeyColumn {
  /* ListBase linkage */
  struct ActKeyColumn *next, *prev;

  /* sorting-tree linkage */
  /** 'children' of this node, less than and greater than it (respectively) */
  struct ActKeyColumn *left, *right;
  /** parent of this node in the tree */
  struct ActKeyColumn *parent;
  /** DLRB_BLACK or DLRB_RED */
  char tree_col;

  /* keyframe info */
  /** eBezTripe_KeyframeType */
  char key_type;
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
} ActKeyColumn;

/* ActKeyBlockInfo - Flag */
typedef enum eActKeyBlock_Hold {
  /* Key block represents a moving hold */
  ACTKEYBLOCK_FLAG_MOVING_HOLD = (1 << 0),
  /* Key block represents a static hold */
  ACTKEYBLOCK_FLAG_STATIC_HOLD = (1 << 1),
  /* Key block represents any kind of hold */
  ACTKEYBLOCK_FLAG_ANY_HOLD = (1 << 2),
  /* The curve segment uses non-bezier interpolation */
  ACTKEYBLOCK_FLAG_NON_BEZIER = (1 << 3),
  /* The block is grease pencil */
  ACTKEYBLOCK_FLAG_GPENCIL = (1 << 4),
} eActKeyBlock_Flag;

/* *********************** Keyframe Drawing ****************************** */

/* options for keyframe shape drawing */
typedef enum eKeyframeShapeDrawOpts {
  /* only the border */
  KEYFRAME_SHAPE_FRAME = 0,
  /* only the inside filling */
  KEYFRAME_SHAPE_INSIDE,
  /* the whole thing */
  KEYFRAME_SHAPE_BOTH,
} eKeyframeShapeDrawOpts;

/* Handle type. */
typedef enum eKeyframeHandleDrawOpts {
  /* Don't draw */
  KEYFRAME_HANDLE_NONE = 0,
  /* Various marks in order of increasing display priority. */
  KEYFRAME_HANDLE_AUTO_CLAMP,
  KEYFRAME_HANDLE_AUTO,
  KEYFRAME_HANDLE_VECTOR,
  KEYFRAME_HANDLE_ALIGNED,
  KEYFRAME_HANDLE_FREE,
} eKeyframeHandleDrawOpts;

/* Extreme type. */
typedef enum eKeyframeExtremeDrawOpts {
  KEYFRAME_EXTREME_NONE = 0,
  /* Minimum/maximum present. */
  KEYFRAME_EXTREME_MIN = (1 << 0),
  KEYFRAME_EXTREME_MAX = (1 << 1),
  /* Grouped keys have different states. */
  KEYFRAME_EXTREME_MIXED = (1 << 2),
  /* Both neighbors are equal to this key. */
  KEYFRAME_EXTREME_FLAT = (1 << 3),
} eKeyframeExtremeDrawOpts;

/* ******************************* Methods ****************************** */

struct AnimKeylist *ED_keylist_create(void);
void ED_keylist_free(struct AnimKeylist *keylist);
void ED_keylist_prepare_for_direct_access(struct AnimKeylist *keylist);
const struct ActKeyColumn *ED_keylist_find_exact(const struct AnimKeylist *keylist, float cfra);
const struct ActKeyColumn *ED_keylist_find_next(const struct AnimKeylist *keylist, float cfra);
const struct ActKeyColumn *ED_keylist_find_prev(const struct AnimKeylist *keylist, float cfra);
const struct ActKeyColumn *ED_keylist_find_any_between(const struct AnimKeylist *keylist,
                                                       const Range2f frame_range);
bool ED_keylist_is_empty(const struct AnimKeylist *keylist);
const struct ListBase /* ActKeyColumn */ *ED_keylist_listbase(const struct AnimKeylist *keylist);
bool ED_keylist_all_keys_frame_range(const struct AnimKeylist *keylist, Range2f *r_frame_range);
/* Return the selected keyframe's range. If none are selected, return False and
 * do not affect the frame range. */
bool ED_keylist_selected_keys_frame_range(const struct AnimKeylist *keylist,
                                          Range2f *r_frame_range);
const ActKeyColumn *ED_keylist_array(const struct AnimKeylist *keylist);
int64_t ED_keylist_array_len(const struct AnimKeylist *keylist);

/* Key-data Generation --------------- */

/* F-Curve */
void fcurve_to_keylist(struct AnimData *adt,
                       struct FCurve *fcu,
                       struct AnimKeylist *keylist,
                       int saction_flag);
/* Action Group */
void agroup_to_keylist(struct AnimData *adt,
                       struct bActionGroup *agrp,
                       struct AnimKeylist *keylist,
                       int saction_flag);
/* Action */
void action_to_keylist(struct AnimData *adt,
                       struct bAction *act,
                       struct AnimKeylist *keylist,
                       int saction_flag);
/* Object */
void ob_to_keylist(struct bDopeSheet *ads,
                   struct Object *ob,
                   struct AnimKeylist *keylist,
                   int saction_flag);
/* Cache File */
void cachefile_to_keylist(struct bDopeSheet *ads,
                          struct CacheFile *cache_file,
                          struct AnimKeylist *keylist,
                          int saction_flag);
/* Scene */
void scene_to_keylist(struct bDopeSheet *ads,
                      struct Scene *sce,
                      struct AnimKeylist *keylist,
                      int saction_flag);
/* DopeSheet Summary */
void summary_to_keylist(struct bAnimContext *ac, struct AnimKeylist *keylist, int saction_flag);
/* Grease Pencil datablock summary */
void gpencil_to_keylist(struct bDopeSheet *ads,
                        struct bGPdata *gpd,
                        struct AnimKeylist *keylist,
                        bool active);

/* Grease Pencil Cels */
void grease_pencil_cels_to_keylist(struct AnimData *adt,
                                   struct GreasePencilLayer *layer,
                                   struct AnimKeylist *keylist,
                                   int saction_flag);

/* Grease Pencil Layer */
void gpl_to_keylist(struct bDopeSheet *ads, struct bGPDlayer *gpl, struct AnimKeylist *keylist);
/* Mask */
void mask_to_keylist(struct bDopeSheet *ads,
                     struct MaskLayer *masklay,
                     struct AnimKeylist *keylist);

/* ActKeyColumn API ---------------- */

/** Checks if #ActKeyColumn has any block data. */
bool actkeyblock_is_valid(const ActKeyColumn *ac);

/** Checks if #ActKeyColumn can be used as a block (i.e. drawn/used to detect "holds"). */
int actkeyblock_get_valid_hold(const ActKeyColumn *ac);
