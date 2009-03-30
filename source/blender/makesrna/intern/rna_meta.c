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
 * Contributor(s): Blender Foundation (2008), Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"

#ifdef RNA_RUNTIME

static int rna_Meta_texspace_editable(PointerRNA *ptr)
{
	MetaBall *mb= (MetaBall*)ptr->data;
	return (mb->texflag & AUTOSPACE)? 0: PROP_EDITABLE;
}

#else

void rna_def_metaelement(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_type_items[] = {
		{MB_BALL, "BALL", "Ball", ""},
		{MB_TUBE, "TUBE", "Tube", ""},
		{MB_PLANE, "PLANE", "Plane", ""},
		{MB_ELIPSOID, "ELLIPSOID", "Ellipsoid", ""}, // NOTE: typo at original definition!
		{MB_CUBE, "CUBE", "Cube", ""},
		{0, NULL, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "MetaElement", NULL);
	RNA_def_struct_ui_text(srna, "Meta Element", "Blobby element in a MetaBall datablock.");
	RNA_def_struct_sdna(srna, "MetaElem");
	
	/* enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Metaball types.");
	
	/* number values */
	prop= RNA_def_property(srna, "location", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "x");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Location", "");

	prop= RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_ROTATION);
	RNA_def_property_float_sdna(prop, NULL, "quat");
	RNA_def_property_ui_text(prop, "Rotation", "");

	prop= RNA_def_property(srna, "radius", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "rad");
	RNA_def_property_ui_text(prop, "Radius", "");

	prop= RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "expx");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Size", "Size of element, use of components depends on element type.");
	
	prop= RNA_def_property(srna, "stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "s");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Stiffness", "Stiffness defines how much of the element to fill.");
	
	/* flags */
	prop= RNA_def_property(srna, "negative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MB_NEGATIVE);
	RNA_def_property_ui_text(prop, "Negative", "Set metaball as negative one.");
	
	prop= RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MB_HIDE);
	RNA_def_property_ui_text(prop, "Hide", "Hide element.");
}

void rna_def_metaball(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_update_items[] = {
		{MB_UPDATE_ALWAYS, "UPDATE_ALWAYS", "Always", "While editing, update metaball always."},
		{MB_UPDATE_HALFRES, "HALFRES", "Half Resolution", "While editing, update metaball in half resolution."},
		{MB_UPDATE_FAST, "FAST", "Fast", "While editing, update metaball without polygonization."},
		{MB_UPDATE_NEVER, "NEVER", "Never", "While editing, don't update metaball at all."},
		{0, NULL, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "MetaBall", "ID");
	RNA_def_struct_ui_text(srna, "MetaBall", "Metaball datablock to defined blobby surfaces.");

	prop= RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "elems", NULL);
	RNA_def_property_struct_type(prop, "MetaElement");
	RNA_def_property_ui_text(prop, "Elements", "Meta elements.");

	/* enums */
	prop= RNA_def_property(srna, "flag", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_update_items);
	RNA_def_property_ui_text(prop, "Update", "Metaball edit update behavior.");
	
	/* number values */
	prop= RNA_def_property(srna, "wire_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "wiresize");
	RNA_def_property_range(prop, 0.050f, 1.0f);
	RNA_def_property_ui_text(prop, "Wire Size", "Polygonization resolution in the 3D viewport.");
	
	prop= RNA_def_property(srna, "render_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rendersize");
	RNA_def_property_range(prop, 0.050f, 1.0f);
	RNA_def_property_ui_text(prop, "Render Size", "Polygonization resolution in rendering.");
	
	prop= RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "thresh");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Influence of meta elements.");

	/* materials, textures */
	rna_def_texmat_common(srna, "rna_Meta_texspace_editable");
}

void RNA_def_meta(BlenderRNA *brna)
{
	rna_def_metaelement(brna);
	rna_def_metaball(brna);
}

#endif
