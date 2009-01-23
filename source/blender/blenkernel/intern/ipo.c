/* ipo.c
 * 
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): 2008,2009 Joshua Leung (IPO System cleanup, Animation System Recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* NOTE:
 *
 * This file is no longer used to provide tools for the depreceated IPO system. Instead, it
 * is only used to house the conversion code to the new system.
 *
 * -- Joshua Leung, Jan 2009
 */
 
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_dynstr.h"

#include "BKE_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_constraint.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h"



/* *************************************************** */
/* Old-Data Freeing Tools */

/* Free data from old IPO-Blocks (those which haven't been converted), but not IPO block itself */
// XXX this shouldn't be necessary anymore, but may occur while not all data is converted yet
void free_ipo (Ipo *ipo)
{
	IpoCurve *icu, *icn;
	int n= 0;
	
	for (icu= ipo->curve.first; icu; icu= icn) {
		icn= icu->next;
		n++;
		
		if (icu->bezt) MEM_freeN(icu->bezt);
		if (icu->bp) MEM_freeN(icu->bp);
		if (icu->driver) MEM_freeN(icu->driver);
		
		BLI_freelinkN(&ipo->curve, icu);
	}
	
	printf("Freed %d (Unconverted) Ipo-Curves from IPO '%s' \n", n, ipo->id.name+2);
}

/* *************************************************** */
/* ADRCODE to RNA-Path Conversion Code */

/* Object types */
static char *ob_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case OB_LOC_X:
			*array_index= 0; return "location";
		case OB_LOC_Y:
			*array_index= 1; return "location";
		case OB_LOC_Z:
			*array_index= 2; return "location";
		case OB_DLOC_X:
			*array_index= 0; return "delta_location";
		case OB_DLOC_Y:
			*array_index= 1; return "delta_location";
		case OB_DLOC_Z:
			*array_index= 2; return "delta_location";
		
		case OB_ROT_X:
			*array_index= 0; return "rotation";
		case OB_ROT_Y:
			*array_index= 1; return "rotation";
		case OB_ROT_Z:
			*array_index= 2; return "rotation";
		case OB_DROT_X:
			*array_index= 0; return "delta_rotation";
		case OB_DROT_Y:
			*array_index= 1; return "delta_rotation";
		case OB_DROT_Z:
			*array_index= 2; return "delta_rotation";
			
		case OB_SIZE_X:
			*array_index= 0; return "scale";
		case OB_SIZE_Y:
			*array_index= 1; return "scale";
		case OB_SIZE_Z:
			*array_index= 2; return "scale";
		case OB_DSIZE_X:
			*array_index= 0; return "delta_scale";
		case OB_DSIZE_Y:
			*array_index= 1; return "delta_scale";
		case OB_DSIZE_Z:
			*array_index= 2; return "delta_scale";
	
#if 0	
		case OB_LAY:	// XXX EVIL BITFLAG ALERT! this one will need special attention...
		//	poin= &(ob->lay); *type= IPO_INT_BIT; break;
			return NULL;
			
		case OB_COL_R:	
			poin= &(ob->col[0]); break;
		case OB_COL_G:
			poin= &(ob->col[1]); break;
		case OB_COL_B:
			poin= &(ob->col[2]); break;
		case OB_COL_A:
			poin= &(ob->col[3]); break;
			
		case OB_PD_FSTR:
			if (ob->pd) poin= &(ob->pd->f_strength);
			break;
		case OB_PD_FFALL:
			if (ob->pd) poin= &(ob->pd->f_power);
			break;
		case OB_PD_SDAMP:
			if (ob->pd) poin= &(ob->pd->pdef_damp);
			break;
		case OB_PD_RDAMP:
			if (ob->pd) poin= &(ob->pd->pdef_rdamp);
			break;
		case OB_PD_PERM:
			if (ob->pd) poin= &(ob->pd->pdef_perm);
			break;
		case OB_PD_FMAXD:
			if (ob->pd) poin= &(ob->pd->maxdist);
			break;
#endif
	}
	
	return NULL;
}

/* PoseChannel types 
 * NOTE: pchan name comes from 'actname' added earlier... 
 */
static char *pchan_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case AC_QUAT_W:
			*array_index= 0; return "rotation";
		case AC_QUAT_X:
			*array_index= 1; return "rotation";
		case AC_QUAT_Y:
			*array_index= 2; return "rotation";
		case AC_QUAT_Z:
			*array_index= 3; return "rotation";
			
#if 0 // XXX these were not 'official' channels (i.e. not in bf-releases)... these will need separate wrapping to work...
		case AC_EUL_X:
			*array_index= 0; return "rotation";
		case AC_EUL_Y:
			*array_index= 1; return "rotation";
		case AC_EUL_Z:
			*array_index= 2; return "rotation";
#endif 
		case -1: // XXX special case for rotation drivers... until eulers are added...
			*array_index= 0; return "rotation";
			
		case AC_LOC_X:
			*array_index= 0; return "location";
		case AC_LOC_Y:
			*array_index= 1; return "location";
		case AC_LOC_Z:
			*array_index= 2; return "location";
		
		case AC_SIZE_X:
			*array_index= 0; return "scale";
		case AC_SIZE_Y:
			*array_index= 1; return "scale";
		case AC_SIZE_Z:
			*array_index= 2; return "scale";
	}
	
	/* for debugging only */
	printf("ERROR: unmatched PoseChannel setting (code %d) \n", adrcode);
	return NULL;
}

/* ShapeKey types 
 * NOTE: as we don't have access to the keyblock where the data comes from (for now), 
 *	 	we'll just use numerical indicies for now... 
 */
