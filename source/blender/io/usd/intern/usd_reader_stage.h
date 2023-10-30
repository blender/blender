/* SPDX-FileCopyrightText: 2021 Tangent Animation and. NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

struct Main;

#include "usd.h"
#include "usd_reader_prim.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/imageable.h>

#include <vector>

struct ImportSettings;

namespace blender::io::usd {

typedef std::map<pxr::SdfPath, std::vector<USDPrimReader *>> ProtoReaderMap;

class USDStageReader {

 protected:
  pxr::UsdStageRefPtr stage_;
  USDImportParams params_;
  ImportSettings settings_;

  std::vector<USDPrimReader *> readers_;

  /* USD material prim paths encountered during stage
   * traversal, for importing unused materials. */
  std::vector<std::string> material_paths_;

 public:
  USDStageReader(pxr::UsdStageRefPtr stage,
                 const USDImportParams &params,
                 const ImportSettings &settings);

  ~USDStageReader();

  USDPrimReader *create_reader_if_allowed(const pxr::UsdPrim &prim);

  USDPrimReader *create_reader(const pxr::UsdPrim &prim);

  void collect_readers(struct Main *bmain);

  /**
   * Complete setting up the armature modifiers that
   * were created for skinned meshes by setting the
   * modifier object on the corresponding modifier.
   */
  void process_armature_modifiers() const;

  /* Convert every material prim on the stage to a Blender
   * material, including materials not used by any geometry.
   * Note that collect_readers() must be called before calling
   * import_all_materials(). */
  void import_all_materials(struct Main *bmain);

  /* Add fake users for any imported materials with no
   * users. This is typically required when importing all
   * materials. */
  void fake_users_for_unused_materials();

  bool valid() const;

  pxr::UsdStageRefPtr stage()
  {
    return stage_;
  }
  const USDImportParams &params() const
  {
    return params_;
  }

  const ImportSettings &settings() const
  {
    return settings_;
  }

  void clear_readers();

  const std::vector<USDPrimReader *> &readers() const
  {
    return readers_;
  };

  void sort_readers();

 private:
  USDPrimReader *collect_readers(Main *bmain, const pxr::UsdPrim &prim);

  /**
   * Returns true if the given prim should be included in the
   * traversal based on the import options and the prim's visibility
   * attribute.  Note that the prim will be trivially included
   * if it has no visibility attribute or if the visibility
   * is inherited.
   */
  bool include_by_visibility(const pxr::UsdGeomImageable &imageable) const;

  /**
   * Returns true if the given prim should be included in the
   * traversal based on the import options and the prim's purpose
   * attribute. E.g., return false (to exclude the prim) if the prim
   * represents guide geometry and the 'Import Guide' option is
   * toggled off.
   */
  bool include_by_purpose(const pxr::UsdGeomImageable &imageable) const;

  /*
   * Returns true if the specified UsdPrim is a UsdGeom primitive,
   * procedural shape, such as UsdGeomCube.
   */
  bool is_primitive_prim(const pxr::UsdPrim &prim) const;
};

};  // namespace blender::io::usd
