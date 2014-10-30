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

// TODO(serrgey): For the time being we're converting simple pipeline
// to an autotrack pipeline we call it tracks.
// Once we've done with porting we remove N.

#ifndef LIBMV_C_API_TRACKSN_H_
#define LIBMV_C_API_TRACKSN_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_TracksN libmv_TracksN;

// Keep order in this enums exactly the same as in mv::Marker.
// Otherwise API wouldn't convert the values properly.
typedef enum libmv_MarkerSource {
  LIBMV_MARKER_SOURCE_MANUAL,
  LIBMV_MARKER_SOURCE_DETECTED,
  LIBMV_MARKER_SOURCE_TRACKED,
  LIBMV_MARKER_SOURCE_MATCHED,
  LIBMV_MARKER_SOURCE_PREDICTED,
} libmv_MarkerSource;

typedef enum libmv_MarkerStatus {
  LIBMV_MARKER_STATUS_UNKNOWN,
  LIBMV_MARKER_STATUS_INLIER,
  LIBMV_MARKER_STATUS_OUTLIER,
} libmv_MarkerStatus;

typedef enum libmv_MarkerModelType {
  LIBMV_MARKER_MODEL_TYPE_POINT,
  LIBMV_MARKER_MODEL_TYPE_PLANE,
  LIBMV_MARKER_MODEL_TYPE_LINE,
  LIBMV_MARKER_MODEL_TYPE_CUBE,
} libmv_MarkerModelType;

enum libmv_MarkerChannel {
  LIBMV_MARKER_CHANNEL_R = (1 << 0),
  LIBMV_MARKER_CHANNEL_G = (1 << 1),
  LIBMV_MARKER_CHANNEL_B = (1 << 2),
};

typedef struct libmv_Marker {
  int clip;
  int frame;
  int track;
  float center[2];
  float patch[4][2];
  float search_region_min[2];
  float search_region_max[2];
  float weight;
  libmv_MarkerSource source;
  libmv_MarkerStatus status;
  int reference_clip;
  int reference_frame;
  libmv_MarkerModelType model_type;
  int model_id;
  int disabled_channels;
} libmv_Marker;

#ifdef __cplusplus
namespace mv {
  class Marker;
}
void libmv_apiMarkerToMarker(const libmv_Marker& libmv_marker,
                             mv::Marker *marker);

void libmv_markerToApiMarker(const mv::Marker& marker,
                             libmv_Marker *libmv_marker);
#endif

libmv_TracksN* libmv_tracksNewN(void);

void libmv_tracksDestroyN(libmv_TracksN* libmv_tracks);


void libmv_tracksAddMarkerN(libmv_TracksN* libmv_tracks,
                            const libmv_Marker* libmv_marker);

void libmv_tracksGetMarkerN(libmv_TracksN* libmv_tracks,
                            int clip,
                            int frame,
                            int track,
                            libmv_Marker* libmv_marker);

void libmv_tracksRemoveMarkerN(libmv_TracksN* libmv_tracks,
                               int clip,
                               int frame,
                               int track);

void libmv_tracksRemoveMarkersForTrack(libmv_TracksN* libmv_tracks,
                                       int track);

int libmv_tracksMaxClipN(libmv_TracksN* libmv_tracks);
int libmv_tracksMaxFrameN(libmv_TracksN* libmv_tracks, int clip);
int libmv_tracksMaxTrackN(libmv_TracksN* libmv_tracks);
int libmv_tracksNumMarkersN(libmv_TracksN* libmv_tracks);

#ifdef __cplusplus
}
#endif

#endif  // LIBMV_C_API_TRACKS_H_
