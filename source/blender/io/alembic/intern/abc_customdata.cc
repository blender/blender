/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_customdata.h"
#include "BLI_color_types.hh"
#include "abc_axis_conversion.h"
#include "abc_util.h"

#include <Alembic/Abc/ICompoundProperty.h>
#include <Alembic/Abc/ISampleSelector.h>
#include <Alembic/Abc/OCompoundProperty.h>
#include <Alembic/Abc/TypedArraySample.h>
#include <Alembic/AbcCoreAbstract/PropertyHeader.h>
#include <Alembic/AbcGeom/GeometryScope.h>
#include <Alembic/AbcGeom/IGeomParam.h>
#include <Alembic/AbcGeom/OGeomParam.h>

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"

/* NOTE: for now only UVs and Vertex Colors are supported for streaming.
 * Although Alembic only allows for a single UV layer per {I|O}Schema, and does
 * not have a vertex color concept, there is a convention between DCCs to write
 * such data in a way that lets other DCC know what they are for. See comments
 * in the write code for the conventions. */

using Alembic::AbcGeom::kFacevaryingScope;
using Alembic::AbcGeom::kVaryingScope;
using Alembic::AbcGeom::kVertexScope;

using Alembic::Abc::C4fArraySample;
using Alembic::Abc::UInt32ArraySample;
using Alembic::Abc::V2fArraySample;