static char *shapekey_adrcodes_to_paths (int adrcode, int *array_index)
{
	static char buf[128];
	
	/* block will be attached to ID_KE block, and setting that we alter is the 'value' (which sets keyblock.curval) */
	// XXX adrcode 0 was dummy 'speed' curve 
	sprintf(buf, "keys[%d].value", adrcode-1); // XXX this doesn't seem too safe...
	return buf;
}

/* ------- */

/* Allocate memory for RNA-path for some property given a blocktype, adrcode, and 'root' parts of path
 *	Input:
 *		- blocktype, adrcode	- determines setting to get
 *		- actname, constname	- used to build path
 *	Output:
 *		- array_index			- index in property's array (if applicable) to use
 *		- return				- the allocated path...
 */
char *get_rna_access (int blocktype, int adrcode, char actname[], char constname[], int *array_index)
{
	DynStr *path= BLI_dynstr_new();
	char *propname=NULL, *rpath=NULL;
	char buf[512];
	int dummy_index= 0;
	
	/* get property name based on blocktype */
	switch (blocktype) {
		case ID_OB: /* object */
			propname= ob_adrcodes_to_paths(adrcode, &dummy_index);
			break;
		
		case ID_PO: /* pose channel */
			propname= pchan_adrcodes_to_paths(adrcode, &dummy_index);
			break;
			
		case ID_KE: /* shapekeys */
			propname= shapekey_adrcodes_to_paths(adrcode, &dummy_index);
			break;
			
		/* XXX problematic blocktypes */
		case ID_CU:
			propname= "speed"; // XXX this was a 'dummy curve' that didn't really correspond to any real var...
			break;
			
		case ID_SEQ:
			//SEQ_FAC1:
			//	poin= &(seq->facf0); // XXX this doesn't seem to be included anywhere in sequencer RNA...
			break;
			
		/* special hacks */
		case -1:
			/* special case for rotdiff drivers... we don't need a property for this... */
			break;
			
		// TODO... add other blocktypes...
		default:
			printf("IPO2ANIMATO WARNING: No path for blocktype %d, adrcode %d yet \n", blocktype, adrcode);
			break;
	}
	
	/* check if any property found 
	 *	- blocktype < 0 is special case for a specific type of driver, where we don't need a property name...
	 */
	if ((propname == NULL) && (blocktype > 0)) {
		/* nothing was found, so exit */
		if (array_index) 
			*array_index= 0;
			
		BLI_dynstr_free(path);
		
		return NULL;
	}
	else {
		if (array_index)
			*array_index= dummy_index;
	}
	
	/* append preceeding bits to path */
	if ((actname && actname[0]) && (constname && constname[0])) {
		/* Constraint in Pose-Channel */
		sprintf(buf, "pose.pose_channels[\"%s\"].constraints[\"%s\"]", actname, constname);
	}
	else if (actname && actname[0]) {
		/* Pose-Channel */
		sprintf(buf, "pose.pose_channels[\"%s\"]", actname);
	}
	else if (constname && constname[0]) {
		/* Constraint in Object */
		sprintf(buf, "constraints[\"%s\"]", constname);
	}
	else
		strcpy(buf, ""); /* empty string */
	BLI_dynstr_append(path, buf);
	
	/* append property to path (only if applicable) */
	if (blocktype > 0) {
		/* need to add dot before property if there was anything precceding this */
		if (buf[0])
			BLI_dynstr_append(path, ".");
		
		/* now write name of property */
		BLI_dynstr_append(path, propname);
	}
	
	/* convert to normal MEM_malloc'd string */
	rpath= BLI_dynstr_get_cstring(path);
	BLI_dynstr_free(path);
	
	/* return path... */
	return rpath;
}

/* *************************************************** */
/* Conversion Utilities */

