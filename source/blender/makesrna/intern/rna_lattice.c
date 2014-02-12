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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_lattice.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_string.h"
#include "BKE_depsgraph.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_deform.h"

#include "WM_api.h"
#include "WM_types.h"
#include "ED_lattice.h"

static void rna_LatticePoint_co_get(PointerRNA *ptr, float *values)
{
	Lattice *lt = (Lattice *)ptr->id.data;
	BPoint *bp = (BPoint *)ptr->data;
	int index = bp - lt->def;
	int u, v, w;

	BKE_lattice_index_to_uvw(lt, index, &u, &v, &w);

	values[0] = lt->fu + u * lt->du;
	values[1] = lt->fv + v * lt->dv;
	values[2] = lt->fw + w * lt->dw;
}

static void rna_LatticePoint_groups_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Lattice *lt = (Lattice *)ptr->id.data;

	if (lt->dvert) {
		BPoint *bp = (BPoint *)ptr->data;
		MDeformVert *dvert = lt->dvert + (bp - lt->def);

		rna_iterator_array_begin(iter, (void *)dvert->dw, sizeof(MDeformWeight), dvert->totweight, 0, NULL);
	}
	else
		rna_iterator_array_begin(iter, NULL, 0, 0, 0, NULL);
}

static void rna_Lattice_points_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Lattice *lt = (Lattice *)ptr->data;
	int tot = lt->pntsu * lt->pntsv * lt->pntsw;

	if (lt->editlatt && lt->editlatt->latt->def)
		rna_iterator_array_begin(iter, (void *)lt->editlatt->latt->def, sizeof(BPoint), tot, 0, NULL);
	else if (lt->def)
		rna_iterator_array_begin(iter, (void *)lt->def, sizeof(BPoint), tot, 0, NULL);
	else
		rna_iterator_array_begin(iter, NULL, 0, 0, 0, NULL);
}

static void rna_Lattice_update_data(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;

	DAG_id_tag_update(id, 0);
	WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

/* copy settings to editlattice,
 * we could split this up differently (one update call per property)
 * but for now thats overkill
 */
static void rna_Lattice_update_data_editlatt(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	Lattice *lt = (Lattice *)ptr->id.data;

	if (lt->editlatt) {
		Lattice *lt_em = lt->editlatt->latt;
		lt_em->typeu = lt->typeu;
		lt_em->typev = lt->typev;
		lt_em->typew = lt->typew;
		lt_em->flag = lt->flag;
		BLI_strncpy(lt_em->vgroup, lt->vgroup, sizeof(lt_em->vgroup));
	}

	DAG_id_tag_update(id, 0);
	WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

static void rna_Lattice_update_size(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Lattice *lt = ptr->id.data;
	Object *ob;
	int newu, newv, neww;

	/* we don't modify the actual pnts, but go through opnts instead */
	newu = (lt->opntsu > 0) ? lt->opntsu : lt->pntsu;
	newv = (lt->opntsv > 0) ? lt->opntsv : lt->pntsv;
	neww = (lt->opntsw > 0) ? lt->opntsw : lt->pntsw;

	/* BKE_lattice_resize needs an object, any object will have the same result */
	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		if (ob->data == lt) {
			BKE_lattice_resize(lt, newu, newv, neww, ob);
			if (lt->editlatt)
				BKE_lattice_resize(lt->editlatt->latt, newu, newv, neww, ob);
			break;
		}
	}

	/* otherwise without, means old points are not repositioned */
	if (!ob) {
		BKE_lattice_resize(lt, newu, newv, neww, NULL);
		if (lt->editlatt)
			BKE_lattice_resize(lt->editlatt->latt, newu, newv, neww, NULL);
	}

	rna_Lattice_update_data(bmain, scene, ptr);
}

static void rna_Lattice_use_outside_set(PointerRNA *ptr, int value)
{
	Lattice *lt = ptr->data;

	if (value) lt->flag |= LT_OUTSIDE;
	else lt->flag &= ~LT_OUTSIDE;

	outside_lattice(lt);

	if (lt->editlatt) {
		if (value) lt->editlatt->latt->flag |= LT_OUTSIDE;
		else lt->editlatt->latt->flag &= ~LT_OUTSIDE;

		outside_lattice(lt->editlatt->latt);
	}
}

static int rna_Lattice_size_editable(PointerRNA *ptr)
{
	Lattice *lt = (Lattice *)ptr->data;

	return lt->key == NULL;
}

static void rna_Lattice_points_u_set(PointerRNA *ptr, int value)
{
	Lattice *lt = (Lattice *)ptr->data;

	lt->opntsu = CLAMPIS(value, 1, 64);
}

static void rna_Lattice_points_v_set(PointerRNA *ptr, int value)
{
	Lattice *lt = (Lattice *)ptr->data;

	lt->opntsv = CLAMPIS(value, 1, 64);
}

static void rna_Lattice_points_w_set(PointerRNA *ptr, int value)
{
	Lattice *lt = (Lattice *)ptr->data;

	lt->opntsw = CLAMPIS(value, 1, 64);
}

static void rna_Lattice_vg_name_set(PointerRNA *ptr, const char *value)
{
	Lattice *lt = ptr->data;
	BLI_strncpy(lt->vgroup, value, sizeof(lt->vgroup));

	if (lt->editlatt) {
		BLI_strncpy(lt->editlatt->latt->vgroup, value, sizeof(lt->editlatt->latt->vgroup));
	}
}

/* annoying, but is a consequence of RNA structures... */
static char *rna_LatticePoint_path(PointerRNA *ptr)
{
	Lattice *lt = (Lattice *)ptr->id.data;
	void *point = ptr->data;
	BPoint *points = NULL;
	
	if (lt->editlatt && lt->editlatt->latt->def)
		points = lt->editlatt->latt->def;
	else
		points = lt->def;
	
	if (points && point) {
		int tot = lt->pntsu * lt->pntsv * lt->pntsw;
		
		/* only return index if in range */
		if ((point >= (void *)points) && (point < (void *)(points + tot))) {
			int pt_index = (int)((BPoint *)point - points);
			
			return BLI_sprintfN("points[%d]", pt_index);
		}
	}

	return BLI_strdup("");
}

static int rna_Lattice_is_editmode_get(PointerRNA *ptr)
{
	Lattice *lt = (Lattice *)ptr->id.data;
	return (lt->editlatt != NULL);
}

#else

static void rna_def_latticepoint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LatticePoint", NULL);
	RNA_def_struct_sdna(srna, "BPoint");
	RNA_def_struct_ui_text(srna, "LatticePoint", "Point in the lattice grid");
	RNA_def_struct_path_func(srna, "rna_LatticePoint_path");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f1", 0);
	RNA_def_property_ui_text(prop, "Point selected", "Selection status");

	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_LatticePoint_co_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Location",
	                         "Original undeformed location used to calculate the strength of the deform effect "
	                         "(edit/animate the Deformed Location instead)");

	prop = RNA_def_property(srna, "co_deform", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "vec");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Deformed Location", "");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data");

	prop = RNA_def_property(srna, "weight_softbody", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight");
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Weight", "Softbody goal weight");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data");

	prop = RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_LatticePoint_groups_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "VertexGroupElement");
	RNA_def_property_ui_text(prop, "Groups", "Weights for the vertex groups this point is member of");
}

