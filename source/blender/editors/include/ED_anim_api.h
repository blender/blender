/**
 * $Id:
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_ANIM_API_H
#define ED_ANIM_API_H

struct ID;
struct Scene;
struct ListBase;
struct bContext;
struct wmWindowManager;
struct ScrArea;
struct ARegion;
struct View2D;
struct gla2DDrawInfo;
struct Object;
struct bActionGroup;
struct FCurve;
struct IpoCurve; // xxx

/* ************************************************ */
/* ANIMATION CHANNEL FILTERING */
/* anim_filter.c */

/* --------------- Context --------------------- */

/* This struct defines a structure used for animation-specific 
 * 'context' information
 */
typedef struct bAnimContext {
	void *data;				/* data to be filtered for use in animation editor */
	short datatype;			/* type of data eAnimCont_Types */
	
	short mode;				/* editor->mode */
	short spacetype;		/* sa->spacetype */
	short regiontype;		/* active region -> type (channels or main) */
	struct ScrArea *sa;		/* editor */ 
	struct ARegion *ar;		/* region within editor */
	
	struct Scene *scene;	/* active scene */
	struct Object *obact;	/* active object */
	ListBase *markers;		/* active set of markers */
} bAnimContext;

/* Main Data container types */
// XXX was ACTCONT_*
typedef enum eAnimCont_Types {
	ANIMCONT_NONE = 0,		/* invalid or no data */
	ANIMCONT_ACTION,		/* action (bAction) */
	ANIMCONT_SHAPEKEY,		/* shapekey (Key) */
	ANIMCONT_GPENCIL,		/* grease pencil (screen) */
	ANIMCONT_DOPESHEET,		/* dopesheet (bDopesheet) */
	ANIMCONT_FCURVES,		/* animation F-Curves (bDopesheet) */
	ANIMCONT_DRIVERS,		/* drivers (bDopesheet) */
	ANIMCONT_NLA,			/* nla (bDopesheet) */
} eAnimCont_Types;

/* --------------- Channels -------------------- */

/* This struct defines a structure used for quick and uniform access for 
 * channels of animation data
 */
typedef struct bAnimListElem {
	struct bAnimListElem *next, *prev;
	
	void 	*data;		/* source data this elem represents */
	int 	type;		/* one of the ANIMTYPE_* values */
	int		flag;		/* copy of elem's flags for quick access */
	int 	index;		/* copy of adrcode where applicable */
	
	void	*key_data;	/* motion data - mostly F-Curves, but can be other types too */
	short	datatype;	/* type of motion data to expect */
	
	struct ID *id;		/* ID block that channel is attached to (may be used  */
	
	void 	*owner;		/* group or channel which acts as this channel's owner */
	short	ownertype;	/* type of owner */
} bAnimListElem;


/* Some types for easier type-testing */
// XXX was ACTTYPE_*
typedef enum eAnim_ChannelType {
	ANIMTYPE_NONE= 0,
	ANIMTYPE_SPECIALDATA,
	
	ANIMTYPE_SCENE,
	ANIMTYPE_OBJECT,
	ANIMTYPE_GROUP,
	ANIMTYPE_FCURVE,
	
	ANIMTYPE_FILLACTD,
	ANIMTYPE_FILLDRIVERS,
	ANIMTYPE_FILLMATD,
	
	ANIMTYPE_DSMAT,
	ANIMTYPE_DSLAM,
	ANIMTYPE_DSCAM,
	ANIMTYPE_DSCUR,
	ANIMTYPE_DSSKEY,
	ANIMTYPE_DSWOR,
	
	ANIMTYPE_SHAPEKEY,		// XXX probably can become depreceated???
	
	ANIMTYPE_GPDATABLOCK,
	ANIMTYPE_GPLAYER,
	
	ANIMTYPE_NLATRACK,
} eAnim_ChannelType;

/* types of keyframe data in bAnimListElem */
typedef enum eAnim_KeyType {
	ALE_NONE = 0,		/* no keyframe data */
	ALE_FCURVE,			/* F-Curve */
	ALE_GPFRAME,		/* Grease Pencil Frames */
	ALE_NLASTRIP,		/* NLA Strips */
	
	// XXX the following are for summaries... should these be kept?
	ALE_SCE,			/* Scene summary */
	ALE_OB,				/* Object summary */
	ALE_ACT,			/* Action summary */
	ALE_GROUP,			/* Action Group summary */
} eAnim_KeyType;

/* ----------------- Filtering -------------------- */

