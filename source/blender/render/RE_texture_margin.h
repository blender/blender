/*
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;
struct IMBuf;
struct MLoopUV;
struct ImBuf;
struct DerivedMesh;

/**
 * Generate a margin around the textures uv islands by copying pixels from the adjacent polygon.
 *
 * \param ibuf: the texture image.
 * \param mask: pixels with a mask value of 1 are not written to.
 * \param margin: the size of the margin in pixels.
 * \param me: the mesh to use the polygons of.
 * \param mloopuv: the uv data to use.
 */
void RE_generate_texturemargin_adjacentfaces(
    struct ImBuf *ibuf, char *mask, const int margin, struct Mesh const *me, char const *uv_layer);

void RE_generate_texturemargin_adjacentfaces_dm(struct ImBuf *ibuf,
                                                char *mask,
                                                const int margin,
                                                struct DerivedMesh *dm);

#ifdef __cplusplus
}
#endif
