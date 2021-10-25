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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_keyframes_edit.h
 *  \ingroup editors
 */

#ifndef __ED_KEYFRAMES_EDIT_H__
#define __ED_KEYFRAMES_EDIT_H__

struct bAnimContext;
struct bAnimListElem;
struct bDopeSheet;
struct FCurve;
struct BezTriple;
struct Scene;

/* ************************************************ */
/* Common Macros and Defines */

/* --------- Tool Flags ------------ */

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

/* ------------ */

/* select modes */
typedef enum eEditKeyframes_Select {
	/* SELECT_SUBTRACT for all, followed by SELECT_ADD for some */
	SELECT_REPLACE  =   (1 << 0),
	/* add ok keyframes to selection */
	SELECT_ADD      =   (1 << 1),
	/* remove ok keyframes from selection */
	SELECT_SUBTRACT =   (1 << 2),
	/* flip ok status of keyframes based on key status */
	SELECT_INVERT   =   (1 << 3)
} eEditKeyframes_Select;

/* "selection map" building modes */
typedef enum eEditKeyframes_SelMap {
	SELMAP_MORE = 0,
	SELMAP_LESS
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
	const int (*mcords)[2];
	int mcords_tot;
} KeyframeEdit_LassoData;

/* use with BEZT_OK_REGION_CIRCLE */
typedef struct KeyframeEdit_CircleData {
	rctf *rectf_scaled;
	const rctf *rectf_view;
	float mval[2];
	float radius_squared;
} KeyframeEdit_CircleData;


/* ************************************************ */
/* Non-Destuctive Editing API (keyframes_edit.c) */

/* --- Defines for 'OK' polls + KeyframeEditData Flags --------- */

/* which verts of a keyframe is active (after polling) */
typedef enum eKeyframeVertOk {
	/* 'key' itself is ok */
	KEYFRAME_OK_KEY     = (1 << 0),
	/* 'handle 1' is ok */
	KEYFRAME_OK_H1      = (1 << 1),
	/* 'handle 2' is ok */
	KEYFRAME_OK_H2      = (1 << 2),
	/* all flags */
	KEYFRAME_OK_ALL     = (KEYFRAME_OK_KEY | KEYFRAME_OK_H1 | KEYFRAME_OK_H2)
} eKeyframeVertOk;

/* Flags for use during iteration */
typedef enum eKeyframeIterFlags {
	/* consider handles in addition to key itself */
	KEYFRAME_ITER_INCL_HANDLES  = (1 << 0),
	
	/* Perform NLA time remapping (global -> strip) for the "f1" parameter
	 * (e.g. used for selection tools on summary tracks)
	 */
	KED_F1_NLA_UNMAP            = (1 << 1),
	
	/* Perform NLA time remapping (global -> strip) for the "f2" parameter */
	KED_F2_NLA_UNMAP            = (1 << 2),
} eKeyframeIterFlags;

/* --- Generic Properties for Keyframe Edit Tools ----- */

typedef struct KeyframeEditData {
	/* generic properties/data access */
	ListBase list;              /* temp list for storing custom list of data to check */
	struct Scene *scene;        /* pointer to current scene - many tools need access to cfra/etc.  */
	void *data;                 /* pointer to custom data - usually 'Object' but also 'rectf', but could be other types too */
	float f1, f2;               /* storage of times/values as 'decimals' */
	int i1, i2;                 /* storage of times/values/flags as 'whole' numbers */

	/* current iteration data */
	struct FCurve *fcu;         /* F-Curve that is being iterated over */
	int curIndex;               /* index of current keyframe being iterated over */
	float channel_y;            /* y-position of midpoint of the channel (for the dopesheet) */
	
	/* flags */
	eKeyframeVertOk curflags;        /* current flags for the keyframe we're reached in the iteration process */
	eKeyframeIterFlags iterflags;    /* settings for iteration process */
} KeyframeEditData;

/* ------- Function Pointer Typedefs ---------------- */

/* callback function that refreshes the F-Curve after use */
typedef void (*FcuEditFunc)(struct FCurve *fcu);
/* callback function that operates on the given BezTriple */
typedef short (*KeyframeEditFunc)(KeyframeEditData *ked, struct BezTriple *bezt);


