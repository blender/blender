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

#include "BLI_math.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"	/* SELECT */
#include "DNA_scene_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_depsgraph.h"
#include "BKE_node.h"

#include "IMB_imbuf.h"

#include "WM_api.h"

static char *rna_tracking_path(PointerRNA *UNUSED(ptr))
{
	return BLI_sprintfN("tracking");
}

static void rna_tracking_defaultSettings_levelsUpdate(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingSettings *settings = &tracking->settings;

	if (settings->default_tracker == TRACKER_KLT) {
		int max_pyramid_level_factor = 1 << (settings->default_pyramid_levels - 1);
		float search_ratio = 2.3f * max_pyramid_level_factor;

		settings->default_search_size = settings->default_pattern_size*search_ratio;
	}
}

static void rna_tracking_defaultSettings_patternUpdate(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingSettings *settings = &tracking->settings;

	if (settings->default_search_size<settings->default_pattern_size)
		settings->default_search_size = settings->default_pattern_size;
}

static void rna_tracking_defaultSettings_searchUpdate(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingSettings *settings = &tracking->settings;

	if (settings->default_pattern_size>settings->default_search_size)
		settings->default_pattern_size = settings->default_search_size;
}

static char *rna_trackingTrack_path(PointerRNA *ptr)
{
	MovieTrackingTrack *track = (MovieTrackingTrack *) ptr->data;

	return BLI_sprintfN("tracking.tracks[\"%s\"]", track->name);
}

static void rna_trackingTracks_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;

	rna_iterator_listbase_begin(iter, &clip->tracking.tracks, NULL);
}

static void rna_trackingObjects_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;

	rna_iterator_listbase_begin(iter, &clip->tracking.objects, NULL);
}

static int rna_tracking_active_object_index_get(PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;

	return clip->tracking.objectnr;
}

static void rna_tracking_active_object_index_set(PointerRNA *ptr, int value)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;

	clip->tracking.objectnr = value;
}

static void rna_tracking_active_object_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;

	*min = 0;
	*max = clip->tracking.tot_object-1;
	*max = MAX2(0, *max);
}

static PointerRNA rna_tracking_active_track_get(PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	MovieTrackingTrack *act_track = BKE_tracking_active_track(&clip->tracking);

	return rna_pointer_inherit_refine(ptr, &RNA_MovieTrackingTrack, act_track);
}

static void rna_tracking_active_track_set(PointerRNA *ptr, PointerRNA value)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	MovieTrackingTrack *track = (MovieTrackingTrack *)value.data;
	ListBase *tracksbase = BKE_tracking_get_tracks(&clip->tracking);
	int index = BLI_findindex(tracksbase, track);

	if (index >= 0)
		clip->tracking.act_track = track;
	else
		clip->tracking.act_track = NULL;
}

void rna_trackingTrack_name_set(PointerRNA *ptr, const char *value)
{
	MovieClip *clip = (MovieClip *)ptr->id.data;
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track = (MovieTrackingTrack *)ptr->data;
	ListBase *tracksbase = &tracking->tracks;

	BLI_strncpy(track->name, value, sizeof(track->name));

	/* TODO: it's a bit difficult to find list track came from knowing just
	 *       movie clip ID and MovieTracking structure, so keep this naive
	 *       search for a while */
	if (BLI_findindex(tracksbase, track) < 0) {
		MovieTrackingObject *object = tracking->objects.first;

		while (object) {
			if (BLI_findindex(&object->tracks, track)) {
				tracksbase = &object->tracks;
				break;
			}

			object = object->next;
		}
	}

	BKE_track_unique_name(tracksbase, track);
}

static int rna_trackingTrack_select_get(PointerRNA *ptr)
{
	MovieTrackingTrack *track = (MovieTrackingTrack *)ptr->data;

	return TRACK_SELECTED(track);
}

static void rna_trackingTrack_select_set(PointerRNA *ptr, int value)
{
	MovieTrackingTrack *track = (MovieTrackingTrack *)ptr->data;

	if (value) {
		track->flag |= SELECT;
		track->pat_flag |= SELECT;
		track->search_flag |= SELECT;
	}
	else {
		track->flag &= ~SELECT;
		track->pat_flag &= ~SELECT;
		track->search_flag &= ~SELECT;
	}
}

static void rna_tracking_trackerPattern_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	MovieTrackingTrack *track = (MovieTrackingTrack *)ptr->data;

	BKE_tracking_clamp_track(track, CLAMP_PAT_DIM);
}

static void rna_tracking_trackerSearch_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	MovieTrackingTrack *track = (MovieTrackingTrack *)ptr->data;

	BKE_tracking_clamp_track(track, CLAMP_SEARCH_DIM);
}

static void rna_tracking_trackerAlgorithm_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	MovieTrackingTrack *track = (MovieTrackingTrack *)ptr->data;

	if (track->tracker == TRACKER_KLT)
		BKE_tracking_clamp_track(track, CLAMP_PYRAMID_LEVELS);
	else
		BKE_tracking_clamp_track(track, CLAMP_SEARCH_DIM);
}

static void rna_tracking_trackerPyramid_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	MovieTrackingTrack *track = (MovieTrackingTrack *)ptr->data;

	BKE_tracking_clamp_track(track, CLAMP_PYRAMID_LEVELS);
}

static char *rna_trackingCamera_path(PointerRNA *UNUSED(ptr))
{
	return BLI_sprintfN("tracking.camera");
}

static float rna_trackingCamera_focal_mm_get(PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	MovieTrackingCamera *camera = &clip->tracking.camera;
	float val = camera->focal;

	if (clip->lastsize[0])
		val = val*camera->sensor_width/(float)clip->lastsize[0];

	return val;
}

