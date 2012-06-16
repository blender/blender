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

#include "ExportSettings.h"
#include "DocumentExporter.h"
#include "DocumentImporter.h"

extern "C"
{
#include "BKE_scene.h"
#include "BKE_context.h"

/* make dummy file */
#include "BLI_fileops.h"
#include "BLI_path_util.h"

int collada_import(bContext *C, const char *filepath)
{
	DocumentImporter imp(C, filepath);
	if (imp.import()) return 1;

	return 0;
}

int collada_export(Scene *sce,
                   const char *filepath,

                   int apply_modifiers,

                   int selected,
                   int include_children,
                   int include_armatures,
                   int deform_bones_only,

                   int use_object_instantiation,
                   int sort_by_name,
                   int second_life)
{
	ExportSettings export_settings;

	/* annoying, collada crashes if file cant be created! [#27162] */
	if (!BLI_exists(filepath)) {
		BLI_make_existing_file(filepath);     /* makes the dir if its not there */
		if (BLI_file_touch(filepath) == 0) {
			return 0;
		}
	}
	/* end! */

	export_settings.filepath                 = (char *)filepath;

	export_settings.apply_modifiers          = apply_modifiers != 0;

	export_settings.selected                 = selected          != 0;
	export_settings.include_children         = include_children  != 0;
	export_settings.include_armatures        = include_armatures != 0;
	export_settings.deform_bones_only        = deform_bones_only != 0;

	export_settings.use_object_instantiation = use_object_instantiation != 0;
	export_settings.sort_by_name             = sort_by_name != 0;
	export_settings.second_life              = second_life != 0;


	int includeFilter = OB_REL_NONE;
	if (export_settings.include_armatures) includeFilter |= OB_REL_MOD_ARMATURE;
	if (export_settings.include_children) includeFilter |= OB_REL_CHILDREN_RECURSIVE;

	eObjectSet objectSet = (export_settings.selected) ? OB_SET_SELECTED : OB_SET_ALL;
	export_settings.export_set = BKE_object_relational_superset(sce, objectSet, (eObRelationTypes)includeFilter);

	if (export_settings.sort_by_name)
		bc_bubble_sort_by_Object_name(export_settings.export_set);

	DocumentExporter exporter(&export_settings);
	exporter.exportCurrentScene(sce);

	BLI_linklist_free(export_settings.export_set, NULL);

	return 1;
}

/* end extern C */
}
