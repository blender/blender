/* SPDX-FileCopyrightText: 2021 Tangent Animation and. NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

struct Main;

#include "usd.h"
#include "usd_reader_prim.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdLux/domeLight.h>

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

  // Readers for scenegraph instance prototypes.
  ProtoReaderMap proto_readers_;

  /* USD dome lights are converted to a world material,
   * rather than light objects, so are handled differently */
  std::vector<pxr::UsdLuxDomeLight> dome_lights_;

  /* USD material prim paths encountered during stage
   * traversal, for importing unused materials. */
  std::vector<std::string> material_paths_;

 public:
  USDStageReader(pxr::UsdStageRefPtr stage,
                 const USDImportParams &params,
                 const ImportSettings &settings);

  ~USDStageReader();

  USDPrimReader *create_reader_if_allowed(const pxr::UsdPrim &prim,
                                          pxr::UsdGeomXformCache *xf_cache);

  USDPrimReader *create_reader(const pxr::UsdPrim &prim, pxr::UsdGeomXformCache *xf_cache);

  void collect_readers(struct Main *bmain);

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

  void clear_proto_readers();

  const ProtoReaderMap &proto_readers() const
  {
    return proto_readers_;
  };

  const std::vector<USDPrimReader *> &readers() const
  {
    return readers_;
  };

  const std::vector<pxr::UsdLuxDomeLight> &dome_lights() const
  {
    return dome_lights_;
  };

  void sort_readers();

 private:
  USDPrimReader *collect_readers(Main *bmain,
                                 const pxr::UsdPrim &prim,
                                 pxr::UsdGeomXformCache *xf_cache,
                                 std::vector<USDPrimReader *> &r_readers);

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

  bool merge_with_parent(USDPrimReader *reader) const;

  /*
   * Returns true if the specified UsdPrim is a UsdGeom primitive,
   * procedural shape, such as UsdGeomCube.
   */
  bool is_primitive_prim(const pxr::UsdPrim &prim) const;
};

}  // namespace blender::io::usd
