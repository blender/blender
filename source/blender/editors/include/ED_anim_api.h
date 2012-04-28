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

/** \file ED_anim_api.h
 *  \ingroup editors
 */

#ifndef __ED_ANIM_API_H__
#define __ED_ANIM_API_H__

struct ID;
struct ListBase;
struct AnimData;

struct bContext;
struct wmKeyConfig;
struct ReportList;
struct ScrArea;
struct SpaceLink;
struct ARegion;
struct View2D;

struct Scene;
struct Object;

struct bDopeSheet;

struct bActionGroup;
struct FCurve;
struct FModifier;

struct uiBlock;
struct uiLayout;

struct PointerRNA;
struct PropertyRNA;

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
	struct ScrArea *sa;		/* editor host */
	struct SpaceLink *sl;	/* editor data */
	struct ARegion *ar;		/* region within editor */
	
	struct bDopeSheet *ads; /* dopesheet data for editor (or which is being used) */
	
	struct Scene *scene;	/* active scene */
	struct Object *obact;	/* active object */
	ListBase *markers;		/* active set of markers */
	
	struct ReportList *reports;	/* pointer to current reports list */
} bAnimContext;

/* Main Data container types */
typedef enum eAnimCont_Types {
	ANIMCONT_NONE = 0,		/* invalid or no data */
	ANIMCONT_ACTION,		/* action (bAction) */
	ANIMCONT_SHAPEKEY,		/* shapekey (Key) */
	ANIMCONT_GPENCIL,		/* grease pencil (screen) */
	ANIMCONT_DOPESHEET,		/* dopesheet (bDopesheet) */
	ANIMCONT_FCURVES,		/* animation F-Curves (bDopesheet) */
	ANIMCONT_DRIVERS,		/* drivers (bDopesheet) */
	ANIMCONT_NLA,			/* nla (bDopesheet) */
	ANIMCONT_CHANNEL		/* animation channel (bAnimListElem) */	
} eAnimCont_Types;

/* --------------- Channels -------------------- */

/* This struct defines a structure used for quick and uniform access for 
 * channels of animation data
 */
typedef struct bAnimListElem {
	struct bAnimListElem *next, *prev;
	
	void 	*data;			/* source data this elem represents */
	int 	type;			/* one of the ANIMTYPE_* values */
	int		flag;			/* copy of elem's flags for quick access */
	int 	index;			/* for un-named data, the index of the data in it's collection */
	
	short	datatype;		/* type of motion data to expect */
	void	*key_data;		/* motion data - mostly F-Curves, but can be other types too */
	
	
	struct ID *id;			/* ID block that channel is attached to */
	struct AnimData *adt; 	/* source of the animation data attached to ID block (for convenience) */
} bAnimListElem;


/* Some types for easier type-testing 
 * NOTE: need to keep the order of these synchronized with the channels define code
 * 		which is used for drawing and handling channel lists for 
 */
typedef enum eAnim_ChannelType {
	ANIMTYPE_NONE= 0,
	ANIMTYPE_ANIMDATA,
	ANIMTYPE_SPECIALDATA,
	
	ANIMTYPE_SUMMARY,
	
	ANIMTYPE_SCENE,
	ANIMTYPE_OBJECT,
	ANIMTYPE_GROUP,
	ANIMTYPE_FCURVE,
	
	ANIMTYPE_FILLACTD,
	ANIMTYPE_FILLDRIVERS,
	
	ANIMTYPE_DSMAT,
	ANIMTYPE_DSLAM,
	ANIMTYPE_DSCAM,
	ANIMTYPE_DSCUR,
	ANIMTYPE_DSSKEY,
	ANIMTYPE_DSWOR,
	ANIMTYPE_DSNTREE,
	ANIMTYPE_DSPART,
	ANIMTYPE_DSMBALL,
	ANIMTYPE_DSARM,
	ANIMTYPE_DSMESH,
	ANIMTYPE_DSTEX,
	ANIMTYPE_DSLAT,
	ANIMTYPE_DSSPK,
	
	ANIMTYPE_SHAPEKEY,
	
	ANIMTYPE_GPDATABLOCK,
	ANIMTYPE_GPLAYER,
	
	ANIMTYPE_NLATRACK,
	ANIMTYPE_NLAACTION,
	
		/* always as last item, the total number of channel types... */
	ANIMTYPE_NUM_TYPES
} eAnim_ChannelType;

