/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include <cstdint>

#include "BLI_span.hh"

struct Object;
struct Depsgraph;

namespace blender::deg::light_linking {

/* Set runtime light linking data on evaluated object. */
void eval_runtime_data(const ::Depsgraph *depsgraph, Object &object_eval);

}  // namespace blender::deg::light_linking
