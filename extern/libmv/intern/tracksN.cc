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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "intern/tracksN.h"
#include "intern/utildefines.h"
#include "libmv/autotrack/marker.h"
#include "libmv/autotrack/tracks.h"

using mv::Marker;
using mv::Tracks;

void libmv_apiMarkerToMarker(const libmv_Marker& libmv_marker,
                             Marker *marker) {
  marker->clip = libmv_marker.clip;
  marker->frame = libmv_marker.frame;
  marker->track = libmv_marker.track;
  marker->center(0) = libmv_marker.center[0];
  marker->center(1) = libmv_marker.center[1];
  for (int i = 0; i < 4; i++) {
    marker->patch.coordinates(i, 0) = libmv_marker.patch[i][0];
    marker->patch.coordinates(i, 1) = libmv_marker.patch[i][1];
  }
  marker->search_region.min(0) = libmv_marker.search_region_min[0];
  marker->search_region.min(1) = libmv_marker.search_region_min[1];
  marker->search_region.max(0) = libmv_marker.search_region_max[0];
  marker->search_region.max(1) = libmv_marker.search_region_max[1];
  marker->weight = libmv_marker.weight;
  marker->source = (Marker::Source) libmv_marker.source;
  marker->status = (Marker::Status) libmv_marker.status;
  marker->reference_clip = libmv_marker.reference_clip;
  marker->reference_frame = libmv_marker.reference_frame;
  marker->model_type = (Marker::ModelType) libmv_marker.model_type;
  marker->model_id = libmv_marker.model_id;
}

void libmv_markerToApiMarker(const Marker& marker,
                             libmv_Marker *libmv_marker) {
  libmv_marker->clip = marker.clip;
  libmv_marker->frame = marker.frame;
  libmv_marker->track = marker.track;
  libmv_marker->center[0] = marker.center(0);
  libmv_marker->center[1] = marker.center(1);
  for (int i = 0; i < 4; i++) {
    libmv_marker->patch[i][0] = marker.patch.coordinates(i, 0);
    libmv_marker->patch[i][1] = marker.patch.coordinates(i, 1);
  }
  libmv_marker->search_region_min[0] = marker.search_region.min(0);
  libmv_marker->search_region_min[1] = marker.search_region.min(1);
  libmv_marker->search_region_max[0] = marker.search_region.max(0);
  libmv_marker->search_region_max[1] = marker.search_region.max(1);
  libmv_marker->weight = marker.weight;
  libmv_marker->source = (libmv_MarkerSource) marker.source;
  libmv_marker->status = (libmv_MarkerStatus) marker.status;
  libmv_marker->reference_clip = marker.reference_clip;
  libmv_marker->reference_frame = marker.reference_frame;
  libmv_marker->model_type = (libmv_MarkerModelType) marker.model_type;
  libmv_marker->model_id = marker.model_id;
}

libmv_TracksN* libmv_tracksNewN(void) {
  Tracks* tracks = LIBMV_OBJECT_NEW(Tracks);

  return (libmv_TracksN*) tracks;
}

void libmv_tracksDestroyN(libmv_TracksN* libmv_tracks) {
  LIBMV_OBJECT_DELETE(libmv_tracks, Tracks);
}

void libmv_tracksAddMarkerN(libmv_TracksN* libmv_tracks,
                            const libmv_Marker* libmv_marker) {
  Marker marker;
  libmv_apiMarkerToMarker(*libmv_marker, &marker);
  ((Tracks*) libmv_tracks)->AddMarker(marker);
}

void libmv_tracksGetMarkerN(libmv_TracksN* libmv_tracks,
                            int clip,
                            int frame,
                            int track,
                            libmv_Marker* libmv_marker) {
  Marker marker;
  ((Tracks*) libmv_tracks)->GetMarker(clip, frame, track, &marker);
  libmv_markerToApiMarker(marker, libmv_marker);
}

void libmv_tracksRemoveMarkerN(libmv_TracksN* libmv_tracks,
                               int clip,
                               int frame,
                               int track) {
  ((Tracks *) libmv_tracks)->RemoveMarker(clip, frame, track);
}

void libmv_tracksRemoveMarkersForTrack(libmv_TracksN* libmv_tracks,
                                       int track) {
  ((Tracks *) libmv_tracks)->RemoveMarkersForTrack(track);
}

int libmv_tracksMaxClipN(libmv_TracksN* libmv_tracks) {
  return ((Tracks*) libmv_tracks)->MaxClip();
}

int libmv_tracksMaxFrameN(libmv_TracksN* libmv_tracks, int clip) {
  return ((Tracks*) libmv_tracks)->MaxFrame(clip);
}

int libmv_tracksMaxTrackN(libmv_TracksN* libmv_tracks) {
  return ((Tracks*) libmv_tracks)->MaxTrack();
}

int libmv_tracksNumMarkersN(libmv_TracksN* libmv_tracks) {
  return ((Tracks*) libmv_tracks)->NumMarkers();
}
