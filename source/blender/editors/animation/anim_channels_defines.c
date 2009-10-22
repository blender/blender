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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_listBase.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_userdef_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h" // XXX move the select modes out of there!
#include "ED_screen.h"
#include "ED_space_api.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

/* *********************************************** */
// XXX constant defines to be moved elsewhere?

/* extra padding for lengths (to go under scrollers) */
#define EXTRA_SCROLL_PAD	100.0f

/* size of indent steps */
#define INDENT_STEP_SIZE 	7

/* macros used for type defines */
	/* get the pointer used for some flag */
#define GET_ACF_FLAG_PTR(ptr) \
	{ \
		*type= sizeof((ptr)); \
		return &(ptr); \
	} 


/* XXX */
extern void gl_round_box(int mode, float minx, float miny, float maxx, float maxy, float rad);
extern void gl_round_box_shade(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadetop, float shadedown);


/* *********************************************** */
/* Generic Functions (Type independent) */

/* Draw Backdrop ---------------------------------- */

/* backdrop for top-level widgets (Scene and Object only) */
static void acf_generic_root_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	short expanded= ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_EXPAND) != 0;
	short offset= (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
	
	/* darker blue for top-level widgets */
	UI_ThemeColor(TH_DOPESHEET_CHANNELOB);
	
	/* rounded corners on LHS only - top only when expanded, but bottom too when collapsed */
	uiSetRoundBox((expanded)? (1):(1|8));
	gl_round_box(GL_POLYGON, offset,  yminc, v2d->cur.xmax+EXTRA_SCROLL_PAD, ymaxc, 8);
}

/* backdrop for data expanders under top-level Scene/Object */
static void acf_generic_dataexpand_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	short offset= (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
	
	/* lighter color than top-level widget */
	UI_ThemeColor(TH_DOPESHEET_CHANNELSUBOB);
	
	/* no rounded corner - just rectangular box */
	glRectf(offset, yminc, 	v2d->cur.xmax+EXTRA_SCROLL_PAD, ymaxc);
}

/* backdrop for generic channels */
static void acf_generic_channel_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	SpaceAction *saction = NULL;
	bActionGroup *grp = NULL;
	short indent= (acf->get_indent_level) ? acf->get_indent_level(ac, ale) : 0;
	short offset= (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
	
	/* get context info needed... */
	if ((ac->sa) && (ac->sa->spacetype == SPACE_ACTION))
		saction= (SpaceAction *)ac->sa->spacedata.first;
		
	if (ale->type == ANIMTYPE_FCURVE) {
		FCurve *fcu= (FCurve *)ale->data;
		grp= fcu->grp;
	}
	
	/* set color for normal channels 
	 *	- use 3 shades of color group/standard color for 3 indention level
	 *	- only use group colors if allowed to, and if actually feasible
	 */
	if ( (saction && !(saction->flag & SACTION_NODRAWGCOLORS)) && 
		 ((grp) && (grp->customCol)) ) 
	{
		char cp[3];
		
		if (indent == 2) {
			VECCOPY(cp, grp->cs.solid);
		}
		else if (indent == 1) {
			VECCOPY(cp, grp->cs.select);
		}
		else {
			VECCOPY(cp, grp->cs.active);
		}
		
		glColor3ub(cp[0], cp[1], cp[2]);
	}
	else // FIXME: what happens when the indention is 1 greater than what it should be (due to grouping)?
		UI_ThemeColorShade(TH_HEADER, ((indent==0)?20: (indent==1)?-20: -40));
	
	/* no rounded corners - just rectangular box */
	glRectf(offset, yminc, 	v2d->cur.xmax+EXTRA_SCROLL_PAD, ymaxc);
}

/* Indention + Offset ------------------------------------------- */

/* indention level is always the value in the name */
static short acf_generic_indention_0(bAnimContext *ac, bAnimListElem *ale)
{
	return 0;
}
static short acf_generic_indention_1(bAnimContext *ac, bAnimListElem *ale)
{
	return 1;
}
#if 0 // XXX not used
static short acf_generic_indention_2(bAnimContext *ac, bAnimListElem *ale)
{
	return 2;
}
#endif

/* indention which varies with the grouping status */
static short acf_generic_indention_flexible(bAnimContext *ac, bAnimListElem *ale)
{
	short indent= 0;
	
	if (ale->id) {
		/* special exception for materials and particles */
		if (ELEM(GS(ale->id->name),ID_MA,ID_PA))
			indent++;
	}
	
	/* grouped F-Curves need extra level of indention */
	if (ale->type == ANIMTYPE_FCURVE) {
		FCurve *fcu= (FCurve *)ale->data;
		
		// TODO: we need some way of specifying that the indention color should be one less...
		if (fcu->grp)
			indent++;
	}
	
	/* no indention */
	return indent;
}

/* basic offset for channels derived from indention */
static short acf_generic_basic_offset(bAnimContext *ac, bAnimListElem *ale)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	
	if (acf && acf->get_indent_level)
		return acf->get_indent_level(ac, ale) * INDENT_STEP_SIZE;
	else
		return 0;
}

/* offset for groups + grouped entities */
static short acf_generic_group_offset(bAnimContext *ac, bAnimListElem *ale)
{
	short offset= acf_generic_basic_offset(ac, ale);
	
	if (ale->id) {
		/* special exception for materials and particles */
		if (ELEM(GS(ale->id->name),ID_MA,ID_PA)) 
			offset += 21;
			
		/* if not in Action Editor mode, groupings must carry some offset too... */
		else if (ac->datatype != ANIMCONT_ACTION)
			offset += 14;
	}
	
	/* offset is just the normal type - i.e. based on indention */
	return offset;
}

/* Name ------------------------------------------- */

/* name for ID block entries */
static void acf_generic_idblock_name(bAnimListElem *ale, char *name)
{
	ID *id= (ID *)ale->data;	/* data pointed to should be an ID block */
	
	/* just copy the name... */
	if (id && name)
		strcpy(name, id->name+2);
}

/* Settings ------------------------------------------- */

#if 0
/* channel type has no settings */
static short acf_generic_none_setting_valid(bAnimContext *ac, bAnimListElem *ale, int setting)
{
	return 0;
}
#endif

/* check if some setting exists for this object-based data-expander (category only) */
static short acf_generic_dsexpand_setting_valid(bAnimContext *ac, bAnimListElem *ale, int setting)
{
	switch (setting) {
		/* only expand supported */
		case ACHANNEL_SETTING_EXPAND:
			return 1;
			
		 /* visible - only available in Graph Editor */
		case ACHANNEL_SETTING_VISIBLE: 
			return ((ac) && (ac->spacetype == SPACE_IPO));
			
		default:
			return 0;
	}
}

/* get pointer to the setting (category only) */
static void *acf_generic_dsexpand_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Object *ob= (Object *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(ob->nlaflag); // XXX
		
		default: /* unsupported */
			return NULL;
	}
}

