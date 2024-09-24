/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_attribute_filter.hh"

#include "BLI_array.hh"
#include "BLI_index_mask_fwd.hh"

struct Mesh;
struct PointCloud;
struct Curves;
struct GreasePencil;

namespace blender::bke {
class Instances;
}

namespace blender::geometry {

Array<Mesh *> extract_mesh_vertices(const Mesh &mesh,
                                    const IndexMask &mask,
                                    const bke::AttributeFilter &attribute_filter);

Array<Mesh *> extract_mesh_edges(const Mesh &mesh,
                                 const IndexMask &mask,
                                 const bke::AttributeFilter &attribute_filter);

Array<Mesh *> extract_mesh_faces(const Mesh &mesh,
                                 const IndexMask &mask,
                                 const bke::AttributeFilter &attribute_filter);

Array<PointCloud *> extract_pointcloud_points(const PointCloud &pointcloud,
                                              const IndexMask &mask,
                                              const bke::AttributeFilter &attribute_filter);

Array<Curves *> extract_curves_points(const Curves &curves,
                                      const IndexMask &mask,
                                      const bke::AttributeFilter &attribute_filter);

Array<Curves *> extract_curves(const Curves &curves,
                               const IndexMask &mask,
                               const bke::AttributeFilter &attribute_filter);

Array<bke::Instances *> extract_instances(const bke::Instances &instances,
                                          const IndexMask &mask,
                                          const bke::AttributeFilter &attribute_filter);

Array<GreasePencil *> extract_greasepencil_layers(const GreasePencil &grease_pencil,
                                                  const IndexMask &mask,
                                                  const bke::AttributeFilter &attribute_filter);

Array<GreasePencil *> extract_greasepencil_layer_points(
    const GreasePencil &grease_pencil,
    int layer_i,
    const IndexMask &mask,
    const bke::AttributeFilter &attribute_filter);

Array<GreasePencil *> extract_greasepencil_layer_curves(
    const GreasePencil &grease_pencil,
    int layer_i,
    const IndexMask &mask,
    const bke::AttributeFilter &attribute_filter);

}  // namespace blender::geometry
