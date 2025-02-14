/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_slim
 */

#pragma once

#include "slim.h"

#include <Eigen/Dense>

using namespace Eigen;

namespace slim {

void correct_map_surface_area_if_necessary(SLIMData &slim_data);
void correct_mesh_surface_area_if_necessary(SLIMData &slim_data);

}  // namespace slim
