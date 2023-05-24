/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

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