static void rna_trackingCamera_focal_mm_set(PointerRNA *ptr, float value)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	MovieTrackingCamera *camera = &clip->tracking.camera;

	if (clip->lastsize[0])
		value = clip->lastsize[0]*value/camera->sensor_width;

	if (value >= 0.0001)
		camera->focal = value;
}

static char *rna_trackingStabilization_path(PointerRNA *UNUSED(ptr))
{
	return BLI_sprintfN("tracking.stabilization");
}

static int rna_track_2d_stabilization(CollectionPropertyIterator *UNUSED(iter), void *data)
{
	MovieTrackingTrack *track = (MovieTrackingTrack*)data;

	if ((track->flag&TRACK_USE_2D_STAB) == 0)
		return 1;

	return 0;
}

static void rna_tracking_stabTracks_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	rna_iterator_listbase_begin(iter, &clip->tracking.tracks, rna_track_2d_stabilization);
}

static int rna_tracking_stabTracks_active_index_get(PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	return clip->tracking.stabilization.act_track;
}

static void rna_tracking_stabTracks_active_index_set(PointerRNA *ptr, int value)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	clip->tracking.stabilization.act_track = value;
}

static void rna_tracking_stabTracks_active_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;

	*min = 0;
	*max = clip->tracking.stabilization.tot_track-1;
	*max = MAX2(0, *max);
}

static void rna_tracking_flushUpdate(Main *UNUSED(bmain), Scene *scene, PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	MovieTrackingStabilization *stab = &clip->tracking.stabilization;

	stab->ok = 0;

	nodeUpdateID(scene->nodetree, &clip->id);

	WM_main_add_notifier(NC_SCENE|ND_NODES, NULL);
	DAG_id_tag_update(&clip->id, 0);
}

static void rna_trackingObject_tracks_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	MovieTrackingObject *object = (MovieTrackingObject* )ptr->data;

	if (object->flag&TRACKING_OBJECT_CAMERA) {
		MovieClip *clip = (MovieClip*)ptr->id.data;

		rna_iterator_listbase_begin(iter, &clip->tracking.tracks, NULL);
	}
	else {
		rna_iterator_listbase_begin(iter, &object->tracks, NULL);
	}
}

static PointerRNA rna_tracking_active_object_get(PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	MovieTrackingObject *object = BLI_findlink(&clip->tracking.objects, clip->tracking.objectnr);

	return rna_pointer_inherit_refine(ptr, &RNA_MovieTrackingObject, object);
}

static void rna_tracking_active_object_set(PointerRNA *ptr, PointerRNA value)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;
	MovieTrackingObject *object = (MovieTrackingObject *)value.data;
	int index = BLI_findindex(&clip->tracking.objects, object);

	if (index >= 0) clip->tracking.objectnr = index;
	else clip->tracking.objectnr = 0;
}

void rna_trackingObject_name_set(PointerRNA *ptr, const char *value)
{
	MovieClip *clip = (MovieClip *)ptr->id.data;
	MovieTrackingObject *object = (MovieTrackingObject *)ptr->data;

	BLI_strncpy(object->name, value, sizeof(object->name));

	BKE_tracking_object_unique_name(&clip->tracking, object);
}

static void rna_trackingObject_flushUpdate(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	MovieClip *clip = (MovieClip*)ptr->id.data;

	WM_main_add_notifier(NC_OBJECT|ND_TRANSFORM, NULL);
	DAG_id_tag_update(&clip->id, 0);
}

static void rna_trackingMarker_frame_set(PointerRNA *ptr, int value)
{
	MovieClip *clip = (MovieClip *) ptr->id.data;
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingTrack *track;
	MovieTrackingMarker *marker = (MovieTrackingMarker *) ptr->data;

	track = tracking->tracks.first;
	while (track) {
		if (marker >= track->markers && marker < track->markers+track->markersnr) {
			break;
		}

		track = track->next;
	}

	if (track) {
		MovieTrackingMarker new_marker = *marker;
		new_marker.framenr = value;

		BKE_tracking_delete_marker(track, marker->framenr);
		BKE_tracking_insert_marker(track, &new_marker);
	}
}

/* API */

static void add_tracks_to_base(MovieClip *clip, MovieTracking *tracking, ListBase *tracksbase, int frame, int number)
{
	int a, width, height;
	MovieClipUser user = {0};

	user.framenr = 1;

	BKE_movieclip_get_size(clip, &user, &width, &height);

	for (a = 0; a<number; a++)
		BKE_tracking_add_track(tracking, tracksbase, 0, 0, frame, width, height);
}

static void rna_trackingTracks_add(ID *id, MovieTracking *tracking, int frame, int number)
{
	MovieClip *clip = (MovieClip *) id;

	add_tracks_to_base(clip, tracking, &tracking->tracks, frame, number);

	WM_main_add_notifier(NC_MOVIECLIP|NA_EDITED, clip);
}

static void rna_trackingObject_tracks_add(ID *id, MovieTrackingObject *object, int frame, int number)
{
	MovieClip *clip = (MovieClip *) id;
	ListBase *tracksbase = &object->tracks;

	if (object->flag&TRACKING_OBJECT_CAMERA)
		tracksbase = &clip->tracking.tracks;

	add_tracks_to_base(clip, &clip->tracking, tracksbase, frame, number);

	WM_main_add_notifier(NC_MOVIECLIP|NA_EDITED, NULL);
}

