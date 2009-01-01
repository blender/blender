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

#include "DNA_constraint_types.h"

#ifdef RNA_RUNTIME

#else

/* base struct for constraints */
void rna_def_constraint_basedata(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem type_items[] ={
		{CONSTRAINT_TYPE_NULL, "NULL", "Null", ""},
		{CONSTRAINT_TYPE_CHILDOF, "CHILDOF", "Child Of", ""},
		{CONSTRAINT_TYPE_TRACKTO, "TRACKTO", "Track To", ""},
		{CONSTRAINT_TYPE_KINEMATIC, "IK", "IK", ""},
		{CONSTRAINT_TYPE_FOLLOWPATH, "FOLLOWPATH", "Follow Path", ""},
		{CONSTRAINT_TYPE_ROTLIMIT, "LIMITROT", "Limit Rotation", ""},
		{CONSTRAINT_TYPE_LOCLIMIT, "LIMITLOC", "Limit Location", ""},
		{CONSTRAINT_TYPE_SIZELIMIT, "LIMITSCALE", "Limit Scale", ""},
		{CONSTRAINT_TYPE_ROTLIKE, "COPYROT", "Copy Rotation", ""},
		{CONSTRAINT_TYPE_LOCLIKE, "COPYLOC", "Copy Location", ""},
		{CONSTRAINT_TYPE_SIZELIKE, "COPYSCALE", "Copy Scale", ""},
		{CONSTRAINT_TYPE_PYTHON, "SCRIPT", "Script", ""},
		{CONSTRAINT_TYPE_ACTION, "ACTION", "Action", ""},
		{CONSTRAINT_TYPE_LOCKTRACK, "LOCKTRACK", "Locked Track", ""},
		{CONSTRAINT_TYPE_DISTLIMIT, "LIMITDIST", "Limit Distance", ""},
		{CONSTRAINT_TYPE_STRETCHTO, "STRETCHTO", "Stretch To", ""},
		{CONSTRAINT_TYPE_MINMAX, "FLOOR", "Floor", ""},
		{CONSTRAINT_TYPE_RIGIDBODYJOINT, "RIGIDBODYJOINT", "Rigid Body Joint", ""},
		{CONSTRAINT_TYPE_CLAMPTO, "CLAMPTO", "Clamp To", ""},
		{CONSTRAINT_TYPE_TRANSFORM, "TRANSFORM", "Transformation", ""},
		{0, NULL, NULL, NULL}};
	/*static EnumPropertyItem space_items[] ={
		{CONSTRAINT_SPACE_WORLD, "WORLD", "World Space", "World/Global space."},
		{CONSTRAINT_SPACE_LOCAL, "LOCAL", "Local", "For objects (relative to parent/without parent influence). | For bones (along normals of bone, without parent/restpositions)."},
		{CONSTRAINT_SPACE_POSE, "POSE", "Pose", "Pose/Armature space (only for Pose Channels)."},
		{CONSTRAINT_SPACE_PARLOCAL, "PARLOCAL", "Local With Parent", "'Local' space with Parent transform taken into account (only for Pose Channels)."},
		{0, NULL, NULL, NULL}};*/
	
	/* data */
	srna= RNA_def_struct(brna, "Constraint", NULL );
	RNA_def_struct_ui_text(srna, "Constraint", "alter the transformation of 'Objects' or 'Bones' from a number of predefined constraints");
	RNA_def_struct_sdna(srna, "bConstraint");
	
	/* strings */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	
	/* enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Type", "");
	
	/* flags */
		// XXX do we want to wrap this?
	prop= RNA_def_property(srna, "expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_EXPAND);
	RNA_def_property_ui_text(prop, "Expanded", "Constraint's panel is expanded in UI.");
	
		// XXX this is really an internal flag, but it may be useful for some tools to be able to access this...
	prop= RNA_def_property(srna, "disabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_DISABLE);
	RNA_def_property_ui_text(prop, "Disabled", "Constraint has invalid settings and will not be evaluated.");
	
		// TODO: setting this to true must ensure that all others in stack are turned off too...
	prop= RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_ACTIVE);
	RNA_def_property_ui_text(prop, "Active", "Constraint is the one being edited ");
	
	prop= RNA_def_property(srna, "own_ipo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_OWN_IPO);
	RNA_def_property_ui_text(prop, "Local IPO", "Constraint has its own IPO data.");
	
	prop= RNA_def_property(srna, "proxy_local", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONSTRAINT_PROXY_LOCAL);
	RNA_def_property_ui_text(prop, "Proxy Local", "Constraint was added in this proxy instance (i.e. did not belong to source Armature).");
	
	
	/* pointers */
		// err... how to enable this to work, since data pointer can be of various types?
	//prop= RNA_def_property(srna, "data", PROP_POINTER, PROP_NONE);
	//RNA_def_property_ui_text(prop, "Settings", "Settings specific to this constraint type.");
	
	prop= RNA_def_property(srna, "ipo", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "IPO", "Local IPO data.");
}

/* ---- */

/* ---- */

void RNA_def_constraint(BlenderRNA *brna)
{
	/* basic constraint struct (inherited data) */
	rna_def_constraint_basedata(brna);
	
	/* add data for specific constraint struct types here... */
	
}

#endif
