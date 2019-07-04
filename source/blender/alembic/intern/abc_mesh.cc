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

#include "abc_mesh.h"

#include <algorithm>

#include "abc_transform.h"
#include "abc_util.h"

extern "C" {
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_types.h"

#include "BLI_math_geom.h"
#include "BLI_string.h"

#include "BKE_animsys.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph_query.h"
}

using Alembic::Abc::C4fArraySample;
using Alembic::Abc::FloatArraySample;
using Alembic::Abc::ICompoundProperty;
using Alembic::Abc::Int32ArraySample;
using Alembic::Abc::Int32ArraySamplePtr;
using Alembic::Abc::P3fArraySamplePtr;
using Alembic::Abc::V2fArraySample;
using Alembic::Abc::V3fArraySample;

using Alembic::AbcGeom::IFaceSet;
using Alembic::AbcGeom::IFaceSetSchema;
using Alembic::AbcGeom::IObject;
using Alembic::AbcGeom::IPolyMesh;
using Alembic::AbcGeom::IPolyMeshSchema;
using Alembic::AbcGeom::ISampleSelector;
using Alembic::AbcGeom::ISubD;
using Alembic::AbcGeom::ISubDSchema;
using Alembic::AbcGeom::IV2fGeomParam;

using Alembic::AbcGeom::OArrayProperty;
using Alembic::AbcGeom::OBoolProperty;
using Alembic::AbcGeom::OC3fArrayProperty;
using Alembic::AbcGeom::OC3fGeomParam;
using Alembic::AbcGeom::OC4fGeomParam;
using Alembic::AbcGeom::OCompoundProperty;
using Alembic::AbcGeom::OFaceSet;
using Alembic::AbcGeom::OFaceSetSchema;
using Alembic::AbcGeom::OFloatGeomParam;
using Alembic::AbcGeom::OInt32GeomParam;
using Alembic::AbcGeom::ON3fArrayProperty;
using Alembic::AbcGeom::ON3fGeomParam;
using Alembic::AbcGeom::OPolyMesh;
using Alembic::AbcGeom::OPolyMeshSchema;
using Alembic::AbcGeom::OSubD;
using Alembic::AbcGeom::OSubDSchema;
using Alembic::AbcGeom::OV2fGeomParam;
using Alembic::AbcGeom::OV3fGeomParam;

using Alembic::AbcGeom::IN3fGeomParam;
using Alembic::AbcGeom::kFacevaryingScope;
using Alembic::AbcGeom::kVaryingScope;
using Alembic::AbcGeom::kVertexScope;
using Alembic::AbcGeom::kWrapExisting;
using Alembic::AbcGeom::N3fArraySamplePtr;
using Alembic::AbcGeom::UInt32ArraySample;

/* ************************************************************************** */

/* NOTE: Alembic's polygon winding order is clockwise, to match with Renderman. */

static void get_vertices(struct Mesh *mesh, std::vector<Imath::V3f> &points)
{
  points.clear();
  points.resize(mesh->totvert);

  MVert *verts = mesh->mvert;

  for (int i = 0, e = mesh->totvert; i < e; ++i) {
    copy_yup_from_zup(points[i].getValue(), verts[i].co);
  }
}

