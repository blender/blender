/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_customdata_types.h"

#ifdef __cplusplus
namespace blender::bke {
class AttributeAccessor;
class MutableAttributeAccessor;
}  // namespace blender::bke
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PointCloud {
  ID id;
  struct AnimData *adt; /* animation data (must be immediately after id) */

  int flag;

  /* Geometry */
  int totpoint;

  /* Custom Data */
  struct CustomData pdata;
  int attributes_active_index;
  int _pad4;

  /* Material */
  struct Material **mat;
  short totcol;
  short _pad3[3];

#ifdef __cplusplus
  blender::bke::AttributeAccessor attributes() const;
  blender::bke::MutableAttributeAccessor attributes_for_write();
#endif

  /* Draw Cache */
  void *batch_cache;
} PointCloud;

/** #PointCloud.flag */
enum {
  PT_DS_EXPAND = (1 << 0),
};

/* Only one material supported currently. */
#define POINTCLOUD_MATERIAL_NR 1

#ifdef __cplusplus
}
#endif