/* Convert IpoDriver to ChannelDriver - will free the old data (i.e. the old driver) */
static ChannelDriver *idriver_to_cdriver (IpoDriver *idriver)
{
	ChannelDriver *cdriver;
	
	/* allocate memory for new driver */
	cdriver= MEM_callocN(sizeof(ChannelDriver), "ChannelDriver");
	
	/* if 'pydriver', just copy data across */
	if (idriver->type == IPO_DRIVER_TYPE_PYTHON) {
		/* PyDriver only requires the expression to be copied */
		cdriver->type = DRIVER_TYPE_PYTHON;
		strcpy(cdriver->expression, idriver->name); // XXX is this safe? 
	}
	else {
		/* what to store depends on the 'blocktype' (ID_OB or ID_PO - object or posechannel) */
		if (idriver->blocktype == ID_AR) {
			/* ID_PO */
			if (idriver->adrcode == OB_ROT_DIFF) {
				/* Rotational Difference is a special type of driver now... */
				cdriver->type= DRIVER_TYPE_ROTDIFF;
				
				/* driver must use bones from same armature... */
				cdriver->id= cdriver->id2= (ID *)idriver->ob;
				
				/* paths for the two targets get the pointers to the relevant Pose-Channels 
				 *	- return pointers to Pose-Channels not rotation channels, as calculation code is picky
				 *	- old bone names were stored in same var, in idriver->name
				 *
				 *	- we use several hacks here - blocktype == -1 specifies that no property needs to be found, and
				 *	  providing a name for 'actname' will automatically imply Pose-Channel with name 'actname'
				 */
				cdriver->rna_path= get_rna_access(-1, -1, idriver->name, NULL, NULL);
				cdriver->rna_path2= get_rna_access(-1, -1, idriver->name+DRIVER_NAME_OFFS, NULL, NULL);
			}
			else {
				/* 'standard' driver */
				cdriver->type= DRIVER_TYPE_CHANNEL;
				cdriver->id= (ID *)idriver->ob;
				
				switch (idriver->adrcode) {
					case OB_LOC_X:	/* x,y,z location are quite straightforward */
						cdriver->rna_path= get_rna_access(ID_PO, AC_LOC_X, idriver->name, NULL, &cdriver->array_index);
						break;
					case OB_LOC_Y:
						cdriver->rna_path= get_rna_access(ID_PO, AC_LOC_Y, idriver->name, NULL, &cdriver->array_index);
						break;
					case OB_LOC_Z:
						cdriver->rna_path= get_rna_access(ID_PO, AC_LOC_Z, idriver->name, NULL, &cdriver->array_index);
						break;
						
					case OB_SIZE_X:	/* x,y,z scaling are also quite straightforward */
						cdriver->rna_path= get_rna_access(ID_PO, AC_SIZE_X, idriver->name, NULL, &cdriver->array_index);
						break;
					case OB_SIZE_Y:
						cdriver->rna_path= get_rna_access(ID_PO, AC_SIZE_Y, idriver->name, NULL, &cdriver->array_index);
						break;
					case OB_SIZE_Z:
						cdriver->rna_path= get_rna_access(ID_PO, AC_SIZE_Z, idriver->name, NULL, &cdriver->array_index);
						break;	
						
					case OB_ROT_X:	/* rotation - we need to be careful with this... XXX (another reason why we need eulers) */	
					case OB_ROT_Y:
					case OB_ROT_Z:
					{
						// XXX this is not yet a 1:1 map, since we'd need euler rotations to make this work nicely (unless we make some hacks)
						// XXX -1 here is a special hack...
						cdriver->rna_path= get_rna_access(ID_PO, -1, idriver->name, NULL, NULL);
						cdriver->array_index= idriver->adrcode - OB_ROT_X;
					}
						break;
				}
			}
		}
		else {
			/* ID_OB */
			cdriver->type= DRIVER_TYPE_CHANNEL;
			cdriver->id= (ID *)idriver->ob;
			cdriver->rna_path= get_rna_access(ID_OB, idriver->adrcode, idriver->name, NULL, &cdriver->array_index);
		}
	}
	
	/* free old driver */
	MEM_freeN(idriver);
	
	/* return the new one */
	return cdriver;
}

/* Convert IPO-Curve to F-Curve (including Driver data), and free any of the old data that 
 * is not relevant, BUT do not free the IPO-Curve itself...
 *	actname: name of Action-Channel (if applicable) that IPO-Curve's IPO-block belonged to
 *	constname: name of Constraint-Channel (if applicable) that IPO-Curve's IPO-block belonged to
 */
static FCurve *icu_to_fcu (IpoCurve *icu, char *actname, char *constname)
{
	FCurve *fcu;
	int i= 0;
	
	/* allocate memory for a new F-Curve */
	fcu= MEM_callocN(sizeof(FCurve), "FCurve");
	
	/* convert driver - will free the old one... */
	if (icu->driver) {
		fcu->driver= idriver_to_cdriver(icu->driver);
		icu->driver= NULL;
	}
	
	/* convert keyframes 
	 *	- beztriples and bpoints are mutually exclusive, so we won't have both at the same time
	 *	- beztriples are more likely to be encountered as they are keyframes (the other type wasn't used yet)
	 */
	// XXX we need to cope with the nasty old 'bitflag' curves... that will be a task for later
	// XXX we also need to correct values for object-rotation curves
	fcu->totvert= icu->totvert;
	
	if (icu->bezt) {
		BezTriple *dst, *src;
		
		/* allocate new array for keyframes/beztriples */
		fcu->bezt= MEM_callocN(sizeof(BezTriple)*fcu->totvert, "BezTriples");
		
		/* loop through copying all BezTriples individually, as we need to modify a few things */
		for (dst=fcu->bezt, src=icu->bezt; i < fcu->totvert; i++, dst++, src++) {
			/* firstly, copy BezTriple data */
			*dst= *src;
			
			/* now copy interpolation from curve (if not already set) */
			if (icu->ipo != IPO_MIXED)
				dst->ipo= icu->ipo;
				
			/* correct values for object rotation curves - they were degrees/10 */
			// XXX for now, just make them into degrees 
			if ((icu->blocktype == ID_OB) && ELEM3(icu->adrcode, OB_ROT_X, OB_ROT_Y, OB_ROT_Z)) {
				dst->vec[0][0] *= 10.0f;
				dst->vec[1][0] *= 10.0f;
				dst->vec[2][0] *= 10.0f;
			}
		}
		
		/* free this data now */
		MEM_freeN(icu->bezt);
	}
	else if (icu->bp) {
		/* TODO: need to convert from BPoint type to the more compact FPoint type... but not priority, since no data used this */
		//BPoint *bp;
		//FPoint *fpt;
	}
	
	/* get rna-path
	 *	- we will need to set the 'disabled' flag if no path is able to be made (for now)
	 */
	fcu->rna_path= get_rna_access(icu->blocktype, icu->adrcode, actname, constname, &fcu->array_index);
	if (fcu->rna_path == NULL)
		fcu->flag |= FCURVE_DISABLED;
	
	/* copy flags */
	if (icu->flag & IPO_VISIBLE) fcu->flag |= FCURVE_VISIBLE;
	if (icu->flag & IPO_SELECT) fcu->flag |= FCURVE_SELECTED;
	if (icu->flag & IPO_ACTIVE) fcu->flag |= FCURVE_ACTIVE;
	if (icu->flag & IPO_MUTE) fcu->flag |= FCURVE_MUTED;
	if (icu->flag & IPO_PROTECT) fcu->flag |= FCURVE_PROTECTED;
	if (icu->flag & IPO_AUTO_HORIZ) fcu->flag |= FCURVE_AUTO_HANDLES;
	
	/* set extrapolation */
	switch (icu->extrap) {
		case IPO_HORIZ: /* constant extrapolation */
		case IPO_DIR: /* linear extrapolation */
		{
			/* just copy, as the new defines match the old ones... */
			fcu->extend= icu->extrap;
		}
			break;
			
		case IPO_CYCL: /* cyclic extrapolation */
		case IPO_CYCLX: /* cyclic extrapolation + offset */
		{
			/* Add a new FModifier (Cyclic) instead of setting extend value 
			 * as that's the new equivilant of that option. 
			 */
			FModifier *fcm= fcurve_add_modifier(fcu, FMODIFIER_TYPE_CYCLES);
			FMod_Cycles *data= (FMod_Cycles *)fcm->data;
			
			/* if 'offset' one is in use, set appropriate settings */
			if (icu->extrap == IPO_CYCLX)
				data->before_mode= data->after_mode= FCM_EXTRAPOLATE_CYCLIC_OFFSET;
			else
				data->before_mode= data->after_mode= FCM_EXTRAPOLATE_CYCLIC;
		}
			break;
	}
	
	/* return new F-Curve */
	return fcu;
}