/* check if some setting exists for this object-based data-expander (datablock only) */
static short acf_generic_dataexpand_setting_valid(bAnimContext *ac, bAnimListElem *ale, int setting)
{
	switch (setting) {
		/* expand is always supported */
		case ACHANNEL_SETTING_EXPAND:
			return 1;
			
		/* mute is only supported for NLA */
		case ACHANNEL_SETTING_MUTE:
			return ((ac) && (ac->spacetype == SPACE_NLA));
			
		/* other flags are never supported */
		default:
			return 0;
	}
}

/* *********************************************** */
/* Type Specific Functions + Defines */

/* Animation Summary ----------------------------------- */

/* backdrop for summary widget */
static void acf_summary_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	View2D *v2d= &ac->ar->v2d;
	
	// FIXME: hardcoded color - same as the 'action' line in NLA
	glColor3f(0.8f, 0.2f, 0.0f);	// reddish color 
	
	/* rounded corners on LHS only 
	 *	- top and bottom 
	 *	- special hack: make the top a bit higher, since we are first... 
	 */
	uiSetRoundBox((1|8));
	gl_round_box(GL_POLYGON, 0,  yminc-2, v2d->cur.xmax+EXTRA_SCROLL_PAD, ymaxc, 8);
}

/* name for summary entries */
static void acf_summary_name(bAnimListElem *ale, char *name)
{
	if (name)
		strcpy(name, "DopeSheet Summary");
}

// TODO: this is really a temp icon I think
static int acf_summary_icon(bAnimListElem *ale)
{
	return ICON_BORDERMOVE;
}

/* check if some setting exists for this channel */
static short acf_summary_setting_valid(bAnimContext *ac, bAnimListElem *ale, int setting)
{
	/* only expanded is supported, as it is used for hiding all stuff which the summary covers */
	return (setting == ACHANNEL_SETTING_EXPAND);
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_summary_setting_flag(int setting, short *neg)
{
	if (setting == ACHANNEL_SETTING_EXPAND) {
		/* expanded */
		*neg= 1;
		return ADS_FLAG_SUMMARY_COLLAPSED;
	}
	else {
		/* unsupported */
		*neg= 0;
		return 0;
	}
}

/* get pointer to the setting */
static void *acf_summary_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	bAnimContext *ac= (bAnimContext *)ale->data;
	
	/* if data is valid, return pointer to active dopesheet's relevant flag 
	 *	- this is restricted to DopeSheet/Action Editor only
	 */
	if ((ac->sa) && (ac->spacetype == SPACE_ACTION) && (setting == ACHANNEL_SETTING_EXPAND)) {
		SpaceAction *saction= (SpaceAction *)ac->sa->spacedata.first;
		bDopeSheet *ads= &saction->ads;
		
		/* return pointer to DopeSheet's flag */
		GET_ACF_FLAG_PTR(ads->flag);
	}
	else {
		/* can't return anything useful - unsupported */
		*type= 0;
		return 0;
	}
}

/* all animation summary (DopeSheet only) type define */
static bAnimChannelType ACF_SUMMARY = 
{
	acf_summary_backdrop,				/* backdrop */
	acf_generic_indention_0,			/* indent level */
	NULL,								/* offset */
	
	acf_summary_name,					/* name */
	acf_summary_icon,					/* icon */
	
	acf_summary_setting_valid,			/* has setting */
	acf_summary_setting_flag,			/* flag for setting */
	acf_summary_setting_ptr				/* pointer for setting */
};

/* Scene ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_scene_icon(bAnimListElem *ale)
{
	return ICON_SCENE_DATA;
}

/* check if some setting exists for this channel */
static short acf_scene_setting_valid(bAnimContext *ac, bAnimListElem *ale, int setting)
{
	switch (setting) {
		/* muted only in NLA */
		case ACHANNEL_SETTING_MUTE: 
			return ((ac) && (ac->spacetype == SPACE_NLA));
			
		/* visible only in Graph Editor */
		case ACHANNEL_SETTING_VISIBLE: 
			return ((ac) && (ac->spacetype == SPACE_IPO));
		
		/* only select and expand supported otherwise */
		case ACHANNEL_SETTING_SELECT:
		case ACHANNEL_SETTING_EXPAND:
			return 1;
			
		default:
			return 0;
	}
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_scene_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			return SCE_DS_SELECTED;
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			*neg= 1;
			return SCE_DS_COLLAPSED;
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
			return ADT_NLA_EVAL_OFF;
			
		case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
			*neg= 1;
			return ADT_CURVES_NOT_VISIBLE;
			
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_scene_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Scene *scene= (Scene *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			GET_ACF_FLAG_PTR(scene->flag);
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(scene->flag);
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (scene->adt)
				GET_ACF_FLAG_PTR(scene->adt->flag)
			else
				return NULL;
			
		default: /* unsupported */
			return 0;
	}
}

/* scene type define */
static bAnimChannelType ACF_SCENE = 
{
	acf_generic_root_backdrop,		/* backdrop */
	acf_generic_indention_0,		/* indent level */
	NULL,							/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_scene_icon,					/* icon */
	
	acf_scene_setting_valid,		/* has setting */
	acf_scene_setting_flag,			/* flag for setting */
	acf_scene_setting_ptr			/* pointer for setting */
};

/* Object ------------------------------------------- */

static int acf_object_icon(bAnimListElem *ale)
{
	Base *base= (Base *)ale->data;
	Object *ob= base->object;
	
	/* icon depends on object-type */
	if (ob->type == OB_ARMATURE)
		return ICON_ARMATURE_DATA;
	else	
		return ICON_OBJECT_DATA;
}

/* name for object */
static void acf_object_name(bAnimListElem *ale, char *name)
{
	Base *base= (Base *)ale->data;
	Object *ob= base->object;
	
	/* just copy the name... */
	if (ob && name)
		strcpy(name, ob->id.name+2);
}

/* check if some setting exists for this channel */
static short acf_object_setting_valid(bAnimContext *ac, bAnimListElem *ale, int setting)
{
	Base *base= (Base *)ale->data;
	Object *ob= base->object;
	
	switch (setting) {
		/* muted only in NLA */
		case ACHANNEL_SETTING_MUTE: 
			return ((ac) && (ac->spacetype == SPACE_NLA));
			
		/* visible only in Graph Editor */
		case ACHANNEL_SETTING_VISIBLE: 
			return ((ac) && (ac->spacetype == SPACE_IPO) && (ob->adt));
		
		/* only select and expand supported otherwise */
		case ACHANNEL_SETTING_SELECT:
		case ACHANNEL_SETTING_EXPAND:
			return 1;
			
		default:
			return 0;
	}
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_object_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			return SELECT;
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			*neg= 1;
			return OB_ADS_COLLAPSED;
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
			return ADT_NLA_EVAL_OFF;
			
		case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
			*neg= 1;
			return ADT_CURVES_NOT_VISIBLE;
			
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_object_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Base *base= (Base *)ale->data;
	Object *ob= base->object;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			GET_ACF_FLAG_PTR(ob->flag);
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(ob->nlaflag); // xxx
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (ob->adt)
				GET_ACF_FLAG_PTR(ob->adt->flag)
			else
				return NULL;
			
		default: /* unsupported */
			return 0;
	}
}

