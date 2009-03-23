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
#include "DNA_scene_types.h"

#ifdef RNA_RUNTIME

static void rna_Bone_layer_set(PointerRNA *ptr, const int *values)
{
	Bone *bone= (Bone*)ptr->data;
	int i, tot= 0;

	/* ensure we always have some layer selected */
	for(i=0; i<20; i++)
		if(values[i])
			tot++;
	
	if(tot==0)
		return;

	for(i=0; i<20; i++) {
		if(values[i]) bone->layer |= (1<<i);
		else bone->layer &= ~(1<<i);
	}
}

static void rna_Armature_layer_set(PointerRNA *ptr, const int *values)
{
	bArmature *arm= (bArmature*)ptr->data;
	int i, tot= 0;

	/* ensure we always have some layer selected */
	for(i=0; i<20; i++)
		if(values[i])
			tot++;
	
	if(tot==0)
		return;

	for(i=0; i<20; i++) {
		if(values[i]) arm->layer |= (1<<i);
		else arm->layer &= ~(1<<i);
	}
}

static void rna_Armature_ghost_start_frame_set(PointerRNA *ptr, int value)
{
	bArmature *data= (bArmature*)ptr->data;
	CLAMP(value, 1, data->ghostef);
	data->ghostsf= value;
}

static void rna_Armature_ghost_end_frame_set(PointerRNA *ptr, int value)
{
	bArmature *data= (bArmature*)ptr->data;
	CLAMP(value, data->ghostsf, (int)(MAXFRAMEF/2));
	data->ghostef= value;
}

static void rna_Armature_path_start_frame_set(PointerRNA *ptr, int value)
{
	bArmature *data= (bArmature*)ptr->data;
	CLAMP(value, 1, data->pathef);
	data->pathsf= value;
}

static void rna_Armature_path_end_frame_set(PointerRNA *ptr, int value)
{
	bArmature *data= (bArmature*)ptr->data;
	CLAMP(value, data->pathsf, (int)(MAXFRAMEF/2));
	data->pathef= value;
}

#else

