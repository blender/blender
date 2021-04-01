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

Mesh *BKE_mesh_boolean(const Mesh **meshes,
                       const float (*obmats[])[4][4],
                       const float (*target_transform)[4][4],
                       const short **material_remaps,
                       const int meshes_len,
                       const bool use_self,
                       const bool hole_tolerant,
                       const int boolean_mode);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#  include "BLI_float4x4.hh"
#  include "BLI_mesh_boolean.hh"
#  include "BLI_span.hh"

namespace blender::meshintersect {

Mesh *direct_mesh_boolean(blender::Span<const Mesh *> meshes,
                          blender::Span<const float4x4 *> obmats,
                          const float4x4 &target_transform,
                          blender::Span<const short *> material_remaps,
                          const bool use_self,
                          const bool hole_tolerant,
                          const int boolean_mode);

}  // namespace blender::meshintersect

#endif
