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

#include <optional>

#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_memory_counter_fwd.hh"
#include "BLI_span.hh"
#include "BLI_virtual_array_fwd.hh"

namespace blender {

template<typename T> class Span;
namespace bke {
class AttributeAccessor;
struct BVHTreeFromPointCloud;
class MutableAttributeAccessor;
struct PointCloudRuntime;
}  // namespace bke

/** #PointCloud.flag */
enum {
  PT_DS_EXPAND = (1 << 0),
};

struct PointCloud {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_PT;
#endif

  ID id;
  struct AnimData *adt = nullptr; /* animation data (must be immediately after id) */

  int flag = 0;

  /* Geometry */
  int totpoint = 0;

  /** Storage for generic attributes. */
  struct AttributeStorage attribute_storage;

  /* Custom Data */
  struct CustomData pdata_legacy;
  /** Set to -1 when none is active. */
  int attributes_active_index = 0;
  int _pad4 = {};

  /* Material */
  struct Material **mat = nullptr;
  short totcol = 0;
  short _pad3[3] = {};

#ifdef __cplusplus
  Span<float3> positions() const;
  MutableSpan<float3> positions_for_write();

  VArray<float> radius() const;
  MutableSpan<float> radius_for_write();

  bke::AttributeAccessor attributes() const;
  bke::MutableAttributeAccessor attributes_for_write();

  void tag_positions_changed();
  void tag_radii_changed();

  std::optional<Bounds<float3>> bounds_min_max(bool use_radius = true) const;

  /** Get the largest material index used by the point-cloud or `nullopt` if it is empty. */
  std::optional<int> material_index_max() const;

  bke::BVHTreeFromPointCloud bvh_tree() const;

  void count_memory(MemoryCounter &memory) const;
#endif

  bke::PointCloudRuntime *runtime = nullptr;

  /* Draw Cache */
  void *batch_cache = nullptr;
};

/* Only one material supported currently. */
#define POINTCLOUD_MATERIAL_NR 1

}  // namespace blender
