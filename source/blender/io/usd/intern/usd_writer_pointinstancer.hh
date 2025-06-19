/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_quaternion_types.hh"
#include "usd_attribute_utils.hh"
#include "usd_writer_abstract.hh"
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/points.h>
#include <vector>

struct USDExporterContext;

namespace blender::io::usd {

class USDPointInstancerWriter final : public USDAbstractWriter {
 public:
  USDPointInstancerWriter(const USDExporterContext &ctx,
                          std::set<std::pair<pxr::SdfPath, Object *>> &prototype_paths,
                          std::unique_ptr<USDAbstractWriter> base_writer);
  ~USDPointInstancerWriter() final = default;

 protected:
  virtual void do_write(HierarchyContext &context) override;

 private:
  std::unique_ptr<USDAbstractWriter> base_writer_;
  std::set<std::pair<pxr::SdfPath, Object *>> prototype_paths_;
  const std::string proto_name_ = "Prototype";

  void write_attribute_data(const bke::AttributeIter &attr,
                            const pxr::UsdGeomPointInstancer &usd_instancer,
                            const pxr::UsdTimeCode timecode);

  void process_instance_reference(
      const bke::InstanceReference &reference,
      int instance_index,
      std::map<std::string, int> &proto_index_map,
      std::map<std::string, int> &final_proto_index_map,
      std::map<std::string, pxr::SdfPath> &proto_path_map,
      pxr::UsdStageRefPtr stage,
      pxr::VtArray<int> &proto_indices,
      std::vector<std::pair<int, int>> &collection_instance_object_count_map);

  void compact_prototypes(const pxr::UsdGeomPointInstancer &usd_instancer,
                          const pxr::UsdTimeCode timecode,
                          const pxr::SdfPathVector &proto_paths);

  void override_transform(pxr::UsdStageRefPtr stage,
                          const pxr::SdfPath &proto_path,
                          const float4x4 &transform);

  void handle_collection_prototypes(
      const pxr::UsdGeomPointInstancer &usd_instancer,
      const pxr::UsdTimeCode timecode,
      int instance_num,
      const std::vector<std::pair<int, int>> &collection_instance_object_count_map);
};

}  // namespace blender::io::usd
