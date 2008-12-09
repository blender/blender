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
 * Contributor(s): Blender Foundation (2008), Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_armature_types.h"

#ifdef RNA_RUNTIME

static void rna_Armature_layer_set(PointerRNA *ptr, int index, int value)
{
	bArmature *arm= (bArmature*)ptr->data;

	if(value) arm->layer |= (1<<index);
	else {
		arm->layer &= ~(1<<index);
		if(arm->layer == 0)
			arm->layer |= (1<<index);
	}
}

#else

// err... bones should not be directly edited (only editbones should be...)
static void rna_def_bone(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Bone", NULL, "Bone");
	
}

void rna_def_armature(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_drawtype_items[] = {
		{ARM_OCTA, "OCTAHEDRAL", "Octahedral", "Draw bones as octahedral shape (default)."},
		{ARM_LINE, "STICK", "Stick", "Draw bones as simple 2D lines with dots."},
		{ARM_B_BONE, "BBONE", "B-Bone", "Draw bones as boxes, showing subdivision and B-Splines"},
		{ARM_ENVELOPE, "ENVELOPE", "Envelope", "Draw bones as extruded spheres, showing defomation influence volume."},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_ghosttype_items[] = {
		{ARM_GHOST_CUR, "CURRENTFRAME", "Around Current Frame", "Draw Ghosts of poses within a fixed number of frames around the current frame."},
		{ARM_GHOST_RANGE, "RANGE", "In Range", "Draw Ghosts of poses within specified range."},
		{ARM_GHOST_KEYS, "KEYS", "On Keyframes", "Draw Ghosts of poses on Keyframes."},
		{0, NULL, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "Armature", "ID", "Armature");
	RNA_def_struct_sdna(srna, "bArmature");
	
	/* Collections */
	prop= RNA_def_property(srna, "bones", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "bonebase", NULL);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_ui_text(prop, "Bones", "");
	
	/* Enum values */
	prop= RNA_def_property(srna, "drawtype", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_drawtype_items);
	RNA_def_property_ui_text(prop, "Draw Type", "");
	
	prop= RNA_def_property(srna, "ghosttype", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_drawtype_items);
	RNA_def_property_ui_text(prop, "Ghost Drawing", "Method of Onion-skinning for active Action");
	
	/* Boolean values */
		/* layer */
	prop= RNA_def_property(srna, "layer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layer", 1);
	RNA_def_property_array(prop, 16);
	RNA_def_property_ui_text(prop, "Visible Layers", "Armature layer visibility.");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Armature_layer_set");	
	
		/* layer protection */
	prop= RNA_def_property(srna, "layer_protection", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layer_protected", 1);
	RNA_def_property_array(prop, 16);
	RNA_def_property_ui_text(prop, "Layer Proxy Protection", "Protected layers in Proxy Instances are restored to Proxy settings on file reload and undo.");	
		
		/* flag */
	prop= RNA_def_property(srna, "rest_position", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_RESTPOS);
	RNA_def_property_ui_text(prop, "Rest Position", "Show Armature in Rest Position. No posing possible.");
	
	prop= RNA_def_property(srna, "draw_axes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_DRAWAXES);
	RNA_def_property_ui_text(prop, "Draw Axes", "Draw bone axes.");
	
	prop= RNA_def_property(srna, "draw_names", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_DRAWNAMES);
	RNA_def_property_ui_text(prop, "Draw Names", "Draw bone names.");
	
	prop= RNA_def_property(srna, "delay_deform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_DELAYDEFORM);
	RNA_def_property_ui_text(prop, "Delay Deform", "Don't deform children when manipulating bones in Pose Mode");
	
	prop= RNA_def_property(srna, "x_axis_mirror", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_MIRROR_EDIT);
	RNA_def_property_ui_text(prop, "X-Axis Mirror", "Apply changes to matching bone on opposite side of X-Axis.");
	
	prop= RNA_def_property(srna, "auto_ik", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_AUTO_IK);
	RNA_def_property_ui_text(prop, "Auto IK", "Add temporaral IK constraints while grabbing bones in Pose Mode.");
	
	prop= RNA_def_property(srna, "draw_custom_bone_shapes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ARM_NO_CUSTOM);
	RNA_def_property_ui_text(prop, "Draw Custom Bone Shapes", "Draw bones with their custom shapes.");
	
	prop= RNA_def_property(srna, "draw_group_colors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_COL_CUSTOM);
	RNA_def_property_ui_text(prop, "Draw Bone Group Colors", "Draw bone group colors.");
	
	prop= RNA_def_property(srna, "ghost_only_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ARM_GHOST_ONLYSEL);
	RNA_def_property_ui_text(prop, "Draw Ghosts on Selected Keyframes Only", "");
	
		/* deformflag */
	prop= RNA_def_property(srna, "deform_vertexgroups", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_VGROUP);
	RNA_def_property_ui_text(prop, "Deform Vertex Groups", "Enable Vertex Groups when defining deform");
	
	prop= RNA_def_property(srna, "deform_envelope", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_ENVELOPE);
	RNA_def_property_ui_text(prop, "Deform Envelopes", "Enable Bone Envelopes when defining deform");
	
	prop= RNA_def_property(srna, "deform_quaternion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_QUATERNION);
	RNA_def_property_ui_text(prop, "Use Dual Quaternion Deformation", "Enable deform rotation with Quaternions");
	
	prop= RNA_def_property(srna, "deform_bbone_rest", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_B_BONE_REST);
	RNA_def_property_ui_text(prop, "B-Bones Deform in Rest Position", "Make B-Bones deform already in Rest Position");
	
	//prop= RNA_def_property(srna, "deform_invert_vertexgroups", PROP_BOOLEAN, PROP_NONE);
	//RNA_def_property_boolean_negative_sdna(prop, NULL, "deformflag", ARM_DEF_INVERT_VGROUP);
	//RNA_def_property_ui_text(prop, "Invert Vertex Group Influence", "Invert Vertex Group influence (only for Modifiers)");
	
		/* pathflag */
	prop= RNA_def_property(srna, "paths_show_frame_numbers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pathflag", ARM_PATH_FNUMS);
	RNA_def_property_ui_text(prop, "Show Frame Numbers on Bone Paths", "When drawing Armature in Pose Mode, show frame numbers on Bone Paths");
	
	prop= RNA_def_property(srna, "paths_highlight_keyframes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pathflag", ARM_PATH_KFRAS);
	RNA_def_property_ui_text(prop, "Highlight Keyframes on Bone Paths", "When drawing Armature in Pose Mode, emphasize position of keyframes on Bone Paths");
	
	prop= RNA_def_property(srna, "paths_show_keyframe_numbers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pathflag", ARM_PATH_KFNOS);
	RNA_def_property_ui_text(prop, "Show frame numbers of Keyframes on Bone Paths", "When drawing Armature in Pose Mode, show frame numbers of Keyframes on Bone Paths");
	
	prop= RNA_def_property(srna, "paths_show_around_current_frame", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pathflag", ARM_PATH_ACFRA);
	RNA_def_property_ui_text(prop, "Only show Bone Paths around current frame", "When drawing Armature in Pose Mode, only show section of Bone Paths that falls around current frame");
	
	/* Number fields */
	
	
}

void RNA_def_armature(BlenderRNA *brna)
{
	rna_def_armature(brna);
	rna_def_bone(brna);
}

#endif