static MovieTrackingObject *rna_trackingObject_new(MovieTracking *tracking, const char *name)
{
	MovieTrackingObject *object = BKE_tracking_new_object(tracking, name);

	WM_main_add_notifier(NC_MOVIECLIP|NA_EDITED, NULL);

	return object;
}

void rna_trackingObject_remove(MovieTracking *tracking, MovieTrackingObject *object)
{
	BKE_tracking_remove_object(tracking, object);

	WM_main_add_notifier(NC_MOVIECLIP|NA_EDITED, NULL);
}

static MovieTrackingMarker *rna_trackingMarkers_find_frame(MovieTrackingTrack *track, int framenr)
{
	return BKE_tracking_exact_marker(track, framenr);
}

static MovieTrackingMarker* rna_trackingMarkers_insert_frame(MovieTrackingTrack *track, int framenr, float *co)
{
	MovieTrackingMarker marker, *new_marker;

	memset(&marker, 0, sizeof(marker));
	marker.framenr = framenr;
	copy_v2_v2(marker.pos, co);

	new_marker = BKE_tracking_insert_marker(track, &marker);

	WM_main_add_notifier(NC_MOVIECLIP|NA_EDITED, NULL);

	return new_marker;
}

void rna_trackingMarkers_delete_frame(MovieTrackingTrack *track, int framenr)
{
	if (track->markersnr == 1)
		return;

	BKE_tracking_delete_marker(track, framenr);

	WM_main_add_notifier(NC_MOVIECLIP|NA_EDITED, NULL);
}

#else

static EnumPropertyItem tracker_items[] = {
	{TRACKER_KLT, "KLT", 0, "KLT",
	              "Kanade–Lucas–Tomasi tracker which works with most of video clips, a bit slower than SAD"},
	{TRACKER_SAD, "SAD", 0, "SAD", "Sum of Absolute Differences tracker which can be used when KLT tracker fails"},
	{TRACKER_HYBRID, "Hybrid", 0, "Hybrid", "A hybrid tracker that uses SAD for rough tracking, KLT for refinement."},
	{0, NULL, 0, NULL, NULL}};

static EnumPropertyItem pattern_match_items[] = {
	{TRACK_MATCH_KEYFRAME, "KEYFRAME", 0, "Keyframe", "Track pattern from keyframe to next frame"},
	{TRACK_MATCH_PREVFRAME, "PREV_FRAME", 0, "Previous frame", "Track pattern from current frame to next frame"},
	{0, NULL, 0, NULL, NULL}};

static int rna_matrix_dimsize_4x4[] = {4, 4};

