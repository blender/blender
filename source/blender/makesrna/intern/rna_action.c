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
 * Contributor(s): Blender Foundation (2008), Roland Hess, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_action_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "WM_types.h"


#ifdef RNA_RUNTIME

#else

static void rna_def_dopesheet(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "DopeSheet", NULL);
	RNA_def_struct_sdna(srna, "bDopeSheet");
	RNA_def_struct_ui_text(srna, "DopeSheet", "Storage for Dopesheet/Grease-Pencil Editor data.");

	prop= RNA_def_property(srna, "source", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_ui_text(prop, "Source", "ID-Block representing source data, currently ID_SCE (for Dopesheet), and ID_SC (for Grease Pencil).");

	prop= RNA_def_property(srna, "only_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filterflag", ADS_FILTER_ONLYSEL);
	RNA_def_property_ui_text(prop, "Only Selected", "Only include channels relating to selected Objects.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "only_drivers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filterflag", ADS_FILTER_ONLYDRIVERS);
	RNA_def_property_ui_text(prop, "Only Drivers", "Only include Driver data from Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "only_nla", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filterflag", ADS_FILTER_ONLYNLA);
	RNA_def_property_ui_text(prop, "Only NLA", "Only include NLA data from Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "use_filter", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filterflag", ADS_FILTER_SELEDIT);
	RNA_def_property_ui_text(prop, "Use Filter", "Indicates if filtering options must be taken into account.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_summary", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filterflag", ADS_FILTER_SUMMARY);
	RNA_def_property_ui_text(prop, "Display Summary", "Display an additional 'summary' line.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_shapekeys", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOSHAPEKEYS);
	RNA_def_property_ui_text(prop, "Display Shapekeys", "Include visualization of Shapekey related Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_camera", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOCAM);
	RNA_def_property_ui_text(prop, "Display Camera", "Include visualization of Camera related Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_material", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOMAT);
	RNA_def_property_ui_text(prop, "Display Material", "Include visualization of Material related Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_lamp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOLAM);
	RNA_def_property_ui_text(prop, "Display Lamp", "Include visualization of Lamp related Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_curve", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOCUR);
	RNA_def_property_ui_text(prop, "Display Curve", "Include visualization of Curve related Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_world", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOWOR);
	RNA_def_property_ui_text(prop, "Display World", "Include visualization of World related Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_scene", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOSCE);
	RNA_def_property_ui_text(prop, "Display Scene", "Include visualization of Scene related Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_particle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOPART);
	RNA_def_property_ui_text(prop, "Display Particle", "Include visualization of Particle related Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_metaball", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOMBA);
	RNA_def_property_ui_text(prop, "Display Metaball", "Include visualization of Metaball related Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_armature", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOARM);
	RNA_def_property_ui_text(prop, "Display Armature", "Include visualization of Armature related Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "display_node", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NONTREE);
	RNA_def_property_ui_text(prop, "Display Node", "Include visualization of Node related Animation data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "include_missing_nla", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NLA_NOACT);
	RNA_def_property_ui_text(prop, "Include Missing NLA", "Include Animation Data blocks with no NLA Data.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */

	prop= RNA_def_property(srna, "collapse_summary", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ADS_FLAG_SUMMARY_COLLAPSED);
	RNA_def_property_ui_text(prop, "Collapse Summary", "Collapse summary when shown, so all other channels get hidden.");
	RNA_def_property_update(prop, NC_ANIMATION, NULL); /* XXX fix notifier */
}

static void rna_def_action_group(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "ActionGroup", NULL);
	RNA_def_struct_sdna(srna, "bActionGroup");
	RNA_def_struct_ui_text(srna, "Action Group", "Groups of F-Curves.");
	
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	
	/* dna warns not to treat the Action Channel listbase in the Action Group struct like a
	   normal listbase. I'll leave this here but comment out, for Joshua to review. He can 
 	   probably shed some more light on why this is */
	/*prop= RNA_def_property(srna, "channels", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "channels", NULL);
	RNA_def_property_struct_type(prop, "FCurve");
	RNA_def_property_ui_text(prop, "Channels", "F-Curves in this group.");*/
	
	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_SELECTED);
	RNA_def_property_ui_text(prop, "Selected", "Action Group is selected.");
	
	prop= RNA_def_property(srna, "locked", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_PROTECTED);
	RNA_def_property_ui_text(prop, "Locked", "Action Group is locked.");
	
	prop= RNA_def_property(srna, "expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_EXPANDED);
	RNA_def_property_ui_text(prop, "Expanded", "Action Group is expanded.");
	
	prop= RNA_def_property(srna, "custom_color", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "customCol");
	RNA_def_property_ui_text(prop, "Custom Color", "Index of custom color set.");
}

static void rna_def_action(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Action", "ID");
	RNA_def_struct_sdna(srna, "bAction");
	RNA_def_struct_ui_text(srna, "Action", "A collection of F-Curves for animation.");
	RNA_def_struct_ui_icon(srna, ICON_ACTION);

	prop= RNA_def_property(srna, "fcurves", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "curves", NULL);
	RNA_def_property_struct_type(prop, "FCurve");
	RNA_def_property_ui_text(prop, "F-Curves", "The individual F-Curves that make up the Action.");

	prop= RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "groups", NULL);
	RNA_def_property_struct_type(prop, "ActionGroup");
	RNA_def_property_ui_text(prop, "Groups", "Convenient groupings of F-Curves.");

	prop= RNA_def_property(srna, "pose_markers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "markers", NULL);
	RNA_def_property_struct_type(prop, "TimelineMarker");
	RNA_def_property_ui_text(prop, "Pose Markers", "Markers specific to this Action, for labeling poses.");

	RNA_api_action(srna);
}

/* --------- */

void RNA_def_action(BlenderRNA *brna)
{
	rna_def_action(brna);
	rna_def_action_group(brna);
	rna_def_dopesheet(brna);
}


#endif
