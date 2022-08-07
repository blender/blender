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
 * The Original Code is Copyright (C) 2022 NVIDIA Corporation.
 * All rights reserved.
 */
#pragma once

#include "usd_writer_mesh.h"

#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/skeleton.h>

struct Key;

namespace blender::io::usd {

bool is_blendshape_mesh(Object *obj);

class USDBlendShapeMeshWriter : public USDMeshWriter {
 public:
  USDBlendShapeMeshWriter(const USDExporterContext &ctx);

  virtual void do_write(HierarchyContext &context) override;

 protected:
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;

  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;

  virtual pxr::UsdTimeCode get_mesh_export_time_code() const override;

  virtual pxr::UsdSkelSkeleton get_skeleton(const HierarchyContext &context) const;

  void write_blendshape(HierarchyContext &context) const;

  void create_blend_shapes(const Key *shape_key,
                           const pxr::UsdPrim &mesh_prim,
                           const pxr::UsdSkelSkeleton &skel) const;

  void add_weights_sample(const Key *shape_key, const pxr::UsdSkelSkeleton &skel) const;

  bool exporting_anim(const Key *shape_key) const;
};

}  // namespace blender::io::usd
