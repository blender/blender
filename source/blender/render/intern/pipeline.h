/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup render
 */

#pragma once

struct Render;
struct RenderData;
struct RenderLayer;
struct RenderResult;

#ifdef __cplusplus
extern "C" {
#endif

struct RenderLayer *render_get_single_layer(struct Render *re, struct RenderResult *rr);
void render_copy_renderdata(struct RenderData *to, struct RenderData *from);

#ifdef __cplusplus
}
#endif