/* ------- Custom Data Type Defines ------------------ */

/* Custom data for remapping one range to another in a fixed way */
typedef struct KeyframeEditCD_Remap {
	float oldMin, oldMax;           /* old range */
	float newMin, newMax;           /* new range */
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
	KEYFRAME_PASTE_OFFSET_NONE
} eKeyPasteOffset;

typedef enum eKeyMergeMode {
	/* overlay existing with new keys */
	KEYFRAME_PASTE_MERGE_MIX,
	/* replace entire fcurve */
	KEYFRAME_PASTE_MERGE_OVER,
	/* overwrite keys in pasted range */
	KEYFRAME_PASTE_MERGE_OVER_RANGE,
	/* overwrite keys in pasted range (use all keyframe start & end for range) */
	KEYFRAME_PASTE_MERGE_OVER_RANGE_ALL
} eKeyMergeMode;

/* ---------------- Looping API --------------------- */

/* functions for looping over keyframes */
/* function for working with F-Curve data only (i.e. when filters have been chosen to explicitly use this) */
short ANIM_fcurve_keyframes_loop(KeyframeEditData *ked, struct FCurve *fcu, KeyframeEditFunc key_ok, KeyframeEditFunc key_cb, FcuEditFunc fcu_cb);
/* function for working with any type (i.e. one of the known types) of animation channel
 *  - filterflag is bDopeSheet->flag (DOPESHEET_FILTERFLAG)
 */
short ANIM_animchannel_keyframes_loop(KeyframeEditData *ked, struct bDopeSheet *ads, struct bAnimListElem *ale, KeyframeEditFunc key_ok, KeyframeEditFunc key_cb, FcuEditFunc fcu_cb);
/* same as above, except bAnimListElem wrapper is not needed...
 *  - keytype is eAnim_KeyType
 */
short ANIM_animchanneldata_keyframes_loop(KeyframeEditData *ked, struct bDopeSheet *ads, void *data, int keytype, KeyframeEditFunc key_ok, KeyframeEditFunc key_cb, FcuEditFunc fcu_cb);

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
KeyframeEditFunc ANIM_editkeyframes_easing(short mode);

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

/* used to calculate the average location of all relevant BezTriples by summing their locations */
short bezt_calc_average(KeyframeEditData *ked, struct BezTriple *bezt);

/* used to extract a set of cfra-elems from the keyframes */
short bezt_to_cfraelem(KeyframeEditData *ked, struct BezTriple *bezt);

/* used to remap times from one range to another
 * requires:  ked->custom = KeyframeEditCD_Remap	
 */
void bezt_remap_times(KeyframeEditData *ked, struct BezTriple *bezt);

/* ------ 1.5-D Region Testing Uitls (Lasso/Circle Select) ------- */
/* XXX: These are temporary, until we can unify GP/Mask Keyframe handling and standard FCurve Keyframe handling */

bool keyframe_region_lasso_test(
        const KeyframeEdit_LassoData *data_lasso,
        const float xy[2]);

bool keyframe_region_circle_test(
        const KeyframeEdit_CircleData *data_circle,
        const float xy[2]);


/* ************************************************ */
/* Destructive Editing API (keyframes_general.c) */

void delete_fcurve_key(struct FCurve *fcu, int index, bool do_recalc);
bool delete_fcurve_keys(struct FCurve *fcu);
void clear_fcurve_keys(struct FCurve *fcu);
void duplicate_fcurve_keys(struct FCurve *fcu);

void clean_fcurve(struct bAnimContext *ac, struct bAnimListElem *ale, float thresh, bool cleardefault);
void smooth_fcurve(struct FCurve *fcu);
void sample_fcurve(struct FCurve *fcu);

/* ----------- */

void ANIM_fcurves_copybuf_free(void);
short copy_animedit_keys(struct bAnimContext *ac, ListBase *anim_data);
short paste_animedit_keys(struct bAnimContext *ac, ListBase *anim_data,
                          const eKeyPasteOffset offset_mode, const eKeyMergeMode merge_mode, bool flip);

/* ************************************************ */

#endif /* __ED_KEYFRAMES_EDIT_H__ */
