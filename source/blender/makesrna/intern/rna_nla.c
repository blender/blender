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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation (2009), Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#ifdef RNA_RUNTIME

#include <stdio.h>
#include <math.h>

/* needed for some of the validation stuff... */
#include "BKE_animsys.h"
#include "BKE_nla.h"

/* temp constant defined for these funcs only... */
#define NLASTRIP_MIN_LEN_THRESH 	0.1f

static void rna_NlaStrip_name_set(PointerRNA *ptr, const char *value)
{
	NlaStrip *data= (NlaStrip *)ptr->data;
	
	/* copy the name first */
	BLI_strncpy(data->name, value, sizeof(data->name));
	
	/* validate if there's enough info to do so */
	if (ptr->id.data) {
		AnimData *adt= BKE_animdata_from_id(ptr->id.data);
		BKE_nlastrip_validate_name(adt, data);
	}
}

static char *rna_NlaStrip_path(PointerRNA *ptr)
{
	NlaStrip *strip= (NlaStrip *)ptr->data;
	AnimData *adt= BKE_animdata_from_id(ptr->id.data);
	
	/* if we're attached to AnimData, try to resolve path back to AnimData */
	if (adt) {
		NlaTrack *nlt;
		NlaStrip *nls;
		
		for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next) {
			for (nls = nlt->strips.first; nls; nls = nls->next) {
				if (nls == strip) {
					// XXX but if we animate like this, the control will never work...
					return BLI_sprintfN("animation_data.nla_tracks[\"%s\"].strips[\"%s\"]", nlt->name, strip->name);
				}
			}
		}
	}
	
	/* no path */
	return "";
}

static void rna_NlaStrip_transform_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	NlaStrip *strip= (NlaStrip*)ptr->data;

	BKE_nlameta_flush_transforms(strip);
}

static void rna_NlaStrip_start_frame_set(PointerRNA *ptr, float value)
{
	NlaStrip *data= (NlaStrip*)ptr->data;
	
	/* clamp value to lie within valid limits 
	 *	- cannot start past the end of the strip + some flexibility threshold
	 *	- cannot start before the previous strip (if present) ends
	 *		-> but if it was a transition, we could go up to the start of the strip + some flexibility threshold
	 *		as long as we re-adjust the transition afterwards
	 *	- minimum frame is -MAXFRAME so that we don't get clipping on frame 0
	 */
	if (data->prev) {
		if (data->prev->type == NLASTRIP_TYPE_TRANSITION) {
			CLAMP(value, data->prev->start+NLASTRIP_MIN_LEN_THRESH, data->end-NLASTRIP_MIN_LEN_THRESH);
			
			/* readjust the transition to stick to the endpoints of the action-clips */
			data->prev->end= value;
		}
		else {
			CLAMP(value, data->prev->end, data->end-NLASTRIP_MIN_LEN_THRESH);
		}
	}
	else {
		CLAMP(value, MINAFRAME, data->end);
	}
	data->start= value;
}

static void rna_NlaStrip_end_frame_set(PointerRNA *ptr, float value)
{
	NlaStrip *data= (NlaStrip*)ptr->data;
	
	/* clamp value to lie within valid limits
	 *	- must not have zero or negative length strip, so cannot start before the first frame 
	 *	  + some minimum-strip-length threshold
	 *	- cannot end later than the start of the next strip (if present)
	 *		-> but if it was a transition, we could go up to the start of the end - some flexibility threshold
	 *		as long as we re-adjust the transition afterwards
	 */
	if (data->next) {
		if (data->next->type == NLASTRIP_TYPE_TRANSITION) {
			CLAMP(value, data->start+NLASTRIP_MIN_LEN_THRESH, data->next->end-NLASTRIP_MIN_LEN_THRESH);
			
			/* readjust the transition to stick to the endpoints of the action-clips */
			data->next->start= value;
		}
		else {
			CLAMP(value, data->start+NLASTRIP_MIN_LEN_THRESH, data->next->start);
		}
	}
	else {
		CLAMP(value, data->start+NLASTRIP_MIN_LEN_THRESH, MAXFRAME);
	}
	data->end= value;
	
	
	/* calculate the lengths the strip and its action (if applicable) */
	if (data->type == NLASTRIP_TYPE_CLIP) {
		float len, actlen;
		
		len= data->end - data->start;
		actlen= data->actend - data->actstart;
		if (IS_EQ(actlen, 0.0f)) actlen= 1.0f;
		
		/* now, adjust the 'scale' setting to reflect this (so that this change can be valid) */
		data->scale= len / ((actlen) * data->repeat);
	}
}

