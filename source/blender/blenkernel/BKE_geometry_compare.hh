/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.hh"
#include "BKE_mesh_types.hh"

#include "DNA_lattice_types.h"

/** \file
 * \ingroup bke
 */

namespace blender::bke::compare_geometry {

enum class GeoMismatch : int8_t;

/**
 * Convert the mismatch to a human-readable string for display.
 */
const char *mismatch_to_string(const GeoMismatch &mismatch);

/**
 * \brief Checks if the two meshes are different, returning the type of mismatch if any. Changes in
 * index order are detected, but treated as a mismatch.
 *
 * \details Instead of just blindly comparing the two meshes, the code tries to determine if they
 * are isomorphic. Two meshes are considered isomorphic, if, for each domain, there is a bijection
 * between the two meshes such that the bijections preserve connectivity.
 *
 * In general, determining if two graphs are isomorphic is a very difficult problem (no polynomial
 * time algorithm is known). Because we have more information than just connectivity (attributes),
 * we can compute it in a more reasonable time in most cases.
 *
 * \returns The type of mismatch that was detected, if there is any.
 */
std::optional<GeoMismatch> compare_meshes(const Mesh &mesh1, const Mesh &mesh2, float threshold);

/**
 * \brief Checks if the two curves geometries are different, returning the type of mismatch if any.
 * Changes in index order are detected, but treated as a mismatch.
 *
 * \returns The type of mismatch that was detected, if there is any.
 */
std::optional<GeoMismatch> compare_curves(const CurvesGeometry &curves1,
                                          const CurvesGeometry &curves2,
                                          float threshold);

/**
 * \brief Checks if the two lattices are different, returning the type of mismatch if any.
 *
 * \returns The type of mismatch that was detected, if there is any.
 */
std::optional<GeoMismatch> compare_lattices(const Lattice &lattice1,
                                            const Lattice &lattice2,
                                            float threshold);

}  // namespace blender::bke::compare_geometry
