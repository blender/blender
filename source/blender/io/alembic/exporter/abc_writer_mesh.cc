/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_writer_mesh.h"
#include "abc_hierarchy_iterator.h"
#include "intern/abc_axis_conversion.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.hh"
#include "BKE_subdiv.hh"

#include "bmesh.hh"
#include "bmesh_tools.hh"

#include "DNA_customdata_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

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

static void get_vertices(Mesh *mesh, std::vector<Imath::V3f> &points);
static void get_topology(Mesh *mesh,
                         std::vector<int32_t> &face_verts,
                         std::vector<int32_t> &loop_counts);
static void get_edge_creases(Mesh *mesh,
                             std::vector<int32_t> &indices,
                             std::vector<int32_t> &lengths,
                             std::vector<float> &sharpnesses);
static void get_vert_creases(Mesh *mesh,
                             std::vector<int32_t> &indices,
                             std::vector<float> &sharpnesses);
static void get_loop_normals(const Mesh *mesh, std::vector<Imath::V3f> &normals);

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
    CLOG_DEBUG(&LOG, "exporting OSubD %s", args_.abc_path.c_str());
    abc_subdiv_ = OSubD(args_.abc_parent, args_.abc_name, timesample_index_);
    abc_subdiv_schema_ = abc_subdiv_.getSchema();
  }
  else {
    CLOG_DEBUG(&LOG, "exporting OPolyMesh %s", args_.abc_path.c_str());
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
  return context->is_object_visible(args_.export_params->evaluation_mode);
}

