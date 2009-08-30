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
 * The Original Code is Copyright (C) 2008, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "DNA_listBase.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"

#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "UI_view2d.h"

#include "ED_armature.h"
#include "ED_gpencil.h"
#include "ED_sequencer.h"
#include "ED_view3d.h"

#include "gpencil_intern.h"

/* ************************************************ */
/* Context Wrangling... */

/* Get pointer to active Grease Pencil datablock, and an RNA-pointer to trace back to whatever owns it */
bGPdata **gpencil_data_get_pointers (bContext *C, PointerRNA *ptr)
{
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= CTX_wm_area(C);
	
	/* if there's an active area, check if the particular editor may
	 * have defined any special Grease Pencil context for editing...
	 */
	if (sa) {
		switch (sa->spacetype) {
			case SPACE_VIEW3D: /* 3D-View */
			{
				Object *ob= CTX_data_active_object(C);
				
				// TODO: we can include other data-types such as bones later if need be...
				
				/* just in case no active object */
				if (ob) {
					/* for now, as long as there's an object, default to using that in 3D-View */
					if (ptr) RNA_id_pointer_create(&ob->id, ptr);
					return &ob->gpd;
				}
			}
				break;
			
			case SPACE_NODE: /* Nodes Editor */
			{
				//SpaceNode *snode= (SpaceNode *)CTX_wm_space_data(C);
				
				/* return the GP data for the active node block/node */
			}
				break;
				
			case SPACE_SEQ: /* Sequencer */
			{
				//SpaceSeq *sseq= (SpaceSeq *)CTX_wm_space_data(C);
				
				/* return the GP data for the active strips/image/etc. */
			}
				break;
				
			case SPACE_IMAGE: /* Image/UV Editor */
			{
				SpaceImage *sima= (SpaceImage *)CTX_wm_space_data(C);
				
				/* for now, Grease Pencil data is associated with the space... */
				// XXX our convention for everything else is to link to data though...
				if (ptr) RNA_pointer_create((ID *)CTX_wm_screen(C), &RNA_SpaceImageEditor, sima, ptr);
				return &sima->gpd;
			}
				break;
				
			default: /* unsupported space */
				return NULL;
		}
	}
	
	/* just fall back on the scene's GP data */
	if (ptr) RNA_id_pointer_create((ID *)scene, ptr);
	return (scene) ? &scene->gpd : NULL;
}

/* Get the active Grease Pencil datablock */
bGPdata *gpencil_data_get_active (bContext *C)
{
	bGPdata **gpd_ptr= gpencil_data_get_pointers(C, NULL);
	return (gpd_ptr) ? *(gpd_ptr) : NULL;
}

/* ************************************************ */
/* Panel Operators */

/* poll callback for adding data/layers - special */
static int gp_add_poll (bContext *C)
{
	/* the base line we have is that we have somewhere to add Grease Pencil data */
	return gpencil_data_get_pointers(C, NULL) != NULL;
}

/* ******************* Add New Data ************************ */

/* add new datablock - wrapper around API */
static int gp_data_add_exec (bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr= gpencil_data_get_pointers(C, NULL);
	
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for Grease Pencil data to go");
		return OPERATOR_CANCELLED;
	}
	else {
		/* just add new datablock now */
		*gpd_ptr= gpencil_data_addnew("GPencil");
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_SCREEN|ND_GPENCIL|NA_EDITED, NULL); // XXX need a nicer one that will work	
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_data_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Grease Pencil Add New";
	ot->idname= "GPENCIL_OT_data_add";
	ot->description= "Add new Grease Pencil datablock.";
	
	/* callbacks */
	ot->exec= gp_data_add_exec;
	ot->poll= gp_add_poll;
}

/* ******************* Unlink Data ************************ */

/* poll callback for adding data/layers - special */
static int gp_data_unlink_poll (bContext *C)
{
	bGPdata **gpd_ptr= gpencil_data_get_pointers(C, NULL);
	
	/* if we have access to some active data, make sure there's a datablock before enabling this */
	return (gpd_ptr && *gpd_ptr);
}


/* unlink datablock - wrapper around API */
static int gp_data_unlink_exec (bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr= gpencil_data_get_pointers(C, NULL);
	
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for Grease Pencil data to go");
		return OPERATOR_CANCELLED;
	}
	else {
		/* just unlink datablock now, decreasing its user count */
		bGPdata *gpd= (*gpd_ptr);
		
		gpd->id.us--;
		*gpd_ptr= NULL;
	}
	
	/* notifiers */
	WM_event_add_notifier(C, NC_SCREEN|ND_GPENCIL|NA_EDITED, NULL); // XXX need a nicer one that will work	
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_data_unlink (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Grease Pencil Unlink";
	ot->idname= "GPENCIL_OT_data_unlink";
	ot->description= "Unlink active Grease Pencil datablock.";
	
	/* callbacks */
	ot->exec= gp_data_unlink_exec;
	ot->poll= gp_data_unlink_poll;
}

/* ******************* Add New Layer ************************ */

/* add new layer - wrapper around API */
static int gp_layer_add_exec (bContext *C, wmOperator *op)
{
	bGPdata **gpd_ptr= gpencil_data_get_pointers(C, NULL);
	
	/* if there's no existing Grease-Pencil data there, add some */
	if (gpd_ptr == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Nowhere for Grease Pencil data to go");
		return OPERATOR_CANCELLED;
	}
	if (*gpd_ptr == NULL)
		*gpd_ptr= gpencil_data_addnew("GPencil");
		
	/* add new layer now */
	gpencil_layer_addnew(*gpd_ptr);
	
	/* notifiers */
	WM_event_add_notifier(C, NC_SCREEN|ND_GPENCIL|NA_EDITED, NULL); // XXX please work!
	
	return OPERATOR_FINISHED;
}

void GPENCIL_OT_layer_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add New Layer";
	ot->idname= "GPENCIL_OT_layer_add";
	ot->description= "Add new Grease Pencil layer for the active Grease Pencil datablock.";
	
	/* callbacks */
	ot->exec= gp_layer_add_exec;
	ot->poll= gp_add_poll;
}

/* ************************************************ */
