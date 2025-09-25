/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_string_ref.hh"

struct ImBuf;
struct Mesh;

/**
 * Generate a margin around the textures uv islands by copying pixels from the adjacent polygon.
 *
 * \param ibuf: the texture image.
 * \param mask: pixels with a mask value of 1 are not written to.
 * \param margin: the size of the margin in pixels.
 * \param me: the mesh to use the polygons of.
 * \param uv_layer: The UV layer to use.
 */
void RE_generate_texturemargin_adjacentfaces(struct ImBuf *ibuf,
                                             char *mask,
                                             int margin,
                                             struct Mesh const *me,
                                             blender::StringRef uv_layer,
                                             const float uv_offset[2]);
