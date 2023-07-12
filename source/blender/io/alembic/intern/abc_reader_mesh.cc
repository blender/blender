/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_reader_mesh.h"
#include "abc_axis_conversion.h"
#include "abc_reader_transform.h"
#include "abc_util.h"

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_compiler_compat.h"
#include "BLI_edgehash.h"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"

#include "BLT_translation.h"

#include "BKE_attribute.hh"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_object.h"

using Alembic::Abc::FloatArraySamplePtr;
using Alembic::Abc::Int32ArraySamplePtr;
using Alembic::Abc::IV3fArrayProperty;
using Alembic::Abc::P3fArraySamplePtr;
using Alembic::Abc::PropertyHeader;
using Alembic::Abc::V3fArraySamplePtr;

using Alembic::AbcGeom::IC3fGeomParam;
using Alembic::AbcGeom::IC4fGeomParam;
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

namespace blender::io::alembic {

/* NOTE: Alembic's polygon winding order is clockwise, to match with Renderman. */

/* Some helpers for mesh generation */
namespace utils {

static std::map<std::string, Material *> build_material_map(const Main *bmain)
{
  std::map<std::string, Material *> mat_map;
  LISTBASE_FOREACH (Material *, material, &bmain->materials) {
    mat_map[material->id.name + 2] = material;
  }
  return mat_map;
}

static void assign_materials(Main *bmain,
                             Object *ob,
                             const std::map<std::string, int> &mat_index_map)
{
  std::map<std::string, int>::const_iterator it;
  if (mat_index_map.size() > MAXMAT) {
    return;
  }

  std::map<std::string, Material *> matname_to_material = build_material_map(bmain);
  std::map<std::string, Material *>::iterator mat_iter;

  for (it = mat_index_map.begin(); it != mat_index_map.end(); ++it) {
    const std::string mat_name = it->first;
    const int mat_index = it->second;

    Material *assigned_mat;
    mat_iter = matname_to_material.find(mat_name);
    if (mat_iter == matname_to_material.end()) {
      assigned_mat = BKE_material_add(bmain, mat_name.c_str());
      id_us_min(&assigned_mat->id);
      matname_to_material[mat_name] = assigned_mat;
    }
    else {
      assigned_mat = mat_iter->second;
    }

    BKE_object_material_assign_single_obdata(bmain, ob, assigned_mat, mat_index);
  }
  if (ob->totcol > 0) {
    ob->actcol = 1;
  }
}

} /* namespace utils */

struct AbcMeshData {
  Int32ArraySamplePtr face_indices;
  Int32ArraySamplePtr face_counts;

  /* Optional settings for reading interpolated vertices. If present, `ceil_positions` has to be
   * valid. */
  std::optional<SampleInterpolationSettings> interpolation_settings;
  P3fArraySamplePtr positions;
  P3fArraySamplePtr ceil_positions;

