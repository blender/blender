/**
 * $Id: rna_object_api.c 21115 2009-06-23 19:17:59Z kazanbas $
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "DNA_object_types.h"

#ifdef RNA_RUNTIME

#include "BKE_scene.h"
#include "ED_object.h"

static void rna_Scene_add_object(Scene *sce, ReportList *reports, Object *ob)
{
	Base *base= object_in_scene(ob, sce);
	if (base) {
		BKE_report(reports, RPT_ERROR, "Object is already in this scene.");
		return;
	}
	base= scene_add_base(sce, ob);
	ob->id.us++;

	/* this is similar to what object_add_type and add_object do */
	ob->lay= base->lay= sce->lay;
	ob->recalc |= OB_RECALC;

	DAG_scene_sort(sce);
}

static void rna_Scene_remove_object(Scene *sce, ReportList *reports, Object *ob)
{
	Base *base= object_in_scene(ob, sce);
	if (!base) {
		BKE_report(reports, RPT_ERROR, "Object is not in this scene.");
		return;
	}
	/* as long as ED_base_object_free_and_unlink calls free_libblock_us, we don't have to decrement ob->id.us */
	ED_base_object_free_and_unlink(sce, base);
}

#else

void RNA_api_scene(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "add_object", "rna_Scene_add_object");
	RNA_def_function_ui_description(func, "Add object to scene.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm= RNA_def_pointer(func, "object", "Object", "", "Object to add to scene.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "remove_object", "rna_Scene_remove_object");
	RNA_def_function_ui_description(func, "Remove object from scene.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm= RNA_def_pointer(func, "object", "Object", "", "Object to remove from scene.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

#endif

