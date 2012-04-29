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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/animation/anim_channels_defines.c
 *  \ingroup edanimation
 */


#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_world_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_speaker_types.h"

#include "RNA_access.h"

#include "BKE_curve.h"
#include "BKE_key.h"
#include "BKE_context.h"
#include "BKE_utildefines.h" /* FILE_MAX */

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

/* *********************************************** */
// XXX constant defines to be moved elsewhere?

/* extra padding for lengths (to go under scrollers) */
#define EXTRA_SCROLL_PAD	100.0f

/* size of indent steps */
#define INDENT_STEP_SIZE 	7

#define ANIM_CHAN_NAME_SIZE 256

/* macros used for type defines */

/* get the pointer used for some flag and return */
#define GET_ACF_FLAG_PTR(ptr, type) ((*(type) = sizeof((ptr))), &(ptr))


/* *********************************************** */
/* Generic Functions (Type independent) */

/* Draw Backdrop ---------------------------------- */

/* get backdrop color for top-level widgets (Scene and Object only) */
static void acf_generic_root_color(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), float r_color[3])
{
	/* darker blue for top-level widgets */
	UI_GetThemeColor3fv(TH_DOPESHEET_CHANNELOB, r_color);
}

/* backdrop for top-level widgets (Scene and Object only) */
static void acf_generic_root_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	short expanded= ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_EXPAND) != 0;
	short offset= (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
	float color[3];
	
	/* set backdrop drawing color */
	acf->get_backdrop_color(ac, ale, color);
	glColor3fv(color);
	
	/* rounded corners on LHS only - top only when expanded, but bottom too when collapsed */
	uiSetRoundBox(expanded ? UI_CNR_TOP_LEFT : (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT));
	uiDrawBox(GL_POLYGON, offset,  yminc, v2d->cur.xmax+EXTRA_SCROLL_PAD, ymaxc, 8);
}


/* get backdrop color for data expanders under top-level Scene/Object */
static void acf_generic_dataexpand_color(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), float r_color[3])
{
	/* lighter color than top-level widget */
	UI_GetThemeColor3fv(TH_DOPESHEET_CHANNELSUBOB, r_color);
}

/* backdrop for data expanders under top-level Scene/Object */
static void acf_generic_dataexpand_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	short offset= (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
	float color[3];
	
	/* set backdrop drawing color */
	acf->get_backdrop_color(ac, ale, color);
	glColor3fv(color);
	
	/* no rounded corner - just rectangular box */
	glRectf(offset, yminc, 	v2d->cur.xmax+EXTRA_SCROLL_PAD, ymaxc);
}

/* get backdrop color for generic channels */
static void acf_generic_channel_color(bAnimContext *ac, bAnimListElem *ale, float r_color[3])
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	SpaceAction *saction = NULL;
	bActionGroup *grp = NULL;
	short indent= (acf->get_indent_level) ? acf->get_indent_level(ac, ale) : 0;
	
	/* get context info needed... */
	if ((ac->sl) && (ac->spacetype == SPACE_ACTION))
		saction= (SpaceAction *)ac->sl;
		
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
		unsigned char cp[3];
		
		if (indent == 2) {
			copy_v3_v3_char((char *)cp, grp->cs.solid);
		}
		else if (indent == 1) {
			copy_v3_v3_char((char *)cp, grp->cs.select);
		}
		else {
			copy_v3_v3_char((char *)cp, grp->cs.active);
		}
		
		/* copy the colors over, transforming from bytes to floats */
		rgb_uchar_to_float(r_color, cp);
	}
	else {
		// FIXME: what happens when the indention is 1 greater than what it should be (due to grouping)?
		int colOfs= 20 - 20*indent;
		UI_GetThemeColorShade3fv(TH_HEADER, colOfs, r_color);
	}
}

/* backdrop for generic channels */
static void acf_generic_channel_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	short offset= (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
	float color[3];
	
	/* set backdrop drawing color */
	acf->get_backdrop_color(ac, ale, color);
	glColor3fv(color);
	
	/* no rounded corners - just rectangular box */
	glRectf(offset, yminc, 	v2d->cur.xmax+EXTRA_SCROLL_PAD, ymaxc);
}

/* Indention + Offset ------------------------------------------- */

/* indention level is always the value in the name */
static short acf_generic_indention_0(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale))
{
	return 0;
}
static short acf_generic_indention_1(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale))
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
static short acf_generic_indention_flexible(bAnimContext *UNUSED(ac), bAnimListElem *ale)
{
	short indent= 0;
	
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

/* offset based on nodetree type */
static short acf_nodetree_rootType_offset(bNodeTree *ntree)
{
	if (ntree) {
		switch (ntree->type) {
			case NTREE_SHADER:
				/* 1 additional level (i.e. is indented one level in from material, 
				 * so shift all right by one step) 
				 */
				return INDENT_STEP_SIZE; 
				
			case NTREE_COMPOSIT:
				/* no additional levels needed */
				return 0; 
				
			case NTREE_TEXTURE:
				/* 2 additional levels */
				return INDENT_STEP_SIZE*2; 
		}
	}
	
	// unknown
	return 0;
}

/* offset for groups + grouped entities */
static short acf_generic_group_offset(bAnimContext *ac, bAnimListElem *ale)
{
	short offset= acf_generic_basic_offset(ac, ale);
	
	if (ale->id) {
		/* texture animdata */
		if (GS(ale->id->name) == ID_TE) {
			offset += 21;
		}
		/* materials and particles animdata */
		else if (ELEM(GS(ale->id->name), ID_MA, ID_PA))
			offset += 14;
			
		/* if not in Action Editor mode, action-groups (and their children) must carry some offset too... */
		else if (ac->datatype != ANIMCONT_ACTION)
			offset += 14;
			
		/* nodetree animdata */
		if (GS(ale->id->name) == ID_NT) {
			offset += acf_nodetree_rootType_offset((bNodeTree*)ale->id);
		}
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
		BLI_strncpy(name, id->name+2, ANIM_CHAN_NAME_SIZE);
}

/* name property for ID block entries */
static short acf_generic_idblock_nameprop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
	RNA_id_pointer_create(ale->id, ptr);
	*prop = RNA_struct_name_property(ptr->type);
	
	return (*prop != NULL);
}


/* name property for ID block entries which are just subheading "fillers" */
static short acf_generic_idfill_nameprop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
	/* actual ID we're representing is stored in ale->data not ale->id, as id gives the owner */
	RNA_id_pointer_create(ale->data, ptr);
	*prop = RNA_struct_name_property(ptr->type);
	
	return (*prop != NULL);
}

/* Settings ------------------------------------------- */

#if 0
/* channel type has no settings */
static short acf_generic_none_setting_valid(bAnimContext *ac, bAnimListElem *ale, int setting)
{
	return 0;
}
#endif

/* check if some setting exists for this object-based data-expander (datablock only) */
static short acf_generic_dataexpand_setting_valid(bAnimContext *ac, bAnimListElem *UNUSED(ale), int setting)
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

/* get backdrop color for summary widget */
static void acf_summary_color(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), float r_color[3])
{
	// FIXME: hardcoded color - same as the 'action' line in NLA
		// reddish color 
	r_color[0] = 0.8f;
	r_color[1] = 0.2f;
	r_color[2] = 0.0f;
}

