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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_wm_manipulator.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME
/* enum definitions */
#endif /* RNA_RUNTIME */

#ifdef RNA_RUNTIME

#include <assert.h>

#include "WM_api.h"
#include "WM_types.h"

#include "DNA_workspace_types.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_workspace.h"

#include "MEM_guardedalloc.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

/* -------------------------------------------------------------------- */

/** \name Manipulator API
 * \{ */

static wmManipulator *rna_ManipulatorProperties_find_operator(PointerRNA *ptr)
{
#if 0
	wmWindowManager *wm = ptr->id.data;
#endif

	/* We could try workaruond this lookup, but not trivial. */
	for (bScreen *screen = G.main->screen.first; screen; screen = screen->id.next) {
		IDProperty *properties = (IDProperty *)ptr->data;
		for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
			for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
				if (ar->manipulator_map) {
					wmManipulatorMap *mmap = ar->manipulator_map;
					for (wmManipulatorGroup *mgroup = WM_manipulatormap_group_list(mmap)->first;
					     mgroup;
					     mgroup = mgroup->next)
					{
						for (wmManipulator *mpr = mgroup->manipulators.first; mpr; mpr = mpr->next) {
							if (mpr->properties == properties) {
								return mpr;
							}
						}
					}
				}
			}
		}
	}
	return NULL;
}

static StructRNA *rna_ManipulatorProperties_refine(PointerRNA *ptr)
{
	wmManipulator *mpr = rna_ManipulatorProperties_find_operator(ptr);

	if (mpr)
		return mpr->type->srna;
	else
		return ptr->type;
}

static IDProperty *rna_ManipulatorProperties_idprops(PointerRNA *ptr, bool create)
{
	if (create && !ptr->data) {
		IDPropertyTemplate val = {0};
		ptr->data = IDP_New(IDP_GROUP, &val, "RNA_ManipulatorProperties group");
	}

	return ptr->data;
}

static PointerRNA rna_Manipulator_properties_get(PointerRNA *ptr)
{
	wmManipulator *mpr = (wmManipulator *)ptr->data;
	return rna_pointer_inherit_refine(ptr, mpr->type->srna, mpr->properties);
}

static StructRNA *rna_Manipulator_refine(PointerRNA *mnp_ptr)
{
	wmManipulator *mpr = mnp_ptr->data;
	return (mpr->type && mpr->type->ext.srna) ? mpr->type->ext.srna : &RNA_Manipulator;
}

/** \} */

/** \name Manipulator Group API
 * \{ */

static StructRNA *rna_ManipulatorGroup_refine(PointerRNA *mgroup_ptr)
{
	wmManipulatorGroup *mgroup = mgroup_ptr->data;
	return (mgroup->type && mgroup->type->ext.srna) ? mgroup->type->ext.srna : &RNA_ManipulatorGroup;
}

static void rna_ManipulatorGroup_manipulators_begin(CollectionPropertyIterator *iter, PointerRNA *mgroup_ptr)
{
	wmManipulatorGroup *mgroup = mgroup_ptr->data;
	rna_iterator_listbase_begin(iter, &mgroup->manipulators, NULL);
}

/** \} */


#else /* RNA_RUNTIME */


/* ManipulatorGroup.manipulators */
static void rna_def_manipulators(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	RNA_def_property_srna(cprop, "Manipulators");
	srna = RNA_def_struct(brna, "Manipulators", NULL);
	RNA_def_struct_sdna(srna, "wmManipulatorGroup");
	RNA_def_struct_ui_text(srna, "Manipulators", "Collection of manipulators");
}


static void rna_def_manipulator(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "Manipulator");
	srna = RNA_def_struct(brna, "Manipulator", NULL);
	RNA_def_struct_sdna(srna, "wmManipulator");
	RNA_def_struct_ui_text(srna, "Manipulator", "Collection of manipulators");
	RNA_def_struct_refine_func(srna, "rna_Manipulator_refine");

	prop = RNA_def_property(srna, "properties", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "ManipulatorProperties");
	RNA_def_property_ui_text(prop, "Properties", "");
	RNA_def_property_pointer_funcs(prop, "rna_Manipulator_properties_get", NULL, NULL, NULL);

	srna = RNA_def_struct(brna, "ManipulatorProperties", NULL);
	RNA_def_struct_ui_text(srna, "Manipulator Properties", "Input properties of an Manipulator");
	RNA_def_struct_refine_func(srna, "rna_ManipulatorProperties_refine");
	RNA_def_struct_idprops_func(srna, "rna_ManipulatorProperties_idprops");
	RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES);
}

static void rna_def_manipulatorgroup(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ManipulatorGroup", NULL);
	RNA_def_struct_ui_text(srna, "ManipulatorGroup", "Storage of an operator being executed, or registered after execution");
	RNA_def_struct_sdna(srna, "wmManipulatorGroup");
	RNA_def_struct_refine_func(srna, "rna_ManipulatorGroup_refine");

	RNA_define_verify_sdna(0); /* not in sdna */

	prop = RNA_def_property(srna, "manipulators", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "manipulators", NULL);
	RNA_def_property_struct_type(prop, "Manipulator");
	RNA_def_property_collection_funcs(
	        prop, "rna_ManipulatorGroup_manipulators_begin", "rna_iterator_listbase_next",
	        "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	        NULL, NULL, NULL, NULL);

	RNA_def_property_ui_text(prop, "Manipulators", "List of manipulators in the Manipulator Map");
	rna_def_manipulator(brna, prop);
	rna_def_manipulators(brna, prop);

	RNA_define_verify_sdna(1); /* not in sdna */
}

void RNA_def_wm_manipulator(BlenderRNA *brna)
{
	rna_def_manipulatorgroup(brna);
}

#endif /* RNA_RUNTIME */
