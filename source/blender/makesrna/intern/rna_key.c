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

#include "DNA_ID.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"

#ifdef RNA_RUNTIME

static Key *rna_ShapeKey_find_key(ID *id)
{
	switch(GS(id->name)) {
		case ID_CU: return ((Curve*)id)->key;
		case ID_KE: return (Key*)id;
		case ID_LT: return ((Lattice*)id)->key;
		case ID_ME: return ((Mesh*)id)->key;
		default: return NULL;
	}
}

static void *rna_ShapeKey_relative_key_get(PointerRNA *ptr)
{
	Key *key= rna_ShapeKey_find_key(ptr->id.data);
	KeyBlock *kb= (KeyBlock*)ptr->data, *kbrel;
	int a;

	if(key && kb->relative < key->totkey)
		for(a=0, kbrel=key->block.first; kbrel; kbrel=kbrel->next, a++)
			if(a == kb->relative)
				return kbrel;

	return NULL;
}

#else

void RNA_def_keyblock(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_keyblock_type_items[] = {
		{KEY_LINEAR, "KEY_LINEAR", "Linear", ""},
		{KEY_CARDINAL, "KEY_CARDINAL", "Cardinal", ""},
		{KEY_BSPLINE, "KEY_BSPLINE", "BSpline", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "ShapeKey", NULL, "Shape Key");
	RNA_def_struct_sdna(srna, "KeyBlock");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);

	/* the current value isn't easily editable this way, it's linked to an IPO.
	prop= RNA_def_property(srna, "current_position", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pos");
	RNA_def_property_ui_text(prop, "Current Position", "");

	prop= RNA_def_property(srna, "current_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "curval");
	RNA_def_property_ui_text(prop, "Current Value", "");*/

	prop= RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation", "Interpolation type.");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex weight group, to blend with basis shape.");

	prop= RNA_def_property(srna, "relative_key", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_struct_type(prop, "ShapeKey");
	RNA_def_property_ui_text(prop, "Relative Key", "Shape used as a relative key.");
	RNA_def_property_pointer_funcs(prop, "rna_ShapeKey_relative_key_get", NULL, NULL);

	prop= RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYBLOCK_MUTE);
	RNA_def_property_ui_text(prop, "Mute", "Mute this shape key.");

	prop= RNA_def_property(srna, "slider_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "slidermin");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Slider Min", "Minimum for slider.");

	prop= RNA_def_property(srna, "slider_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "slidermax");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Slider Max", "Maximum for slider.");

	/* KeyBlock.data has to be wrapped still */
}

void RNA_def_key(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	RNA_def_keyblock(brna);

	srna= RNA_def_struct(brna, "Key", "ID", "Key");

	prop= RNA_def_property(srna, "reference_key", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_pointer_sdna(prop, NULL, "refkey");
	RNA_def_property_ui_text(prop, "Reference Key", "");

	prop= RNA_def_property(srna, "shape_keys", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "block", NULL);
	RNA_def_property_struct_type(prop, "ShapeKey");
	RNA_def_property_ui_text(prop, "Shape Keys", "");

	rna_def_ipo_common(srna);

	prop= RNA_def_property(srna, "from", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "From", "Datablock using these shape keys.");

	prop= RNA_def_property(srna, "relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type", KEY_RELATIVE);
	RNA_def_property_ui_text(prop, "Relative", "Makes shape keys relative.");

	prop= RNA_def_property(srna, "slurph", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "slurph");
	RNA_def_property_range(prop, -500, 500);
	RNA_def_property_ui_text(prop, "Slurph", "Creates a delay in amount of frames in applying keypositions, first vertex goes first.");
}

#endif