/* backdrop for summary widget */
static void acf_summary_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	float color[3];
	
	/* set backdrop drawing color */
	acf->get_backdrop_color(ac, ale, color);
	glColor3fv(color);
	
	/* rounded corners on LHS only 
	 *	- top and bottom 
	 *	- special hack: make the top a bit higher, since we are first... 
	 */
	uiSetRoundBox(UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT);
	uiDrawBox(GL_POLYGON, 0,  yminc-2, v2d->cur.xmax+EXTRA_SCROLL_PAD, ymaxc, 8);
}

/* name for summary entries */
static void acf_summary_name(bAnimListElem *UNUSED(ale), char *name)
{
	if (name)
		BLI_strncpy(name, "DopeSheet Summary", ANIM_CHAN_NAME_SIZE);
}

// TODO: this is really a temp icon I think
static int acf_summary_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_BORDERMOVE;
}

/* check if some setting exists for this channel */
static short acf_summary_setting_valid(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), int setting)
{
	/* only expanded is supported, as it is used for hiding all stuff which the summary covers */
	return (setting == ACHANNEL_SETTING_EXPAND);
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_summary_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
	if ((ac->sl) && (ac->spacetype == SPACE_ACTION) && (setting == ACHANNEL_SETTING_EXPAND)) {
		SpaceAction *saction= (SpaceAction *)ac->sl;
		bDopeSheet *ads= &saction->ads;
		
		/* return pointer to DopeSheet's flag */
		return GET_ACF_FLAG_PTR(ads->flag, type);
	}
	else {
		/* can't return anything useful - unsupported */
		*type= 0;
		return NULL;
	}
}

/* all animation summary (DopeSheet only) type define */
static bAnimChannelType ACF_SUMMARY = 
{
	"Summary",							/* type name */

	acf_summary_color,					/* backdrop color */
	acf_summary_backdrop,				/* backdrop */
	acf_generic_indention_0,			/* indent level */
	NULL,								/* offset */
	
	acf_summary_name,					/* name */
	NULL,								/* name prop */
	acf_summary_icon,					/* icon */
	
	acf_summary_setting_valid,			/* has setting */
	acf_summary_setting_flag,			/* flag for setting */
	acf_summary_setting_ptr				/* pointer for setting */
};

/* Scene ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_scene_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_SCENE_DATA;
}

/* check if some setting exists for this channel */
static short acf_scene_setting_valid(bAnimContext *ac, bAnimListElem *UNUSED(ale), int setting)
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
static int acf_scene_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
			return GET_ACF_FLAG_PTR(scene->flag, type);
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return GET_ACF_FLAG_PTR(scene->flag, type);
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (scene->adt)
				return GET_ACF_FLAG_PTR(scene->adt->flag, type);
			else
				return NULL;
			
		default: /* unsupported */
			return NULL;
	}
}

/* scene type define */
static bAnimChannelType ACF_SCENE = 
{
	"Scene",						/* type name */

	acf_generic_root_color,			/* backdrop color */
	acf_generic_root_backdrop,		/* backdrop */
	acf_generic_indention_0,		/* indent level */
	NULL,							/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
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

	switch (ob->type) {
		case OB_LAMP:
			return ICON_OUTLINER_OB_LAMP;
		case OB_MESH: 
			return ICON_OUTLINER_OB_MESH;
		case OB_CAMERA: 
			return ICON_OUTLINER_OB_CAMERA;
		case OB_CURVE: 
			return ICON_OUTLINER_OB_CURVE;
		case OB_MBALL: 
			return ICON_OUTLINER_OB_META;
		case OB_LATTICE: 
			return ICON_OUTLINER_OB_LATTICE;
		case OB_SPEAKER:
			return ICON_OUTLINER_OB_SPEAKER;
		case OB_ARMATURE:
			return ICON_OUTLINER_OB_ARMATURE;
		case OB_FONT: 
			return ICON_OUTLINER_OB_FONT;
		case OB_SURF: 
			return ICON_OUTLINER_OB_SURFACE;
		case OB_EMPTY: 
			return ICON_OUTLINER_OB_EMPTY;
		default:
			return ICON_OBJECT_DATA;
	}
	
}

/* name for object */
static void acf_object_name(bAnimListElem *ale, char *name)
{
	Base *base= (Base *)ale->data;
	Object *ob= base->object;
	
	/* just copy the name... */
	if (ob && name)
		BLI_strncpy(name, ob->id.name+2, ANIM_CHAN_NAME_SIZE);
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
static int acf_object_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
			return GET_ACF_FLAG_PTR(ob->flag, type);
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return GET_ACF_FLAG_PTR(ob->nlaflag, type); // xxx
			
		case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (ob->adt)
				return GET_ACF_FLAG_PTR(ob->adt->flag, type);
			else
				return NULL;
			
		default: /* unsupported */
			return NULL;
	}
}

/* object type define */
static bAnimChannelType ACF_OBJECT = 
{
	"Object",						/* type name */
	
	acf_generic_root_color,			/* backdrop color */
	acf_generic_root_backdrop,		/* backdrop */
	acf_generic_indention_0,		/* indent level */
	NULL,							/* offset */
	
	acf_object_name,				/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_object_icon,				/* icon */
	
	acf_object_setting_valid,		/* has setting */
	acf_object_setting_flag,		/* flag for setting */
	acf_object_setting_ptr			/* pointer for setting */
};

/* Group ------------------------------------------- */

/* get backdrop color for group widget */
static void acf_group_color(bAnimContext *UNUSED(ac), bAnimListElem *ale, float r_color[3])
{
	/* highlight only for action group channels */
	if (ale->flag & AGRP_ACTIVE)
		UI_GetThemeColorShade3fv(TH_GROUP_ACTIVE, 10, r_color);
	else
		UI_GetThemeColorShade3fv(TH_GROUP, 20, r_color);
}

/* backdrop for group widget */
static void acf_group_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	short expanded= ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_EXPAND) != 0;
	short offset= (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
	float color[3];
	
	/* set backdrop drawing color */
	acf->get_backdrop_color(ac, ale, color);
	glColor3fv(color);
	
	/* rounded corners on LHS only - top only when expanded, but bottom too when collapsed */
	uiSetRoundBox(expanded ? UI_CNR_TOP_LEFT : (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT));
	uiDrawBox(GL_POLYGON, offset,  yminc, v2d->cur.xmax+EXTRA_SCROLL_PAD, ymaxc, 8);
}

/* name for group entries */
static void acf_group_name(bAnimListElem *ale, char *name)
{
	bActionGroup *agrp= (bActionGroup *)ale->data;
	
	/* just copy the name... */
	if (agrp && name)
		BLI_strncpy(name, agrp->name, ANIM_CHAN_NAME_SIZE);
}

/* name property for group entries */
static short acf_group_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
	RNA_pointer_create(ale->id, &RNA_ActionGroup, ale->data, ptr);
	*prop = RNA_struct_name_property(ptr->type);
	
	return (*prop != NULL);
}

