/**
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_KEYFRAMES_EDIT_H
#define ED_KEYFRAMES_EDIT_H

struct bAnimContext;
struct bAnimListElem;
struct FCurve;
struct BezTriple;
struct Scene;

/* ************************************************ */
/* Common Macros and Defines */

/* --------- BezTriple Selection ------------- */

#define BEZ_SEL(bezt)		{ (bezt)->f1 |=  SELECT; (bezt)->f2 |=  SELECT; (bezt)->f3 |=  SELECT; }
#define BEZ_DESEL(bezt)		{ (bezt)->f1 &= ~SELECT; (bezt)->f2 &= ~SELECT; (bezt)->f3 &= ~SELECT; }
#define BEZ_INVSEL(bezt)	{ (bezt)->f1 ^=  SELECT; (bezt)->f2 ^=  SELECT; (bezt)->f3 ^=  SELECT; }

/* --------- Tool Flags ------------ */

/* bezt validation */
typedef enum eEditKeyframes_Validate {
	BEZT_OK_FRAME	= 1,
	BEZT_OK_FRAMERANGE,
	BEZT_OK_SELECTED,
	BEZT_OK_VALUE,
	BEZT_OK_VALUERANGE,
	BEZT_OK_REGION,
} eEditKeyframes_Validate;

/* ------------ */

/* select modes */
typedef enum eEditKeyframes_Select {
		/* SELECT_SUBTRACT for all, followed by SELECT_ADD for some */
	SELECT_REPLACE	=	(1<<0),
		/* add ok keyframes to selection */
	SELECT_ADD		= 	(1<<1),
		/* remove ok keyframes from selection */
	SELECT_SUBTRACT	= 	(1<<2),
		/* flip ok status of keyframes based on key status */
	SELECT_INVERT	= 	(1<<3),
} eEditKeyframes_Select;

/* "selection map" building modes */
typedef enum eEditKeyframes_SelMap {
	SELMAP_MORE	= 0,
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
} eEditKeyframes_Snap;

/* mirroring tools */
typedef enum eEditKeyframes_Mirror {
	MIRROR_KEYS_CURFRAME = 1,
	MIRROR_KEYS_YAXIS,
	MIRROR_KEYS_XAXIS,
	MIRROR_KEYS_MARKER,
	MIRROR_KEYS_VALUE,
} eEditKeyframes_Mirror;

/* ************************************************ */
/* Non-Destuctive Editing API (keyframes_edit.c) */

/* --- Generic Properties for Keyframe Edit Tools ----- */

typedef struct KeyframeEditData {
		/* generic properties/data access */
	ListBase list;				/* temp list for storing custom list of data to check */
	struct Scene *scene;		/* pointer to current scene - many tools need access to cfra/etc.  */
	void *data;					/* pointer to custom data - usually 'Object' but also 'rectf', but could be other types too */
	float f1, f2;				/* storage of times/values as 'decimals' */
	int i1, i2;					/* storage of times/values/flags as 'whole' numbers */
	
		/* current iteration data */
	struct FCurve *fcu;			/* F-Curve that is being iterated over */
	int curIndex;				/* index of current keyframe being iterated over */
	
		/* flags */
	short curflags;				/* current flags for the keyframe we're reached in the iteration process */
	short iterflags;			/* settings for iteration process */ // XXX: unused...
} KeyframeEditData;

/* ------- Function Pointer Typedefs ---------------- */

	/* callback function that refreshes the F-Curve after use */
typedef void (*FcuEditFunc)(struct FCurve *fcu);
	/* callback function that operates on the given BezTriple */
typedef short (*KeyframeEditFunc)(KeyframeEditData *ked, struct BezTriple *bezt);

/* ---------- Defines for 'OK' polls ----------------- */

/* which verts of a keyframe is active (after polling) */
typedef enum eKeyframeVertOk {
		/* 'key' itself is ok */
	KEYFRAME_OK_KEY		= (1<<0),
		/* 'handle 1' is ok */
	KEYFRAME_OK_H1		= (1<<1),
		/* 'handle 2' is ok */
	KEYFRAME_OK_H2		= (1<<2),
		/* all flags */
	KEYFRAME_OK_ALL		= (KEYFRAME_OK_KEY|KEYFRAME_OK_H1|KEYFRAME_OK_H2)
} eKeyframeVertOk;