using Alembic::AbcGeom::OC4fGeomParam;
using Alembic::AbcGeom::OV2fGeomParam;
using Alembic::AbcGeom::OV3fGeomParam;
namespace blender::io::alembic {

/* ORCO, Generated Coordinates, and Reference Points ("Pref") are all terms for the same thing.
 * Other applications (Maya, Houdini) write these to a property called "Pref". */
static const std::string propNameOriginalCoordinates("Pref");

static void get_uvs(const CDStreamConfig &config,
                    std::vector<Imath::V2f> &uvs,
                    std::vector<uint32_t> &uvidx,
                    const Span<float2> uv_map_array)
{
  const OffsetIndices faces = config.mesh->faces();
  int *corner_verts = config.corner_verts;

  if (!config.pack_uvs) {
    int count = 0;
    uvidx.resize(config.totloop);
    uvs.resize(config.totloop);

    /* Iterate in reverse order to match exported polygons. */
    for (const int i : faces.index_range()) {
      const IndexRange face = faces[i];
      const float2 *loopuv = uv_map_array.data() + face.start() + face.size();

      for (int j = 0; j < face.size(); j++, count++) {
        loopuv--;

        uvidx[count] = count;
        uvs[count][0] = (*loopuv)[0];
        uvs[count][1] = (*loopuv)[1];
      }
    }
  }
  else {
    /* Mapping for indexed UVs, deduplicating UV coordinates at vertices. */
    std::vector<std::vector<uint32_t>> idx_map(config.totvert);
    int idx_count = 0;

    for (const int i : faces.index_range()) {
      const IndexRange face = faces[i];
      int *face_verts = corner_verts + face.start() + face.size();
      const float2 *loopuv = uv_map_array.data() + face.start() + face.size();

      for (int j = 0; j < face.size(); j++) {
        face_verts--;
        loopuv--;

        Imath::V2f uv((*loopuv)[0], (*loopuv)[1]);
        bool found_same = false;

        /* Find UV already in uvs array. */
        for (uint32_t uv_idx : idx_map[*face_verts]) {
          if (uvs[uv_idx] == uv) {
            found_same = true;
            uvidx.push_back(uv_idx);
            break;
          }
        }

        /* UV doesn't exists for this vertex, add it. */
        if (!found_same) {
          uint32_t uv_idx = idx_count++;
          idx_map[*face_verts].push_back(uv_idx);
          uvidx.push_back(uv_idx);
          uvs.push_back(uv);
        }
      }
    }
  }
}

const char *get_uv_sample(UVSample &sample, const CDStreamConfig &config, const Mesh &mesh)
{
  const StringRefNull name = mesh.active_uv_map_name();
  if (name.is_empty()) {
    return "";
  }
  const VArraySpan uv_map = *mesh.attributes().lookup<float2>(name, bke::AttrDomain::Corner);
  if (uv_map.is_empty()) {
    return "";
  }

  get_uvs(config, sample.uvs, sample.indices, uv_map);

  return name.c_str();
}

/* Convention to write UVs:
 * - V2fGeomParam on the arbGeomParam
 * - set scope as face varying
 * - (optional due to its behavior) tag as UV using Alembic::AbcGeom::SetIsUV
 */
static void write_uv(const OCompoundProperty &prop,
                     CDStreamConfig &config,
                     const Span<float2> data,
                     const std::string &uv_map_name)
{
  std::vector<uint32_t> indices;
  std::vector<Imath::V2f> uvs;

  get_uvs(config, uvs, indices, data);

  if (indices.empty() || uvs.empty()) {
    return;
  }

  OV2fGeomParam param = config.abc_uv_maps[uv_map_name];

  if (!param.valid()) {
    param = OV2fGeomParam(prop, uv_map_name, true, kFacevaryingScope, 1);
  }
  OV2fGeomParam::Sample sample(V2fArraySample(&uvs.front(), uvs.size()),
                               UInt32ArraySample(&indices.front(), indices.size()),
                               kFacevaryingScope);
  param.set(sample);
  param.setTimeSampling(config.timesample_index);

  config.abc_uv_maps[uv_map_name] = param;
}

static void get_cols(const CDStreamConfig &config,
                     std::vector<Imath::C4f> &buffer,
                     std::vector<uint32_t> &uvidx,
                     const void *cd_data)
{
  const float cscale = 1.0f / 255.0f;
  const OffsetIndices faces = config.mesh->faces();
  const MCol *cfaces = static_cast<const MCol *>(cd_data);

  buffer.reserve(config.totvert);
  uvidx.reserve(config.totvert);

  Imath::C4f col;

  for (const int i : faces.index_range()) {
    const IndexRange face = faces[i];
    const MCol *cface = &cfaces[face.start() + face.size()];

    for (int j = 0; j < face.size(); j++) {
      cface--;

      col[0] = cface->a * cscale;
      col[1] = cface->r * cscale;
      col[2] = cface->g * cscale;
      col[3] = cface->b * cscale;

      buffer.push_back(col);
      uvidx.push_back(buffer.size() - 1);
    }
  }
}

/* Convention to write Vertex Colors:
 * - C3fGeomParam/C4fGeomParam on the arbGeomParam
 * - set scope as vertex varying
 */
static void write_mcol(const OCompoundProperty &prop,
                       CDStreamConfig &config,
                       const void *data,
                       const std::string &vcol_name)
{
  std::vector<uint32_t> indices;
  std::vector<Imath::C4f> buffer;

  get_cols(config, buffer, indices, data);

  if (indices.empty() || buffer.empty()) {
    return;
  }

  OC4fGeomParam param = config.abc_vertex_colors[vcol_name];

  if (!param.valid()) {
    param = OC4fGeomParam(prop, vcol_name, true, kFacevaryingScope, 1);
  }

  OC4fGeomParam::Sample sample(C4fArraySample(&buffer.front(), buffer.size()),
                               UInt32ArraySample(&indices.front(), indices.size()),
                               kVertexScope);

  param.set(sample);
  param.setTimeSampling(config.timesample_index);

  config.abc_vertex_colors[vcol_name] = param;
}

void write_generated_coordinates(const OCompoundProperty &prop, CDStreamConfig &config)
{
  Mesh *mesh = config.mesh;
  const void *customdata = CustomData_get_layer(&mesh->vert_data, CD_ORCO);
  if (customdata == nullptr) {
    /* Data not available, so don't even bother creating an Alembic property for it. */
    return;
  }
  const float (*orcodata)[3] = static_cast<const float (*)[3]>(customdata);

  /* Convert 3D vertices from float[3] z=up to V3f y=up. */
  std::vector<Imath::V3f> coords(config.totvert);
  float orco_yup[3];
  for (int vertex_idx = 0; vertex_idx < config.totvert; vertex_idx++) {
    copy_yup_from_zup(orco_yup, orcodata[vertex_idx]);
    coords[vertex_idx].setValue(orco_yup[0], orco_yup[1], orco_yup[2]);
  }

  /* ORCOs are always stored in the normalized 0..1 range in Blender, but Alembic stores them
   * unnormalized, so we need to unnormalize (invert transform) them. */
  BKE_mesh_orco_verts_transform(
      mesh, reinterpret_cast<float (*)[3]>(coords.data()), mesh->verts_num, true);

  if (!config.abc_orco.valid()) {
    /* Create the Alembic property and keep a reference so future frames can reuse it. */
    config.abc_orco = OV3fGeomParam(prop, propNameOriginalCoordinates, false, kVertexScope, 1);
  }

  OV3fGeomParam::Sample sample(coords, kVertexScope);
  config.abc_orco.set(sample);
}

void write_custom_data(const OCompoundProperty &prop,
                       CDStreamConfig &config,
                       const Mesh &mesh,
                       int data_type)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  if (data_type == CD_PROP_FLOAT2) {
    const StringRef active_uv_name = mesh.active_uv_map_name();
    for (const StringRefNull name : mesh.uv_map_names()) {
      if (name == active_uv_name) {
        /* Already exported. */
        continue;
      }
      const VArraySpan uv_map = *attributes.lookup<float2>(name, bke::AttrDomain::Corner);
      write_uv(prop, config, uv_map, get_valid_abc_name(name.c_str()));
    }
  }
  else if (data_type == CD_PROP_BYTE_COLOR) {
    mesh.attributes().foreach_attribute([&](const bke::AttributeIter &iter) {
      if (iter.data_type != bke::AttrType::ColorByte) {
        return;
      }
      if (iter.domain != bke::AttrDomain::Corner) {
        return;
      }
      const VArraySpan attr = *attributes.lookup<ColorGeometry4b>(iter.name,
                                                                  bke::AttrDomain::Corner);
      write_mcol(prop, config, attr.data(), get_valid_abc_name(iter.name.c_str()));
    });
  }
}