/* object type define */
static bAnimChannelType ACF_OBJECT = 
{
	acf_generic_root_backdrop,		/* backdrop */
	acf_generic_indention_0,		/* indent level */
	NULL,							/* offset */
	
	acf_object_name,				/* name */
	acf_object_icon,				/* icon */
	
	acf_object_setting_valid,		/* has setting */
	acf_object_setting_flag,		/* flag for setting */
	acf_object_setting_ptr			/* pointer for setting */
};

/* Group ------------------------------------------- */

/* backdrop for group widget */
static void acf_group_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	short expanded= ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_EXPAND) != 0;
	short offset= (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
	
	/* only for action group channels */
	if (ale->flag & AGRP_ACTIVE)
		UI_ThemeColorShade(TH_GROUP_ACTIVE, 10);
	else
		UI_ThemeColorShade(TH_GROUP, 20);
	
	/* rounded corners on LHS only - top only when expanded, but bottom too when collapsed */
	uiSetRoundBox((expanded)? (1):(1|8));
	gl_round_box(GL_POLYGON, offset,  yminc, v2d->cur.xmax+EXTRA_SCROLL_PAD, ymaxc, 8);
}

/* name for group entries */
static void acf_group_name(bAnimListElem *ale, char *name)
{
	bActionGroup *agrp= (bActionGroup *)ale->data;
	
	/* just copy the name... */
	if (agrp && name)
		strcpy(name, agrp->name);
}

/* check if some setting exists for this channel */
static short acf_group_setting_valid(bAnimContext *ac, bAnimListElem *ale, int setting)
{
	/* for now, all settings are supported, though some are only conditionally */
	switch (setting) {
		case ACHANNEL_SETTING_VISIBLE: /* Only available in Graph Editor */
			return ((ac->sa) && (ac->sa->spacetype==SPACE_IPO));
			
		default: /* always supported */
			return 1;
	}
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_group_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			return AGRP_SELECTED;
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return AGRP_EXPANDED;
			
		case ACHANNEL_SETTING_MUTE: /* muted */
			return AGRP_MUTED;
			
		case ACHANNEL_SETTING_PROTECT: /* protected */
			//*neg= 1; - if we change this to edtiability
			return AGRP_PROTECTED;
			
		case ACHANNEL_SETTING_VISIBLE: /* visiblity - graph editor */
			*neg= 1;
			return AGRP_NOTVISIBLE;
	}
	
	/* this shouldn't happen */
	return 0;
}

/* get pointer to the setting */
static void *acf_group_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	bActionGroup *agrp= (bActionGroup *)ale->data;
	
	/* all flags are just in agrp->flag for now... */
	GET_ACF_FLAG_PTR(agrp->flag);
}

/* group type define */
static bAnimChannelType ACF_GROUP = 
{
	acf_group_backdrop,				/* backdrop */
	acf_generic_indention_0,		/* indent level */
	acf_generic_group_offset,		/* offset */
	
	acf_group_name,					/* name */
	NULL,							/* icon */
	
	acf_group_setting_valid,		/* has setting */
	acf_group_setting_flag,			/* flag for setting */
	acf_group_setting_ptr			/* pointer for setting */
};

/* F-Curve ------------------------------------------- */

/* name for fcurve entries */
static void acf_fcurve_name(bAnimListElem *ale, char *name)
{
	getname_anim_fcurve(name, ale->id, ale->data);
}

/* check if some setting exists for this channel */
static short acf_fcurve_setting_valid(bAnimContext *ac, bAnimListElem *ale, int setting)
{
	FCurve *fcu= (FCurve *)ale->data;
	
	switch (setting) {
		/* unsupported */
		case ACHANNEL_SETTING_EXPAND: /* F-Curves are not containers */
			return 0;
		
		/* conditionally available */
		case ACHANNEL_SETTING_PROTECT: /* Protection is only valid when there's keyframes */
			if (fcu->bezt)
				return 1;
			else
				return 0; // NOTE: in this special case, we need to draw ICON_ZOOMOUT
				
		case ACHANNEL_SETTING_VISIBLE: /* Only available in Graph Editor */
			return ((ac->sa) && (ac->sa->spacetype==SPACE_IPO));
			
		/* always available */
		default:
			return 1;
	}
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_fcurve_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			return FCURVE_SELECTED;
			
		case ACHANNEL_SETTING_MUTE: /* muted */
			return FCURVE_MUTED;
			
		case ACHANNEL_SETTING_PROTECT: /* protected */
			//*neg= 1; - if we change this to edtiability
			return FCURVE_PROTECTED;
			
		case ACHANNEL_SETTING_VISIBLE: /* visiblity - graph editor */
			return FCURVE_VISIBLE;
			
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_fcurve_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	FCurve *fcu= (FCurve *)ale->data;
	
	/* all flags are just in agrp->flag for now... */
	GET_ACF_FLAG_PTR(fcu->flag);
}

/* fcurve type define */
static bAnimChannelType ACF_FCURVE = 
{
	acf_generic_channel_backdrop,	/* backdrop */
	acf_generic_indention_flexible,	/* indent level */		// xxx rename this to f-curves only?
	acf_generic_group_offset,		/* offset */
	
	acf_fcurve_name,				/* name */
	NULL,							/* icon */
	
	acf_fcurve_setting_valid,		/* has setting */
	acf_fcurve_setting_flag,		/* flag for setting */
	acf_fcurve_setting_ptr			/* pointer for setting */
};

/* Object Action Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_fillactd_icon(bAnimListElem *ale)
{
	return ICON_ACTION;
}

/* check if some setting exists for this channel */
static short acf_fillactd_setting_valid(bAnimContext *ac, bAnimListElem *ale, int setting)
{
	switch (setting) {
		/* only select and expand supported */
		case ACHANNEL_SETTING_SELECT:
		case ACHANNEL_SETTING_EXPAND:
			return 1;
			
		default:
			return 0;
	}
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_fillactd_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			return ADT_UI_SELECTED;
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			*neg= 1;
			return ACT_COLLAPSED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_fillactd_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	bAction *act= (bAction *)ale->data;
	AnimData *adt= ale->adt;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			if (adt) {
				GET_ACF_FLAG_PTR(adt->flag);
			}
			else
				return 0;
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(act->flag);
		
		default: /* unsupported */
			return 0;
	}
}

/* object action expander type define */
static bAnimChannelType ACF_FILLACTD = 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_fillactd_icon,				/* icon */
	
	acf_fillactd_setting_valid,		/* has setting */
	acf_fillactd_setting_flag,		/* flag for setting */
	acf_fillactd_setting_ptr		/* pointer for setting */
};

/* Drivers Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_filldrivers_icon(bAnimListElem *ale)
{
	return ICON_ANIM_DATA;
}

static void acf_filldrivers_name(bAnimListElem *ale, char *name)
{
	strcpy(name, "Drivers");
}

/* check if some setting exists for this channel */
// TODO: this could be made more generic
static short acf_filldrivers_setting_valid(bAnimContext *ac, bAnimListElem *ale, int setting)
{
	switch (setting) {
		/* only expand supported */
		case ACHANNEL_SETTING_EXPAND:
			return 1;
			
		default:
			return 0;
	}
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_filldrivers_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			*neg= 1;
			return ADT_DRIVERS_COLLAPSED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_filldrivers_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	AnimData *adt= (AnimData *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(adt->flag);
		
		default: /* unsupported */
			return 0;
	}
}

