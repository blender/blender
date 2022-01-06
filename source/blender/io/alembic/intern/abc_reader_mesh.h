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

#include "abc_customdata.h"
#include "abc_reader_object.h"

struct Mesh;

namespace blender::io::alembic {

class AbcMeshReader final : public AbcObjectReader {
  Alembic::AbcGeom::IPolyMeshSchema m_schema;

  CDStreamConfig m_mesh_data;

 public:
  AbcMeshReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

  bool valid() const override;
  bool accepts_object_type(const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
                           const Object *const ob,
                           const char **err_str) const override;
  void readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel) override;

  struct Mesh *read_mesh(struct Mesh *existing_mesh,
                         const Alembic::Abc::ISampleSelector &sample_sel,
                         int read_flag,
                         const char *velocity_name,
                         float velocity_scale,
                         const char **err_str) override;
  bool topology_changed(Mesh *existing_mesh,
                        const Alembic::Abc::ISampleSelector &sample_sel) override;

 private:
  void readFaceSetsSample(Main *bmain,
                          Mesh *mesh,
                          const Alembic::AbcGeom::ISampleSelector &sample_sel);

  void assign_facesets_to_mpoly(const Alembic::Abc::ISampleSelector &sample_sel,
                                MPoly *mpoly,
                                int totpoly,
                                std::map<std::string, int> &r_mat_map);
};

class AbcSubDReader final : public AbcObjectReader {
  Alembic::AbcGeom::ISubDSchema m_schema;

  CDStreamConfig m_mesh_data;

 public:
  AbcSubDReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

  bool valid() const override;
  bool accepts_object_type(const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
                           const Object *const ob,
                           const char **err_str) const override;
  void readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel) override;
  struct Mesh *read_mesh(struct Mesh *existing_mesh,
                         const Alembic::Abc::ISampleSelector &sample_sel,
                         int read_flag,
                         const char *velocity_name,
                         float velocity_scale,
                         const char **err_str) override;
};

void read_mverts(MVert *mverts,
                 const Alembic::AbcGeom::P3fArraySamplePtr positions,
                 const Alembic::AbcGeom::N3fArraySamplePtr normals);

CDStreamConfig get_config(struct Mesh *mesh, bool use_vertex_interpolation);

}  // namespace blender::io::alembic
