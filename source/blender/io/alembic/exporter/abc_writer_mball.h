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
 */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_writer_mesh.h"

namespace blender {
namespace io {
namespace alembic {

class ABCMetaballWriter : public ABCGenericMeshWriter {
 public:
  explicit ABCMetaballWriter(const ABCWriterConstructorArgs &args);

 protected:
  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;
  virtual void free_export_mesh(Mesh *mesh) override;
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;
  virtual bool export_as_subdivision_surface(Object *ob_eval) const override;

 private:
  bool is_basis_ball(Scene *scene, Object *ob) const;
};

}  // namespace alembic
}  // namespace io
}  // namespace blender
