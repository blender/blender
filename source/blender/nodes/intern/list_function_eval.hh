/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "FN_multi_function.hh"

#include "NOD_geometry_exec.hh"

namespace blender::nodes {

void execute_multi_function_on_value_variant__list(const MultiFunction &fn,
                                                   const Span<SocketValueVariant *> input_values,
                                                   const Span<SocketValueVariant *> output_values,
                                                   GeoNodesUserData *user_data);

ListPtr evaluate_field_to_list(GField field, const int64_t count);

}  // namespace blender::nodes