static void rna_def_trackingSettings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem speed_items[] = {
		{0, "FASTEST", 0, "Fastest", "Track as fast as it's possible"},
	    {TRACKING_SPEED_DOUBLE, "DOUBLE", 0, "Double", "Track with double speed"},
		{TRACKING_SPEED_REALTIME, "REALTIME", 0, "Realtime", "Track with realtime speed"},
		{TRACKING_SPEED_HALF, "HALF", 0, "Half", "Track with half of realtime speed"},
		{TRACKING_SPEED_QUARTER, "QUARTER", 0, "Quarter", "Track with quarter of realtime speed"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem cleanup_items[] = {
		{TRACKING_CLEAN_SELECT, "SELECT", 0, "Select", "Select unclean tracks"},
		{TRACKING_CLEAN_DELETE_TRACK, "DELETE_TRACK", 0, "Delete Track", "Delete unclean tracks"},
		{TRACKING_CLEAN_DELETE_SEGMENT, "DELETE_SEGMENTS", 0, "Delete Segments", "Delete unclean segments of tracks"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem refine_items[] = {
		{0, "NONE", 0, "Nothing", "Do not refine camera intrinsics"},
		{REFINE_FOCAL_LENGTH, "FOCAL_LENGTH", 0, "Focal Length", "Refine focal length"},
		{REFINE_FOCAL_LENGTH|REFINE_RADIAL_DISTORTION_K1, "FOCAL_LENGTH_RADIAL_K1", 0, "Focal length, K1",
		                                                  "Refine focal length and radial distortion K1"},
		{REFINE_FOCAL_LENGTH|
		 REFINE_RADIAL_DISTORTION_K1|
		 REFINE_RADIAL_DISTORTION_K2, "FOCAL_LENGTH_RADIAL_K1_K2", 0, "Focal length, K1, K2",
		                              "Refine focal length and radial distortion K1 and K2"},
		{REFINE_FOCAL_LENGTH|
		 REFINE_PRINCIPAL_POINT|
		 REFINE_RADIAL_DISTORTION_K1|
		 REFINE_RADIAL_DISTORTION_K2, "FOCAL_LENGTH_PRINCIPAL_POINT_RADIAL_K1_K2", 0,
		                              "Focal Length, Optical Center, K1, K2",
		                              "Refine focal length, optical center and radial distortion K1 and K2"},
		{REFINE_FOCAL_LENGTH|
		 REFINE_PRINCIPAL_POINT, "FOCAL_LENGTH_PRINCIPAL_POINT", 0, "Focal Length, Optical Center",
		                         "Refine focal length and optical center"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "MovieTrackingSettings", NULL);
	RNA_def_struct_ui_text(srna, "Movie tracking settings", "Match moving settings");

	/* speed */
	prop = RNA_def_property(srna, "speed", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, speed_items);
	RNA_def_property_ui_text(prop, "Speed",
	                         "Limit speed of tracking to make visual feedback easier "
	                         "(this does not affect the tracking quality)");

	/* keyframe_a */
	prop = RNA_def_property(srna, "keyframe_a", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "keyframe1");
	RNA_def_property_ui_text(prop, "Keyframe A", "First keyframe used for reconstruction initialization");

	/* keyframe_b */
	prop = RNA_def_property(srna, "keyframe_b", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "keyframe2");
	RNA_def_property_ui_text(prop, "Keyframe B", "Second keyframe used for reconstruction initialization");

	/* intrinsics refinement during bundle adjustment */
	prop = RNA_def_property(srna, "refine_intrinsics", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "refine_camera_intrinsics");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, refine_items);
	RNA_def_property_ui_text(prop, "Refine", "Refine intrinsics during camera solving");

	/* tool settings */

	/* distance */
	prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_float_sdna(prop, NULL, "dist");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Distance", "Distance between two bundles used for scene scaling");

	/* frames count */
	prop = RNA_def_property(srna, "clean_frames", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "clean_frames");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_text(prop, "Tracked Frames",
	                         "Effect on tracks which are tracked less than the specified amount of frames");

	/* re-projection error */
	prop = RNA_def_property(srna, "clean_error", PROP_FLOAT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_float_sdna(prop, NULL, "clean_error");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Reprojection Error", "Effect on tracks which have a larger re-projection error");

	/* cleanup action */
	prop = RNA_def_property(srna, "clean_action", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "clean_action");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, cleanup_items);
	RNA_def_property_ui_text(prop, "Action", "Cleanup action to execute");

	/* ** default tracker settings ** */
	prop = RNA_def_property(srna, "show_default_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TRACKING_SETTINGS_SHOW_DEFAULT_EXPANDED);
	RNA_def_property_ui_text(prop, "Show Expanded", "Show the expanded in the user interface");
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);

	/* solver settings */
	prop = RNA_def_property(srna, "use_tripod_solver", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "motion_flag", TRACKING_MOTION_TRIPOD);
	RNA_def_property_ui_text(prop, "Tripod Motion", "Tracking footage is shooted by tripod camera and should use special sovler for this");

	/* limit frames */
	prop = RNA_def_property(srna, "default_frames_limit", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "default_frames_limit");
	RNA_def_property_range(prop, 0, SHRT_MAX);
	RNA_def_property_ui_text(prop, "Frames Limit", "Every tracking cycle, this number of frames are tracked");

	/* pattern match */
	prop = RNA_def_property(srna, "default_pattern_match", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "default_pattern_match");
	RNA_def_property_enum_items(prop, pattern_match_items);
	RNA_def_property_ui_text(prop, "Pattern Match",
	                         "Track pattern from given frame when tracking marker to next frame");

	/* margin */
	prop = RNA_def_property(srna, "default_margin", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "default_margin");
	RNA_def_property_range(prop, 0, 300);
	RNA_def_property_ui_text(prop, "Margin", "Default distance from image boudary at which marker stops tracking");

	/* tracking algorithm */
	prop = RNA_def_property(srna, "default_tracker", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, tracker_items);
	RNA_def_property_update(prop, 0, "rna_tracking_defaultSettings_levelsUpdate");
	RNA_def_property_ui_text(prop, "Tracker", "Default tracking algorithm to use");

	/* pyramid level for pyramid klt tracking */
	prop = RNA_def_property(srna, "default_pyramid_levels", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "default_pyramid_levels");
	RNA_def_property_range(prop, 1, 16);
	RNA_def_property_update(prop, 0, "rna_tracking_defaultSettings_levelsUpdate");
	RNA_def_property_ui_text(prop, "Pyramid levels", "Default number of pyramid levels (increase on blurry footage)");

	/* minmal correlation - only used for SAD tracker */
	prop = RNA_def_property(srna, "default_correlation_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_float_sdna(prop, NULL, "default_minimum_correlation");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_range(prop, -1.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Correlation",
	                         "Default minimal value of correlation between matched pattern and reference "
	                         "which is still treated as successful tracking");

	/* default pattern size */
	prop = RNA_def_property(srna, "default_pattern_size", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "default_pattern_size");
	RNA_def_property_range(prop, 5, 1000);
	RNA_def_property_update(prop, 0, "rna_tracking_defaultSettings_patternUpdate");
	RNA_def_property_ui_text(prop, "Pattern Size", "Size of pattern area for newly created tracks");

	/* default search size */
	prop = RNA_def_property(srna, "default_search_size", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "default_search_size");
	RNA_def_property_range(prop, 5, 1000);
	RNA_def_property_update(prop, 0, "rna_tracking_defaultSettings_searchUpdate");
	RNA_def_property_ui_text(prop, "Search Size", "Size of search area for newly created tracks");

	/* use_red_channel */
	prop = RNA_def_property(srna, "use_default_red_channel", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "default_flag", TRACK_DISABLE_RED);
	RNA_def_property_ui_text(prop, "Use Red Channel", "Use red channel from footage for tracking");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* use_green_channel */
	prop = RNA_def_property(srna, "use_default_green_channel", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "default_flag", TRACK_DISABLE_GREEN);
	RNA_def_property_ui_text(prop, "Use Green Channel", "Use green channel from footage for tracking");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* use_blue_channel */
	prop = RNA_def_property(srna, "use_default_blue_channel", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "default_flag", TRACK_DISABLE_BLUE);
	RNA_def_property_ui_text(prop, "Use Blue Channel", "Use blue channel from footage for tracking");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* ** object tracking ** */

	/* object distance */
	prop = RNA_def_property(srna, "object_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_float_sdna(prop, NULL, "object_distance");
	RNA_def_property_ui_text(prop, "Distance", "Distance between two bundles used for object scaling");
	RNA_def_property_range(prop, 0.001, 10000);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_range(prop, 0.001, 10000.0, 1, 3);
}

static void rna_def_trackingCamera(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem camera_units_items[] = {
		{CAMERA_UNITS_PX, "PIXELS", 0, "px", "Use pixels for units of focal length"},
		{CAMERA_UNITS_MM, "MILLIMETERS", 0, "mm", "Use millimeters for units of focal length"},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "MovieTrackingCamera", NULL);
	RNA_def_struct_path_func(srna, "rna_trackingCamera_path");
	RNA_def_struct_ui_text(srna, "Movie tracking camera data", "Match-moving camera data for tracking");

	/* Sensor */
	prop = RNA_def_property(srna, "sensor_width", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sensor_width");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, 500.0f);
	RNA_def_property_ui_text(prop, "Sensor", "Width of CCD sensor in millimeters");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, NULL);

	/* Focal Length */
	prop = RNA_def_property(srna, "focal_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "focal");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0001f, 5000.0f);
	RNA_def_property_float_funcs(prop, "rna_trackingCamera_focal_mm_get", "rna_trackingCamera_focal_mm_set", NULL);
	RNA_def_property_ui_text(prop, "Focal Length", "Camera's focal length");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, NULL);

	/* Focal Length in pixels */
	prop = RNA_def_property(srna, "focal_length_pixels", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "focal");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, 5000.0f);
	RNA_def_property_ui_text(prop, "Focal Length", "Camera's focal length");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, NULL);

	/* Units */
	prop = RNA_def_property(srna, "units", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "units");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, camera_units_items);
	RNA_def_property_ui_text(prop, "Units", "Units used for camera focal length");

	/* Principal Point */
	prop = RNA_def_property(srna, "principal", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_sdna(prop, NULL, "principal");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Principal Point", "Optical center of lens");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, NULL);

	/* Radial distortion parameters */
	prop = RNA_def_property(srna, "k1", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "k1");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_range(prop, -10, 10, .1, 3);
	RNA_def_property_ui_text(prop, "K1", "First coefficient of third order polynomial radial distortion");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_flushUpdate");

	prop = RNA_def_property(srna, "k2", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "k2");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_range(prop, -10, 10, .1, 3);
	RNA_def_property_ui_text(prop, "K2", "Second coefficient of third order polynomial radial distortion");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_flushUpdate");

	prop = RNA_def_property(srna, "k3", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "k3");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_range(prop, -10, 10, .1, 3);
	RNA_def_property_ui_text(prop, "K3", "Third coefficient of third order polynomial radial distortion");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_flushUpdate");

	/* pixel aspect */
	prop = RNA_def_property(srna, "pixel_aspect", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "pixel_aspect");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.1f, 5000.0f);
	RNA_def_property_ui_range(prop, 0.1f, 5000.0f, 1, 2);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Pixel Aspect Ratio", "Pixel aspect ratio");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_tracking_flushUpdate");
}