  AbcUvScope uv_scope;
  V2fArraySamplePtr uvs;
  UInt32ArraySamplePtr uvs_indices;
};

static void read_mverts_interp(float3 *vert_positions,
                               const P3fArraySamplePtr &positions,
                               const P3fArraySamplePtr &ceil_positions,
                               const double weight)
{
  float tmp[3];
  for (int i = 0; i < positions->size(); i++) {
    const Imath::V3f &floor_pos = (*positions)[i];
    const Imath::V3f &ceil_pos = (*ceil_positions)[i];

    interp_v3_v3v3(tmp, floor_pos.getValue(), ceil_pos.getValue(), float(weight));
    copy_zup_from_yup(vert_positions[i], tmp);
  }
}

static void read_mverts(CDStreamConfig &config, const AbcMeshData &mesh_data)
{
  float3 *vert_positions = config.positions;
  const P3fArraySamplePtr &positions = mesh_data.positions;

  if (mesh_data.interpolation_settings.has_value()) {
    BLI_assert_msg(
        mesh_data.ceil_positions != nullptr,
        "AbcMeshData does not have ceil positions although it has some interpolation settings.");

    const double weight = mesh_data.interpolation_settings->weight;
    read_mverts_interp(vert_positions, positions, mesh_data.ceil_positions, weight);
    BKE_mesh_tag_positions_changed(config.mesh);
    return;
  }

  read_mverts(*config.mesh, positions, nullptr);
}

void read_mverts(Mesh &mesh, const P3fArraySamplePtr positions, const N3fArraySamplePtr normals)
{
  MutableSpan<float3> vert_positions = mesh.vert_positions_for_write();
  for (int i = 0; i < positions->size(); i++) {
    Imath::V3f pos_in = (*positions)[i];

    copy_zup_from_yup(vert_positions[i], pos_in.getValue());
  }
  BKE_mesh_tag_positions_changed(&mesh);

  if (normals) {
    float(*vert_normals)[3] = BKE_mesh_vert_normals_for_write(&mesh);
    for (const int64_t i : IndexRange(normals->size())) {
      Imath::V3f nor_in = (*normals)[i];
      copy_zup_from_yup(vert_normals[i], nor_in.getValue());
    }
    BKE_mesh_vert_normals_clear_dirty(&mesh);
  }
}

static void read_mpolys(CDStreamConfig &config, const AbcMeshData &mesh_data)
{
  int *poly_offsets = config.poly_offsets;
  int *corner_verts = config.corner_verts;
  float2 *mloopuvs = config.mloopuv;

  const Int32ArraySamplePtr &face_indices = mesh_data.face_indices;
  const Int32ArraySamplePtr &face_counts = mesh_data.face_counts;
  const V2fArraySamplePtr &uvs = mesh_data.uvs;
  const size_t uvs_size = uvs == nullptr ? 0 : uvs->size();

  const UInt32ArraySamplePtr &uvs_indices = mesh_data.uvs_indices;

  const bool do_uvs = (mloopuvs && uvs && uvs_indices);
  const bool do_uvs_per_loop = do_uvs && mesh_data.uv_scope == ABC_UV_SCOPE_LOOP;
  BLI_assert(!do_uvs || mesh_data.uv_scope != ABC_UV_SCOPE_NONE);
  uint loop_index = 0;
  uint rev_loop_index = 0;
  uint uv_index = 0;
  bool seen_invalid_geometry = false;

  for (int i = 0; i < face_counts->size(); i++) {
    const int face_size = (*face_counts)[i];

    poly_offsets[i] = loop_index;

    /* Polygons are always assumed to be smooth-shaded. If the Alembic mesh should be flat-shaded,
     * this is encoded in custom loop normals. See #71246. */

    /* NOTE: Alembic data is stored in the reverse order. */
    rev_loop_index = loop_index + (face_size - 1);

    uint last_vertex_index = 0;
    for (int f = 0; f < face_size; f++, loop_index++, rev_loop_index--) {
      const int vert = (*face_indices)[loop_index];
      corner_verts[rev_loop_index] = vert;

      if (f > 0 && vert == last_vertex_index) {
        /* This face is invalid, as it has consecutive loops from the same vertex. This is caused
         * by invalid geometry in the Alembic file, such as in #76514. */
        seen_invalid_geometry = true;
      }
      last_vertex_index = vert;

      if (do_uvs) {
        uv_index = (*uvs_indices)[do_uvs_per_loop ? loop_index : last_vertex_index];

        /* Some Alembic files are broken (or at least export UVs in a way we don't expect). */
        if (uv_index >= uvs_size) {
          continue;
        }

        mloopuvs[rev_loop_index][0] = (*uvs)[uv_index][0];
        mloopuvs[rev_loop_index][1] = (*uvs)[uv_index][1];
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

static void process_no_normals(CDStreamConfig & /*config*/)
{
  /* Absence of normals in the Alembic mesh is interpreted as 'smooth'. */
}

static void process_loop_normals(CDStreamConfig &config, const N3fArraySamplePtr loop_normals_ptr)
{
  size_t loop_count = loop_normals_ptr->size();

  if (loop_count == 0) {
    process_no_normals(config);
    return;
  }

  Mesh *mesh = config.mesh;
  if (loop_count != mesh->totloop) {
    /* This happens in certain Houdini exports. When a mesh is animated and then replaced by a
     * fluid simulation, Houdini will still write the original mesh's loop normals, but the mesh
     * verts/loops/polys are from the simulation. In such cases the normals cannot be mapped to the
     * mesh, so it's better to ignore them. */
    process_no_normals(config);
    return;
  }

  float(*lnors)[3] = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(loop_count, sizeof(float[3]), "ABC::FaceNormals"));

  const OffsetIndices polys = mesh->polys();
  const N3fArraySample &loop_normals = *loop_normals_ptr;
  int abc_index = 0;
  for (int i = 0, e = mesh->totpoly; i < e; i++) {
    const IndexRange poly = polys[i];
    /* As usual, ABC orders the loops in reverse. */
    for (int j = poly.size() - 1; j >= 0; j--, abc_index++) {
      int blender_index = poly[j];
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

  float(*vert_normals)[3] = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(normals_count, sizeof(float[3]), "ABC::VertexNormals"));

  const N3fArraySample &vertex_normals = *vertex_normals_ptr;
  for (int index = 0; index < normals_count; index++) {
    copy_zup_from_yup(vert_normals[index], vertex_normals[index].getValue());
  }

  config.mesh->flag |= ME_AUTOSMOOTH;
  BKE_mesh_set_custom_normals_from_verts(config.mesh, vert_normals);
  MEM_freeN(vert_normals);
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
    case Alembic::AbcGeom::kFacevaryingScope: /* 'Vertex Normals' in Houdini. */
      process_loop_normals(config, normsamp.getVals());
      break;
    case Alembic::AbcGeom::kVertexScope:
    case Alembic::AbcGeom::kVaryingScope: /* 'Point Normals' in Houdini. */
      process_vertex_normals(config, normsamp.getVals());
      break;
    case Alembic::AbcGeom::kConstantScope:
    case Alembic::AbcGeom::kUniformScope:
    case Alembic::AbcGeom::kUnknownScope:
      process_no_normals(config);
      break;
  }
}

BLI_INLINE void read_uvs_params(CDStreamConfig &config,
                                AbcMeshData &abc_data,
                                const IV2fGeomParam &uv,
                                const ISampleSelector &selector)
{
  if (!uv.valid()) {
    return;
  }

  IV2fGeomParam::Sample uvsamp;
  uv.getIndexed(uvsamp, selector);

  UInt32ArraySamplePtr uvs_indices = uvsamp.getIndices();

  const AbcUvScope uv_scope = get_uv_scope(uv.getScope(), config, uvs_indices);

  if (uv_scope == ABC_UV_SCOPE_NONE) {
    return;
  }

  abc_data.uv_scope = uv_scope;
  abc_data.uvs = uvsamp.getVals();
  abc_data.uvs_indices = uvs_indices;

  std::string name = Alembic::Abc::GetSourceName(uv.getMetaData());

  /* According to the convention, primary UVs should have had their name
   * set using Alembic::Abc::SetSourceName, but you can't expect everyone
   * to follow it! :) */
  if (name.empty()) {
    name = uv.getName();
  }

  void *cd_ptr = config.add_customdata_cb(config.mesh, name.c_str(), CD_PROP_FLOAT2);
  config.mloopuv = static_cast<float2 *>(cd_ptr);
}

static void *add_customdata_cb(Mesh *mesh, const char *name, int data_type)
{
  eCustomDataType cd_data_type = static_cast<eCustomDataType>(data_type);

  /* unsupported custom data type -- don't do anything. */
  if (!ELEM(cd_data_type, CD_PROP_FLOAT2, CD_PROP_BYTE_COLOR)) {
    return nullptr;
  }

  void *cd_ptr = CustomData_get_layer_named_for_write(
      &mesh->ldata, cd_data_type, name, mesh->totloop);
  if (cd_ptr != nullptr) {
    /* layer already exists, so just return it. */
    return cd_ptr;
  }

  /* Create a new layer. */
  int numloops = mesh->totloop;
  cd_ptr = CustomData_add_layer_named(&mesh->ldata, cd_data_type, CD_SET_DEFAULT, numloops, name);
  return cd_ptr;
}

static V3fArraySamplePtr get_velocity_prop(const ICompoundProperty &schema,
                                           const ISampleSelector &selector,
                                           const std::string &name)
{
  for (size_t i = 0; i < schema.getNumProperties(); i++) {
    const PropertyHeader &header = schema.getPropertyHeader(i);

    if (header.isCompound()) {
      const ICompoundProperty &prop = ICompoundProperty(schema, header.getName());

      if (has_property(prop, name)) {
        /* Header cannot be null here, as its presence is checked via has_property, so it is safe
         * to dereference. */
        const PropertyHeader *header = prop.getPropertyHeader(name);
        if (!IV3fArrayProperty::matches(*header)) {
          continue;
        }

        const IV3fArrayProperty &velocity_prop = IV3fArrayProperty(prop, name, 0);
        if (velocity_prop) {
          return velocity_prop.getValue(selector);
        }
      }
    }
    else if (header.isArray()) {
      if (header.getName() == name && IV3fArrayProperty::matches(header)) {
        const IV3fArrayProperty &velocity_prop = IV3fArrayProperty(schema, name, 0);
        return velocity_prop.getValue(selector);
      }
    }
  }

  return V3fArraySamplePtr();
}

static void read_velocity(const V3fArraySamplePtr &velocities,
                          const CDStreamConfig &config,
                          const float velocity_scale)
{
  const int num_velocity_vectors = int(velocities->size());
  if (num_velocity_vectors != config.mesh->totvert) {
    /* Files containing videogrammetry data may be malformed and export velocity data on missing
     * frames (most likely by copying the last valid data). */
    return;
  }

  CustomDataLayer *velocity_layer = BKE_id_attribute_new(
      &config.mesh->id, "velocity", CD_PROP_FLOAT3, ATTR_DOMAIN_POINT, nullptr);
  float(*velocity)[3] = (float(*)[3])velocity_layer->data;

  for (int i = 0; i < num_velocity_vectors; i++) {
    const Imath::V3f &vel_in = (*velocities)[i];
    copy_zup_from_yup(velocity[i], vel_in.getValue());
    mul_v3_fl(velocity[i], velocity_scale);
  }
}

template<typename SampleType>
static bool samples_have_same_topology(const SampleType &sample, const SampleType &ceil_sample)
{
  const P3fArraySamplePtr &positions = sample.getPositions();
  const Alembic::Abc::Int32ArraySamplePtr &face_indices = sample.getFaceIndices();
  const Alembic::Abc::Int32ArraySamplePtr &face_counts = sample.getFaceCounts();

  const P3fArraySamplePtr &ceil_positions = ceil_sample.getPositions();
  const Alembic::Abc::Int32ArraySamplePtr &ceil_face_indices = ceil_sample.getFaceIndices();
  const Alembic::Abc::Int32ArraySamplePtr &ceil_face_counts = ceil_sample.getFaceCounts();

  /* It the counters are different, we can be sure the topology is different. */
  const bool different_counters = positions->size() != ceil_positions->size() ||
                                  face_counts->size() != ceil_face_counts->size() ||
                                  face_indices->size() != ceil_face_indices->size();
  if (different_counters) {
    return false;
  }

  /* Otherwise, we need to check the connectivity as files from e.g. videogrammetry may have the
   * same face count, but different connections between faces. */

  if (memcmp(face_counts->get(), ceil_face_counts->get(), face_counts->size() * sizeof(int))) {
    return false;
  }

  if (memcmp(face_indices->get(), ceil_face_indices->get(), face_indices->size() * sizeof(int))) {
    return false;
  }

  return true;
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

  const std::optional<SampleInterpolationSettings> interpolation_settings =
      get_sample_interpolation_settings(
          selector, schema.getTimeSampling(), schema.getNumSamples());

  const bool use_vertex_interpolation = settings->read_flag & MOD_MESHSEQ_INTERPOLATE_VERTICES;
  if (use_vertex_interpolation && interpolation_settings.has_value()) {
    Alembic::AbcGeom::IPolyMeshSchema::Sample ceil_sample;
    schema.get(ceil_sample, Alembic::Abc::ISampleSelector(interpolation_settings->ceil_index));
    if (samples_have_same_topology(sample, ceil_sample)) {
      /* Only set interpolation data if the samples are compatible. */
      abc_mesh_data.ceil_positions = ceil_sample.getPositions();
      abc_mesh_data.interpolation_settings = interpolation_settings;
    }
  }

  if ((settings->read_flag & MOD_MESHSEQ_READ_UV) != 0) {
    read_uvs_params(config, abc_mesh_data, schema.getUVsParam(), selector);
  }

  if ((settings->read_flag & MOD_MESHSEQ_READ_VERT) != 0) {
    read_mverts(config, abc_mesh_data);
    read_generated_coordinates(schema.getArbGeomParams(), config, selector);
  }

  if ((settings->read_flag & MOD_MESHSEQ_READ_POLY) != 0) {
    read_mpolys(config, abc_mesh_data);
    process_normals(config, schema.getNormalsParam(), selector);
  }

  if ((settings->read_flag & (MOD_MESHSEQ_READ_UV | MOD_MESHSEQ_READ_COLOR)) != 0) {
    read_custom_data(iobject_full_name, schema.getArbGeomParams(), config, selector);
  }

  if (!settings->velocity_name.empty() && settings->velocity_scale != 0.0f) {
    V3fArraySamplePtr velocities = get_velocity_prop(schema, selector, settings->velocity_name);
    if (velocities) {
      read_velocity(velocities, config, settings->velocity_scale);
    }
  }
}

CDStreamConfig get_config(Mesh *mesh)
{
  CDStreamConfig config;
  config.mesh = mesh;
  config.positions = mesh->vert_positions_for_write().data();
  config.corner_verts = mesh->corner_verts_for_write().data();
  config.poly_offsets = mesh->poly_offsets_for_write().data();
  config.totvert = mesh->totvert;
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

template<class typedGeomParam>
bool is_valid_animated(const ICompoundProperty arbGeomParams, const PropertyHeader &prop_header)
{
  if (!typedGeomParam::matches(prop_header)) {
    return false;
  }

  typedGeomParam geom_param(arbGeomParams, prop_header.getName());
  return geom_param.valid() && !geom_param.isConstant();
}

static bool has_animated_geom_params(const ICompoundProperty arbGeomParams)
{
  if (!arbGeomParams.valid()) {
    return false;
  }

  const int num_props = arbGeomParams.getNumProperties();
  for (int i = 0; i < num_props; i++) {
    const PropertyHeader &prop_header = arbGeomParams.getPropertyHeader(i);

    /* These are interpreted as vertex colors later (see 'read_custom_data'). */
    if (is_valid_animated<IC3fGeomParam>(arbGeomParams, prop_header)) {
      return true;
    }
    if (is_valid_animated<IC4fGeomParam>(arbGeomParams, prop_header)) {
      return true;
    }
  }

  return false;
}

/* Specialization of #has_animations() as defined in abc_reader_object.h. */
template<> bool has_animations(Alembic::AbcGeom::IPolyMeshSchema &schema, ImportSettings *settings)
{
  if (settings->is_sequence || !schema.isConstant()) {
    return true;
  }

  IV2fGeomParam uvsParam = schema.getUVsParam();
  if (uvsParam.valid() && !uvsParam.isConstant()) {
    return true;
  }

  IN3fGeomParam normalsParam = schema.getNormalsParam();
  if (normalsParam.valid() && !normalsParam.isConstant()) {
    return true;
  }

  ICompoundProperty arbGeomParams = schema.getArbGeomParams();
  if (has_animated_geom_params(arbGeomParams)) {
    return true;
  }

  return false;
}

void AbcMeshReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  Mesh *mesh = BKE_mesh_add(bmain, m_data_name.c_str());

  m_object = BKE_object_add_only_object(bmain, OB_MESH, m_object_name.c_str());
  m_object->data = mesh;

  Mesh *read_mesh = this->read_mesh(mesh, sample_sel, MOD_MESHSEQ_READ_ALL, "", 0.0f, nullptr);
  if (read_mesh != mesh) {
    BKE_mesh_nomain_to_mesh(read_mesh, mesh, m_object);
  }

  if (m_settings->validate_meshes) {
    BKE_mesh_validate(mesh, false, false);
  }

  readFaceSetsSample(bmain, mesh, sample_sel);

  if (m_settings->always_add_cache_reader || has_animations(m_schema, m_settings)) {
    addCacheModifier();
  }
}

bool AbcMeshReader::accepts_object_type(
    const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
    const Object *const ob,
    const char **err_str) const
{
  if (!Alembic::AbcGeom::IPolyMesh::matches(alembic_header)) {
    *err_str = TIP_(
        "Object type mismatch, Alembic object path pointed to PolyMesh when importing, but not "
        "any more");
    return false;
  }

  if (ob->type != OB_MESH) {
    *err_str = TIP_("Object type mismatch, Alembic object path points to PolyMesh");
    return false;
  }

  return true;
}

bool AbcMeshReader::topology_changed(const Mesh *existing_mesh, const ISampleSelector &sample_sel)
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
    /* A similar error in read_mesh() would just return existing_mesh. */
    return false;
  }

  const P3fArraySamplePtr &positions = sample.getPositions();
  const Alembic::Abc::Int32ArraySamplePtr &face_indices = sample.getFaceIndices();
  const Alembic::Abc::Int32ArraySamplePtr &face_counts = sample.getFaceCounts();

  /* It the counters are different, we can be sure the topology is different. */
  const bool different_counters = positions->size() != existing_mesh->totvert ||
                                  face_counts->size() != existing_mesh->totpoly ||
                                  face_indices->size() != existing_mesh->totloop;
  if (different_counters) {
    return true;
  }

  /* Check first if we indeed have multiple samples, unless we read a file sequence in which case
   * we need to do a full topology comparison. */
  if (!m_is_reading_a_file_sequence && (m_schema.getFaceIndicesProperty().getNumSamples() == 1 &&
                                        m_schema.getFaceCountsProperty().getNumSamples() == 1))
  {
    return false;
  }

  /* Otherwise, we need to check the connectivity as files from e.g. videogrammetry may have the
   * same face count, but different connections between faces. */
  uint abc_index = 0;

  const int *mesh_corner_verts = existing_mesh->corner_verts().data();
  const int *mesh_poly_offsets = existing_mesh->poly_offsets().data();

  for (int i = 0; i < face_counts->size(); i++) {
    if (mesh_poly_offsets[i] != abc_index) {
      return true;
    }

    const int abc_face_size = (*face_counts)[i];
    /* NOTE: Alembic data is stored in the reverse order. */
    uint rev_loop_index = abc_index + (abc_face_size - 1);
    for (int f = 0; f < abc_face_size; f++, abc_index++, rev_loop_index--) {
      const int mesh_vert = mesh_corner_verts[rev_loop_index];
      const int abc_vert = (*face_indices)[abc_index];
      if (mesh_vert != abc_vert) {
        return true;
      }
    }
  }

  return false;
}

Mesh *AbcMeshReader::read_mesh(Mesh *existing_mesh,
                               const ISampleSelector &sample_sel,
                               const int read_flag,
                               const char *velocity_name,
                               const float velocity_scale,
                               const char **err_str)
{
  IPolyMeshSchema::Sample sample;
  try {
    sample = m_schema.getValue(sample_sel);
  }
  catch (Alembic::Util::Exception &ex) {
    if (err_str != nullptr) {
      *err_str = TIP_("Error reading mesh sample; more detail on the console");
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
      *err_str = TIP_("Invalid mesh; more detail on the console");
    }
    printf("Alembic: invalid mesh sample for '%s/%s' at time %f, less than 2 loops per face\n",
           m_iobject.getFullName().c_str(),
           m_schema.getName().c_str(),
           sample_sel.getRequestedTime());
    return existing_mesh;
  }

  Mesh *new_mesh = nullptr;

  /* Only read point data when streaming meshes, unless we need to create new ones. */
  ImportSettings settings;
  settings.read_flag |= read_flag;
  settings.velocity_name = velocity_name;
  settings.velocity_scale = velocity_scale;

  if (topology_changed(existing_mesh, sample_sel)) {
    new_mesh = BKE_mesh_new_nomain_from_template(
        existing_mesh, positions->size(), 0, face_counts->size(), face_indices->size());

    settings.read_flag |= MOD_MESHSEQ_READ_ALL;
  }
  else {
    /* If the face count changed (e.g. by triangulation), only read points.
     * This prevents crash from #49813.
     * TODO(kevin): perhaps find a better way to do this? */
    if (face_counts->size() != existing_mesh->totpoly ||
        face_indices->size() != existing_mesh->totloop)
    {
      settings.read_flag = MOD_MESHSEQ_READ_VERT;

      if (err_str) {
        *err_str = TIP_(
            "Topology has changed, perhaps by triangulating the mesh. Only vertices will be "
            "read!");
      }
    }
  }

  Mesh *mesh_to_export = new_mesh ? new_mesh : existing_mesh;
  CDStreamConfig config = get_config(mesh_to_export);
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
      bke::MutableAttributeAccessor attributes = new_mesh->attributes_for_write();
      bke::SpanAttributeWriter<int> material_indices =
          attributes.lookup_or_add_for_write_span<int>("material_index", ATTR_DOMAIN_FACE);
      assign_facesets_to_material_indices(sample_sel, material_indices.span, mat_map);
      material_indices.finish();
    }

    return new_mesh;
  }

