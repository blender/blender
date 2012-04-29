/*
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
 * Contributor(s): Blender Foundation (2008), Roland Hess, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_action.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_action.h"

#include "WM_types.h"


#ifdef RNA_RUNTIME

#include "ED_keyframing.h"
#include "BKE_fcurve.h"

static void rna_ActionGroup_channels_next(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = iter->internal;
	FCurve *fcu = (FCurve*)internal->link;
	bActionGroup *grp = fcu->grp;
	
	/* only continue if the next F-Curve (if existant) belongs in the same group */
	if ((fcu->next) && (fcu->next->grp == grp))
		internal->link = (Link*)fcu->next;
	else
		internal->link = NULL;
		
	iter->valid = (internal->link != NULL);
}

static bActionGroup *rna_Action_groups_new(bAction *act, const char name[])
{
	return action_groups_add_new(act, name);
}

static void rna_Action_groups_remove(bAction *act, ReportList *reports, bActionGroup *agrp)
{
	FCurve *fcu, *fcn;
	
	/* try to remove the F-Curve from the action */
	if (!BLI_remlink_safe(&act->groups, agrp)) {
		BKE_reportf(reports, RPT_ERROR, "ActionGroup '%s' not found in action '%s'", agrp->name, act->id.name+2);
		return;
	}

	/* move every one one of the group's F-Curves out into the Action again */
	for (fcu = agrp->channels.first; (fcu) && (fcu->grp == agrp); fcu = fcn) {
		fcn = fcu->next;
		
		/* remove from group */
		action_groups_remove_channel(act, fcu);
		
		/* tack onto the end */
		BLI_addtail(&act->curves, fcu);
	}
	
	/* XXX, invalidates PyObject */
	MEM_freeN(agrp);
}

static FCurve *rna_Action_fcurve_new(bAction *act, ReportList *reports, const char *data_path,
                                     int index, const char *group)
{
	if (group && group[0] =='\0') group = NULL;

	if (data_path[0] == '\0') {
		BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
		return NULL;
	}

	/* annoying, check if this exists */
	if (verify_fcurve(act, group, data_path, index, 0)) {
		BKE_reportf(reports, RPT_ERROR, "F-Curve '%s[%d]' already exists in action '%s'", data_path,
		            index, act->id.name+2);
		return NULL;
	}
	return verify_fcurve(act, group, data_path, index, 1);
}

static void rna_Action_fcurve_remove(bAction *act, ReportList *reports, FCurve *fcu)
{
	if (fcu->grp) {
		if (BLI_findindex(&act->groups, fcu->grp) == -1) {
			BKE_reportf(reports, RPT_ERROR, "F-Curve's ActionGroup '%s' not found in action '%s'",
			            fcu->grp->name, act->id.name+2);
			return;
		}
		
		action_groups_remove_channel(act, fcu);
		free_fcurve(fcu);
	}
	else {
		if (BLI_findindex(&act->curves, fcu) == -1) {
			BKE_reportf(reports, RPT_ERROR, "F-Curve not found in action '%s'", act->id.name+2);
			return;
		}
		
		BLI_remlink(&act->curves, fcu);
		free_fcurve(fcu);
	}
}

static TimeMarker *rna_Action_pose_markers_new(bAction *act, const char name[])
{
	TimeMarker *marker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
	marker->flag = 1;
	marker->frame = 1;
	BLI_strncpy_utf8(marker->name, name, sizeof(marker->name));
	BLI_addtail(&act->markers, marker);
	return marker;
}

static void rna_Action_pose_markers_remove(bAction *act, ReportList *reports, TimeMarker *marker)
{
	if (!BLI_remlink_safe(&act->markers, marker)) {
		BKE_reportf(reports, RPT_ERROR, "TimelineMarker '%s' not found in Action '%s'", marker->name, act->id.name+2);
		return;
	}

	/* XXX, invalidates PyObject */
	MEM_freeN(marker);
}

static PointerRNA rna_Action_active_pose_marker_get(PointerRNA *ptr)
{
	bAction *act = (bAction*)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_TimelineMarker, BLI_findlink(&act->markers, act->active_marker-1));
}

