/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "usd_writer_abstract.hh"

#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include <pxr/usd/usdGeom/pointInstancer.h>

struct USDExporterContext;

namespace blender::io::usd {

class USDPointInstancerWriter final : public USDAbstractWriter {
 private:
  std::unique_ptr<USDAbstractWriter> base_writer_;
  blender::Set<std::pair<pxr::SdfPath, Object *>> prototype_paths_;

 public:
  USDPointInstancerWriter(const USDExporterContext &ctx,
                          const blender::Set<std::pair<pxr::SdfPath, Object *>> &prototype_paths,
                          std::unique_ptr<USDAbstractWriter> base_writer);
  ~USDPointInstancerWriter() override = default;

 protected:
  void do_write(HierarchyContext &context) override;

 private:
  void write_attribute_data(const bke::AttributeIter &attr,
                            const pxr::UsdGeomPointInstancer &usd_instancer,
                            const pxr::UsdTimeCode time);

  void process_instance_reference(
      const bke::InstanceReference &reference,
      int instance_index,
      blender::Map<std::string, int> &proto_index_map,
      blender::Map<std::string, int> &final_proto_index_map,
      blender::Map<std::string, pxr::SdfPath> &proto_path_map,
      pxr::UsdStageRefPtr stage,
      pxr::VtArray<int> &proto_indices,
      blender::Vector<std::pair<int, int>> &collection_instance_object_count_map);

  void compact_prototypes(const pxr::UsdGeomPointInstancer &usd_instancer,
                          const pxr::UsdTimeCode time,
                          const pxr::SdfPathVector &proto_paths) const;

  void override_transform(pxr::UsdStageRefPtr stage,
                          const pxr::SdfPath &proto_path,
                          const float4x4 &transform) const;

  void handle_collection_prototypes(
      const pxr::UsdGeomPointInstancer &usd_instancer,
      const pxr::UsdTimeCode time,
      int instance_num,
      const blender::Span<std::pair<int, int>> collection_instance_object_count_map) const;
};

}  // namespace blender::io::usd