/* Convert IPO-block (i.e. all its IpoCurves) for some ID to the new system
 * This assumes that AnimData has been added already. Separation of drivers
 * from animation data is accomplished here too...
 */
static void ipo_to_animdata (ID *id, Ipo *ipo, char *actname, char *constname)
{
	AnimData *adt= BKE_animdata_from_id(id);
	//bActionGroup *grp;
	IpoCurve *icu, *icn;
	FCurve *fcu;
	
	/* sanity check */
	if ELEM(NULL, id, ipo)
		return;
	if (adt == NULL) {
		printf("ERROR ipo_to_animdata(): adt invalid \n");
		return;
	}
	
	printf("ipo to animdata - ID:%s, IPO:%s, actname:%s constname:%s  curves:%d \n", 
		id->name+2, ipo->id.name+2, (actname)?actname:"<None>", (constname)?constname:"<None>", 
		BLI_countlist(&ipo->curve));
	
	/* validate actname and constname 
	 *	- clear actname if it was one of the generic <builtin> ones (i.e. 'Object', or 'Shapes')
	 *	- actname can then be used to assign F-Curves in Action to Action Groups 
	 *	  (i.e. thus keeping the benefits that used to be provided by Action Channels for grouping
	 *		F-Curves for bones). This may be added later... for now let's just dump without them...
	 */
	if (actname) {
		if ((GS(id->name) == ID_OB) && (strcmp(actname, "Object") == 0))
			actname= NULL;
		if ((GS(id->name) == ID_OB) && (strcmp(actname, "Shape") == 0))
			actname= NULL;
	}
	
	/* loop over IPO-Curves, freeing as we progress */
	for (icu= ipo->curve.first; icu; icu= icn) {
		/* get link to next (for later) */
		icn= icu->next;
		
		/* convert IPO-Curve to F-Curve 
		 * NOTE: this frees any of the old data stored in the IPO-Curve that isn't needed anymore...
		 */
		// XXX we need to cope with the nasty old 'bitflag' curves... that will be a task for later
		//		we will need to create a few new curves when doing so, and will need to sift through the keyframes to add relevant data
		fcu= icu_to_fcu(icu, actname, constname);
		
		/* conversion path depends on whether it's a driver or not */
		if (fcu->driver == NULL) {
			/* try to get action */
			if (adt->action == NULL) {
				adt->action= add_empty_action("ConvertedAction"); // XXX we need a better name for this...
				printf("added new action \n");
			}
				
			/* add F-Curve to action */
			BLI_addtail(&adt->action->curves, fcu);
		}
		else {
			/* add F-Curve to AnimData's drivers */
			BLI_addtail(&adt->drivers, fcu);
		}
		
		/* free this IpoCurve now that it's been converted */
		BLI_freelinkN(&ipo->curve, icu);
	}
}

/* Convert Action-block to new system
 * NOTE: we need to be careful here, as same data-structs are used for new system too!
 */
static void action_to_animdata (ID *id, bAction *act)
{
	AnimData *adt= BKE_animdata_from_id(id);
	bActionChannel *achan, *achann;
	bConstraintChannel *conchan, *conchann;
	
	/* only continue if there are Action Channels (indicating unconverted data) */
	if (ELEM(NULL, adt, act->chanbase.first))
		return;
		
	/* get rid of all Action Groups */
	// XXX this is risky if there's some old, some new data in the Action...
	if (act->groups.first) 
		BLI_freelistN(&act->groups);
		
	/* check if we need to set this Action as the AnimData's action */
	if (adt->action == NULL) {
		/* set this Action as AnimData's Action */
		printf("act_to_adt - set adt action to act \n");
		adt->action= act;
	}
		
	/* loop through Action-Channels, converting data, freeing as we go */
	for (achan= act->chanbase.first; achan; achan= achann) {
		/* get pointer to next Action Channel */
		achann= achan->next;
		
		/* convert Action Channel's IPO data */
		if (achan->ipo) {
			ipo_to_animdata(id, achan->ipo, achan->name, NULL);
			achan->ipo->id.us--;
			achan->ipo= NULL;
		}
		
		/* convert constraint channel IPO-data */
		for (conchan= achan->constraintChannels.first; conchan; conchan= conchann) {
			/* get pointer to next Constraint Channel */
			conchann= conchan->next;
			
			/* convert Constraint Channel's IPO data */
			if (conchan->ipo) {
				ipo_to_animdata(id, conchan->ipo, achan->name, conchan->name);
				conchan->ipo->id.us--;
				conchan->ipo= NULL;
			}
			
			/* free Constraint Channel */
			BLI_freelinkN(&achan->constraintChannels, conchan);
		}
		
		/* free Action Channel */
		BLI_freelinkN(&act->chanbase, achan);
	}
}