/* types of keyframe data in bAnimListElem */
typedef enum eAnim_KeyType {
	ALE_NONE = 0,		/* no keyframe data */
	ALE_FCURVE,			/* F-Curve */
	ALE_GPFRAME,		/* Grease Pencil Frames */
	ALE_NLASTRIP,		/* NLA Strips */
	
	ALE_ALL,			/* All channels summary */
	ALE_SCE,			/* Scene summary */
	ALE_OB,				/* Object summary */
	ALE_ACT,			/* Action summary */
	ALE_GROUP			/* Action Group summary */
} eAnim_KeyType;

/* ----------------- Filtering -------------------- */

/* filtering flags  - under what circumstances should a channel be returned */
typedef enum eAnimFilter_Flags {
	/* data which channel represents is fits the dopesheet filters (i.e. scene visibility criteria) */
	// XXX: it's hard to think of any examples where this *ISN'T* the case... perhaps becomes implicit?
	ANIMFILTER_DATA_VISIBLE   = (1<<0),
	/* channel is visible within the channel-list hierarchy (i.e. F-Curves within Groups in ActEdit) */
	ANIMFILTER_LIST_VISIBLE   = (1<<1),
	/* channel has specifically been tagged as visible in Graph Editor (* Graph Editor Only) */
	ANIMFILTER_CURVE_VISIBLE  = (1<<2),
	
	/* include summary channels and "expanders" (for drawing/mouse-selection in channel list) */
	ANIMFILTER_LIST_CHANNELS  = (1<<3),
	
	/* for its type, channel should be "active" one */
	ANIMFILTER_ACTIVE         = (1<<4),
	/* channel is a child of the active group (* Actions speciality) */
	ANIMFILTER_ACTGROUPED     = (1<<5),
	
	/* channel must be selected/not-selected, but both must not be set together */
	ANIMFILTER_SEL            = (1<<6),
	ANIMFILTER_UNSEL          = (1<<7),
	
	/* editability status - must be editable to be included */
	ANIMFILTER_FOREDIT        = (1<<8),
	/* only selected animchannels should be considerable as editable - mainly for Graph Editor's option for keys on select curves only */
	ANIMFILTER_SELEDIT        = (1<<9), 
	
	/* flags used to enforce certain data types */
	// NOTE: the ones for curves and NLA tracks were redundant and have been removed for now...
	ANIMFILTER_ANIMDATA       = (1<<10),
	
	/* duplicate entries for animation data attached to multi-user blocks must not occur */
	ANIMFILTER_NODUPLIS       = (1<<11),
	
	/* for checking if we should keep some collapsed channel around (internal use only!) */
	ANIMFILTER_TMP_PEEK       = (1<<30)
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
	/* 'Sub-object' channels (flags stored in Data block) */
#define FILTER_SKE_OBJD(key) ((key->flag & KEY_DS_EXPAND))
#define FILTER_MAT_OBJD(ma) ((ma->flag & MA_DS_EXPAND))
#define FILTER_LAM_OBJD(la) ((la->flag & LA_DS_EXPAND))
#define FILTER_CAM_OBJD(ca) ((ca->flag & CAM_DS_EXPAND))
#define FILTER_CUR_OBJD(cu) ((cu->flag & CU_DS_EXPAND))
#define FILTER_PART_OBJD(part) ((part->flag & PART_DS_EXPAND))
#define FILTER_MBALL_OBJD(mb) ((mb->flag2 & MB_DS_EXPAND))
#define FILTER_ARM_OBJD(arm) ((arm->flag & ARM_DS_EXPAND))
#define FILTER_MESH_OBJD(me) ((me->flag & ME_DS_EXPAND))
#define FILTER_LATTICE_OBJD(lt) ((lt->flag & LT_DS_EXPAND))
#define FILTER_SPK_OBJD(spk) ((spk->flag & SPK_DS_EXPAND))
	/* Variable use expanders */
#define FILTER_NTREE_DATA(ntree) ((ntree->flag & NTREE_DS_EXPAND))
#define FILTER_TEX_DATA(tex) ((tex->flag & TEX_DS_EXPAND))

	/* 'Sub-object/Action' channels (flags stored in Action) */
#define SEL_ACTC(actc) ((actc->flag & ACT_SELECTED))
#define EXPANDED_ACTC(actc) ((actc->flag & ACT_COLLAPSED)==0)
	/* 'Sub-AnimData' channels */
#define EXPANDED_DRVD(adt) ((adt->flag & ADT_DRIVERS_COLLAPSED)==0)


/* Actions (also used for Dopesheet) */
	/* Action Channel Group */
#define EDITABLE_AGRP(agrp) ((agrp->flag & AGRP_PROTECTED)==0)
#define EXPANDED_AGRP(ac, agrp) \
	( ((!(ac) || ((ac)->spacetype != SPACE_IPO)) && (agrp->flag & AGRP_EXPANDED)) || \
	  (( (ac) && ((ac)->spacetype == SPACE_IPO)) && (agrp->flag & AGRP_EXPANDED_G)) )
#define SEL_AGRP(agrp) ((agrp->flag & AGRP_SELECTED) || (agrp->flag & AGRP_ACTIVE))
	/* F-Curve Channels */
#define EDITABLE_FCU(fcu) ((fcu->flag & FCURVE_PROTECTED)==0)
#define SEL_FCU(fcu) (fcu->flag & FCURVE_SELECTED)

/* ShapeKey mode only */
#define EDITABLE_SHAPEKEY(kb) ((kb->flag & KEYBLOCK_LOCKED)==0)
#define SEL_SHAPEKEY(kb) (kb->flag & KEYBLOCK_SEL)

/* Grease Pencil only */
	/* Grease Pencil datablock settings */
#define EXPANDED_GPD(gpd) (gpd->flag & GP_DATA_EXPAND) 
	/* Grease Pencil Layer settings */
#define EDITABLE_GPL(gpl) ((gpl->flag & GP_LAYER_LOCKED)==0)
#define SEL_GPL(gpl) (gpl->flag & GP_LAYER_SELECT)

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


/* -------------- NLA Channel Defines -------------- */

/* NLA channel heights */
// XXX: NLACHANNEL_FIRST isn't used?
#define NLACHANNEL_FIRST			    -16
#define	NLACHANNEL_HEIGHT(snla)	        ((snla && (snla->flag & SNLA_NOSTRIPCURVES)) ? 16 : 24)
#define NLACHANNEL_HEIGHT_HALF(snla)	((snla && (snla->flag & SNLA_NOSTRIPCURVES)) ?  8 : 12)
#define	NLACHANNEL_SKIP	                2
#define NLACHANNEL_STEP(snla)           (NLACHANNEL_HEIGHT(snla) + NLACHANNEL_SKIP)

/* channel widths */
#define NLACHANNEL_NAMEWIDTH		200

/* channel toggle-buttons */
#define NLACHANNEL_BUTTON_WIDTH	16

/* ---------------- API  -------------------- */

/* Obtain list of filtered Animation channels to operate on.
 * Returns the number of channels in the list
 */
size_t ANIM_animdata_filter(bAnimContext *ac, ListBase *anim_data, int filter_mode, void *data, short datatype);

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
/* anim_channels_*.c */

/* ------------------------ Drawing TypeInfo -------------------------- */

/* flag-setting behavior */
typedef enum eAnimChannels_SetFlag {
	ACHANNEL_SETFLAG_CLEAR = 0,		/* turn off */
	ACHANNEL_SETFLAG_ADD,			/* turn on */
	ACHANNEL_SETFLAG_INVERT,		/* on->off, off->on */
	ACHANNEL_SETFLAG_TOGGLE			/* some on -> all off // all on */
} eAnimChannels_SetFlag;

/* types of settings for AnimChannels */
typedef enum eAnimChannel_Settings {
	ACHANNEL_SETTING_SELECT = 0,
	ACHANNEL_SETTING_PROTECT,			// warning: for drawing UI's, need to check if this is off (maybe inverse this later)
	ACHANNEL_SETTING_MUTE,
	ACHANNEL_SETTING_EXPAND,
	ACHANNEL_SETTING_VISIBLE,			/* only for Graph Editor */
	ACHANNEL_SETTING_SOLO				/* only for NLA Tracks */
} eAnimChannel_Settings;


/* Drawing, mouse handling, and flag setting behavior... */
typedef struct bAnimChannelType {
	/* type data */
		/* name of the channel type, for debugging */
	const char *channel_type_name;
	
	/* drawing */
		/* get RGB color that is used to draw the majority of the backdrop */
	void (*get_backdrop_color)(bAnimContext *ac, bAnimListElem *ale, float r_color[3]);
		/* draw backdrop strip for channel */
	void (*draw_backdrop)(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc);
		/* get depth of indention (relative to the depth channel is nested at) */
	short (*get_indent_level)(bAnimContext *ac, bAnimListElem *ale);
		/* get offset in pixels for the start of the channel (in addition to the indent depth) */
	short (*get_offset)(bAnimContext *ac, bAnimListElem *ale);
	
	/* get name (for channel lists) */
	void (*name)(bAnimListElem *ale, char *name);
	/* get RNA property+pointer for editing the name */
	short (*name_prop)(bAnimListElem *ale, struct PointerRNA *ptr, struct PropertyRNA **prop);
	/* get icon (for channel lists) */
	int (*icon)(bAnimListElem *ale);
	
	/* settings */
		/* check if the given setting is valid in the current context */
	short (*has_setting)(bAnimContext *ac, bAnimListElem *ale, int setting);
		/* get the flag used for this setting */
	int (*setting_flag)(bAnimContext *ac, int setting, short *neg);
		/* get the pointer to int/short where data is stored, 
		 * with type being  sizeof(ptr_data) which should be fine for runtime use...
		 *	- assume that setting has been checked to be valid for current context
		 */
	void *(*setting_ptr)(bAnimListElem *ale, int setting, short *type);
} bAnimChannelType;

/* ------------------------ Drawing API -------------------------- */

/* Get typeinfo for the given channel */
bAnimChannelType *ANIM_channel_get_typeinfo(bAnimListElem *ale);

/* Print debugging info about a given channel */
void ANIM_channel_debug_print_info(bAnimListElem *ale, short indent_level);

/* Draw the given channel */
void ANIM_channel_draw(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc);
/* Draw the widgets for the given channel */
void ANIM_channel_draw_widgets(struct bContext *C, bAnimContext *ac, bAnimListElem *ale, struct uiBlock *block, float yminc, float ymaxc, size_t channel_index);


/* ------------------------ Editing API -------------------------- */

/* Check if some setting for a channel is enabled 
 * Returns: 1 = On, 0 = Off, -1 = Invalid
 *
 * 	- setting: eAnimChannel_Settings
 */
short ANIM_channel_setting_get(bAnimContext *ac, bAnimListElem *ale, int setting);

/* Change value of some setting for a channel 
 *	- setting: eAnimChannel_Settings
 *	- mode: eAnimChannels_SetFlag
 */
void ANIM_channel_setting_set(bAnimContext *ac, bAnimListElem *ale, int setting, short mode);


/* Flush visibility (for Graph Editor) changes up/down hierarchy for changes in the given setting 
 *	- anim_data: list of the all the anim channels that can be chosen
 *		-> filtered using ANIMFILTER_CHANNELS only, since if we took VISIBLE too,
 *		  then the channels under closed expanders get ignored...
 *	- ale_setting: the anim channel (not in the anim_data list directly, though occurring there)
 *		with the new state of the setting that we want flushed up/down the hierarchy 
 *	- setting: type of setting to set
 *	- on: whether the visibility setting has been enabled or disabled 
 */
void ANIM_flush_setting_anim_channels(bAnimContext *ac, ListBase *anim_data, bAnimListElem *ale_setting, int setting, short on);


/* Deselect all animation channels */
void ANIM_deselect_anim_channels(bAnimContext *ac, void *data, short datatype, short test, short sel);

/* Set the 'active' channel of type channel_type, in the given action */
void ANIM_set_active_channel(bAnimContext *ac, void *data, short datatype, int filter, void *channel_data, short channel_type);


/* Delete the F-Curve from the given AnimData block (if possible), as appropriate according to animation context */
void ANIM_fcurve_delete_from_animdata(bAnimContext *ac, struct AnimData *adt, struct FCurve *fcu);

/* ************************************************ */
/* DRAWING API */
/* anim_draw.c */

/* Get string representing the given frame number as an appropriately represented frame or timecode */
void ANIM_timecode_string_from_frame(char *str, struct Scene *scene, int power, short timecodes, float cfra);

/* ---------- Current Frame Drawing ---------------- */

/* flags for Current Frame Drawing */
enum {
		/* plain time indicator with no special indicators */
	DRAWCFRA_PLAIN			= 0,
		/* draw box indicating current frame number */
	DRAWCFRA_SHOW_NUMBOX	= (1<<0),
		/* time indication in seconds or frames */
	DRAWCFRA_UNIT_SECONDS 	= (1<<1),
		/* draw indicator extra wide (for timeline) */
	DRAWCFRA_WIDE			= (1<<2)
} eAnimEditDraw_CurrentFrame; 

/* main call to draw current-frame indicator in an Animation Editor */
void ANIM_draw_cfra(const struct bContext *C, struct View2D *v2d, short flag);

/* ------------- Preview Range Drawing -------------- */

/* main call to draw preview range curtains */
void ANIM_draw_previewrange(const struct bContext *C, struct View2D *v2d);

/* ************************************************* */
/* F-MODIFIER TOOLS */

/* ------------- UI Panel Drawing -------------- */

/* draw a given F-Modifier for some layout/UI-Block */
void ANIM_uiTemplate_fmodifier_draw(struct uiLayout *layout, struct ID *id, ListBase *modifiers, struct FModifier *fcm);

/* ------------- Copy/Paste Buffer -------------- */


/* free the copy/paste buffer */
void free_fmodifiers_copybuf(void);

/* copy the given F-Modifiers to the buffer, returning whether anything was copied or not
 * assuming that the buffer has been cleared already with free_fmodifiers_copybuf()
 *	- active: only copy the active modifier
 */
short ANIM_fmodifiers_copy_to_buf(ListBase *modifiers, short active);

/* 'Paste' the F-Modifier(s) from the buffer to the specified list 
 *	- replace: free all the existing modifiers to leave only the pasted ones 
 */
short ANIM_fmodifiers_paste_from_buf(ListBase *modifiers, short replace);

/* ************************************************* */
/* ASSORTED TOOLS */

/* ------------ Animation F-Curves <-> Icons/Names Mapping ------------ */
/* anim_ipo_utils.c */

/* Get icon + name for channel-list displays for F-Curve */
int getname_anim_fcurve(char *name, struct ID *id, struct FCurve *fcu);

/* Automatically determine a color for the nth F-Curve */
void getcolor_fcurve_rainbow(int cur, int tot, float *out);

/* ----------------- NLA-Mapping ----------------------- */
/* anim_draw.c */

/* Obtain the AnimData block providing NLA-scaling for the given channel if applicable */
struct AnimData *ANIM_nla_mapping_get(bAnimContext *ac, bAnimListElem *ale);

/* Apply/Unapply NLA mapping to all keyframes in the nominated F-Curve */
void ANIM_nla_mapping_apply_fcurve(struct AnimData *adt, struct FCurve *fcu, short restore, short only_keys);

/* ..... */

/* Perform auto-blending/extend refreshes after some operations */
// NOTE: defined in space_nla/nla_edit.c, not in animation/
void ED_nla_postop_refresh(bAnimContext *ac);

/* ------------- Unit Conversion Mappings ------------- */
/* anim_draw.c */

/* flags for conversion mapping */
typedef enum eAnimUnitConv_Flags {
		/* restore to original internal values */
	ANIM_UNITCONV_RESTORE	= (1<<0),
		/* ignore handles (i.e. only touch main keyframes) */
	ANIM_UNITCONV_ONLYKEYS	= (1<<1),
		/* only touch selected BezTriples */
	ANIM_UNITCONV_ONLYSEL	= (1<<2),
		/* only touch selected vertices */
	ANIM_UNITCONV_SELVERTS	= (1<<3)
} eAnimUnitConv_Flags;

/* Get unit conversion factor for given ID + F-Curve */
float ANIM_unit_mapping_get_factor(struct Scene *scene, struct ID *id, struct FCurve *fcu, short restore);

/* Apply/Unapply units conversions to keyframes */
void ANIM_unit_mapping_apply_fcurve(struct Scene *scene, struct ID *id, struct FCurve *fcu, short flag);

/* ------------- Utility macros ----------------------- */

/* provide access to Keyframe Type info in BezTriple
 * NOTE: this is so that we can change it from being stored in 'hide'
 */
#define BEZKEYTYPE(bezt) ((bezt)->hide)

/* set/clear/toggle macro 
 *	- channel - channel with a 'flag' member that we're setting
 *	- smode - 0=clear, 1=set, 2=invert
 *	- sflag - bitflag to set
 */
#define ACHANNEL_SET_FLAG(channel, smode, sflag) \
	{ \
		if (smode == ACHANNEL_SETFLAG_INVERT) 	(channel)->flag ^= (sflag); \
		else if (smode == ACHANNEL_SETFLAG_ADD) (channel)->flag |= (sflag); \
		else 									(channel)->flag &= ~(sflag); \
	}
	
/* set/clear/toggle macro, where the flag is negative 
 *	- channel - channel with a 'flag' member that we're setting
 *	- smode - 0=clear, 1=set, 2=invert
 *	- sflag - bitflag to set
 */
#define ACHANNEL_SET_FLAG_NEG(channel, smode, sflag) \
	{ \
		if (smode == ACHANNEL_SETFLAG_INVERT) 	(channel)->flag ^= (sflag); \
		else if (smode == ACHANNEL_SETFLAG_ADD) (channel)->flag &= ~(sflag); \
		else 									(channel)->flag |= (sflag); \
	}


/* --------- anim_deps.c, animation updates -------- */

void ANIM_id_update(struct Scene *scene, struct ID *id);
void ANIM_list_elem_update(struct Scene *scene, bAnimListElem *ale);

/* data -> channels syncing */
void ANIM_sync_animchannels_to_data(const struct bContext *C);

/* ************************************************* */
/* OPERATORS */
	
	/* generic animation channels */
void ED_operatortypes_animchannels(void);
void ED_keymap_animchannels(struct wmKeyConfig *keyconf);

	/* generic time editing */
void ED_operatortypes_anim(void);
void ED_keymap_anim(struct wmKeyConfig *keyconf);
	
	/* space_graph */
void ED_operatormacros_graph(void);
	/* space_action */
void ED_operatormacros_action(void);

/* ************************************************ */

#endif /* __ED_ANIM_API_H__ */

