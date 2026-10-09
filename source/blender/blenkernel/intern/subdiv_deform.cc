/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv_deform.hh"

#include "DNA_mesh_types.h"

#include "BLI_array_utils.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "BKE_mesh.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_eval.hh"
#include "BKE_subdiv_mesh_layout.hh"

namespace blender::bke::subdiv {

/* -------------------------------------------------------------------- */
/** \name Evaluation
 * \{ */

/** The ptex face coordinate of a vertex in a base mesh face. */
static PtexCoord face_vert_ptex_coord(const IndexRange face,
                                      const int face_ptex_offset,
                                      const Span<int> corner_verts,
                                      const int vert)
{
  const int corner = mesh::face_find_corner_from_vert(face, corner_verts, vert);
  return face_corner_ptex_coord(face, face_ptex_offset, corner - face.start());
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
    /* The vertices used by faces, which are evaluated together. */
    Vector<int> verts;
    Vector<PtexCoord> coords;
    /* The coordinates of each vertex in every face using it, for averaging displacement. */
    Vector<int> displacement_offsets = {0};
    Vector<PtexCoord> displacement_coords;
    for (const int vert : range) {
      const Span<int> vert_faces = vert_to_face[vert];
      if (vert_faces.is_empty()) {
        /* Vertices that aren't used by faces are not deformed. */
        continue;
      }
      const auto ptex_coord = [&](const int face) {
        return face_vert_ptex_coord(faces[face], face_ptex_offset[face], corner_verts, vert);
      };
      /* Evaluate the limit position from the first face using the vertex. */
      verts.append(vert);
      coords.append(ptex_coord(vert_faces.first()));
      if (have_displacement) {
        for (const int face : vert_faces) {
          displacement_coords.append(ptex_coord(face));
        }
        displacement_offsets.append(displacement_coords.size());
      }
    }

    Array<float3> positions(coords.size());
    eval_limit_points(subdiv, coords, positions);
    if (have_displacement) {
      Array<float3> displacement(coords.size());
      eval_displacement_averaged(
          subdiv, displacement_coords, displacement_offsets.as_span(), displacement);
      for (const int i : positions.index_range()) {
        positions[i] += displacement[i];
      }
    }
    array_utils::scatter(positions.as_span(), verts.as_span(), vert_positions, exec_mode::serial);
  });
  stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);

  stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
}

/** \} */

}  // namespace blender::bke::subdiv
