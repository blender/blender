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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_group_types.h"

#ifdef RNA_RUNTIME

PointerRNA rna_Group_objects_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal= iter->internal;

	/* we are actually iterating a GroupObject list, so override get */
	return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, ((GroupObject*)internal->link)->ob);
}

#else

void RNA_def_group(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Group", "ID");
	RNA_def_struct_ui_text(srna, "Group", "Group of Object datablocks.");
	RNA_def_struct_ui_icon(srna, ICON_GROUP);

	prop= RNA_def_property(srna, "dupli_offset", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "dupli_ofs");
	RNA_def_property_ui_text(prop, "Dupli Offset", "Offset from the center to use when instancing as DupliGroup.");
	RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 4);
	
	prop= RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "gobject", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_ui_text(prop, "Objects", "A collection of this groups objects.");
	RNA_def_property_collection_funcs(prop, 0, 0, 0, "rna_Group_objects_get", 0, 0, 0, 0, 0);
	
	prop= RNA_def_property(srna, "layer", PROP_BOOLEAN, PROP_LAYER);
	RNA_def_property_boolean_sdna(prop, NULL, "layer", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_ui_text(prop, "Dupli Layers", "Layers visible when this groups is instanced as a dupli.");	
}

#endif

