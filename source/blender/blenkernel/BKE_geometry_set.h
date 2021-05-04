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

/* Each geometry component has a specific type. The type determines what kind of data the component
 * stores. Functions modifying a geometry will usually just modify a subset of the component types.
 */
typedef enum GeometryComponentType {
  GEO_COMPONENT_TYPE_MESH = 0,
  GEO_COMPONENT_TYPE_POINT_CLOUD = 1,
  GEO_COMPONENT_TYPE_INSTANCES = 2,
  GEO_COMPONENT_TYPE_VOLUME = 3,
  GEO_COMPONENT_TYPE_CURVE = 4,
} GeometryComponentType;

void BKE_geometry_set_free(struct GeometrySet *geometry_set);

bool BKE_geometry_set_has_instances(const struct GeometrySet *geometry_set);

#ifdef __cplusplus
}
#endif
