/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "paint_intern.hh"
#include "sculpt_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Sculpt Flood Fill API
 *
 * Iterate over connected vertices, starting from one or more initial vertices.
 * \{ */

namespace blender::ed::sculpt_paint {

namespace flood_fill {

FillData init_fill(SculptSession &ss)
{
  SCULPT_vertex_random_access_ensure(ss);
  FillData data;
  data.visited_verts.resize(SCULPT_vertex_count_get(ss));
  return data;
}

void add_initial(FillData &flood, PBVHVertRef vertex)
{
  flood.queue.push(vertex);
}

void add_and_skip_initial(FillData &flood, PBVHVertRef vertex)
{
  flood.queue.push(vertex);
  flood.visited_verts[vertex.i].set(vertex.i);
}

void add_initial_with_symmetry(
    const Object &ob, const SculptSession &ss, FillData &flood, PBVHVertRef vertex, float radius)
{
  /* Add active vertex and symmetric vertices to the queue. */
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char i = 0; i <= symm; ++i) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }
    PBVHVertRef v = {PBVH_REF_NONE};

    if (i == 0) {
      v = vertex;
    }
    else if (radius > 0.0f) {
      float radius_squared = (radius == FLT_MAX) ? FLT_MAX : radius * radius;
      float location[3];
      flip_v3_v3(location, SCULPT_vertex_co_get(ss, vertex), ePaintSymmetryFlags(i));
      v = nearest_vert_calc(ob, location, radius_squared, false);
    }

    if (v.i != PBVH_REF_NONE) {
      add_initial(flood, v);
    }
  }
}

void add_active(const Object &ob, const SculptSession &ss, FillData &flood, float radius)
{
  /* Add active vertex and symmetric vertices to the queue. */
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char i = 0; i <= symm; ++i) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }

    PBVHVertRef v = {PBVH_REF_NONE};

    if (i == 0) {
      v = SCULPT_active_vertex_get(ss);
    }
    else if (radius > 0.0f) {
      float location[3];
      flip_v3_v3(location, SCULPT_active_vertex_co_get(ss), ePaintSymmetryFlags(i));
      v = nearest_vert_calc(ob, location, radius, false);
    }

    if (v.i != PBVH_REF_NONE) {
      add_initial(flood, v);
    }
  }
}

void execute(SculptSession &ss,
             FillData &flood,
             FunctionRef<bool(PBVHVertRef from_v, PBVHVertRef to_v, bool is_duplicate)> func)
{
  while (!flood.queue.empty()) {
    PBVHVertRef from_v = flood.queue.front();
    flood.queue.pop();

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_DUPLICATES_AND_NEIGHBORS_ITER_BEGIN (ss, from_v, ni) {
      const PBVHVertRef to_v = ni.vertex;
      int to_v_i = BKE_pbvh_vertex_to_index(*ss.pbvh, to_v);

      if (flood.visited_verts[to_v_i]) {
        continue;
      }

      if (!hide::vert_visible_get(ss, to_v)) {
        continue;
      }

      flood.visited_verts[BKE_pbvh_vertex_to_index(*ss.pbvh, to_v)].set();

      if (func(from_v, to_v, ni.is_duplicate)) {
        flood.queue.push(to_v);
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }
}

}  // namespace flood_fill

}  // namespace blender::ed::sculpt_paint

/** \} */
