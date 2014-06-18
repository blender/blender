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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Joshua Leung, Arystanbek Dyussenov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_scene_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>

#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "RNA_define.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "rna_internal.h"  /* own include */

#ifdef RNA_RUNTIME

#include "BKE_animsys.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_scene.h"
#include "BKE_writeavi.h"

#include "ED_transform.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

static void rna_Scene_frame_set(Scene *scene, int frame, float subframe)
{
	double cfra = (double)frame + (double)subframe;

	CLAMP(cfra, MINAFRAME, MAXFRAME);
	BKE_scene_frame_set(scene, cfra);

#ifdef WITH_PYTHON
	BPy_BEGIN_ALLOW_THREADS;
#endif

	/* It's possible that here we're including layers which were never visible before. */
	BKE_scene_update_for_newframe_ex(G.main->eval_ctx, G.main, scene, (1 << 20) - 1, true);

#ifdef WITH_PYTHON
	BPy_END_ALLOW_THREADS;
#endif

	BKE_scene_camera_switch_update(scene);

	/* don't do notifier when we're rendering, avoid some viewport crashes
	 * redrawing while the data is being modified for render */
	if (!G.is_rendering) {
		/* cant use NC_SCENE|ND_FRAME because this causes wm_event_do_notifiers to call
		 * BKE_scene_update_for_newframe which will loose any un-keyed changes [#24690] */
		/* WM_main_add_notifier(NC_SCENE|ND_FRAME, scene); */
		
		/* instead just redraw the views */
		WM_main_add_notifier(NC_WINDOW, NULL);
	}
}

static void rna_Scene_update_tagged(Scene *scene)
{
#ifdef WITH_PYTHON
	BPy_BEGIN_ALLOW_THREADS;
#endif

	BKE_scene_update_tagged(G.main->eval_ctx, G.main, scene);

#ifdef WITH_PYTHON
	BPy_END_ALLOW_THREADS;
#endif
}

static void rna_SceneRender_get_frame_path(RenderData *rd, int frame, char *name)
{
	if (BKE_imtype_is_movie(rd->im_format.imtype))
		BKE_movie_filepath_get(name, rd);
	else
		BKE_makepicstring(name, rd->pic, G.main->name, (frame == INT_MIN) ? rd->cfra : frame,
		                  &rd->im_format, (rd->scemode & R_EXTENSION) != 0, true);
}

static void rna_Scene_ray_cast(Scene *scene, float ray_start[3], float ray_end[3],
                               int *r_success, Object **r_ob, float r_obmat[16],
                               float r_location[3], float r_normal[3])
{
	float dummy_dist_px = 0;
	float ray_nor[3];
	float ray_dist;

	sub_v3_v3v3(ray_nor, ray_end, ray_start);
	ray_dist = normalize_v3(ray_nor);

	if (snapObjectsRayEx(scene, NULL, NULL, NULL, NULL, SCE_SNAP_MODE_FACE,
	                     r_ob, (float(*)[4])r_obmat,
	                     ray_start, ray_nor, &ray_dist,
	                     NULL, &dummy_dist_px, r_location, r_normal, SNAP_ALL))
	{
		*r_success = true;
	}
	else {
		unit_m4((float(*)[4])r_obmat);
		zero_v3(r_location);
		zero_v3(r_normal);

		*r_success = false;
	}
}

#ifdef WITH_COLLADA
/* don't remove this, as COLLADA exporting cannot be done through operators in render() callback. */
#include "../../collada/collada.h"

static void rna_Scene_collada_export(
        Scene *scene,
        const char *filepath,
        int apply_modifiers,
        int export_mesh_type,

        int selected,
        int include_children,
        int include_armatures,
        int include_shapekeys,
        int deform_bones_only,

        int active_uv_only,
        int include_uv_textures,
        int include_material_textures,
        int use_texture_copies,

        int use_ngons,
        int use_object_instantiation,
        int sort_by_name,
        int export_transformation_type,
        int open_sim)
{
	collada_export(scene, filepath, apply_modifiers, export_mesh_type, selected,
	               include_children, include_armatures, include_shapekeys, deform_bones_only,
	               active_uv_only, include_uv_textures, include_material_textures,
	               use_texture_copies, use_ngons, use_object_instantiation, sort_by_name, export_transformation_type, open_sim);
}

#endif

#else