/* drivers expander type define */
static bAnimChannelType ACF_FILLDRIVERS = 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_filldrivers_name,			/* name */
	acf_filldrivers_icon,			/* icon */
	
	acf_filldrivers_setting_valid,	/* has setting */
	acf_filldrivers_setting_flag,	/* flag for setting */
	acf_filldrivers_setting_ptr		/* pointer for setting */
};

/* Materials Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_fillmatd_icon(bAnimListElem *ale)
{
	return ICON_MATERIAL_DATA;
}

static void acf_fillmatd_name(bAnimListElem *ale, char *name)
{
	strcpy(name, "Materials");
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_fillmatd_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return OB_ADS_SHOWMATS;
		
		default: /* unsupported */
			return 0;
	}
}

/* materials expander type define */
static bAnimChannelType ACF_FILLMATD= 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_fillmatd_name,				/* name */
	acf_fillmatd_icon,				/* icon */
	
	acf_generic_dsexpand_setting_valid,	/* has setting */
	acf_fillmatd_setting_flag,				/* flag for setting */
	acf_generic_dsexpand_setting_ptr		/* pointer for setting */
};

/* Particles Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_fillpartd_icon(bAnimListElem *ale)
{
	return ICON_PARTICLE_DATA;
}

static void acf_fillpartd_name(bAnimListElem *ale, char *name)
{
	strcpy(name, "Particles");
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_fillpartd_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return OB_ADS_SHOWPARTS;
		
		default: /* unsupported */
			return 0;
	}
}

/* particles expander type define */
static bAnimChannelType ACF_FILLPARTD= 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_fillpartd_name,				/* name */
	acf_fillpartd_icon,				/* icon */
	
	acf_generic_dsexpand_setting_valid,	/* has setting */
	acf_fillpartd_setting_flag,				/* flag for setting */
	acf_generic_dsexpand_setting_ptr		/* pointer for setting */
};

/* Material Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsmat_icon(bAnimListElem *ale)
{
	return ICON_MATERIAL_DATA;
}

/* offset for material expanders */
static short acf_dsmat_offset(bAnimContext *ac, bAnimListElem *ale)
{
	return 21;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsmat_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return MA_DS_EXPAND;
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
			return ADT_NLA_EVAL_OFF;
			
		case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
			*neg= 1;
			return ADT_CURVES_NOT_VISIBLE;
			
		case ACHANNEL_SETTING_SELECT: /* selected */
			return ADT_UI_SELECTED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_dsmat_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Material *ma= (Material *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(ma->flag);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (ma->adt)
				GET_ACF_FLAG_PTR(ma->adt->flag)
			else
				return NULL;	
		
		default: /* unsupported */
			return NULL;
	}
}

/* material expander type define */
static bAnimChannelType ACF_DSMAT= 
{
	acf_generic_channel_backdrop,	/* backdrop */
	acf_generic_indention_0,		/* indent level */
	acf_dsmat_offset,				/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_dsmat_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dsmat_setting_flag,					/* flag for setting */
	acf_dsmat_setting_ptr					/* pointer for setting */
};

/* Lamp Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dslam_icon(bAnimListElem *ale)
{
	return ICON_LAMP_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dslam_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return LA_DS_EXPAND;
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
			return ADT_NLA_EVAL_OFF;
			
		case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
			*neg= 1;
			return ADT_CURVES_NOT_VISIBLE;
			
		case ACHANNEL_SETTING_SELECT: /* selected */
			return ADT_UI_SELECTED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_dslam_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Lamp *la= (Lamp *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(la->flag);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (la->adt)
				GET_ACF_FLAG_PTR(la->adt->flag)
			else
				return NULL;	
		
		default: /* unsupported */
			return NULL;
	}
}

/* lamp expander type define */
static bAnimChannelType ACF_DSLAM= 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_dslam_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dslam_setting_flag,					/* flag for setting */
	acf_dslam_setting_ptr					/* pointer for setting */
};

/* Camera Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dscam_icon(bAnimListElem *ale)
{
	return ICON_CAMERA_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dscam_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return CAM_DS_EXPAND;
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
			return ADT_NLA_EVAL_OFF;
			
		case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
			*neg= 1;
			return ADT_CURVES_NOT_VISIBLE;
			
		case ACHANNEL_SETTING_SELECT: /* selected */
			return ADT_UI_SELECTED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_dscam_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Camera *ca= (Camera *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(ca->flag);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (ca->adt)
				GET_ACF_FLAG_PTR(ca->adt->flag)
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* camera expander type define */
static bAnimChannelType ACF_DSCAM= 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_dscam_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dscam_setting_flag,					/* flag for setting */
	acf_dscam_setting_ptr					/* pointer for setting */
};

/* Curve Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dscur_icon(bAnimListElem *ale)
{
	return ICON_CURVE_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dscur_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return CU_DS_EXPAND;
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
			return ADT_NLA_EVAL_OFF;
			
		case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
			*neg= 1;
			return ADT_CURVES_NOT_VISIBLE;
			
		case ACHANNEL_SETTING_SELECT: /* selected */
			return ADT_UI_SELECTED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_dscur_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Curve *cu= (Curve *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(cu->flag);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (cu->adt)
				GET_ACF_FLAG_PTR(cu->adt->flag)
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* curve expander type define */
static bAnimChannelType ACF_DSCUR= 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_dscur_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dscur_setting_flag,					/* flag for setting */
	acf_dscur_setting_ptr					/* pointer for setting */
};

/* Shape Key Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsskey_icon(bAnimListElem *ale)
{
	return ICON_SHAPEKEY_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsskey_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return KEYBLOCK_DS_EXPAND;
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
			return ADT_NLA_EVAL_OFF;
			
		case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
			*neg= 1;
			return ADT_CURVES_NOT_VISIBLE;
			
		case ACHANNEL_SETTING_SELECT: /* selected */
			return ADT_UI_SELECTED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_dsskey_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Key *key= (Key *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(key->flag);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (key->adt)
				GET_ACF_FLAG_PTR(key->adt->flag)
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* shapekey expander type define */
static bAnimChannelType ACF_DSSKEY= 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_dsskey_icon,				/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dsskey_setting_flag,				/* flag for setting */
	acf_dsskey_setting_ptr					/* pointer for setting */
};

/* World Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dswor_icon(bAnimListElem *ale)
{
	return ICON_WORLD_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dswor_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return WO_DS_EXPAND;
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
			return ADT_NLA_EVAL_OFF;
			
		case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
			*neg= 1;
			return ADT_CURVES_NOT_VISIBLE;
			
		case ACHANNEL_SETTING_SELECT: /* selected */
			return ADT_UI_SELECTED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_dswor_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	World *wo= (World *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(wo->flag);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (wo->adt)
				GET_ACF_FLAG_PTR(wo->adt->flag)
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* world expander type define */
static bAnimChannelType ACF_DSWOR= 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_dswor_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dswor_setting_flag,					/* flag for setting */
	acf_dswor_setting_ptr					/* pointer for setting */
};

