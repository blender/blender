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

#include "abc_reader_mesh.h"
#include "abc_axis_conversion.h"
#include "abc_reader_transform.h"
#include "abc_util.h"

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math_geom.h"

#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

using Alembic::Abc::Int32ArraySamplePtr;
using Alembic::Abc::P3fArraySamplePtr;

using Alembic::AbcGeom::IFaceSet;
using Alembic::AbcGeom::IFaceSetSchema;
using Alembic::AbcGeom::IN3fGeomParam;
using Alembic::AbcGeom::IObject;
using Alembic::AbcGeom::IPolyMesh;
using Alembic::AbcGeom::IPolyMeshSchema;
using Alembic::AbcGeom::ISampleSelector;
using Alembic::AbcGeom::ISubD;
using Alembic::AbcGeom::ISubDSchema;
using Alembic::AbcGeom::IV2fGeomParam;
using Alembic::AbcGeom::kWrapExisting;
using Alembic::AbcGeom::N3fArraySample;
using Alembic::AbcGeom::N3fArraySamplePtr;
using Alembic::AbcGeom::UInt32ArraySamplePtr;
using Alembic::AbcGeom::V2fArraySamplePtr;

namespace blender {
namespace io {
namespace alembic {

/* NOTE: Alembic's polygon winding order is clockwise, to match with Renderman. */

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
  for (; it != mat_index_map.end(); ++it, matcount++) {
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

      BKE_object_material_assign(bmain, ob, assigned_mat, it->second, BKE_MAT_ASSIGN_OBDATA);
    }
  }
}

} /* namespace utils */

struct AbcMeshData {
  Int32ArraySamplePtr face_indices;
  Int32ArraySamplePtr face_counts;

  P3fArraySamplePtr positions;
  P3fArraySamplePtr ceil_positions;

  V2fArraySamplePtr uvs;
  UInt32ArraySamplePtr uvs_indices;
};