/* Flags for use during iteration */
typedef enum eKeyframeIterFlags {
		/* consider handles in addition to key itself */
	KEYFRAME_ITER_INCL_HANDLES	= (1<<0),
} eKeyframeIterFlags;	

/* ------- Custom Data Type Defines ------------------ */

/* Custom data for remapping one range to another in a fixed way */
typedef struct KeyframeEditCD_Remap {
	float oldMin, oldMax;			/* old range */
	float newMin, newMax;			/* new range */
} KeyframeEditCD_Remap;

/* ---------------- Looping API --------------------- */

/* functions for looping over keyframes */
	/* function for working with F-Curve data only (i.e. when filters have been chosen to explicitly use this) */
short ANIM_fcurve_keyframes_loop(KeyframeEditData *ked, struct FCurve *fcu, KeyframeEditFunc key_ok, KeyframeEditFunc key_cb, FcuEditFunc fcu_cb);
	/* function for working with any type (i.e. one of the known types) of animation channel 
	 *	- filterflag is bDopeSheet->flag (DOPESHEET_FILTERFLAG)
	 */
short ANIM_animchannel_keyframes_loop(KeyframeEditData *ked, struct bAnimListElem *ale, KeyframeEditFunc key_ok, KeyframeEditFunc key_cb, FcuEditFunc fcu_cb, int filterflag);
	/* same as above, except bAnimListElem wrapper is not needed... 
	 * 	- keytype is eAnim_KeyType
	 */
short ANIM_animchanneldata_keyframes_loop(KeyframeEditData *ked, void *data, int keytype, KeyframeEditFunc key_ok, KeyframeEditFunc key_cb, FcuEditFunc fcu_cb, int filterflag);

/* functions for making sure all keyframes are in good order */
void ANIM_editkeyframes_refresh(struct bAnimContext *ac);

/* ----------- BezTriple Callback Getters ---------- */

/* accessories */
KeyframeEditFunc ANIM_editkeyframes_ok(short mode);

/* edit */
KeyframeEditFunc ANIM_editkeyframes_snap(short mode);
KeyframeEditFunc ANIM_editkeyframes_mirror(short mode);
KeyframeEditFunc ANIM_editkeyframes_select(short mode);
KeyframeEditFunc ANIM_editkeyframes_handles(short mode);
KeyframeEditFunc ANIM_editkeyframes_ipo(short mode);
KeyframeEditFunc ANIM_editkeyframes_keytype(short mode);

/* -------- BezTriple Callbacks (Selection Map) ---------- */

/* Get a callback to populate the selection settings map  
 * requires: ked->custom = char[] of length fcurve->totvert 
 */
KeyframeEditFunc ANIM_editkeyframes_buildselmap(short mode);

/* Change the selection status of the keyframe based on the map entry for this vert
 * requires: ked->custom = char[] of length fcurve->totvert
 */
short bezt_selmap_flush(KeyframeEditData *ked, struct BezTriple *bezt);

/* ----------- BezTriple Callback (Assorted Utilities) ---------- */

/* used to calculate the the average location of all relevant BezTriples by summing their locations */
short bezt_calc_average(KeyframeEditData *ked, struct BezTriple *bezt);

/* used to extract a set of cfra-elems from the keyframes */
short bezt_to_cfraelem(KeyframeEditData *ked, struct BezTriple *bezt);

/* used to remap times from one range to another
 * requires:  ked->custom = KeyframeEditCD_Remap	
 */
void bezt_remap_times(KeyframeEditData *ked, struct BezTriple *bezt);

/* ************************************************ */
/* Destructive Editing API (keyframes_general.c) */

void delete_fcurve_key(struct FCurve *fcu, int index, short do_recalc);
void delete_fcurve_keys(struct FCurve *fcu);
void duplicate_fcurve_keys(struct FCurve *fcu);

void clean_fcurve(struct FCurve *fcu, float thresh);
void smooth_fcurve(struct FCurve *fcu);
void sample_fcurve(struct FCurve *fcu);

/* ----------- */

void free_anim_copybuf(void);
short copy_animedit_keys(struct bAnimContext *ac, ListBase *anim_data);
short paste_animedit_keys(struct bAnimContext *ac, ListBase *anim_data);

/* ************************************************ */

#endif /* ED_KEYFRAMES_EDIT_H */
