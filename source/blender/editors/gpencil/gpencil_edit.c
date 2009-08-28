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



/* ************************************************ */