void ABCGenericMeshWriter::do_write(HierarchyContext &context)
{
  Object *object = context.object;
  bool needsfree = false;

  Mesh *mesh = get_export_mesh(object, needsfree);

  if (mesh == nullptr) {
    return;
  }

  /* Ensure data exists if currently in edit mode. */
  BKE_mesh_wrapper_ensure_mdata(mesh);

  if (args_.export_params->triangulate) {
    const bool tag_only = false;
    const int quad_method = args_.export_params->quad_method;
    const int ngon_method = args_.export_params->ngon_method;

    BMeshCreateParams bmesh_create_params{};
    BMeshFromMeshParams bmesh_from_mesh_params{};
    bmesh_from_mesh_params.calc_face_normal = true;
    bmesh_from_mesh_params.calc_vert_normal = true;
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
  m_custom_data_config.face_offsets = mesh->face_offsets_for_write().data();
  m_custom_data_config.corner_verts = mesh->corner_verts_for_write().data();
  m_custom_data_config.faces_num = mesh->faces_num;
  m_custom_data_config.totloop = mesh->corners_num;
  m_custom_data_config.totvert = mesh->verts_num;
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
  std::vector<int32_t> face_verts, loop_counts;
  std::vector<Imath::V3f> velocities;

  get_vertices(mesh, points);
  get_topology(mesh, face_verts, loop_counts);

  if (!frame_has_been_written_ && args_.export_params->face_sets) {
    write_face_sets(context.object, mesh, abc_poly_mesh_schema_);
  }

  OPolyMeshSchema::Sample mesh_sample = OPolyMeshSchema::Sample(
      V3fArraySample(points), Int32ArraySample(face_verts), Int32ArraySample(loop_counts));

  UVSample uvs_and_indices;

  if (args_.export_params->uvs) {
    const char *name = get_uv_sample(uvs_and_indices, m_custom_data_config, *mesh);

    if (!uvs_and_indices.indices.empty() && !uvs_and_indices.uvs.empty()) {
      OV2fGeomParam::Sample uv_sample;
      uv_sample.setVals(V2fArraySample(uvs_and_indices.uvs));
      uv_sample.setIndices(UInt32ArraySample(uvs_and_indices.indices));
      uv_sample.setScope(kFacevaryingScope);

      abc_poly_mesh_schema_.setUVSourceName(name);
      mesh_sample.setUVs(uv_sample);
    }

    write_custom_data(
        abc_poly_mesh_schema_.getArbGeomParams(), m_custom_data_config, *mesh, CD_PROP_FLOAT2);
  }

  if (args_.export_params->normals) {
    get_loop_normals(mesh, normals);

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

void ABCGenericMeshWriter::write_subd(HierarchyContext &context, Mesh *mesh)
{
  std::vector<float> edge_crease_sharpness, vert_crease_sharpness;
  std::vector<Imath::V3f> points;
  std::vector<int32_t> face_verts, loop_counts;
  std::vector<int32_t> edge_crease_indices, edge_crease_lengths, vert_crease_indices;

  get_vertices(mesh, points);
  get_topology(mesh, face_verts, loop_counts);
  get_edge_creases(mesh, edge_crease_indices, edge_crease_lengths, edge_crease_sharpness);
  get_vert_creases(mesh, vert_crease_indices, vert_crease_sharpness);

  if (!frame_has_been_written_ && args_.export_params->face_sets) {
    write_face_sets(context.object, mesh, abc_subdiv_schema_);
  }

  OSubDSchema::Sample subdiv_sample = OSubDSchema::Sample(
      V3fArraySample(points), Int32ArraySample(face_verts), Int32ArraySample(loop_counts));

  UVSample sample;
  if (args_.export_params->uvs) {
    const char *name = get_uv_sample(sample, m_custom_data_config, *mesh);

    if (!sample.indices.empty() && !sample.uvs.empty()) {
      OV2fGeomParam::Sample uv_sample;
      uv_sample.setVals(V2fArraySample(sample.uvs));
      uv_sample.setIndices(UInt32ArraySample(sample.indices));
      uv_sample.setScope(kFacevaryingScope);

      abc_subdiv_schema_.setUVSourceName(name);
      subdiv_sample.setUVs(uv_sample);
    }

    write_custom_data(
        abc_subdiv_schema_.getArbGeomParams(), m_custom_data_config, *mesh, CD_PROP_FLOAT2);
  }

  if (args_.export_params->orcos) {
    write_generated_coordinates(abc_subdiv_schema_.getArbGeomParams(), m_custom_data_config);
  }

  if (!edge_crease_indices.empty()) {
    subdiv_sample.setCreaseIndices(Int32ArraySample(edge_crease_indices));
    subdiv_sample.setCreaseLengths(Int32ArraySample(edge_crease_lengths));
    subdiv_sample.setCreaseSharpnesses(FloatArraySample(edge_crease_sharpness));
  }

  if (!vert_crease_indices.empty()) {
    subdiv_sample.setCornerIndices(Int32ArraySample(vert_crease_indices));
    subdiv_sample.setCornerSharpnesses(FloatArraySample(vert_crease_sharpness));
  }

  update_bounding_box(context.object);
  subdiv_sample.setSelfBounds(bounding_box_);
  abc_subdiv_schema_.set(subdiv_sample);

  write_arb_geo_params(mesh);
}

template<typename Schema>
void ABCGenericMeshWriter::write_face_sets(Object *object, Mesh *mesh, Schema &schema)
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

void ABCGenericMeshWriter::write_arb_geo_params(Mesh *mesh)
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
  write_custom_data(arb_geom_params, m_custom_data_config, *mesh, CD_PROP_BYTE_COLOR);
}

bool ABCGenericMeshWriter::get_velocities(Mesh *mesh, std::vector<Imath::V3f> &vels)
{
  /* Export velocity attribute output by fluid sim, sequence cache modifier
   * and geometry nodes. */
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan attr = *attributes.lookup<float3>("velocity", bke::AttrDomain::Point);
  if (attr.is_empty()) {
    return false;
  }

  const int totverts = mesh->verts_num;

  vels.clear();
  vels.resize(totverts);

  for (int i = 0; i < totverts; i++) {
    copy_yup_from_zup(vels[i].getValue(), attr[i]);
  }

  return true;
}

void ABCGenericMeshWriter::get_geo_groups(Object *object,
                                          Mesh *mesh,
                                          std::map<std::string, std::vector<int32_t>> &geo_groups)
{
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Face, 0);

  for (const int i : material_indices.index_range()) {
    short mnr = material_indices[i];

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

    for (int i = 0, e = mesh->totface_legacy; i < e; i++) {
      faceArray.push_back(i);
    }

    geo_groups[name] = faceArray;
  }
}

/* NOTE: Alembic's polygon winding order is clockwise, to match with Renderman. */

static void get_vertices(Mesh *mesh, std::vector<Imath::V3f> &points)
{
  points.clear();
  points.resize(mesh->verts_num);

  const Span<float3> positions = mesh->vert_positions();
  for (int i = 0, e = mesh->verts_num; i < e; i++) {
    copy_yup_from_zup(points[i].getValue(), positions[i]);
  }
}

static void get_topology(Mesh *mesh,
                         std::vector<int32_t> &face_verts,
                         std::vector<int32_t> &loop_counts)
{
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  face_verts.clear();
  loop_counts.clear();
  face_verts.reserve(corner_verts.size());
  loop_counts.reserve(faces.size());

  /* NOTE: data needs to be written in the reverse order. */
  for (const int i : faces.index_range()) {
    const IndexRange face = faces[i];
    loop_counts.push_back(face.size());

    int corner = face.start() + (face.size() - 1);
    for (int j = 0; j < face.size(); j++, corner--) {
      face_verts.push_back(corner_verts[corner]);
    }
  }
}

static void get_edge_creases(Mesh *mesh,
                             std::vector<int32_t> &indices,
                             std::vector<int32_t> &lengths,
                             std::vector<float> &sharpnesses)
{
  indices.clear();
  lengths.clear();
  sharpnesses.clear();

  const bke::AttributeAccessor attributes = mesh->attributes();
  const bke::AttributeReader attribute = attributes.lookup<float>("crease_edge",
                                                                  bke::AttrDomain::Edge);
  if (!attribute) {
    return;
  }
  const VArraySpan creases(*attribute);
  const Span<int2> edges = mesh->edges();
  for (const int i : edges.index_range()) {
    const float crease = std::clamp(creases[i], 0.0f, 1.0f);

    if (crease != 0.0f) {
      indices.push_back(edges[i][0]);
      indices.push_back(edges[i][1]);
      sharpnesses.push_back(bke::subdiv::crease_to_sharpness(crease));
    }
  }

  lengths.resize(sharpnesses.size(), 2);
}

static void get_vert_creases(Mesh *mesh,
                             std::vector<int32_t> &indices,
                             std::vector<float> &sharpnesses)
{
  indices.clear();
  sharpnesses.clear();

  const bke::AttributeAccessor attributes = mesh->attributes();
  const bke::AttributeReader attribute = attributes.lookup<float>("crease_vert",
                                                                  bke::AttrDomain::Point);
  if (!attribute) {
    return;
  }
  const VArraySpan creases(*attribute);
  for (const int i : creases.index_range()) {
    const float crease = std::clamp(creases[i], 0.0f, 1.0f);

    if (crease != 0.0f) {
      indices.push_back(i);
      sharpnesses.push_back(bke::subdiv::crease_to_sharpness(crease));
    }
  }
}

static void get_loop_normals(const Mesh *mesh, std::vector<Imath::V3f> &normals)
{
  normals.clear();

  switch (mesh->normals_domain()) {
    case blender::bke::MeshNormalDomain::Point: {
      /* If all faces are smooth shaded, and there are no custom normals, we don't need to
       * export normals at all. This is also done by other software, see #71246. */
      break;
    }
    case blender::bke::MeshNormalDomain::Face: {
      normals.resize(mesh->corners_num);
      MutableSpan dst_normals(reinterpret_cast<float3 *>(normals.data()), normals.size());

      const OffsetIndices faces = mesh->faces();
      const Span<float3> face_normals = mesh->face_normals();
      threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
        for (const int i : range) {
          float3 y_up;
          copy_yup_from_zup(y_up, face_normals[i]);
          dst_normals.slice(faces[i]).fill(y_up);
        }
      });
      break;
    }
    case blender::bke::MeshNormalDomain::Corner: {
      normals.resize(mesh->corners_num);
      MutableSpan dst_normals(reinterpret_cast<float3 *>(normals.data()), normals.size());

      /* NOTE: data needs to be written in the reverse order. */
      const OffsetIndices faces = mesh->faces();
      const Span<float3> corner_normals = mesh->corner_normals();
      threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
        for (const int i : range) {
          const IndexRange face = faces[i];
          for (const int i : face.index_range()) {
            copy_yup_from_zup(dst_normals[face.last(i)], corner_normals[face[i]]);
          }
        }
      });
      break;
    }
  }
}

ABCMeshWriter::ABCMeshWriter(const ABCWriterConstructorArgs &args) : ABCGenericMeshWriter(args) {}

Mesh *ABCMeshWriter::get_export_mesh(Object *object_eval, bool & /*r_needsfree*/)
{
  return BKE_object_get_evaluated_mesh(object_eval);
}

}  // namespace blender::io::alembic