  return existing_mesh;
}

void AbcMeshReader::assign_facesets_to_material_indices(const ISampleSelector &sample_sel,
                                                        MutableSpan<int> material_indices,
                                                        std::map<std::string, int> &r_mat_map)
{
  std::vector<std::string> face_sets;
  m_schema.getFaceSetNames(face_sets);

  if (face_sets.empty()) {
    return;
  }

  int current_mat = 0;

  for (const std::string &grp_name : face_sets) {
    if (r_mat_map.find(grp_name) == r_mat_map.end()) {
      r_mat_map[grp_name] = ++current_mat;
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

      if (pos >= material_indices.size()) {
        std::cerr << "Faceset overflow on " << faceset.getName() << '\n';
        break;
      }

      material_indices[pos] = assigned_mat - 1;
    }
  }
}

void AbcMeshReader::readFaceSetsSample(Main *bmain, Mesh *mesh, const ISampleSelector &sample_sel)
{
  std::map<std::string, int> mat_map;
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_span<int>(
      "material_index", ATTR_DOMAIN_FACE);
  assign_facesets_to_material_indices(sample_sel, material_indices.span, mat_map);
  material_indices.finish();
  utils::assign_materials(bmain, m_object, mat_map);
}

/* ************************************************************************** */

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

  const std::optional<SampleInterpolationSettings> interpolation_settings =
      get_sample_interpolation_settings(
          selector, schema.getTimeSampling(), schema.getNumSamples());

  const bool use_vertex_interpolation = settings->read_flag & MOD_MESHSEQ_INTERPOLATE_VERTICES;
  if (use_vertex_interpolation && interpolation_settings.has_value()) {
    Alembic::AbcGeom::ISubDSchema::Sample ceil_sample;
    schema.get(ceil_sample, Alembic::Abc::ISampleSelector(interpolation_settings->ceil_index));
    if (samples_have_same_topology(sample, ceil_sample)) {
      /* Only set interpolation data if the samples are compatible. */
      abc_mesh_data.ceil_positions = ceil_sample.getPositions();
      abc_mesh_data.interpolation_settings = interpolation_settings;
    }
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

  if (!settings->velocity_name.empty() && settings->velocity_scale != 0.0f) {
    V3fArraySamplePtr velocities = get_velocity_prop(schema, selector, settings->velocity_name);
    if (velocities) {
      read_velocity(velocities, config, settings->velocity_scale);
    }
  }
}