/* Particle Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dspart_icon(bAnimListElem *ale)
{
	return ICON_PARTICLE_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dspart_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return PART_DS_EXPAND;
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
			return ADT_NLA_EVAL_OFF;
			
		case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
			*neg= 1;
			return ADT_CURVES_NOT_VISIBLE;
			
		case ACHANNEL_SETTING_SELECT: /* selected */
			return ADT_UI_SELECTED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_dspart_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	ParticleSettings *part= (ParticleSettings *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(part->flag);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (part->adt)
				GET_ACF_FLAG_PTR(part->adt->flag)
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* particle expander type define */
static bAnimChannelType ACF_DSPART= 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_dspart_icon,				/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dspart_setting_flag,				/* flag for setting */
	acf_dspart_setting_ptr					/* pointer for setting */
};

/* MetaBall Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsmball_icon(bAnimListElem *ale)
{
	return ICON_META_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsmball_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return MB_DS_EXPAND;
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
			return ADT_NLA_EVAL_OFF;
			
		case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
			*neg= 1;
			return ADT_CURVES_NOT_VISIBLE;
		
		case ACHANNEL_SETTING_SELECT: /* selected */
			return ADT_UI_SELECTED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_dsmball_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	MetaBall *mb= (MetaBall *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(mb->flag);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (mb->adt)
				GET_ACF_FLAG_PTR(mb->adt->flag)
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* metaball expander type define */
static bAnimChannelType ACF_DSMBALL= 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_dsmball_icon,				/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dsmball_setting_flag,				/* flag for setting */
	acf_dsmball_setting_ptr					/* pointer for setting */
};

/* Armature Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsarm_icon(bAnimListElem *ale)
{
	return ICON_ARMATURE_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsarm_setting_flag(int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return ARM_DS_EXPAND;
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
			return ADT_NLA_EVAL_OFF;
			
		case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
			*neg= 1;
			return ADT_CURVES_NOT_VISIBLE;
			
		case ACHANNEL_SETTING_SELECT: /* selected */
			return ADT_UI_SELECTED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_dsarm_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	bArmature *arm= (bArmature *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			GET_ACF_FLAG_PTR(arm->flag);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (arm->adt)
				GET_ACF_FLAG_PTR(arm->adt->flag)
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* metaball expander type define */
static bAnimChannelType ACF_DSARM= 
{
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_dsarm_icon,				/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dsarm_setting_flag,				/* flag for setting */
	acf_dsarm_setting_ptr					/* pointer for setting */
};


/* ShapeKey Entry  ------------------------------------------- */
// XXX ... this is currently obsolete...

#if 0
static void dummy_olddraw_shapekeys ()
{
	case ANIMTYPE_SHAPEKEY: /* shapekey channel */
	{
		KeyBlock *kb = (KeyBlock *)ale->data;
		
		indent = 0;
		special = -1;
		
		offset= (ale->id) ? 21 : 0;
		
		if (kb->name[0] == '\0')
			sprintf(name, "Key %d", ale->index);
		else
			strcpy(name, kb->name);
	}
		break;
}
#endif

/* Grease Pencil entries  ------------------------------------------- */
// XXX ... this is currently not restored yet

#if 0
static void dummy_olddraw_gpencil ()
{
	/* determine what needs to be drawn */
	switch (ale->type) {
		case ANIMTYPE_GPDATABLOCK: /* gpencil datablock */
		{
			bGPdata *gpd = (bGPdata *)ale->data;
			ScrArea *sa = (ScrArea *)ale->owner;
			
			indent = 0;
			group= 3;
			
			/* only show expand if there are any channels */
			if (gpd->layers.first) {
				if (gpd->flag & GP_DATA_EXPAND)
					expand = ICON_TRIA_DOWN;
				else
					expand = ICON_TRIA_RIGHT;
			}
			
			switch (sa->spacetype) {
				case SPACE_VIEW3D:
				{
					/* this shouldn't cause any overflow... */
					//sprintf(name, "3DView:%s", view3d_get_name(sa->spacedata.first)); // XXX missing func..
					strcpy(name, "3dView");
					special= ICON_VIEW3D;
				}
					break;
				case SPACE_NODE:
				{
					SpaceNode *snode= sa->spacedata.first;
					char treetype[12];
					
					if (snode->treetype == 1)
						strcpy(treetype, "Composite");
					else
						strcpy(treetype, "Material");
					sprintf(name, "Nodes:%s", treetype);
					
					special= ICON_NODE;
				}
					break;
				case SPACE_SEQ:
				{
					SpaceSeq *sseq= sa->spacedata.first;
					char imgpreview[10];
					
					switch (sseq->mainb) {
						case 1: 	sprintf(imgpreview, "Image..."); 	break;
						case 2: 	sprintf(imgpreview, "Luma..."); 	break;
						case 3: 	sprintf(imgpreview, "Chroma...");	break;
						case 4: 	sprintf(imgpreview, "Histogram");	break;
						
						default:	sprintf(imgpreview, "Sequence");	break;
					}
					sprintf(name, "Sequencer:%s", imgpreview);
					
					special= ICON_SEQUENCE;
				}
					break;
				case SPACE_IMAGE:
				{
					SpaceImage *sima= sa->spacedata.first;
					
					if (sima->image)
						sprintf(name, "Image:%s", sima->image->id.name+2);
					else
						strcpy(name, "Image:<None>");
						
					special= ICON_IMAGE_COL;
				}
					break;
				
				default:
				{
					sprintf(name, "<Unknown GP-Data Source>");
					special= -1;
				}
					break;
			}
		}
			break;
		case ANIMTYPE_GPLAYER: /* gpencil layer */
		{
			bGPDlayer *gpl = (bGPDlayer *)ale->data;
			
			indent = 0;
			special = -1;
			expand = -1;
			group = 1;
			
			if (EDITABLE_GPL(gpl))
				protect = ICON_UNLOCKED;
			else
				protect = ICON_LOCKED;
				
			if (gpl->flag & GP_LAYER_HIDE)
				mute = ICON_MUTE_IPO_ON;
			else
				mute = ICON_MUTE_IPO_OFF;
			
			sel = SEL_GPL(gpl);
			BLI_snprintf(name, 32, gpl->info);
		}
			break;
	}	
	
	if (group == 3) {
		/* only for gp-data channels */
		UI_ThemeColorShade(TH_GROUP, 20);
		uiSetRoundBox((expand == ICON_TRIA_DOWN)? (1):(1|8));
		gl_round_box(GL_POLYGON, x+offset,  yminc, (float)ACHANNEL_NAMEWIDTH, ymaxc, 8);
	}
}
#endif

/* *********************************************** */
/* Type Registration and General Access */

/* These globals only ever get directly accessed in this file */
static bAnimChannelType *animchannelTypeInfo[ANIMTYPE_NUM_TYPES];
static short ACF_INIT= 1; /* when non-zero, the list needs to be updated */

/* Initialise type info definitions */
void ANIM_init_channel_typeinfo_data (void)
{
	int type= 0;
	
	/* start initialising if necessary... */
	if (ACF_INIT) {
		ACF_INIT= 0;
		
		animchannelTypeInfo[type++]= NULL; 				/* None */
		animchannelTypeInfo[type++]= NULL; 				/* AnimData */
		animchannelTypeInfo[type++]= NULL; 				/* Special */
		
		animchannelTypeInfo[type++]= &ACF_SUMMARY;		/* Motion Summary */
		
		animchannelTypeInfo[type++]= &ACF_SCENE; 		/* Scene */
		animchannelTypeInfo[type++]= &ACF_OBJECT; 		/* Object */
		animchannelTypeInfo[type++]= &ACF_GROUP; 		/* Group */
		animchannelTypeInfo[type++]= &ACF_FCURVE; 		/* F-Curve */
		
		animchannelTypeInfo[type++]= &ACF_FILLACTD; 	/* Object Action Expander */
		animchannelTypeInfo[type++]= &ACF_FILLDRIVERS; 	/* Drivers Expander */
		animchannelTypeInfo[type++]= &ACF_FILLMATD; 	/* Materials Expander */
		animchannelTypeInfo[type++]= &ACF_FILLPARTD; 	/* Particles Expander */
		
		animchannelTypeInfo[type++]= &ACF_DSMAT;		/* Material Channel */
		animchannelTypeInfo[type++]= &ACF_DSLAM;		/* Lamp Channel */
		animchannelTypeInfo[type++]= &ACF_DSCAM;		/* Camera Channel */
		animchannelTypeInfo[type++]= &ACF_DSCUR;		/* Curve Channel */
		animchannelTypeInfo[type++]= &ACF_DSSKEY;		/* ShapeKey Channel */
		animchannelTypeInfo[type++]= &ACF_DSWOR;		/* World Channel */
		animchannelTypeInfo[type++]= &ACF_DSPART;		/* Particle Channel */
		animchannelTypeInfo[type++]= &ACF_DSMBALL;		/* MetaBall Channel */
		animchannelTypeInfo[type++]= &ACF_DSARM;		/* Armature Channel */
		
		animchannelTypeInfo[type++]= NULL;				/* ShapeKey */ // XXX this is no longer used for now...
		
			// XXX not restored yet
		animchannelTypeInfo[type++]= NULL;				/* Grease Pencil Datablock */ 
		animchannelTypeInfo[type++]= NULL;				/* Grease Pencil Layer */ 
		
			// TODO: these types still need to be implemented!!!
			// probably need a few extra flags for these special cases...
		animchannelTypeInfo[type++]= NULL;				/* NLA Track */ 
		animchannelTypeInfo[type++]= NULL;				/* NLA Action */ 
	}
} 

/* Get type info from given channel type */
bAnimChannelType *ANIM_channel_get_typeinfo (bAnimListElem *ale)
{
	/* santiy checks */
	if (ale == NULL)
		return NULL;
		
	/* init the typeinfo if not available yet... */
	ANIM_init_channel_typeinfo_data();
	
	/* check if type is in bounds... */
	if ((ale->type >= 0) && (ale->type < ANIMTYPE_NUM_TYPES))
		return animchannelTypeInfo[ale->type];
	else
		return NULL;
}

/* --------------------------- */

/* Check if some setting for a channel is enabled 
 * Returns: 1 = On, 0 = Off, -1 = Invalid
 */
short ANIM_channel_setting_get (bAnimContext *ac, bAnimListElem *ale, int setting)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	
	/* 1) check that the setting exists for the current context */
	if ( (acf) && (!acf->has_setting || acf->has_setting(ac, ale, setting)) ) 
	{
		/* 2) get pointer to check for flag in, and the flag to check for */
		short negflag, ptrsize;
		int flag;
		void *ptr;
		
		flag= acf->setting_flag(setting, &negflag);
		ptr= acf->setting_ptr(ale, setting, &ptrsize);
		
		/* check if flag is enabled */
		if (ptr && flag) {
			switch (ptrsize) {
				case sizeof(int):	/* integer pointer for setting */
				{
					int *val= (int *)ptr;
					
					if (negflag)
						return ((*val) & flag) == 0;
					else
						return ((*val) & flag) != 0;
				}
					break;
					
				case sizeof(short):	/* short pointer for setting */
				{
					short *val= (short *)ptr;
					
					if (negflag)
						return ((*val) & flag) == 0;
					else
						return ((*val) & flag) != 0;
				}
					break;
					
				case sizeof(char):	/* char pointer for setting */
				{
					char *val= (char *)ptr;
					
					if (negflag)
						return ((*val) & flag) == 0;
					else
						return ((*val) & flag) != 0;
				}
					break;
			}
		}
	}
	
	/* not found... */
	return -1;
}	


