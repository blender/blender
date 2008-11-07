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

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_object_types.h"

#ifdef RNA_RUNTIME

static StructRNA *rna_Object_data_type(PointerRNA *ptr)
{
	Object *ob= (Object*)ptr->data;

	switch(ob->type) {
		case OB_EMPTY:
			return NULL;
		case OB_MESH:
			return &RNA_Mesh;
#if 0
		case OB_CURVE:
			return &RNA_Curve;
		case OB_SURF:
			return &RNA_Surface;
		case OB_FONT:
			return &RNA_Font;
		case OB_MBALL:
			return &RNA_MBall;
		case OB_LAMP:
			return &RNA_Lamp;
		case OB_CAMERA:
			return &RNA_Camera;
		case OB_WAVE:
			return &RNA_Wave;
		case OB_LATTICE:
			return &RNA_Lattice;
#endif
		default:
			return NULL;
	}
}

#else

void RNA_def_object(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Object", "Object");

	RNA_def_ID(srna);

	prop= RNA_def_property(srna, "data", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_RENDER_DEPENDENCY);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Object_data_type", NULL);

	prop= RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EVALUATE_DEPENDENCY);

	prop= RNA_def_property(srna, "track", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EVALUATE_DEPENDENCY);
}

#endif

