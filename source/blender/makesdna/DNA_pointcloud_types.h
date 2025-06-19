/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_attribute_types.h"
#include "DNA_customdata_types.h"

#ifdef __cplusplus
#  include <optional>

#  include "BLI_bounds_types.hh"
#  include "BLI_math_vector_types.hh"
#  include "BLI_memory_counter_fwd.hh"
#  include "BLI_span.hh"
#  include "BLI_virtual_array_fwd.hh"
#endif

#ifdef __cplusplus
namespace blender {
template<typename T> class Span;
namespace bke {
class AttributeAccessor;
struct BVHTreeFromPointCloud;
class MutableAttributeAccessor;
struct PointCloudRuntime;
}  // namespace bke
}  // namespace blender
using PointCloudRuntimeHandle = blender::bke::PointCloudRuntime;
#else
typedef struct PointCloudRuntimeHandle PointCloudRuntimeHandle;
#endif

typedef struct PointCloud {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_PT;
#endif

  ID id;
  struct AnimData *adt; /* animation data (must be immediately after id) */

  int flag;

  /* Geometry */
  int totpoint;

  /** Storage for generic attributes. */
  struct AttributeStorage attribute_storage;

  /* Custom Data */
  struct CustomData pdata_legacy;
  /** Set to -1 when none is active. */
  int attributes_active_index;
  int _pad4;

  /* Material */
  struct Material **mat;
  short totcol;
  short _pad3[3];

#ifdef __cplusplus
  blender::Span<blender::float3> positions() const;
  blender::MutableSpan<blender::float3> positions_for_write();

  blender::VArray<float> radius() const;
  blender::MutableSpan<float> radius_for_write();

  blender::bke::AttributeAccessor attributes() const;
  blender::bke::MutableAttributeAccessor attributes_for_write();

  void tag_positions_changed();
  void tag_radii_changed();

  std::optional<blender::Bounds<blender::float3>> bounds_min_max(bool use_radius = true) const;

  /** Get the largest material index used by the point-cloud or `nullopt` if it is empty. */
  std::optional<int> material_index_max() const;

  blender::bke::BVHTreeFromPointCloud bvh_tree() const;

  void count_memory(blender::MemoryCounter &memory) const;
#endif

  PointCloudRuntimeHandle *runtime;

  /* Draw Cache */
  void *batch_cache;
} PointCloud;

/** #PointCloud.flag */
enum {
  PT_DS_EXPAND = (1 << 0),
};

/* Only one material supported currently. */
#define POINTCLOUD_MATERIAL_NR 1