static void rna_NlaStrip_scale_set(PointerRNA *ptr, float value)
{
	NlaStrip *data= (NlaStrip*)ptr->data;
	
	/* set scale value */
	CLAMP(value, 0.0001f, 1000.0f); /* NOTE: these need to be synced with the values in the property definition in rna_def_nlastrip() */
	data->scale= value;
	
	/* adjust the strip extents in response to this */
	BKE_nlastrip_recalculate_bounds(data);
}

static void rna_NlaStrip_repeat_set(PointerRNA *ptr, float value)
{
	NlaStrip *data= (NlaStrip*)ptr->data;
	
	/* set repeat value */
	CLAMP(value, 0.01f, 1000.0f); /* NOTE: these need to be synced with the values in the property definition in rna_def_nlastrip() */
	data->repeat= value;
	
	/* adjust the strip extents in response to this */
	BKE_nlastrip_recalculate_bounds(data);
}

static void rna_NlaStrip_blend_in_set(PointerRNA *ptr, float value)
{
	NlaStrip *data= (NlaStrip*)ptr->data;
	float len;
	
	/* blend-in is limited to the length of the strip, and also cannot overlap with blendout */
	len= (data->end - data->start) - data->blendout;
	CLAMP(value, 0, len);
	
	data->blendin= value;
}

static void rna_NlaStrip_blend_out_set(PointerRNA *ptr, float value)
{
	NlaStrip *data= (NlaStrip*)ptr->data;
	float len;
	
	/* blend-out is limited to the length of the strip */
	len= (data->end - data->start);
	CLAMP(value, 0, len);
	
	/* it also cannot overlap with blendin */
	if ((len - value) < data->blendin)
		value= len - data->blendin;
	
	data->blendout= value;
}

static void rna_NlaStrip_action_start_frame_set(PointerRNA *ptr, float value)
{
	NlaStrip *data= (NlaStrip*)ptr->data;
	
	/* prevent start frame from occurring after end of action */
	CLAMP(value, MINAFRAME, data->actend);
	data->actstart= value;
	
	/* adjust the strip extents in response to this */
	// TODO: should the strip be moved backwards instead as a special case?
	BKE_nlastrip_recalculate_bounds(data);
}

static void rna_NlaStrip_action_end_frame_set(PointerRNA *ptr, float value)
{
	NlaStrip *data= (NlaStrip*)ptr->data;
	
	/* prevent end frame from starting before start of action */
	CLAMP(value, data->actstart, MAXFRAME);
	data->actend= value;
	
	/* adjust the strip extents in response to this */
	BKE_nlastrip_recalculate_bounds(data);
}

static void rna_NlaStrip_animated_influence_set(PointerRNA *ptr, int value)
{
	NlaStrip *data= (NlaStrip*)ptr->data;
	
	if (value) {
		/* set the flag, then make sure a curve for this exists */
		data->flag |= NLASTRIP_FLAG_USR_INFLUENCE;
		BKE_nlastrip_validate_fcurves(data);
	}
	else
		data->flag &= ~NLASTRIP_FLAG_USR_INFLUENCE;
}

static void rna_NlaStrip_animated_time_set(PointerRNA *ptr, int value)
{
	NlaStrip *data= (NlaStrip*)ptr->data;
	
	if (value) {
		/* set the flag, then make sure a curve for this exists */
		data->flag |= NLASTRIP_FLAG_USR_TIME;
		BKE_nlastrip_validate_fcurves(data);
	}
	else
		data->flag &= ~NLASTRIP_FLAG_USR_TIME;
}

#else

/* enum defines exported for rna_animation.c */
EnumPropertyItem nla_mode_blend_items[] = {
	{NLASTRIP_MODE_REPLACE, "REPLACE", 0, "Replace", "Result strip replaces the accumulated results by amount specified by influence"},
	{NLASTRIP_MODE_ADD, "ADD", 0, "Add", "Weighted result of strip is added to the accumlated results"},
	{NLASTRIP_MODE_SUBTRACT, "SUBTRACT", 0, "Subtract", "Weighted result of strip is removed from the accumlated results"},
	{NLASTRIP_MODE_MULTIPLY, "MULITPLY", 0, "Multiply", "Weighted result of strip is multiplied with the accumlated results"},
	{0, NULL, 0, NULL, NULL}};
EnumPropertyItem nla_mode_extend_items[] = {
	{NLASTRIP_EXTEND_NOTHING, "NOTHING", 0, "Nothing", "Strip has no influence past its extents"},
	{NLASTRIP_EXTEND_HOLD, "HOLD", 0, "Hold", "Hold the first frame if no previous strips in track, and always hold last frame"},
	{NLASTRIP_EXTEND_HOLD_FORWARD, "HOLD_FORWARD", 0, "Hold Forward", "Only hold last frame"},
	{0, NULL, 0, NULL, NULL}};

