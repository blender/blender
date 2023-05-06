/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2004 Blender Foundation */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct GPUOffScreen;

enum eDrawType;
enum eImBufFlags;

typedef struct ImBuf *(*SequencerDrawView)(struct Depsgraph *depsgraph,
                                           struct Scene *scene,
                                           struct View3DShading *shading_override,
                                           enum eDrawType drawtype,
                                           struct Object *camera,
                                           int width,
                                           int height,
                                           enum eImBufFlags flag,
                                           eV3DOffscreenDrawFlag draw_flags,
                                           int alpha_mode,
                                           const char *viewname,
                                           struct GPUOffScreen *ofs,
                                           char err_out[256]);
extern SequencerDrawView sequencer_view3d_fn;

#ifdef __cplusplus
}
#endif