/* *************************************************** */
/* External API - Only Called from do_versions() */

/* Called from do_versions() in readfile.c to convert the old 'IPO/adrcode' system
 * to the new 'Animato/RNA' system.
 *
 * The basic method used here, is to loop over datablocks which have IPO-data, and 
 * add those IPO's to new AnimData blocks as Actions. 
 * Action/NLA data only works well for Objects, so these only need to be checked for there.
 *  
 * Data that has been converted should be freed immediately, which means that it is immediately
 * clear which datablocks have yet to be converted, and also prevent freeing errors when we exit.
 */
// XXX currently done after all file reading... 
void do_versions_ipos_to_animato(Main *main)
{
	ID *id;
	AnimData *adt;
	
	if (main == NULL) {
		printf("Argh! Main is NULL in do_versions_ipos_to_animato() \n");
		return;
	}
		
	/* only convert if version is right */
	// XXX???
	if (main->versionfile >= 250) {
		printf("WARNING: Animation data too new to convert (Version %d) \n", main->versionfile);
		return;
	}
	else
		printf("INFO: Converting to Animato... \n"); // xxx debug
		
	
	/* objects */
	for (id= main->object.first; id; id= id->next) {
		Object *ob= (Object *)id;
		bPoseChannel *pchan;
		bConstraint *con;
		bConstraintChannel *conchan, *conchann;
		
		printf("\tconverting ob %s \n", id->name+2);
		
		/* check if object has any animation data */
		if ((ob->ipo) || (ob->action) || (ob->nlastrips.first)) {
			/* Add AnimData block */
			adt= BKE_id_add_animdata(id);
			
			/* IPO first */
			if (ob->ipo) {
				ipo_to_animdata(id, ob->ipo, NULL, NULL);
				ob->ipo->id.us--;
				ob->ipo= NULL;
			}
			
			/* now Action */
			if (ob->action) {
				action_to_animdata(id, ob->action);
				
				/* only decrease usercount if this Action isn't now being used by AnimData */
				if (ob->action != adt->action) {
					ob->action->id.us--;
					ob->action= NULL;
				}
			}
			
			/* finally NLA */
			// XXX todo... for now, new NLA code not hooked up yet, so keep old stuff (but not for too long!)
		}
		
		/* check PoseChannels for constraints with local data */
		if (ob->pose) {
			for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				for (con= pchan->constraints.first; con; con= con->next) {
					/* if constraint has own IPO, convert add these to Object 
					 * (NOTE: they're most likely to be drivers too) 
					 */
					 
					/* check for Action Constraint */
					// XXX do we really want to do this here?
				}
			}
		}
		
		/* check constraint channels - we need to remove them anyway... */
		for (conchan= ob->constraintChannels.first; conchan; conchan= conchann) {
			/* get pointer to next Constraint Channel */
			conchann= conchan->next;
			
			/* convert Constraint Channel's IPO data */
			if (conchan->ipo) {
				ipo_to_animdata(id, conchan->ipo, NULL, conchan->name);
				conchan->ipo->id.us--;
				conchan->ipo= NULL;
			}
			
			/* free Constraint Channel */
			BLI_freelinkN(&ob->constraintChannels, conchan);
		}
	}
	
	/* shapekeys */
	for (id= main->key.first; id; id= id->next) {
		Key *key= (Key *)id;
		
		printf("\tconverting key %s \n", id->name+2);
		
		/* we're only interested in the IPO 
		 * NOTE: for later, it might be good to port these over to Object instead, as many of these
		 * are likely to be drivers, but it's hard to trace that from here, so move this to Ob loop?
		 */
		if (key->ipo) {
			/* Add AnimData block */
			adt= BKE_id_add_animdata(id);
			
			/* Convert Shapekey data... */
			ipo_to_animdata(id, key->ipo, NULL, NULL);
			key->ipo->id.us--;
			key->ipo= NULL;
		}
	}
	
	// XXX add other types too...
	
	printf("INFO: animato convert done \n"); // xxx debug
}



#if 0 // XXX old animation system

/* ***************************** IPO - DataAPI ********************************* */

// !!!!!!!!!!!!!!!!!!!!!!!!!!!! FIXME - BAD CRUFT WARNING !!!!!!!!!!!!!!!!!!!!!!!

/* These functions here should be replaced eventually by the Data API, as this is 
 * inflexible duplication...
 */

/* --------------------- Get Pointer API ----------------------------- */ 

