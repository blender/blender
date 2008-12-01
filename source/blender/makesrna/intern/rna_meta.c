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

#include "DNA_meta_types.h"

#ifdef RNA_RUNTIME

#else

void RNA_def_metaelem(BlenderRNA *brna)
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
	
	srna= RNA_def_struct(brna, "MetaElem", "ID", "MetaElem");
	
	/* Enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Metaball types.");
	
	/* Number values */
	prop= RNA_def_property(srna, "x_dimension", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "expx");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_text(prop, "X Dimension", "X dimension of metaelement. Used for elements such as cubes.");
	
	prop= RNA_def_property(srna, "y_dimension", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "expy");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_text(prop, "Y Dimension", "Y dimension of metaelement. Used for elements such as cubes.");
	
	prop= RNA_def_property(srna, "z_dimension", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "expz");
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_text(prop, "Z Dimension", "Z dimension of metaelement. Used for elements such as cubes.");
	
	prop= RNA_def_property(srna, "stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "s");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Stiffness", "Stiffness defines how much of the metaelement to fill.");
	
	/* flag */
	prop= RNA_def_property(srna, "metaelem_negative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MB_NEGATIVE);
	RNA_def_property_ui_text(prop, "Negative Metaelement", "Set metaball as negative one.");
	
	prop= RNA_def_property(srna, "metaelem_hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MB_HIDE);
	RNA_def_property_ui_text(prop, "Hide Metaelement", "Hide metaball?");
	
	prop= RNA_def_property(srna, "metaelem_scale_radius", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MB_SCALE_RAD);
	RNA_def_property_ui_text(prop, "Scale Metaelement Radius", "Scale metaball radius?");
}

void RNA_def_metaball(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_update_items[] = {
		{MB_UPDATE_ALWAYS, "UPDATE_ALWAYS", "Always", "While editing, update metaball always."},
		{MB_UPDATE_HALFRES, "HALFRES", "Half Resolution", "While editing, update metaball in half resolution."},
		{MB_UPDATE_FAST, "FAST", "Fast", "While editing, update metaball without polygonization."},
		{MB_UPDATE_NEVER, "NEVER", "Never", "While editing, don't update metaball at all."},
		{0, NULL, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "MetaBall", "ID", "MetaBall");
	
	/* Enums */
	prop= RNA_def_property(srna, "flag", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_update_items);
	RNA_def_property_ui_text(prop, "Update", "Metaball edit update option.");
	
	/* Number values */
	prop= RNA_def_property(srna, "wiresize", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "wiresize");
	RNA_def_property_range(prop, 0.050f, 1.0f);
	RNA_def_property_ui_text(prop, "Wiresize", "Polygonization resolution in the 3D viewport.");
	
	prop= RNA_def_property(srna, "rendersize", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rendersize");
	RNA_def_property_range(prop, 0.050f, 1.0f);
	RNA_def_property_ui_text(prop, "Rendersize", "Polygonization resolution in rendering.");
	
	prop= RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "thresh");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Influence of metaelements.");
}

#endif
