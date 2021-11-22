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

#include "abc_writer_mesh.h"
#include "abc_hierarchy_iterator.h"
#include "intern/abc_axis_conversion.h"

#include "BLI_assert.h"
#include "BLI_math_vector.h"

#include "BKE_attribute.h"
#include "BKE_customdata.h"
#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph.h"

#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_particle_types.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

using Alembic::Abc::FloatArraySample;
using Alembic::Abc::Int32ArraySample;
using Alembic::Abc::OObject;
using Alembic::Abc::V2fArraySample;
using Alembic::Abc::V3fArraySample;

using Alembic::AbcGeom::kFacevaryingScope;
using Alembic::AbcGeom::OBoolProperty;
using Alembic::AbcGeom::OCompoundProperty;
using Alembic::AbcGeom::OFaceSet;
using Alembic::AbcGeom::OFaceSetSchema;
using Alembic::AbcGeom::ON3fGeomParam;
using Alembic::AbcGeom::OPolyMesh;
using Alembic::AbcGeom::OPolyMeshSchema;
using Alembic::AbcGeom::OSubD;
using Alembic::AbcGeom::OSubDSchema;
using Alembic::AbcGeom::OV2fGeomParam;
using Alembic::AbcGeom::UInt32ArraySample;

namespace blender::io::alembic {

/* NOTE: Alembic's polygon winding order is clockwise, to match with Renderman. */

static void get_vertices(struct Mesh *mesh, std::vector<Imath::V3f> &points);
static void get_topology(struct Mesh *mesh,
                         std::vector<int32_t> &poly_verts,
                         std::vector<int32_t> &loop_counts,
                         bool &r_has_flat_shaded_poly);
static void get_creases(struct Mesh *mesh,
                        std::vector<int32_t> &indices,
                        std::vector<int32_t> &lengths,
                        std::vector<float> &sharpnesses);
static void get_loop_normals(struct Mesh *mesh,
                             std::vector<Imath::V3f> &normals,
                             bool has_flat_shaded_poly);

ABCGenericMeshWriter::ABCGenericMeshWriter(const ABCWriterConstructorArgs &args)
    : ABCAbstractWriter(args), is_subd_(false)
{
}

void ABCGenericMeshWriter::create_alembic_objects(const HierarchyContext *context)
{
  if (!args_.export_params->apply_subdiv && export_as_subdivision_surface(context->object)) {
    is_subd_ = args_.export_params->use_subdiv_schema;
  }

  if (is_subd_) {
    CLOG_INFO(&LOG, 2, "exporting OSubD %s", args_.abc_path.c_str());
    abc_subdiv_ = OSubD(args_.abc_parent, args_.abc_name, timesample_index_);
    abc_subdiv_schema_ = abc_subdiv_.getSchema();
  }
  else {
    CLOG_INFO(&LOG, 2, "exporting OPolyMesh %s", args_.abc_path.c_str());
    abc_poly_mesh_ = OPolyMesh(args_.abc_parent, args_.abc_name, timesample_index_);
    abc_poly_mesh_schema_ = abc_poly_mesh_.getSchema();

    OCompoundProperty typeContainer = abc_poly_mesh_.getSchema().getUserProperties();
    OBoolProperty type(typeContainer, "meshtype");
    type.set(subsurf_modifier_ == nullptr);
  }
}

Alembic::Abc::OObject ABCGenericMeshWriter::get_alembic_object() const
{
  if (is_subd_) {
    return abc_subdiv_;
  }
  return abc_poly_mesh_;
}

Alembic::Abc::OCompoundProperty ABCGenericMeshWriter::abc_prop_for_custom_props()
{
  if (is_subd_) {
    return abc_schema_prop_for_custom_props(abc_subdiv_schema_);
  }
  return abc_schema_prop_for_custom_props(abc_poly_mesh_schema_);
}

bool ABCGenericMeshWriter::export_as_subdivision_surface(Object *ob_eval) const
{
  ModifierData *md = static_cast<ModifierData *>(ob_eval->modifiers.last);

  for (; md; md = md->prev) {
    /* This modifier has been temporarily disabled by SubdivModifierDisabler,
     * so this indicates this is to be exported as subdivision surface. */
    if (md->type == eModifierType_Subsurf && (md->mode & eModifierMode_DisableTemporary)) {
      return true;
    }
  }

  return false;
}

bool ABCGenericMeshWriter::is_supported(const HierarchyContext *context) const
{
  if (args_.export_params->visible_objects_only) {
    return context->is_object_visible(args_.export_params->evaluation_mode);
  }
  return true;
}

void ABCGenericMeshWriter::do_write(HierarchyContext &context)
{
  Object *object = context.object;
  bool needsfree = false;

  Mesh *mesh = get_export_mesh(object, needsfree);

  if (mesh == nullptr) {
    return;
  }

  if (args_.export_params->triangulate) {
    const bool tag_only = false;
    const int quad_method = args_.export_params->quad_method;
    const int ngon_method = args_.export_params->ngon_method;

    BMeshCreateParams bmesh_create_params{};
    BMeshFromMeshParams bmesh_from_mesh_params{};
    bmesh_from_mesh_params.calc_face_normal = true;
    BMesh *bm = BKE_mesh_to_bmesh_ex(mesh, &bmesh_create_params, &bmesh_from_mesh_params);

    BM_mesh_triangulate(bm, quad_method, ngon_method, 4, tag_only, nullptr, nullptr, nullptr);

    Mesh *triangulated_mesh = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh);
    BM_mesh_free(bm);

    if (needsfree) {
      free_export_mesh(mesh);
    }
    mesh = triangulated_mesh;
    needsfree = true;
  }