/* check if some setting exists for this channel */
static short acf_group_setting_valid(bAnimContext *ac, bAnimListElem *UNUSED(ale), int setting)
{
	/* for now, all settings are supported, though some are only conditionally */
	switch (setting) {
		case ACHANNEL_SETTING_VISIBLE: /* Only available in Graph Editor */
			return (ac->spacetype==SPACE_IPO);
			
		default: /* always supported */
			return 1;
	}
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_group_setting_flag(bAnimContext *ac, int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			return AGRP_SELECTED;
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
		{
			/* NOTE: Graph Editor uses a different flag to everywhere else for this,
			 * allowing different collapsing of groups there, since sharing the flag
			 * proved to be a hazard for workflows...
			 */
			return (ac->spacetype == SPACE_IPO) ? 
						AGRP_EXPANDED_G :	/* Graph Editor case */
						AGRP_EXPANDED;		/* DopeSheet and elsewhere */
		}
			
		case ACHANNEL_SETTING_MUTE: /* muted */
			return AGRP_MUTED;
			
		case ACHANNEL_SETTING_PROTECT: /* protected */
			// *neg= 1; - if we change this to edtiability
			return AGRP_PROTECTED;
			
		case ACHANNEL_SETTING_VISIBLE: /* visiblity - graph editor */
			*neg= 1;
			return AGRP_NOTVISIBLE;
	}
	
	/* this shouldn't happen */
	return 0;
}

/* get pointer to the setting */
static void *acf_group_setting_ptr(bAnimListElem *ale, int UNUSED(setting), short *type)
{
	bActionGroup *agrp= (bActionGroup *)ale->data;
	
	/* all flags are just in agrp->flag for now... */
	return GET_ACF_FLAG_PTR(agrp->flag, type);
}

/* group type define */
static bAnimChannelType ACF_GROUP = 
{
	"Group",						/* type name */
	
	acf_group_color,				/* backdrop color */
	acf_group_backdrop,				/* backdrop */
	acf_generic_indention_0,		/* indent level */
	acf_generic_group_offset,		/* offset */
	
	acf_group_name,					/* name */
	acf_group_name_prop,			/* name prop */
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
			return (ac->spacetype==SPACE_IPO);
			
		/* always available */
		default:
			return 1;
	}
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_fcurve_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			return FCURVE_SELECTED;
			
		case ACHANNEL_SETTING_MUTE: /* muted */
			return FCURVE_MUTED;
			
		case ACHANNEL_SETTING_PROTECT: /* protected */
			// *neg= 1; - if we change this to edtiability
			return FCURVE_PROTECTED;
			
		case ACHANNEL_SETTING_VISIBLE: /* visiblity - graph editor */
			return FCURVE_VISIBLE;
			
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_fcurve_setting_ptr(bAnimListElem *ale, int UNUSED(setting), short *type)
{
	FCurve *fcu= (FCurve *)ale->data;
	
	/* all flags are just in agrp->flag for now... */
	return GET_ACF_FLAG_PTR(fcu->flag, type);
}

/* fcurve type define */
static bAnimChannelType ACF_FCURVE = 
{
	"F-Curve",						/* type name */
	
	acf_generic_channel_color,		/* backdrop color */
	acf_generic_channel_backdrop,	/* backdrop */
	acf_generic_indention_flexible,	/* indent level */		// xxx rename this to f-curves only?
	acf_generic_group_offset,		/* offset */
	
	acf_fcurve_name,				/* name */
	NULL,							/* name prop */
	NULL,							/* icon */
	
	acf_fcurve_setting_valid,		/* has setting */
	acf_fcurve_setting_flag,		/* flag for setting */
	acf_fcurve_setting_ptr			/* pointer for setting */
};

/* Object Action Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_fillactd_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_ACTION;
}

/* check if some setting exists for this channel */
static short acf_fillactd_setting_valid(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), int setting)
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
static int acf_fillactd_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
				return GET_ACF_FLAG_PTR(adt->flag, type);
			}
			else
				return NULL;
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return GET_ACF_FLAG_PTR(act->flag, type);
		
		default: /* unsupported */
			return NULL;
	}
}

/* object action expander type define */
static bAnimChannelType ACF_FILLACTD = 
{
	"Ob-Action Filler",				/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idfill_nameprop,	/* name prop */
	acf_fillactd_icon,				/* icon */
	
	acf_fillactd_setting_valid,		/* has setting */
	acf_fillactd_setting_flag,		/* flag for setting */
	acf_fillactd_setting_ptr		/* pointer for setting */
};

/* Drivers Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_filldrivers_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_DRIVER;
}

static void acf_filldrivers_name(bAnimListElem *UNUSED(ale), char *name)
{
	BLI_strncpy(name, "Drivers", ANIM_CHAN_NAME_SIZE);
}

/* check if some setting exists for this channel */
// TODO: this could be made more generic
static short acf_filldrivers_setting_valid(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), int setting)
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
static int acf_filldrivers_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
			return GET_ACF_FLAG_PTR(adt->flag, type);
		
		default: /* unsupported */
			return NULL;
	}
}

/* drivers expander type define */
static bAnimChannelType ACF_FILLDRIVERS = 
{
	"Drivers Filler",				/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_filldrivers_name,			/* name */
	NULL,							/* name prop */
	acf_filldrivers_icon,			/* icon */
	
	acf_filldrivers_setting_valid,	/* has setting */
	acf_filldrivers_setting_flag,	/* flag for setting */
	acf_filldrivers_setting_ptr		/* pointer for setting */
};


/* Material Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsmat_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_MATERIAL_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsmat_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
			return GET_ACF_FLAG_PTR(ma->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (ma->adt)
				return GET_ACF_FLAG_PTR(ma->adt->flag, type);
			else
				return NULL;	
		
		default: /* unsupported */
			return NULL;
	}
}

/* material expander type define */
static bAnimChannelType ACF_DSMAT= 
{
	"Material Data Expander",		/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_dsmat_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dsmat_setting_flag,					/* flag for setting */
	acf_dsmat_setting_ptr					/* pointer for setting */
};

/* Lamp Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dslam_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_LAMP_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dslam_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
			return GET_ACF_FLAG_PTR(la->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (la->adt)
				return GET_ACF_FLAG_PTR(la->adt->flag, type);
			else
				return NULL;	
		
		default: /* unsupported */
			return NULL;
	}
}

/* lamp expander type define */
static bAnimChannelType ACF_DSLAM= 
{
	"Lamp Expander",				/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_dslam_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dslam_setting_flag,					/* flag for setting */
	acf_dslam_setting_ptr					/* pointer for setting */
};

