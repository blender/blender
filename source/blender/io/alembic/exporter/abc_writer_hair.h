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

#include "abc_writer_abstract.h"
#include <Alembic/AbcGeom/OCurves.h>
#include <vector>

struct ParticleSettings;
struct ParticleSystem;

namespace blender {
namespace io {
namespace alembic {

class ABCHairWriter : public ABCAbstractWriter {
 private:
  Alembic::AbcGeom::OCurves abc_curves_;
  Alembic::AbcGeom::OCurvesSchema abc_curves_schema_;

  bool uv_warning_shown_;

 public:
  explicit ABCHairWriter(const ABCWriterConstructorArgs &args);

  virtual void create_alembic_objects(const HierarchyContext *context) override;
  virtual const Alembic::Abc::OObject get_alembic_object() const override;

 protected:
  virtual void do_write(HierarchyContext &context) override;
  virtual bool check_is_animated(const HierarchyContext &context) const override;

 private:
  void write_hair_sample(const HierarchyContext &context,
                         struct Mesh *mesh,
                         std::vector<Imath::V3f> &verts,
                         std::vector<Imath::V3f> &norm_values,
                         std::vector<Imath::V2f> &uv_values,
                         std::vector<int32_t> &hvertices);

  void write_hair_child_sample(const HierarchyContext &context,
                               struct Mesh *mesh,
                               std::vector<Imath::V3f> &verts,
                               std::vector<Imath::V3f> &norm_values,
                               std::vector<Imath::V2f> &uv_values,
                               std::vector<int32_t> &hvertices);
};

}  // namespace alembic
}  // namespace io
}  // namespace blender