/* ************************************************************************** */

using Alembic::Abc::C3fArraySamplePtr;
using Alembic::Abc::C4fArraySamplePtr;
using Alembic::Abc::PropertyHeader;
using Alembic::Abc::UInt32ArraySamplePtr;

using Alembic::AbcGeom::IC3fGeomParam;
using Alembic::AbcGeom::IC4fGeomParam;
using Alembic::AbcGeom::IV2fGeomParam;
using Alembic::AbcGeom::IV3fGeomParam;

static void read_uvs(const CDStreamConfig &config,
                     void *data,
                     const AbcUvScope uv_scope,
                     const Alembic::AbcGeom::V2fArraySamplePtr &uvs,
                     const UInt32ArraySamplePtr &indices)
{
  const OffsetIndices faces = config.mesh->faces();
  const int *corner_verts = config.corner_verts;
  float2 *uv_map = static_cast<float2 *>(data);

  uint uv_index, loop_index, rev_loop_index;

  BLI_assert(uv_scope != ABC_UV_SCOPE_NONE);
  const bool do_uvs_per_loop = (uv_scope == ABC_UV_SCOPE_LOOP);

  for (const int i : faces.index_range()) {
    const IndexRange face = faces[i];
    uint rev_loop_offset = face.start() + face.size() - 1;

    for (int f = 0; f < face.size(); f++) {
      rev_loop_index = rev_loop_offset - f;
      loop_index = do_uvs_per_loop ? face.start() + f : corner_verts[rev_loop_index];
      uv_index = (*indices)[loop_index];
      const Imath::V2f &uv = (*uvs)[uv_index];

      float2 &loopuv = uv_map[rev_loop_index];
      loopuv[0] = uv[0];
      loopuv[1] = uv[1];
    }
  }
}

static size_t mcols_out_of_bounds_check(const size_t color_index,
                                        const size_t array_size,
                                        const std::string &iobject_full_name,
                                        const PropertyHeader &prop_header,
                                        bool &r_is_out_of_bounds,
                                        bool &r_bounds_warning_given)
{
  if (color_index < array_size) {
    return color_index;
  }

  if (!r_bounds_warning_given) {
    std::cerr << "Alembic: color index out of bounds "
                 "reading face colors for object "
              << iobject_full_name << ", property " << prop_header.getName() << std::endl;
    r_bounds_warning_given = true;
  }
  r_is_out_of_bounds = true;
  return 0;
}

