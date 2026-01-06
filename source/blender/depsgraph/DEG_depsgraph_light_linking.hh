/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

namespace blender {

struct Depsgraph;
struct Object;
namespace deg::light_linking {

/* Set runtime light linking data on evaluated object. */
void eval_runtime_data(const Depsgraph *depsgraph, Object &object_eval);

}  // namespace deg::light_linking

}  // namespace blender
