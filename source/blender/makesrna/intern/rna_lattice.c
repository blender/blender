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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"

#ifdef RNA_RUNTIME

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_depsgraph.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_deform.h"

#include "WM_api.h"
#include "WM_types.h"

static void rna_LatticePoint_co_get(PointerRNA *ptr, float *values)
{
	Lattice *lt= (Lattice*)ptr->id.data;
	BPoint *bp= (BPoint*)ptr->data;
	int a= bp - lt->def;
	int x= a % lt->pntsu;
	int y= (a/lt->pntsu) % lt->pntsv;
	int z= (a/(lt->pntsu*lt->pntsv));

	values[0]= lt->fu + x*lt->du;
	values[1]= lt->fv + y*lt->dv;
	values[2]= lt->fw + z*lt->dw;
}

static void rna_LatticePoint_groups_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Lattice *lt= (Lattice*)ptr->id.data;

	if(lt->dvert) {
		BPoint *bp= (BPoint*)ptr->data;
		MDeformVert *dvert= lt->dvert + (bp-lt->def);

		rna_iterator_array_begin(iter, (void*)dvert->dw, sizeof(MDeformWeight), dvert->totweight, 0, NULL);
	}
	else
		rna_iterator_array_begin(iter, NULL, 0, 0, 0, NULL);
}

static void rna_Lattice_points_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Lattice *lt= (Lattice*)ptr->data;
	int tot= lt->pntsu*lt->pntsv*lt->pntsw;

	if(lt->editlatt && lt->editlatt->def)
		rna_iterator_array_begin(iter, (void*)lt->editlatt->def, sizeof(BPoint), tot, 0, NULL);
	else if(lt->def)
		rna_iterator_array_begin(iter, (void*)lt->def, sizeof(BPoint), tot, 0, NULL);
	else
		rna_iterator_array_begin(iter, NULL, 0, 0, 0, NULL);
}

static void rna_Lattice_update_data(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ID *id= ptr->id.data;

	DAG_id_flush_update(id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_GEOM|ND_DATA, id);
}

static void rna_Lattice_update_size(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Lattice *lt= ptr->id.data;
	Object *ob;
	int newu, newv, neww;

	/* we don't modify the actual pnts, but go through opnts instead */
	newu= (lt->opntsu > 0)? lt->opntsu: lt->pntsu;
	newv= (lt->opntsv > 0)? lt->opntsv: lt->pntsv;
	neww= (lt->opntsw > 0)? lt->opntsw: lt->pntsw;

	/* resizelattice needs an object, any object will have the same result */
	for(ob=bmain->object.first; ob; ob= ob->id.next) {
		if(ob->data == lt) {
			resizelattice(lt, newu, newv, neww, ob);
			if(lt->editlatt)
				resizelattice(lt->editlatt, newu, newv, neww, ob);
			break;
		}
	}

	/* otherwise without, means old points are not repositioned */
	if(!ob) {
		resizelattice(lt, newu, newv, neww, NULL);
		if(lt->editlatt)
			resizelattice(lt->editlatt, newu, newv, neww, NULL);
	}

	rna_Lattice_update_data(bmain, scene, ptr);
}

static void rna_Lattice_outside_set(PointerRNA *ptr, int value)
{
	Lattice *lt= ptr->data;

	if(value) lt->flag |= LT_OUTSIDE;
	else lt->flag &= ~LT_OUTSIDE;

	outside_lattice(lt);

	if(lt->editlatt) {
		if(value) lt->editlatt->flag |= LT_OUTSIDE;
		else lt->editlatt->flag &= ~LT_OUTSIDE;

		outside_lattice(lt->editlatt);
	}
}

static void rna_Lattice_points_u_set(PointerRNA *ptr, int value)
{
	((Lattice*)ptr->data)->opntsu= CLAMPIS(value, 1, 64);
}

