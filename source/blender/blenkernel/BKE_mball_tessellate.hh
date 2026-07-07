/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

namespace blender {

struct Depsgraph;
struct Object;
struct Scene;
struct Mesh;

Mesh *BKE_mball_polygonize(Depsgraph *depsgraph, Scene *scene, Object *ob);

void BKE_mball_cubeTable_free();

}  // namespace blender