  m_custom_data_config.pack_uvs = args_.export_params->packuv;
  m_custom_data_config.mesh = mesh;
  m_custom_data_config.mpoly = mesh->mpoly;
  m_custom_data_config.mloop = mesh->mloop;
  m_custom_data_config.totpoly = mesh->totpoly;
  m_custom_data_config.totloop = mesh->totloop;
  m_custom_data_config.totvert = mesh->totvert;
  m_custom_data_config.timesample_index = timesample_index_;

  try {
    if (is_subd_) {
      write_subd(context, mesh);
    }
    else {
      write_mesh(context, mesh);
    }

    if (needsfree) {
      free_export_mesh(mesh);
    }
  }
  catch (...) {
    if (needsfree) {
      free_export_mesh(mesh);
    }
    throw;
  }
}

void ABCGenericMeshWriter::free_export_mesh(Mesh *mesh)
{
  BKE_id_free(nullptr, mesh);
}

void ABCGenericMeshWriter::write_mesh(HierarchyContext &context, Mesh *mesh)
{
  std::vector<Imath::V3f> points, normals;
  std::vector<int32_t> poly_verts, loop_counts;
  std::vector<Imath::V3f> velocities;
  bool has_flat_shaded_poly = false;

  get_vertices(mesh, points);
  get_topology(mesh, poly_verts, loop_counts, has_flat_shaded_poly);

  if (!frame_has_been_written_ && args_.export_params->face_sets) {
    write_face_sets(context.object, mesh, abc_poly_mesh_schema_);
  }

  OPolyMeshSchema::Sample mesh_sample = OPolyMeshSchema::Sample(
      V3fArraySample(points), Int32ArraySample(poly_verts), Int32ArraySample(loop_counts));

  UVSample uvs_and_indices;

  if (args_.export_params->uvs) {
    const char *name = get_uv_sample(uvs_and_indices, m_custom_data_config, &mesh->ldata);

    if (!uvs_and_indices.indices.empty() && !uvs_and_indices.uvs.empty()) {
      OV2fGeomParam::Sample uv_sample;
      uv_sample.setVals(V2fArraySample(uvs_and_indices.uvs));
      uv_sample.setIndices(UInt32ArraySample(uvs_and_indices.indices));
      uv_sample.setScope(kFacevaryingScope);

      abc_poly_mesh_schema_.setUVSourceName(name);
      mesh_sample.setUVs(uv_sample);
    }

    write_custom_data(
        abc_poly_mesh_schema_.getArbGeomParams(), m_custom_data_config, &mesh->ldata, CD_MLOOPUV);
  }

  if (args_.export_params->normals) {
    get_loop_normals(mesh, normals, has_flat_shaded_poly);

    ON3fGeomParam::Sample normals_sample;
    if (!normals.empty()) {
      normals_sample.setScope(kFacevaryingScope);
      normals_sample.setVals(V3fArraySample(normals));
    }

    mesh_sample.setNormals(normals_sample);
  }

  if (args_.export_params->orcos) {
    write_generated_coordinates(abc_poly_mesh_schema_.getArbGeomParams(), m_custom_data_config);
  }

  if (get_velocities(mesh, velocities)) {
    mesh_sample.setVelocities(V3fArraySample(velocities));
  }

  update_bounding_box(context.object);
  mesh_sample.setSelfBounds(bounding_box_);

  abc_poly_mesh_schema_.set(mesh_sample);

  write_arb_geo_params(mesh);
}