static void rna_def_trackingMarker(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MovieTrackingMarker", NULL);
	RNA_def_struct_ui_text(srna, "Movie tracking marker data", "Match-moving marker data for tracking");

	/* position */
	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_float_sdna(prop, NULL, "pos");
	RNA_def_property_ui_text(prop, "Position", "Marker position at frame in normalized coordinates");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, NULL);

	/* frame */
	prop = RNA_def_property(srna, "frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "framenr");
	RNA_def_property_ui_text(prop, "Frame", "Frame number marker is keyframed on");
	RNA_def_property_int_funcs(prop, NULL, "rna_trackingMarker_frame_set", NULL);
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, 0);

	/* enable */
	prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MARKER_DISABLED);
	RNA_def_property_ui_text(prop, "Mode", "Is marker muted for current frame");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, NULL);
}

static void rna_def_trackingMarkers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MovieTrackingMarkers");
	srna = RNA_def_struct(brna, "MovieTrackingMarkers", NULL);
	RNA_def_struct_sdna(srna, "MovieTrackingTrack");
	RNA_def_struct_ui_text(srna, "Movie Tracking Markers", "Collection of markers for movie tracking track");

	func = RNA_def_function(srna, "find_frame", "rna_trackingMarkers_find_frame");
	RNA_def_function_ui_description(func, "Get marker for specified frame");
	parm = RNA_def_int(func, "frame", 1, MINFRAME, MAXFRAME, "Frame",
	                   "Frame number to find marker for", MINFRAME, MAXFRAME);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "marker", "MovieTrackingMarker", "", "Marker for specified frame");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "insert_frame", "rna_trackingMarkers_insert_frame");
	RNA_def_function_ui_description(func, "Add a number of tracks to this movie clip");
	parm = RNA_def_int(func, "frame", 1, MINFRAME, MAXFRAME, "Frame",
	                   "Frame number to insert marker to", MINFRAME, MAXFRAME);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_float_vector(func, "co", 2, 0, -1.0, 1.0, "Coordinate",
	                     "Place new marker at the given frame using specified in normalized space coordinates",
	                     -1.0, 1.0);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "marker", "MovieTrackingMarker", "", "Newly created marker");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "delete_frame", "rna_trackingMarkers_delete_frame");
	RNA_def_function_ui_description(func, "Delete marker at specified frame");
	parm = RNA_def_int(func, "frame", 1, MINFRAME, MAXFRAME, "Frame",
	                   "Frame number to delete marker from", MINFRAME, MAXFRAME);
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

