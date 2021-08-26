/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#pragma once

/** \file
 * \ingroup bke
 */
#ifndef __cplusplus
#  error This is a C++ only header.
#endif

#include "BLI_function_ref.hh"

struct FCurve;
struct bAction;

namespace blender::bke {

using FoundFCurveCallback = blender::FunctionRef<void(FCurve *fcurve, const char *bone_name)>;
void BKE_action_find_fcurves_with_bones(const bAction *action, FoundFCurveCallback callback);

};  // namespace blender::bke
