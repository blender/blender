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

#ifndef __BKE_MESH_MIRROR_H__
#define __BKE_MESH_MIRROR_H__

/** \file
 * \ingroup bke
 */

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct MirrorModifierData;
struct ModifierEvalContext;
struct Object;

struct Mesh *BKE_mesh_mirror_bisect_on_mirror_plane(struct MirrorModifierData *mmd,
                                                    const struct Mesh *mesh,
                                                    int axis,
                                                    const float plane_co[3],
                                                    float plane_no[3]);

struct Mesh *BKE_mesh_mirror_apply_mirror_on_axis(struct MirrorModifierData *mmd,
                                                  const struct ModifierEvalContext *UNUSED(ctx),
                                                  struct Object *ob,
                                                  const struct Mesh *mesh,
                                                  int axis);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_MESH_MIRROR_H__ */
