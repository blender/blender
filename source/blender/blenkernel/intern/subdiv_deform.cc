/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv_deform.hh"

#include "DNA_mesh_types.h"

#include "BLI_task.hh"

#include "BKE_mesh.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_eval.hh"

namespace blender::bke::subdiv {

/* -------------------------------------------------------------------- */
/** \name Evaluation
 * \{ */

struct PtexCoord {
  int ptex_face_index;
  float2 uv;
};

/** The ptex face coordinate of a vertex in a base mesh face. */
static PtexCoord face_vert_ptex_coord(const IndexRange face,
                                      const int face_ptex_offset,
                                      const Span<int> corner_verts,
                                      const int vert)
{
  const int corner_in_face = mesh::face_find_corner_from_vert(face, corner_verts, vert) -
                             face.start();
  if (face.size() == 4) {
    static constexpr float2 quad_uvs[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    return {face_ptex_offset, quad_uvs[corner_in_face]};
  }
  return {face_ptex_offset + corner_in_face, float2(0.0f)};
}

static float3 eval_displacement_at_ptex_coord(Subdiv *subdiv, const PtexCoord &coord)
{
  float3 dummy_P;
  float3 dPdu;
  float3 dPdv;
  float3 D;
  const float u = coord.uv.x;
  const float v = coord.uv.y;
  eval_limit_point_and_derivatives(subdiv, coord.ptex_face_index, u, v, dummy_P, dPdu, dPdv);
  eval_displacement(subdiv, coord.ptex_face_index, u, v, dPdu, dPdv, D);
  return D;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public entry point
 * \{ */

void deform_coarse_vertices(Subdiv *subdiv,
                            const Mesh *coarse_mesh,
                            MutableSpan<float3> vert_positions)
{
  stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
  /* Make sure evaluator is up to date with possible new topology, and that
   * is refined for the new positions of coarse vertices. */
  if (!eval_begin_from_mesh(subdiv, coarse_mesh, SUBDIV_EVALUATOR_TYPE_CPU, vert_positions)) {
    /* This could happen in two situations:
     * - OpenSubdiv is disabled.
     * - Something totally bad happened, and OpenSubdiv rejected our
     *   topology.
     * In either way, we can't safely continue. */
    if (coarse_mesh->faces_num) {
      stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
      return;
    }
  }

  const OffsetIndices faces = coarse_mesh->faces();
  const Span<int> corner_verts = coarse_mesh->corner_verts();
  const GroupedSpan<int> vert_to_face = coarse_mesh->vert_to_face_map();
  const Span<int> face_ptex_offset = face_ptex_offset_get(subdiv);
  const bool have_displacement = subdiv->displacement_evaluator != nullptr;

  stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);
  threading::parallel_for(vert_positions.index_range(), 512, [&](const IndexRange range) {
    for (const int vert : range) {
      const Span<int> vert_faces = vert_to_face[vert];
      if (vert_faces.is_empty()) {
        /* Vertices that aren't used by faces are not deformed. */
        continue;
      }
      const auto ptex_coord = [&](const int face) {
        return face_vert_ptex_coord(faces[face], face_ptex_offset[face], corner_verts, vert);
      };
      /* Average the displacement from all faces using the vertex. */
      float3 D(0.0f);
      if (have_displacement) {
        for (const int face : vert_faces) {
          D += eval_displacement_at_ptex_coord(subdiv, ptex_coord(face));
        }
        D *= 1.0f / float(vert_faces.size());
      }
      /* Evaluate the limit position from the first face using the vertex. */
      const PtexCoord coord = ptex_coord(vert_faces.first());
      vert_positions[vert] = eval_limit_point(
                                 subdiv, coord.ptex_face_index, coord.uv.x, coord.uv.y) +
                             D;
    }
  });
  stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);

  stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
}

/** \} */

}  // namespace blender::bke::subdiv