static void get_topology(struct Mesh *mesh,
                         std::vector<int32_t> &poly_verts,
                         std::vector<int32_t> &loop_counts,
                         bool &r_smooth_normal)
{
  const int num_poly = mesh->totpoly;
  const int num_loops = mesh->totloop;
  MLoop *mloop = mesh->mloop;
  MPoly *mpoly = mesh->mpoly;

  poly_verts.clear();
  loop_counts.clear();
  poly_verts.reserve(num_loops);
  loop_counts.reserve(num_poly);

  /* NOTE: data needs to be written in the reverse order. */
  for (int i = 0; i < num_poly; ++i) {
    MPoly &poly = mpoly[i];
    loop_counts.push_back(poly.totloop);

    r_smooth_normal |= ((poly.flag & ME_SMOOTH) != 0);

    MLoop *loop = mloop + poly.loopstart + (poly.totloop - 1);

    for (int j = 0; j < poly.totloop; ++j, --loop) {
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

  for (int i = 0, e = mesh->totedge; i < e; ++i) {
    const float sharpness = static_cast<float>(edge[i].crease) * factor;

    if (sharpness != 0.0f) {
      indices.push_back(edge[i].v1);
      indices.push_back(edge[i].v2);
      sharpnesses.push_back(sharpness);
    }
  }

  lengths.resize(sharpnesses.size(), 2);
}

static void get_vertex_normals(struct Mesh *mesh, std::vector<Imath::V3f> &normals)
{
  normals.clear();
  normals.resize(mesh->totvert);

  MVert *verts = mesh->mvert;
  float no[3];

  for (int i = 0, e = mesh->totvert; i < e; ++i) {
    normal_short_to_float_v3(no, verts[i].no);
    copy_yup_from_zup(normals[i].getValue(), no);
  }
}

static void get_loop_normals(struct Mesh *mesh, std::vector<Imath::V3f> &normals)
{
  MPoly *mp = mesh->mpoly;

  MLoop *mloop = mesh->mloop;
  MLoop *ml = mloop;

  MVert *verts = mesh->mvert;

  const float(*lnors)[3] = static_cast<float(*)[3]>(CustomData_get_layer(&mesh->ldata, CD_NORMAL));

  normals.clear();
  normals.resize(mesh->totloop);

  unsigned loop_index = 0;

  /* NOTE: data needs to be written in the reverse order. */

  if (lnors) {
    for (int i = 0, e = mesh->totpoly; i < e; ++i, ++mp) {
      ml = mloop + mp->loopstart + (mp->totloop - 1);

      for (int j = 0; j < mp->totloop; --ml, ++j, ++loop_index) {
        const int index = ml->v;
        copy_yup_from_zup(normals[loop_index].getValue(), lnors[index]);
      }
    }
  }
  else {
    float no[3];

    for (int i = 0, e = mesh->totpoly; i < e; ++i, ++mp) {
      ml = mloop + mp->loopstart + (mp->totloop - 1);

      /* Flat shaded, use common normal for all verts. */
      if ((mp->flag & ME_SMOOTH) == 0) {
        BKE_mesh_calc_poly_normal(mp, ml - (mp->totloop - 1), verts, no);

        for (int j = 0; j < mp->totloop; --ml, ++j, ++loop_index) {
          copy_yup_from_zup(normals[loop_index].getValue(), no);
        }
      }
      else {
        /* Smooth shaded, use individual vert normals. */
        for (int j = 0; j < mp->totloop; --ml, ++j, ++loop_index) {
          normal_short_to_float_v3(no, verts[ml->v].no);
          copy_yup_from_zup(normals[loop_index].getValue(), no);
        }
      }
    }
  }
}

/* *************** Modifiers *************** */

/* check if the mesh is a subsurf, ignoring disabled modifiers and
 * displace if it's after subsurf. */
static ModifierData *get_subsurf_modifier(Scene *scene, Object *ob)
{
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.last);

  for (; md; md = md->prev) {
    if (!modifier_isEnabled(scene, md, eModifierMode_Render)) {
      continue;
    }

    if (md->type == eModifierType_Subsurf) {
      SubsurfModifierData *smd = reinterpret_cast<SubsurfModifierData *>(md);

      if (smd->subdivType == ME_CC_SUBSURF) {
        return md;
      }
    }

    /* mesh is not a subsurf. break */
    if ((md->type != eModifierType_Displace) && (md->type != eModifierType_ParticleSystem)) {
      return NULL;
    }
  }

  return NULL;
}

static ModifierData *get_liquid_sim_modifier(Scene *scene, Object *ob)
{
  ModifierData *md = modifiers_findByType(ob, eModifierType_Fluidsim);

  if (md && (modifier_isEnabled(scene, md, eModifierMode_Render))) {
    FluidsimModifierData *fsmd = reinterpret_cast<FluidsimModifierData *>(md);

    if (fsmd->fss && fsmd->fss->type == OB_FLUIDSIM_DOMAIN) {
      return md;
    }
  }

  return NULL;
}

/* ************************************************************************** */

AbcGenericMeshWriter::AbcGenericMeshWriter(Object *ob,
                                           AbcTransformWriter *parent,
                                           uint32_t time_sampling,
                                           ExportSettings &settings)
    : AbcObjectWriter(ob, time_sampling, settings, parent)
{
  m_is_animated = isAnimated();
  m_subsurf_mod = NULL;
  m_is_subd = false;

  /* If the object is static, use the default static time sampling. */
  if (!m_is_animated) {
    time_sampling = 0;
  }

  if (!m_settings.apply_subdiv) {
    m_subsurf_mod = get_subsurf_modifier(m_settings.scene, m_object);
    m_is_subd = (m_subsurf_mod != NULL);
  }

  m_is_liquid = (get_liquid_sim_modifier(m_settings.scene, m_object) != NULL);

  while (parent->alembicXform().getChildHeader(m_name)) {
    m_name.append("_");
  }

  if (m_settings.use_subdiv_schema && m_is_subd) {
    OSubD subd(parent->alembicXform(), m_name, m_time_sampling);
    m_subdiv_schema = subd.getSchema();
  }
  else {
    OPolyMesh mesh(parent->alembicXform(), m_name, m_time_sampling);
    m_mesh_schema = mesh.getSchema();

    OCompoundProperty typeContainer = m_mesh_schema.getUserProperties();
    OBoolProperty type(typeContainer, "meshtype");
    type.set(m_is_subd);
  }
}

AbcGenericMeshWriter::~AbcGenericMeshWriter()
{
  if (m_subsurf_mod) {
    m_subsurf_mod->mode &= ~eModifierMode_DisableTemporary;
  }
}

bool AbcGenericMeshWriter::isAnimated() const
{
  if (m_object->data != NULL) {
    AnimData *adt = BKE_animdata_from_id(static_cast<ID *>(m_object->data));
    /* TODO(Sybren): make this check more strict, as the AnimationData may
     * actually be empty (no fcurves, drivers, etc.) and thus effectively
     * have no animation at all. */
    if (adt != NULL) {
      return true;
    }
  }
  if (BKE_key_from_object(m_object) != NULL) {
    return true;
  }

  /* Test modifiers. */
  ModifierData *md = static_cast<ModifierData *>(m_object->modifiers.first);
  while (md) {

    if (md->type != eModifierType_Subsurf) {
      return true;
    }

    md = md->next;
  }

  return false;
}

void AbcGenericMeshWriter::setIsAnimated(bool is_animated)
{
  m_is_animated = is_animated;
}

void AbcGenericMeshWriter::do_write()
{
  /* We have already stored a sample for this object. */
  if (!m_first_frame && !m_is_animated) {
    return;
  }

  bool needsfree;
  struct Mesh *mesh = getFinalMesh(needsfree);

  try {
    if (m_settings.use_subdiv_schema && m_subdiv_schema.valid()) {
      writeSubD(mesh);
    }
    else {
      writeMesh(mesh);
    }

    if (needsfree) {
      freeEvaluatedMesh(mesh);
    }
  }
  catch (...) {
    if (needsfree) {
      freeEvaluatedMesh(mesh);
    }
    throw;
  }
}

void AbcGenericMeshWriter::freeEvaluatedMesh(struct Mesh *mesh)
{
  BKE_id_free(NULL, mesh);
}

void AbcGenericMeshWriter::writeMesh(struct Mesh *mesh)
{
  std::vector<Imath::V3f> points, normals;
  std::vector<int32_t> poly_verts, loop_counts;
  std::vector<Imath::V3f> velocities;

  bool smooth_normal = false;

  get_vertices(mesh, points);
  get_topology(mesh, poly_verts, loop_counts, smooth_normal);

  if (m_first_frame && m_settings.export_face_sets) {
    writeFaceSets(mesh, m_mesh_schema);
  }

  m_mesh_sample = OPolyMeshSchema::Sample(
      V3fArraySample(points), Int32ArraySample(poly_verts), Int32ArraySample(loop_counts));

  UVSample sample;
  if (m_first_frame && m_settings.export_uvs) {
    const char *name = get_uv_sample(sample, m_custom_data_config, &mesh->ldata);

    if (!sample.indices.empty() && !sample.uvs.empty()) {
      OV2fGeomParam::Sample uv_sample;
      uv_sample.setVals(V2fArraySample(sample.uvs));
      uv_sample.setIndices(UInt32ArraySample(sample.indices));
      uv_sample.setScope(kFacevaryingScope);

      m_mesh_schema.setUVSourceName(name);
      m_mesh_sample.setUVs(uv_sample);
    }

    write_custom_data(
        m_mesh_schema.getArbGeomParams(), m_custom_data_config, &mesh->ldata, CD_MLOOPUV);
  }

  if (m_settings.export_normals) {
    if (smooth_normal) {
      get_loop_normals(mesh, normals);
    }
    else {
      get_vertex_normals(mesh, normals);
    }

    ON3fGeomParam::Sample normals_sample;
    if (!normals.empty()) {
      normals_sample.setScope((smooth_normal) ? kFacevaryingScope : kVertexScope);
      normals_sample.setVals(V3fArraySample(normals));
    }

    m_mesh_sample.setNormals(normals_sample);
  }

  if (m_is_liquid) {
    getVelocities(mesh, velocities);
    m_mesh_sample.setVelocities(V3fArraySample(velocities));
  }

  m_mesh_sample.setSelfBounds(bounds());

  m_mesh_schema.set(m_mesh_sample);

  writeArbGeoParams(mesh);
}

void AbcGenericMeshWriter::writeSubD(struct Mesh *mesh)
{
  std::vector<float> crease_sharpness;
  std::vector<Imath::V3f> points;
  std::vector<int32_t> poly_verts, loop_counts;
  std::vector<int32_t> crease_indices, crease_lengths;

  bool smooth_normal = false;

  get_vertices(mesh, points);
  get_topology(mesh, poly_verts, loop_counts, smooth_normal);
  get_creases(mesh, crease_indices, crease_lengths, crease_sharpness);

  if (m_first_frame && m_settings.export_face_sets) {
    writeFaceSets(mesh, m_subdiv_schema);
  }

  m_subdiv_sample = OSubDSchema::Sample(
      V3fArraySample(points), Int32ArraySample(poly_verts), Int32ArraySample(loop_counts));

  UVSample sample;
  if (m_first_frame && m_settings.export_uvs) {
    const char *name = get_uv_sample(sample, m_custom_data_config, &mesh->ldata);

    if (!sample.indices.empty() && !sample.uvs.empty()) {
      OV2fGeomParam::Sample uv_sample;
      uv_sample.setVals(V2fArraySample(sample.uvs));
      uv_sample.setIndices(UInt32ArraySample(sample.indices));
      uv_sample.setScope(kFacevaryingScope);

      m_subdiv_schema.setUVSourceName(name);
      m_subdiv_sample.setUVs(uv_sample);
    }

    write_custom_data(
        m_subdiv_schema.getArbGeomParams(), m_custom_data_config, &mesh->ldata, CD_MLOOPUV);
  }

  if (!crease_indices.empty()) {
    m_subdiv_sample.setCreaseIndices(Int32ArraySample(crease_indices));
    m_subdiv_sample.setCreaseLengths(Int32ArraySample(crease_lengths));
    m_subdiv_sample.setCreaseSharpnesses(FloatArraySample(crease_sharpness));
  }

  m_subdiv_sample.setSelfBounds(bounds());
  m_subdiv_schema.set(m_subdiv_sample);

  writeArbGeoParams(mesh);
}

template<typename Schema> void AbcGenericMeshWriter::writeFaceSets(struct Mesh *me, Schema &schema)
{
  std::map<std::string, std::vector<int32_t>> geo_groups;
  getGeoGroups(me, geo_groups);

  std::map<std::string, std::vector<int32_t>>::iterator it;
  for (it = geo_groups.begin(); it != geo_groups.end(); ++it) {
    OFaceSet face_set = schema.createFaceSet(it->first);
    OFaceSetSchema::Sample samp;
    samp.setFaces(Int32ArraySample(it->second));
    face_set.getSchema().set(samp);
  }
}

Mesh *AbcGenericMeshWriter::getFinalMesh(bool &r_needsfree)
{
  /* We don't want subdivided mesh data */
  if (m_subsurf_mod) {
    m_subsurf_mod->mode |= eModifierMode_DisableTemporary;
  }

  r_needsfree = false;

  Scene *scene = DEG_get_evaluated_scene(m_settings.depsgraph);
  Object *ob_eval = DEG_get_evaluated_object(m_settings.depsgraph, m_object);
  struct Mesh *mesh = getEvaluatedMesh(scene, ob_eval, r_needsfree);

  if (m_subsurf_mod) {
    m_subsurf_mod->mode &= ~eModifierMode_DisableTemporary;
  }

  if (m_settings.triangulate) {
    const bool tag_only = false;
    const int quad_method = m_settings.quad_method;
    const int ngon_method = m_settings.ngon_method;

    struct BMeshCreateParams bmcp = {false};
    struct BMeshFromMeshParams bmfmp = {true, false, false, 0};
    BMesh *bm = BKE_mesh_to_bmesh_ex(mesh, &bmcp, &bmfmp);

    BM_mesh_triangulate(bm, quad_method, ngon_method, 4, tag_only, NULL, NULL, NULL);

    Mesh *result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL);
    BM_mesh_free(bm);

    if (r_needsfree) {
      BKE_id_free(NULL, mesh);
    }

    mesh = result;
    r_needsfree = true;
  }

  m_custom_data_config.pack_uvs = m_settings.pack_uv;
  m_custom_data_config.mpoly = mesh->mpoly;
  m_custom_data_config.mloop = mesh->mloop;
  m_custom_data_config.totpoly = mesh->totpoly;
  m_custom_data_config.totloop = mesh->totloop;
  m_custom_data_config.totvert = mesh->totvert;

  return mesh;
}

