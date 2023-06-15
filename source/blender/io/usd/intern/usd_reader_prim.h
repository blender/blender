/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#pragma once

#include "usd.h"

#include <pxr/usd/usd/prim.h>

#include <map>
#include <string>

struct CacheFile;
struct Main;
struct Material;
struct Object;

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

  CacheFile *cache_file;

  /* Map a USD material prim path to a Blender material name.
   * This map is updated by readers during stage traversal.
   * This field is mutable because it is used to keep track
   * of what the importer is doing. This is necessary even
   * when all the other import settings are to remain const. */
  mutable std::map<std::string, std::string> usd_path_to_mat_name;
  /* Map a material name to Blender material.
   * This map is updated by readers during stage traversal,
   * and is mutable similar to the map above. */
  mutable std::map<std::string, Material *> mat_name_to_mat;

  /* We use the stage metersPerUnit to convert camera properties from USD scene units to the
   * correct millimeter scale that Blender uses for camera parameters. */
  double stage_meters_per_unit;

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
        cache_file(NULL),
        stage_meters_per_unit(1.0)
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

 public:
  USDPrimReader(const pxr::UsdPrim &prim,
                const USDImportParams &import_params,
                const ImportSettings &settings);
  virtual ~USDPrimReader();

  const pxr::UsdPrim &prim() const;

  virtual bool valid() const;

  virtual void create_object(Main *bmain, double motionSampleTime) = 0;
  virtual void read_object_data(Main * /* bmain */, double /* motionSampleTime */){};

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
};

}  // namespace blender::io::usd
