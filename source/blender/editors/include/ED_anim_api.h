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
struct ListBase;
struct bContext;
struct View2D;
struct bActionGroup;

/* ************************************************ */
/* ANIMATION CHANNEL FILTERING */

/* --------------- Data Types -------------------- */

/* This struct defines a structure used for quick and uniform access for 
 * channels of animation data
 */
typedef struct bAnimListElem {
	struct bAnimListElem *next, *prev;
	
	void 	*data;		/* source data this elem represents */
	int 	type;		/* one of the ANIMTYPE_* values */
	int		flag;		/* copy of elem's flags for quick access */
	int 	index;		/* copy of adrcode where applicable */
	
	void	*key_data;	/* motion data - ipo or ipo-curve */
	short	datatype;	/* type of motion data to expect */
	
	struct ID *id;				/* ID block (ID_SC, ID_SCE, or ID_OB) that owns the channel */
	struct bActionGroup *grp;	/* action group that owns the channel (only for Action/Dopesheet) */
	
	void 	*owner;		/* will either be an action channel or fake ipo-channel (for keys) */
	short	ownertype;	/* type of owner */
} bAnimListElem;


/* Some types for easier type-testing */
// XXX was ACTTYPE_*
typedef enum eAnim_ChannelType {
	ANIMTYPE_NONE= 0,
	ANIMTYPE_SPECIALDATA,
	
	ANIMTYPE_OBJECT,
	ANIMTYPE_GROUP,
	
	ANIMTYPE_FILLIPO,
	ANIMTYPE_FILLCON,
	
	ANIMTYPE_FILLACTD,
	ANIMTYPE_FILLIPOD,
	ANIMTYPE_FILLCOND,
	ANIMTYPE_FILLMATD,
	
	ANIMTYPE_DSMAT,
	ANIMTYPE_DSLAM,
	ANIMTYPE_DSCAM,
	ANIMTYPE_DSCUR,
	ANIMTYPE_DSSKEY,
	
	ANIMTYPE_ACHAN,
	ANIMTYPE_CONCHAN,
	ANIMTYPE_CONCHAN2,
	ANIMTYPE_ICU,
	ANIMTYPE_IPO,
	
	ANIMTYPE_SHAPEKEY,
	ANIMTYPE_GPDATABLOCK,
	ANIMTYPE_GPLAYER,
} eAnim_ChannelType;

/* types of keyframe data in bAnimListElem */
typedef enum eAnim_KeyType {
	ALE_NONE = 0,		/* no keyframe data */
	ALE_IPO,			/* IPO block */
	ALE_ICU,			/* IPO-Curve block */
	ALE_GPFRAME,		/* Grease Pencil Frames */
	
	// XXX the following are for summaries... should these be kept?
	ALE_OB,				/* Object summary */
	ALE_ACT,			/* Action summary */
	ALE_GROUP,			/* Action Group summary */
} eAnim_KeyType;

/* Main Data container types */
// XXX was ACTCONT_*
typedef enum eAnimCont_Types {
	ANIMCONT_NONE = 0,		/* invalid or no data */
	ANIMCONT_ACTION,		/* action (bAction) */
	ANIMCONT_SHAPEKEY,		/* shapekey (Key) */
	ANIMCONT_GPENCIL,		/* grease pencil (screen) */
	ANIMCONT_DOPESHEET,		/* dopesheet (bDopesheet) */
} eAnimCont_Types;

/* filtering flags  - under what circumstances should a channel be added */
// XXX was ACTFILTER_*
typedef enum eAnimFilter_Flags {
	ALEFILTER_VISIBLE		= (1<<0),	/* should channels be visible */
	ALEFILTER_SEL			= (1<<1),	/* should channels be selected */
	ALEFILTER_FOREDIT		= (1<<2),	/* does editable status matter */
	ALEFILTER_CHANNELS		= (1<<3),	/* do we only care that it is a channel */
	ALEFILTER_IPOKEYS		= (1<<4),	/* only channels referencing ipo's */
	ALEFILTER_ONLYICU		= (1<<5),	/* only reference ipo-curves */
	ALEFILTER_FORDRAWING	= (1<<6),	/* make list for interface drawing */
	ALEFILTER_ACTGROUPED	= (1<<7),	/* belongs to the active actiongroup */
} eAnimFilter_Flags;


/* ---------------- API  -------------------- */

/* Obtain list of filtered Animation channels to operate on */
void ANIM_animdata_filter(struct ListBase *anim_data, int filter_mode, void *data, short datatype);

/* Obtain current anim-data context from Blender Context info */
void *ANIM_animdata_get_context(const struct bContext *C, short *datatype);

/* ************************************************ */
/* DRAWING API */
// XXX should this get its own header file?

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
void ANIM_draw_cfra(const bContext *C, struct View2D *v2d, short flag);

/* ------------- Preview Range Drawing -------------- */

// XXX should preview range get its own file?

/* main call to draw preview range curtains */
void ANIM_draw_previewrange(const bContext *C, struct View2D *v2d);

/* ************************************************* */

#endif /* ED_ANIM_API_H */

