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

/** \file
 * \ingroup balembic
 */

#ifndef __ABC_WRITER_MESH_H__
#define __ABC_WRITER_MESH_H__

#include "abc_writer_object.h"
#include "intern/abc_customdata.h"

struct Mesh;
struct ModifierData;

/* Writer for Alembic meshes. Does not assume the object is a mesh object. */
class AbcGenericMeshWriter : public AbcObjectWriter {
 protected:
  Alembic::AbcGeom::OPolyMeshSchema m_mesh_schema;
  Alembic::AbcGeom::OPolyMeshSchema::Sample m_mesh_sample;

  Alembic::AbcGeom::OSubDSchema m_subdiv_schema;
  Alembic::AbcGeom::OSubDSchema::Sample m_subdiv_sample;

  Alembic::Abc::OArrayProperty m_mat_indices;

  bool m_is_animated;
  ModifierData *m_subsurf_mod;

  CDStreamConfig m_custom_data_config;

  bool m_is_liquid;
  bool m_is_subd;

 public:
  AbcGenericMeshWriter(Object *ob,
                       AbcTransformWriter *parent,
                       uint32_t time_sampling,
                       ExportSettings &settings);

  ~AbcGenericMeshWriter();
  void setIsAnimated(bool is_animated);

 protected:
  virtual void do_write();
  virtual bool isAnimated() const;
  virtual Mesh *getEvaluatedMesh(Scene *scene_eval, Object *ob_eval, bool &r_needsfree) = 0;
  virtual void freeEvaluatedMesh(struct Mesh *mesh);

  Mesh *getFinalMesh(bool &r_needsfree);

  void writeMesh(struct Mesh *mesh);
  void writeSubD(struct Mesh *mesh);

  void writeArbGeoParams(struct Mesh *mesh);
  void getGeoGroups(struct Mesh *mesh, std::map<std::string, std::vector<int32_t>> &geoGroups);

  /* fluid surfaces support */
  void getVelocities(struct Mesh *mesh, std::vector<Imath::V3f> &vels);

  template<typename Schema> void writeFaceSets(struct Mesh *mesh, Schema &schema);
};

class AbcMeshWriter : public AbcGenericMeshWriter {
 public:
  AbcMeshWriter(Object *ob,
                AbcTransformWriter *parent,
                uint32_t time_sampling,
                ExportSettings &settings);

  ~AbcMeshWriter();

 protected:
  virtual Mesh *getEvaluatedMesh(Scene *scene_eval, Object *ob_eval, bool &r_needsfree) override;
};

#endif /* __ABC_WRITER_MESH_H__ */
