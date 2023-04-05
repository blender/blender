/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation */

#ifndef LIBMV_C_API_AUTOTRACK_H_
#define LIBMV_C_API_AUTOTRACK_H_

#include "intern/frame_accessor.h"
#include "intern/region.h"
#include "intern/track_region.h"
#include "intern/tracksN.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_AutoTrack libmv_AutoTrack;

typedef struct libmv_AutoTrackOptions {
  libmv_TrackRegionOptions track_region;
  libmv_Region search_region;
} libmv_AutoTrackOptions;

libmv_AutoTrack* libmv_autoTrackNew(libmv_FrameAccessor* frame_accessor);

void libmv_autoTrackDestroy(libmv_AutoTrack* libmv_autotrack);

void libmv_autoTrackSetOptions(libmv_AutoTrack* libmv_autotrack,
                               const libmv_AutoTrackOptions* options);

int libmv_autoTrackMarker(libmv_AutoTrack* libmv_autotrack,
                          const libmv_TrackRegionOptions* libmv_options,
                          libmv_Marker* libmv_tracker_marker,
                          libmv_TrackRegionResult* libmv_result);

void libmv_autoTrackAddMarker(libmv_AutoTrack* libmv_autotrack,
                              const libmv_Marker* libmv_marker);

void libmv_autoTrackSetMarkers(libmv_AutoTrack* libmv_autotrack,
                               const libmv_Marker* libmv_marker,
                               size_t num_markers);

int libmv_autoTrackGetMarker(libmv_AutoTrack* libmv_autotrack,
                             int clip,
                             int frame,
                             int track,
                             libmv_Marker* libmv_marker);

#ifdef __cplusplus
}
#endif

#endif  // LIBMV_C_API_TRACKS_H_