/* quick macro for use in ANIM_channel_setting_set - set flag for setting according the mode given */
#define ACF_SETTING_SET(sval, sflag, smode) \
	{\
		if (negflag) {\
			if (smode == ACHANNEL_SETFLAG_TOGGLE) 	(sval) ^= (sflag); \
			else if (smode == ACHANNEL_SETFLAG_ADD) (sval) &= ~(sflag); \
			else 									(sval) |= (sflag); \
		} \
		else {\
			if (smode == ACHANNEL_SETFLAG_TOGGLE) 	(sval) ^= (sflag); \
			else if (smode == ACHANNEL_SETFLAG_ADD) (sval) |= (sflag); \
			else 									(sval) &= ~(sflag); \
		}\
	}

/* Change value of some setting for a channel 
 *	- setting: eAnimChannel_Settings
 *	- mode: eAnimChannels_SetFlag
 */
void ANIM_channel_setting_set (bAnimContext *ac, bAnimListElem *ale, int setting, short mode)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	
	/* 1) check that the setting exists for the current context */
	if ( (acf) && (!acf->has_setting || acf->has_setting(ac, ale, setting)) ) 
	{
		/* 2) get pointer to check for flag in, and the flag to check for */
		short negflag, ptrsize;
		int flag;
		void *ptr;
		
		flag= acf->setting_flag(setting, &negflag);
		ptr= acf->setting_ptr(ale, setting, &ptrsize);
		
		/* check if flag is enabled */
		if (ptr && flag) {
			switch (ptrsize) {
				case sizeof(int):	/* integer pointer for setting */
				{
					int *val= (int *)ptr;
					ACF_SETTING_SET(*val, flag, mode);
				}
					break;
					
				case sizeof(short):	/* short pointer for setting */
				{
					short *val= (short *)ptr;
					ACF_SETTING_SET(*val, flag, mode);
				}
					break;
					
				case sizeof(char):	/* char pointer for setting */
				{
					char *val= (char *)ptr;
					ACF_SETTING_SET(*val, flag, mode);
				}
					break;
			}
		}
	}
}

/* --------------------------- */

// XXX hardcoded size of icons
#define ICON_WIDTH		17
// XXX hardcoded width of sliders
#define SLIDER_WIDTH	70

