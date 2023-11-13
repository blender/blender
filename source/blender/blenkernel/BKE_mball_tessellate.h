/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct Object;
struct Scene;
struct Mesh;

struct Mesh *BKE_mball_polygonize(struct Depsgraph *depsgraph,
                                  struct Scene *scene,
                                  struct Object *ob);

void BKE_mball_cubeTable_free(void);

#ifdef __cplusplus
}
#endif
