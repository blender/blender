/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_geometry_fields.hh"

#include "ED_curves.h"

float (*ED_curves_point_normals_array_create(const Curves *curves_id))[3]
{
  using namespace blender;
  const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id->geometry);
  const int size = curves.points_num();

  float3 *data = static_cast<float3 *>(MEM_malloc_arrayN(size, sizeof(float3), __func__));

  const bke::CurvesFieldContext context(curves, ATTR_DOMAIN_POINT);
  fn::FieldEvaluator evaluator(context, size);
  fn::Field<float3> field(std::make_shared<bke::NormalFieldInput>());
  evaluator.add_with_destination(std::move(field), {data, size});
  evaluator.evaluate();

  return reinterpret_cast<float(*)[3]>(data);
}
