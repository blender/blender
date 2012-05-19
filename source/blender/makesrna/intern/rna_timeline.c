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
 * Contributor(s): Blender Foundation (2008), Roland Hess
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_timeline.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_scene_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "WM_api.h"

static void rna_TimelineMarker_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
	WM_main_add_notifier(NC_SCENE | ND_MARKERS, NULL);
	WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, NULL);
}

#else

static void rna_def_timeline_marker(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "TimelineMarker", NULL);
	RNA_def_struct_sdna(srna, "TimeMarker");
	RNA_def_struct_ui_text(srna, "Marker", "Marker for noting points in the timeline");

	/* String values */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, 0, "rna_TimelineMarker_update");

	prop = RNA_def_property(srna, "frame", PROP_INT, PROP_TIME);
	RNA_def_property_ui_text(prop, "Frame", "The frame on which the timeline marker appears");
	RNA_def_property_update(prop, 0, "rna_TimelineMarker_update");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 1 /*SELECT*/);
	RNA_def_property_ui_text(prop, "Select", "Marker selection state");
	RNA_def_property_update(prop, 0, "rna_TimelineMarker_update");

#ifdef DURIAN_CAMERA_SWITCH
	prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_ui_text(prop, "Camera", "Camera this timeline sets to active");
#endif
}

void RNA_def_timeline_marker(BlenderRNA *brna)
{
	rna_def_timeline_marker(brna);
}


#endif