/* get texture channel */
static void *give_tex_poin (Tex *tex, int adrcode, int *type )
{
	void *poin= NULL;

	switch (adrcode) {
	case TE_NSIZE:
		poin= &(tex->noisesize); break;
	case TE_TURB:
		poin= &(tex->turbul); break;
	case TE_NDEPTH:
		poin= &(tex->noisedepth); *type= IPO_SHORT; break;
	case TE_NTYPE:
		poin= &(tex->noisetype); *type= IPO_SHORT; break;
	case TE_VNW1:
		poin= &(tex->vn_w1); break;
	case TE_VNW2:
		poin= &(tex->vn_w2); break;
	case TE_VNW3:
		poin= &(tex->vn_w3); break;
	case TE_VNW4:
		poin= &(tex->vn_w4); break;
	case TE_VNMEXP:
		poin= &(tex->vn_mexp); break;
	case TE_ISCA:
		poin= &(tex->ns_outscale); break;
	case TE_DISTA:
		poin= &(tex->dist_amount); break;
	case TE_VN_COLT:
		poin= &(tex->vn_coltype); *type= IPO_SHORT; break;
	case TE_VN_DISTM:
		poin= &(tex->vn_distm); *type= IPO_SHORT; break;
	case TE_MG_TYP:
		poin= &(tex->stype); *type= IPO_SHORT; break;
	case TE_MGH:
		poin= &(tex->mg_H); break;
	case TE_MG_LAC:
		poin= &(tex->mg_lacunarity); break;
	case TE_MG_OCT:
		poin= &(tex->mg_octaves); break;
	case TE_MG_OFF:
		poin= &(tex->mg_offset); break;
	case TE_MG_GAIN:
		poin= &(tex->mg_gain); break;
	case TE_N_BAS1:
		poin= &(tex->noisebasis); *type= IPO_SHORT; break;
	case TE_N_BAS2:
		poin= &(tex->noisebasis2); *type= IPO_SHORT; break;
	case TE_COL_R:
		poin= &(tex->rfac); break;
	case TE_COL_G:
		poin= &(tex->gfac); break;
	case TE_COL_B:
		poin= &(tex->bfac); break;
	case TE_BRIGHT:
		poin= &(tex->bright); break;
	case TE_CONTRA:
		poin= &(tex->contrast); break;
	}
	
	/* return pointer */
	return poin;
}

/* get texture-slot/mapping channel */
void *give_mtex_poin (MTex *mtex, int adrcode)
{
	void *poin= NULL;
	
	switch (adrcode) {
	case MAP_OFS_X:
		poin= &(mtex->ofs[0]); break;
	case MAP_OFS_Y:
		poin= &(mtex->ofs[1]); break;
	case MAP_OFS_Z:
		poin= &(mtex->ofs[2]); break;
	case MAP_SIZE_X:
		poin= &(mtex->size[0]); break;
	case MAP_SIZE_Y:
		poin= &(mtex->size[1]); break;
	case MAP_SIZE_Z:
		poin= &(mtex->size[2]); break;
	case MAP_R:
		poin= &(mtex->r); break;
	case MAP_G:
		poin= &(mtex->g); break;
	case MAP_B:
		poin= &(mtex->b); break;
	case MAP_DVAR:
		poin= &(mtex->def_var); break;
	case MAP_COLF:
		poin= &(mtex->colfac); break;
	case MAP_NORF:
		poin= &(mtex->norfac); break;
	case MAP_VARF:
		poin= &(mtex->varfac); break;
	case MAP_DISP:
		poin= &(mtex->dispfac); break;
	}
	
	/* return pointer */
	return poin;
}

/* GS reads the memory pointed at in a specific ordering. There are,
 * however two definitions for it. I have jotted them down here, both,
 * but I think the first one is actually used. The thing is that
 * big-endian systems might read this the wrong way round. OTOH, we
 * constructed the IDs that are read out with this macro explicitly as
 * well. I expect we'll sort it out soon... */

/* from blendef: */
#define GS(a)	(*((short *)(a)))

/* from misc_util: flip the bytes from x  */
/*  #define GS(x) (((unsigned char *)(x))[0] << 8 | ((unsigned char *)(x))[1]) */