void AbcGenericMeshWriter::writeArbGeoParams(struct Mesh *me)
{
  if (m_is_liquid) {
    /* We don't need anything more for liquid meshes. */
    return;
  }

  if (m_first_frame && m_settings.export_vcols) {
    if (m_subdiv_schema.valid()) {
      write_custom_data(
          m_subdiv_schema.getArbGeomParams(), m_custom_data_config, &me->ldata, CD_MLOOPCOL);
    }
    else {
      write_custom_data(
          m_mesh_schema.getArbGeomParams(), m_custom_data_config, &me->ldata, CD_MLOOPCOL);
    }
  }
}

void AbcGenericMeshWriter::getVelocities(struct Mesh *mesh, std::vector<Imath::V3f> &vels)
{
  const int totverts = mesh->totvert;

  vels.clear();
  vels.resize(totverts);

  ModifierData *md = get_liquid_sim_modifier(m_settings.scene, m_object);
  FluidsimModifierData *fmd = reinterpret_cast<FluidsimModifierData *>(md);
  FluidsimSettings *fss = fmd->fss;

  if (fss->meshVelocities) {
    float *mesh_vels = reinterpret_cast<float *>(fss->meshVelocities);

    for (int i = 0; i < totverts; ++i) {
      copy_yup_from_zup(vels[i].getValue(), mesh_vels);
      mesh_vels += 3;
    }
  }
  else {
    std::fill(vels.begin(), vels.end(), Imath::V3f(0.0f));
  }
}