// err... bones should not be directly edited (only editbones should be...)
static void rna_def_bone(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Bone", NULL);
	RNA_def_struct_ui_text(srna, "Bone", "Bone in an Armature datablock.");
	
	/* pointers/collections */
		/* parent (pointer) */
	prop= RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_pointer_sdna(prop, NULL, "parent");
	RNA_def_property_ui_text(prop, "Parent", "Parent bone (in same Armature).");
	
		/* children (collection) */
	prop= RNA_def_property(srna, "children", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "childbase", NULL);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_ui_text(prop, "Children", "Bones which are children of this bone");
	
	/* strings */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* must be unique */
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	
	/* flags */
		/* layer */
	prop= RNA_def_property(srna, "layer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layer", 1);
	RNA_def_property_array(prop, 16);
	RNA_def_property_ui_text(prop, "Bone Layers", "Layers bone exists in");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Bone_layer_set");

		/* flag */
	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_SELECTED);
	RNA_def_property_ui_text(prop, "Selected", "");
	
	prop= RNA_def_property(srna, "head_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_ROOTSEL);
	RNA_def_property_ui_text(prop, "Head Selected", "");
	
	prop= RNA_def_property(srna, "tail_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_TIPSEL);
	RNA_def_property_ui_text(prop, "Tail Selected", "");
	
	prop= RNA_def_property(srna, "connected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_CONNECTED);
	RNA_def_property_ui_text(prop, "Connected", "When bone has a parent, bone's head is struck to the parent's tail.");
	
		// XXX should we define this in PoseChannel wrapping code instead? but PoseChannels directly get some of their flags from here...
	prop= RNA_def_property(srna, "pose_channel_hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_HIDDEN_P);
	RNA_def_property_ui_text(prop, "Pose Channel Hidden", "Bone is not visible when it is not in Edit Mode (i.e. in Object or Pose Modes).");
	
	prop= RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_ACTIVE);
	RNA_def_property_ui_text(prop, "Active", "Bone was the last bone clicked on (most operations are applied to only this bone)");
	
	prop= RNA_def_property(srna, "hinge", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_HINGE);
	RNA_def_property_ui_text(prop, "Hinge", "Bone doesn't inherit rotation or scale from parent bone.");
	
	prop= RNA_def_property(srna, "editmode_hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_HIDDEN_A);
	RNA_def_property_ui_text(prop, "Edit Mode Hidden", "Bone is not visible when in Edit Mode");
	
	prop= RNA_def_property(srna, "multiply_vertexgroup_with_envelope", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_MULT_VG_ENV);
	RNA_def_property_ui_text(prop, "Multiply Vertex Group with Envelope", "When deforming bone, multiply effects of Vertex Group weights with Envelope influence.");
	
	prop= RNA_def_property(srna, "deform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_NO_DEFORM);
	RNA_def_property_ui_text(prop, "Deform", "Bone does not deform any geometry.");
	
	prop= RNA_def_property(srna, "inherit_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_NO_SCALE);
	RNA_def_property_ui_text(prop, "Inherit scale", "Bone inherits scaling from parent bone.");
	
	prop= RNA_def_property(srna, "draw_wire", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_DRAWWIRE);
	RNA_def_property_ui_text(prop, "Draw Wire", "Bone is always drawn as Wireframe regardless of viewport draw mode. Useful for non-obstructive custom bone shapes.");
	
	prop= RNA_def_property(srna, "cyclic_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_NO_CYCLICOFFSET);
	RNA_def_property_ui_text(prop, "Cyclic Offset", "When bone doesn't have a parent, it recieves cyclic offset effects.");
	
	prop= RNA_def_property(srna, "editmode_locked", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_EDITMODE_LOCKED);
	RNA_def_property_ui_text(prop, "Edit Mode Locked", "Bone is not able to be transformed when in Edit Mode.");
	
	/* Number values */
		/* envelope deform settings */
	prop= RNA_def_property(srna, "envelope_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Envelope Deform Distance", "Bone deformation distance (for Envelope deform only).");
	
	prop= RNA_def_property(srna, "envelope_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Envelope Deform Weight", "Bone deformation weight (for Envelope deform only).");
	
	prop= RNA_def_property(srna, "radius_head", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rad_head");
	//RNA_def_property_range(prop, 0, 1000);  // XXX range is 0 to lim, where lim= 10000.0f*MAX2(1.0, view3d->grid);
	RNA_def_property_ui_text(prop, "Envelope Radius Head", "Radius of head of bone (for Envelope deform only).");
	
	prop= RNA_def_property(srna, "radius_tail", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rad_tail");
	//RNA_def_property_range(prop, 0, 1000);  // XXX range is 0 to lim, where lim= 10000.0f*MAX2(1.0, view3d->grid);
	RNA_def_property_ui_text(prop, "Envelope Radius Tail", "Radius of tail of bone (for Envelope deform only).");
	
		/* b-bones deform settings */
	prop= RNA_def_property(srna, "bbone_segments", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "segments");
	RNA_def_property_range(prop, 1, 32);
	RNA_def_property_ui_text(prop, "B-Bone Segments", "Number of subdivisions of bone (for B-Bones only).");
	
	prop= RNA_def_property(srna, "bbone_in", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ease1");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "B-Bone Ease In", "Length of first Bezier Handle (for B-Bones only).");
	
	prop= RNA_def_property(srna, "bbone_out", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ease2");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "B-Bone Ease Out", "Length of second Bezier Handle (for B-Bones only).");
	
		/* editmode bone coordinates */
	// XXX not sure if we want to wrap these here... besides, changing these requires changing the matrix?
	prop= RNA_def_property(srna, "head", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_ui_text(prop, "Bone Head Location", "In Edit Mode, the location of the 'head' of the bone.");
	
	prop= RNA_def_property(srna, "tail", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_ui_text(prop, "Bone Tail Location", "In Edit Mode, the location of the 'head' of the bone.");
	
	prop= RNA_def_property(srna, "roll", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Bone Roll", "In Edit Mode, the 'roll' (i.e. rotation around the bone vector, equivilant to local Y-axis rotation).");
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
	static EnumPropertyItem prop_ghost_type_items[] = {
		{ARM_GHOST_CUR, "CURRENT_FRAME", "Around Current Frame", "Draw Ghosts of poses within a fixed number of frames around the current frame."},
		{ARM_GHOST_RANGE, "RANGE", "In Range", "Draw Ghosts of poses within specified range."},
		{ARM_GHOST_KEYS, "KEYS", "On Keyframes", "Draw Ghosts of poses on Keyframes."},
		{0, NULL, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "Armature", "ID");
	RNA_def_struct_ui_text(srna, "Armature", "Armature datablock containing a hierarchy of bones, usually used for rigging characters.");
	
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
	
	prop= RNA_def_property(srna, "ghost_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ghosttype");
	RNA_def_property_enum_items(prop, prop_ghost_type_items);
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
	RNA_def_property_ui_text(prop, "Bone Paths Show Frame Numbers", "When drawing Armature in Pose Mode, show frame numbers on Bone Paths");
	
	prop= RNA_def_property(srna, "paths_highlight_keyframes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pathflag", ARM_PATH_KFRAS);
	RNA_def_property_ui_text(prop, "Bone Paths Highlight Keyframes", "When drawing Armature in Pose Mode, emphasize position of keyframes on Bone Paths");
	
	prop= RNA_def_property(srna, "paths_show_keyframe_numbers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pathflag", ARM_PATH_KFNOS);
	RNA_def_property_ui_text(prop, "Bone Paths Show Keyframe Numbers", "When drawing Armature in Pose Mode, show frame numbers of Keyframes on Bone Paths");
	
	prop= RNA_def_property(srna, "paths_show_around_current_frame", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pathflag", ARM_PATH_ACFRA);
	RNA_def_property_ui_text(prop, "Bone Paths Around Current Frame", "When drawing Armature in Pose Mode, only show section of Bone Paths that falls around current frame");
	
	prop= RNA_def_property(srna, "paths_calculate_head_positions", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pathflag", ARM_PATH_HEADS);
	RNA_def_property_ui_text(prop, "Bone Paths Use Heads", "When calculating Bone Paths, use Head locations instead of Tips");
	
	/* Number fields */
		/* ghost/onionskining settings */
	prop= RNA_def_property(srna, "ghost_step", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ghostep");
	RNA_def_property_range(prop, 0, 30);
	RNA_def_property_ui_text(prop, "Ghosting Step", "Number of frame steps on either side of current frame to show as ghosts (only for 'Around Current Frame' Onion-skining method).");
	
	prop= RNA_def_property(srna, "ghost_size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ghostsize");
	RNA_def_property_range(prop, 1, 20);
	RNA_def_property_ui_text(prop, "Ghosting Frame Step", "Frame step for Ghosts (not for 'On Keyframes' Onion-skining method).");
	
	prop= RNA_def_property(srna, "ghost_start_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ghostsf");
	RNA_def_property_int_funcs(prop, NULL, "rna_Armature_ghost_start_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Ghosting Start Frame", "Starting frame of range of Ghosts to display (not for 'Around Current Frame' Onion-skinning method).");
	
	prop= RNA_def_property(srna, "ghost_end_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ghostef");
	RNA_def_property_int_funcs(prop, NULL, "rna_Armature_ghost_end_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Ghosting End Frame", "End frame of range of Ghosts to display (not for 'Around Current Frame' Onion-skinning method).");
	
		/* bone path settings */
	prop= RNA_def_property(srna, "path_size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pathsize");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Bone Paths Frame Step", "Number of frames between 'dots' on Bone Paths (when drawing).");
	
	prop= RNA_def_property(srna, "path_start_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pathsf");
	RNA_def_property_int_funcs(prop, NULL, "rna_Armature_path_start_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Bone Paths Calculation Start Frame", "Starting frame of range of frames to use for Bone Path calculations.");
	
	prop= RNA_def_property(srna, "path_end_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pathef");
	RNA_def_property_int_funcs(prop, NULL, "rna_Armature_path_end_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Bone Paths Calculation End Frame", "End frame of range of frames to use for Bone Path calculations.");
	
	prop= RNA_def_property(srna, "path_before_current", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pathbc");
	RNA_def_property_range(prop, 1, MAXFRAMEF/2);
	RNA_def_property_ui_text(prop, "Bone Paths Frames Before Current", "Number of frames before current frame to show on Bone Paths (only for 'Around Current' option).");
	
	prop= RNA_def_property(srna, "path_after_current", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pathac");
	RNA_def_property_range(prop, 1, MAXFRAMEF/2);
	RNA_def_property_ui_text(prop, "Bone Paths Frames After Current", "Number of frames after current frame to show on Bone Paths (only for 'Around Current' option).");
}

void RNA_def_armature(BlenderRNA *brna)
{
	rna_def_armature(brna);
	rna_def_bone(brna);
}

#endif
