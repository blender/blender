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

#ifndef LIBMV_C_API_AUTOTRACK_H_
#define LIBMV_C_API_AUTOTRACK_H_

#include "intern/frame_accessor.h"
#include "intern/tracksN.h"
#include "intern/track_region.h"
#include "intern/region.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_AutoTrack libmv_AutoTrack;

typedef struct libmv_AutoTrackOptions {
  libmv_TrackRegionOptions track_region;
  libmv_Region search_region;
} libmv_AutoTrackOptions;

libmv_AutoTrack* libmv_autoTrackNew(libmv_FrameAccessor *frame_accessor);

void libmv_autoTrackDestroy(libmv_AutoTrack* libmv_autotrack);

void libmv_autoTrackSetOptions(libmv_AutoTrack* libmv_autotrack,
                               const libmv_AutoTrackOptions* options);

int libmv_autoTrackMarker(libmv_AutoTrack* libmv_autotrack,
                          const libmv_TrackRegionOptions* libmv_options,
                          libmv_Marker *libmv_tracker_marker,
                          libmv_TrackRegionResult* libmv_result);

void libmv_autoTrackAddMarker(libmv_AutoTrack* libmv_autotrack,
                              const libmv_Marker* libmv_marker);

int libmv_autoTrackGetMarker(libmv_AutoTrack* libmv_autotrack,
                             int clip,
                             int frame,
                             int track,
                             libmv_Marker *libmv_marker);

#ifdef __cplusplus
}
#endif

#endif  // LIBMV_C_API_TRACKS_H_
