/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation */

#ifndef LIBMV_C_API_TRACKS_H_
#define LIBMV_C_API_TRACKS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_Tracks libmv_Tracks;

libmv_Tracks* libmv_tracksNew(void);

void libmv_tracksDestroy(libmv_Tracks* libmv_tracks);

void libmv_tracksInsert(libmv_Tracks* libmv_tracks,
                        int image,
                        int track,
                        double x,
                        double y,
                        double weight);

#ifdef __cplusplus
}
#endif

#endif  // LIBMV_C_API_TRACKS_H_
