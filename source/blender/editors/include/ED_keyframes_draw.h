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
 * The Original Code is Copyright (C) (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_KEYFRAMES_DRAW_H__
#define __ED_KEYFRAMES_DRAW_H__

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct CacheFile;
struct DLRBT_Tree;
struct FCurve;
struct ListBase;
struct MaskLayer;
struct Object;
struct Scene;
struct View2D;
struct bAction;
struct bActionGroup;
struct bAnimContext;
struct bDopeSheet;
struct bGPDlayer;

/* ****************************** Base Structs ****************************** */

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

/* draw simple diamond-shape keyframe */
/* caller should set up vertex format, bind GPU_SHADER_KEYFRAME_DIAMOND,
 * immBegin(GPU_PRIM_POINTS, n), then call this n times */
void draw_keyframe_shape(float x,
                         float y,
                         float size,
                         bool sel,
                         short key_type,
                         short mode,
                         float alpha,
                         unsigned int pos_id,
                         unsigned int size_id,
                         unsigned int color_id,
                         unsigned int outline_color_id,
                         unsigned int linemask_id,
                         short ipo_type,
                         short extreme_type);

/* ******************************* Methods ****************************** */

/* Channel Drawing ------------------ */
/* F-Curve */
void draw_fcurve_channel(struct View2D *v2d,
                         struct AnimData *adt,
                         struct FCurve *fcu,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Action Group Summary */
void draw_agroup_channel(struct View2D *v2d,
                         struct AnimData *adt,
                         struct bActionGroup *agrp,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Action Summary */
void draw_action_channel(struct View2D *v2d,
                         struct AnimData *adt,
                         struct bAction *act,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Object Summary */
void draw_object_channel(struct View2D *v2d,
                         struct bDopeSheet *ads,
                         struct Object *ob,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Scene Summary */
void draw_scene_channel(struct View2D *v2d,
                        struct bDopeSheet *ads,
                        struct Scene *sce,
                        float ypos,
                        float yscale_fac,
                        int saction_flag);
/* DopeSheet Summary */
void draw_summary_channel(
    struct View2D *v2d, struct bAnimContext *ac, float ypos, float yscale_fac, int saction_flag);
/* Grease Pencil datablock summary */
void draw_gpencil_channel(struct View2D *v2d,
                          struct bDopeSheet *ads,
                          struct bGPdata *gpd,
                          float ypos,
                          float yscale_fac,
                          int saction_flag);
/* Grease Pencil Layer */
void draw_gpl_channel(struct View2D *v2d,
                      struct bDopeSheet *ads,
                      struct bGPDlayer *gpl,
                      float ypos,
                      float yscale_fac,
                      int saction_flag);
/* Mask Layer */
void draw_masklay_channel(struct View2D *v2d,
                          struct bDopeSheet *ads,
                          struct MaskLayer *masklay,
                          float ypos,
                          float yscale_fac,
                          int saction_flag);

/* Keydata Generation --------------- */
/* F-Curve */
void fcurve_to_keylist(struct AnimData *adt,
                       struct FCurve *fcu,
                       struct DLRBT_Tree *keys,
                       int saction_flag);
/* Action Group */
void agroup_to_keylist(struct AnimData *adt,
                       struct bActionGroup *agrp,
                       struct DLRBT_Tree *keys,
                       int saction_flag);
/* Action */
void action_to_keylist(struct AnimData *adt,
                       struct bAction *act,
                       struct DLRBT_Tree *keys,
                       int saction_flag);
/* Object */
void ob_to_keylist(struct bDopeSheet *ads,
                   struct Object *ob,
                   struct DLRBT_Tree *keys,
                   int saction_flag);
/* Cache File */
void cachefile_to_keylist(struct bDopeSheet *ads,
                          struct CacheFile *cache_file,
                          struct DLRBT_Tree *keys,
                          int saction_flag);
/* Scene */
void scene_to_keylist(struct bDopeSheet *ads,
                      struct Scene *sce,
                      struct DLRBT_Tree *keys,
                      int saction_flag);
/* DopeSheet Summary */
void summary_to_keylist(struct bAnimContext *ac, struct DLRBT_Tree *keys, int saction_flag);
/* Grease Pencil datablock summary */
void gpencil_to_keylist(struct bDopeSheet *ads,
                        struct bGPdata *gpd,
                        struct DLRBT_Tree *keys,
                        const bool active);
/* Grease Pencil Layer */
void gpl_to_keylist(struct bDopeSheet *ads, struct bGPDlayer *gpl, struct DLRBT_Tree *keys);
/* Mask */
void mask_to_keylist(struct bDopeSheet *UNUSED(ads),
                     struct MaskLayer *masklay,
                     struct DLRBT_Tree *keys);

/* ActKeyColumn API ---------------- */
/* Comparator callback used for ActKeyColumns and cframe float-value pointer */
short compare_ak_cfraPtr(void *node, void *data);

/* Checks if ActKeyColumn has any block data */
bool actkeyblock_is_valid(ActKeyColumn *ab);

/* Checks if ActKeyColumn can be used as a block (i.e. drawn/used to detect "holds") */
int actkeyblock_get_valid_hold(ActKeyColumn *ab);

#ifdef __cplusplus
}
#endif

#endif /*  __ED_KEYFRAMES_DRAW_H__ */
