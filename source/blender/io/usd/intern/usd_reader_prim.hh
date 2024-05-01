/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#pragma once

#include "usd.hh"

#include "BLI_map.hh"

#include "WM_types.hh"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>

#include <optional>
#include <string>

struct CacheFile;
struct Main;
struct Material;
struct Object;
struct ReportList;

namespace blender::io::usd {

struct ImportSettings {
  bool do_convert_mat;
  float conversion_mat[4][4];

  int from_up;
  int from_forward;
  float scale;
  bool is_sequence;
  bool set_frame_range;

  /* Length and frame offset of file sequences. */
  int sequence_len;
  int sequence_offset;

  /* From MeshSeqCacheModifierData.read_flag */
  int read_flag;

  bool validate_meshes;

  std::function<CacheFile *()> get_cache_file;

  /* Map a USD material prim path to a Blender material name.
   * This map is updated by readers during stage traversal.
   * This field is mutable because it is used to keep track
   * of what the importer is doing. This is necessary even
   * when all the other import settings are to remain const. */
  mutable blender::Map<std::string, std::string> usd_path_to_mat_name;
  /* Map a material name to Blender material.
   * This map is updated by readers during stage traversal,
   * and is mutable similar to the map above. */
  mutable blender::Map<std::string, Material *> mat_name_to_mat;

  /* We use the stage metersPerUnit to convert camera properties from USD scene units to the
   * correct millimeter scale that Blender uses for camera parameters. */
  double stage_meters_per_unit;

  pxr::SdfPath skip_prefix;

  ImportSettings()
      : do_convert_mat(false),
        from_up(0),
        from_forward(0),
        scale(1.0f),
        is_sequence(false),
        set_frame_range(false),
        sequence_len(1),
        sequence_offset(0),
        read_flag(0),
        validate_meshes(false),
        get_cache_file(nullptr),
        stage_meters_per_unit(1.0),
        skip_prefix(pxr::SdfPath{})
  {
  }
};

/* Most generic USD Reader. */

class USDPrimReader {

 protected:
  std::string name_;
  std::string prim_path_;
  Object *object_;
  pxr::UsdPrim prim_;
  const USDImportParams &import_params_;
  USDPrimReader *parent_reader_;
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

  virtual void create_object(Main *bmain, double motionSampleTime) = 0;

  virtual void read_object_data(Main *bmain, double motionSampleTime);

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

  const std::string &name() const
  {
    return name_;
  }
  const std::string &prim_path() const
  {
    return prim_path_;
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
<<<<<<< HEAD
  void set_props(ID *id, const pxr::UsdPrim &prim, double motionSampleTime);
=======
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
   * \param motionSampleTime: The time code for sampling the USD attributes.
   */
  void set_props(bool merge_with_parent = false,
                 pxr::UsdTimeCode motionSampleTime = pxr::UsdTimeCode::Default());
>>>>>>> main
};

}  // namespace blender::io::usd