void AbcGenericMeshWriter::getGeoGroups(struct Mesh *mesh,
                                        std::map<std::string, std::vector<int32_t>> &geo_groups)
{
  const int num_poly = mesh->totpoly;
  MPoly *polygons = mesh->mpoly;

  for (int i = 0; i < num_poly; ++i) {
    MPoly &current_poly = polygons[i];
    short mnr = current_poly.mat_nr;

    Material *mat = give_current_material(m_object, mnr + 1);

    if (!mat) {
      continue;
    }

    std::string name = get_id_name(&mat->id);

    if (geo_groups.find(name) == geo_groups.end()) {
      std::vector<int32_t> faceArray;
      geo_groups[name] = faceArray;
    }

    geo_groups[name].push_back(i);
  }

  if (geo_groups.size() == 0) {
    Material *mat = give_current_material(m_object, 1);

    std::string name = (mat) ? get_id_name(&mat->id) : "default";

    std::vector<int32_t> faceArray;

    for (int i = 0, e = mesh->totface; i < e; ++i) {
      faceArray.push_back(i);
    }

    geo_groups[name] = faceArray;
  }
}

AbcMeshWriter::AbcMeshWriter(Object *ob,
                             AbcTransformWriter *parent,
                             uint32_t time_sampling,
                             ExportSettings &settings)
    : AbcGenericMeshWriter(ob, parent, time_sampling, settings)
{
}

AbcMeshWriter::~AbcMeshWriter()
{
}

Mesh *AbcMeshWriter::getEvaluatedMesh(Scene *scene_eval,
                                      Object *ob_eval,
                                      bool &UNUSED(r_needsfree))
{
  return mesh_get_eval_final(m_settings.depsgraph, scene_eval, ob_eval, &CD_MASK_MESH);
}

/* ************************************************************************** */

/* Some helpers for mesh generation */
namespace utils {

static void build_mat_map(const Main *bmain, std::map<std::string, Material *> &mat_map)
{
  Material *material = static_cast<Material *>(bmain->materials.first);

  for (; material; material = static_cast<Material *>(material->id.next)) {
    mat_map[material->id.name + 2] = material;
  }
}

static void assign_materials(Main *bmain,
                             Object *ob,
                             const std::map<std::string, int> &mat_index_map)
{
  bool can_assign = true;
  std::map<std::string, int>::const_iterator it = mat_index_map.begin();

  int matcount = 0;
  for (; it != mat_index_map.end(); ++it, ++matcount) {
    if (!BKE_object_material_slot_add(bmain, ob)) {
      can_assign = false;
      break;
    }
  }

  /* TODO(kevin): use global map? */
  std::map<std::string, Material *> mat_map;
  build_mat_map(bmain, mat_map);

  std::map<std::string, Material *>::iterator mat_iter;

  if (can_assign) {
    it = mat_index_map.begin();

    for (; it != mat_index_map.end(); ++it) {
      std::string mat_name = it->first;
      mat_iter = mat_map.find(mat_name.c_str());

      Material *assigned_mat;

      if (mat_iter == mat_map.end()) {
        assigned_mat = BKE_material_add(bmain, mat_name.c_str());
        mat_map[mat_name] = assigned_mat;
      }
      else {
        assigned_mat = mat_iter->second;
      }

      assign_material(bmain, ob, assigned_mat, it->second, BKE_MAT_ASSIGN_OBDATA);
    }
  }
}

} /* namespace utils */

/* ************************************************************************** */

using Alembic::AbcGeom::UInt32ArraySamplePtr;
using Alembic::AbcGeom::V2fArraySamplePtr;

struct AbcMeshData {
  Int32ArraySamplePtr face_indices;
  Int32ArraySamplePtr face_counts;

  P3fArraySamplePtr positions;
  P3fArraySamplePtr ceil_positions;

  N3fArraySamplePtr vertex_normals;
  N3fArraySamplePtr face_normals;

  V2fArraySamplePtr uvs;
  UInt32ArraySamplePtr uvs_indices;
};

static void read_mverts_interp(MVert *mverts,
                               const P3fArraySamplePtr &positions,
                               const P3fArraySamplePtr &ceil_positions,
                               const float weight)
{
  float tmp[3];
  for (int i = 0; i < positions->size(); ++i) {
    MVert &mvert = mverts[i];
    const Imath::V3f &floor_pos = (*positions)[i];
    const Imath::V3f &ceil_pos = (*ceil_positions)[i];

    interp_v3_v3v3(tmp, floor_pos.getValue(), ceil_pos.getValue(), weight);
    copy_zup_from_yup(mvert.co, tmp);

    mvert.bweight = 0;
  }
}

