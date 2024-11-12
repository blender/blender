/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief Utility functions for vertex groups in grease pencil objects
 */

#include "DNA_grease_pencil_types.h"

#include "BKE_attribute.hh"

namespace blender::bke::greasepencil {

/** Make sure drawings only contain vertex groups of the #GreasePencil. */
void validate_drawing_vertex_groups(GreasePencil &grease_pencil);

/** Find or create a vertex group in a drawing. */
int ensure_vertex_group(const StringRef name, ListBase &vertex_group_names);

/** Assign selected vertices to the vertex group. */
void assign_to_vertex_group(Drawing &drawing, StringRef name, float weight);

void assign_to_vertex_group_from_mask(CurvesGeometry &curves,
                                      const IndexMask &mask,
                                      StringRef name,
                                      float weight);

/**
 * Remove selected vertices from the vertex group.
 * \return True if at least one vertex was removed from the group.
 */
bool remove_from_vertex_group(Drawing &drawing, StringRef name, bool use_selection);

/** Remove vertices from all vertex groups. */
void clear_vertex_groups(GreasePencil &grease_pencil);

/** Select or deselect vertices assigned to this group. */
void select_from_group(Drawing &drawing,
                       const AttrDomain selection_domain,
                       StringRef name,
                       bool select);

}  // namespace blender::bke::greasepencil
