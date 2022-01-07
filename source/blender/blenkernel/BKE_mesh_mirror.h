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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct Mesh;
struct MirrorModifierData;
struct Object;

struct Mesh *BKE_mesh_mirror_bisect_on_mirror_plane_for_modifier(struct MirrorModifierData *mmd,
                                                                 const struct Mesh *mesh,
                                                                 int axis,
                                                                 const float plane_co[3],
                                                                 float plane_no[3]);

void BKE_mesh_mirror_apply_mirror_on_axis(struct Main *bmain,
                                          struct Mesh *mesh,
                                          int axis,
                                          float dist);

/**
 * \warning This should _not_ be used to modify original meshes since
 * it doesn't handle shape-keys, use #BKE_mesh_mirror_apply_mirror_on_axis instead.
 */
struct Mesh *BKE_mesh_mirror_apply_mirror_on_axis_for_modifier(struct MirrorModifierData *mmd,
                                                               struct Object *ob,
                                                               const struct Mesh *mesh,
                                                               int axis,
                                                               bool use_correct_order_on_merge);

#ifdef __cplusplus
}
#endif
