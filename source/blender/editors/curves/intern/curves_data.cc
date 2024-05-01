/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "DNA_object_types.h"

#include "ED_curves.hh"
#include "ED_transverts.hh"

namespace blender::ed::curves {

void transverts_from_curves_positions_create(bke::CurvesGeometry &curves, TransVertStore *tvs)
{
  IndexMaskMemory memory;
  IndexMask selection = retrieve_selected_points(curves, memory);
  MutableSpan<float3> positions = curves.positions_for_write();

  tvs->transverts = static_cast<TransVert *>(
      MEM_calloc_arrayN(selection.size(), sizeof(TransVert), __func__));
  tvs->transverts_tot = selection.size();

  selection.foreach_index(GrainSize(1024), [&](const int64_t i, const int64_t pos) {
    TransVert &tv = tvs->transverts[pos];
    tv.loc = positions[i];
    tv.flag = SELECT;
    copy_v3_v3(tv.oldloc, tv.loc);
  });
}

float (*point_normals_array_create(const Curves *curves_id))[3]
{
  using namespace blender;
  const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  const int size = curves.points_num();
  float3 *data = static_cast<float3 *>(MEM_malloc_arrayN(size, sizeof(float3), __func__));
  bke::curves_normals_point_domain_calc(curves, {data, size});
  return reinterpret_cast<float(*)[3]>(data);
}

}  // namespace blender::ed::curves