static void rna_def_lattice(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Lattice", "ID");
	RNA_def_struct_ui_text(srna, "Lattice", "Lattice datablock defining a grid for deforming other objects");
	RNA_def_struct_ui_icon(srna, ICON_LATTICE_DATA);

	prop = RNA_def_property(srna, "points_u", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsu");
	RNA_def_property_int_funcs(prop, NULL, "rna_Lattice_points_u_set", NULL);
	RNA_def_property_range(prop, 1, 64);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "U", "Point in U direction (can't be changed when there are shape keys)");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_size");
	RNA_def_property_editable_func(prop, "rna_Lattice_size_editable");

	prop = RNA_def_property(srna, "points_v", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsv");
	RNA_def_property_int_funcs(prop, NULL, "rna_Lattice_points_v_set", NULL);
	RNA_def_property_range(prop, 1, 64);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "V", "Point in V direction (can't be changed when there are shape keys)");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_size");
	RNA_def_property_editable_func(prop, "rna_Lattice_size_editable");

	prop = RNA_def_property(srna, "points_w", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsw");
	RNA_def_property_int_funcs(prop, NULL, "rna_Lattice_points_w_set", NULL);
	RNA_def_property_range(prop, 1, 64);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "W", "Point in W direction (can't be changed when there are shape keys)");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_size");
	RNA_def_property_editable_func(prop, "rna_Lattice_size_editable");

	prop = RNA_def_property(srna, "interpolation_type_u", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typeu");
	RNA_def_property_enum_items(prop, keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type U", "");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data_editlatt");

	prop = RNA_def_property(srna, "interpolation_type_v", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typev");
	RNA_def_property_enum_items(prop, keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type V", "");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data_editlatt");

	prop = RNA_def_property(srna, "interpolation_type_w", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typew");
	RNA_def_property_enum_items(prop, keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type W", "");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data_editlatt");

	prop = RNA_def_property(srna, "use_outside", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LT_OUTSIDE);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Lattice_use_outside_set");
	RNA_def_property_ui_text(prop, "Outside", "Only draw, and take into account, the outer vertices");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data_editlatt");
	
	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group to apply the influence of the lattice");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Lattice_vg_name_set");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data_editlatt");

	prop = RNA_def_property(srna, "shape_keys", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "key");
	RNA_def_property_ui_text(prop, "Shape Keys", "");

	prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "LatticePoint");
	RNA_def_property_collection_funcs(prop, "rna_Lattice_points_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Points", "Points of the lattice");

	prop = RNA_def_property(srna, "is_editmode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Lattice_is_editmode_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Editmode", "True when used in editmode");

	/* pointers */
	rna_def_animdata_common(srna);

	RNA_api_lattice(srna);
}

void RNA_def_lattice(BlenderRNA *brna)
{
	rna_def_lattice(brna);
	rna_def_latticepoint(brna);
}

#endif