/* Texture Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dstex_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_TEXTURE_DATA;
}

/* offset for texture expanders */
// FIXME: soon to be obsolete?
static short acf_dstex_offset(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale))
{
	return 14; // XXX: simply include this in indention instead?
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dstex_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return TEX_DS_EXPAND;
			
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
static void *acf_dstex_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Tex *tex= (Tex *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return GET_ACF_FLAG_PTR(tex->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (tex->adt)
				return GET_ACF_FLAG_PTR(tex->adt->flag, type);
			else
				return NULL;	
		
		default: /* unsupported */
			return NULL;
	}
}

/* texture expander type define */
static bAnimChannelType ACF_DSTEX= 
{
	"Texture Data Expander",		/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_dstex_offset,				/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idfill_nameprop,	/* name prop */
	acf_dstex_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dstex_setting_flag,					/* flag for setting */
	acf_dstex_setting_ptr					/* pointer for setting */
};

/* Camera Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dscam_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_CAMERA_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dscam_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
			return GET_ACF_FLAG_PTR(ca->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (ca->adt)
				return GET_ACF_FLAG_PTR(ca->adt->flag, type);
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* camera expander type define */
static bAnimChannelType ACF_DSCAM= 
{
	"Camera Expander",				/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idfill_nameprop,	/* name prop */
	acf_dscam_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dscam_setting_flag,					/* flag for setting */
	acf_dscam_setting_ptr					/* pointer for setting */
};

/* Curve Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dscur_icon(bAnimListElem *ale)
{
	Curve *cu= (Curve *)ale->data;
	short obtype= BKE_curve_type_get(cu);
	
	switch (obtype) {
		case OB_FONT:
			return ICON_FONT_DATA;
		case OB_SURF:
			return ICON_SURFACE_DATA;
		default:
			return ICON_CURVE_DATA;
	}
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dscur_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
			return GET_ACF_FLAG_PTR(cu->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (cu->adt)
				return GET_ACF_FLAG_PTR(cu->adt->flag, type);
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* curve expander type define */
static bAnimChannelType ACF_DSCUR= 
{
	"Curve Expander",				/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_dscur_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dscur_setting_flag,					/* flag for setting */
	acf_dscur_setting_ptr					/* pointer for setting */
};

/* Shape Key Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsskey_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_SHAPEKEY_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsskey_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return KEY_DS_EXPAND;
			
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
			return GET_ACF_FLAG_PTR(key->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (key->adt)
				return GET_ACF_FLAG_PTR(key->adt->flag, type);
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* shapekey expander type define */
static bAnimChannelType ACF_DSSKEY= 
{
	"Shape Key Expander",			/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_dsskey_icon,				/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dsskey_setting_flag,				/* flag for setting */
	acf_dsskey_setting_ptr					/* pointer for setting */
};

/* World Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dswor_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_WORLD_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dswor_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
			return GET_ACF_FLAG_PTR(wo->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (wo->adt)
				return GET_ACF_FLAG_PTR(wo->adt->flag, type);
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* world expander type define */
static bAnimChannelType ACF_DSWOR= 
{
	"World Expander",				/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idfill_nameprop,	/* name prop */
	acf_dswor_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dswor_setting_flag,					/* flag for setting */
	acf_dswor_setting_ptr					/* pointer for setting */
};

/* Particle Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dspart_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_PARTICLE_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dspart_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
			return GET_ACF_FLAG_PTR(part->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (part->adt)
				return GET_ACF_FLAG_PTR(part->adt->flag, type);
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* particle expander type define */
static bAnimChannelType ACF_DSPART= 
{
	"Particle Data Expander",		/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_dspart_icon,				/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dspart_setting_flag,				/* flag for setting */
	acf_dspart_setting_ptr					/* pointer for setting */
};

/* MetaBall Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsmball_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_META_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsmball_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
			return GET_ACF_FLAG_PTR(mb->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (mb->adt)
				return GET_ACF_FLAG_PTR(mb->adt->flag, type);
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* metaball expander type define */
static bAnimChannelType ACF_DSMBALL= 
{
	"Metaball Expander",			/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_dsmball_icon,				/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dsmball_setting_flag,				/* flag for setting */
	acf_dsmball_setting_ptr					/* pointer for setting */
};

/* Armature Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsarm_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_ARMATURE_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsarm_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
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
			return GET_ACF_FLAG_PTR(arm->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (arm->adt)
				return GET_ACF_FLAG_PTR(arm->adt->flag, type);
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* metaball expander type define */
static bAnimChannelType ACF_DSARM= 
{
	"Armature Expander",			/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_dsarm_icon,				/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dsarm_setting_flag,					/* flag for setting */
	acf_dsarm_setting_ptr					/* pointer for setting */
};

/* NodeTree Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsntree_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_NODETREE;
}

/* offset for nodetree expanders */
static short acf_dsntree_offset(bAnimContext *ac, bAnimListElem *ale)
{
	bNodeTree *ntree = (bNodeTree *)ale->data;
	short offset= acf_generic_basic_offset(ac, ale);
	
	offset += acf_nodetree_rootType_offset(ntree); 
	
	return offset;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsntree_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return NTREE_DS_EXPAND;
			
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
static void *acf_dsntree_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	bNodeTree *ntree= (bNodeTree *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return GET_ACF_FLAG_PTR(ntree->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (ntree->adt)
				return GET_ACF_FLAG_PTR(ntree->adt->flag, type);
				else
					return NULL;
			
		default: /* unsupported */
			return NULL;
	}
}

/* node tree expander type define */
static bAnimChannelType ACF_DSNTREE= 
{
	"Node Tree Expander",			/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_dsntree_offset,				/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_dsntree_icon,				/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dsntree_setting_flag,				/* flag for setting */
	acf_dsntree_setting_ptr					/* pointer for setting */
};

/* Mesh Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsmesh_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_MESH_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsmesh_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return ME_DS_EXPAND;
			
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
static void *acf_dsmesh_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Mesh *me= (Mesh *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return GET_ACF_FLAG_PTR(me->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (me->adt)
				return GET_ACF_FLAG_PTR(me->adt->flag, type);
				else
					return NULL;
			
		default: /* unsupported */
			return NULL;
	}
}

/* node tree expander type define */
static bAnimChannelType ACF_DSMESH= 
{
	"Mesh Expander",				/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */		// XXX this only works for compositing
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_dsmesh_icon,				/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dsmesh_setting_flag,				/* flag for setting */
	acf_dsmesh_setting_ptr					/* pointer for setting */
};

/* Lattice Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dslat_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_LATTICE_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dslat_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return LT_DS_EXPAND;
			
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
static void *acf_dslat_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Lattice *lt= (Lattice *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return GET_ACF_FLAG_PTR(lt->flag, type);
			
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (lt->adt)
				return GET_ACF_FLAG_PTR(lt->adt->flag, type);
				else
					return NULL;
			
		default: /* unsupported */
			return NULL;
	}
}

/* node tree expander type define */
static bAnimChannelType ACF_DSLAT= 
{
	"Lattice Expander",				/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */		// XXX this only works for compositing
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_dslat_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dslat_setting_flag,					/* flag for setting */
	acf_dslat_setting_ptr					/* pointer for setting */
};

/* Speaker Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsspk_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_SPEAKER;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsspk_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;

	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return SPK_DS_EXPAND;
		
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
static void *acf_dsspk_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	Speaker *spk= (Speaker *)ale->data;

	/* clear extra return data first */
	*type= 0;

	switch (setting) {
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return GET_ACF_FLAG_PTR(spk->flag, type);
		
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted (for NLA only) */
		case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
			if (spk->adt)
				return GET_ACF_FLAG_PTR(spk->adt->flag, type);
			else
				return NULL;
		
		default: /* unsupported */
			return NULL;
	}
}

/* speaker expander type define */
static bAnimChannelType ACF_DSSPK=
{
	"Speaker Expander",				/* type name */
	
	acf_generic_dataexpand_color,	/* backdrop color */
	acf_generic_dataexpand_backdrop,/* backdrop */
	acf_generic_indention_1,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idblock_nameprop,	/* name prop */
	acf_dsspk_icon,					/* icon */
	
	acf_generic_dataexpand_setting_valid,	/* has setting */
	acf_dsspk_setting_flag,					/* flag for setting */
	acf_dsspk_setting_ptr					/* pointer for setting */
};

/* ShapeKey Entry  ------------------------------------------- */

