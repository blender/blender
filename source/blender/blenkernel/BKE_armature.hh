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

#include "BKE_armature.h"

#include "BLI_function_ref.hh"
#include "BLI_set.hh"

namespace blender::bke {

struct SelectedBonesResult {
  bool all_bones_selected = true;
  bool no_bones_selected = true;
};

using SelectedBoneCallback = blender::FunctionRef<void(Bone *bone)>;
SelectedBonesResult BKE_armature_find_selected_bones(const bArmature *armature,
                                                     SelectedBoneCallback callback);

using BoneNameSet = blender::Set<std::string>;
/**
 * Return a set of names of the selected bones. An empty set means "ignore bone
 * selection", which either means all bones are selected, or none are.
 */
BoneNameSet BKE_armature_find_selected_bone_names(const bArmature *armature);

};  // namespace blender::bke
