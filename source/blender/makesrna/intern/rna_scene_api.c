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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung, Arystanbek Dyussenov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "BKE_utildefines.h"

#ifdef RNA_RUNTIME

#include "BKE_animsys.h"
#include "BKE_scene.h"
#include "BKE_image.h"
#include "BKE_depsgraph.h"
#include "BKE_writeavi.h"



static void rna_Scene_set_frame(Scene *scene, bContext *C, int frame)
{
	scene->r.cfra= frame;
	CLAMP(scene->r.cfra, MINAFRAME, MAXFRAME);
	scene_update_for_newframe(scene, (1<<20) - 1);

	WM_main_add_notifier(NC_SCENE|ND_FRAME, scene);
}

static KeyingSet *rna_Scene_add_keying_set(Scene *sce, ReportList *reports, 
		char name[], int absolute, int insertkey_needed, int insertkey_visual)
{
	KeyingSet *ks= NULL;
	short flag=0, keyingflag=0;
	
	/* validate flags */
	if (absolute)
		flag |= KEYINGSET_ABSOLUTE;
	if (insertkey_needed)
		keyingflag |= INSERTKEY_NEEDED;
	if (insertkey_visual)
		keyingflag |= INSERTKEY_MATRIX;
		
	/* call the API func, and set the active keyingset index */
	ks= BKE_keyingset_add(&sce->keyingsets, name, flag, keyingflag);
	
	if (ks) {
		sce->active_keyingset= BLI_countlist(&sce->keyingsets);
		return ks;
	}
	else {
		BKE_report(reports, RPT_ERROR, "Keying Set could not be added.");
		return NULL;
	}
}

static void rna_SceneRender_get_frame_path(RenderData *rd, int frame, char *name)
{
	if(BKE_imtype_is_movie(rd->imtype))
		BKE_makeanimstring(name, rd);
	else
		BKE_makepicstring(name, rd->pic, (frame==INT_MIN) ? rd->cfra : frame, rd->imtype, rd->scemode & R_EXTENSION);
}

#else

void RNA_api_scene(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "set_frame", "rna_Scene_set_frame");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Set scene frame updating all objects immediately.");
	parm= RNA_def_int(func, "frame", 0, MINAFRAME, MAXFRAME, "", "Frame number to set.", MINAFRAME, MAXFRAME);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "update", "scene_update_tagged");
	RNA_def_function_ui_description(func, "Update data tagged to be updated from previous access to data or operators.");

	/* Add Keying Set */
	func= RNA_def_function(srna, "add_keying_set", "rna_Scene_add_keying_set");
	RNA_def_function_ui_description(func, "Add a new Keying Set to Scene.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* returns the new KeyingSet */
	parm= RNA_def_pointer(func, "keyingset", "KeyingSet", "", "Newly created Keying Set.");
	RNA_def_function_return(func, parm);
	/* name */
	RNA_def_string(func, "name", "KeyingSet", 64, "Name", "Name of Keying Set");
	/* flags */
	RNA_def_boolean(func, "absolute", 1, "Absolute", "Keying Set defines specific paths/settings to be keyframed (i.e. is not reliant on context info)");
	/* keying flags */
	RNA_def_boolean(func, "insertkey_needed", 0, "Insert Keyframes - Only Needed", "Only insert keyframes where they're needed in the relevant F-Curves.");
	RNA_def_boolean(func, "insertkey_visual", 0, "Insert Keyframes - Visual", "Insert keyframes based on 'visual transforms'.");
}

void RNA_api_scene_render(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "frame_path", "rna_SceneRender_get_frame_path");
	RNA_def_function_ui_description(func, "Set scene frame updating all objects immediately.");
	parm= RNA_def_int(func, "frame", INT_MIN, INT_MIN, INT_MAX, "", "Frame number to use, if unset the current frame will be used.", MINAFRAME, MAXFRAME);
	parm= RNA_def_string(func, "name", "", FILE_MAX, "File Name", "the resulting filename from the scenes render settings.");
	RNA_def_property_flag(parm, PROP_THICK_WRAP); /* needed for string return value */
	RNA_def_function_output(func, parm);
}

#endif

