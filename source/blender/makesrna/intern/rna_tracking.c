/*
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
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_tracking.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_depsgraph.h"

static void rna_tracking_markers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	MovieClip *clip= (MovieClip*)ptr->id.data;
	rna_iterator_listbase_begin(iter, &clip->tracking.markers, NULL);
}

static PointerRNA rna_tracking_act_marker_get(PointerRNA *ptr)
{
	MovieClip *clip= (MovieClip*)ptr->id.data;
	int type;
	void *sel;

	BKE_movieclip_last_selection(clip, &type, &sel);
	if(type!=MCLIP_SEL_MARKER)
		return rna_pointer_inherit_refine(ptr, &RNA_MovieTrackingMarker, NULL);

	return rna_pointer_inherit_refine(ptr, &RNA_MovieTrackingMarker, sel);
}

static void rna_tracking_markerPattern_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	MovieClip *clip= (MovieClip*)ptr->id.data;
	MovieTrackingMarker *marker;

	/* XXX: clamp modified marker only */
	marker= clip->tracking.markers.first;
	while(marker) {
		BKE_tracking_clamp_marker(marker, CLAMP_PAT_DIM);
		marker= marker->next;
	}
}

static void rna_tracking_markerSearch_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	MovieClip *clip= (MovieClip*)ptr->id.data;
	MovieTrackingMarker *marker;

	/* XXX: clamp modified marker only */
	marker= clip->tracking.markers.first;
	while(marker) {
		BKE_tracking_clamp_marker(marker, CLAMP_SEARCH_DIM);
		marker= marker->next;
	}
}

#else

static void rna_def_trackingCamera(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MovieTrackingCamera", NULL);
	RNA_def_struct_ui_text(srna, "Movie tracking camera data", "Match-moving camera data for tracking");

	prop= RNA_def_property(srna, "focal_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "focal");
	RNA_def_property_range(prop, 1.0f, 5000.0f);
	RNA_def_property_ui_text(prop, "Focal Length", "Camera's focal length in millimeters");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, NULL);
}

static void rna_def_trackingMarker(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MovieTrackingMarker", NULL);
	RNA_def_struct_ui_text(srna, "Movie tracking marker data", "Match-moving marker data for tracking");

	/* Position */
	prop= RNA_def_property(srna, "pos", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 5);
	RNA_def_property_float_sdna(prop, NULL, "pos");
	RNA_def_property_ui_text(prop, "Position", "Marker position at frame in unified coordinates");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, NULL);

	/* Pattern */
	prop= RNA_def_property(srna, "pattern_min", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 5);
	RNA_def_property_float_sdna(prop, NULL, "pat_min");
	RNA_def_property_ui_text(prop, "Pattern Min", "Left-bottom corner of pattern area in unified coordinates relative to marker position");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_markerPattern_update");

	prop= RNA_def_property(srna, "pattern_max", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 5);
	RNA_def_property_float_sdna(prop, NULL, "pat_max");
	RNA_def_property_ui_text(prop, "Pattern Max", "Right-bottom corner of pattern area in unified coordinates relative to marker position");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_markerPattern_update");

	/* Search */
	prop= RNA_def_property(srna, "search_min", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 5);
	RNA_def_property_float_sdna(prop, NULL, "search_min");
	RNA_def_property_ui_text(prop, "Search Min", "Left-bottom corner of search area in unified coordinates relative to marker position");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_markerSearch_update");

	prop= RNA_def_property(srna, "search_max", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 5);
	RNA_def_property_float_sdna(prop, NULL, "search_max");
	RNA_def_property_ui_text(prop, "Search Max", "Right-bottom corner of search area in unified coordinates relative to marker position");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_markerSearch_update");
}

static void rna_def_tracking(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	rna_def_trackingCamera(brna);
	rna_def_trackingMarker(brna);

	srna= RNA_def_struct(brna, "MovieTracking", NULL);
	RNA_def_struct_ui_text(srna, "Movie tracking data", "Match-moving data for tracking");

	prop= RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingCamera");

	prop= RNA_def_property(srna, "markers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_tracking_markers_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", 0, 0, 0);
	RNA_def_property_struct_type(prop, "MovieTrackingMarker");
	RNA_def_property_ui_text(prop, "Markers", "Collection of markers in this tracking data object");

	prop= RNA_def_property(srna, "active_marker", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingMarker");
	RNA_def_property_pointer_funcs(prop, "rna_tracking_act_marker_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Active marker", "Active marker markers in this tracking data object");
}

void RNA_def_tracking(BlenderRNA *brna)
{
	rna_def_tracking(brna);
}

#endif