/* filtering flags  - under what circumstances should a channel be added */
// XXX was ACTFILTER_*
typedef enum eAnimFilter_Flags {
	ANIMFILTER_VISIBLE		= (1<<0),	/* should channels be visible (in terms of hierarchy only) */
	ANIMFILTER_SEL			= (1<<1),	/* should channels be selected */
	ANIMFILTER_UNSEL		= (1<<2),	/* should channels be NOT selected */
	ANIMFILTER_FOREDIT		= (1<<3),	/* does editable status matter */
	ANIMFILTER_CURVESONLY	= (1<<4),	/* don't include summary-channels, etc. */
	ANIMFILTER_CHANNELS		= (1<<5),	/* make list for interface drawing */
	ANIMFILTER_ACTGROUPED	= (1<<6),	/* belongs to the active actiongroup */
	ANIMFILTER_CURVEVISIBLE	= (1<<7),	/* F-Curve is visible for editing/viewing in Graph Editor */
	ANIMFILTER_ACTIVE		= (1<<8),	/* channel should be 'active' */  // FIXME: this is only relevant for F-Curves for now
} eAnimFilter_Flags;


/* ---------- Flag Checking Macros ------------ */
// xxx check on all of these flags again...

/* Dopesheet only */
	/* 'Scene' channels */
#define SEL_SCEC(sce) ((sce->flag & SCE_DS_SELECTED))
#define EXPANDED_SCEC(sce) ((sce->flag & SCE_DS_COLLAPSED)==0)
	/* 'Sub-Scene' channels (flags stored in Data block) */
#define FILTER_WOR_SCED(wo) ((wo->flag & WO_DS_EXPAND))
	/* 'Object' channels */
#define SEL_OBJC(base) ((base->flag & SELECT))
#define EXPANDED_OBJC(ob) ((ob->nlaflag & OB_ADS_COLLAPSED)==0)
	/* 'Sub-object' channels (flags stored in Object block) */
#define FILTER_MAT_OBJC(ob) ((ob->nlaflag & OB_ADS_SHOWMATS))
	/* 'Sub-object' channels (flags stored in Data block) */
#define FILTER_SKE_OBJD(key) ((key->flag & KEYBLOCK_DS_EXPAND))
#define FILTER_MAT_OBJD(ma) ((ma->flag & MA_DS_EXPAND))
#define FILTER_LAM_OBJD(la) ((la->flag & LA_DS_EXPAND))
#define FILTER_CAM_OBJD(ca) ((ca->flag & CAM_DS_EXPAND))
#define FILTER_CUR_OBJD(cu) ((cu->flag & CU_DS_EXPAND))
	/* 'Sub-object/Action' channels (flags stored in Action) */
#define SEL_ACTC(actc) ((actc->flag & ACT_SELECTED))
#define EXPANDED_ACTC(actc) ((actc->flag & ACT_COLLAPSED)==0)
	/* 'Sub-AnimData' chanenls */
#define EXPANDED_DRVD(adt) ((adt->flag & ADT_DRIVERS_COLLAPSED)==0)

/* Actions (also used for Dopesheet) */
	/* Action Channel Group */
#define EDITABLE_AGRP(agrp) ((agrp->flag & AGRP_PROTECTED)==0)
#define EXPANDED_AGRP(agrp) (agrp->flag & AGRP_EXPANDED)
#define SEL_AGRP(agrp) ((agrp->flag & AGRP_SELECTED) || (agrp->flag & AGRP_ACTIVE))
	/* F-Curve Channels */
#define EDITABLE_FCU(fcu) ((fcu->flag & FCURVE_PROTECTED)==0)
#define SEL_FCU(fcu) (fcu->flag & (FCURVE_ACTIVE|FCURVE_SELECTED))

/* Grease Pencil only */
	/* Grease Pencil datablock settings */
#define EXPANDED_GPD(gpd) (gpd->flag & GP_DATA_EXPAND) 
	/* Grease Pencil Layer settings */
#define EDITABLE_GPL(gpl) ((gpl->flag & GP_LAYER_LOCKED)==0)
#define SEL_GPL(gpl) ((gpl->flag & GP_LAYER_ACTIVE) || (gpl->flag & GP_LAYER_SELECT))

/* NLA only */
#define SEL_NLT(nlt) (nlt->flag & NLATRACK_SELECTED)
#define EDITABLE_NLT(nlt) ((nlt->flag & NLATRACK_PROTECTED)==0)

/* -------------- Channel Defines -------------- */

/* channel heights */
#define ACHANNEL_FIRST			-16
#define	ACHANNEL_HEIGHT			16
#define ACHANNEL_HEIGHT_HALF	8
#define	ACHANNEL_SKIP			2
#define ACHANNEL_STEP			(ACHANNEL_HEIGHT + ACHANNEL_SKIP)

/* channel widths */
#define ACHANNEL_NAMEWIDTH		200

/* channel toggle-buttons */
#define ACHANNEL_BUTTON_WIDTH	16

/* ---------------- API  -------------------- */

/* Obtain list of filtered Animation channels to operate on.
 * Returns the number of channels in the list
 */
int ANIM_animdata_filter(bAnimContext *ac, ListBase *anim_data, int filter_mode, void *data, short datatype);