/* Draw the given channel */
// TODO: make this use UI controls for the buttons
void ANIM_channel_draw (bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	short selected, offset;
	float y, ymid, ytext;
	
	/* sanity checks - don't draw anything */
	if ELEM(NULL, acf, ale)
		return;
	
	/* get initial offset */
	if (acf->get_offset)
		offset= acf->get_offset(ac, ale);
	else
		offset= 0;
		
	/* calculate appropriate y-coordinates for icon buttons 
	 *	7 is hardcoded factor for half-height of icons
	 */
	y= (ymaxc - yminc)/2 + yminc;
	ymid= y - 7;
	/* y-coordinates for text is only 4 down from middle */
	ytext= y - 4;
	
	/* check if channel is selected */
	if (acf->has_setting(ac, ale, ACHANNEL_SETTING_SELECT))
		selected= ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_SELECT);
	else
		selected= 0;
		
	/* set blending again, as may not be set in previous step */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	/* step 1) draw backdrop ...........................................  */
	if (acf->draw_backdrop)
		acf->draw_backdrop(ac, ale, yminc, ymaxc);
		
	/* step 2) draw expand widget ....................................... */
	if (acf->has_setting(ac, ale, ACHANNEL_SETTING_EXPAND)) {
		/* just skip - drawn as widget now */
		offset += ICON_WIDTH; 
	}
		
	/* step 3) draw icon ............................................... */
	if (acf->icon) {
		UI_icon_draw(offset, ymid, acf->icon(ale));
		offset += ICON_WIDTH; 
	}
		
	/* step 4) draw special toggles  .................................
	 *	- in Graph Editor, checkboxes for visibility in curves area
	 *	- in NLA Editor, glowing dots for solo/not solo...
	 */
	if (ac->sa) {
		if ((ac->spacetype == SPACE_IPO) && acf->has_setting(ac, ale, ACHANNEL_SETTING_VISIBLE)) {
			/* for F-Curves, draw color-preview of curve behind checkbox */
			if (ale->type == ANIMTYPE_FCURVE) {
				FCurve *fcu= (FCurve *)ale->data;
				
				/* F-Curve channels need to have a special 'color code' box drawn, which is colored with whatever 
				 * color the curve has stored 
				 */
				glColor3fv(fcu->color);
				
				/* just a solid color rect
				 * 	hardcoded 17 pixels width is slightly wider than icon width, so that 
				 *	there's a slight border around it 
				 */
				glRectf(offset, yminc, offset+17, ymaxc);
			}
			
			/* icon is drawn as widget now... */
			offset += ICON_WIDTH; 
		}
		else if ((ac->spacetype == SPACE_NLA) && acf->has_setting(ac, ale, ACHANNEL_SETTING_SOLO)) {
			/* just skip - drawn as widget now */
			offset += ICON_WIDTH; 
		}
	}
	
	/* step 5) draw name ............................................... */
	if (acf->name) {
		char name[256]; /* hopefully this will be enough! */
		
		/* set text color */
		if (selected)
			UI_ThemeColor(TH_TEXT_HI);
		else
			UI_ThemeColor(TH_TEXT);
			
		/* get name */
		acf->name(ale, name);
		
		offset += 3;
		UI_DrawString(offset, ytext, name);
	}
}

/* ------------------ */

/* callback for (normal) widget settings - send notifiers */
static void achannel_setting_widget_cb(bContext *C, void *poin, void *poin2)
{
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN_EDIT, NULL);
}

/* callback for widget sliders - insert keyframes */
static void achannel_setting_slider_cb(bContext *C, void *id_poin, void *fcu_poin)
{
	ID *id= (ID *)id_poin;
	FCurve *fcu= (FCurve *)fcu_poin;
	
	Scene *scene= CTX_data_scene(C);
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	short flag=0, done=0;
	float cfra;
	
	/* get current frame */
	// NOTE: this will do for now...
	cfra= (float)CFRA;
	
	/* get flags for keyframing */
	if (IS_AUTOKEY_FLAG(INSERTNEEDED))
		flag |= INSERTKEY_NEEDED;
	if (IS_AUTOKEY_FLAG(AUTOMATKEY))
		flag |= INSERTKEY_MATRIX;
	if (IS_AUTOKEY_MODE(scene, EDITKEYS))
		flag |= INSERTKEY_REPLACE;
	
	
	/* get RNA pointer, and resolve the path */
	RNA_id_pointer_create(id, &id_ptr);
	
	/* try to resolve the path stored in the F-Curve */
	if (RNA_path_resolve(&id_ptr, fcu->rna_path, &ptr, &prop)) {
		/* set the special 'replace' flag if on a keyframe */
		if (fcurve_frame_has_keyframe(fcu, cfra, 0))
			flag |= INSERTKEY_REPLACE;
		
		/* insert a keyframe for this F-Curve */
		done= insert_keyframe_direct(ptr, prop, fcu, cfra, flag);
		
		if (done)
			WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN_EDIT, NULL);
	}
}



/* Draw a widget for some setting */
static void draw_setting_widget (bAnimContext *ac, bAnimListElem *ale, bAnimChannelType *acf, uiBlock *block, int xpos, int ypos, int setting)
{
	short negflag, ptrsize, enabled, butType;
	int flag, icon;
	void *ptr;
	char *tooltip;
	uiBut *but = NULL;
	
	/* get the flag and the pointer to that flag */
	flag= acf->setting_flag(setting, &negflag);
	ptr= acf->setting_ptr(ale, setting, &ptrsize);
	enabled= ANIM_channel_setting_get(ac, ale, setting);
	
	/* get the base icon for the setting */
	switch (setting) {
		case ACHANNEL_SETTING_VISIBLE:	/* visibility checkboxes */
			//icon= ((enabled)? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT);
			icon= ICON_CHECKBOX_DEHLT;
			
			if (ale->type == ANIMTYPE_FCURVE)
				tooltip= "F-Curve is visible in Graph Editor for editing.";
			else
				tooltip= "F-Curve(s) are visible in Graph Editor for editing.";
			break;
			
		case ACHANNEL_SETTING_EXPAND: /* expanded triangle */
			//icon= ((enabled)? ICON_TRIA_DOWN : ICON_TRIA_RIGHT);
			icon= ICON_TRIA_RIGHT;
			tooltip= "Make channels grouped under this channel visible.";
			break;
			
		case ACHANNEL_SETTING_SOLO: /* NLA Tracks only */
			//icon= ((enabled)? ICON_LAYER_ACTIVE : ICON_LAYER_USED);
			icon= ICON_LAYER_USED;
			tooltip= "NLA Track is the only one evaluated for the AnimData block it belongs to.";
			break;
		
		/* --- */
		
		case ACHANNEL_SETTING_PROTECT: /* protected lock */
			// TODO: what about when there's no protect needed?
			//icon= ((enabled)? ICON_LOCKED : ICON_UNLOCKED);
			icon= ICON_UNLOCKED;
			tooltip= "Editability of keyframes for this channel.";
			break;
			
		case ACHANNEL_SETTING_MUTE: /* muted eye */
			//icon= ((enabled)? ICON_MUTE_IPO_ON : ICON_MUTE_IPO_OFF);
			icon= ICON_MUTE_IPO_OFF;
			
			if (ale->type == ALE_FCURVE) 
				tooltip= "Does F-Curve contribute to result.";
			else
				tooltip= "Do channels contribute to result.";
			break;
			
		default:
			tooltip= NULL;
			icon= 0;
			break;
	}
	
	/* type of button */
	if (negflag)
		butType= ICONTOGN;
	else
		butType= ICONTOG;
	
	/* draw button for setting */
	if (ptr && flag) {
		switch (ptrsize) {
			case sizeof(int):	/* integer pointer for setting */
				but= uiDefIconButBitI(block, butType, flag, 0, icon, 
						xpos, ypos, ICON_WIDTH, ICON_WIDTH, ptr, 0, 0, 0, 0, tooltip);
				break;
				
			case sizeof(short):	/* short pointer for setting */
				but= uiDefIconButBitS(block, butType, flag, 0, icon, 
						xpos, ypos, ICON_WIDTH, ICON_WIDTH, ptr, 0, 0, 0, 0, tooltip);
				break;
				
			case sizeof(char):	/* char pointer for setting */
				but= uiDefIconButBitC(block, butType, flag, 0, icon, 
						xpos, ypos, ICON_WIDTH, ICON_WIDTH, ptr, 0, 0, 0, 0, tooltip);
				break;
		}
		
		/* set call to send relevant notifiers */
		// NOTE: for now, we only need to send 'edited' 
		if (but)
			uiButSetFunc(but, achannel_setting_widget_cb, NULL, NULL);
	}
}

