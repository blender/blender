/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "IO_abstract_hierarchy_iterator.h"
#include "usd.hh"
#include "usd_exporter_context.hh"
#include "usd_skel_convert.hh"

#include <string>

#include <pxr/usd/usd/common.h>
#include <pxr/usd/usd/timeCode.h>

struct Depsgraph;
struct Main;
struct Object;

namespace blender::io::usd {

using blender::io::AbstractHierarchyIterator;
using blender::io::AbstractHierarchyWriter;
using blender::io::HierarchyContext;

class USDHierarchyIterator : public AbstractHierarchyIterator {
 private:
  const pxr::UsdStageRefPtr stage_;
  pxr::UsdTimeCode export_time_;
  const USDExportParams &params_;

  ObjExportMap armature_export_map_;
  ObjExportMap skinned_mesh_export_map_;
  ObjExportMap shape_key_mesh_export_map_;

  /*
   * The field below is mutable because it is used to keep track
   * of what the exporter is doing. This is necessary even when all
   * the other export settings are to remain const.
   */

  /* Map a USD prim path to a list of Blender IDs associated with that prim.
   * This map is updated by writers during stage export. */
  mutable blender::Map<pxr::SdfPath, blender::Vector<ID *>> exported_prim_map_;

  /* Map prototype_paths[instancer path] = [
   *   (proto_path_1, proto_object_1), (proto_path_2, proto_object_2), ... ] */
  Map<pxr::SdfPath, Set<std::pair<pxr::SdfPath, Object *>>> prototype_paths_;

 public:
  USDHierarchyIterator(Main *bmain,
                       Depsgraph *depsgraph,
                       pxr::UsdStageRefPtr stage,
                       const USDExportParams &params);

  void set_export_frame(float frame_nr);

  std::string make_valid_name(const std::string &name) const override;

  void process_usd_skel() const;

  /* Get the USD stage being exported to. */
  pxr::UsdStageRefPtr get_stage() const;

  /* Get the mapping of exported objects to their USD prim paths. */
  const blender::Map<pxr::SdfPath, blender::Vector<ID *>> &get_exported_prim_map() const;

  /* Add an ID to the prim map for a given USD path. */
  void add_to_prim_map(const pxr::SdfPath &usd_path, const ID *id) const;

 protected:
  bool mark_as_weak_export(const Object *object) const override;
  bool determine_point_instancers(const HierarchyContext *context);

  AbstractHierarchyWriter *create_transform_writer(const HierarchyContext *context) override;
  AbstractHierarchyWriter *create_data_writer(const HierarchyContext *context) override;
  AbstractHierarchyWriter *create_hair_writer(const HierarchyContext *context) override;
  AbstractHierarchyWriter *create_particle_writer(const HierarchyContext *context) override;

  void release_writer(AbstractHierarchyWriter *writer) override;

  bool include_data_writers(const HierarchyContext *context) const override;
  bool include_child_writers(const HierarchyContext *context) const override;

 private:
  USDExporterContext create_usd_export_context(const HierarchyContext *context);
  USDExporterContext create_point_instancer_context(
      const HierarchyContext *context, const USDExporterContext &usd_export_context) const;

  void add_usd_skel_export_mapping(const Object *obj, const pxr::SdfPath &usd_path);
};

}  // namespace blender::io::usd
