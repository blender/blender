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
 *
 * The Original Code is Copyright (C) 2016 Kévin Dietrich.
 * All rights reserved.
 */

/** \file
 * \ingroup balembic
 */

#include "abc_customdata.h"

#include <Alembic/AbcGeom/All.h>
#include <algorithm>
#include <unordered_map>

#include "DNA_customdata_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"

/* NOTE: for now only UVs and Vertex Colors are supported for streaming.
 * Although Alembic only allows for a single UV layer per {I|O}Schema, and does
 * not have a vertex color concept, there is a convention between DCCs to write
 * such data in a way that lets other DCC know what they are for. See comments
 * in the write code for the conventions. */

using Alembic::AbcGeom::kFacevaryingScope;
using Alembic::AbcGeom::kVertexScope;

using Alembic::Abc::C4fArraySample;
using Alembic::Abc::UInt32ArraySample;
using Alembic::Abc::V2fArraySample;

using Alembic::AbcGeom::OC4fGeomParam;
using Alembic::AbcGeom::OV2fGeomParam;
namespace blender {
namespace io {
namespace alembic {

static void get_uvs(const CDStreamConfig &config,
                    std::vector<Imath::V2f> &uvs,
                    std::vector<uint32_t> &uvidx,
                    void *cd_data)
{
  MLoopUV *mloopuv_array = static_cast<MLoopUV *>(cd_data);

  if (!mloopuv_array) {
    return;
  }

  const int num_poly = config.totpoly;
  MPoly *polygons = config.mpoly;
  MLoop *mloop = config.mloop;

  if (!config.pack_uvs) {
    int cnt = 0;
    uvidx.resize(config.totloop);
    uvs.resize(config.totloop);

    /* Iterate in reverse order to match exported polygons. */
    for (int i = 0; i < num_poly; i++) {
      MPoly &current_poly = polygons[i];
      MLoopUV *loopuv = mloopuv_array + current_poly.loopstart + current_poly.totloop;

      for (int j = 0; j < current_poly.totloop; j++, cnt++) {
        loopuv--;

        uvidx[cnt] = cnt;
        uvs[cnt][0] = loopuv->uv[0];
        uvs[cnt][1] = loopuv->uv[1];
      }
    }
  }
  else {
    /* Mapping for indexed UVs, deduplicating UV coordinates at vertices. */
    std::vector<std::vector<uint32_t>> idx_map(config.totvert);
    int idx_count = 0;

    for (int i = 0; i < num_poly; i++) {
      MPoly &current_poly = polygons[i];
      MLoop *looppoly = mloop + current_poly.loopstart + current_poly.totloop;
      MLoopUV *loopuv = mloopuv_array + current_poly.loopstart + current_poly.totloop;

      for (int j = 0; j < current_poly.totloop; j++) {
        looppoly--;
        loopuv--;

        Imath::V2f uv(loopuv->uv[0], loopuv->uv[1]);
        bool found_same = false;

        /* Find UV already in uvs array. */
        for (uint32_t uv_idx : idx_map[looppoly->v]) {
          if (uvs[uv_idx] == uv) {
            found_same = true;
            uvidx.push_back(uv_idx);
            break;
          }
        }

        /* UV doesn't exists for this vertex, add it. */
        if (!found_same) {
          uint32_t uv_idx = idx_count++;
          idx_map[looppoly->v].push_back(uv_idx);
          uvidx.push_back(uv_idx);
          uvs.push_back(uv);
        }
      }
    }
  }
}

const char *get_uv_sample(UVSample &sample, const CDStreamConfig &config, CustomData *data)
{
  const int active_uvlayer = CustomData_get_active_layer(data, CD_MLOOPUV);

  if (active_uvlayer < 0) {
    return "";
  }

  void *cd_data = CustomData_get_layer_n(data, CD_MLOOPUV, active_uvlayer);

  get_uvs(config, sample.uvs, sample.indices, cd_data);

  return CustomData_get_layer_name(data, CD_MLOOPUV, active_uvlayer);
}

/* Convention to write UVs:
 * - V2fGeomParam on the arbGeomParam
 * - set scope as face varying
 * - (optional due to its behavior) tag as UV using Alembic::AbcGeom::SetIsUV
 */
static void write_uv(const OCompoundProperty &prop,
                     CDStreamConfig &config,
                     void *data,
                     const char *name)
{
  std::vector<uint32_t> indices;
  std::vector<Imath::V2f> uvs;

  get_uvs(config, uvs, indices, data);

  if (indices.empty() || uvs.empty()) {
    return;
  }

  std::string uv_map_name(name);
  OV2fGeomParam param = config.abc_uv_maps[uv_map_name];

  if (!param.valid()) {
    param = OV2fGeomParam(prop, name, true, kFacevaryingScope, 1);
  }
  OV2fGeomParam::Sample sample(V2fArraySample(&uvs.front(), uvs.size()),
                               UInt32ArraySample(&indices.front(), indices.size()),
                               kFacevaryingScope);
  param.set(sample);

  config.abc_uv_maps[uv_map_name] = param;
}

/* Convention to write Vertex Colors:
 * - C3fGeomParam/C4fGeomParam on the arbGeomParam
 * - set scope as vertex varying
 */
static void write_mcol(const OCompoundProperty &prop,
                       const CDStreamConfig &config,
                       void *data,
                       const char *name)
{
  const float cscale = 1.0f / 255.0f;
  MPoly *polys = config.mpoly;
  MLoop *mloops = config.mloop;
  MCol *cfaces = static_cast<MCol *>(data);

  std::vector<Imath::C4f> buffer;
  std::vector<uint32_t> indices;

  buffer.reserve(config.totvert);
  indices.reserve(config.totvert);

  Imath::C4f col;

  for (int i = 0; i < config.totpoly; i++) {
    MPoly *p = &polys[i];
    MCol *cface = &cfaces[p->loopstart + p->totloop];
    MLoop *mloop = &mloops[p->loopstart + p->totloop];

    for (int j = 0; j < p->totloop; j++) {
      cface--;
      mloop--;

      col[0] = cface->a * cscale;
      col[1] = cface->r * cscale;
      col[2] = cface->g * cscale;
      col[3] = cface->b * cscale;

      buffer.push_back(col);
      indices.push_back(buffer.size() - 1);
    }
  }

  OC4fGeomParam param(prop, name, true, kFacevaryingScope, 1);

  OC4fGeomParam::Sample sample(C4fArraySample(&buffer.front(), buffer.size()),
                               UInt32ArraySample(&indices.front(), indices.size()),
                               kVertexScope);

  param.set(sample);
}

void write_custom_data(const OCompoundProperty &prop,
                       CDStreamConfig &config,
                       CustomData *data,
                       int data_type)
{
  CustomDataType cd_data_type = static_cast<CustomDataType>(data_type);

  if (!CustomData_has_layer(data, cd_data_type)) {
    return;
  }

  const int active_layer = CustomData_get_active_layer(data, cd_data_type);
  const int tot_layers = CustomData_number_of_layers(data, cd_data_type);

  for (int i = 0; i < tot_layers; i++) {
    void *cd_data = CustomData_get_layer_n(data, cd_data_type, i);
    const char *name = CustomData_get_layer_name(data, cd_data_type, i);

    if (cd_data_type == CD_MLOOPUV) {
      /* Already exported. */
      if (i == active_layer) {
        continue;
      }

      write_uv(prop, config, cd_data, name);
    }
    else if (cd_data_type == CD_MLOOPCOL) {
      write_mcol(prop, config, cd_data, name);
    }
  }
}

/* ************************************************************************** */

using Alembic::Abc::C3fArraySamplePtr;
using Alembic::Abc::C4fArraySamplePtr;
using Alembic::Abc::PropertyHeader;

using Alembic::AbcGeom::IC3fGeomParam;
using Alembic::AbcGeom::IC4fGeomParam;
using Alembic::AbcGeom::IV2fGeomParam;

static void read_uvs(const CDStreamConfig &config,
                     void *data,
                     const Alembic::AbcGeom::V2fArraySamplePtr &uvs,
                     const Alembic::AbcGeom::UInt32ArraySamplePtr &indices)
{
  MPoly *mpolys = config.mpoly;
  MLoopUV *mloopuvs = static_cast<MLoopUV *>(data);

  unsigned int uv_index, loop_index, rev_loop_index;

  for (int i = 0; i < config.totpoly; i++) {
    MPoly &poly = mpolys[i];
    unsigned int rev_loop_offset = poly.loopstart + poly.totloop - 1;

    for (int f = 0; f < poly.totloop; f++) {
      loop_index = poly.loopstart + f;
      rev_loop_index = rev_loop_offset - f;
      uv_index = (*indices)[loop_index];
      const Imath::V2f &uv = (*uvs)[uv_index];

      MLoopUV &loopuv = mloopuvs[rev_loop_index];
      loopuv.uv[0] = uv[0];
      loopuv.uv[1] = uv[1];
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
    BLI_assert(!strcmp("rgb", color_param.getInterpretation()));

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
    BLI_assert(!strcmp("rgba", color_param.getInterpretation()));

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
      config.mesh, prop_header.getName().c_str(), CD_MLOOPCOL);
  MCol *cfaces = static_cast<MCol *>(cd_data);
  MPoly *mpolys = config.mpoly;
  MLoop *mloops = config.mloop;

  size_t face_index = 0;
  size_t color_index;
  bool bounds_warning_given = false;

  /* The colors can go through two layers of indexing. Often the 'indices'
   * array doesn't do anything (i.e. indices[n] = n), but when it does, it's
   * important. Blender 2.79 writes indices incorrectly (see T53745), which
   * is why we have to check for indices->size() > 0 */
  bool use_dual_indexing = is_facevarying && indices->size() > 0;

  for (int i = 0; i < config.totpoly; i++) {
    MPoly *poly = &mpolys[i];
    MCol *cface = &cfaces[poly->loopstart + poly->totloop];
    MLoop *mloop = &mloops[poly->loopstart + poly->totloop];

    for (int j = 0; j < poly->totloop; j++, face_index++) {
      cface--;
      mloop--;

      color_index = is_facevarying ? face_index : mloop->v;
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

  if (uv_param.getScope() != kFacevaryingScope) {
    return;
  }

  void *cd_data = config.add_customdata_cb(config.mesh, prop_header.getName().c_str(), CD_MLOOPUV);

  read_uvs(config, cd_data, sample.getVals(), sample.getIndices());
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
  int num_colors = 0;

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
      if (++num_colors > MAX_MCOL) {
        continue;
      }

      read_custom_data_mcols(iobject_full_name, prop, prop_header, config, iss);
      continue;
    }
  }
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
