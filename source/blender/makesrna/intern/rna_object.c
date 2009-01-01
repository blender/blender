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

static int rna_VertexGroup_index_get(PointerRNA *ptr)
{
	Object *ob= ptr->id.data;

	return BLI_findindex(&ob->defbase, ptr->data);
}

static void *rna_Object_active_vertex_group_get(PointerRNA *ptr)
{
	Object *ob= ptr->id.data;
	return BLI_findlink(&ob->defbase, ob->actdef);
}

static void *rna_Object_game_settings_get(PointerRNA *ptr)
{
	return ptr->id.data;
}

static void rna_Object_layer_set(PointerRNA *ptr, int index, int value)
{
	Object *ob= (Object*)ptr->data;

	if(value) ob->lay |= (1<<index);
	else {
		ob->lay &= ~(1<<index);
		if(ob->lay == 0)
			ob->lay |= (1<<index);
	}
}

static void rna_ObjectGameSettings_state_set(PointerRNA *ptr, int index, int value)
{
	Object *ob= (Object*)ptr->data;

	if(value) ob->state |= (1<<index);
	else {
		ob->state &= ~(1<<index);
		if(ob->state == 0)
			ob->state |= (1<<index);
	}
}

#else

static void rna_def_vertex_group(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "VertexGroup", NULL);
	RNA_def_struct_sdna(srna, "bDeformGroup");
	RNA_def_struct_ui_text(srna, "Vertex Group", "Group of vertices, used for armature deform and other purposes.");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Vertex group name.");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_VertexGroup_index_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Index", "Index number of the vertex group.");
}

