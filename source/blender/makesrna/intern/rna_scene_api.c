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
#include "BLI_kdopbvh.h"
#include "BLI_path_util.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "rna_internal.h"  /* own include */

#ifdef WITH_ALEMBIC
#  include "../../alembic/ABC_alembic.h"
#endif

const EnumPropertyItem rna_enum_abc_compression_items[] = {
#ifdef WITH_ALEMBIC
	{ ABC_ARCHIVE_OGAWA, "OGAWA", 0, "Ogawa", "" },
	{ ABC_ARCHIVE_HDF5, "HDF5", 0, "HDF5", "" },
#endif
	{ 0, NULL, 0, NULL, NULL }
};

#ifdef RNA_RUNTIME

#include "BKE_animsys.h"
#include "BKE_depsgraph.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_scene.h"
#include "BKE_writeavi.h"

#include "ED_transform.h"
#include "ED_transform_snap_object_context.h"
#include "ED_uvedit.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

static void rna_Scene_frame_set(Scene *scene, Main *bmain, int frame, float subframe)
{
	double cfra = (double)frame + (double)subframe;

	CLAMP(cfra, MINAFRAME, MAXFRAME);
	BKE_scene_frame_set(scene, cfra);

#ifdef WITH_PYTHON
	BPy_BEGIN_ALLOW_THREADS;
#endif

	/* It's possible that here we're including layers which were never visible before. */
	BKE_scene_update_for_newframe_ex(bmain->eval_ctx, bmain, scene, (1 << 20) - 1, true);

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

static void rna_Scene_uvedit_aspect(Scene *scene, Object *ob, float *aspect)
{
	if ((ob->type == OB_MESH) && (ob->mode == OB_MODE_EDIT)) {
		BMEditMesh *em;
		em = BKE_editmesh_from_object(ob);
		if (EDBM_mtexpoly_check(em)) {
			ED_uvedit_get_aspect(scene, ob, em->bm, aspect, aspect + 1);
			return;
		}
	}

	aspect[0] = aspect[1] = 1.0f;
}

static void rna_Scene_update_tagged(Scene *scene, Main *bmain)
{
#ifdef WITH_PYTHON
	BPy_BEGIN_ALLOW_THREADS;
#endif

	BKE_scene_update_tagged(bmain->eval_ctx, bmain, scene);

#ifdef WITH_PYTHON
	BPy_END_ALLOW_THREADS;
#endif
}

static void rna_SceneRender_get_frame_path(RenderData *rd, int frame, int preview, const char *view, char *name)
{
	const char *suffix = BKE_scene_multiview_view_suffix_get(rd, view);

	/* avoid NULL pointer */
	if (!suffix)
		suffix = "";

	if (BKE_imtype_is_movie(rd->im_format.imtype)) {
		BKE_movie_filepath_get(name, rd, preview != 0, suffix);
	}
	else {
		BKE_image_path_from_imformat(
		        name, rd->pic, G.main->name, (frame == INT_MIN) ? rd->cfra : frame,
		        &rd->im_format, (rd->scemode & R_EXTENSION) != 0, true, suffix);
	}
}

static void rna_Scene_ray_cast(
        Scene *scene, float origin[3], float direction[3], float ray_dist,
        int *r_success, float r_location[3], float r_normal[3], int *r_index,
        Object **r_ob, float r_obmat[16])
{
	normalize_v3(direction);

	SnapObjectContext *sctx = ED_transform_snap_object_context_create(
	        G.main, scene, 0);

	bool ret = ED_transform_snap_object_project_ray_ex(
	        sctx,
	        &(const struct SnapObjectParams){
	            .snap_select = SNAP_ALL,
	        },
	        origin, direction, &ray_dist,
	        r_location, r_normal, r_index,
	        r_ob, (float(*)[4])r_obmat);

	ED_transform_snap_object_context_destroy(sctx);

	if (ret) {
		*r_success = true;
	}
	else {
		*r_success = false;

		unit_m4((float(*)[4])r_obmat);
		zero_v3(r_location);
		zero_v3(r_normal);
	}
}

#ifdef WITH_ALEMBIC

static void rna_Scene_alembic_export(
        Scene *scene,
        bContext *C,
        const char *filepath,
        int frame_start,
        int frame_end,
        int xform_samples,
        int geom_samples,
        float shutter_open,
        float shutter_close,
        int selected_only,
        int uvs,
        int normals,
        int vcolors,
        int apply_subdiv,
        int flatten_hierarchy,
        int visible_layers_only,
        int renderable_only,
        int face_sets,
        int use_subdiv_schema,
        int export_hair,
        int export_particles,
        int compression_type,
        int packuv,
        float scale,
        int triangulate,
        int quad_method,
        int ngon_method)
{
/* We have to enable allow_threads, because we may change scene frame number
 * during export. */
#ifdef WITH_PYTHON
	BPy_BEGIN_ALLOW_THREADS;
#endif

	const struct AlembicExportParams params = {
	    .frame_start = frame_start,
	    .frame_end = frame_end,

	    .frame_samples_xform = xform_samples,
	    .frame_samples_shape = geom_samples,

	    .shutter_open = shutter_open,
	    .shutter_close = shutter_close,

	    .selected_only = selected_only,
	    .uvs = uvs,
	    .normals = normals,
	    .vcolors = vcolors,
	    .apply_subdiv = apply_subdiv,
	    .flatten_hierarchy = flatten_hierarchy,
	    .visible_layers_only = visible_layers_only,
	    .renderable_only = renderable_only,
	    .face_sets = face_sets,
	    .use_subdiv_schema = use_subdiv_schema,
	    .export_hair = export_hair,
	    .export_particles = export_particles,
	    .compression_type = compression_type,
	    .packuv = packuv,
	    .triangulate = triangulate,
	    .quad_method = quad_method,
	    .ngon_method = ngon_method,

	    .global_scale = scale,
	};

	ABC_export(scene, C, filepath, &params, true);

#ifdef WITH_PYTHON
	BPy_END_ALLOW_THREADS;
#endif
}

#endif

#ifdef WITH_COLLADA
/* don't remove this, as COLLADA exporting cannot be done through operators in render() callback. */
#include "../../collada/collada.h"

/* Note: This definition must match to the generated function call */
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
        int export_texture_type,
        int use_texture_copies,
        int triangulate,
        int use_object_instantiation,
        int use_blender_profile,
        int sort_by_name,
        int export_transformation_type,
        int open_sim,
        int limit_precision,
        int keep_bind_info)
{
	collada_export(scene,
		filepath,

		apply_modifiers,
		export_mesh_type,

		selected,
		include_children,
		include_armatures,
		include_shapekeys,
		deform_bones_only,

		active_uv_only,
		export_texture_type,
		use_texture_copies,

		triangulate,
		use_object_instantiation,
		use_blender_profile,
		sort_by_name,

		export_transformation_type,
		open_sim,
		limit_precision,
		keep_bind_info);
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
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_float(func, "subframe", 0.0, 0.0, 1.0, "", "Sub-frame time, between 0.0 and 1.0", 0.0, 1.0);
	RNA_def_function_flag(func, FUNC_USE_MAIN);

	func = RNA_def_function(srna, "update", "rna_Scene_update_tagged");
	RNA_def_function_ui_description(func,
	                                "Update data tagged to be updated from previous access to data or operators");
	RNA_def_function_flag(func, FUNC_USE_MAIN);

	func = RNA_def_function(srna, "uvedit_aspect", "rna_Scene_uvedit_aspect");
	RNA_def_function_ui_description(func, "Get uv aspect for current object");
	parm = RNA_def_pointer(func, "object", "Object", "", "Object");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_float_vector(func, "result", 2, NULL, 0.0f, FLT_MAX, "", "aspect", 0.0f, FLT_MAX);
	RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0);
	RNA_def_function_output(func, parm);
	
	/* Ray Cast */
	func = RNA_def_function(srna, "ray_cast", "rna_Scene_ray_cast");
	RNA_def_function_ui_description(func, "Cast a ray onto in object space");
	/* ray start and end */
	parm = RNA_def_float_vector(func, "origin", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_float_vector(func, "direction", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_float(func, "distance", BVH_RAYCAST_DIST_MAX, 0.0, BVH_RAYCAST_DIST_MAX,
	              "", "Maximum distance", 0.0, BVH_RAYCAST_DIST_MAX);
	/* return location and normal */
	parm = RNA_def_boolean(func, "result", 0, "", "");
	RNA_def_function_output(func, parm);
	parm = RNA_def_float_vector(func, "location", 3, NULL, -FLT_MAX, FLT_MAX, "Location",
	                            "The hit location of this ray cast", -1e4, 1e4);
	RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0);
	RNA_def_function_output(func, parm);
	parm = RNA_def_float_vector(func, "normal", 3, NULL, -FLT_MAX, FLT_MAX, "Normal",
	                            "The face normal at the ray cast hit location", -1e4, 1e4);
	RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0);
	RNA_def_function_output(func, parm);
	parm = RNA_def_int(func, "index", 0, 0, 0, "", "The face index, -1 when original data isn't available", 0, 0);
	RNA_def_function_output(func, parm);
	parm = RNA_def_pointer(func, "object", "Object", "", "Ray cast object");
	RNA_def_function_output(func, parm);
	parm = RNA_def_float_matrix(func, "matrix", 4, 4, NULL, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
	RNA_def_function_output(func, parm);

#ifdef WITH_COLLADA
	/* don't remove this, as COLLADA exporting cannot be done through operators in render() callback. */
	func = RNA_def_function(srna, "collada_export", "rna_Scene_collada_export");
	parm = RNA_def_string(func, "filepath", NULL, FILE_MAX, "File Path", "File path to write Collada file");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_property_subtype(parm, PROP_FILEPATH); /* allow non utf8 */

	RNA_def_boolean(func, "apply_modifiers", false,
	                "Apply Modifiers", "Apply modifiers to exported mesh (non destructive))");

	RNA_def_int(func, "export_mesh_type", 0, INT_MIN, INT_MAX,
	            "Resolution", "Modifier resolution for export", INT_MIN, INT_MAX);

	RNA_def_boolean(func, "selected", false, "Selection Only", "Export only selected elements");

	RNA_def_boolean(func, "include_children", false,
	                "Include Children", "Export all children of selected objects (even if not selected)");

	RNA_def_boolean(func, "include_armatures", false,
	                "Include Armatures", "Export related armatures (even if not selected)");

	RNA_def_boolean(func, "include_shapekeys", true, "Include Shape Keys", "Export all Shape Keys from Mesh Objects");

	RNA_def_boolean(func, "deform_bones_only", false,
	                "Deform Bones only", "Only export deforming bones with armatures");

	RNA_def_boolean(func, "active_uv_only", false, "Only Selected UV Map", "Export only the selected UV Map");

	RNA_def_int(func, "export_texture_type", 0, INT_MIN, INT_MAX,
		"Texture Type", "Type for exported Textures (UV or MAT)", INT_MIN, INT_MAX);

	RNA_def_boolean(func, "use_texture_copies", true,
	                "Copy", "Copy textures to same folder where the .dae file is exported");

	RNA_def_boolean(func, "triangulate", true, "Triangulate", "Export Polygons (Quads & NGons) as Triangles");

	RNA_def_boolean(func, "use_object_instantiation", true,
	                "Use Object Instances", "Instantiate multiple Objects from same Data");

	RNA_def_boolean(func, "use_blender_profile", true, "Use Blender Profile",
	                "Export additional Blender specific information (for material, shaders, bones, etc.)");

	RNA_def_boolean(func, "sort_by_name", false, "Sort by Object name", "Sort exported data by Object name");

	RNA_def_int(func, "export_transformation_type", 0, INT_MIN, INT_MAX,
	            "Transform", "Transformation type for translation, scale and rotation", INT_MIN, INT_MAX);

	RNA_def_boolean(func, "open_sim", false,
	                "Export to SL/OpenSim", "Compatibility mode for SL, OpenSim and other compatible online worlds");

	RNA_def_boolean(func, "limit_precision", false,
	                "Limit Precision",
	                "Reduce the precision of the exported data to 6 digits");

	RNA_def_boolean(func, "keep_bind_info", false,
	                "Keep Bind Info",
	                "Store bind pose information in custom bone properties for later use during Collada export");

#endif

#ifdef WITH_ALEMBIC
	/* XXX Deprecated, will be removed in 2.8 in favour of calling the export operator. */
	func = RNA_def_function(srna, "alembic_export", "rna_Scene_alembic_export");
	RNA_def_function_ui_description(func, "Export to Alembic file (deprecated, use the Alembic export operator)");

	parm = RNA_def_string(func, "filepath", NULL, FILE_MAX, "File Path", "File path to write Alembic file");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_property_subtype(parm, PROP_FILEPATH); /* allow non utf8 */

	RNA_def_int(func, "frame_start", 1, INT_MIN, INT_MAX, "Start", "Start Frame", INT_MIN, INT_MAX);
	RNA_def_int(func, "frame_end", 1, INT_MIN, INT_MAX, "End", "End Frame", INT_MIN, INT_MAX);
	RNA_def_int(func, "xform_samples", 1, 1, 128, "Xform samples", "Transform samples per frame", 1, 128);
	RNA_def_int(func, "geom_samples", 1, 1, 128, "Geom samples", "Geometry samples per frame", 1, 128);
	RNA_def_float(func, "shutter_open", 0.0f, -1.0f, 1.0f, "Shutter open", "", -1.0f, 1.0f);
	RNA_def_float(func, "shutter_close", 1.0f, -1.0f, 1.0f, "Shutter close", "", -1.0f, 1.0f);
	RNA_def_boolean(func, "selected_only"	, 0, "Selected only", "Export only selected objects");
	RNA_def_boolean(func, "uvs"			, 1, "UVs", "Export UVs");
	RNA_def_boolean(func, "normals"		, 1, "Normals", "Export cormals");
	RNA_def_boolean(func, "vcolors"		, 0, "Vertex colors", "Export vertex colors");
	RNA_def_boolean(func, "apply_subdiv"	, 1, "Subsurfs as meshes", "Export subdivision surfaces as meshes");
	RNA_def_boolean(func, "flatten"		, 0, "Flatten hierarchy", "Flatten hierarchy");
	RNA_def_boolean(func, "visible_layers_only"	, 0, "Visible layers only", "Export only objects in visible layers");
	RNA_def_boolean(func, "renderable_only"	, 0, "Renderable objects only", "Export only objects marked renderable in the outliner");
	RNA_def_boolean(func, "face_sets"	, 0, "Facesets", "Export face sets");
	RNA_def_boolean(func, "subdiv_schema", 0, "Use Alembic subdivision Schema", "Use Alembic subdivision Schema");
	RNA_def_boolean(func, "export_hair", 1, "Export Hair", "Exports hair particle systems as animated curves");
	RNA_def_boolean(func, "export_particles", 1, "Export Particles", "Exports non-hair particle systems");
	RNA_def_enum(func, "compression_type", rna_enum_abc_compression_items, 0, "Compression", "");
	RNA_def_boolean(func, "packuv"		, 0, "Export with packed UV islands", "Export with packed UV islands");
	RNA_def_float(func, "scale", 1.0f, 0.0001f, 1000.0f, "Scale", "Value by which to enlarge or shrink the objects with respect to the world's origin", 0.0001f, 1000.0f);
	RNA_def_boolean(func, "triangulate", 0, "Triangulate", "Export Polygons (Quads & NGons) as Triangles");
	RNA_def_enum(func, "quad_method", rna_enum_modifier_triangulate_quad_method_items, 0, "Quad Method", "Method for splitting the quads into triangles");
	RNA_def_enum(func, "ngon_method", rna_enum_modifier_triangulate_quad_method_items, 0, "Polygon Method", "Method for splitting the polygons into triangles");

	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
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
	RNA_def_boolean(func, "preview", 0, "Preview", "Use preview range");
	RNA_def_string_file_path(func, "view", NULL, FILE_MAX, "View",
	                                "The name of the view to use to replace the \"%\" chars");
	parm = RNA_def_string_file_path(func, "filepath", NULL, FILE_MAX, "File Path",
	                                "The resulting filepath from the scenes render settings");
	RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0); /* needed for string return value */
	RNA_def_function_output(func, parm);
}

#endif
