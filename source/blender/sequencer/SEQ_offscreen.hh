/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

namespace blender {

struct Depsgraph;
struct GPUOffScreen;
struct GPUViewport;
struct ImBuf;
struct Object;
struct Scene;
struct View3DShading;

enum eDrawType;
enum eV3DOffscreenDrawFlag;

namespace seq {
using DrawViewFn = ImBuf *(*)(Depsgraph *,
                              Scene *,
                              View3DShading *,
                              eDrawType,
                              Object *,
                              int,
                              int,
                              eImBufFlags,
                              eV3DOffscreenDrawFlag,
                              int,
                              const char *,
                              GPUOffScreen *,
                              GPUViewport *,
                              char *);
extern DrawViewFn view3d_fn;
}  // namespace seq
}  // namespace blender
