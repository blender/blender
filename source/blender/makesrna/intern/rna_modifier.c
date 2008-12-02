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

#include "DNA_modifier_types.h"

#ifdef RNA_RUNTIME

#else

void RNA_def_modifier(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem type_items[] ={
		{eModifierType_Subsurf, "SUBSURF", "Subsurf", ""},
		{eModifierType_Lattice, "LATTICE", "Lattice", ""},
		{eModifierType_Curve, "CURVE", "Curve", ""},
		{eModifierType_Build, "BUILD", "Build", ""},
		{eModifierType_Mirror, "MIRROR", "Mirror", ""},
		{eModifierType_Decimate, "DECIMATE", "Decimate", ""},
		{eModifierType_Wave, "WAVE", "Wave", ""},
		{eModifierType_Armature, "ARMATURE", "Armature", ""},
		{eModifierType_Hook, "HOOK", "Hook", ""},
		{eModifierType_Softbody, "SOFTBODY", "Softbody", ""},
		{eModifierType_Boolean, "BOOLEAN", "Boolean", ""},
		{eModifierType_Array, "ARRAY", "Array", ""},
		{eModifierType_EdgeSplit, "EDGESPLIT", "EdgeSplit", ""},
		{eModifierType_Displace, "DISPLACE", "Displace", ""},
		{eModifierType_UVProject, "UVPROJECT", "UVProject", ""},
		{eModifierType_Smooth, "SMOOTH", "Smooth", ""},
		{eModifierType_Cast, "CAST", "Cast", ""},
		{eModifierType_MeshDeform, "MESHDEFORM", "MeshDeform", ""},
		{eModifierType_ParticleSystem, "PARTICLESYSTEM", "Particle System", ""},
		{eModifierType_ParticleInstance, "PARTICLEINSTANCE", "Particle Instance", ""},
		{eModifierType_Explode, "EXPLODE", "Explode", ""},
		{eModifierType_Cloth, "CLOTH", "Cloth", ""},
		{eModifierType_Collision, "COLLISION", "Collision", ""},
		{eModifierType_Bevel, "BEVEL", "Bevel", ""},
		{eModifierType_Shrinkwrap, "SHRINKWRAP", "Shrinkwrap", ""},
		{eModifierType_Fluidsim, "FLUIDSIM", "Fluidsim", ""},
		{eModifierType_Mask, "MASK", "Mask", ""},
		{eModifierType_SimpleDeform, "SIMPLEDEFORM", "SimpleDeform", ""},
		{0, NULL, NULL, NULL}};
	
	/* data */
	srna= RNA_def_struct(brna, "Modifier", NULL , "Object Modifier");
	RNA_def_struct_sdna(srna, "ModifierData");
	
	/* strings */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	
	/* enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_sdna(prop, NULL, "type", PROP_DEF_ENUM_BITFLAGS);
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Type", "");
	
	/* flags */
	prop= RNA_def_property(srna, "realtime", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Realtime);
	RNA_def_property_ui_text(prop, "Realtime", "Realtime display of a modifier.");
	
	prop= RNA_def_property(srna, "render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Render);
	RNA_def_property_ui_text(prop, "Render", "Use modifier during rendering.");
	
	prop= RNA_def_property(srna, "editmode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Editmode);
	RNA_def_property_ui_text(prop, "Editmode", "Use modifier while in the edit mode.");
	
	prop= RNA_def_property(srna, "on_cage", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_OnCage);
	RNA_def_property_ui_text(prop, "On Cage", "Enable direct editing of modifier control cage.");
	
	prop= RNA_def_property(srna, "expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eModifierMode_Expanded);
	RNA_def_property_ui_text(prop, "Expanded", "Set modifier expanded in the user interface.");
	
	/* TODO: expose "virtual" and "disable temporary" enum items? */

	/* pointers */
	/* TODO: expose error? */
}

#endif
