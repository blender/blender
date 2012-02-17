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
struct BezTriple;
struct FCurve;
struct bDopeSheet;
struct bAction;
struct bActionGroup;
struct Object;
struct ListBase;
struct bGPDlayer;
struct Scene;
struct View2D;
struct DLRBT_Tree;

/* ****************************** Base Structs ****************************** */

/* Keyframe Column Struct */
typedef struct ActKeyColumn {
		/* ListBase linkage */
	struct ActKeyColumn *next, *prev;
	
		/* sorting-tree linkage */
	struct ActKeyColumn *left, *right;	/* 'children' of this node, less than and greater than it (respectively) */
	struct ActKeyColumn *parent;		/* parent of this node in the tree */
	char tree_col;						/* DLRB_BLACK or DLRB_RED */
	
		/* keyframe info */
	char key_type;						/* eBezTripe_KeyframeType */
	short sel;
	float cfra;
	
	/* only while drawing - used to determine if long-keyframe needs to be drawn */
	short modified;
	short totcurve;
} ActKeyColumn;

/* 'Long Keyframe' Struct */
typedef struct ActKeyBlock {
		/* ListBase linkage */
	struct ActKeyBlock *next, *prev;
	
		/* sorting-tree linkage */
	struct ActKeyBlock *left, *right;	/* 'children' of this node, less than and greater than it (respectively) */
	struct ActKeyBlock *parent;			/* parent of this node in the tree */
	char tree_col;						/* DLRB_BLACK or DLRB_RED */
	
		/* key-block info */
	char sel;
	short handle_type;
	float val;
	float start, end;
	
	/* only while drawing - used to determine if block needs to be drawn */
	short modified;
	short totcurve; 
} ActKeyBlock;

/* *********************** Keyframe Drawing ****************************** */

/* options for keyframe shape drawing */
typedef enum eKeyframeShapeDrawOpts {
		/* only the border */
	KEYFRAME_SHAPE_FRAME	= 0,
		/* only the inside filling */
	KEYFRAME_SHAPE_INSIDE,
		/* the whole thing */
	KEYFRAME_SHAPE_BOTH
} eKeyframeShapeDrawOpts;

/* draw simple diamond-shape keyframe (with OpenGL) */
void draw_keyframe_shape(float x, float y, float xscale, float hsize, short sel, short key_type, short mode, float alpha);

/* ******************************* Methods ****************************** */

/* Channel Drawing ------------------ */
/* F-Curve */
void draw_fcurve_channel(struct View2D *v2d, struct AnimData *adt, struct FCurve *fcu, float ypos);
/* Action Group Summary */
void draw_agroup_channel(struct View2D *v2d, struct AnimData *adt, struct bActionGroup *agrp, float ypos);
/* Action Summary */
void draw_action_channel(struct View2D *v2d, struct AnimData *adt, struct bAction *act, float ypos);
/* Object Summary */
void draw_object_channel(struct View2D *v2d, struct bDopeSheet *ads, struct Object *ob, float ypos);
/* Scene Summary */
void draw_scene_channel(struct View2D *v2d, struct bDopeSheet *ads, struct Scene *sce, float ypos);
/* DopeSheet Summary */
void draw_summary_channel(struct View2D *v2d, struct bAnimContext *ac, float ypos);
/* Grease Pencil Layer */ 
// XXX not restored 
void draw_gpl_channel(struct View2D *v2d, struct bDopeSheet *ads, struct bGPDlayer *gpl, float ypos);

/* Keydata Generation --------------- */
/* F-Curve */
void fcurve_to_keylist(struct AnimData *adt, struct FCurve *fcu, struct DLRBT_Tree *keys, struct DLRBT_Tree *blocks);
/* Action Group */
void agroup_to_keylist(struct AnimData *adt, struct bActionGroup *agrp, struct DLRBT_Tree *keys, struct DLRBT_Tree *blocks);
/* Action */
void action_to_keylist(struct AnimData *adt, struct bAction *act, struct DLRBT_Tree *keys, struct DLRBT_Tree *blocks);
/* Object */
void ob_to_keylist(struct bDopeSheet *ads, struct Object *ob, struct DLRBT_Tree *keys, struct DLRBT_Tree *blocks);
/* Scene */
void scene_to_keylist(struct bDopeSheet *ads, struct Scene *sce, struct DLRBT_Tree *keys, struct DLRBT_Tree *blocks);
/* DopeSheet Summary */
void summary_to_keylist(struct bAnimContext *ac, struct DLRBT_Tree *keys, struct DLRBT_Tree *blocks);
/* Grease Pencil Layer */
// XXX not restored
void gpl_to_keylist(struct bDopeSheet *ads, struct bGPDlayer *gpl, struct DLRBT_Tree *keys);

/* ActKeyColumn API ---------------- */
/* Comparator callback used for ActKeyColumns and cframe float-value pointer */
short compare_ak_cfraPtr(void *node, void *data);

/* Comparator callback used for ActKeyBlocks and cframe float-value pointer */
short compare_ab_cfraPtr(void *node, void *data);

/* Checks if ActKeyBlock can be used (i.e. drawn/used to detect "holds") */
short actkeyblock_is_valid(ActKeyBlock *ab, struct DLRBT_Tree *keys);

#endif  /*  __ED_KEYFRAMES_DRAW_H__ */

