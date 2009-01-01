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
#include "DNA_property_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_depsgraph.h"

static void rna_Object_update(bContext *C, PointerRNA *ptr)
{
	DAG_object_flush_update(CTX_data_scene(C), ptr->id.data, OB_RECALC_OB);
}

#else

void RNA_def_object(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Object", "ID");
	RNA_def_struct_ui_text(srna, "Object", "DOC_BROKEN");

	prop= RNA_def_property(srna, "data", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_ui_text(prop, "Data", "Object data.");

	prop= RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Parent", "Parent Object");

	prop= RNA_def_property(srna, "track", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Track", "Object being tracked to define the rotation (Old Track).");

	prop= RNA_def_property(srna, "loc", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_ui_text(prop, "Location", "DOC_BROKEN");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_update");
	
	prop= RNA_def_property(srna, "rot", PROP_FLOAT, PROP_ROTATION);
	RNA_def_property_ui_text(prop, "Rotation", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_update");
	
	prop= RNA_def_property(srna, "size", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_ui_text(prop, "Scale", "DOC_BROKEN");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_update");

	prop= RNA_def_property(srna, "ipo", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Ipo");
	RNA_def_property_ui_text(prop, "Ipo", "DOC_BROKEN");

	prop= RNA_def_property(srna, "constraints", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Constraint");
	RNA_def_property_ui_text(prop, "Constraints", "DOC_BROKEN");
	
	prop= RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Modifier");
	RNA_def_property_ui_text(prop, "Modifiers", "DOC_BROKEN");

	prop= RNA_def_property(srna, "sensors", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Sensor");
	RNA_def_property_ui_text(prop, "Sensors", "DOC_BROKEN");

	prop= RNA_def_property(srna, "controllers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Controller");
	RNA_def_property_ui_text(prop, "Controllers", "DOC_BROKEN");

	prop= RNA_def_property(srna, "actuators", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Actuator");
	RNA_def_property_ui_text(prop, "Actuators", "DOC_BROKEN");

	prop= RNA_def_property(srna, "game_properties", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "prop", NULL);
	RNA_def_property_struct_type(prop, "GameProperty");
	RNA_def_property_ui_text(prop, "Game Properties", "Game engine properties.");
}

#endif

