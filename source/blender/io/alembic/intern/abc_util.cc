/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_util.h"

#include "abc_reader_camera.h"
#include "abc_reader_curves.h"
#include "abc_reader_mesh.h"
#include "abc_reader_points.h"
#include "abc_reader_transform.h"

#include <Alembic/AbcGeom/ILight.h>
#include <Alembic/AbcGeom/INuPatch.h>
#include <Alembic/AbcMaterial/IMaterial.h>

#include <algorithm>

using Alembic::Abc::IV3fArrayProperty;
using Alembic::Abc::PropertyHeader;
using Alembic::Abc::V3fArraySamplePtr;

namespace blender::io::alembic {

std::string get_valid_abc_name(const char *name)
{
  std::string abc_name(name);
  std::replace(abc_name.begin(), abc_name.end(), ' ', '_');
  std::replace(abc_name.begin(), abc_name.end(), '.', '_');
  std::replace(abc_name.begin(), abc_name.end(), ':', '_');
  std::replace(abc_name.begin(), abc_name.end(), '/', '_');
  return abc_name;
}

Imath::M44d convert_matrix_datatype(const float mat[4][4])
{
  Imath::M44d m;

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      m[i][j] = double(mat[i][j]);
    }
  }

  return m;
}

void convert_matrix_datatype(const Imath::M44d &xform, float r_mat[4][4])
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      r_mat[i][j] = float(xform[i][j]);
    }
  }
}

void split(const std::string &s, const char delim, std::vector<std::string> &tokens)
{
  tokens.clear();

  std::stringstream ss(s);
  std::string item;

  while (std::getline(ss, item, delim)) {
    if (!item.empty()) {
      tokens.push_back(item);
    }
  }
}

bool has_property(const Alembic::Abc::ICompoundProperty &prop, const std::string &name)
{
  if (!prop.valid()) {
    return false;
  }

  return prop.getPropertyHeader(name) != nullptr;
}

V3fArraySamplePtr get_velocity_prop(const Alembic::Abc::ICompoundProperty &schema,
                                    const Alembic::AbcGeom::ISampleSelector &selector,
                                    const std::string &name)
{
  for (size_t i = 0; i < schema.getNumProperties(); i++) {
    const PropertyHeader &header = schema.getPropertyHeader(i);

    if (header.isCompound()) {
      const Alembic::Abc::ICompoundProperty &prop = Alembic::Abc::ICompoundProperty(
          schema, header.getName());

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

using index_time_pair_t = std::pair<Alembic::AbcCoreAbstract::index_t, Alembic::AbcGeom::chrono_t>;

std::optional<SampleInterpolationSettings> get_sample_interpolation_settings(
    const Alembic::AbcGeom::ISampleSelector &selector,
    const Alembic::AbcCoreAbstract::TimeSamplingPtr &time_sampling,
    size_t samples_number)
{
  const chrono_t time = selector.getRequestedTime();
  samples_number = std::max(samples_number, size_t(1));

  index_time_pair_t t0 = time_sampling->getFloorIndex(time, samples_number);
  Alembic::AbcCoreAbstract::index_t i0 = t0.first;

  if (samples_number == 1 || (fabs(time - t0.second) < 0.0001)) {
    return {};
  }

  index_time_pair_t t1 = time_sampling->getCeilIndex(time, samples_number);
  Alembic::AbcCoreAbstract::index_t i1 = t1.first;

  if (i0 == i1) {
    return {};
  }

  const double bias = (time - t0.second) / (t1.second - t0.second);

  if (fabs(1.0 - bias) < 0.0001) {
    return {};
  }

  return SampleInterpolationSettings{i0, i1, bias};
}

// #define USE_NURBS

AbcObjectReader *create_reader(const Alembic::AbcGeom::IObject &object, ImportSettings &settings)
{
  AbcObjectReader *reader = nullptr;

  const Alembic::AbcGeom::MetaData &md = object.getMetaData();

  if (Alembic::AbcGeom::IXform::matches(md)) {
    reader = new AbcEmptyReader(object, settings);
  }
  else if (Alembic::AbcGeom::IPolyMesh::matches(md)) {
    reader = new AbcMeshReader(object, settings);
  }
  else if (Alembic::AbcGeom::ISubD::matches(md)) {
    reader = new AbcSubDReader(object, settings);
  }
  else if (Alembic::AbcGeom::INuPatch::matches(md)) {
#ifdef USE_NURBS
    /* TODO(kevin): importing cyclic NURBS from other software crashes
     * at the moment. This is due to the fact that NURBS in other
     * software have duplicated points which causes buffer overflows in
     * Blender. Need to figure out exactly how these points are
     * duplicated, in all cases (cyclic U, cyclic V, and cyclic UV).
     * Until this is fixed, disabling NURBS reading. */
    reader = new AbcNurbsReader(child, settings);
#endif
  }
  else if (Alembic::AbcGeom::ICamera::matches(md)) {
    reader = new AbcCameraReader(object, settings);
  }
  else if (Alembic::AbcGeom::IPoints::matches(md)) {
    reader = new AbcPointsReader(object, settings);
  }
  else if (Alembic::AbcMaterial::IMaterial::matches(md)) {
    /* Pass for now. */
  }
  else if (Alembic::AbcGeom::ILight::matches(md)) {
    /* Pass for now. */
  }
  else if (Alembic::AbcGeom::IFaceSet::matches(md)) {
    /* Pass, those are handled in the mesh reader. */
  }
  else if (Alembic::AbcGeom::ICurves::matches(md)) {
    reader = new AbcCurveReader(object, settings);
  }
  else {
    std::cerr << "Alembic: unknown how to handle objects of schema '" << md.get("schemaObjTitle")
              << "', skipping object '" << object.getFullName() << "'" << std::endl;
  }

  return reader;
}

}  // namespace blender::io::alembic