/* name for ShapeKey */
static void acf_shapekey_name(bAnimListElem *ale, char *name)
{
	KeyBlock *kb= (KeyBlock *)ale->data;
	
	/* just copy the name... */
	if (kb && name) {
		/* if the KeyBlock had a name, use it, otherwise use the index */
		if (kb->name[0])
			BLI_strncpy(name, kb->name, ANIM_CHAN_NAME_SIZE);
		else
			BLI_snprintf(name, ANIM_CHAN_NAME_SIZE, "Key %d", ale->index);
	}
}

/* name property for ShapeKey entries */
static short acf_shapekey_nameprop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
	KeyBlock *kb= (KeyBlock *)ale->data;
	
	/* if the KeyBlock had a name, use it, otherwise use the index */
	if (kb && kb->name[0]) {
		RNA_pointer_create(ale->id, &RNA_ShapeKey, kb, ptr);
		*prop = RNA_struct_name_property(ptr->type);
		
		return (*prop != NULL);
	}
	
	return 0;
}

/* check if some setting exists for this channel */
static short acf_shapekey_setting_valid(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), int setting)
{
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted */
		case ACHANNEL_SETTING_PROTECT: /* protected */
			return 1;
			
		/* nothing else is supported */
		default:
			return 0;
	}
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_shapekey_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_MUTE: /* mute */
			return KEYBLOCK_MUTE;
		
		case ACHANNEL_SETTING_SELECT: /* selected */
			return KEYBLOCK_SEL;
		
		case ACHANNEL_SETTING_PROTECT: /* locked */
			return KEYBLOCK_LOCKED;
		
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_shapekey_setting_ptr(bAnimListElem *ale, int setting, short *type)
{
	KeyBlock *kb= (KeyBlock *)ale->data;
	
	/* clear extra return data first */
	*type= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
		case ACHANNEL_SETTING_MUTE: /* muted */
		case ACHANNEL_SETTING_PROTECT: /* protected */
			return GET_ACF_FLAG_PTR(kb->flag, type);
		
		default: /* unsupported */
			return NULL;
	}
}

/* shapekey expander type define */
static bAnimChannelType ACF_SHAPEKEY= 
{
	"Shape Key",					/* type name */
	
	acf_generic_channel_color,		/* backdrop color */
	acf_generic_channel_backdrop,	/* backdrop */
	acf_generic_indention_0,		/* indent level */
	acf_generic_basic_offset,		/* offset */
	
	acf_shapekey_name,				/* name */
	acf_shapekey_nameprop,			/* name prop */
	NULL,							/* icon */
	
	acf_shapekey_setting_valid,		/* has setting */
	acf_shapekey_setting_flag,		/* flag for setting */
	acf_shapekey_setting_ptr		/* pointer for setting */
};

/* GPencil Datablock ------------------------------------------- */

/* get backdrop color for gpencil datablock widget */
static void acf_gpd_color(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), float r_color[3])
{
	/* these are ID-blocks, but not exactly standalone... */
	UI_GetThemeColorShade3fv(TH_DOPESHEET_CHANNELSUBOB, 20, r_color);
}

// TODO: just get this from RNA?
static int acf_gpd_icon(bAnimListElem *UNUSED(ale))
{
	return ICON_GREASEPENCIL;
}

/* check if some setting exists for this channel */
static short acf_gpd_setting_valid(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), int setting)
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
static int acf_gpd_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			return AGRP_SELECTED;
			
		case ACHANNEL_SETTING_EXPAND: /* expanded */
			return GP_DATA_EXPAND;
	}
	
	/* this shouldn't happen */
	return 0;
}

/* get pointer to the setting */
static void *acf_gpd_setting_ptr(bAnimListElem *ale, int UNUSED(setting), short *type)
{
	bGPdata *gpd= (bGPdata *)ale->data;
	
	/* all flags are just in gpd->flag for now... */
	return GET_ACF_FLAG_PTR(gpd->flag, type);
}

/* gpencil datablock type define */
static bAnimChannelType ACF_GPD = 
{
	"GPencil Datablock",			/* type name */
	
	acf_gpd_color,					/* backdrop color */
	acf_group_backdrop,				/* backdrop */
	acf_generic_indention_0,		/* indent level */
	acf_generic_group_offset,		/* offset */
	
	acf_generic_idblock_name,		/* name */
	acf_generic_idfill_nameprop,	/* name prop */
	acf_gpd_icon,					/* icon */
	
	acf_gpd_setting_valid,			/* has setting */
	acf_gpd_setting_flag,			/* flag for setting */
	acf_gpd_setting_ptr				/* pointer for setting */
};

/* GPencil Layer ------------------------------------------- */

/* name for grease pencil layer entries */
static void acf_gpl_name(bAnimListElem *ale, char *name)
{
	bGPDlayer *gpl = (bGPDlayer *)ale->data;
	
	if (gpl && name)
		BLI_strncpy(name, gpl->info, ANIM_CHAN_NAME_SIZE);
}

/* name property for grease pencil layer entries */
static short acf_gpl_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
	if (ale->data) {
		RNA_pointer_create(ale->id, &RNA_GPencilLayer, ale->data, ptr);
		*prop = RNA_struct_name_property(ptr->type);
		
		return (*prop != NULL);
	}
	
	return 0;
}

