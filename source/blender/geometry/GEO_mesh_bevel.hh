/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once
#include <optional>

#include "BLI_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_vector_types.hh"

#include "BKE_attribute.hh"

struct MDeformVert;
struct Mesh;

namespace blender::geometry {

/**
 * \warning Each of following enum values  are saved in files.
 * Currently, they also need to be in sync with values in `bmesh_opdefines.h`
 * and `DNA_modifier_types.h`.
 */
enum class BevelAffect {
  Vertices = 0,
  Edges = 1,
  /* TODO: Implement Face Bevel. */
  // Faces = 2,
};

enum class BevelMiterType {
  Sharp = 0,
  Patch = 1,
  Arc = 2,
};

struct BevelAttributeOutputs {
  std::optional<std::string> vertex_face_id;
  std::optional<std::string> edge_face_id;
  std::optional<std::string> outer_edge_id;
  std::optional<std::string> mid_edge_id;
};

struct BevelParameters {
  int segments = 1;
  /** Profile shape parameter.  */
  float shape = 0.5f;
  /** Bevel vertices only or edges. */
  BevelAffect affect_type = BevelAffect::Edges;
  /** Pre-sampled custom profile points, if non-empty.
   * Each element is a (x, y) coordinate in the unit square [0,1]x[0,1],
   * sampled at even arc-length intervals from the input curve.
   * The array has #segments + 1 entries (inclusive of both endpoints).
   * Endpoint values (index 0 and last) are always (1,0) and (0,1) so that
   * they align with the bevel strip corners, matching the #CurveProfile convention. */
  Array<float2> custom_profile_samples;
  /** Blender units to offset each end of each edge.
   * A 4d Array of Arrays indexed by mesh edge id.
   * If affect_type is Edges or Faces, these are in order: source end (left, right), destination
   * end (left, right), viewed from the source end looking towards the destination end,
   * standing on the normal positive side.
   * If affect_type is Vertices, these are the amounts to move along each edge
   * from the vertex, and only the first and third values are used.
   */
  std::array<Array<float>, 4> offsets;
  /** Per corner bool saying whether or not to miter at that corner. */
  Array<bool> miter;
  /** Per corner float saying how much to spread arc miters. */
  Array<float> spread;
  /** Which output attributes are needed.*/
  geometry::BevelAttributeOutputs attribute_outputs;
};

/**
 * \return #std::nullopt if the mesh is not changed (when every selected face is already a
 * triangle).
 */
std::optional<Mesh *> mesh_bevel(const Mesh &src_mesh,
                                 const IndexMask &selection,
                                 const BevelParameters &params,
                                 const bke::AttributeFilter &attribute_filter);

}  // namespace blender::geometry