/* general function to get pointer to source/destination data  */
void *get_ipo_poin (ID *id, IpoCurve *icu, int *type)
{
	void *poin= NULL;
	MTex *mtex= NULL;

	/* most channels will have float data, but those with other types will override this */
	*type= IPO_FLOAT;

	/* data is divided into 'blocktypes' based on ID-codes */
	switch (GS(id->name)) {
		case ID_MA: /* material channels -----------------------------  */
		{
			Material *ma= (Material *)id;
			
			switch (icu->adrcode) {
			case MA_COL_R:
				poin= &(ma->r); break;
			case MA_COL_G:
				poin= &(ma->g); break;
			case MA_COL_B:
				poin= &(ma->b); break;
			case MA_SPEC_R:
				poin= &(ma->specr); break;
			case MA_SPEC_G:
				poin= &(ma->specg); break;
			case MA_SPEC_B:
				poin= &(ma->specb); break;
			case MA_MIR_R:
				poin= &(ma->mirr); break;
			case MA_MIR_G:
				poin= &(ma->mirg); break;
			case MA_MIR_B:
				poin= &(ma->mirb); break;
			case MA_REF:
				poin= &(ma->ref); break;
			case MA_ALPHA:
				poin= &(ma->alpha); break;
			case MA_EMIT:
				poin= &(ma->emit); break;
			case MA_AMB:
				poin= &(ma->amb); break;
			case MA_SPEC:
				poin= &(ma->spec); break;
			case MA_HARD:
				poin= &(ma->har); *type= IPO_SHORT; break;
			case MA_SPTR:
				poin= &(ma->spectra); break;
			case MA_IOR:
				poin= &(ma->ang); break;
			case MA_MODE:
				poin= &(ma->mode); *type= IPO_INT_BIT; break; // evil... dumping bitflags directly to user!
			case MA_HASIZE:
				poin= &(ma->hasize); break;
			case MA_TRANSLU:
				poin= &(ma->translucency); break;
			case MA_RAYM:
				poin= &(ma->ray_mirror); break;
			case MA_FRESMIR:
				poin= &(ma->fresnel_mir); break;
			case MA_FRESMIRI:
				poin= &(ma->fresnel_mir_i); break;
			case MA_FRESTRA:
				poin= &(ma->fresnel_tra); break;
			case MA_FRESTRAI:
				poin= &(ma->fresnel_tra_i); break;
			case MA_ADD:
				poin= &(ma->add); break;
			}
			
			if (poin == NULL) {
				if (icu->adrcode & MA_MAP1) mtex= ma->mtex[0];
				else if (icu->adrcode & MA_MAP2) mtex= ma->mtex[1];
				else if (icu->adrcode & MA_MAP3) mtex= ma->mtex[2];
				else if (icu->adrcode & MA_MAP4) mtex= ma->mtex[3];
				else if (icu->adrcode & MA_MAP5) mtex= ma->mtex[4];
				else if (icu->adrcode & MA_MAP6) mtex= ma->mtex[5];
				else if (icu->adrcode & MA_MAP7) mtex= ma->mtex[6];
				else if (icu->adrcode & MA_MAP8) mtex= ma->mtex[7];
				else if (icu->adrcode & MA_MAP9) mtex= ma->mtex[8];
				else if (icu->adrcode & MA_MAP10) mtex= ma->mtex[9];
				else if (icu->adrcode & MA_MAP12) mtex= ma->mtex[11];
				else if (icu->adrcode & MA_MAP11) mtex= ma->mtex[10];
				else if (icu->adrcode & MA_MAP13) mtex= ma->mtex[12];
				else if (icu->adrcode & MA_MAP14) mtex= ma->mtex[13];
				else if (icu->adrcode & MA_MAP15) mtex= ma->mtex[14];
				else if (icu->adrcode & MA_MAP16) mtex= ma->mtex[15];
				else if (icu->adrcode & MA_MAP17) mtex= ma->mtex[16];
				else if (icu->adrcode & MA_MAP18) mtex= ma->mtex[17];
				
				if (mtex)
					poin= give_mtex_poin(mtex, (icu->adrcode & (MA_MAP1-1)));
			}
		}
			break;
		case ID_TE: /* texture channels -----------------------------  */
		{
			Tex *tex= (Tex *)id;
			
			if (tex) 
				poin= give_tex_poin(tex, icu->adrcode, type);
		}
			break;
		case ID_WO: /* world channels -----------------------------  */
		{
			World *wo= (World *)id;
			
			switch (icu->adrcode) {
			case WO_HOR_R:
				poin= &(wo->horr); break;
			case WO_HOR_G:
				poin= &(wo->horg); break;
			case WO_HOR_B:
				poin= &(wo->horb); break;
			case WO_ZEN_R:
				poin= &(wo->zenr); break;
			case WO_ZEN_G:
				poin= &(wo->zeng); break;
			case WO_ZEN_B:
				poin= &(wo->zenb); break;
			
			case WO_EXPOS:
				poin= &(wo->exposure); break;
			
			case WO_MISI:
				poin= &(wo->misi); break;
			case WO_MISTDI:
				poin= &(wo->mistdist); break;
			case WO_MISTSTA:
				poin= &(wo->miststa); break;
			case WO_MISTHI:
				poin= &(wo->misthi); break;
			
			case WO_STAR_R:
				poin= &(wo->starr); break;
			case WO_STAR_G:
				poin= &(wo->starg); break;
			case WO_STAR_B:
				poin= &(wo->starb); break;
			
			case WO_STARDIST:
				poin= &(wo->stardist); break;
			case WO_STARSIZE:
				poin= &(wo->starsize); break;
			}
			
			if (poin == NULL) {
				if (icu->adrcode & MA_MAP1) mtex= wo->mtex[0];
				else if (icu->adrcode & MA_MAP2) mtex= wo->mtex[1];
				else if (icu->adrcode & MA_MAP3) mtex= wo->mtex[2];
				else if (icu->adrcode & MA_MAP4) mtex= wo->mtex[3];
				else if (icu->adrcode & MA_MAP5) mtex= wo->mtex[4];
				else if (icu->adrcode & MA_MAP6) mtex= wo->mtex[5];
				else if (icu->adrcode & MA_MAP7) mtex= wo->mtex[6];
				else if (icu->adrcode & MA_MAP8) mtex= wo->mtex[7];
				else if (icu->adrcode & MA_MAP9) mtex= wo->mtex[8];
				else if (icu->adrcode & MA_MAP10) mtex= wo->mtex[9];
				else if (icu->adrcode & MA_MAP11) mtex= wo->mtex[10];
				else if (icu->adrcode & MA_MAP12) mtex= wo->mtex[11];
				else if (icu->adrcode & MA_MAP13) mtex= wo->mtex[12];
				else if (icu->adrcode & MA_MAP14) mtex= wo->mtex[13];
				else if (icu->adrcode & MA_MAP15) mtex= wo->mtex[14];
				else if (icu->adrcode & MA_MAP16) mtex= wo->mtex[15];
				else if (icu->adrcode & MA_MAP17) mtex= wo->mtex[16];
				else if (icu->adrcode & MA_MAP18) mtex= wo->mtex[17];
				
				if (mtex)
					poin= give_mtex_poin(mtex, (icu->adrcode & (MA_MAP1-1)));
			}
		}
			break;
		case ID_LA: /* lamp channels -----------------------------  */
		{
			Lamp *la= (Lamp *)id;
			
			switch (icu->adrcode) {
			case LA_ENERGY:
				poin= &(la->energy); break;		
			case LA_COL_R:
				poin= &(la->r); break;
			case LA_COL_G:
				poin= &(la->g); break;
			case LA_COL_B:
				poin= &(la->b); break;
			case LA_DIST:
				poin= &(la->dist); break;		
			case LA_SPOTSI:
				poin= &(la->spotsize); break;
			case LA_SPOTBL:
				poin= &(la->spotblend); break;
			case LA_QUAD1:
				poin= &(la->att1); break;
			case LA_QUAD2:
				poin= &(la->att2); break;
			case LA_HALOINT:
				poin= &(la->haint); break;
			}
			
			if (poin == NULL) {
				if (icu->adrcode & MA_MAP1) mtex= la->mtex[0];
				else if (icu->adrcode & MA_MAP2) mtex= la->mtex[1];
				else if (icu->adrcode & MA_MAP3) mtex= la->mtex[2];
				else if (icu->adrcode & MA_MAP4) mtex= la->mtex[3];
				else if (icu->adrcode & MA_MAP5) mtex= la->mtex[4];
				else if (icu->adrcode & MA_MAP6) mtex= la->mtex[5];
				else if (icu->adrcode & MA_MAP7) mtex= la->mtex[6];
				else if (icu->adrcode & MA_MAP8) mtex= la->mtex[7];
				else if (icu->adrcode & MA_MAP9) mtex= la->mtex[8];
				else if (icu->adrcode & MA_MAP10) mtex= la->mtex[9];
				else if (icu->adrcode & MA_MAP11) mtex= la->mtex[10];
				else if (icu->adrcode & MA_MAP12) mtex= la->mtex[11];
				else if (icu->adrcode & MA_MAP13) mtex= la->mtex[12];
				else if (icu->adrcode & MA_MAP14) mtex= la->mtex[13];
				else if (icu->adrcode & MA_MAP15) mtex= la->mtex[14];
				else if (icu->adrcode & MA_MAP16) mtex= la->mtex[15];
				else if (icu->adrcode & MA_MAP17) mtex= la->mtex[16];
				else if (icu->adrcode & MA_MAP18) mtex= la->mtex[17];
				
				if (mtex)
					poin= give_mtex_poin(mtex, (icu->adrcode & (MA_MAP1-1)));
			}
		}
			break;
		case ID_CA: /* camera channels -----------------------------  */
		{
			Camera *ca= (Camera *)id;
			
			switch (icu->adrcode) {
			case CAM_LENS:
				if (ca->type == CAM_ORTHO)
					poin= &(ca->ortho_scale);
				else
					poin= &(ca->lens); 
				break;
			case CAM_STA:
				poin= &(ca->clipsta); break;
			case CAM_END:
				poin= &(ca->clipend); break;
				
			case CAM_YF_APERT:
				poin= &(ca->YF_aperture); break;
			case CAM_YF_FDIST:
				poin= &(ca->YF_dofdist); break;
				
			case CAM_SHIFT_X:
				poin= &(ca->shiftx); break;
			case CAM_SHIFT_Y:
				poin= &(ca->shifty); break;
			}
		}
			break;
		case ID_SO: /* sound channels -----------------------------  */
		{
			bSound *snd= (bSound *)id;
			
			switch (icu->adrcode) {
			case SND_VOLUME:
				poin= &(snd->volume); break;
			case SND_PITCH:
				poin= &(snd->pitch); break;
			case SND_PANNING:
				poin= &(snd->panning); break;
			case SND_ATTEN:
				poin= &(snd->attenuation); break;
			}
		}
			break;
		case ID_PA: /* particle channels -----------------------------  */
		{
			ParticleSettings *part= (ParticleSettings *)id;
			
			switch (icu->adrcode) {
			case PART_EMIT_FREQ:
			case PART_EMIT_LIFE:
			case PART_EMIT_VEL:
			case PART_EMIT_AVE:
			case PART_EMIT_SIZE:
				poin= NULL; 
				break;
			
			case PART_CLUMP:
				poin= &(part->clumpfac); break;
			case PART_AVE:
				poin= &(part->avefac); break;
			case PART_SIZE:
				poin= &(part->size); break;
			case PART_DRAG:
				poin= &(part->dragfac); break;
			case PART_BROWN:
				poin= &(part->brownfac); break;
			case PART_DAMP:
				poin= &(part->dampfac); break;
			case PART_LENGTH:
				poin= &(part->length); break;
			case PART_GRAV_X:
				poin= &(part->acc[0]); break;
			case PART_GRAV_Y:
				poin= &(part->acc[1]); break;
			case PART_GRAV_Z:
				poin= &(part->acc[2]); break;
			case PART_KINK_AMP:
				poin= &(part->kink_amp); break;
			case PART_KINK_FREQ:
				poin= &(part->kink_freq); break;
			case PART_KINK_SHAPE:
				poin= &(part->kink_shape); break;
			case PART_BB_TILT:
				poin= &(part->bb_tilt); break;
				
			case PART_PD_FSTR:
				if (part->pd) poin= &(part->pd->f_strength);
				break;
			case PART_PD_FFALL:
				if (part->pd) poin= &(part->pd->f_power);
				break;
			case PART_PD_FMAXD:
				if (part->pd) poin= &(part->pd->maxdist);
				break;
			case PART_PD2_FSTR:
				if (part->pd2) poin= &(part->pd2->f_strength);
				break;
			case PART_PD2_FFALL:
				if (part->pd2) poin= &(part->pd2->f_power);
				break;
			case PART_PD2_FMAXD:
				if (part->pd2) poin= &(part->pd2->maxdist);
				break;
			}
		}
			break;
	}

	/* return pointer */
	return poin;
}


#endif // XXX old animation system