/* check if some setting exists for this channel */
static short acf_gpl_setting_valid(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), int setting)
{
	switch (setting) {
		/* unsupported */
		case ACHANNEL_SETTING_EXPAND: /* gpencil layers are more like F-Curves than groups */
		case ACHANNEL_SETTING_VISIBLE: /* graph editor only */
			return 0;
		
		/* always available */
		default:
			return 1;
	}
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_gpl_setting_flag(bAnimContext *UNUSED(ac), int setting, short *neg)
{
	/* clear extra return data first */
	*neg= 0;
	
	switch (setting) {
		case ACHANNEL_SETTING_SELECT: /* selected */
			return GP_LAYER_SELECT;
			
		case ACHANNEL_SETTING_MUTE: /* muted */
			return GP_LAYER_HIDE;
			
		case ACHANNEL_SETTING_PROTECT: /* protected */
			// *neg= 1; - if we change this to edtiability
			return GP_LAYER_LOCKED;
			
		default: /* unsupported */
			return 0;
	}
}

/* get pointer to the setting */
static void *acf_gpl_setting_ptr(bAnimListElem *ale, int UNUSED(setting), short *type)
{
	bGPDlayer *gpl= (bGPDlayer *)ale->data;
	
	/* all flags are just in agrp->flag for now... */
	return GET_ACF_FLAG_PTR(gpl->flag, type);
}

/* grease pencil layer type define */
static bAnimChannelType ACF_GPL = 
{
	"GPencil Layer",				/* type name */
	
	acf_generic_channel_color,		/* backdrop color */
	acf_generic_channel_backdrop,	/* backdrop */
	acf_generic_indention_flexible,	/* indent level */
	acf_generic_group_offset,		/* offset */
	
	acf_gpl_name,					/* name */
	acf_gpl_name_prop,				/* name prop */
	NULL,							/* icon */
	
	acf_gpl_setting_valid,			/* has setting */
	acf_gpl_setting_flag,			/* flag for setting */
	acf_gpl_setting_ptr				/* pointer for setting */
};

/* *********************************************** */
/* Type Registration and General Access */

/* These globals only ever get directly accessed in this file */
static bAnimChannelType *animchannelTypeInfo[ANIMTYPE_NUM_TYPES];
static short ACF_INIT= 1; /* when non-zero, the list needs to be updated */

/* Initialize type info definitions */
static void ANIM_init_channel_typeinfo_data (void)
{
	int type= 0;
	
	/* start initializing if necessary... */
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
		
		animchannelTypeInfo[type++]= &ACF_DSMAT;		/* Material Channel */
		animchannelTypeInfo[type++]= &ACF_DSLAM;		/* Lamp Channel */
		animchannelTypeInfo[type++]= &ACF_DSCAM;		/* Camera Channel */
		animchannelTypeInfo[type++]= &ACF_DSCUR;		/* Curve Channel */
		animchannelTypeInfo[type++]= &ACF_DSSKEY;		/* ShapeKey Channel */
		animchannelTypeInfo[type++]= &ACF_DSWOR;		/* World Channel */
		animchannelTypeInfo[type++]= &ACF_DSNTREE;		/* NodeTree Channel */
		animchannelTypeInfo[type++]= &ACF_DSPART;		/* Particle Channel */
		animchannelTypeInfo[type++]= &ACF_DSMBALL;		/* MetaBall Channel */
		animchannelTypeInfo[type++]= &ACF_DSARM;		/* Armature Channel */
		animchannelTypeInfo[type++]= &ACF_DSMESH;		/* Mesh Channel */
		animchannelTypeInfo[type++]= &ACF_DSTEX;		/* Texture Channel */
		animchannelTypeInfo[type++]= &ACF_DSLAT;		/* Lattice Channel */
		animchannelTypeInfo[type++]= &ACF_DSSPK;		/* Speaker Channel */
		
		animchannelTypeInfo[type++]= &ACF_SHAPEKEY;		/* ShapeKey */
		
		animchannelTypeInfo[type++]= &ACF_GPD;			/* Grease Pencil Datablock */ 
		animchannelTypeInfo[type++]= &ACF_GPL;			/* Grease Pencil Layer */ 
		
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

/* Print debug info string for the given channel */
void ANIM_channel_debug_print_info (bAnimListElem *ale, short indent_level)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	
	/* print indents */
	for (; indent_level > 0; indent_level--)
		printf("  ");
	
	/* print info */
	if (acf) {
		char name[ANIM_CHAN_NAME_SIZE]; /* hopefully this will be enough! */
		
		/* get UI name */
		if (acf->name)
			acf->name(ale, name);
		else
			BLI_strncpy(name, "<No name>", sizeof(name));

		/* print type name + ui name */
		printf("ChanType: <%s> Name: \"%s\"\n", acf->channel_type_name, name);
	}
	else if (ale)
		printf("ChanType: <Unknown - %d>\n", ale->type);
	else
		printf("<Invalid channel - NULL>\n");
}

/* --------------------------- */

/* Check if some setting for a channel is enabled 
 * Returns: 1 = On, 0 = Off, -1 = Invalid
 */
short ANIM_channel_setting_get (bAnimContext *ac, bAnimListElem *ale, int setting)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	
	/* 1) check that the setting exists for the current context */
	if ((acf) && (!acf->has_setting || acf->has_setting(ac, ale, setting))) {
		/* 2) get pointer to check for flag in, and the flag to check for */
		short negflag, ptrsize;
		int flag;
		void *ptr;
		
		flag= acf->setting_flag(ac, setting, &negflag);
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
			if (smode == ACHANNEL_SETFLAG_INVERT) 	(sval) ^= (sflag); \
			else if (smode == ACHANNEL_SETFLAG_ADD) (sval) &= ~(sflag); \
			else 									(sval) |= (sflag); \
		} \
		else {\
			if (smode == ACHANNEL_SETFLAG_INVERT) 	(sval) ^= (sflag); \
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
	if ((acf) && (!acf->has_setting || acf->has_setting(ac, ale, setting))) {
		/* 2) get pointer to check for flag in, and the flag to check for */
		short negflag, ptrsize;
		int flag;
		void *ptr;
		
		flag= acf->setting_flag(ac, setting, &negflag);
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
#define SLIDER_WIDTH	80
// XXX hardcoded width of rename textboxes
#define RENAME_TEXT_WIDTH 100

/* Draw the given channel */
// TODO: make this use UI controls for the buttons
void ANIM_channel_draw (bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	short selected, offset;
	float y, ymid, ytext;
	
	/* sanity checks - don't draw anything */
	if (ELEM(NULL, acf, ale))
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
	
	/* turn off blending, since not needed anymore... */
	glDisable(GL_BLEND);
		
	/* step 4) draw special toggles  .................................
	 *	- in Graph Editor, checkboxes for visibility in curves area
	 *	- in NLA Editor, glowing dots for solo/not solo...
	 */
	if (ac->sl) {
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
	// TODO: when renaming, we might not want to draw this, especially if name happens to be longer than channel
	if (acf->name) {
		char name[ANIM_CHAN_NAME_SIZE]; /* hopefully this will be enough! */
		
		/* set text color */
		// XXX: if active, highlight differently?
		if (selected)
			UI_ThemeColor(TH_TEXT_HI);
		else
			UI_ThemeColor(TH_TEXT);
			
		/* get name */
		acf->name(ale, name);
		
		offset += 3;
		UI_DrawString(offset, ytext, name);
		
		/* draw red underline if channel is disabled */
		if ((ale->type == ANIMTYPE_FCURVE) && (ale->flag & FCURVE_DISABLED)) {
			// FIXME: replace hardcoded color here, and check on extents!
			glColor3f(1.0f, 0.0f, 0.0f);
			glLineWidth(2.0);
				fdrawline((float)(offset), yminc, 
						  (float)(v2d->cur.xmax), yminc);
			glLineWidth(1.0);
		}
	}
	
	/* step 6) draw backdrops behidn mute+protection toggles + (sliders) ....................... */
	/* reset offset - now goes from RHS of panel */
	offset = 0;
	
	// TODO: when drawing sliders, make those draw instead of these toggles if not enough space
	
	if (v2d) {
		short draw_sliders = 0;
		float color[3];
		
		/* get and set backdrop color */
		acf->get_backdrop_color(ac, ale, color);
		glColor3fv(color);
		
		/* check if we need to show the sliders */
		if ((ac->sl) && ELEM(ac->spacetype, SPACE_ACTION, SPACE_IPO)) {
			switch (ac->spacetype) {
				case SPACE_ACTION:
				{
					SpaceAction *saction= (SpaceAction *)ac->sl;
					draw_sliders= (saction->flag & SACTION_SLIDERS);
				}
					break;
				case SPACE_IPO:
				{
					SpaceIpo *sipo= (SpaceIpo *)ac->sl;
					draw_sliders= (sipo->flag & SIPO_SLIDERS);
				}
					break;
			}
		}
		
		/* check if there's enough space for the toggles if the sliders are drawn too */
		if ( !(draw_sliders) || ((v2d->mask.xmax-v2d->mask.xmin) > ACHANNEL_BUTTON_WIDTH/2) ) {
			/* protect... */
			if (acf->has_setting(ac, ale, ACHANNEL_SETTING_PROTECT))
				offset += ICON_WIDTH;
			/* mute... */
			if (acf->has_setting(ac, ale, ACHANNEL_SETTING_MUTE))
				offset += ICON_WIDTH;
		}
		
		/* draw slider
		 *	- even if we can draw sliders for this view, we must also check that the channel-type supports them
		 *	  (only only F-Curves really can support them for now)
		 *	- slider should start before the toggles (if they're visible) to keep a clean line down the side
		 */
		if ((draw_sliders) && ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_SHAPEKEY)) {
			/* adjust offset */
			offset += SLIDER_WIDTH;
		}
		
		
		/* finally draw a backdrop rect behind these 
		 *	- starts from the point where the first toggle/slider starts, 
		 *	- ends past the space that might be reserved for a scroller
		 */
		glRectf(v2d->cur.xmax-(float)offset, yminc, v2d->cur.xmax+EXTRA_SCROLL_PAD, ymaxc);
	}
}

/* ------------------ */

/* callback for (normal) widget settings - send notifiers */
static void achannel_setting_widget_cb(bContext *C, void *UNUSED(arg1), void *UNUSED(arg2))
{
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
}

/* callback for widget settings that need flushing */
static void achannel_setting_flush_widget_cb(bContext *C, void *ale_npoin, void *setting_wrap)
{
	bAnimListElem *ale_setting= (bAnimListElem *)ale_npoin;
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	int filter;
	int setting = GET_INT_FROM_POINTER(setting_wrap);
	short on = 0;
	
	/* send notifiers before doing anything else... */
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	/* verify animation context */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return;
	
	/* verify that we have a channel to operate on, and that it has all we need */
	if (ale_setting) {
		/* check if the setting is on... */
		on= ANIM_channel_setting_get(&ac, ale_setting, setting);
		
		/* on == -1 means setting not found... */
		if (on == -1)
			return;
	}
	else
		return;
	
	/* get all channels that can possibly be chosen - but ignore hierarchy */
	filter= ANIMFILTER_DATA_VISIBLE|ANIMFILTER_LIST_CHANNELS;
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* call API method to flush the setting */
	ANIM_flush_setting_anim_channels(&ac, &anim_data, ale_setting, setting, on);
	
	/* free temp data */
	BLI_freelistN(&anim_data);
}

/* callback for rename widgets - clear rename-in-progress */
static void achannel_setting_rename_done_cb(bContext *C, void *ads_poin, void *UNUSED(arg2))
{
	bDopeSheet *ads = (bDopeSheet *)ads_poin;
	
	/* reset rename index so that edit box disappears now that editing is done */
	ads->renameIndex = 0;
	
	/* send notifiers */
	// XXX: right notifier?
	WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_RENAME, NULL);
}

/* callback for widget sliders - insert keyframes */
static void achannel_setting_slider_cb(bContext *C, void *id_poin, void *fcu_poin)
{
	ID *id= (ID *)id_poin;
	FCurve *fcu= (FCurve *)fcu_poin;
	
	ReportList *reports = CTX_wm_reports(C);
	Scene *scene= CTX_data_scene(C);
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	short flag=0, done=0;
	float cfra;
	
	/* get current frame */
	// NOTE: this will do for now...
	cfra= (float)CFRA;
	
	/* get flags for keyframing */
	flag = ANIM_get_keyframing_flags(scene, 1);
	
	/* get RNA pointer, and resolve the path */
	RNA_id_pointer_create(id, &id_ptr);
	
	/* try to resolve the path stored in the F-Curve */
	if (RNA_path_resolve(&id_ptr, fcu->rna_path, &ptr, &prop)) {
		/* set the special 'replace' flag if on a keyframe */
		if (fcurve_frame_has_keyframe(fcu, cfra, 0))
			flag |= INSERTKEY_REPLACE;
		
		/* insert a keyframe for this F-Curve */
		done= insert_keyframe_direct(reports, ptr, prop, fcu, cfra, flag);
		
		if (done)
			WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	}
}

/* callback for shapekey widget sliders - insert keyframes */
static void achannel_setting_slider_shapekey_cb(bContext *C, void *key_poin, void *kb_poin)
{
	Key *key= (Key *)key_poin;
	KeyBlock *kb= (KeyBlock *)kb_poin;
	char *rna_path= key_get_curValue_rnaPath(key, kb);
	
	ReportList *reports = CTX_wm_reports(C);
	Scene *scene= CTX_data_scene(C);
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	short flag=0, done=0;
	float cfra;
	
	/* get current frame */
	// NOTE: this will do for now...
	cfra= (float)CFRA;
	
	/* get flags for keyframing */
	flag = ANIM_get_keyframing_flags(scene, 1);
	
	/* get RNA pointer, and resolve the path */
	RNA_id_pointer_create((ID *)key, &id_ptr);
	
	/* try to resolve the path stored in the F-Curve */
	if (RNA_path_resolve(&id_ptr, rna_path, &ptr, &prop)) {
		/* find or create new F-Curve */
		// XXX is the group name for this ok?
		bAction *act= verify_adt_action((ID *)key, 1);
		FCurve *fcu= verify_fcurve(act, NULL, rna_path, 0, 1);
		
		/* set the special 'replace' flag if on a keyframe */
		if (fcurve_frame_has_keyframe(fcu, cfra, 0))
			flag |= INSERTKEY_REPLACE;
		
		/* insert a keyframe for this F-Curve */
		done= insert_keyframe_direct(reports, ptr, prop, fcu, cfra, flag);
		
		if (done)
			WM_event_add_notifier(C, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	}
	
	/* free the path */
	if (rna_path)
		MEM_freeN(rna_path);
}

/* Draw a widget for some setting */
static void draw_setting_widget(bAnimContext *ac, bAnimListElem *ale, bAnimChannelType *acf,
                                uiBlock *block, int xpos, int ypos, int setting)
{
	short negflag, ptrsize /* , enabled */ /* UNUSED */, butType;
	int flag, icon;
	void *ptr;
	const char *tooltip;
	uiBut *but = NULL;
	
	/* get the flag and the pointer to that flag */
	flag= acf->setting_flag(ac, setting, &negflag);
	ptr= acf->setting_ptr(ale, setting, &ptrsize);
	/* enabled= ANIM_channel_setting_get(ac, ale, setting); */ /* UNUSED */
	
	/* get the base icon for the setting */
	switch (setting) {
		case ACHANNEL_SETTING_VISIBLE:	/* visibility eyes */
			//icon= ((enabled)? ICON_VISIBLE_IPO_ON : ICON_VISIBLE_IPO_OFF);
			icon= ICON_VISIBLE_IPO_OFF;
			
			if (ale->type == ANIMTYPE_FCURVE)
				tooltip= "Channel is visible in Graph Editor for editing";
			else
				tooltip= "Channel(s) are visible in Graph Editor for editing";
			break;
			
		case ACHANNEL_SETTING_EXPAND: /* expanded triangle */
			//icon= ((enabled)? ICON_TRIA_DOWN : ICON_TRIA_RIGHT);
			icon= ICON_TRIA_RIGHT;
			tooltip= "Make channels grouped under this channel visible";
			break;
			
		case ACHANNEL_SETTING_SOLO: /* NLA Tracks only */
			//icon= ((enabled)? ICON_LAYER_ACTIVE : ICON_LAYER_USED);
			icon= ICON_LAYER_USED;
			tooltip= "NLA Track is the only one evaluated for the AnimData block it belongs to";
			break;
		
		/* --- */
		
		case ACHANNEL_SETTING_PROTECT: /* protected lock */
			// TODO: what about when there's no protect needed?
			//icon= ((enabled)? ICON_LOCKED : ICON_UNLOCKED);
			icon= ICON_UNLOCKED;
			tooltip= "Editability of keyframes for this channel";
			break;
			
		case ACHANNEL_SETTING_MUTE: /* muted speaker */
			//icon= ((enabled)? ICON_MUTE_IPO_ON : ICON_MUTE_IPO_OFF);
			icon= ICON_MUTE_IPO_OFF;
			
			if (ale->type == ALE_FCURVE) 
				tooltip= "Does F-Curve contribute to result";
			else
				tooltip= "Do channels contribute to result";
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
				but = uiDefIconButBitI(block, butType, flag, 0, icon, 
						xpos, ypos, ICON_WIDTH, ICON_WIDTH, ptr, 0, 0, 0, 0, tooltip);
				break;
				
			case sizeof(short):	/* short pointer for setting */
				but = uiDefIconButBitS(block, butType, flag, 0, icon, 
						xpos, ypos, ICON_WIDTH, ICON_WIDTH, ptr, 0, 0, 0, 0, tooltip);
				break;
				
			case sizeof(char):	/* char pointer for setting */
				but = uiDefIconButBitC(block, butType, flag, 0, icon, 
						xpos, ypos, ICON_WIDTH, ICON_WIDTH, ptr, 0, 0, 0, 0, tooltip);
				break;
		}
		
		/* set call to send relevant notifiers and/or perform type-specific updates */
		if (but) {
			switch (setting) {
				/* settings needing flushing up/down hierarchy  */
				case ACHANNEL_SETTING_VISIBLE: /* Graph Editor - 'visibility' toggles */
				case ACHANNEL_SETTING_PROTECT: /* General - protection flags */
				case ACHANNEL_SETTING_MUTE: /* General - muting flags */
					uiButSetNFunc(but, achannel_setting_flush_widget_cb, MEM_dupallocN(ale), SET_INT_IN_POINTER(setting));
					break;
					
				/* no flushing */
				case ACHANNEL_SETTING_EXPAND: /* expanding - cannot flush, otherwise all would open/close at once */
				default:
					uiButSetFunc(but, achannel_setting_widget_cb, NULL, NULL);
			}
		}
	}
}

/* Draw UI widgets the given channel */
void ANIM_channel_draw_widgets (bContext *C, bAnimContext *ac, bAnimListElem *ale, uiBlock *block, float yminc, float ymaxc, size_t channel_index)
{
	bAnimChannelType *acf= ANIM_channel_get_typeinfo(ale);
	View2D *v2d= &ac->ar->v2d;
	float y, ymid/*, ytext*/;
	short offset;
	
	/* sanity checks - don't draw anything */
	if (ELEM3(NULL, acf, ale, block))
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
	/* ytext= y - 4; */
	
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
	if (ac->sl) {
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
	
	/* step 4) draw text - check if renaming widget is in use... */
	if (acf->name_prop && ac->ads) {
		float channel_height = ymaxc - yminc;
		
		/* if rename index matches, add widget for this */
		if (ac->ads->renameIndex == channel_index+1) {
			PointerRNA ptr;
			PropertyRNA *prop;
			
			/* draw renaming widget if we can get RNA pointer for it */
			if (acf->name_prop(ale, &ptr, &prop)) {
				uiBut *but;
				
				uiBlockSetEmboss(block, UI_EMBOSS);
				
				but = uiDefButR(block, TEX, 1, "", offset+3, yminc, RENAME_TEXT_WIDTH, channel_height,
				                &ptr, RNA_property_identifier(prop), -1, 0, 0, -1, -1, NULL);
				uiButSetFunc(but, achannel_setting_rename_done_cb, ac->ads, NULL);
				uiButActiveOnly(C, block, but);
				
				uiBlockSetEmboss(block, UI_EMBOSSN);
			}
		}
	}
	
	/* step 5) draw mute+protection toggles + (sliders) ....................... */
	/* reset offset - now goes from RHS of panel */
	offset = 0;
	
	// TODO: when drawing sliders, make those draw instead of these toggles if not enough space
	
	if (v2d) {
		short draw_sliders = 0;
		
		/* check if we need to show the sliders */
		if ((ac->sl) && ELEM(ac->spacetype, SPACE_ACTION, SPACE_IPO)) {
			switch (ac->spacetype) {
				case SPACE_ACTION:
				{
					SpaceAction *saction= (SpaceAction *)ac->sl;
					draw_sliders= (saction->flag & SACTION_SLIDERS);
				}
					break;
				case SPACE_IPO:
				{
					SpaceIpo *sipo= (SpaceIpo *)ac->sl;
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
		 *	  wherever they occurred. BUT, we don't use the layout engine, otherwise we'd get wrong alignment,
		 *	  and wouldn't be able to auto-keyframe...
		 *	- slider should start before the toggles (if they're visible) to keep a clean line down the side
		 */
		if ((draw_sliders) && ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_SHAPEKEY)) {
			/* adjust offset */
			// TODO: make slider width dynamic, so that they can be easier to use when the view is wide enough
			offset += SLIDER_WIDTH;
			
			/* need backdrop behind sliders... */
			uiBlockSetEmboss(block, UI_EMBOSS);
			
			if (ale->id) { /* Slider using RNA Access -------------------- */
				PointerRNA id_ptr, ptr;
				PropertyRNA *prop;
				char *rna_path = NULL;
				int array_index = 0;
				short free_path = 0;
				
				/* get destination info */
				if (ale->type == ANIMTYPE_FCURVE) {
					FCurve *fcu= (FCurve *)ale->data;
					
					rna_path= fcu->rna_path;
					array_index= fcu->array_index;
				}
				else if (ale->type == ANIMTYPE_SHAPEKEY) {
					KeyBlock *kb= (KeyBlock *)ale->data;
					Key *key= (Key *)ale->id;
					
					rna_path= key_get_curValue_rnaPath(key, kb);
					free_path= 1;
				}
				
				/* only if RNA-Path found */
				if (rna_path) {
					/* get RNA pointer, and resolve the path */
					RNA_id_pointer_create(ale->id, &id_ptr);
					
					/* try to resolve the path */
					if (RNA_path_resolve(&id_ptr, rna_path, &ptr, &prop)) {
						uiBut *but;
						
						/* create the slider button, and assign relevant callback to ensure keyframes are inserted... */
						but = uiDefAutoButR(block, &ptr, prop, array_index, "", ICON_NONE, (int)v2d->cur.xmax-offset, ymid, SLIDER_WIDTH, (int)ymaxc-yminc);
						
						/* assign keyframing function according to slider type */
						if (ale->type == ANIMTYPE_SHAPEKEY)
							uiButSetFunc(but, achannel_setting_slider_shapekey_cb, ale->id, ale->data);
						else
							uiButSetFunc(but, achannel_setting_slider_cb, ale->id, ale->data);
					}
					
					/* free the path if necessary */
					if (free_path)
						MEM_freeN(rna_path);
				}
			}
			else { /* Special Slider for stuff without RNA Access ---------- */
				// TODO: only implement this case when we really need it...
			}
		}
	}
}

/* *********************************************** */
