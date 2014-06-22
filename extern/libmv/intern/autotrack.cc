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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "intern/autotrack.h"
#include "intern/tracksN.h"
#include "intern/utildefines.h"
#include "libmv/autotrack/autotrack.h"

using mv::AutoTrack;
using mv::FrameAccessor;
using mv::Marker;
using libmv::TrackRegionOptions;
using libmv::TrackRegionResult;

libmv_AutoTrack* libmv_autoTrackNew(libmv_FrameAccessor *frame_accessor) {
  return (libmv_AutoTrack*) LIBMV_OBJECT_NEW(AutoTrack,
                                             (FrameAccessor*) frame_accessor);
}

void libmv_autoTrackDestroy(libmv_AutoTrack* libmv_autotrack) {
  LIBMV_OBJECT_DELETE(libmv_autotrack, AutoTrack);
}

void libmv_autoTrackSetOptions(libmv_AutoTrack* libmv_autotrack,
                               const libmv_AutoTrackOptions* options) {
  AutoTrack *autotrack = ((AutoTrack*) libmv_autotrack);
  libmv_configureTrackRegionOptions(options->track_region,
                                    &autotrack->options.track_region);

  autotrack->options.search_region.min(0) = options->search_region.min[0];
  autotrack->options.search_region.min(1) = options->search_region.min[1];
  autotrack->options.search_region.max(0) = options->search_region.max[0];
  autotrack->options.search_region.max(1) = options->search_region.max[1];
}

int libmv_autoTrackMarker(libmv_AutoTrack* libmv_autotrack,
                          const libmv_TrackRegionOptions* libmv_options,
                          libmv_Marker *libmv_tracked_marker,
                          libmv_TrackRegionResult* libmv_result) {

  Marker tracked_marker;
  TrackRegionOptions options;
  TrackRegionResult result;
  libmv_apiMarkerToMarker(*libmv_tracked_marker, &tracked_marker);
  libmv_configureTrackRegionOptions(*libmv_options,
                                    &options);
  (((AutoTrack*) libmv_autotrack)->TrackMarker(&tracked_marker,
                                               &result,
                                               &options));
  libmv_markerToApiMarker(tracked_marker, libmv_tracked_marker);
  libmv_regionTrackergetResult(result, libmv_result);
  return result.is_usable();
}

void libmv_autoTrackAddMarker(libmv_AutoTrack* libmv_autotrack,
                              const libmv_Marker* libmv_marker) {
  Marker marker;
  libmv_apiMarkerToMarker(*libmv_marker, &marker);
  ((AutoTrack*) libmv_autotrack)->AddMarker(marker);
}

int libmv_autoTrackGetMarker(libmv_AutoTrack* libmv_autotrack,
                             int clip,
                             int frame,
                             int track,
                             libmv_Marker *libmv_marker) {
  Marker marker;
  int ok = ((AutoTrack*) libmv_autotrack)->GetMarker(clip,
                                                     frame,
                                                     track,
                                                     &marker);
  if (ok) {
    libmv_markerToApiMarker(marker, libmv_marker);
  }
  return ok;
}