static void rna_Action_active_pose_marker_set(PointerRNA *ptr, PointerRNA value)
{
	bAction *act = (bAction*)ptr->data;
	act->active_marker = BLI_findindex(&act->markers, value.data) + 1;
}

static int rna_Action_active_pose_marker_index_get(PointerRNA *ptr)
{
	bAction *act = (bAction*)ptr->data;
	return MAX2(act->active_marker-1, 0);
}

static void rna_Action_active_pose_marker_index_set(PointerRNA *ptr, int value)
{
	bAction *act = (bAction*)ptr->data;
	act->active_marker = value+1;
}

static void rna_Action_active_pose_marker_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	bAction *act = (bAction*)ptr->data;

	*min = 0;
	*max = BLI_countlist(&act->markers)-1;
	*max = MAX2(0, *max);
}



static void rna_Action_frame_range_get(PointerRNA *ptr, float *values)
{	/* don't include modifiers because they too easily can have very large
	 * ranges: MINAFRAMEF to MAXFRAMEF. */
	calc_action_range(ptr->id.data, values, values+1, FALSE);
}


/* used to check if an action (value pointer) is suitable to be assigned to the ID-block that is ptr */
int rna_Action_id_poll(PointerRNA *ptr, PointerRNA value)
{
	ID *srcId = (ID *)ptr->id.data;
	bAction *act = (bAction *)value.id.data;
	
	if (act) {
		/* there can still be actions that will have undefined id-root
		 * (i.e. floating "action-library" members) which we will not
		 * be able to resolve an idroot for automatically, so let these through
		 */
		if (act->idroot == 0)
			return 1;
		else if (srcId)
			return GS(srcId->name) == act->idroot;
	}
	
	return 0;
}

/* used to check if an action (value pointer) can be assigned to Action Editor given current mode */
int rna_Action_actedit_assign_poll(PointerRNA *ptr, PointerRNA value)
{
	SpaceAction *saction = (SpaceAction *)ptr->data;
	bAction *act = (bAction *)value.id.data;
	
	if (act) {
		/* there can still be actions that will have undefined id-root
		 * (i.e. floating "action-library" members) which we will not
		 * be able to resolve an idroot for automatically, so let these through
		 */
		if (act->idroot == 0)
			return 1;
		
		if (saction) {
			if (saction->mode == SACTCONT_ACTION) {
				/* this is only Object-level for now... */
				return act->idroot == ID_OB;
			}
			else if (saction->mode == SACTCONT_SHAPEKEY) {
				/* obviously shapekeys only */
				return act->idroot == ID_KE;
			}
		}
	}
	
	return 0;
}

#else

