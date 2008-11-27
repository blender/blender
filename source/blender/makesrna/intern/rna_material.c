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
 * Contributor(s): Blender Foundation (2008), Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_material_types.h"

#ifdef RNA_RUNTIME

#else

void RNA_def_material(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_type_items[] = {
		{MA_RGB, "RGB", "RGB", ""},
		{MA_CMYK, "CMYK", "CMYK", ""},
		{MA_YUV, "YUV", "YUV", ""},
		{MA_HSV, "HSV", "HSV", ""},
		{0, NULL, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "Material", "ID", "Material");
		
	prop= RNA_def_property(srna, "colormodel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Color Model", "Color model.");
	
	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, "Material", "r");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Color", "Diffuse color.");
	RNA_def_property_ui_range(prop, 0.0f , 1.0f, 10.0f, 3.0f);
	
	prop= RNA_def_property(srna, "specular", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "specr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Specular", "Specular color.");
	RNA_def_property_ui_range(prop, 0.0f , 1.0f, 10.0f, 3.0f);

	prop= RNA_def_property(srna, "nodetree", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "bNodeTree");
	RNA_def_property_ui_text(prop, "Nodetree", "Nodetree");

}

#endif