static void read_custom_data_mcols(const std::string &iobject_full_name,
                                   const ICompoundProperty &arbGeomParams,
                                   const PropertyHeader &prop_header,
                                   const CDStreamConfig &config,
                                   const Alembic::Abc::ISampleSelector &iss)
{
  C3fArraySamplePtr c3f_ptr = C3fArraySamplePtr();
  C4fArraySamplePtr c4f_ptr = C4fArraySamplePtr();
  Alembic::Abc::UInt32ArraySamplePtr indices;
  bool use_c3f_ptr;
  bool is_facevarying;

  /* Find the correct interpretation of the data */
  if (IC3fGeomParam::matches(prop_header)) {
    IC3fGeomParam color_param(arbGeomParams, prop_header.getName());
    IC3fGeomParam::Sample sample;
    BLI_assert(STREQ("rgb", color_param.getInterpretation()));

    color_param.getIndexed(sample, iss);
    is_facevarying = sample.getScope() == kFacevaryingScope &&
                     config.totloop == sample.getIndices()->size();

    c3f_ptr = sample.getVals();
    indices = sample.getIndices();
    use_c3f_ptr = true;
  }
  else if (IC4fGeomParam::matches(prop_header)) {
    IC4fGeomParam color_param(arbGeomParams, prop_header.getName());
    IC4fGeomParam::Sample sample;
    BLI_assert(STREQ("rgba", color_param.getInterpretation()));

    color_param.getIndexed(sample, iss);
    is_facevarying = sample.getScope() == kFacevaryingScope &&
                     config.totloop == sample.getIndices()->size();

    c4f_ptr = sample.getVals();
    indices = sample.getIndices();
    use_c3f_ptr = false;
  }
  else {
    /* this won't happen due to the checks in read_custom_data() */
    return;
  }
  BLI_assert(c3f_ptr || c4f_ptr);

  /* Read the vertex colors */
  void *cd_data = config.add_customdata_cb(
      config.mesh, prop_header.getName().c_str(), CD_PROP_BYTE_COLOR);
  MCol *cfaces = static_cast<MCol *>(cd_data);
  const OffsetIndices faces = config.mesh->faces();
  const int *corner_verts = config.corner_verts;

  size_t face_index = 0;
  size_t color_index;
  bool bounds_warning_given = false;

  /* The colors can go through two layers of indexing. Often the 'indices'
   * array doesn't do anything (i.e. indices[n] = n), but when it does, it's
   * important. Blender 2.79 writes indices incorrectly (see #53745), which
   * is why we have to check for indices->size() > 0 */
  bool use_dual_indexing = is_facevarying && indices->size() > 0;

  for (const int i : faces.index_range()) {
    const IndexRange face = faces[i];
    MCol *cface = &cfaces[face.start() + face.size()];
    const int *face_verts = &corner_verts[face.start() + face.size()];

    for (int j = 0; j < face.size(); j++, face_index++) {
      cface--;
      face_verts--;

      color_index = is_facevarying ? face_index : *face_verts;
      if (use_dual_indexing) {
        color_index = (*indices)[color_index];
      }
      if (use_c3f_ptr) {
        bool is_mcols_out_of_bounds = false;
        color_index = mcols_out_of_bounds_check(color_index,
                                                c3f_ptr->size(),
                                                iobject_full_name,
                                                prop_header,
                                                is_mcols_out_of_bounds,
                                                bounds_warning_given);
        if (is_mcols_out_of_bounds) {
          continue;
        }
        const Imath::C3f &color = (*c3f_ptr)[color_index];
        cface->a = unit_float_to_uchar_clamp(color[0]);
        cface->r = unit_float_to_uchar_clamp(color[1]);
        cface->g = unit_float_to_uchar_clamp(color[2]);
        cface->b = 255;
      }
      else {
        bool is_mcols_out_of_bounds = false;
        color_index = mcols_out_of_bounds_check(color_index,
                                                c4f_ptr->size(),
                                                iobject_full_name,
                                                prop_header,
                                                is_mcols_out_of_bounds,
                                                bounds_warning_given);
        if (is_mcols_out_of_bounds) {
          continue;
        }
        const Imath::C4f &color = (*c4f_ptr)[color_index];
        cface->a = unit_float_to_uchar_clamp(color[0]);
        cface->r = unit_float_to_uchar_clamp(color[1]);
        cface->g = unit_float_to_uchar_clamp(color[2]);
        cface->b = unit_float_to_uchar_clamp(color[3]);
      }
    }
  }
}

static void read_custom_data_uvs(const ICompoundProperty &prop,
                                 const PropertyHeader &prop_header,
                                 const CDStreamConfig &config,
                                 const Alembic::Abc::ISampleSelector &iss)
{
  IV2fGeomParam uv_param(prop, prop_header.getName());

  if (!uv_param.isIndexed()) {
    return;
  }

  IV2fGeomParam::Sample sample;
  uv_param.getIndexed(sample, iss);

  UInt32ArraySamplePtr uvs_indices = sample.getIndices();

  const AbcUvScope uv_scope = get_uv_scope(uv_param.getScope(), config, uvs_indices);

  if (uv_scope == ABC_UV_SCOPE_NONE) {
    return;
  }

  void *cd_data = config.add_customdata_cb(
      config.mesh, prop_header.getName().c_str(), CD_PROP_FLOAT2);

  read_uvs(config, cd_data, uv_scope, sample.getVals(), uvs_indices);
}