static void read_mverts(CDStreamConfig &config, const AbcMeshData &mesh_data)
{
  MVert *mverts = config.mvert;
  const P3fArraySamplePtr &positions = mesh_data.positions;
  const N3fArraySamplePtr &normals = mesh_data.vertex_normals;

  if (config.weight != 0.0f && mesh_data.ceil_positions != NULL &&
      mesh_data.ceil_positions->size() == positions->size()) {
    read_mverts_interp(mverts, positions, mesh_data.ceil_positions, config.weight);
    return;
  }

  read_mverts(mverts, positions, normals);
}

void read_mverts(MVert *mverts,
                 const P3fArraySamplePtr &positions,
                 const N3fArraySamplePtr &normals)
{
  for (int i = 0; i < positions->size(); ++i) {
    MVert &mvert = mverts[i];
    Imath::V3f pos_in = (*positions)[i];

    copy_zup_from_yup(mvert.co, pos_in.getValue());

    mvert.bweight = 0;

    if (normals) {
      Imath::V3f nor_in = (*normals)[i];

      short no[3];
      normal_float_to_short_v3(no, nor_in.getValue());

      copy_zup_from_yup(mvert.no, no);
    }
  }
}

static void read_mpolys(CDStreamConfig &config, const AbcMeshData &mesh_data)
{
  MPoly *mpolys = config.mpoly;
  MLoop *mloops = config.mloop;
  MLoopUV *mloopuvs = config.mloopuv;

  const Int32ArraySamplePtr &face_indices = mesh_data.face_indices;
  const Int32ArraySamplePtr &face_counts = mesh_data.face_counts;
  const V2fArraySamplePtr &uvs = mesh_data.uvs;
  const size_t uvs_size = uvs == nullptr ? 0 : uvs->size();

  const UInt32ArraySamplePtr &uvs_indices = mesh_data.uvs_indices;
  const N3fArraySamplePtr &normals = mesh_data.face_normals;

  const bool do_uvs = (mloopuvs && uvs && uvs_indices) &&
                      (uvs_indices->size() == face_indices->size());
  unsigned int loop_index = 0;
  unsigned int rev_loop_index = 0;
  unsigned int uv_index = 0;

  for (int i = 0; i < face_counts->size(); ++i) {
    const int face_size = (*face_counts)[i];

    MPoly &poly = mpolys[i];
    poly.loopstart = loop_index;
    poly.totloop = face_size;

    if (normals != NULL) {
      poly.flag |= ME_SMOOTH;
    }

    /* NOTE: Alembic data is stored in the reverse order. */
    rev_loop_index = loop_index + (face_size - 1);

    for (int f = 0; f < face_size; ++f, ++loop_index, --rev_loop_index) {
      MLoop &loop = mloops[rev_loop_index];
      loop.v = (*face_indices)[loop_index];

      if (do_uvs) {
        MLoopUV &loopuv = mloopuvs[rev_loop_index];

        uv_index = (*uvs_indices)[loop_index];

        /* Some Alembic files are broken (or at least export UVs in a way we don't expect). */
        if (uv_index >= uvs_size) {
          continue;
        }

        loopuv.uv[0] = (*uvs)[uv_index][0];
        loopuv.uv[1] = (*uvs)[uv_index][1];
      }
    }
  }
}

ABC_INLINE void read_uvs_params(CDStreamConfig &config,
                                AbcMeshData &abc_data,
                                const IV2fGeomParam &uv,
                                const ISampleSelector &selector)
{
  if (!uv.valid()) {
    return;
  }

  IV2fGeomParam::Sample uvsamp;
  uv.getIndexed(uvsamp, selector);

  abc_data.uvs = uvsamp.getVals();
  abc_data.uvs_indices = uvsamp.getIndices();

  if (abc_data.uvs_indices->size() == config.totloop) {
    std::string name = Alembic::Abc::GetSourceName(uv.getMetaData());

    /* According to the convention, primary UVs should have had their name
     * set using Alembic::Abc::SetSourceName, but you can't expect everyone
     * to follow it! :) */
    if (name.empty()) {
      name = uv.getName();
    }

    void *cd_ptr = config.add_customdata_cb(config.user_data, name.c_str(), CD_MLOOPUV);
    config.mloopuv = static_cast<MLoopUV *>(cd_ptr);
  }
}

/* TODO(kevin): normals from Alembic files are not read in anymore, this is due
 * to the fact that there are many issues that are not so easy to solve, mainly
 * regarding the way normals are handled in Blender (MPoly.flag vs loop normals).
 */
ABC_INLINE void read_normals_params(AbcMeshData &abc_data,
                                    const IN3fGeomParam &normals,
                                    const ISampleSelector &selector)
{
  if (!normals.valid()) {
    return;
  }

  IN3fGeomParam::Sample normsamp = normals.getExpandedValue(selector);

  Alembic::AbcGeom::GeometryScope scope = normals.getScope();
  switch (scope) {
    case Alembic::AbcGeom::kFacevaryingScope:
      abc_data.face_normals = normsamp.getVals();
      break;
    case Alembic::AbcGeom::kVertexScope:
    case Alembic::AbcGeom::kVaryingScope:
      abc_data.vertex_normals = N3fArraySamplePtr();
      break;
    case Alembic::AbcGeom::kConstantScope:
    case Alembic::AbcGeom::kUniformScope:
    case Alembic::AbcGeom::kUnknownScope:
      break;
  }
}

static bool check_smooth_poly_flag(Mesh *mesh)
{
  MPoly *mpolys = mesh->mpoly;

  for (int i = 0, e = mesh->totpoly; i < e; ++i) {
    MPoly &poly = mpolys[i];

    if ((poly.flag & ME_SMOOTH) != 0) {
      return true;
    }
  }

  return false;
}

static void set_smooth_poly_flag(Mesh *mesh)
{
  MPoly *mpolys = mesh->mpoly;

  for (int i = 0, e = mesh->totpoly; i < e; ++i) {
    MPoly &poly = mpolys[i];
    poly.flag |= ME_SMOOTH;
  }
}