static void rna_Lattice_points_v_set(PointerRNA *ptr, int value)
{
	((Lattice*)ptr->data)->opntsv= CLAMPIS(value, 1, 64);
}

static void rna_Lattice_points_w_set(PointerRNA *ptr, int value)
{
	((Lattice*)ptr->data)->opntsw= CLAMPIS(value, 1, 64);
}

static void rna_Lattice_vg_name_set(PointerRNA *ptr, const char *value)
{
	Lattice *lt= ptr->data;
	strcpy(lt->vgroup, value);

	if(lt->editlatt)
		strcpy(lt->editlatt->vgroup, value);
}


#else

static void rna_def_latticepoint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "LatticePoint", NULL);
	RNA_def_struct_sdna(srna, "BPoint");
	RNA_def_struct_ui_text(srna, "LatticePoint", "Point in the lattice grid");

	prop= RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_LatticePoint_co_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Location", "");

	prop= RNA_def_property(srna, "deformed_co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "vec");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Deformed Location", "");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data");

	prop= RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_LatticePoint_groups_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, 0, 0);
	RNA_def_property_struct_type(prop, "VertexGroupElement");
	RNA_def_property_ui_text(prop, "Groups", "Weights for the vertex groups this point is member of");
}

static void rna_def_lattice(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_keyblock_type_items[] = {
		{KEY_LINEAR, "KEY_LINEAR", 0, "Linear", ""},
		{KEY_CARDINAL, "KEY_CARDINAL", 0, "Cardinal", ""},
		{KEY_BSPLINE, "KEY_BSPLINE", 0, "BSpline", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "Lattice", "ID");
	RNA_def_struct_ui_text(srna, "Lattice", "Lattice datablock defining a grid for deforming other objects");
	RNA_def_struct_ui_icon(srna, ICON_LATTICE_DATA);

	prop= RNA_def_property(srna, "points_u", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsu");
	RNA_def_property_int_funcs(prop, NULL, "rna_Lattice_points_u_set", NULL);
	RNA_def_property_range(prop, 1, 64);
	RNA_def_property_ui_text(prop, "U", "Points in U direction");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_size");

	prop= RNA_def_property(srna, "points_v", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsv");
	RNA_def_property_int_funcs(prop, NULL, "rna_Lattice_points_v_set", NULL);
	RNA_def_property_range(prop, 1, 64);
	RNA_def_property_ui_text(prop, "V", "Points in V direction");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_size");

	prop= RNA_def_property(srna, "points_w", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsw");
	RNA_def_property_int_funcs(prop, NULL, "rna_Lattice_points_w_set", NULL);
	RNA_def_property_range(prop, 1, 64);
	RNA_def_property_ui_text(prop, "W", "Points in W direction");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_size");

	prop= RNA_def_property(srna, "interpolation_type_u", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typeu");
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type U", "");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data");

	prop= RNA_def_property(srna, "interpolation_type_v", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typev");
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type V", "");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data");

	prop= RNA_def_property(srna, "interpolation_type_w", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typew");
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type W", "");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data");

	prop= RNA_def_property(srna, "outside", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LT_OUTSIDE);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Lattice_outside_set");
	RNA_def_property_ui_text(prop, "Outside", "Only draw, and take into account, the outer vertices");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data");
	
	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group to apply the influence of the lattice");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Lattice_vg_name_set");
	RNA_def_property_update(prop, 0, "rna_Lattice_update_data");

	prop= RNA_def_property(srna, "shape_keys", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "key");
	RNA_def_property_ui_text(prop, "Shape Keys", "");

	prop= RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "LatticePoint");
	RNA_def_property_collection_funcs(prop, "rna_Lattice_points_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, 0, 0);
	RNA_def_property_ui_text(prop, "Points", "Points of the lattice");
}

void RNA_def_lattice(BlenderRNA *brna)
{
	rna_def_lattice(brna);
	rna_def_latticepoint(brna);
}

#endif