static void rna_def_nlastrip(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
		/* enum defs */
	static EnumPropertyItem prop_type_items[] = {
		{NLASTRIP_TYPE_CLIP, "CLIP", 0, "Action Clip", "NLA Strip references some Action"},
		{NLASTRIP_TYPE_TRANSITION, "TRANSITION", 0, "Transition", "NLA Strip 'transitions' between adjacent strips"},
		{NLASTRIP_TYPE_META, "META", 0, "Meta", "NLA Strip acts as a container for adjacent strips"},
		{0, NULL, 0, NULL, NULL}};
	
	/* struct definition */
	srna= RNA_def_struct(brna, "NlaStrip", NULL);
	RNA_def_struct_ui_text(srna, "NLA Strip", "A container referencing an existing Action");
	RNA_def_struct_path_func(srna, "rna_NlaStrip_path");
	RNA_def_struct_ui_icon(srna, ICON_NLA); // XXX
	
	/* name property */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_NlaStrip_name_set");
	RNA_def_struct_name_property(srna, prop);
	
	/* Enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // XXX for now, not editable, since this is dangerous
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Type of NLA Strip");
	
	prop= RNA_def_property(srna, "extrapolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "extendmode");
	RNA_def_property_enum_items(prop, nla_mode_extend_items);
	RNA_def_property_ui_text(prop, "Extrapolation", "Action to take for gaps past the strip extents");
	
	prop= RNA_def_property(srna, "blending", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "blendmode");
	RNA_def_property_enum_items(prop, nla_mode_blend_items);
	RNA_def_property_ui_text(prop, "Blending", "Method used for combining strip's result with accumulated result");
	
	/* Strip extents */
	prop= RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "start");
	RNA_def_property_float_funcs(prop, NULL, "rna_NlaStrip_start_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Start Frame", "");
	RNA_def_property_update(prop, 0, "rna_NlaStrip_transform_update");
	
	prop= RNA_def_property(srna, "frame_end", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "end");
	RNA_def_property_float_funcs(prop, NULL, "rna_NlaStrip_end_frame_set", NULL);
	RNA_def_property_ui_text(prop, "End Frame", "");
	RNA_def_property_update(prop, 0, "rna_NlaStrip_transform_update");
	
	/* Blending */
	prop= RNA_def_property(srna, "blend_in", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "blendin");
	RNA_def_property_float_funcs(prop, NULL, "rna_NlaStrip_blend_in_set", NULL);
	RNA_def_property_ui_text(prop, "Blend In", "Number of frames at start of strip to fade in influence");
	
	prop= RNA_def_property(srna, "blend_out", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "blendout");
	RNA_def_property_float_funcs(prop, NULL, "rna_NlaStrip_blend_out_set", NULL);
	RNA_def_property_ui_text(prop, "Blend Out", "");
	
	prop= RNA_def_property(srna, "auto_blending", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLASTRIP_FLAG_AUTO_BLENDS);
	RNA_def_property_ui_text(prop, "Auto Blend In/Out", "Number of frames for Blending In/Out is automatically determined from overlapping strips");
	
	/* Action */
	prop= RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "act");
	RNA_def_property_flag(prop, PROP_EDITABLE); 
	RNA_def_property_ui_text(prop, "Action", "Action referenced by this strip");
	
	/* Action extents */
	prop= RNA_def_property(srna, "action_frame_start", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "actstart");
	RNA_def_property_float_funcs(prop, NULL, "rna_NlaStrip_action_start_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Action Start Frame", "");
	
	prop= RNA_def_property(srna, "action_frame_end", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "actend");
	RNA_def_property_float_funcs(prop, NULL, "rna_NlaStrip_action_end_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Action End Frame", "");
	
	/* Action Reuse */
	prop= RNA_def_property(srna, "repeat", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "repeat"); 
	RNA_def_property_float_funcs(prop, NULL, "rna_NlaStrip_repeat_set", NULL);
	RNA_def_property_range(prop, 0.1f, 1000.0f); /* these limits have currently be chosen arbitarily, but could be extended (minimum should still be > 0 though) if needed... */
	RNA_def_property_ui_text(prop, "Repeat", "Number of times to repeat the action range");
	
	prop= RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "scale"); 
	RNA_def_property_float_funcs(prop, NULL, "rna_NlaStrip_scale_set", NULL);
	RNA_def_property_range(prop, 0.0001f, 1000.0f); /* these limits can be extended, but beyond this, we can get some crazy+annoying bugs due to numeric errors */
	RNA_def_property_ui_text(prop, "Scale", "Scaling factor for action");
	
	/* Strip's F-Curves */
	prop= RNA_def_property(srna, "fcurves", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "FCurve");
	RNA_def_property_ui_text(prop, "F-Curves", "F-Curves for controlling the strip's influence and timing");
	
	/* Strip's F-Modifiers */
	prop= RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "FModifier");
	RNA_def_property_ui_text(prop, "Modifiers", "Modifiers affecting all the F-Curves in the referenced Action");
	
	/* Strip's Sub-Strips (for Meta-Strips) */
	prop= RNA_def_property(srna, "strips", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "NlaStrip");
	RNA_def_property_ui_text(prop, "NLA Strips", "NLA Strips that this strip acts as a container for (if it is of type Meta)");
	
	/* Settings - Values necessary for evaluation */
	prop= RNA_def_property(srna, "influence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Influence", "Amount the strip contributes to the current result");
	
	prop= RNA_def_property(srna, "strip_time", PROP_FLOAT, PROP_TIME);
	RNA_def_property_ui_text(prop, "Strip Time", "Frame of referenced Action to evaluate");
	
		// TODO: should the animated_influence/time settings be animatable themselves?
	prop= RNA_def_property(srna, "animated_influence", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLASTRIP_FLAG_USR_INFLUENCE);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_NlaStrip_animated_influence_set");
	RNA_def_property_ui_text(prop, "Animated Influence", "Influence setting is controlled by an F-Curve rather than automatically determined");
	
	prop= RNA_def_property(srna, "animated_time", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLASTRIP_FLAG_USR_TIME);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_NlaStrip_animated_time_set");
	RNA_def_property_ui_text(prop, "Animated Strip Time", "Strip time is controlled by an F-Curve rather than automatically determined");

	prop= RNA_def_property(srna, "animated_time_cyclic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLASTRIP_FLAG_USR_TIME_CYCLIC);
	RNA_def_property_ui_text(prop, "Cyclic Strip Time", "Cycle the animated time within the action start & end");
	RNA_def_property_update(prop, 0, "rna_NlaStrip_transform_update"); // is there a better update flag?
	
	/* settings */
	prop= RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* can be made editable by hooking it up to the necessary NLA API methods */
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLASTRIP_FLAG_ACTIVE);
	RNA_def_property_ui_text(prop, "Active", "NLA Strip is active");
	
	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLASTRIP_FLAG_SELECT);
	RNA_def_property_ui_text(prop, "Selected", "NLA Strip is selected");
	
	prop= RNA_def_property(srna, "muted", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLASTRIP_FLAG_MUTED);
	RNA_def_property_ui_text(prop, "Muted", "NLA Strip is not evaluated");
	
	prop= RNA_def_property(srna, "reversed", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLASTRIP_FLAG_REVERSE);
	RNA_def_property_ui_text(prop, "Reversed", "NLA Strip is played back in reverse order (only when timing is automatically determined)");
	
	// TODO: 
	// - sync length
}

