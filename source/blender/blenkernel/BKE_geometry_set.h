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
 */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Collection;
struct GeometrySet;
struct Object;

void BKE_geometry_set_free(struct GeometrySet *geometry_set);

bool BKE_geometry_set_has_instances(const struct GeometrySet *geometry_set);

typedef enum InstancedDataType {
  INSTANCE_DATA_TYPE_OBJECT = 0,
  INSTANCE_DATA_TYPE_COLLECTION = 1,
} InstancedDataType;

typedef struct InstancedData {
  InstancedDataType type;
  union {
    struct Object *object;
    struct Collection *collection;
  } data;
} InstancedData;

int BKE_geometry_set_instances(const struct GeometrySet *geometry_set,
                               float (**r_positions)[3],
                               float (**r_rotations)[3],
                               float (**r_scales)[3],
                               int **r_ids,
                               struct InstancedData **r_instanced_data);

#ifdef __cplusplus
}
#endif
