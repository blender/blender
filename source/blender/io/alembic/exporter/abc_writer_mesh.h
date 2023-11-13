/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_writer_abstract.h"
#include "intern/abc_customdata.h"

#include <Alembic/AbcGeom/OPolyMesh.h>
#include <Alembic/AbcGeom/OSubD.h>

struct ModifierData;

namespace blender::io::alembic {

/* Writer for Alembic geometry. Does not assume the object is a mesh object. */
class ABCGenericMeshWriter : public ABCAbstractWriter {
 private:
  /* Either poly-mesh or subdivision-surface is used, depending on is_subd_.
   * References to the schema must be kept, or Alembic will not properly write. */
  Alembic::AbcGeom::OPolyMesh abc_poly_mesh_;
  Alembic::AbcGeom::OPolyMeshSchema abc_poly_mesh_schema_;

  Alembic::AbcGeom::OSubD abc_subdiv_;
  Alembic::AbcGeom::OSubDSchema abc_subdiv_schema_;

  /* Determines whether a poly mesh or a subdivision surface is exported.
   * The value is set by an export option but only true if there is a subdivision modifier on the
   * exported object. */
  bool is_subd_;
  ModifierData *subsurf_modifier_;

  CDStreamConfig m_custom_data_config;

 public:
  explicit ABCGenericMeshWriter(const ABCWriterConstructorArgs &args);

  virtual void create_alembic_objects(const HierarchyContext *context) override;
  virtual Alembic::Abc::OObject get_alembic_object() const override;
  Alembic::Abc::OCompoundProperty abc_prop_for_custom_props() override;

 protected:
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual void do_write(HierarchyContext &context) override;

  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) = 0;
  virtual void free_export_mesh(Mesh *mesh);

  virtual bool export_as_subdivision_surface(Object *ob_eval) const;

 private:
  void write_mesh(HierarchyContext &context, Mesh *mesh);
  void write_subd(HierarchyContext &context, Mesh *mesh);
  template<typename Schema> void write_face_sets(Object *object, Mesh *mesh, Schema &schema);

  void write_arb_geo_params(Mesh *me);
  bool get_velocities(Mesh *mesh, std::vector<Imath::V3f> &vels);
  void get_geo_groups(Object *object,
                      Mesh *mesh,
                      std::map<std::string, std::vector<int32_t>> &geo_groups);
};

/* Writer for Alembic geometry of Blender Mesh objects. */
class ABCMeshWriter : public ABCGenericMeshWriter {
 public:
  ABCMeshWriter(const ABCWriterConstructorArgs &args);

 protected:
  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;
};

}  // namespace blender::io::alembic