void read_velocity(const V3fArraySamplePtr &velocities,
                   const CDStreamConfig &config,
                   const float velocity_scale)
{
  const int num_velocity_vectors = int(velocities->size());
  if (num_velocity_vectors != config.mesh->verts_num) {
    /* Files containing videogrammetry data may be malformed and export velocity data on missing
     * frames (most likely by copying the last valid data). */
    return;
  }

  bke::MutableAttributeAccessor attributes = config.mesh->attributes_for_write();
  bke::SpanAttributeWriter attr = attributes.lookup_or_add_for_write_span<float3>(
      "velocity", bke::AttrDomain::Point);
  MutableSpan<float3> velocity = attr.span;
  for (int i = 0; i < num_velocity_vectors; i++) {
    const Imath::V3f &vel_in = (*velocities)[i];
    copy_zup_from_yup(velocity[i], vel_in.getValue());
    mul_v3_fl(velocity[i], velocity_scale);
  }
  attr.finish();
}

void read_generated_coordinates(const ICompoundProperty &prop,
                                const CDStreamConfig &config,
                                const Alembic::Abc::ISampleSelector &iss)
{
  if (!prop.valid() || prop.getPropertyHeader(propNameOriginalCoordinates) == nullptr) {
    /* The ORCO property isn't there, so don't bother trying to process it. */
    return;
  }

  IV3fGeomParam param(prop, propNameOriginalCoordinates);
  if (!param.valid() || param.isIndexed()) {
    /* Invalid or indexed coordinates aren't supported. */
    return;
  }
  if (param.getScope() != kVertexScope) {
    /* These are original vertex coordinates, so must be vertex-scoped. */
    return;
  }

  IV3fGeomParam::Sample sample = param.getExpandedValue(iss);
  Alembic::AbcGeom::V3fArraySamplePtr abc_orco = sample.getVals();
  const size_t totvert = abc_orco.get()->size();
  Mesh *mesh = config.mesh;

  if (totvert != mesh->verts_num) {
    /* Either the data is somehow corrupted, or we have a dynamic simulation where only the ORCOs
     * for the first frame were exported. */
    return;
  }

  void *cd_data;
  if (CustomData_has_layer(&mesh->vert_data, CD_ORCO)) {
    cd_data = CustomData_get_layer_for_write(&mesh->vert_data, CD_ORCO, mesh->verts_num);
  }
  else {
    cd_data = CustomData_add_layer(&mesh->vert_data, CD_ORCO, CD_CONSTRUCT, totvert);
  }

  float (*orcodata)[3] = static_cast<float (*)[3]>(cd_data);
  for (int vertex_idx = 0; vertex_idx < totvert; ++vertex_idx) {
    const Imath::V3f &abc_coords = (*abc_orco)[vertex_idx];
    copy_zup_from_yup(orcodata[vertex_idx], abc_coords.getValue());
  }

  /* ORCOs are always stored in the normalized 0..1 range in Blender, but Alembic stores them
   * unnormalized, so we need to normalize them. */
  BKE_mesh_orco_verts_transform(mesh, orcodata, mesh->verts_num, false);
}

void read_custom_data(const std::string &iobject_full_name,
                      const ICompoundProperty &prop,
                      const CDStreamConfig &config,
                      const Alembic::Abc::ISampleSelector &iss)
{
  if (!prop.valid()) {
    return;
  }

  int num_uvs = 0;

  const size_t num_props = prop.getNumProperties();

  for (size_t i = 0; i < num_props; i++) {
    const Alembic::Abc::PropertyHeader &prop_header = prop.getPropertyHeader(i);

    /* Read UVs according to convention. */
    if (IV2fGeomParam::matches(prop_header) && Alembic::AbcGeom::isUV(prop_header)) {
      if (++num_uvs > MAX_MTFACE) {
        continue;
      }

      read_custom_data_uvs(prop, prop_header, config, iss);
      continue;
    }

    /* Read vertex colors according to convention. */
    if (IC3fGeomParam::matches(prop_header) || IC4fGeomParam::matches(prop_header)) {
      read_custom_data_mcols(iobject_full_name, prop, prop_header, config, iss);
      continue;
    }
  }
}

AbcUvScope get_uv_scope(const Alembic::AbcGeom::GeometryScope scope,
                        const CDStreamConfig &config,
                        const Alembic::AbcGeom::UInt32ArraySamplePtr &indices)
{
  if (scope == kFacevaryingScope && indices->size() == config.totloop) {
    return ABC_UV_SCOPE_LOOP;
  }

  /* kVaryingScope is sometimes used for vertex scopes as the values vary across the vertices. To
   * be sure, one has to check the size of the data against the number of vertices, as it could
   * also be a varying attribute across the faces (i.e. one value per face). */
  if (ELEM(scope, kVaryingScope, kVertexScope) && indices->size() == config.totvert) {
    return ABC_UV_SCOPE_VERTEX;
  }

  return ABC_UV_SCOPE_NONE;
}

}  // namespace blender::io::alembic