/* Obtain current anim-data context from Blender Context info.
 * Returns whether the operation was successful. 
 */
short ANIM_animdata_get_context(const struct bContext *C, bAnimContext *ac);

/* Obtain current anim-data context (from Animation Editor) given 
 * that Blender Context info has already been set. 
 * Returns whether the operation was successful.
 */
short ANIM_animdata_context_getdata(bAnimContext *ac);

/* ************************************************ */
/* ANIMATION CHANNELS LIST */
/* anim_channels.c */

/* ------------------------ API -------------------------- */

/* Deselect all animation channels */
void ANIM_deselect_anim_channels(void *data, short datatype, short test, short sel);

/* Set the 'active' channel of type channel_type, in the given action */
void ANIM_set_active_channel(void *data, short datatype, int filter, void *channel_data, short channel_type);

/* --------------- Settings and/or Defines -------------- */

/* flag-setting behaviour */
enum {
	ACHANNEL_SETFLAG_CLEAR = 0,
	ACHANNEL_SETFLAG_ADD,
	ACHANNEL_SETFLAG_TOGGLE
} eAnimChannels_SetFlag;

/* ************************************************ */
/* DRAWING API */
/* anim_draw.c */

/* ---------- Current Frame Drawing ---------------- */

/* flags for Current Frame Drawing */
enum {
		/* plain time indicator with no special indicators */
	DRAWCFRA_PLAIN			= 0,
		/* draw box indicating current frame number */
	DRAWCFRA_SHOW_NUMBOX	= (1<<0),
		/* time indication in seconds or frames */
	DRAWCFRA_UNIT_SECONDS 	= (1<<1),
		/* show time-offset line */
	DRAWCFRA_SHOW_TIMEOFS	= (1<<2),
} eAnimEditDraw_CurrentFrame; 

/* main call to draw current-frame indicator in an Animation Editor */
void ANIM_draw_cfra(const struct bContext *C, struct View2D *v2d, short flag);

/* ------------- Preview Range Drawing -------------- */

/* main call to draw preview range curtains */
void ANIM_draw_previewrange(const struct bContext *C, struct View2D *v2d);

/* ************************************************* */
/* ASSORTED TOOLS */

/* ------------ Animation F-Curves <-> Icons/Names Mapping ------------ */
/* anim_ipo_utils.c */

int geticon_anim_blocktype(short blocktype);

void getname_anim_fcurve(char *name, struct ID *id, struct FCurve *fcu);


void ipo_rainbow(int cur, int tot, float *out);


/* ------------- NLA-Mapping ----------------------- */
/* anim_draw.c */

/* Obtain the Object providing NLA-scaling for the given channel if applicable */
struct Object *ANIM_nla_mapping_get(bAnimContext *ac, bAnimListElem *ale);

/* Set/clear temporary mapping of coordinates from 'local-action' time to 'global-nla-scaled' time */
void ANIM_nla_mapping_draw(struct gla2DDrawInfo *di, struct Object *ob, short restore);

/* Apply/Unapply NLA mapping to all keyframes in the nominated IPO block */
void ANIM_nla_mapping_apply_fcurve(struct Object *ob, struct FCurve *fcu, short restore, short only_keys);

/* ------------- xxx macros ----------------------- */

#define BEZSELECTED(bezt) ((bezt->f2 & SELECT) || (bezt->f1 & SELECT) || (bezt->f3 & SELECT))


/* --------- anim_deps.c, animation updates -------- */

	/* generic update flush, does tagged objects only, reads from Context screen (layers) and scene */
void ED_anim_dag_flush_update(const struct bContext *C);
	/* only flush object */
void ED_anim_object_flush_update(const struct bContext *C, struct Object *ob);

/* pose <-> action syncing */
void ANIM_action_to_pose_sync(struct Object *ob);
void ANIM_pose_to_action_sync(struct Object *ob, struct ScrArea *sa);


/* what types of animation data was changed (for sending notifiers from animation tools) */
enum {
	ANIM_CHANGED_BOTH= 0,
	ANIM_CHANGED_KEYFRAMES_VALUES,
	ANIM_CHANGED_KEYFRAMES_SELECT,
	ANIM_CHANGED_CHANNELS
} eAnimData_Changed;

/* Send notifiers on behalf of animation editing tools, based on various context info */
void ANIM_animdata_send_notifiers(struct bContext *C, bAnimContext *ac, short data_changed);

/* ************************************************* */
/* OPERATORS */
	
	/* generic animation channels */
void ED_operatortypes_animchannels(void);
void ED_keymap_animchannels(struct wmWindowManager *wm);

	/* generic time editing */
void ED_operatortypes_anim(void);
void ED_keymap_anim(struct wmWindowManager *wm);

/* ************************************************ */

#endif /* ED_ANIM_API_H */

