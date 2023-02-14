/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

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
  GEO_COMPONENT_TYPE_EDIT = 5,
} GeometryComponentType;

#define GEO_COMPONENT_TYPE_ENUM_SIZE 6

void BKE_geometry_set_free(struct GeometrySet *geometry_set);

bool BKE_object_has_geometry_set_instances(const struct Object *ob);

#ifdef __cplusplus
}
#endif