static void *add_customdata_cb(void *user_data, const char *name, int data_type)
{
  Mesh *mesh = static_cast<Mesh *>(user_data);
  CustomDataType cd_data_type = static_cast<CustomDataType>(data_type);
  void *cd_ptr;
  CustomData *loopdata;
  int numloops;

  /* unsupported custom data type -- don't do anything. */
  if (!ELEM(cd_data_type, CD_MLOOPUV, CD_MLOOPCOL)) {
    return NULL;
  }

  loopdata = &mesh->ldata;
  cd_ptr = CustomData_get_layer_named(loopdata, cd_data_type, name);
  if (cd_ptr != NULL) {
    /* layer already exists, so just return it. */
    return cd_ptr;
  }

  /* Create a new layer. */
  numloops = mesh->totloop;
  cd_ptr = CustomData_add_layer_named(loopdata, cd_data_type, CD_DEFAULT, NULL, numloops, name);
  return cd_ptr;
}

static void get_weight_and_index(CDStreamConfig &config,
                                 Alembic::AbcCoreAbstract::TimeSamplingPtr time_sampling,
                                 size_t samples_number)
{
  Alembic::AbcGeom::index_t i0, i1;

  config.weight = get_weight_and_index(config.time, time_sampling, samples_number, i0, i1);

  config.index = i0;
  config.ceil_index = i1;
}

static void read_mesh_sample(const std::string &iobject_full_name,
                             ImportSettings *settings,
                             const IPolyMeshSchema &schema,
                             const ISampleSelector &selector,
                             CDStreamConfig &config,
                             bool &r_do_normals)
{
  const IPolyMeshSchema::Sample sample = schema.getValue(selector);

  AbcMeshData abc_mesh_data;
  abc_mesh_data.face_counts = sample.getFaceCounts();
  abc_mesh_data.face_indices = sample.getFaceIndices();
  abc_mesh_data.positions = sample.getPositions();

  read_normals_params(abc_mesh_data, schema.getNormalsParam(), selector);

  r_do_normals = (abc_mesh_data.face_normals != NULL);

  get_weight_and_index(config, schema.getTimeSampling(), schema.getNumSamples());

  if (config.weight != 0.0f) {
    Alembic::AbcGeom::IPolyMeshSchema::Sample ceil_sample;
    schema.get(ceil_sample, Alembic::Abc::ISampleSelector(config.ceil_index));
    abc_mesh_data.ceil_positions = ceil_sample.getPositions();
  }

  if ((settings->read_flag & MOD_MESHSEQ_READ_UV) != 0) {
    read_uvs_params(config, abc_mesh_data, schema.getUVsParam(), selector);
  }

  if ((settings->read_flag & MOD_MESHSEQ_READ_VERT) != 0) {
    read_mverts(config, abc_mesh_data);
  }

  if ((settings->read_flag & MOD_MESHSEQ_READ_POLY) != 0) {
    read_mpolys(config, abc_mesh_data);
  }

  if ((settings->read_flag & (MOD_MESHSEQ_READ_UV | MOD_MESHSEQ_READ_COLOR)) != 0) {
    read_custom_data(iobject_full_name, schema.getArbGeomParams(), config, selector);
  }
}

CDStreamConfig get_config(Mesh *mesh)
{
  CDStreamConfig config;

  BLI_assert(mesh->mvert || mesh->totvert == 0);

  config.user_data = mesh;
  config.mvert = mesh->mvert;
  config.mloop = mesh->mloop;
  config.mpoly = mesh->mpoly;
  config.totloop = mesh->totloop;
  config.totpoly = mesh->totpoly;
  config.loopdata = &mesh->ldata;
  config.add_customdata_cb = add_customdata_cb;

  return config;
}

/* ************************************************************************** */

AbcMeshReader::AbcMeshReader(const IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
  m_settings->read_flag |= MOD_MESHSEQ_READ_ALL;

  IPolyMesh ipoly_mesh(m_iobject, kWrapExisting);
  m_schema = ipoly_mesh.getSchema();

  get_min_max_time(m_iobject, m_schema, m_min_time, m_max_time);
}

bool AbcMeshReader::valid() const
{
  return m_schema.valid();
}

void AbcMeshReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  Mesh *mesh = BKE_mesh_add(bmain, m_data_name.c_str());

  m_object = BKE_object_add_only_object(bmain, OB_MESH, m_object_name.c_str());
  m_object->data = mesh;

  Mesh *read_mesh = this->read_mesh(mesh, sample_sel, MOD_MESHSEQ_READ_ALL, NULL);
  if (read_mesh != mesh) {
    BKE_mesh_nomain_to_mesh(read_mesh, mesh, m_object, &CD_MASK_MESH, true);
  }

  if (m_settings->validate_meshes) {
    BKE_mesh_validate(mesh, false, false);
  }

  readFaceSetsSample(bmain, mesh, sample_sel);

  if (has_animations(m_schema, m_settings)) {
    addCacheModifier();
  }
}

bool AbcMeshReader::accepts_object_type(
    const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
    const Object *const ob,
    const char **err_str) const
{
  if (!Alembic::AbcGeom::IPolyMesh::matches(alembic_header)) {
    *err_str =
        "Object type mismatch, Alembic object path pointed to PolyMesh when importing, but not "
        "any more.";
    return false;
  }

  if (ob->type != OB_MESH) {
    *err_str = "Object type mismatch, Alembic object path points to PolyMesh.";
    return false;
  }

  return true;
}

