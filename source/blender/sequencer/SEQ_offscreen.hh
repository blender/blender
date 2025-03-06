/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_object_enums.h"
#include "DNA_view3d_enums.h"

#include "IMB_imbuf_types.hh"

struct GPUOffScreen;
struct GPUViewport;
struct Depsgraph;
struct View3DShading;
struct Object;
enum eDrawType;
enum eV3DOffscreenDrawFlag;

namespace blender::seq {
using DrawViewFn = struct ImBuf *(*)(struct Depsgraph *,
                                     struct Scene *,
                                     struct View3DShading *,
                                     eDrawType,
                                     struct Object *,
                                     int,
                                     int,
                                     enum eImBufFlags,
                                     eV3DOffscreenDrawFlag,
                                     int,
                                     const char *,
                                     struct GPUOffScreen *,
                                     struct GPUViewport *,
                                     char *);
extern DrawViewFn view3d_fn;
}  // namespace blender::seq
