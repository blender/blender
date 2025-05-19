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

/**
 * Execute the multi-function with the given parameters. It is assumed that at least one of the
 * inputs is a grid. Otherwise the topology of the output grids is not known.
 *
 * \param fn: The multi-function to call.
 * \param input_values: All input values which may be grids, fields or single values.
 * \param output_values: Where the output grids will be stored.
 * \param r_error_message: An error message that is set if false is returned.
 *
 * \return False if an error occurred. In this case the output values should not be used.
 */
[[nodiscard]] bool execute_multi_function_on_value_variant__volume_grid(
    const mf::MultiFunction &fn,
    const Span<SocketValueVariant *> input_values,
    const Span<SocketValueVariant *> output_values,
    std::string &r_error_message);

}  // namespace blender::nodes