Mesh *AbcMeshReader::read_mesh(Mesh *existing_mesh,
                               const ISampleSelector &sample_sel,
                               int read_flag,
                               const char **err_str)
{
  IPolyMeshSchema::Sample sample;
  try {
    sample = m_schema.getValue(sample_sel);
  }
  catch (Alembic::Util::Exception &ex) {
    if (err_str != nullptr) {
      *err_str = "Error reading mesh sample; more detail on the console";
    }
    printf("Alembic: error reading mesh sample for '%s/%s' at time %f: %s\n",
           m_iobject.getFullName().c_str(),
           m_schema.getName().c_str(),
           sample_sel.getRequestedTime(),
           ex.what());
    return existing_mesh;
  }

  const P3fArraySamplePtr &positions = sample.getPositions();
  const Alembic::Abc::Int32ArraySamplePtr &face_indices = sample.getFaceIndices();
  const Alembic::Abc::Int32ArraySamplePtr &face_counts = sample.getFaceCounts();

  Mesh *new_mesh = NULL;

  /* Only read point data when streaming meshes, unless we need to create new ones. */
  ImportSettings settings;
  settings.read_flag |= read_flag;

  bool topology_changed = positions->size() != existing_mesh->totvert ||
                          face_counts->size() != existing_mesh->totpoly ||
                          face_indices->size() != existing_mesh->totloop;
  if (topology_changed) {
    new_mesh = BKE_mesh_new_nomain_from_template(
        existing_mesh, positions->size(), 0, 0, face_indices->size(), face_counts->size());

    settings.read_flag |= MOD_MESHSEQ_READ_ALL;
  }
  else {
    /* If the face count changed (e.g. by triangulation), only read points.
     * This prevents crash from T49813.
     * TODO(kevin): perhaps find a better way to do this? */
    if (face_counts->size() != existing_mesh->totpoly ||
        face_indices->size() != existing_mesh->totloop) {
      settings.read_flag = MOD_MESHSEQ_READ_VERT;

      if (err_str) {
        *err_str =
            "Topology has changed, perhaps by triangulating the"
            " mesh. Only vertices will be read!";
      }
    }
  }

  CDStreamConfig config = get_config(new_mesh ? new_mesh : existing_mesh);
  config.time = sample_sel.getRequestedTime();

  bool do_normals = false;
  read_mesh_sample(m_iobject.getFullName(), &settings, m_schema, sample_sel, config, do_normals);

  if (new_mesh) {
    /* Check if we had ME_SMOOTH flag set to restore it. */
    if (!do_normals && check_smooth_poly_flag(existing_mesh)) {
      set_smooth_poly_flag(new_mesh);
    }

    BKE_mesh_calc_normals(new_mesh);
    BKE_mesh_calc_edges(new_mesh, false, false);

    /* Here we assume that the number of materials doesn't change, i.e. that
     * the material slots that were created when the object was loaded from
     * Alembic are still valid now. */
    size_t num_polys = new_mesh->totpoly;
    if (num_polys > 0) {
      std::map<std::string, int> mat_map;
      assign_facesets_to_mpoly(sample_sel, new_mesh->mpoly, num_polys, mat_map);
    }

    return new_mesh;
  }

  if (do_normals) {
    BKE_mesh_calc_normals(existing_mesh);
  }

  return existing_mesh;
}

void AbcMeshReader::assign_facesets_to_mpoly(const ISampleSelector &sample_sel,
                                             MPoly *mpoly,
                                             int totpoly,
                                             std::map<std::string, int> &r_mat_map)
{
  std::vector<std::string> face_sets;
  m_schema.getFaceSetNames(face_sets);

  if (face_sets.empty()) {
    return;
  }

  int current_mat = 0;

  for (int i = 0; i < face_sets.size(); ++i) {
    const std::string &grp_name = face_sets[i];

    if (r_mat_map.find(grp_name) == r_mat_map.end()) {
      r_mat_map[grp_name] = 1 + current_mat++;
    }

    const int assigned_mat = r_mat_map[grp_name];

    const IFaceSet faceset = m_schema.getFaceSet(grp_name);

    if (!faceset.valid()) {
      std::cerr << " Face set " << grp_name << " invalid for " << m_object_name << "\n";
      continue;
    }

    const IFaceSetSchema face_schem = faceset.getSchema();
    const IFaceSetSchema::Sample face_sample = face_schem.getValue(sample_sel);
    const Int32ArraySamplePtr group_faces = face_sample.getFaces();
    const size_t num_group_faces = group_faces->size();

    for (size_t l = 0; l < num_group_faces; l++) {
      size_t pos = (*group_faces)[l];

      if (pos >= totpoly) {
        std::cerr << "Faceset overflow on " << faceset.getName() << '\n';
        break;
      }

      MPoly &poly = mpoly[pos];
      poly.mat_nr = assigned_mat - 1;
    }
  }
}

void AbcMeshReader::readFaceSetsSample(Main *bmain, Mesh *mesh, const ISampleSelector &sample_sel)
{
  std::map<std::string, int> mat_map;
  assign_facesets_to_mpoly(sample_sel, mesh->mpoly, mesh->totpoly, mat_map);
  utils::assign_materials(bmain, m_object, mat_map);
}

/* ************************************************************************** */

ABC_INLINE MEdge *find_edge(MEdge *edges, int totedge, int v1, int v2)
{
  for (int i = 0, e = totedge; i < e; ++i) {
    MEdge &edge = edges[i];

    if (edge.v1 == v1 && edge.v2 == v2) {
      return &edge;
    }
  }

  return NULL;
}

static void read_subd_sample(const std::string &iobject_full_name,
                             ImportSettings *settings,
                             const ISubDSchema &schema,
                             const ISampleSelector &selector,
                             CDStreamConfig &config)
{
  const ISubDSchema::Sample sample = schema.getValue(selector);

  AbcMeshData abc_mesh_data;
  abc_mesh_data.face_counts = sample.getFaceCounts();
  abc_mesh_data.face_indices = sample.getFaceIndices();
  abc_mesh_data.vertex_normals = N3fArraySamplePtr();
  abc_mesh_data.face_normals = N3fArraySamplePtr();
  abc_mesh_data.positions = sample.getPositions();

  get_weight_and_index(config, schema.getTimeSampling(), schema.getNumSamples());

  if (config.weight != 0.0f) {
    Alembic::AbcGeom::ISubDSchema::Sample ceil_sample;
    schema.get(ceil_sample, Alembic::Abc::ISampleSelector(config.ceil_index));
    abc_mesh_data.ceil_positions = ceil_sample.getPositions();
  }

  if ((settings->read_flag & MOD_MESHSEQ_READ_UV) != 0) {
    read_uvs_params(config, abc_mesh_data, schema.getUVsParam(), selector);
  }

  if ((settings->read_flag & MOD_MESHSEQ_READ_VERT) != 0) {
    read_mverts(config, abc_mesh_data);
  }

  if ((settings->read_flag & MOD_MESHSEQ_READ_POLY) != 0) {
    read_mpolys(config, abc_mesh_data);
  }

  if ((settings->read_flag & (MOD_MESHSEQ_READ_UV | MOD_MESHSEQ_READ_COLOR)) != 0) {
    read_custom_data(iobject_full_name, schema.getArbGeomParams(), config, selector);
  }
}

