/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "BLI_span.hh"

#include "abc_customdata.h"
#include "abc_reader_object.h"

struct Mesh;

namespace blender::io::alembic {

class AbcMeshReader final : public AbcObjectReader {
  Alembic::AbcGeom::IPolyMeshSchema m_schema;

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
  bool topology_changed(const Mesh *existing_mesh,
                        const Alembic::Abc::ISampleSelector &sample_sel) override;

 private:
  void readFaceSetsSample(Main *bmain,
                          Mesh *mesh,
                          const Alembic::AbcGeom::ISampleSelector &sample_sel);

  void assign_facesets_to_material_indices(const Alembic::Abc::ISampleSelector &sample_sel,
                                           MutableSpan<int> material_indices,
                                           std::map<std::string, int> &r_mat_map);
};

class AbcSubDReader final : public AbcObjectReader {
  Alembic::AbcGeom::ISubDSchema m_schema;

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

void read_mverts(Mesh &mesh,
                 const Alembic::AbcGeom::P3fArraySamplePtr positions,
                 const Alembic::AbcGeom::N3fArraySamplePtr normals);

CDStreamConfig get_config(struct Mesh *mesh);

}  // namespace blender::io::alembic
