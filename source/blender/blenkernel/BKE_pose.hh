/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_assert.h"

#include <concepts>
#include <type_traits>

/** \file
 * \ingroup bke
 *
 * Pose-related types and functions.
 */

namespace blender {
struct bArmature;
struct Bone;
struct bPoseChannel;
}  // namespace blender

namespace blender::bke {

/**
 * Pairing of a pose channel with its corresponding armature bone.
 *
 * This struct allows code which already has both pointers can pass them around without repeating
 * the armature bone lookup. Currently this lookup is cheap (just following a pointer), but in the
 * future it will become more expensive (array index lookup to find that pointer, then follow it).
 *
 * Both pointers are expected to both be null, or both be non-null and refer to matching data (i.e.
 * `bone` is the #Bone that `pchan` currently resolves to via its armature index).
 *
 * This type is non-owning. It must not outlive either pointers.
 */
template<typename PoseChannelT, typename BoneT> struct PChanBoneT {
  PoseChannelT *pchan;
  BoneT *bone;

  PChanBoneT(PoseChannelT *pchan, BoneT *bone) : pchan(pchan), bone(bone)
  {
    BLI_assert_msg((pchan == nullptr) == (bone == nullptr),
                   "either both pointers should be nil, or none of them should be");
  }

  /**
   * Default constructor for nullptr values.
   *
   * This constructor is here to allow arrays of PChanBoneT to be preallocated.
   */
  PChanBoneT() : pchan(nullptr), bone(nullptr) {}

  /* Implicit conversion from the non-const variant to the const variant.
   * Enabled only when that direction actually adds const, so the reverse
   * (const -> non-const) remains forbidden. */
  template<typename OtherPChan, typename OtherBone>
    requires std::convertible_to<OtherPChan *, PoseChannelT *> &&
                 std::convertible_to<OtherBone *, BoneT *>
  PChanBoneT(const PChanBoneT<OtherPChan, OtherBone> &other) : pchan(other.pchan), bone(other.bone)
  {
  }
};

using PChanBone = PChanBoneT<bPoseChannel, Bone>;
using PChanBoneConst = PChanBoneT<const bPoseChannel, const Bone>;

}  // namespace blender::bke
