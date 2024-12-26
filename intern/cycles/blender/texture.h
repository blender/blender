/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "RNA_blender_cpp.hh"

#include "util/types.h"

CCL_NAMESPACE_BEGIN

void point_density_texture_space(BL::Depsgraph &b_depsgraph,
                                 BL::ShaderNodeTexPointDensity &b_point_density_node,
                                 float3 &loc,
                                 float3 &size);

CCL_NAMESPACE_END
