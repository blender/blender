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
#include "abc_writer_transform.h"
#include "intern/abc_axis_conversion.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_fluidsim_types.h"

#include "BKE_anim_data.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph_query.h"

using Alembic::Abc::FloatArraySample;
using Alembic::Abc::Int32ArraySample;
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
  BLI_assert(lnors != NULL || !"BKE_mesh_calc_normals_split() should have computed CD_NORMAL");

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

/* *************** Modifiers *************** */

/* check if the mesh is a subsurf, ignoring disabled modifiers and
 * displace if it's after subsurf. */
static ModifierData *get_subsurf_modifier(Scene *scene, Object *ob)
{
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.last);

  for (; md; md = md->prev) {
    if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Render)) {
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
  ModifierData *md = BKE_modifiers_findby_type(ob, eModifierType_Fluidsim);

  if (md && (BKE_modifier_is_enabled(scene, md, eModifierMode_Render))) {
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
  if (BKE_animdata_id_is_animated(static_cast<ID *>(m_object->data))) {
    return true;
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
  bool has_flat_shaded_poly = false;

  get_vertices(mesh, points);
  get_topology(mesh, poly_verts, loop_counts, has_flat_shaded_poly);

  if (m_first_frame && m_settings.export_face_sets) {
    writeFaceSets(mesh, m_mesh_schema);
  }

  m_mesh_sample = OPolyMeshSchema::Sample(
      V3fArraySample(points), Int32ArraySample(poly_verts), Int32ArraySample(loop_counts));

  UVSample sample;
  if (m_settings.export_uvs) {
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
    get_loop_normals(mesh, normals, has_flat_shaded_poly);

    ON3fGeomParam::Sample normals_sample;
    if (!normals.empty()) {
      normals_sample.setScope(kFacevaryingScope);
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
  bool has_flat_poly = false;

  get_vertices(mesh, points);
  get_topology(mesh, poly_verts, loop_counts, has_flat_poly);
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

    Mesh *result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL, mesh);
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

    for (int i = 0; i < totverts; i++) {
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

  for (int i = 0; i < num_poly; i++) {
    MPoly &current_poly = polygons[i];
    short mnr = current_poly.mat_nr;

    Material *mat = BKE_object_material_get(m_object, mnr + 1);

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
    Material *mat = BKE_object_material_get(m_object, 1);

    std::string name = (mat) ? get_id_name(&mat->id) : "default";

    std::vector<int32_t> faceArray;

    for (int i = 0, e = mesh->totface; i < e; i++) {
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