static void rna_def_nlatrack(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "NlaTrack", NULL);
	RNA_def_struct_ui_text(srna, "NLA Track", "A animation layer containing Actions referenced as NLA strips");
	RNA_def_struct_ui_icon(srna, ICON_NLA);
	
	/* strips collection */
	prop= RNA_def_property(srna, "strips", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "NlaStrip");
	RNA_def_property_ui_text(prop, "NLA Strips", "NLA Strips on this NLA-track");
	
	/* name property */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	
	/* settings */
	prop= RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* can be made editable by hooking it up to the necessary NLA API methods */
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLATRACK_ACTIVE);
	RNA_def_property_ui_text(prop, "Active", "NLA Track is active");
	
	prop= RNA_def_property(srna, "solo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* can be made editable by hooking it up to the necessary NLA API methods */
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLATRACK_SOLO);
	RNA_def_property_ui_text(prop, "Solo", "NLA Track is evaluated itself (i.e. active Action and all other NLA Tracks in the same AnimData block are disabled)");
	
	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLATRACK_SELECTED);
	RNA_def_property_ui_text(prop, "Selected", "NLA Track is selected");
	
	prop= RNA_def_property(srna, "muted", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLATRACK_MUTED);
	RNA_def_property_ui_text(prop, "Muted", "NLA Track is not evaluated");

	prop= RNA_def_property(srna, "locked", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NLATRACK_PROTECTED);
	RNA_def_property_ui_text(prop, "Locked", "NLA Track is locked");
}

/* --------- */

void RNA_def_nla(BlenderRNA *brna)
{
	rna_def_nlatrack(brna);
	rna_def_nlastrip(brna);
}


#endif
