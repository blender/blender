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
 * The Original Code is Copyright (C) 2021 NVIDIA Corporation.
 * All rights reserved.
 */
#pragma once

#include "usd_writer_mesh.h"

#include <pxr/usd/usdSkel/bindingAPI.h>

#include <string>
#include <vector>

namespace blender::io::usd {

bool is_skinned_mesh(Object *obj);

class USDSkinnedMeshWriter : public USDMeshWriter {
 public:
  USDSkinnedMeshWriter(const USDExporterContext &ctx);

  virtual void do_write(HierarchyContext &context) override;

 protected:
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;

  void write_weights(const Object *ob,
                     const Mesh *mesh,
                     const pxr::UsdSkelBindingAPI &skel_api,
                     const std::vector<std::string> &bone_names) const;
};

}  // namespace blender::io::usd