/* ************************************************************************** */

AbcSubDReader::AbcSubDReader(const IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
  m_settings->read_flag |= MOD_MESHSEQ_READ_ALL;

  ISubD isubd_mesh(m_iobject, kWrapExisting);
  m_schema = isubd_mesh.getSchema();

  get_min_max_time(m_iobject, m_schema, m_min_time, m_max_time);
}

bool AbcSubDReader::valid() const
{
  return m_schema.valid();
}

bool AbcSubDReader::accepts_object_type(
    const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
    const Object *const ob,
    const char **err_str) const
{
  if (!Alembic::AbcGeom::ISubD::matches(alembic_header)) {
    *err_str =
        "Object type mismatch, Alembic object path pointed to SubD when importing, but not any "
        "more.";
    return false;
  }

  if (ob->type != OB_MESH) {
    *err_str = "Object type mismatch, Alembic object path points to SubD.";
    return false;
  }

  return true;
}

void AbcSubDReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  Mesh *mesh = BKE_mesh_add(bmain, m_data_name.c_str());

  m_object = BKE_object_add_only_object(bmain, OB_MESH, m_object_name.c_str());
  m_object->data = mesh;

  Mesh *read_mesh = this->read_mesh(mesh, sample_sel, MOD_MESHSEQ_READ_ALL, NULL);
  if (read_mesh != mesh) {
    BKE_mesh_nomain_to_mesh(read_mesh, mesh, m_object, &CD_MASK_MESH, true);
  }

  ISubDSchema::Sample sample;
  try {
    sample = m_schema.getValue(sample_sel);
  }
  catch (Alembic::Util::Exception &ex) {
    printf("Alembic: error reading mesh sample for '%s/%s' at time %f: %s\n",
           m_iobject.getFullName().c_str(),
           m_schema.getName().c_str(),
           sample_sel.getRequestedTime(),
           ex.what());
    return;
  }

  Int32ArraySamplePtr indices = sample.getCreaseIndices();
  Alembic::Abc::FloatArraySamplePtr sharpnesses = sample.getCreaseSharpnesses();

  MEdge *edges = mesh->medge;

  if (indices && sharpnesses) {
    for (int i = 0, s = 0, e = indices->size(); i < e; i += 2, ++s) {
      MEdge *edge = find_edge(edges, mesh->totedge, (*indices)[i], (*indices)[i + 1]);

      if (edge) {
        edge->crease = unit_float_to_uchar_clamp((*sharpnesses)[s]);
      }
    }

    mesh->cd_flag |= ME_CDFLAG_EDGE_CREASE;
  }

  BKE_mesh_calc_normals(mesh);
  BKE_mesh_calc_edges(mesh, false, false);

  if (m_settings->validate_meshes) {
    BKE_mesh_validate(mesh, false, false);
  }

  if (has_animations(m_schema, m_settings)) {
    addCacheModifier();
  }
}

Mesh *AbcSubDReader::read_mesh(Mesh *existing_mesh,
                               const ISampleSelector &sample_sel,
                               int read_flag,
                               const char **err_str)
{
  ISubDSchema::Sample sample;
  try {
    sample = m_schema.getValue(sample_sel);
  }
  catch (Alembic::Util::Exception &ex) {
    if (err_str != nullptr) {
      *err_str = "Error reading mesh sample; more detail on the console";
    }
    printf("Alembic: error reading mesh sample for '%s/%s' at time %f: %s\n",
           m_iobject.getFullName().c_str(),
           m_schema.getName().c_str(),
           sample_sel.getRequestedTime(),
           ex.what());
    return existing_mesh;
  }

  const P3fArraySamplePtr &positions = sample.getPositions();
  const Alembic::Abc::Int32ArraySamplePtr &face_indices = sample.getFaceIndices();
  const Alembic::Abc::Int32ArraySamplePtr &face_counts = sample.getFaceCounts();

  Mesh *new_mesh = NULL;

  ImportSettings settings;
  settings.read_flag |= read_flag;

  if (existing_mesh->totvert != positions->size()) {
    new_mesh = BKE_mesh_new_nomain_from_template(
        existing_mesh, positions->size(), 0, 0, face_indices->size(), face_counts->size());

    settings.read_flag |= MOD_MESHSEQ_READ_ALL;
  }
  else {
    /* If the face count changed (e.g. by triangulation), only read points.
     * This prevents crash from T49813.
     * TODO(kevin): perhaps find a better way to do this? */
    if (face_counts->size() != existing_mesh->totpoly ||
        face_indices->size() != existing_mesh->totpoly) {
      settings.read_flag = MOD_MESHSEQ_READ_VERT;

      if (err_str) {
        *err_str =
            "Topology has changed, perhaps by triangulating the"
            " mesh. Only vertices will be read!";
      }
    }
  }

  /* Only read point data when streaming meshes, unless we need to create new ones. */
  CDStreamConfig config = get_config(new_mesh ? new_mesh : existing_mesh);
  config.time = sample_sel.getRequestedTime();
  read_subd_sample(m_iobject.getFullName(), &settings, m_schema, sample_sel, config);

  if (new_mesh) {
    /* Check if we had ME_SMOOTH flag set to restore it. */
    if (check_smooth_poly_flag(existing_mesh)) {
      set_smooth_poly_flag(new_mesh);
    }

    BKE_mesh_calc_normals(new_mesh);
    BKE_mesh_calc_edges(new_mesh, false, false);

    return new_mesh;
  }

  return existing_mesh;
}
