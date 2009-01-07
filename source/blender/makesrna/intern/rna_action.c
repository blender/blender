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
 * Contributor(s): Blender Foundation (2008), Roland Hess
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_scene_types.h"


#ifdef RNA_RUNTIME

#else

void rna_def_action_channel(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ActionChannel", NULL);
	RNA_def_struct_sdna(srna, "bActionChannel");
	RNA_def_struct_ui_text(srna, "Action Channel", "A channel for one object or bone's Ipos in an Action.");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "action_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "grp");
	RNA_def_property_struct_type(prop, "ActionGroup");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Action Group", "Action Group that this Action Channel belongs to.");

	prop= RNA_def_property(srna, "ipo", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Ipo");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Ipo", "Ipo block this Action Channel uses.");	

	/* constraint channel rna not yet implemented */
	/*prop= RNA_def_property(srna, "constraint_channels", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "ConstraintChannel", NULL);
	RNA_def_property_struct_type(prop, "ConstraintChannel");
	RNA_def_property_ui_text(prop, "Constraint Channels", "Ipos of Constraints attached to this object or bone."); */

	prop= RNA_def_property(srna, "action_channel_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACHAN_SELECTED);
	RNA_def_property_ui_text(prop, "Selected", "Action Channel is selected.");

	prop= RNA_def_property(srna, "action_channel_highlighted", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACHAN_HILIGHTED);
	RNA_def_property_ui_text(prop, "Highlighted", "Action Channel is highlighted.");

	prop= RNA_def_property(srna, "action_channel_hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACHAN_HIDDEN);
	RNA_def_property_ui_text(prop, "Hidden", "Action Channel is hidden.");

	prop= RNA_def_property(srna, "action_channel_protected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACHAN_PROTECTED);
	RNA_def_property_ui_text(prop, "Protected", "Action Channel is protected.");

	prop= RNA_def_property(srna, "action_channel_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACHAN_EXPANDED);
	RNA_def_property_ui_text(prop, "Expanded", "Action Channel is expanded.");

	prop= RNA_def_property(srna, "action_channel_show_ipo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACHAN_SHOWIPO);
	RNA_def_property_ui_text(prop, "Show Ipo", "Action Channel's Ipos are visible.");

	prop= RNA_def_property(srna, "action_channel_show_constraints", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACHAN_SHOWCONS);
	RNA_def_property_ui_text(prop, "Show Constraints", "Action Channel's constraints are visible.");
}

void rna_def_action_group(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ActionGroup", NULL);
	RNA_def_struct_sdna(srna, "bActionGroup");
	RNA_def_struct_ui_text(srna, "Action Group", "Groups of Actions Channels.");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);

	/* dna warns not to treat the Action Channel listbase in the Action Group struct like a
	   normal listbase. I'll leave this here but comment out, for Joshua to review. He can 
 	   probably shed some more light on why this is */
	/*prop= RNA_def_property(srna, "action_channels", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "channels", NULL);
	RNA_def_property_struct_type(prop, "ActionChannel");
	RNA_def_property_ui_text(prop, "Action Channels", "DOC_BROKEN");*/

	prop= RNA_def_property(srna, "action_group_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_SELECTED);
	RNA_def_property_ui_text(prop, "Selected", "Action Group is selected.");

	prop= RNA_def_property(srna, "action_group_protected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_PROTECTED);
	RNA_def_property_ui_text(prop, "Protected", "Action Group is protected.");

	prop= RNA_def_property(srna, "action_group_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_EXPANDED);
	RNA_def_property_ui_text(prop, "Expanded", "Action Group is expanded.");

	prop= RNA_def_property(srna, "custom_color", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "customCol");
	RNA_def_property_ui_text(prop, "Custom Color", "Index of custom color set.");
}

void RNA_def_action(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	rna_def_action_channel(brna);
	rna_def_action_group(brna);

	srna= RNA_def_struct(brna, "Action", "ID");
	RNA_def_struct_sdna(srna, "bAction");
	RNA_def_struct_ui_text(srna, "Action", "A collection of Ipos for animation.");

	prop= RNA_def_property(srna, "action_channels", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "chanbase", NULL);
	RNA_def_property_struct_type(prop, "ActionChannel");
	RNA_def_property_ui_text(prop, "Action Channels", "The individual animation channels that make up the Action.");

	prop= RNA_def_property(srna, "action_groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "groups", NULL);
	RNA_def_property_struct_type(prop, "ActionGroup");
	RNA_def_property_ui_text(prop, "Action Groups", "Convenient groupings of Action Channels.");

	prop= RNA_def_property(srna, "timeline_markers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "markers", NULL);
	RNA_def_property_struct_type(prop, "UnknownType"); /* implement when timeline rna is wrapped */
	RNA_def_property_ui_text(prop, "Timeline Markers", "Markers specific to this Action, for labeling poses.");

	prop= RNA_def_property(srna, "action_show_sliders", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_SLIDERS);
	RNA_def_property_ui_text(prop, "Show Sliders", "Show Shape Key sliders.");

	prop= RNA_def_property(srna, "action_time_units", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_DRAWTIME);
	RNA_def_property_ui_text(prop, "Time Units", "Show seconds or frames in the timeline.");

	prop= RNA_def_property(srna, "action_show_all", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_NOHIDE);
	RNA_def_property_ui_text(prop, "Show All", "Show all channels regardless of hidden status.");

	prop= RNA_def_property(srna, "action_kill_overlapping_keys", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_NOTRANSKEYCULL);
	RNA_def_property_ui_text(prop, "Kill Overlapping Keys", "Remove overlapping keys after a transform.");

	prop= RNA_def_property(srna, "action_key_cull_to_view", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_HORIZOPTIMISEON);
	RNA_def_property_ui_text(prop, "Cull Keys to View", "Only consider keys that are within the view.");

	prop= RNA_def_property(srna, "action_group_colors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_NODRAWGCOLORS);
	RNA_def_property_ui_text(prop, "Group Color", "Use custom color grouping and instead of default color scheme.");

	prop= RNA_def_property(srna, "action_current_frame_number", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_NODRAWCFRANUM);
	RNA_def_property_ui_text(prop, "Current Frame Number", "Draw the frame number beside the current frame indicator.");

}

#endif