static void read_mverts_interp(MVert *mverts,
                               const P3fArraySamplePtr &positions,
                               const P3fArraySamplePtr &ceil_positions,
                               const float weight)
{
  float tmp[3];
  for (int i = 0; i < positions->size(); i++) {
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

  if (config.weight != 0.0f && mesh_data.ceil_positions != NULL &&
      mesh_data.ceil_positions->size() == positions->size()) {
    read_mverts_interp(mverts, positions, mesh_data.ceil_positions, config.weight);
    return;
  }

  read_mverts(mverts, positions, nullptr);
}

void read_mverts(MVert *mverts, const P3fArraySamplePtr positions, const N3fArraySamplePtr normals)
{
  for (int i = 0; i < positions->size(); i++) {
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

  const bool do_uvs = (mloopuvs && uvs && uvs_indices) &&
                      (uvs_indices->size() == face_indices->size());
  unsigned int loop_index = 0;
  unsigned int rev_loop_index = 0;
  unsigned int uv_index = 0;
  bool seen_invalid_geometry = false;

  for (int i = 0; i < face_counts->size(); i++) {
    const int face_size = (*face_counts)[i];

    MPoly &poly = mpolys[i];
    poly.loopstart = loop_index;
    poly.totloop = face_size;

    /* Polygons are always assumed to be smooth-shaded. If the Alembic mesh should be flat-shaded,
     * this is encoded in custom loop normals. See T71246. */
    poly.flag |= ME_SMOOTH;

    /* NOTE: Alembic data is stored in the reverse order. */
    rev_loop_index = loop_index + (face_size - 1);

    uint last_vertex_index = 0;
    for (int f = 0; f < face_size; f++, loop_index++, rev_loop_index--) {
      MLoop &loop = mloops[rev_loop_index];
      loop.v = (*face_indices)[loop_index];

      if (f > 0 && loop.v == last_vertex_index) {
        /* This face is invalid, as it has consecutive loops from the same vertex. This is caused
         * by invalid geometry in the Alembic file, such as in T76514. */
        seen_invalid_geometry = true;
      }
      last_vertex_index = loop.v;

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

  BKE_mesh_calc_edges(config.mesh, false, false);
  if (seen_invalid_geometry) {
    if (config.modifier_error_message) {
      *config.modifier_error_message = "Mesh hash invalid geometry; more details on the console";
    }
    BKE_mesh_validate(config.mesh, true, true);
  }
}

static void process_no_normals(CDStreamConfig &config)
{
  /* Absense of normals in the Alembic mesh is interpreted as 'smooth'. */
  BKE_mesh_calc_normals(config.mesh);
}

static void process_loop_normals(CDStreamConfig &config, const N3fArraySamplePtr loop_normals_ptr)
{
  size_t loop_count = loop_normals_ptr->size();

  if (loop_count == 0) {
    process_no_normals(config);
    return;
  }

  float(*lnors)[3] = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(loop_count, sizeof(float[3]), "ABC::FaceNormals"));

  Mesh *mesh = config.mesh;
  MPoly *mpoly = mesh->mpoly;
  const N3fArraySample &loop_normals = *loop_normals_ptr;
  int abc_index = 0;
  for (int i = 0, e = mesh->totpoly; i < e; i++, mpoly++) {
    /* As usual, ABC orders the loops in reverse. */
    for (int j = mpoly->totloop - 1; j >= 0; j--, abc_index++) {
      int blender_index = mpoly->loopstart + j;
      copy_zup_from_yup(lnors[blender_index], loop_normals[abc_index].getValue());
    }
  }

  mesh->flag |= ME_AUTOSMOOTH;
  BKE_mesh_set_custom_normals(mesh, lnors);

  MEM_freeN(lnors);
}

static void process_vertex_normals(CDStreamConfig &config,
                                   const N3fArraySamplePtr vertex_normals_ptr)
{
  size_t normals_count = vertex_normals_ptr->size();
  if (normals_count == 0) {
    process_no_normals(config);
    return;
  }

  float(*vnors)[3] = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(normals_count, sizeof(float[3]), "ABC::VertexNormals"));

  const N3fArraySample &vertex_normals = *vertex_normals_ptr;
  for (int index = 0; index < normals_count; index++) {
    copy_zup_from_yup(vnors[index], vertex_normals[index].getValue());
  }

  config.mesh->flag |= ME_AUTOSMOOTH;
  BKE_mesh_set_custom_normals_from_vertices(config.mesh, vnors);
  MEM_freeN(vnors);
}

static void process_normals(CDStreamConfig &config,
                            const IN3fGeomParam &normals,
                            const ISampleSelector &selector)
{
  if (!normals.valid()) {
    process_no_normals(config);
    return;
  }

  IN3fGeomParam::Sample normsamp = normals.getExpandedValue(selector);
  Alembic::AbcGeom::GeometryScope scope = normals.getScope();

  switch (scope) {
    case Alembic::AbcGeom::kFacevaryingScope:  // 'Vertex Normals' in Houdini.
      process_loop_normals(config, normsamp.getVals());
      break;
    case Alembic::AbcGeom::kVertexScope:
    case Alembic::AbcGeom::kVaryingScope:  // 'Point Normals' in Houdini.
      process_vertex_normals(config, normsamp.getVals());
      break;
    case Alembic::AbcGeom::kConstantScope:
    case Alembic::AbcGeom::kUniformScope:
    case Alembic::AbcGeom::kUnknownScope:
      process_no_normals(config);
      break;
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

    void *cd_ptr = config.add_customdata_cb(config.mesh, name.c_str(), CD_MLOOPUV);
    config.mloopuv = static_cast<MLoopUV *>(cd_ptr);
  }
}

static void *add_customdata_cb(Mesh *mesh, const char *name, int data_type)
{
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
                             CDStreamConfig &config)
{
  const IPolyMeshSchema::Sample sample = schema.getValue(selector);

  AbcMeshData abc_mesh_data;
  abc_mesh_data.face_counts = sample.getFaceCounts();
  abc_mesh_data.face_indices = sample.getFaceIndices();
  abc_mesh_data.positions = sample.getPositions();

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
    process_normals(config, schema.getNormalsParam(), selector);
  }

  if ((settings->read_flag & (MOD_MESHSEQ_READ_UV | MOD_MESHSEQ_READ_COLOR)) != 0) {
    read_custom_data(iobject_full_name, schema.getArbGeomParams(), config, selector);
  }
}

CDStreamConfig get_config(Mesh *mesh)
{
  CDStreamConfig config;

  BLI_assert(mesh->mvert || mesh->totvert == 0);

  config.mesh = mesh;
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

/* Specialisation of has_animations() as defined in abc_reader_object.h. */
template<> bool has_animations(Alembic::AbcGeom::IPolyMeshSchema &schema, ImportSettings *settings)
{
  if (settings->is_sequence || !schema.isConstant()) {
    return true;
  }

  IV2fGeomParam uvsParam = schema.getUVsParam();
  IN3fGeomParam normalsParam = schema.getNormalsParam();
  return (uvsParam.valid() && !uvsParam.isConstant()) ||
         (normalsParam.valid() && !normalsParam.isConstant());
}

void AbcMeshReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  Mesh *mesh = BKE_mesh_add(bmain, m_data_name.c_str());

  m_object = BKE_object_add_only_object(bmain, OB_MESH, m_object_name.c_str());
  m_object->data = mesh;

  Mesh *read_mesh = this->read_mesh(mesh, sample_sel, MOD_MESHSEQ_READ_ALL, NULL);
  if (read_mesh != mesh) {
    /* XXX fixme after 2.80; mesh->flag isn't copied by BKE_mesh_nomain_to_mesh() */
    /* read_mesh can be freed by BKE_mesh_nomain_to_mesh(), so get the flag before that happens. */
    short autosmooth = (read_mesh->flag & ME_AUTOSMOOTH);
    BKE_mesh_nomain_to_mesh(read_mesh, mesh, m_object, &CD_MASK_MESH, true);
    mesh->flag |= autosmooth;
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

bool AbcMeshReader::topology_changed(Mesh *existing_mesh, const ISampleSelector &sample_sel)
{
  IPolyMeshSchema::Sample sample;
  try {
    sample = m_schema.getValue(sample_sel);
  }
  catch (Alembic::Util::Exception &ex) {
    printf("Alembic: error reading mesh sample for '%s/%s' at time %f: %s\n",
           m_iobject.getFullName().c_str(),
           m_schema.getName().c_str(),
           sample_sel.getRequestedTime(),
           ex.what());
    // A similar error in read_mesh() would just return existing_mesh.
    return false;
  }

  const P3fArraySamplePtr &positions = sample.getPositions();
  const Alembic::Abc::Int32ArraySamplePtr &face_indices = sample.getFaceIndices();
  const Alembic::Abc::Int32ArraySamplePtr &face_counts = sample.getFaceCounts();

  return positions->size() != existing_mesh->totvert ||
         face_counts->size() != existing_mesh->totpoly ||
         face_indices->size() != existing_mesh->totloop;
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

  /* Do some very minimal mesh validation. */
  const int poly_count = face_counts->size();
  const int loop_count = face_indices->size();
  /* This is the same test as in poly_to_tri_count(). */
  if (poly_count > 0 && loop_count < poly_count * 2) {
    if (err_str != nullptr) {
      *err_str = "Invalid mesh; more detail on the console";
    }
    printf("Alembic: invalid mesh sample for '%s/%s' at time %f, less than 2 loops per face\n",
           m_iobject.getFullName().c_str(),
           m_schema.getName().c_str(),
           sample_sel.getRequestedTime());
    return existing_mesh;
  }

  Mesh *new_mesh = NULL;

  /* Only read point data when streaming meshes, unless we need to create new ones. */
  ImportSettings settings;
  settings.read_flag |= read_flag;

  if (topology_changed(existing_mesh, sample_sel)) {
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
  config.modifier_error_message = err_str;

  read_mesh_sample(m_iobject.getFullName(), &settings, m_schema, sample_sel, config);

  if (new_mesh) {
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

  for (int i = 0; i < face_sets.size(); i++) {
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
  for (int i = 0, e = totedge; i < e; i++) {
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
    /* Alembic's 'SubD' scheme is used to store subdivision surfaces, i.e. the pre-subdivision
     * mesh. Currently we don't add a subdivision modifier when we load such data. This code is
     * assuming that the subdivided surface should be smooth. */
    read_mpolys(config, abc_mesh_data);
    process_no_normals(config);
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

  if (indices && sharpnesses) {
    MEdge *edges = mesh->medge;
    int totedge = mesh->totedge;

    for (int i = 0, s = 0, e = indices->size(); i < e; i += 2, s++) {
      int v1 = (*indices)[i];
      int v2 = (*indices)[i + 1];

      if (v2 < v1) {
        /* It appears to be common to store edges with the smallest index first, in which case this
         * prevents us from doing the second search below. */
        std::swap(v1, v2);
      }

      MEdge *edge = find_edge(edges, totedge, v1, v2);
      if (edge == NULL) {
        edge = find_edge(edges, totedge, v2, v1);
      }

      if (edge) {
        edge->crease = unit_float_to_uchar_clamp((*sharpnesses)[s]);
      }
    }

    mesh->cd_flag |= ME_CDFLAG_EDGE_CREASE;
  }

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
        face_indices->size() != existing_mesh->totloop) {
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

  return config.mesh;
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
