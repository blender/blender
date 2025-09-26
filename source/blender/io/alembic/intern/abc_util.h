/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "abc_reader_object.h"

#include <Alembic/Abc/Foundation.h>
#include <Alembic/Abc/ICompoundProperty.h>
#include <Alembic/Abc/IObject.h>
#include <Alembic/Abc/ISampleSelector.h>
#include <Alembic/Abc/TypedArraySample.h>
#include <Alembic/AbcCoreAbstract/Foundation.h>
#include <Alembic/AbcCoreAbstract/TimeSampling.h>
#include <Alembic/AbcGeom/IXform.h>

#include <optional>
#include <string>
#include <vector>

using Alembic::Abc::chrono_t;
using Alembic::Abc::V3fArraySamplePtr;

struct ID;
struct Object;

namespace blender::io::alembic {

class AbcObjectReader;
struct ImportSettings;

std::string get_valid_abc_name(const char *name);

/* Convert from float to Alembic matrix representations. Does NOT convert from Z-up to Y-up. */
Imath::M44d convert_matrix_datatype(const float mat[4][4]);
/* Convert from Alembic to float matrix representations. Does NOT convert from Y-up to Z-up. */
void convert_matrix_datatype(const Imath::M44d &xform, float r_mat[4][4]);

void split(const std::string &s, char delim, std::vector<std::string> &tokens);

template<class TContainer> bool begins_with(const TContainer &input, const TContainer &match)
{
  return input.size() >= match.size() && std::equal(match.begin(), match.end(), input.begin());
}

template<typename Schema>
void get_min_max_time_ex(const Schema &schema, chrono_t &min, chrono_t &max)
{
  const Alembic::Abc::TimeSamplingPtr &time_samp = schema.getTimeSampling();

  if (!schema.isConstant()) {
    const size_t num_samps = schema.getNumSamples();

    if (num_samps > 0) {
      const chrono_t min_time = time_samp->getSampleTime(0);
      min = std::min(min, min_time);

      const chrono_t max_time = time_samp->getSampleTime(num_samps - 1);
      max = std::max(max, max_time);
    }
  }
}

template<typename Schema>
void get_min_max_time(const Alembic::AbcGeom::IObject &object,
                      const Schema &schema,
                      chrono_t &min,
                      chrono_t &max)
{
  get_min_max_time_ex(schema, min, max);

  const Alembic::AbcGeom::IObject &parent = object.getParent();
  if (parent.valid() && Alembic::AbcGeom::IXform::matches(parent.getMetaData())) {
    Alembic::AbcGeom::IXform xform(parent, Alembic::AbcGeom::kWrapExisting);
    get_min_max_time_ex(xform.getSchema(), min, max);
  }
}

bool has_property(const Alembic::Abc::ICompoundProperty &prop, const std::string &name);
V3fArraySamplePtr get_velocity_prop(const Alembic::Abc::ICompoundProperty &schema,
                                    const Alembic::AbcGeom::ISampleSelector &selector,
                                    const std::string &name);

/**
 * The SampleInterpolationSettings struct holds information for interpolating data between two
 * samples.
 */
struct SampleInterpolationSettings {
  /* Index of the first ("floor") sample. */
  Alembic::AbcGeom::index_t index;
  /* Index of the second ("ceil") sample. */
  Alembic::AbcGeom::index_t ceil_index;
  /* Factor to interpolate between the `index` and `ceil_index`. */
  double weight;
};

/**
 * Check whether the requested time from the \a selector falls between two sampling time from the
 * \a time_sampling. If so, returns a #SampleInterpolationSettings with the required data to
 * interpolate. If not, returns nothing and we can assume that the requested time falls on a
 * specific sampling time of \a time_sampling and no interpolation is necessary.
 */
std::optional<SampleInterpolationSettings> get_sample_interpolation_settings(
    const Alembic::AbcGeom::ISampleSelector &selector,
    const Alembic::AbcCoreAbstract::TimeSamplingPtr &time_sampling,
    size_t samples_number);

AbcObjectReader *create_reader(const Alembic::AbcGeom::IObject &object, ImportSettings &settings);

}  // namespace blender::io::alembic