/* Draw UI widgets the given channel */
// TODO: make this use UI controls for the buttons
void ANIM_channel_draw_widgets (bAnimContext *ac, bAnimListElem *ale, uiBlock *block, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	float y, ymid, ytext;
	short offset;
	
	/* sanity checks - don't draw anything */
	if ELEM3(NULL, acf, ale, block)
		return;
	
	/* get initial offset */
	if (acf->get_offset)
		offset= acf->get_offset(ac, ale);
	else
		offset= 0;
		
	/* calculate appropriate y-coordinates for icon buttons 
	 *	7 is hardcoded factor for half-height of icons
	 */
	y= (ymaxc - yminc)/2 + yminc;
	ymid= y - 7;
	/* y-coordinates for text is only 4 down from middle */
	ytext= y - 4;
	
	/* no button backdrop behind icons */
	uiBlockSetEmboss(block, UI_EMBOSSN);
	
	/* step 1) draw expand widget ....................................... */
	if (acf->has_setting(ac, ale, ACHANNEL_SETTING_EXPAND)) {
		draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_EXPAND);
		offset += ICON_WIDTH; 
	}
		
	/* step 2) draw icon ............................................... */
	if (acf->icon) {
		/* icon is not drawn here (not a widget) */
		offset += ICON_WIDTH; 
	}
		
	/* step 3) draw special toggles  .................................
	 *	- in Graph Editor, checkboxes for visibility in curves area
	 *	- in NLA Editor, glowing dots for solo/not solo...
	 */
	if (ac->sa) {
		if ((ac->spacetype == SPACE_IPO) && acf->has_setting(ac, ale, ACHANNEL_SETTING_VISIBLE)) {
			/* visibility toggle  */
			draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_VISIBLE);
			offset += ICON_WIDTH; 
		}
		else if ((ac->spacetype == SPACE_NLA) && acf->has_setting(ac, ale, ACHANNEL_SETTING_SOLO)) {
			/* 'solo' setting for NLA Tracks */
			draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_SOLO);
			offset += ICON_WIDTH; 
		}
	}
	
	/* step 4) draw text... */
	/* NOTE: this is not done here, since nothing to be clicked on... */
	
	/* step 5) draw mute+protection toggles + (sliders) ....................... */
	/* reset offset - now goes from RHS of panel */
	offset = 0;
	
	// TODO: when drawing sliders, make those draw instead of these toggles if not enough space
	
	if (v2d) {
		short draw_sliders = 0;
		
		/* check if we need to show the sliders */
		if ((ac->sa) && ELEM(ac->spacetype, SPACE_ACTION, SPACE_IPO)) {
			switch (ac->spacetype) {
				case SPACE_ACTION:
				{
					SpaceAction *saction= (SpaceAction *)ac->sa->spacedata.first;
					draw_sliders= (saction->flag & SACTION_SLIDERS);
				}
					break;
				case SPACE_IPO:
				{
					SpaceIpo *sipo= (SpaceIpo *)ac->sa->spacedata.first;
					draw_sliders= (sipo->flag & SIPO_SLIDERS);
				}
					break;
			}
		}
		
		/* check if there's enough space for the toggles if the sliders are drawn too */
		if ( !(draw_sliders) || ((v2d->mask.xmax-v2d->mask.xmin) > ACHANNEL_BUTTON_WIDTH/2) ) {
			/* protect... */
			if (acf->has_setting(ac, ale, ACHANNEL_SETTING_PROTECT)) {
				offset += ICON_WIDTH; 
				draw_setting_widget(ac, ale, acf, block, (int)v2d->cur.xmax-offset, ymid, ACHANNEL_SETTING_PROTECT);
			}
			/* mute... */
			if (acf->has_setting(ac, ale, ACHANNEL_SETTING_MUTE)) {
				offset += ICON_WIDTH;
				draw_setting_widget(ac, ale, acf, block, (int)v2d->cur.xmax-offset, ymid, ACHANNEL_SETTING_MUTE);
			}
		}
		
		/* draw slider
		 *	- even if we can draw sliders for this view, we must also check that the channel-type supports them
		 *	  (only only F-Curves really can support them for now)
		 *	- to make things easier, we use RNA-autobuts for this so that changes are reflected immediately, 
		 *	  whereever they occurred. BUT, we don't use the layout engine, otherwise we'd get wrong alignment,
		 *	  and wouldn't be able to auto-keyframe...
		 *	- slider should start before the toggles (if they're visible) to keep a clean line down the side
		 */
		if ((draw_sliders) && (ale->type == ANIMTYPE_FCURVE)) {
			/* adjust offset */
			offset += SLIDER_WIDTH;
			
			/* need backdrop behind sliders... */
			uiBlockSetEmboss(block, UI_EMBOSS);
			
			if (ale->id) { /* Slider using RNA Access -------------------- */
				FCurve *fcu= (FCurve *)ale->data;
				PointerRNA id_ptr, ptr;
				PropertyRNA *prop;
				
				/* get RNA pointer, and resolve the path */
				RNA_id_pointer_create(ale->id, &id_ptr);
				
				/* try to resolve the path */
				if (RNA_path_resolve(&id_ptr, fcu->rna_path, &ptr, &prop)) {
					uiBut *but;
					
					/* create the slider button, and assign relevant callback to ensure keyframes are inserted... */
					but= uiDefAutoButR(block, &ptr, prop, fcu->array_index, "", 0, (int)v2d->cur.xmax-offset, ymid, SLIDER_WIDTH, (int)ymaxc-yminc);
					uiButSetFunc(but, achannel_setting_slider_cb, ale->id, fcu);
				}
			}
			else { /* Special Slider for stuff without RNA Access ---------- */
				// TODO: only implement this case when we really need it...
			}
		}
	}
}

/* *********************************************** */
