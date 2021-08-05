/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Adapted from the Blender Alembic importer implementation.
 *
 * Modifications Copyright (C) 2021 Tangent Animation.
 * All rights reserved.
 */
#pragma once

#include "usd.h"

#include <pxr/usd/usd/prim.h>

struct Main;
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
        cache_file(NULL)
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
