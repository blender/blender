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
 * Contributor(s): Blender Foundation (2008), Juho Vepsalainen, Jiri Hnidek
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_meta.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"

#ifdef RNA_RUNTIME

#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BKE_mball.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"

#include "WM_types.h"
#include "WM_api.h"

static int rna_Meta_texspace_editable(PointerRNA *ptr)
{
	MetaBall *mb = (MetaBall*)ptr->data;
	return (mb->texflag & MB_AUTOSPACE)? 0: PROP_EDITABLE;
}

static void rna_Meta_texspace_loc_get(PointerRNA *ptr, float *values)
{
	MetaBall *mb = (MetaBall*)ptr->data;
	
	/* tex_space_mball() needs object.. ugh */
	
	copy_v3_v3(values, mb->loc);
}

static void rna_Meta_texspace_loc_set(PointerRNA *ptr, const float *values)
{
	MetaBall *mb = (MetaBall*)ptr->data;
	
	copy_v3_v3(mb->loc, values);
}

static void rna_Meta_texspace_size_get(PointerRNA *ptr, float *values)
{
	MetaBall *mb = (MetaBall*)ptr->data;
	
	/* tex_space_mball() needs object.. ugh */
	
	copy_v3_v3(values, mb->size);
}

static void rna_Meta_texspace_size_set(PointerRNA *ptr, const float *values)
{
	MetaBall *mb = (MetaBall*)ptr->data;
	
	copy_v3_v3(mb->size, values);
}


static void rna_MetaBall_update_data(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	MetaBall *mb = ptr->id.data;
	Object *ob;

	/* cheating way for importers to avoid slow updates */
	if (mb->id.us > 0) {
		for (ob = bmain->object.first; ob; ob = ob->id.next)
			if (ob->data == mb)
				BKE_metaball_properties_copy(scene, ob);
	
		DAG_id_tag_update(&mb->id, 0);
		WM_main_add_notifier(NC_GEOM|ND_DATA, mb);
	}
}

static void rna_MetaBall_update_rotation(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	MetaElem *ml = ptr->data;
	normalize_qt(ml->quat);
	rna_MetaBall_update_data(bmain, scene, ptr);
}

static MetaElem *rna_MetaBall_elements_new(MetaBall *mb, int type)
{
	MetaElem *ml = BKE_metaball_element_add(mb, type);

	/* cheating way for importers to avoid slow updates */
	if (mb->id.us > 0) {
		DAG_id_tag_update(&mb->id, 0);
		WM_main_add_notifier(NC_GEOM|ND_DATA, &mb->id);
	}

	return ml;
}

static void rna_MetaBall_elements_remove(MetaBall *mb, ReportList *reports, MetaElem *ml)
{
	int found = 0;

	found = BLI_remlink_safe(&mb->elems, ml);

	if (!found) {
		BKE_reportf(reports, RPT_ERROR, "Metaball \"%s\" does not contain spline given", mb->id.name+2);
		return;
	}

	MEM_freeN(ml);
	/* invalidate pointer!, no can do */

	/* cheating way for importers to avoid slow updates */
	if (mb->id.us > 0) {
		DAG_id_tag_update(&mb->id, 0);
		WM_main_add_notifier(NC_GEOM|ND_DATA, &mb->id);
	}
}

static void rna_MetaBall_elements_clear(MetaBall *mb)
{
	BLI_freelistN(&mb->elems);

	/* cheating way for importers to avoid slow updates */
	if (mb->id.us > 0) {
		DAG_id_tag_update(&mb->id, 0);
		WM_main_add_notifier(NC_GEOM|ND_DATA, &mb->id);
	}
}

#else

static void rna_def_metaelement(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MetaElement", NULL);
	RNA_def_struct_sdna(srna, "MetaElem");
	RNA_def_struct_ui_text(srna, "Meta Element", "Blobby element in a Metaball datablock");
	RNA_def_struct_ui_icon(srna, ICON_OUTLINER_DATA_META);

	/* enums */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, metaelem_type_items);
	RNA_def_property_ui_text(prop, "Type", "Metaball types");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
	
	/* number values */
	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "x");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_QUATERNION);
	RNA_def_property_float_sdna(prop, NULL, "quat");
	RNA_def_property_ui_text(prop, "Rotation", "Normalized quaternion rotation");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_rotation");

	prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_UNSIGNED|PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "rad");
	RNA_def_property_ui_text(prop, "Radius", "");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

	prop = RNA_def_property(srna, "size_x", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "expx");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_text(prop, "Size X", "Size of element, use of components depends on element type");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

	prop = RNA_def_property(srna, "size_y", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "expy");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_text(prop, "Size Y", "Size of element, use of components depends on element type");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

	prop = RNA_def_property(srna, "size_z", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "expz");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_text(prop, "Size Z", "Size of element, use of components depends on element type");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
	
	prop = RNA_def_property(srna, "stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "s");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Stiffness", "Stiffness defines how much of the element to fill");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
	
	/* flags */
	prop = RNA_def_property(srna, "use_negative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MB_NEGATIVE);
	RNA_def_property_ui_text(prop, "Negative", "Set metaball as negative one");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
	
	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MB_HIDE);
	RNA_def_property_ui_text(prop, "Hide", "Hide element");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
}