void ABCGenericMeshWriter::write_subd(HierarchyContext &context, struct Mesh *mesh)
{
  std::vector<float> crease_sharpness;
  std::vector<Imath::V3f> points;
  std::vector<int32_t> poly_verts, loop_counts;
  std::vector<int32_t> crease_indices, crease_lengths;
  bool has_flat_poly = false;

  get_vertices(mesh, points);
  get_topology(mesh, poly_verts, loop_counts, has_flat_poly);
  get_creases(mesh, crease_indices, crease_lengths, crease_sharpness);

  if (!frame_has_been_written_ && args_.export_params->face_sets) {
    write_face_sets(context.object, mesh, abc_subdiv_schema_);
  }

  OSubDSchema::Sample subdiv_sample = OSubDSchema::Sample(
      V3fArraySample(points), Int32ArraySample(poly_verts), Int32ArraySample(loop_counts));

  UVSample sample;
  if (args_.export_params->uvs) {
    const char *name = get_uv_sample(sample, m_custom_data_config, &mesh->ldata);

    if (!sample.indices.empty() && !sample.uvs.empty()) {
      OV2fGeomParam::Sample uv_sample;
      uv_sample.setVals(V2fArraySample(sample.uvs));
      uv_sample.setIndices(UInt32ArraySample(sample.indices));
      uv_sample.setScope(kFacevaryingScope);

      abc_subdiv_schema_.setUVSourceName(name);
      subdiv_sample.setUVs(uv_sample);
    }

    write_custom_data(
        abc_subdiv_schema_.getArbGeomParams(), m_custom_data_config, &mesh->ldata, CD_MLOOPUV);
  }

  if (args_.export_params->orcos) {
    write_generated_coordinates(abc_poly_mesh_schema_.getArbGeomParams(), m_custom_data_config);
  }

  if (!crease_indices.empty()) {
    subdiv_sample.setCreaseIndices(Int32ArraySample(crease_indices));
    subdiv_sample.setCreaseLengths(Int32ArraySample(crease_lengths));
    subdiv_sample.setCreaseSharpnesses(FloatArraySample(crease_sharpness));
  }

  update_bounding_box(context.object);
  subdiv_sample.setSelfBounds(bounding_box_);
  abc_subdiv_schema_.set(subdiv_sample);

  write_arb_geo_params(mesh);
}

template<typename Schema>
void ABCGenericMeshWriter::write_face_sets(Object *object, struct Mesh *mesh, Schema &schema)
{
  std::map<std::string, std::vector<int32_t>> geo_groups;
  get_geo_groups(object, mesh, geo_groups);

  std::map<std::string, std::vector<int32_t>>::iterator it;
  for (it = geo_groups.begin(); it != geo_groups.end(); ++it) {
    OFaceSet face_set = schema.createFaceSet(it->first);
    OFaceSetSchema::Sample samp;
    samp.setFaces(Int32ArraySample(it->second));
    face_set.getSchema().set(samp);
  }
}

void ABCGenericMeshWriter::write_arb_geo_params(struct Mesh *me)
{
  if (!args_.export_params->vcolors) {
    return;
  }

  OCompoundProperty arb_geom_params;
  if (is_subd_) {
    arb_geom_params = abc_subdiv_.getSchema().getArbGeomParams();
  }
  else {
    arb_geom_params = abc_poly_mesh_.getSchema().getArbGeomParams();
  }
  write_custom_data(arb_geom_params, m_custom_data_config, &me->ldata, CD_MLOOPCOL);
}

bool ABCGenericMeshWriter::get_velocities(struct Mesh *mesh, std::vector<Imath::V3f> &vels)
{
  /* Export velocity attribute output by fluid sim, sequence cache modifier
   * and geometry nodes. */
  CustomDataLayer *velocity_layer = BKE_id_attribute_find(
      &mesh->id, "velocity", CD_PROP_FLOAT3, ATTR_DOMAIN_POINT);

  if (velocity_layer == nullptr) {
    return false;
  }

  const int totverts = mesh->totvert;
  const float(*mesh_velocities)[3] = reinterpret_cast<float(*)[3]>(velocity_layer->data);

  vels.clear();
  vels.resize(totverts);

  for (int i = 0; i < totverts; i++) {
    copy_yup_from_zup(vels[i].getValue(), mesh_velocities[i]);
  }

  return true;
}