static void rna_def_trackingTrack(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	rna_def_trackingMarker(brna);

	srna = RNA_def_struct(brna, "MovieTrackingTrack", NULL);
	RNA_def_struct_path_func(srna, "rna_trackingTrack_path");
	RNA_def_struct_ui_text(srna, "Movie tracking track data", "Match-moving track data for tracking");
	RNA_def_struct_ui_icon(srna, ICON_ANIM_DATA);

	/* name */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Unique name of track");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_trackingTrack_name_set");
	RNA_def_property_string_maxlength(prop, MAX_ID_NAME-2);
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, NULL);
	RNA_def_struct_name_property(srna, prop);

	/* Pattern */
	prop = RNA_def_property(srna, "pattern_min", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_float_sdna(prop, NULL, "pat_min");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Pattern Min",
	                         "Left-bottom corner of pattern area in normalized coordinates relative "
	                         "to marker position");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_trackerPattern_update");

	prop = RNA_def_property(srna, "pattern_max", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_float_sdna(prop, NULL, "pat_max");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Pattern Max",
	                         "Right-bottom corner of pattern area in normalized coordinates relative "
	                         "to marker position");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_trackerPattern_update");

	/* Search */
	prop = RNA_def_property(srna, "search_min", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_float_sdna(prop, NULL, "search_min");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Search Min",
	                         "Left-bottom corner of search area in normalized coordinates relative "
	                         "to marker position");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_trackerSearch_update");

	prop = RNA_def_property(srna, "search_max", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_float_sdna(prop, NULL, "search_max");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Search Max",
	                         "Right-bottom corner of search area in normalized coordinates relative "
	                         "to marker position");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_trackerSearch_update");

	/* limit frames */
	prop = RNA_def_property(srna, "frames_limit", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "frames_limit");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0, SHRT_MAX);
	RNA_def_property_ui_text(prop, "Frames Limit", "Every tracking cycle, this number of frames are tracked");

	/* pattern match */
	prop = RNA_def_property(srna, "pattern_match", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "pattern_match");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, pattern_match_items);
	RNA_def_property_ui_text(prop, "Pattern Match",
	                         "Track pattern from given frame when tracking marker to next frame");

	/* margin */
	prop = RNA_def_property(srna, "margin", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "margin");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0, 300);
	RNA_def_property_ui_text(prop, "Margin", "Distance from image boudary at which marker stops tracking");

	/* tracking algorithm */
	prop = RNA_def_property(srna, "tracker", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, tracker_items);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Tracker", "Tracking algorithm to use");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_trackerAlgorithm_update");

	/* pyramid level for pyramid klt tracking */
	prop = RNA_def_property(srna, "pyramid_levels", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "pyramid_levels");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1, 16);
	RNA_def_property_ui_text(prop, "Pyramid levels", "Number of pyramid levels (increase on blurry footage)");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_trackerPyramid_update");

	/* minmal correlation - only used for SAD tracker */
	prop = RNA_def_property(srna, "correlation_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_float_sdna(prop, NULL, "minimum_correlation");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_range(prop, -1.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Correlation",
	                         "Minimal value of correlation between matched pattern and reference "
	                         "which is still treated as successful tracking");

	/* markers */
	prop = RNA_def_property(srna, "markers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingMarker");
	RNA_def_property_collection_sdna(prop, NULL, "markers", "markersnr");
	RNA_def_property_ui_text(prop, "Markers", "Collection of markers in track");
	rna_def_trackingMarkers(brna, prop);

	/* ** channels ** */

	/* use_red_channel */
	prop = RNA_def_property(srna, "use_red_channel", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", TRACK_DISABLE_RED);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Use Red Channel", "Use red channel from footage for tracking");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* use_green_channel */
	prop = RNA_def_property(srna, "use_green_channel", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", TRACK_DISABLE_GREEN);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Use Green Channel", "Use green channel from footage for tracking");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* use_blue_channel */
	prop = RNA_def_property(srna, "use_blue_channel", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", TRACK_DISABLE_BLUE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Use Blue Channel", "Use blue channel from footage for tracking");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* preview_grayscale */
	prop = RNA_def_property(srna, "use_grayscale_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TRACK_PREVIEW_GRAYSCALE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Grayscale", "Display what the tracking algorithm sees in the preview");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* has bundle */
	prop = RNA_def_property(srna, "has_bundle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TRACK_HAS_BUNDLE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Has Bundle", "True if track has a valid bundle");

	/* bundle position */
	prop = RNA_def_property(srna, "bundle", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_sdna(prop, NULL, "bundle_pos");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Bundle", "Position of bundle reconstructed from this track");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);

	/* hide */
	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TRACK_HIDDEN);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Hide", "Track is hidden");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* select */
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_trackingTrack_select_get", "rna_trackingTrack_select_set");
	RNA_def_property_ui_text(prop, "Select", "Track is selected");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* select_anchor */
	prop = RNA_def_property(srna, "select_anchor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
	RNA_def_property_ui_text(prop, "Select Anchor", "Track's anchor point is selected");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* select_pattern */
	prop = RNA_def_property(srna, "select_pattern", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pat_flag", SELECT);
	RNA_def_property_ui_text(prop, "Select Pattern", "Track's pattern area is selected");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* select_search */
	prop = RNA_def_property(srna, "select_search", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "search_flag", SELECT);
	RNA_def_property_ui_text(prop, "Select Search", "Track's search area is selected");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* locked */
	prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TRACK_LOCKED);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Lock", "Track is locked and all changes to it are disabled");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* custom color */
	prop = RNA_def_property(srna, "use_custom_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TRACK_CUSTOMCOLOR);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Custom Color", "Use custom color instead of theme-defined");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* color */
	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Color",
	                         "Color of the track in the Movie Clip Editor and the 3D viewport after a solve");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* average error */
	prop = RNA_def_property(srna, "average_error", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "error");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Average Error", "Average error of re-projection");
}

