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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"

#ifdef RNA_RUNTIME

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

		rna_iterator_array_begin(iter, (void*)dvert->dw, sizeof(MDeformWeight), dvert->totweight, NULL);
	}
	else
		rna_iterator_array_begin(iter, NULL, 0, 0, NULL);
}

static void rna_Lattice_points_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Lattice *lt= (Lattice*)ptr->data;

	if(lt->def) {
		int tot= lt->pntsu*lt->pntsv*lt->pntsw;
		rna_iterator_array_begin(iter, (void*)lt->def, sizeof(BPoint), tot, NULL);
	}
	else
		rna_iterator_array_begin(iter, NULL, 0, 0, NULL);
}

#else

static void rna_def_latticepoint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "LatticePoint", NULL);
	RNA_def_struct_sdna(srna, "BPoint");
	RNA_def_struct_ui_text(srna, "LatticePoint", "Point in the lattice grid.");

	prop= RNA_def_property(srna, "co", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_LatticePoint_co_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Location", "");

	prop= RNA_def_property(srna, "deformed_co", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "vec");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Deformed Location", "");

	prop= RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_LatticePoint_groups_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, 0, 0, 0, 0);
	RNA_def_property_struct_type(prop, "VertexGroupElement");
	RNA_def_property_ui_text(prop, "Groups", "Weights for the vertex groups this point is member of.");
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
	RNA_def_struct_ui_text(srna, "Lattice", "Lattice datablock defining a grid for deforming other objects.");
	RNA_def_struct_ui_icon(srna, ICON_LATTICE_DATA);

	prop= RNA_def_property(srna, "points_u", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsu");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "U", "Points in U direction.");

	prop= RNA_def_property(srna, "points_v", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsv");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "V", "Points in V direction.");

	prop= RNA_def_property(srna, "points_w", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsw");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "W", "Points in W direction.");

	prop= RNA_def_property(srna, "interpolation_type_u", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typeu");
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type U", "");

	prop= RNA_def_property(srna, "interpolation_type_v", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typev");
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type V", "");

	prop= RNA_def_property(srna, "interpolation_type_w", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typew");
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type W", "");

	prop= RNA_def_property(srna, "outside", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LT_OUTSIDE);
	RNA_def_property_ui_text(prop, "Outside", "Only draw, and take into account, the outer vertices.");

	prop= RNA_def_property(srna, "shape_keys", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "key");
	RNA_def_property_ui_text(prop, "Shape Keys", "");

	prop= RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "LatticePoint");
	RNA_def_property_collection_funcs(prop, "rna_Lattice_points_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, 0, 0, 0, 0);
	RNA_def_property_ui_text(prop, "Points", "Points of the lattice.");
}

void RNA_def_lattice(BlenderRNA *brna)
{
	rna_def_lattice(brna);
	rna_def_latticepoint(brna);
}

#endif

