/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_generic_span.hh"
#include "BLI_math_base.hh"
#include "BLI_string_ref.hh"

#include "GEO_grid_samplers.hh"

#include "BKE_attribute.hh"
#include "BKE_volume_enums.hh"
#include "BKE_volume_grid.hh"

namespace blender {

struct Volume;

/** \file
 * \ingroup geo
 */

namespace geometry {

/** Supported rasterization types. */
enum class PointRasterizeType {
  /** Rasterize a float attribute into a float grid. */
  Scalar = 0,
  /** Rasterize a vector attribute into a vector grid. */
  Vector = 1,
};

/** Kernel functions for weighting point influence on surrounding voxels. */
enum class KernelType {
  /** Only the nearest voxel is affected. */
  NearestPoint = 0,
  /** Linear falloff over 1 voxel. */
  Linear = 1,
  /** Quadratic falloff over 1.5 voxels with a continuous derivative. */
  Quadratic = 2,
  /** Cubic falloff over 2 voxels with a continuous and smooth derivative. */
  Cubic = 3,
};

/** Attribute type required for the given rasterization type. */
const CPPType &points_rasterize_attribute_type(const PointRasterizeType rasterize_type);
/** Grid type produced by the given rasterization type. */
const CPPType &points_rasterize_grid_type(const PointRasterizeType rasterize_type);

#ifdef WITH_OPENVDB

/**
 * Add a new fog VolumeGrid to the Volume by converting the supplied points.
 */
bke::VolumeGridData *fog_volume_grid_add_from_points(Volume *volume,
                                                     StringRefNull name,
                                                     Span<float3> positions,
                                                     Span<float> radii,
                                                     float voxel_size,
                                                     float density);

bke::VolumeGrid<float> points_to_sdf_grid(Span<float3> positions,
                                          Span<float> radii,
                                          float voxel_size);

/** True for data types that can be stored as point data grid attributes. */
bool is_point_attribute_grid_supported(const CPPType &cpp_type);

/**
 * Description of an attribute array to store in a point data grid.
 */
struct PointDataGridAttributeInfo {
  StringRef name;
  GSpan data;
};

using PointAttributeNameMap = Vector<std::pair<std::string, std::string>>;

struct MappedPointDataGrid {
  /* Point data grid. */
  bke::GVolumeGrid grid;
  /* Attribute pairs mapping original attribute names to internal identifiers.
   * OpenVDB PointDataGrid does not allow certain characters in attribute names, so internal names
   * are generated and mapped to original names. */
  PointAttributeNameMap attribute_map;
};

/**
 * Construct a point data grid from a positions array and optional attributes.
 * The resulting grid is of the \a VOLUME_GRID_POINTS type.
 *
 * \param positions Point positions array.
 * \param attributes Attributes to store in the grid.
 * \param transform Grid transform defining voxel size and offset.
 */
MappedPointDataGrid points_to_point_data_grid(const Span<float3> positions,
                                              const Span<PointDataGridAttributeInfo> attributes,
                                              const float4x4 &transform);

/**
 * Construct a point data grid from a positions array and optional attributes.
 * The resulting grid is of the \a VOLUME_GRID_POINTS type.
 *
 * \param positions Point positions array.
 * \param attributes Attributes to store in the grid.
 * \param attribute_filter Optional filter for attributes to be stored.
 * \param transform Grid transform defining voxel size and offset.
 */
MappedPointDataGrid points_to_point_data_grid(const VArray<float3> positions,
                                              const bke::AttributeAccessor &attributes,
                                              const bke::AttributeFilter &attribute_filter,
                                              const float4x4 &transform);

/** Rasterization settings for a single attribute. */
struct PointRasterizeAttributeInfo {
  StringRef name;
  PointRasterizeType type;
};

/**
 * Rasterize points into grids using a weighting kernel.
 *
 * Each point attribute generates an output grid.
 * Each voxel contains the weighted sum of points within the maximum range of the voxel center.
 * Each point contributes a value according to the kernel function. The kernel function takes
 * the distance between the voxel and particle and computes a weighting factor, falling to zero
 * within the range of the kernel.
 *
 * \param point_data_grid Point grid with optional attributes.
 * \param kernel_type Weighting kernel function.
 * \param point_attributes List of attributes that should be converted to grids.
 * \param transform Grid transform defining voxel size and offset.
 * \param r_attribute_grids List of output grids, must have the same size as \a point_attributes.
 */
void points_rasterize(const MappedPointDataGrid &point_data_grid,
                      const KernelType kernel_type,
                      Span<PointRasterizeAttributeInfo> point_attributes,
                      const float4x4 &transform,
                      MutableSpan<bke::GVolumeGrid> r_attribute_grids);

#endif
}  // namespace geometry
}  // namespace blender