static void rna_def_trackingStabilization(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem filter_items[] = {
		{TRACKING_FILTER_NEAREAST, "NEAREST",   0, "Nearest",   ""},
		{TRACKING_FILTER_BILINEAR, "BILINEAR",   0, "Bilinear",   ""},
		{TRACKING_FILTER_BICUBIC, "BICUBIC", 0, "Bicubic", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "MovieTrackingStabilization", NULL);
	RNA_def_struct_path_func(srna, "rna_trackingStabilization_path");
	RNA_def_struct_ui_text(srna, "Movie tracking stabilization data", "Match-moving stabilization data for tracking");

	/* 2d stabilization */
	prop = RNA_def_property(srna, "use_2d_stabilization", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TRACKING_2D_STABILIZATION);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Use 2D stabilization", "Use 2D stabilization for footage");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_tracking_flushUpdate");

	/* tracks */
	prop = RNA_def_property(srna, "tracks", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_tracking_stabTracks_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MovieTrackingTrack");
	RNA_def_property_ui_text(prop, "Tracks", "Collection of tracks used for stabilization");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_tracking_flushUpdate");

	/* rotation track */
	prop = RNA_def_property(srna, "rotation_track", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "rot_track");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Rotation Track", "Track used to compensate rotation");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_tracking_flushUpdate");

	/* active track index */
	prop = RNA_def_property(srna, "active_track_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "act_track");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_funcs(prop, "rna_tracking_stabTracks_active_index_get",
	                           "rna_tracking_stabTracks_active_index_set",
	                           "rna_tracking_stabTracks_active_index_range");
	RNA_def_property_ui_text(prop, "Active Track Index", "Index of active track in stabilization tracks list");

	/* autoscale */
	prop = RNA_def_property(srna, "use_autoscale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TRACKING_AUTOSCALE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Autoscale",
	                         "Automatically scale footage to cover unfilled areas when stabilizating");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_tracking_flushUpdate");

	/* max scale */
	prop = RNA_def_property(srna, "scale_max", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "maxscale");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Maximal Scale", "Limit the amount of automatic scaling");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_tracking_flushUpdate");

	/* influence_location */
	prop = RNA_def_property(srna, "influence_location", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "locinf");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Location Influence", "Influence of stabilization algorithm on footage location");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_tracking_flushUpdate");

	/* influence_scale */
	prop = RNA_def_property(srna, "influence_scale", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "scaleinf");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Scale Influence", "Influence of stabilization algorithm on footage scale");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_tracking_flushUpdate");

	/* use_stabilize_rotation */
	prop = RNA_def_property(srna, "use_stabilize_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TRACKING_STABILIZE_ROTATION);
	RNA_def_property_ui_text(prop, "Stabilize Rotation", "Stabilize horizon line on the shot");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_tracking_flushUpdate");

	/* influence_rotation */
	prop = RNA_def_property(srna, "influence_rotation", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "rotinf");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Rotation Influence", "Influence of stabilization algorithm on footage rotation");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_tracking_flushUpdate");

	/* filter */
	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "filter");
	RNA_def_property_enum_items(prop, filter_items);
	RNA_def_property_ui_text(prop, "Filter", "Method to use to filter stabilization");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, "rna_tracking_flushUpdate");
}

static void rna_def_reconstructedCamera(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MovieReconstructedCamera", NULL);
	RNA_def_struct_ui_text(srna, "Movie tracking reconstructed camera data",
	                       "Match-moving reconstructed camera data from tracker");

	/* frame */
	prop = RNA_def_property(srna, "frame", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_sdna(prop, NULL, "framenr");
	RNA_def_property_ui_text(prop, "Frame", "Frame number marker is keyframed on");

	/* matrix */
	prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "mat");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Matrix", "Worldspace transformation matrix");

	/* average_error */
	prop = RNA_def_property(srna, "average_error", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "error");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Average Error", "Average error of resonctruction");
}

static void rna_def_trackingReconstruction(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	rna_def_reconstructedCamera(brna);

	srna = RNA_def_struct(brna, "MovieTrackingReconstruction", NULL);
	RNA_def_struct_ui_text(srna, "Movie tracking reconstruction data",
	                       "Match-moving reconstruction data from tracker");

	/* is_valid */
	prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TRACKING_RECONSTRUCTED);
	RNA_def_property_ui_text(prop, "Reconstructed", "Is tracking data contains valid reconstruction information");

	/* average_error */
	prop = RNA_def_property(srna, "average_error", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "error");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Average Error", "Average error of resonctruction");

	/* cameras */
	prop = RNA_def_property(srna, "cameras", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieReconstructedCamera");
	RNA_def_property_collection_sdna(prop, NULL, "cameras", "camnr");
	RNA_def_property_ui_text(prop, "Cameras", "Collection of solved cameras");
}

static void rna_def_trackingTracks(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MovieTrackingTracks", NULL);
	RNA_def_struct_sdna(srna, "MovieTracking");
	RNA_def_struct_ui_text(srna, "Movie Tracks", "Collection of movie tracking tracks");

	func = RNA_def_function(srna, "add", "rna_trackingTracks_add");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Add a number of tracks to this movie clip");
	RNA_def_int(func, "frame", 1, MINFRAME, MAXFRAME, "Frame", "Frame number to add tracks on", MINFRAME, MAXFRAME);
	RNA_def_int(func, "count", 1, 0, INT_MAX, "Number", "Number of tracks to add to the movie clip", 0, INT_MAX);

	/* active track */
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingTrack");
	RNA_def_property_pointer_funcs(prop, "rna_tracking_active_track_get", "rna_tracking_active_track_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Track", "Active track in this tracking data object");
}

