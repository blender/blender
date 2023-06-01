/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
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
