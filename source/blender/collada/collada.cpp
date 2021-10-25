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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/collada.cpp
 *  \ingroup collada
 */


/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include "DocumentExporter.h"
#include "DocumentImporter.h"
#include "ExportSettings.h"
#include "ImportSettings.h"

extern "C"
{
#include "BKE_scene.h"
#include "BKE_context.h"

/* make dummy file */
#include "BLI_fileops.h"
#include "BLI_linklist.h"

int collada_import(bContext *C,
				   const char *filepath,
				   int import_units,
				   int find_chains,
				   int auto_connect,
				   int fix_orientation,
				   int min_chain_length,
				   int keep_bind_info)
{

	ImportSettings import_settings;
	import_settings.filepath         = (char *)filepath;
	import_settings.import_units     = import_units != 0;
	import_settings.auto_connect     = auto_connect != 0;
	import_settings.find_chains      = find_chains != 0;
	import_settings.fix_orientation  = fix_orientation != 0;
	import_settings.min_chain_length = min_chain_length;
	import_settings.keep_bind_info = keep_bind_info !=0;

	DocumentImporter imp(C, &import_settings);
	if (imp.import()) return 1;

	return 0;
}

int collada_export(Scene *sce,
                   const char *filepath,

                   int apply_modifiers,
				   BC_export_mesh_type export_mesh_type,

                   int selected,
                   int include_children,
                   int include_armatures,
				   int include_shapekeys,
                   int deform_bones_only,

				   int active_uv_only,
				   BC_export_texture_type export_texture_type,
				   int use_texture_copies,

                   int triangulate,
				   int use_object_instantiation,
				   int use_blender_profile,
				   int sort_by_name,
				   BC_export_transformation_type export_transformation_type,
				   int open_sim,
				   int limit_precision,
				   int keep_bind_info)
{
	ExportSettings export_settings;

	export_settings.filepath                 = (char *)filepath;

	export_settings.apply_modifiers          = apply_modifiers != 0;
	export_settings.export_mesh_type         = export_mesh_type;
	export_settings.selected                 = selected          != 0;
	export_settings.include_children         = include_children  != 0;
	export_settings.include_armatures        = include_armatures != 0;
	export_settings.include_shapekeys        = include_shapekeys != 0;
	export_settings.deform_bones_only        = deform_bones_only != 0;

	export_settings.active_uv_only           = active_uv_only != 0;
	export_settings.export_texture_type      = export_texture_type;
	export_settings.use_texture_copies       = use_texture_copies != 0;

	export_settings.triangulate                = triangulate != 0;
	export_settings.use_object_instantiation   = use_object_instantiation != 0;
	export_settings.use_blender_profile        = use_blender_profile != 0;
	export_settings.sort_by_name               = sort_by_name != 0;
	export_settings.export_transformation_type = export_transformation_type;
	export_settings.open_sim                   = open_sim != 0;
	export_settings.limit_precision = limit_precision != 0;
	export_settings.keep_bind_info = keep_bind_info !=0;

	int includeFilter = OB_REL_NONE;
	if (export_settings.include_armatures) includeFilter |= OB_REL_MOD_ARMATURE;
	if (export_settings.include_children) includeFilter |= OB_REL_CHILDREN_RECURSIVE;

	eObjectSet objectSet = (export_settings.selected) ? OB_SET_SELECTED : OB_SET_ALL;
	export_settings.export_set = BKE_object_relational_superset(sce, objectSet, (eObRelationTypes)includeFilter);
	int export_count = BLI_linklist_count(export_settings.export_set);

	if (export_count == 0) {
		if (export_settings.selected) {
			fprintf(stderr, "Collada: Found no objects to export.\nPlease ensure that all objects which shall be exported are also visible in the 3D Viewport.\n");
		}
		else {
			fprintf(stderr, "Collada: Your scene seems to be empty. No Objects will be exported.\n");
		}
	}
	else {
		if (export_settings.sort_by_name)
			bc_bubble_sort_by_Object_name(export_settings.export_set);
	}

	DocumentExporter exporter(&export_settings);
	int status = exporter.exportCurrentScene(sce);

	BLI_linklist_free(export_settings.export_set, NULL);

	return (status) ? -1:export_count;
}

/* end extern C */
}