/* mball.elements */
static void rna_def_metaball_elements(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MetaBallElements");
	srna = RNA_def_struct(brna, "MetaBallElements", NULL);
	RNA_def_struct_sdna(srna, "MetaBall");
	RNA_def_struct_ui_text(srna, "Meta Elements", "Collection of metaball elements");

	func = RNA_def_function(srna, "new", "rna_MetaBall_elements_new");
	RNA_def_function_ui_description(func, "Add a new element to the metaball");
	RNA_def_enum(func, "type", metaelem_type_items, MB_BALL, "", "type for the new meta-element");
	parm = RNA_def_pointer(func, "element", "MetaElement", "", "The newly created meta-element");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_MetaBall_elements_remove");
	RNA_def_function_ui_description(func, "Remove an element from the metaball");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "element", "MetaElement", "", "The element to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);

	func = RNA_def_function(srna, "clear", "rna_MetaBall_elements_clear");
	RNA_def_function_ui_description(func, "Remove all elements from the metaball");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "lastelem");
	RNA_def_property_ui_text(prop, "Active Element", "Last selected element");
}

static void rna_def_metaball(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_update_items[] = {
		{MB_UPDATE_ALWAYS, "UPDATE_ALWAYS", 0, "Always", "While editing, update metaball always"},
		{MB_UPDATE_HALFRES, "HALFRES", 0, "Half", "While editing, update metaball in half resolution"},
		{MB_UPDATE_FAST, "FAST", 0, "Fast", "While editing, update metaball without polygonization"},
		{MB_UPDATE_NEVER, "NEVER", 0, "Never", "While editing, don't update metaball at all"},
		{0, NULL, 0, NULL, NULL}};
	
	srna = RNA_def_struct(brna, "MetaBall", "ID");
	RNA_def_struct_ui_text(srna, "MetaBall", "Metaball datablock to defined blobby surfaces");
	RNA_def_struct_ui_icon(srna, ICON_META_DATA);

	prop = RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "elems", NULL);
	RNA_def_property_struct_type(prop, "MetaElement");
	RNA_def_property_ui_text(prop, "Elements", "Meta elements");
	rna_def_metaball_elements(brna, prop);

	/* enums */
	prop = RNA_def_property(srna, "update_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_update_items);
	RNA_def_property_ui_text(prop, "Update", "Metaball edit update behavior");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
	
	/* number values */
	prop = RNA_def_property(srna, "resolution", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "wiresize");
	RNA_def_property_range(prop, 0.050f, 1.0f);
	RNA_def_property_ui_text(prop, "Wire Size", "Polygonization resolution in the 3D viewport");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
	
	prop = RNA_def_property(srna, "render_resolution", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "rendersize");
	RNA_def_property_range(prop, 0.050f, 1.0f);
	RNA_def_property_ui_text(prop, "Render Size", "Polygonization resolution in rendering");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
	
	prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "thresh");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Influence of meta elements");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

	/* texture space */
	prop = RNA_def_property(srna, "use_auto_texspace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MB_AUTOSPACE);
	RNA_def_property_ui_text(prop, "Auto Texture Space",
	                         "Adjust active object's texture space automatically when transforming object");
	
	prop = RNA_def_property(srna, "texspace_location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Texture Space Location", "Texture space location");
	RNA_def_property_editable_func(prop, "rna_Meta_texspace_editable");
	RNA_def_property_float_funcs(prop, "rna_Meta_texspace_loc_get", "rna_Meta_texspace_loc_set", NULL);
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
	
	prop = RNA_def_property(srna, "texspace_size", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Texture Space Size", "Texture space size");
	RNA_def_property_editable_func(prop, "rna_Meta_texspace_editable");
	RNA_def_property_float_funcs(prop, "rna_Meta_texspace_size_get", "rna_Meta_texspace_size_set", NULL);
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
	
	/* not supported yet */
#if 0
	prop= RNA_def_property(srna, "texspace_rot", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Texture Space Rotation", "Texture space rotation");
	RNA_def_property_editable_func(prop, "rna_Meta_texspace_editable");
	RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
#endif
	
	/* materials */
	prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_ui_text(prop, "Materials", "");
	RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.c */
	RNA_def_property_collection_funcs(prop, 0, NULL, NULL, NULL, NULL, NULL, NULL, "rna_IDMaterials_assign_int");
	
	/* anim */
	rna_def_animdata_common(srna);
}

void RNA_def_meta(BlenderRNA *brna)
{
	rna_def_metaelement(brna);
	rna_def_metaball(brna);
}

#endif
