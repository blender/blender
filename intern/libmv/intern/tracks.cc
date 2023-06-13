/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation */

#include "intern/tracks.h"
#include "intern/utildefines.h"

#include "libmv/simple_pipeline/tracks.h"

using libmv::Marker;
using libmv::Tracks;

libmv_Tracks* libmv_tracksNew(void) {
  Tracks* tracks = LIBMV_OBJECT_NEW(Tracks);

  return (libmv_Tracks*)tracks;
}

void libmv_tracksDestroy(libmv_Tracks* libmv_tracks) {
  LIBMV_OBJECT_DELETE(libmv_tracks, Tracks);
}

void libmv_tracksInsert(libmv_Tracks* libmv_tracks,
                        int image,
                        int track,
                        double x,
                        double y,
                        double weight) {
  ((Tracks*)libmv_tracks)->Insert(image, track, x, y, weight);
}