static void rna_def_dopesheet(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DopeSheet", NULL);
	RNA_def_struct_sdna(srna, "bDopeSheet");
	RNA_def_struct_ui_text(srna, "DopeSheet", "Settings for filtering the channels shown in Animation Editors");
	
	/* Source of DopeSheet data */
	/* XXX: make this obsolete? */
	prop = RNA_def_property(srna, "source", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_ui_text(prop, "Source",
	                         "ID-Block representing source data, currently ID_SCE (for Dopesheet), "
	                         "and ID_SC (for Grease Pencil)");
	
	/* Show datablock filters */
	prop = RNA_def_property(srna, "show_datablock_filters", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ADS_FLAG_SHOW_DBFILTERS);
	RNA_def_property_ui_text(prop, "Show Datablock Filters",
	                         "Show options for whether channels related to certain types of data are included");
	RNA_def_property_ui_icon(prop, ICON_DISCLOSURE_TRI_RIGHT, -1);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN, NULL);
	
	/* General Filtering Settings */
	prop = RNA_def_property(srna, "show_only_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filterflag", ADS_FILTER_ONLYSEL);
	RNA_def_property_ui_text(prop, "Only Selected", "Only include channels relating to selected objects and data");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filterflag", ADS_FILTER_INCL_HIDDEN);
	RNA_def_property_ui_text(prop, "Display Hidden", "Include channels from objects/bone that aren't visible");
	RNA_def_property_ui_icon(prop, ICON_GHOST_ENABLED, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	/* Object Group Filtering Settings */
	prop = RNA_def_property(srna, "show_only_group_objects", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filterflag", ADS_FILTER_ONLYOBGROUP);
	RNA_def_property_ui_text(prop, "Only Objects in Group",
	                         "Only include channels from Objects in the specified Group");
	RNA_def_property_ui_icon(prop, ICON_GROUP, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "filter_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "filter_grp");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Filtering Group", "Group that included Object should be a member of");
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	/* FCurve Display Name Search Settings */
	prop = RNA_def_property(srna, "show_only_matching_fcurves", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filterflag", ADS_FILTER_BY_FCU_NAME);
	RNA_def_property_ui_text(prop, "Only Matching F-Curves",
	                         "Only include F-Curves with names containing search text");
	RNA_def_property_ui_icon(prop, ICON_VIEWZOOM, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "filter_fcurve_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "searchstr");
	RNA_def_property_ui_text(prop, "F-Curve Name Filter", "F-Curve live filtering string");
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	/* NLA Specific Settings */
	prop = RNA_def_property(srna, "show_missing_nla", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NLA_NOACT);
	RNA_def_property_ui_text(prop, "Include Missing NLA",
	                         "Include Animation Data blocks with no NLA data (NLA Editor only)");
	RNA_def_property_ui_icon(prop, ICON_ACTION, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	/* Summary Settings (DopeSheet editors only) */
	prop = RNA_def_property(srna, "show_summary", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filterflag", ADS_FILTER_SUMMARY);
	RNA_def_property_ui_text(prop, "Display Summary", "Display an additional 'summary' line (DopeSheet Editors only)");
	RNA_def_property_ui_icon(prop, ICON_BORDERMOVE, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_expanded_summary", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ADS_FLAG_SUMMARY_COLLAPSED);
	RNA_def_property_ui_text(prop, "Collapse Summary",
	                         "Collapse summary when shown, so all other channels get hidden (DopeSheet Editors Only)");
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	
	/* General DataType Filtering Settings */
	prop = RNA_def_property(srna, "show_transforms", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOOBJ);
	RNA_def_property_ui_text(prop, "Display Transforms",
	                         "Include visualization of Object-level Animation data (mostly Transforms)");
	RNA_def_property_ui_icon(prop, ICON_MANIPUL, 0); /* XXX? */
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_shapekeys", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOSHAPEKEYS);
	RNA_def_property_ui_text(prop, "Display Shapekeys", "Include visualization of ShapeKey related Animation data");
	RNA_def_property_ui_icon(prop, ICON_SHAPEKEY_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_meshes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOMESH);
	RNA_def_property_ui_text(prop, "Display Meshes", "Include visualization of Mesh related Animation data");
	RNA_def_property_ui_icon(prop, ICON_MESH_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_lattices", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOLAT);
	RNA_def_property_ui_text(prop, "Display Lattices", "Include visualization of Lattice related Animation data");
	RNA_def_property_ui_icon(prop, ICON_LATTICE_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_cameras", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOCAM);
	RNA_def_property_ui_text(prop, "Display Camera", "Include visualization of Camera related Animation data");
	RNA_def_property_ui_icon(prop, ICON_CAMERA_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_materials", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOMAT);
	RNA_def_property_ui_text(prop, "Display Material", "Include visualization of Material related Animation data");
	RNA_def_property_ui_icon(prop, ICON_MATERIAL_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_lamps", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOLAM);
	RNA_def_property_ui_text(prop, "Display Lamp", "Include visualization of Lamp related Animation data");
	RNA_def_property_ui_icon(prop, ICON_LAMP_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_textures", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOTEX);
	RNA_def_property_ui_text(prop, "Display Texture", "Include visualization of Texture related Animation data");
	RNA_def_property_ui_icon(prop, ICON_TEXTURE_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_curves", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOCUR);
	RNA_def_property_ui_text(prop, "Display Curve", "Include visualization of Curve related Animation data");
	RNA_def_property_ui_icon(prop, ICON_CURVE_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_worlds", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOWOR);
	RNA_def_property_ui_text(prop, "Display World", "Include visualization of World related Animation data");
	RNA_def_property_ui_icon(prop, ICON_WORLD_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_scenes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOSCE);
	RNA_def_property_ui_text(prop, "Display Scene", "Include visualization of Scene related Animation data");
	RNA_def_property_ui_icon(prop, ICON_SCENE_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_particles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOPART);
	RNA_def_property_ui_text(prop, "Display Particle", "Include visualization of Particle related Animation data");
	RNA_def_property_ui_icon(prop, ICON_PARTICLE_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_metaballs", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOMBA);
	RNA_def_property_ui_text(prop, "Display Metaball", "Include visualization of Metaball related Animation data");
	RNA_def_property_ui_icon(prop, ICON_META_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_armatures", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOARM);
	RNA_def_property_ui_text(prop, "Display Armature", "Include visualization of Armature related Animation data");
	RNA_def_property_ui_icon(prop, ICON_ARMATURE_DATA, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NONTREE);
	RNA_def_property_ui_text(prop, "Display Node", "Include visualization of Node related Animation data");
	RNA_def_property_ui_icon(prop, ICON_NODETREE, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);

	prop = RNA_def_property(srna, "show_speakers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "filterflag", ADS_FILTER_NOSPK);
	RNA_def_property_ui_text(prop, "Display Speaker", "Include visualization of Speaker related Animation data");
	RNA_def_property_ui_icon(prop, ICON_SPEAKER, 0);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
}

static void rna_def_action_group(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "ActionGroup", NULL);
	RNA_def_struct_sdna(srna, "bActionGroup");
	RNA_def_struct_ui_text(srna, "Action Group", "Groups of F-Curves");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	/* WARNING: be very careful when working with this list, since the endpoint is not
	 * defined like a standard ListBase. Adding/removing channels from this list needs
	 * extreme care, otherwise the F-Curve list running through adjacent groups does
	 * not match up with the one stored in the Action, resulting in curves which do not
	 * show up in animation editors. In extreme cases, animation may also selectively
	 * fail to play back correctly.
	 *
	 * If such changes are required, these MUST go through the API functions for manipulating
	 * these F-Curve groupings. Also, note that groups only apply in actions ONLY.
	 */
	prop = RNA_def_property(srna, "channels", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "channels", NULL);
	RNA_def_property_struct_type(prop, "FCurve");
	RNA_def_property_collection_funcs(prop, 0, "rna_ActionGroup_channels_next", NULL, NULL, NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Channels", "F-Curves in this group");
	
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_SELECTED);
	RNA_def_property_ui_text(prop, "Select", "Action Group is selected");
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_SELECTED, NULL);
	
	prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_PROTECTED);
	RNA_def_property_ui_text(prop, "Lock", "Action Group is locked");
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_EXPANDED);
	RNA_def_property_ui_text(prop, "Expanded", "Action Group is expanded");
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "custom_color", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "customCol");
	RNA_def_property_ui_text(prop, "Custom Color", "Index of custom color set");
	RNA_def_property_update(prop, NC_ANIMATION|ND_ANIMCHAN|NA_EDITED, NULL);
}

/* fcurve.keyframe_points */
static void rna_def_action_groups(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "ActionGroups");
	srna = RNA_def_struct(brna, "ActionGroups", NULL);
	RNA_def_struct_sdna(srna, "bAction");
	RNA_def_struct_ui_text(srna, "Action Groups", "Collection of action groups");

	func = RNA_def_function(srna, "new", "rna_Action_groups_new");
	RNA_def_function_ui_description(func, "Add a keyframe to the curve");
	parm = RNA_def_string(func, "name", "Group", 0, "", "New name for the action group");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	parm = RNA_def_pointer(func, "action_group", "ActionGroup", "", "Newly created action group");
	RNA_def_function_return(func, parm);


	func = RNA_def_function(srna, "remove", "rna_Action_groups_remove");
	RNA_def_function_ui_description(func, "Remove action group");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "action_group", "ActionGroup", "", "Action group to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);
}

static void rna_def_action_fcurves(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "ActionFCurves");
	srna = RNA_def_struct(brna, "ActionFCurves", NULL);
	RNA_def_struct_sdna(srna, "bAction");
	RNA_def_struct_ui_text(srna, "Action F-Curves", "Collection of action F-Curves");

	func = RNA_def_function(srna, "new", "rna_Action_fcurve_new");
	RNA_def_function_ui_description(func, "Add a keyframe to the F-Curve");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_string(func, "data_path", "", 0, "Data Path", "F-Curve data path to use");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
	RNA_def_string(func, "action_group", "", 0, "Action Group", "Acton group to add this F-Curve into");

	parm = RNA_def_pointer(func, "fcurve", "FCurve", "", "Newly created F-Curve");
	RNA_def_function_return(func, parm);


	func = RNA_def_function(srna, "remove", "rna_Action_fcurve_remove");
	RNA_def_function_ui_description(func, "Remove action group");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "fcurve", "FCurve", "", "F-Curve to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);
}

static void rna_def_action_pose_markers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "ActionPoseMarkers");
	srna = RNA_def_struct(brna, "ActionPoseMarkers", NULL);
	RNA_def_struct_sdna(srna, "bAction");
	RNA_def_struct_ui_text(srna, "Action Pose Markers", "Collection of timeline markers");

	func = RNA_def_function(srna, "new", "rna_Action_pose_markers_new");
	RNA_def_function_ui_description(func, "Add a pose marker to the action");
	parm = RNA_def_string(func, "name", "Marker", 0, "", "New name for the marker (not unique)");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	parm = RNA_def_pointer(func, "marker", "TimelineMarker", "", "Newly created marker");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_Action_pose_markers_remove");
	RNA_def_function_ui_description(func, "Remove a timeline marker");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "marker", "TimelineMarker", "", "Timeline marker to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);
	
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "TimelineMarker");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_Action_active_pose_marker_get",
	                               "rna_Action_active_pose_marker_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Pose Marker", "Active pose marker for this Action");
	
	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "active_marker");
	RNA_def_property_int_funcs(prop, "rna_Action_active_pose_marker_index_get",
	                           "rna_Action_active_pose_marker_index_set", "rna_Action_active_pose_marker_index_range");
	RNA_def_property_ui_text(prop, "Active Pose Marker Index", "Index of active pose marker");
}

static void rna_def_action(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "Action", "ID");
	RNA_def_struct_sdna(srna, "bAction");
	RNA_def_struct_ui_text(srna, "Action", "A collection of F-Curves for animation");
	RNA_def_struct_ui_icon(srna, ICON_ACTION);
	
	/* collections */
	prop = RNA_def_property(srna, "fcurves", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "curves", NULL);
	RNA_def_property_struct_type(prop, "FCurve");
	RNA_def_property_ui_text(prop, "F-Curves", "The individual F-Curves that make up the Action");
	rna_def_action_fcurves(brna, prop);
	
	prop = RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "groups", NULL);
	RNA_def_property_struct_type(prop, "ActionGroup");
	RNA_def_property_ui_text(prop, "Groups", "Convenient groupings of F-Curves");
	rna_def_action_groups(brna, prop);
	
	prop = RNA_def_property(srna, "pose_markers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "markers", NULL);
	RNA_def_property_struct_type(prop, "TimelineMarker");
	RNA_def_property_ui_text(prop, "Pose Markers", "Markers specific to this Action, for labeling poses");
	rna_def_action_pose_markers(brna, prop);
	
	/* properties */
	prop = RNA_def_float_vector(srna, "frame_range", 2, NULL, 0, 0, "Frame Range",
	                            "The final frame range of all F-Curves within this action", 0, 0);
	RNA_def_property_float_funcs(prop, "rna_Action_frame_range_get", NULL, NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	/* special "type" limiter - should not really be edited in general,
	 * but is still available/editable in 'emergencies' */
	prop = RNA_def_property(srna, "id_root", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "idroot");
	RNA_def_property_enum_items(prop, id_type_items);
	RNA_def_property_ui_text(prop, "ID Root Type",
	                         "Type of ID-block that action can be used on - "
	                         "DO NOT CHANGE UNLESS YOU KNOW WHAT YOU'RE DOING");
	
	/* API calls */
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
