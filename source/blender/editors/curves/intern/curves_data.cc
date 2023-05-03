/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_geometry_fields.hh"

#include "BLI_task.hh"

#include "DNA_object_types.h"

#include "ED_curves.h"
#include "ED_transverts.h"

namespace blender::ed::curves {

void transverts_from_curves_positions_create(bke::CurvesGeometry &curves, TransVertStore *tvs)
{
  Vector<int64_t> selected_indices;
  IndexMask selection = retrieve_selected_points(curves, selected_indices);
  MutableSpan<float3> positions = curves.positions_for_write();

  tvs->transverts = static_cast<TransVert *>(
      MEM_calloc_arrayN(selection.size(), sizeof(TransVert), __func__));
  tvs->transverts_tot = selection.size();

  threading::parallel_for(selection.index_range(), 1024, [&](const IndexRange selection_range) {
    for (const int point_i : selection_range) {
      TransVert &tv = tvs->transverts[point_i];
      tv.loc = positions[selection[point_i]];
      tv.flag = SELECT;
      copy_v3_v3(tv.oldloc, tv.loc);
    }
  });
}

}  // namespace blender::ed::curves

float (*ED_curves_point_normals_array_create(const Curves *curves_id))[3]
{
  using namespace blender;
  const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  const int size = curves.points_num();

  float3 *data = static_cast<float3 *>(MEM_malloc_arrayN(size, sizeof(float3), __func__));

  const bke::CurvesFieldContext context(curves, ATTR_DOMAIN_POINT);
  fn::FieldEvaluator evaluator(context, size);
  fn::Field<float3> field(std::make_shared<bke::NormalFieldInput>());
  evaluator.add_with_destination(std::move(field), {data, size});
  evaluator.evaluate();

  return reinterpret_cast<float(*)[3]>(data);
}

void ED_curves_transverts_create(Curves *curves_id, TransVertStore *tvs)
{
  using namespace blender;
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  ed::curves::transverts_from_curves_positions_create(curves, tvs);
}

int *ED_curves_offsets_for_write(Curves *curves_id)
{
  return curves_id->geometry.wrap().offsets_for_write().data();
}
