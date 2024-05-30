/* SPDX-FileCopyrightText: 2021 Tangent Animation and. NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "usd.hh"
#include "usd_hash_types.hh"
#include "usd_reader_prim.hh"

#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdLux/domeLight.h>

#include <string>

struct Collection;
struct ImportSettings;
struct Main;
struct ReportList;

namespace blender::io::usd {

class USDPointInstancerReader;

/**
 * Map a USD prototype prim path to the list of readers that convert
 * the prototype data.
 */
using ProtoReaderMap = blender::Map<pxr::SdfPath, blender::Vector<USDPrimReader *>>;

using UsdPathSet = blender::Set<pxr::SdfPath>;

class USDStageReader {

 protected:
  pxr::UsdStageRefPtr stage_;
  USDImportParams params_;
  ImportSettings settings_;

  blender::Vector<USDPrimReader *> readers_;

  /* USD dome lights are converted to a world material,
   * rather than light objects, so are handled differently */
  blender::Vector<pxr::UsdLuxDomeLight> dome_lights_;

  /* USD material prim paths encountered during stage
   * traversal, for importing unused materials. */
  blender::Vector<std::string> material_paths_;

  /* Readers for scene-graph instance prototypes. */
  ProtoReaderMap proto_readers_;

  /* Readers for point instancer prototypes. */
  ProtoReaderMap instancer_proto_readers_;

 public:
  USDStageReader(pxr::UsdStageRefPtr stage,
                 const USDImportParams &params,
                 const ImportSettings &settings);

  ~USDStageReader();

  USDPrimReader *create_reader_if_allowed(const pxr::UsdPrim &prim);

  USDPrimReader *create_reader(const pxr::UsdPrim &prim);

  void collect_readers();

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

  /** Get the wmJobWorkerStatus-provided `reports` list pointer, to use with the BKE_report API. */
  ReportList *reports() const
  {
    return params_.worker_status ? params_.worker_status->reports : nullptr;
  }

  /** Clear all cached reader collections. */
  void clear_readers();

  const blender::Vector<USDPrimReader *> &readers() const
  {
    return readers_;
  };

  const blender::Vector<pxr::UsdLuxDomeLight> &dome_lights() const
  {
    return dome_lights_;
  };

  void sort_readers();

  /**
   * Create prototype collections for instancing by the USD instance readers.
   */
  void create_proto_collections(Main *bmain, Collection *parent_collection);

 private:
  /**
   * Create readers for the subtree rooted at the given prim and append the
   * new readers in r_readers.
   *
   * \param prim: Root of the subtree to convert to readers
   * \param pruned_prims: Set of paths to prune when iterating over the
   *                      stage during conversion.  I.e., these prims
   *                      and their descendants will not be converted to
   *                      readers.
   * \param defined_prims_only: If true, only defined prims will be converted,
   *                            skipping abstract and over prims.  This should
   *                            be set to false when converting point instancer
   *                            prototype prims, which can be declared as overs.
   * \param r_readers: Readers created for the prims in the converted subtree.
   * \return A pointer to the reader created for the given prim or null if
   *         the prim cannot be converted.
   */
  USDPrimReader *collect_readers(const pxr::UsdPrim &prim,
                                 const UsdPathSet &pruned_prims,
                                 bool defined_prims_only,
                                 blender::Vector<USDPrimReader *> &r_readers);

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

  /**
   * Returns true if the specified UsdPrim is a UsdGeom primitive,
   * procedural shape, such as UsdGeomCube.
   */
  bool is_primitive_prim(const pxr::UsdPrim &prim) const;

  /**
   * Iterate over the stage and return the paths of all prototype
   * primitives references by point instancers.
   *
   * \return The prototype paths, or an empty path set if the scene
   *         does not contain any point instancers.
   */
  UsdPathSet collect_point_instancer_proto_paths() const;

  /**
   * Populate the instancer_proto_readers_ map for the prototype prims
   * in the given set.  For each prototype path, this function will
   * create readers for the prims in the subtree rooted at the prototype
   * prim.
   */
  void create_point_instancer_proto_readers(const UsdPathSet &proto_paths);
};

};  // namespace blender::io::usd