void ABCGenericMeshWriter::get_geo_groups(Object *object,
                                          struct Mesh *mesh,
                                          std::map<std::string, std::vector<int32_t>> &geo_groups)
{
  const int num_poly = mesh->totpoly;
  MPoly *polygons = mesh->mpoly;

  for (int i = 0; i < num_poly; i++) {
    MPoly &current_poly = polygons[i];
    short mnr = current_poly.mat_nr;

    Material *mat = BKE_object_material_get(object, mnr + 1);

    if (!mat) {
      continue;
    }

    std::string name = args_.hierarchy_iterator->get_id_name(&mat->id);

    if (geo_groups.find(name) == geo_groups.end()) {
      std::vector<int32_t> faceArray;
      geo_groups[name] = faceArray;
    }

    geo_groups[name].push_back(i);
  }

  if (geo_groups.empty()) {
    Material *mat = BKE_object_material_get(object, 1);

    std::string name = (mat) ? args_.hierarchy_iterator->get_id_name(&mat->id) : "default";

    std::vector<int32_t> faceArray;

    for (int i = 0, e = mesh->totface; i < e; i++) {
      faceArray.push_back(i);
    }

    geo_groups[name] = faceArray;
  }
}

/* NOTE: Alembic's polygon winding order is clockwise, to match with Renderman. */

static void get_vertices(struct Mesh *mesh, std::vector<Imath::V3f> &points)
{
  points.clear();
  points.resize(mesh->totvert);

  MVert *verts = mesh->mvert;

  for (int i = 0, e = mesh->totvert; i < e; i++) {
    copy_yup_from_zup(points[i].getValue(), verts[i].co);
  }
}

static void get_topology(struct Mesh *mesh,
                         std::vector<int32_t> &poly_verts,
                         std::vector<int32_t> &loop_counts,
                         bool &r_has_flat_shaded_poly)
{
  const int num_poly = mesh->totpoly;
  const int num_loops = mesh->totloop;
  MLoop *mloop = mesh->mloop;
  MPoly *mpoly = mesh->mpoly;
  r_has_flat_shaded_poly = false;

  poly_verts.clear();
  loop_counts.clear();
  poly_verts.reserve(num_loops);
  loop_counts.reserve(num_poly);

  /* NOTE: data needs to be written in the reverse order. */
  for (int i = 0; i < num_poly; i++) {
    MPoly &poly = mpoly[i];
    loop_counts.push_back(poly.totloop);

    r_has_flat_shaded_poly |= (poly.flag & ME_SMOOTH) == 0;

    MLoop *loop = mloop + poly.loopstart + (poly.totloop - 1);

    for (int j = 0; j < poly.totloop; j++, loop--) {
      poly_verts.push_back(loop->v);
    }
  }
}

static void get_creases(struct Mesh *mesh,
                        std::vector<int32_t> &indices,
                        std::vector<int32_t> &lengths,
                        std::vector<float> &sharpnesses)
{
  const float factor = 1.0f / 255.0f;

  indices.clear();
  lengths.clear();
  sharpnesses.clear();

  MEdge *edge = mesh->medge;

  for (int i = 0, e = mesh->totedge; i < e; i++) {
    const float sharpness = static_cast<float>(edge[i].crease) * factor;

    if (sharpness != 0.0f) {
      indices.push_back(edge[i].v1);
      indices.push_back(edge[i].v2);
      sharpnesses.push_back(sharpness);
    }
  }

  lengths.resize(sharpnesses.size(), 2);
}

static void get_loop_normals(struct Mesh *mesh,
                             std::vector<Imath::V3f> &normals,
                             bool has_flat_shaded_poly)
{
  normals.clear();

  /* If all polygons are smooth shaded, and there are no custom normals, we don't need to export
   * normals at all. This is also done by other software, see T71246. */
  if (!has_flat_shaded_poly && !CustomData_has_layer(&mesh->ldata, CD_CUSTOMLOOPNORMAL) &&
      (mesh->flag & ME_AUTOSMOOTH) == 0) {
    return;
  }

  BKE_mesh_calc_normals_split(mesh);
  const float(*lnors)[3] = static_cast<float(*)[3]>(CustomData_get_layer(&mesh->ldata, CD_NORMAL));
  BLI_assert_msg(lnors != nullptr, "BKE_mesh_calc_normals_split() should have computed CD_NORMAL");

  normals.resize(mesh->totloop);

  /* NOTE: data needs to be written in the reverse order. */
  int abc_index = 0;
  MPoly *mp = mesh->mpoly;
  for (int i = 0, e = mesh->totpoly; i < e; i++, mp++) {
    for (int j = mp->totloop - 1; j >= 0; j--, abc_index++) {
      int blender_index = mp->loopstart + j;
      copy_yup_from_zup(normals[abc_index].getValue(), lnors[blender_index]);
    }
  }
}

ABCMeshWriter::ABCMeshWriter(const ABCWriterConstructorArgs &args) : ABCGenericMeshWriter(args)
{
}

Mesh *ABCMeshWriter::get_export_mesh(Object *object_eval, bool & /*r_needsfree*/)
{
  return BKE_object_get_evaluated_mesh(object_eval);
}

}  // namespace blender::io::alembic
