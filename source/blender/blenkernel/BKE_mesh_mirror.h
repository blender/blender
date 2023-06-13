/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
                                                               bool use_correct_order_on_merge,
                                                               int **r_vert_merge_map,
                                                               int *r_vert_merge_map_len);

#ifdef __cplusplus
}
#endif