static void rna_def_trackingObjectTracks(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MovieTrackingObjectTracks", NULL);
	RNA_def_struct_sdna(srna, "MovieTrackingObject");
	RNA_def_struct_ui_text(srna, "Movie Tracks", "Collection of movie tracking tracks");

	func = RNA_def_function(srna, "add", "rna_trackingObject_tracks_add");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Add a number of tracks to this movie clip");
	RNA_def_int(func, "frame", 1, MINFRAME, MAXFRAME, "Frame", "Frame number to add tracks on", MINFRAME, MAXFRAME);
	RNA_def_int(func, "count", 1, 0, INT_MAX, "Number", "Number of tracks to add to the movie clip", 0, INT_MAX);

	/* active track */
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingTrack");
	RNA_def_property_pointer_funcs(prop, "rna_tracking_active_track_get", "rna_tracking_active_track_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Track", "Active track in this tracking data object");
}

static void rna_def_trackingObject(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MovieTrackingObject", NULL);
	RNA_def_struct_ui_text(srna, "Movie tracking object data", "Match-moving object tracking and reconstruction data");

	/* name */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Unique name of object");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_trackingObject_name_set");
	RNA_def_property_string_maxlength(prop, MAX_ID_NAME-2);
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, NULL);
	RNA_def_struct_name_property(srna, prop);

	/* is_camera */
	prop = RNA_def_property(srna, "is_camera", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TRACKING_OBJECT_CAMERA);
	RNA_def_property_ui_text(prop, "Camera", "Object is used for camera tracking");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);

	/* tracks */
	prop = RNA_def_property(srna, "tracks", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_trackingObject_tracks_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MovieTrackingTrack");
	RNA_def_property_ui_text(prop, "Tracks", "Collection of tracks in this tracking data object");
	RNA_def_property_srna(prop, "MovieTrackingObjectTracks");

	/* reconstruction */
	prop = RNA_def_property(srna, "reconstruction", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingReconstruction");

	/* scale */
	prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_float_sdna(prop, NULL, "scale");
	RNA_def_property_range(prop, 0.0001f, 10000.0f);
	RNA_def_property_ui_range(prop, 0.0001f, 10000.0, 1, 4);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Scale", "Scale of object solution in camera space");
	RNA_def_property_update(prop, NC_MOVIECLIP|NA_EDITED, "rna_trackingObject_flushUpdate");
}

static void rna_def_trackingObjects(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MovieTrackingObjects");
	srna = RNA_def_struct(brna, "MovieTrackingObjects", NULL);
	RNA_def_struct_sdna(srna, "MovieTracking");
	RNA_def_struct_ui_text(srna, "Movie Objects", "Collection of movie trackingobjects");

	func = RNA_def_function(srna, "new", "rna_trackingObject_new");
	RNA_def_function_ui_description(func, "Add tracking object to this movie clip");
	parm = RNA_def_string(func, "name", "", 0, "", "Name of new object");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "object", "MovieTrackingObject", "", "New motion tracking object");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_trackingObject_remove");
	RNA_def_function_ui_description(func, "Remove tracking object from this movie clip");
	RNA_def_pointer(func, "object", "MovieTrackingObject", "", "Motion tracking object to be removed");

	/* active object */
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingObject");
	RNA_def_property_pointer_funcs(prop, "rna_tracking_active_object_get",
	                               "rna_tracking_active_object_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Object", "Active object in this tracking data object");
}

static void rna_def_tracking(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	rna_def_trackingSettings(brna);
	rna_def_trackingCamera(brna);
	rna_def_trackingTrack(brna);
	rna_def_trackingTracks(brna);
	rna_def_trackingObjectTracks(brna);
	rna_def_trackingStabilization(brna);
	rna_def_trackingReconstruction(brna);
	rna_def_trackingObject(brna);

	srna = RNA_def_struct(brna, "MovieTracking", NULL);
	RNA_def_struct_path_func(srna, "rna_tracking_path");
	RNA_def_struct_ui_text(srna, "Movie tracking data", "Match-moving data for tracking");

	/* settings */
	prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingSettings");

	/* camera properties */
	prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingCamera");

	/* tracks */
	prop = RNA_def_property(srna, "tracks", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_trackingTracks_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MovieTrackingTrack");
	RNA_def_property_ui_text(prop, "Tracks", "Collection of tracks in this tracking data object");
	RNA_def_property_srna(prop, "MovieTrackingTracks");

	/* stabilization */
	prop = RNA_def_property(srna, "stabilization", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingStabilization");

	/* reconstruction */
	prop = RNA_def_property(srna, "reconstruction", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MovieTrackingReconstruction");

	/* objects */
	prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_trackingObjects_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MovieTrackingObject");
	RNA_def_property_ui_text(prop, "Objects", "Collection of objects in this tracking data object");
	rna_def_trackingObjects(brna, prop);

	/* active object index */
	prop = RNA_def_property(srna, "active_object_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "objectnr");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_funcs(prop, "rna_tracking_active_object_index_get", "rna_tracking_active_object_index_set",
	                           "rna_tracking_active_object_index_range");
	RNA_def_property_ui_text(prop, "Active Object Index", "Index of active object");
	RNA_def_property_update(prop, NC_MOVIECLIP|ND_DISPLAY, NULL);
}

void RNA_def_tracking(BlenderRNA *brna)
{
	rna_def_tracking(brna);
}

#endif
