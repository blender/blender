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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */
#ifndef __USD_WRITER_METABALL_H__
#define __USD_WRITER_METABALL_H__

#include "usd_writer_mesh.h"

namespace blender {
namespace io {
namespace usd {

class USDMetaballWriter : public USDGenericMeshWriter {
 public:
  USDMetaballWriter(const USDExporterContext &ctx);

 protected:
  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;
  virtual void free_export_mesh(Mesh *mesh) override;
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;

 private:
  bool is_basis_ball(Scene *scene, Object *ob) const;
};

}  // namespace usd
}  // namespace io
}  // namespace blender

#endif /* __USD_WRITER_METABALL_H__ */
