/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_keyframes_draw.h
 *  \ingroup editors
 */

#ifndef __ED_KEYFRAMES_DRAW_H__
#define __ED_KEYFRAMES_DRAW_H__

struct bAnimContext;
struct AnimData;
struct CacheFile;
struct FCurve;
struct bDopeSheet;
struct bAction;
struct bActionGroup;
struct Object;
struct ListBase;
struct bGPDlayer;
struct Palette;
struct MaskLayer;
struct Scene;
struct View2D;
struct DLRBT_Tree;

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
	struct ActKeyColumn *left, *right;  /* 'children' of this node, less than and greater than it (respectively) */
	struct ActKeyColumn *parent;        /* parent of this node in the tree */
	char tree_col;                      /* DLRB_BLACK or DLRB_RED */

	/* keyframe info */
	char key_type;                      /* eBezTripe_KeyframeType */
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
	ACTKEYBLOCK_FLAG_MOVING_HOLD     = (1 << 0),
	/* Key block represents a static hold */
	ACTKEYBLOCK_FLAG_STATIC_HOLD     = (1 << 1),
	/* Key block represents any kind of hold */
	ACTKEYBLOCK_FLAG_ANY_HOLD        = (1 << 2),
} eActKeyBlock_Flag;

/* *********************** Keyframe Drawing ****************************** */

/* options for keyframe shape drawing */
typedef enum eKeyframeShapeDrawOpts {
	/* only the border */
	KEYFRAME_SHAPE_FRAME = 0,
	/* only the inside filling */
	KEYFRAME_SHAPE_INSIDE,
	/* the whole thing */
	KEYFRAME_SHAPE_BOTH
} eKeyframeShapeDrawOpts;

/* draw simple diamond-shape keyframe */
/* caller should set up vertex format, bind GPU_SHADER_KEYFRAME_DIAMOND, immBegin(GPU_PRIM_POINTS, n), then call this n times */
void draw_keyframe_shape(float x, float y, float size, bool sel, short key_type, short mode, float alpha,
                         unsigned int pos_id, unsigned int size_id, unsigned int color_id, unsigned int outline_color_id);

/* ******************************* Methods ****************************** */

/* Channel Drawing ------------------ */
/* F-Curve */
void draw_fcurve_channel(struct View2D *v2d, struct AnimData *adt, struct FCurve *fcu, float ypos, float yscale_fac);
/* Action Group Summary */
void draw_agroup_channel(struct View2D *v2d, struct AnimData *adt, struct bActionGroup *agrp, float ypos, float yscale_fac);
/* Action Summary */
void draw_action_channel(struct View2D *v2d, struct AnimData *adt, struct bAction *act, float ypos, float yscale_fac);
/* Object Summary */
void draw_object_channel(struct View2D *v2d, struct bDopeSheet *ads, struct Object *ob, float ypos, float yscale_fac);
/* Scene Summary */
void draw_scene_channel(struct View2D *v2d, struct bDopeSheet *ads, struct Scene *sce, float ypos, float yscale_fac);
/* DopeSheet Summary */
void draw_summary_channel(struct View2D *v2d, struct bAnimContext *ac, float ypos, float yscale_fac);
/* Grease Pencil datablock summary */
void draw_gpencil_channel(struct View2D *v2d, struct bDopeSheet *ads, struct bGPdata *gpd, float ypos, float yscale_fac);
/* Grease Pencil Layer */
void draw_gpl_channel(struct View2D *v2d, struct bDopeSheet *ads, struct bGPDlayer *gpl, float ypos, float yscale_fac);
/* Mask Layer */
void draw_masklay_channel(struct View2D *v2d, struct bDopeSheet *ads, struct MaskLayer *masklay, float ypos, float yscale_fac);

/* Keydata Generation --------------- */
/* F-Curve */
void fcurve_to_keylist(struct AnimData *adt, struct FCurve *fcu, struct DLRBT_Tree *keys);
/* Action Group */
void agroup_to_keylist(struct AnimData *adt, struct bActionGroup *agrp, struct DLRBT_Tree *keys);
/* Action */
void action_to_keylist(struct AnimData *adt, struct bAction *act, struct DLRBT_Tree *keys);
/* Object */
void ob_to_keylist(struct bDopeSheet *ads, struct Object *ob, struct DLRBT_Tree *keys);
/* Cache File */
void cachefile_to_keylist(struct bDopeSheet *ads, struct CacheFile *cache_file, struct DLRBT_Tree *keys);
/* Scene */
void scene_to_keylist(struct bDopeSheet *ads, struct Scene *sce, struct DLRBT_Tree *keys);
/* DopeSheet Summary */
void summary_to_keylist(struct bAnimContext *ac, struct DLRBT_Tree *keys);
/* Grease Pencil datablock summary */
void gpencil_to_keylist(struct bDopeSheet *ads, struct bGPdata *gpd, struct DLRBT_Tree *keys, const bool active);
/* Grease Pencil Layer */
void gpl_to_keylist(struct bDopeSheet *ads, struct bGPDlayer *gpl, struct DLRBT_Tree *keys);
/* Palette */
void palette_to_keylist(struct bDopeSheet *ads, struct Palette *palette, struct DLRBT_Tree *keys);
/* Mask */
void mask_to_keylist(struct bDopeSheet *UNUSED(ads), struct MaskLayer *masklay, struct DLRBT_Tree *keys);

/* ActKeyColumn API ---------------- */
/* Comparator callback used for ActKeyColumns and cframe float-value pointer */
short compare_ak_cfraPtr(void *node, void *data);

/* Checks if ActKeyColumn can be used as a block (i.e. drawn/used to detect "holds") */
int actkeyblock_get_valid_hold(ActKeyColumn *ab);

#endif  /*  __ED_KEYFRAMES_DRAW_H__ */