void RNA_api_scene(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "frame_set", "rna_Scene_frame_set");
	RNA_def_function_ui_description(func, "Set scene frame updating all objects immediately");
	parm = RNA_def_int(func, "frame", 0, MINAFRAME, MAXFRAME, "", "Frame number to set", MINAFRAME, MAXFRAME);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_float(func, "subframe", 0.0, 0.0, 1.0, "", "Sub-frame time, between 0.0 and 1.0", 0.0, 1.0);

	func = RNA_def_function(srna, "update", "rna_Scene_update_tagged");
	RNA_def_function_ui_description(func,
	                                "Update data tagged to be updated from previous access to data or operators");

	/* Ray Cast */
	func = RNA_def_function(srna, "ray_cast", "rna_Scene_ray_cast");
	RNA_def_function_ui_description(func, "Cast a ray onto in object space");

	/* ray start and end */
	parm = RNA_def_float_vector(func, "start", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_float_vector(func, "end", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* return location and normal */
	parm = RNA_def_boolean(func, "result", 0, "", "");
	RNA_def_function_output(func, parm);
	parm = RNA_def_pointer(func, "object", "Object", "", "Ray cast object");
	RNA_def_function_output(func, parm);
	parm = RNA_def_float_matrix(func, "matrix", 4, 4, NULL, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
	RNA_def_function_output(func, parm);
	parm = RNA_def_float_vector(func, "location", 3, NULL, -FLT_MAX, FLT_MAX, "Location",
	                            "The hit location of this ray cast", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);
	parm = RNA_def_float_vector(func, "normal", 3, NULL, -FLT_MAX, FLT_MAX, "Normal",
	                            "The face normal at the ray cast hit location", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);

#ifdef WITH_COLLADA
	/* don't remove this, as COLLADA exporting cannot be done through operators in render() callback. */
	func = RNA_def_function(srna, "collada_export", "rna_Scene_collada_export");
	parm = RNA_def_string(func, "filepath", NULL, FILE_MAX, "File Path", "File path to write Collada file");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_property_subtype(parm, PROP_FILEPATH); /* allow non utf8 */
	parm = RNA_def_boolean(func, "apply_modifiers", 0, "Apply Modifiers", "Apply modifiers");
	parm = RNA_def_int(func, "export_mesh_type", 0, INT_MIN, INT_MAX,
	            "Resolution", "Modifier resolution for export", INT_MIN, INT_MAX);
	parm = RNA_def_boolean(func, "selected", 0, "Selection Only", "Export only selected elements");
	parm = RNA_def_boolean(func, "include_children", 0, "Include Children", "Export all children of selected objects (even if not selected)");
	parm = RNA_def_boolean(func, "include_armatures", 0, "Include Armatures", "Export related armatures (even if not selected)");
	parm = RNA_def_boolean(func, "include_shapekeys", 0, "Include Shape Keys", "Export all Shape Keys from Mesh Objects");
	parm = RNA_def_boolean(func, "deform_bones_only", 0, "Deform Bones only", "Only export deforming bones with armatures");

	parm = RNA_def_boolean(func, "active_uv_only", 0, "Active UV Layer only", "Export only the active UV Layer");
	parm = RNA_def_boolean(func, "include_uv_textures", 0, "Include UV Textures", "Export textures assigned to the object UV maps");
	parm = RNA_def_boolean(func, "include_material_textures", 0, "Include Material Textures", "Export textures assigned to the object Materials");
	parm = RNA_def_boolean(func, "use_texture_copies", 0, "copy", "Copy textures to same folder where the .dae file is exported");

	parm = RNA_def_boolean(func, "use_ngons", 1, "Use NGons", "Keep NGons in Export");
	parm = RNA_def_boolean(func, "use_object_instantiation", 1, "Use Object Instances", "Instantiate multiple Objects from same Data");
	parm = RNA_def_boolean(func, "sort_by_name", 0, "Sort by Object name", "Sort exported data by Object name");
	parm = RNA_def_boolean(func, "open_sim", 0, "Export for SL/OpenSim", "Compatibility mode for SL, OpenSim and similar online worlds");

	parm = RNA_def_int(func, "export_transformation_type", 0, INT_MIN, INT_MAX,
	            "Transformation", "Transformation type for translation, scale and rotation", INT_MIN, INT_MAX);

	RNA_def_function_ui_description(func, "Export to collada file");
#endif
}


void RNA_api_scene_render(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "frame_path", "rna_SceneRender_get_frame_path");
	RNA_def_function_ui_description(func, "Return the absolute path to the filename to be written for a given frame");
	RNA_def_int(func, "frame", INT_MIN, INT_MIN, INT_MAX, "",
	            "Frame number to use, if unset the current frame will be used", MINAFRAME, MAXFRAME);
	parm = RNA_def_string_file_path(func, "filepath", NULL, FILE_MAX, "File Path",
	                                "The resulting filepath from the scenes render settings");
	RNA_def_property_flag(parm, PROP_THICK_WRAP); /* needed for string return value */
	RNA_def_function_output(func, parm);
}

#endif