static void rna_def_object_game_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem body_type_items[] = {
		{OB_BODY_TYPE_NO_COLLISION, "NO_COLLISION", "No Collision", ""},
		{OB_BODY_TYPE_STATIC, "STATIC", "Static", ""},
		{OB_BODY_TYPE_DYNAMIC, "DYNAMIC", "Dynamic", ""},
		{OB_BODY_TYPE_RIGID, "RIGID_BODY", "Rigid Body", ""},
		{OB_BODY_TYPE_SOFT, "SOFT_BODY", "Soft Body", ""},
		{0, NULL, NULL, NULL}};

	static EnumPropertyItem collision_bounds_items[] = {
		{OB_BOUND_BOX, "BOX", "Box", ""},
		{OB_BOUND_SPHERE, "SPHERE", "Sphere", ""},
		{OB_BOUND_CYLINDER, "CYLINDER", "Cylinder", ""},
		{OB_BOUND_CONE, "CONE", "Cone", ""},
		{OB_BOUND_POLYH, "CONVEX_HULL", "Convex Hull", ""},
		{OB_BOUND_POLYT, "TRIANGLE_MESH", "Triangle Mesh", ""},
		//{OB_DYN_MESH, "DYNAMIC_MESH", "Dynamic Mesh", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "ObjectGameSettings", NULL);
	RNA_def_struct_sdna(srna, "Object");
	RNA_def_struct_ui_text(srna, "Object Game Settings", "Game engine related settings for the object.");

	prop= RNA_def_property(srna, "sensors", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Sensor");
	RNA_def_property_ui_text(prop, "Sensors", "DOC_BROKEN");

	prop= RNA_def_property(srna, "controllers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Controller");
	RNA_def_property_ui_text(prop, "Controllers", "DOC_BROKEN");

	prop= RNA_def_property(srna, "actuators", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Actuator");
	RNA_def_property_ui_text(prop, "Actuators", "DOC_BROKEN");

	prop= RNA_def_property(srna, "properties", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "prop", NULL);
	RNA_def_property_struct_type(prop, "GameProperty");
	RNA_def_property_ui_text(prop, "Properties", "Game engine properties.");

	prop= RNA_def_property(srna, "physics_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "body_type");
	RNA_def_property_enum_items(prop, body_type_items);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE); // this controls various gameflags
	RNA_def_property_ui_text(prop, "Physics Type",  "Selects the type of physical representation.");

	prop= RNA_def_property(srna, "actor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_ACTOR);
	RNA_def_property_ui_text(prop, "Actor", "Object is detected by the Near and Radar sensor.");

	prop= RNA_def_property(srna, "ghost", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_GHOST);
	RNA_def_property_ui_text(prop, "Ghost", "Object does not restitute collisions, like a ghost.");

	prop= RNA_def_property(srna, "mass", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 10000.0);
	RNA_def_property_ui_text(prop, "Mass", "Mass of the object.");

	prop= RNA_def_property(srna, "radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "inertia");
	RNA_def_property_range(prop, 0.01, 10.0);
	RNA_def_property_ui_text(prop, "Radius", "Radius for Bounding sphere and Fh/Fh Rot.");

	prop= RNA_def_property(srna, "no_sleeping", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_COLLISION_RESPONSE);
	RNA_def_property_ui_text(prop, "No Sleeping", "Disable auto (de)activation in physics simulation.");

	prop= RNA_def_property(srna, "damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "damping");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Damping", "General movement damping.");

	prop= RNA_def_property(srna, "rotation_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rdamping");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Rotation Damping", "General rotation damping.");

	prop= RNA_def_property(srna, "do_fh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_DO_FH);
	RNA_def_property_ui_text(prop, "Do Fh", "Use Fh settings in materials.");

	prop= RNA_def_property(srna, "rotation_fh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_ROT_FH);
	RNA_def_property_ui_text(prop, "Rotation Fh", "Use face normal to rotate Object");

	prop= RNA_def_property(srna, "form_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "formfactor");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Form Factor", "Form factor scales the inertia tensor.");

	prop= RNA_def_property(srna, "anisotropic_friction", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_ANISOTROPIC_FRICTION);
	RNA_def_property_ui_text(prop, "Anisotropic Friction", "Enable anisotropic friction.");

	prop= RNA_def_property(srna, "friction_coefficients", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "anisotropicFriction");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Friction Coefficients", "Relative friction coefficient in the in the X, Y and Z directions, when anisotropic friction is enabled.");

	prop= RNA_def_property(srna, "use_collision_bounds", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_BOUNDS);
	RNA_def_property_ui_text(prop, "Use Collision Bounds", "Specify a collision bounds type other than the default.");

	prop= RNA_def_property(srna, "collision_bounds", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "boundtype");
	RNA_def_property_enum_items(prop, collision_bounds_items);
	RNA_def_property_ui_text(prop, "Collision Bounds",  "Selects the collision type.");

	prop= RNA_def_property(srna, "collision_compound", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gameflag", OB_CHILD);
	RNA_def_property_ui_text(prop, "Collison Compound", "Add children to form a compound collision object.");

	prop= RNA_def_property(srna, "collision_margin", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "margin");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Collision Margin", "Extra margin around object for collision detection, small amount required for stability.");

	prop= RNA_def_property(srna, "state", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "state", 1);
	RNA_def_property_array(prop, 30);
	RNA_def_property_ui_text(prop, "State", "State determining which controllers are displayed.");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_ObjectGameSettings_state_set");

	prop= RNA_def_property(srna, "initial_state", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "init_state", 1);
	RNA_def_property_array(prop, 30);
	RNA_def_property_ui_text(prop, "Initial State", "Initial state when the game starts.");
}

static void rna_def_object(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Object", "ID");
	RNA_def_struct_ui_text(srna, "Object", "DOC_BROKEN");

	prop= RNA_def_property(srna, "data", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_ui_text(prop, "Data", "Object data.");

	prop= RNA_def_property(srna, "layers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "lay", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_ui_text(prop, "Layers", "Layers the object is on.");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Object_layer_set");

	/* parent and track */

	prop= RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Parent", "Parent Object");

	prop= RNA_def_property(srna, "track", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Track", "Object being tracked to define the rotation (Old Track).");

	/* transform */

	prop= RNA_def_property(srna, "location", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "loc");
	RNA_def_property_ui_text(prop, "Location", "Location of the object.");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_update");

	prop= RNA_def_property(srna, "delta_location", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "dloc");
	RNA_def_property_ui_text(prop, "Delta Location", "Extra added translation to object location.");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_update");
	
	prop= RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_ROTATION);
	RNA_def_property_float_sdna(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Rotation", "Rotation of the object.");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_update");

	prop= RNA_def_property(srna, "delta_rotation", PROP_FLOAT, PROP_ROTATION);
	RNA_def_property_float_sdna(prop, NULL, "drot");
	RNA_def_property_ui_text(prop, "Delta Rotation", "Extra added rotation to the rotation of the object.");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_update");
	
	prop= RNA_def_property(srna, "scale", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_ui_text(prop, "Scale", "Scaling of the object.");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_update");

	prop= RNA_def_property(srna, "delta_scale", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "dsize");
	RNA_def_property_ui_text(prop, "Delta Scale", "Extra added scaling to the scale of the object.");
	RNA_def_property_update(prop, NC_OBJECT|ND_TRANSFORM, "rna_Object_update");

	/* collections */

	prop= RNA_def_property(srna, "ipo", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Ipo");
	RNA_def_property_ui_text(prop, "Ipo", "DOC_BROKEN");

	prop= RNA_def_property(srna, "constraints", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Constraint");
	RNA_def_property_ui_text(prop, "Constraints", "DOC_BROKEN");
	
	prop= RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Modifier");
	RNA_def_property_ui_text(prop, "Modifiers", "DOC_BROKEN");

	/* game engine */

	prop= RNA_def_property(srna, "game_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ObjectGameSettings");
	RNA_def_property_pointer_funcs(prop, "rna_Object_game_settings_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Game Settings", "Game engine related settings for the object.");

	/* vertex groups */

	prop= RNA_def_property(srna, "vertex_groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "defbase", NULL);
	RNA_def_property_struct_type(prop, "VertexGroup");
	RNA_def_property_ui_text(prop, "Vertex Groups", "Vertex groups of the object.");

	prop= RNA_def_property(srna, "active_vertex_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "VertexGroup");
	RNA_def_property_pointer_funcs(prop, "rna_Object_active_vertex_group_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Vertex Group", "Vertex groups of the object.");

	/* various */

	prop= RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "index");
	RNA_def_property_ui_text(prop, "Pass Index", "Index # for the IndexOB render pass.");

	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "col");
	RNA_def_property_ui_text(prop, "Color", "Object color and alpha, used when faces have the ObColor mode enabled.");
}

void RNA_def_object(BlenderRNA *brna)
{
	rna_def_vertex_group(brna);
	rna_def_object_game_settings(brna);
	rna_def_object(brna);
}

#endif

