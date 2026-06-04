/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "populate_context.hh"

namespace blender {

struct Material;

namespace io::hydra {

/** Build a Hydra material data source. */
EmittedMaterial build_emitted_material(const PopulateContext &ctx, const Material *material);

}  // namespace io::hydra
}  // namespace blender