static void read_vertex_creases(Mesh *mesh,
                                const Int32ArraySamplePtr &indices,
                                const FloatArraySamplePtr &sharpnesses)
{
  if (!(indices && sharpnesses && indices->size() == sharpnesses->size() && indices->size() != 0))
  {
    return;
  }

  float *vertex_crease_data = (float *)CustomData_add_layer_named(
      &mesh->vdata, CD_PROP_FLOAT, CD_SET_DEFAULT, mesh->totvert, "crease_vert");
  const int totvert = mesh->totvert;

  for (int i = 0, v = indices->size(); i < v; ++i) {
    const int idx = (*indices)[i];

    if (idx >= totvert) {
      continue;
    }

    vertex_crease_data[idx] = (*sharpnesses)[i];
  }
}

static void read_edge_creases(Mesh *mesh,
                              const Int32ArraySamplePtr &indices,
                              const FloatArraySamplePtr &sharpnesses)
{
  if (!(indices && sharpnesses)) {
    return;
  }

  MutableSpan<int2> edges = mesh->edges_for_write();
  EdgeHash *edge_hash = BLI_edgehash_new_ex(__func__, edges.size());

  float *creases = static_cast<float *>(CustomData_add_layer_named(
      &mesh->edata, CD_PROP_FLOAT, CD_SET_DEFAULT, edges.size(), "crease_edge"));

  for (const int i : edges.index_range()) {
    int2 &edge = edges[i];
    BLI_edgehash_insert(edge_hash, edge[0], edge[1], &edge);
  }

  for (int i = 0, s = 0, e = indices->size(); i < e; i += 2, s++) {
    int v1 = (*indices)[i];
    int v2 = (*indices)[i + 1];

    if (v2 < v1) {
      /* It appears to be common to store edges with the smallest index first, in which case this
       * prevents us from doing the second search below. */
      std::swap(v1, v2);
    }

    int2 *edge = static_cast<int2 *>(BLI_edgehash_lookup(edge_hash, v1, v2));
    if (edge == nullptr) {
      edge = static_cast<int2 *>(BLI_edgehash_lookup(edge_hash, v2, v1));
    }

    if (edge) {
      creases[edge - edges.data()] = unit_float_to_uchar_clamp((*sharpnesses)[s]);
    }
  }

  BLI_edgehash_free(edge_hash, nullptr);
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
    *err_str = TIP_(
        "Object type mismatch, Alembic object path pointed to SubD when importing, but not any "
        "more");
    return false;
  }

  if (ob->type != OB_MESH) {
    *err_str = TIP_("Object type mismatch, Alembic object path points to SubD");
    return false;
  }

  return true;
}

void AbcSubDReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  Mesh *mesh = BKE_mesh_add(bmain, m_data_name.c_str());

  m_object = BKE_object_add_only_object(bmain, OB_MESH, m_object_name.c_str());
  m_object->data = mesh;

  Mesh *read_mesh = this->read_mesh(mesh, sample_sel, MOD_MESHSEQ_READ_ALL, "", 0.0f, nullptr);
  if (read_mesh != mesh) {
    BKE_mesh_nomain_to_mesh(read_mesh, mesh, m_object);
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

  read_edge_creases(mesh, sample.getCreaseIndices(), sample.getCreaseSharpnesses());

  read_vertex_creases(mesh, sample.getCornerIndices(), sample.getCornerSharpnesses());

  if (m_settings->validate_meshes) {
    BKE_mesh_validate(mesh, false, false);
  }

  if (m_settings->always_add_cache_reader || has_animations(m_schema, m_settings)) {
    addCacheModifier();
  }
}

Mesh *AbcSubDReader::read_mesh(Mesh *existing_mesh,
                               const ISampleSelector &sample_sel,
                               const int read_flag,
                               const char *velocity_name,
                               const float velocity_scale,
                               const char **err_str)
{
  ISubDSchema::Sample sample;
  try {
    sample = m_schema.getValue(sample_sel);
  }
  catch (Alembic::Util::Exception &ex) {
    if (err_str != nullptr) {
      *err_str = TIP_("Error reading mesh sample; more detail on the console");
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

  Mesh *new_mesh = nullptr;

  ImportSettings settings;
  settings.read_flag |= read_flag;
  settings.velocity_name = velocity_name;
  settings.velocity_scale = velocity_scale;

  if (existing_mesh->totvert != positions->size()) {
    new_mesh = BKE_mesh_new_nomain_from_template(
        existing_mesh, positions->size(), 0, face_counts->size(), face_indices->size());

    settings.read_flag |= MOD_MESHSEQ_READ_ALL;
  }
  else {
    /* If the face count changed (e.g. by triangulation), only read points.
     * This prevents crash from #49813.
     * TODO(kevin): perhaps find a better way to do this? */
    if (face_counts->size() != existing_mesh->totpoly ||
        face_indices->size() != existing_mesh->totloop)
    {
      settings.read_flag = MOD_MESHSEQ_READ_VERT;

      if (err_str) {
        *err_str = TIP_(
            "Topology has changed, perhaps by triangulating the mesh. Only vertices will be "
            "read!");
      }
    }
  }

  /* Only read point data when streaming meshes, unless we need to create new ones. */
  Mesh *mesh_to_export = new_mesh ? new_mesh : existing_mesh;
  CDStreamConfig config = get_config(mesh_to_export);
  config.time = sample_sel.getRequestedTime();
  config.modifier_error_message = err_str;
  read_subd_sample(m_iobject.getFullName(), &settings, m_schema, sample_sel, config);

  return mesh_to_export;
}

}  // namespace blender::io::alembic
