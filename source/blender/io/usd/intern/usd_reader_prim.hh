/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#pragma once

#include "usd.hh"
#include "usd_hash_types.hh"

#include "BLI_map.hh"
#include "BLI_set.hh"

#include "WM_types.hh"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>

#include <string>

struct CacheFile;
struct Main;
struct Material;
struct Object;
struct ReportList;

namespace blender::io::usd {

struct ImportSettings {
  bool blender_stage_version_prior_44 = false;
  bool do_convert_mat = false;
  float conversion_mat[4][4] = {};

  /* From MeshSeqCacheModifierData.read_flag */
  int read_flag = 0;

  std::function<CacheFile *()> get_cache_file{};

  /*
   * The fields below are mutable because they are used to keep track
   * of what the importer is doing. This is necessary even when all
   * the other import settings are to remain const.
   */

  /* Map a USD material prim path to a Blender material.
   * This map is updated by readers during stage traversal. */
  mutable blender::Map<pxr::SdfPath, Material *> usd_path_to_mat{};
  /* Map a material name to Blender material.
   * This map is updated by readers during stage traversal. */
  mutable blender::Map<std::string, Material *> mat_name_to_mat{};
  /* Map a USD material prim path to a Blender material to be
   * converted by invoking the 'on_material_import' USD hook.
   * This map is updated by readers during stage traversal. */
  mutable blender::Map<pxr::SdfPath, Material *> usd_path_to_mat_for_hook{};
  /* Set of paths to USD material primitives that can be converted by the
   * 'on_material_import' USD hook. For efficiency this set should
   * be populated prior to stage traversal. */
  mutable blender::Set<pxr::SdfPath> mat_import_hook_sources{};

  /* We use the stage metersPerUnit to convert camera properties from USD scene units to the
   * correct millimeter scale that Blender uses for camera parameters. */
  double stage_meters_per_unit = 1.0;

  pxr::SdfPath skip_prefix{};

  /* Combined user-specified and unit conversion scales. */
  double scene_scale = 1.0;
};

/* Most generic USD Reader. */

class USDPrimReader {

 protected:
  StringRefNull name_;
  Object *object_;
  pxr::UsdPrim prim_;
  USDPrimReader *parent_reader_;
  const USDImportParams &import_params_;
  const ImportSettings *settings_;
  int refcount_;
  bool is_in_instancer_proto_;

 public:
  USDPrimReader(const pxr::UsdPrim &prim,
                const USDImportParams &import_params,
                const ImportSettings &settings);
  virtual ~USDPrimReader();

  const pxr::UsdPrim &prim() const;

  virtual bool valid() const;

  virtual void create_object(Main *bmain) = 0;
  virtual void read_object_data(Main * /*bmain*/, pxr::UsdTimeCode /*time*/) {};

  Object *object() const;
  void object(Object *ob);

  USDPrimReader *parent() const
  {
    return parent_reader_;
  }
  void parent(USDPrimReader *parent)
  {
    parent_reader_ = parent;
  }

  /** Get the wmJobWorkerStatus-provided `reports` list pointer, to use with the BKE_report API. */
  ReportList *reports() const
  {
    return import_params_.worker_status ? import_params_.worker_status->reports : nullptr;
  }

  /* Since readers might be referenced through handles
   * maintained by modifiers and constraints, we provide
   * a reference count to facilitate managing the object
   * lifetime.
   * TODO(makowalski): investigate transitioning to using
   * smart pointers for readers, or, alternatively look into
   * making the lifetime management more robust, e.g., by
   * making the destructors protected and implementing deletion
   * in decref(), etc. */
  int refcount() const;
  void incref();
  void decref();

  StringRefNull name() const
  {
    return name_;
  }
  pxr::SdfPath prim_path() const
  {
    return prim_.GetPrimPath();
  }

  virtual pxr::SdfPath object_prim_path() const
  {
    return prim_path();
  }

  virtual pxr::SdfPath data_prim_path() const
  {
    return prim_path();
  }

  void set_is_in_instancer_proto(bool flag)
  {
    is_in_instancer_proto_ = flag;
  }

  bool is_in_instancer_proto() const
  {
    return is_in_instancer_proto_;
  }

  bool is_in_proto() const;

 protected:
  /**
   * Convert custom attributes on the encapsulated USD prim (or on its parent)
   * to custom properties on the generated object and/or data.  This function
   * assumes create_object() and read_object_data() have been called.
   *
   * If the generated object has instantiated data, it's assumed that the data
   * represents the USD prim, and the prim properties will be set on the data ID.
   * If the object data is null (which would be the case when a USD Xform is
   * converted to an Empty object), then the prim properties will be set on the
   * object ID.  Finally, a true value for the 'merge_with_parent' argument indicates
   * that the object represents a USD Xform and its child prim that were merged
   * on import, and the properties of the prim's parent will be set on the object
   * ID.
   *
   * \param merge_with_parent: If true, set the properties of the prim's parent
   *                           on the object ID
   * \param time: The time code for sampling the USD attributes.
   */
  void set_props(bool merge_with_parent = false,
                 pxr::UsdTimeCode time = pxr::UsdTimeCode::Default());
};

}  // namespace blender::io::usd
