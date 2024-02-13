/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include <cstdio>
#include <cstring>

#include "ANIM_rna.hh"
#include "ANIM_visualkey.hh"

#include "BKE_armature.hh"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"

#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

namespace blender::animrig {

/* Internal status codes for visualkey_can_use. */
enum {
  VISUALKEY_NONE = 0,
  VISUALKEY_LOC,
  VISUALKEY_ROT,
  VISUALKEY_SCA,
};

bool visualkey_can_use(PointerRNA *ptr, PropertyRNA *prop)
{
  bConstraint *con = nullptr;
  bool has_rigidbody = false;
  bool has_parent = false;

  if (ELEM(nullptr, ptr, ptr->data, prop)) {
    return false;
  }

  /* Get first constraint and determine type of keyframe constraints to check for
   * - constraints can be on either Objects or PoseChannels, so we only check if the
   *   ptr->type is RNA_Object or RNA_PoseBone, which are the RNA wrapping-info for
   *   those structs, allowing us to identify the owner of the data
   */
  if (ptr->type == &RNA_Object) {
    Object *ob = static_cast<Object *>(ptr->data);
    RigidBodyOb *rbo = ob->rigidbody_object;

    con = static_cast<bConstraint *>(ob->constraints.first);
    has_parent = (ob->parent != nullptr);

    /* Active rigidbody objects only, as only those are affected by sim. */
    has_rigidbody = ((rbo) && (rbo->type == RBO_TYPE_ACTIVE));
  }
  else if (ptr->type == &RNA_PoseBone) {
    bPoseChannel *pchan = static_cast<bPoseChannel *>(ptr->data);

    if (pchan->constflag & (PCHAN_HAS_IK | PCHAN_INFLUENCED_BY_IK)) {
      /* Spline IK cannot generally be keyed visually, because (at least with the default
       * constraint settings) it requires non-uniform scaling that causes shearing in child bones,
       * which cannot be represented by the bone's loc/rot/scale properties. */
      return true;
    }

    con = static_cast<bConstraint *>(pchan->constraints.first);
    has_parent = (pchan->parent != nullptr);
  }
  else {
    BLI_assert(!"visualkey_can_use called for data-block that is not an Object or PoseBone.");
    return false;
  }

  /* Parent or rigidbody are always matching, no need to check further. */
  if (has_parent || has_rigidbody) {
    return true;
  }

  /* Only do visual keying on transforms. */
  const char *identifier = RNA_property_identifier(prop);
  if (identifier == nullptr) {
    printf("%s failed: nullptr identifier\n", __func__);
    return false;
  }

  short searchtype = VISUALKEY_NONE;
  if (strstr(identifier, "location")) {
    searchtype = VISUALKEY_LOC;
  }
  else if (strstr(identifier, "rotation")) {
    searchtype = VISUALKEY_ROT;
  }
  else if (strstr(identifier, "scale")) {
    searchtype = VISUALKEY_SCA;
  }
  else {
    printf("%s failed: identifier - '%s'\n", __func__, identifier);
    return false;
  }

  /* Check constraints. */
  for (; con; con = con->next) {
    /* only consider constraint if it is not disabled, and has influence */
    if (con->flag & CONSTRAINT_DISABLE) {
      continue;
    }
    if (con->enforce == 0.0f) {
      continue;
    }

    /* Some constraints may alter these transforms. */
    switch (con->type) {
      /* Multi-transform constraints. */
      case CONSTRAINT_TYPE_CHILDOF:
      case CONSTRAINT_TYPE_ARMATURE:
        return true;
      case CONSTRAINT_TYPE_TRANSFORM:
      case CONSTRAINT_TYPE_TRANSLIKE:
        return true;
      case CONSTRAINT_TYPE_FOLLOWPATH:
        return true;
      case CONSTRAINT_TYPE_KINEMATIC:
        return true;

      /* Single-transform constraints. */
      case CONSTRAINT_TYPE_TRACKTO:
        if (searchtype == VISUALKEY_ROT) {
          return true;
        }
        break;
      case CONSTRAINT_TYPE_DAMPTRACK:
        if (searchtype == VISUALKEY_ROT) {
          return true;
        }
        break;
      case CONSTRAINT_TYPE_ROTLIMIT:
        if (searchtype == VISUALKEY_ROT) {
          return true;
        }
        break;
      case CONSTRAINT_TYPE_LOCLIMIT:
        if (searchtype == VISUALKEY_LOC) {
          return true;
        }
        break;
      case CONSTRAINT_TYPE_SIZELIMIT:
        if (searchtype == VISUALKEY_SCA) {
          return true;
        }
        break;
      case CONSTRAINT_TYPE_DISTLIMIT:
        if (searchtype == VISUALKEY_LOC) {
          return true;
        }
        break;
      case CONSTRAINT_TYPE_ROTLIKE:
        if (searchtype == VISUALKEY_ROT) {
          return true;
        }
        break;
      case CONSTRAINT_TYPE_LOCLIKE:
        if (searchtype == VISUALKEY_LOC) {
          return true;
        }
        break;
      case CONSTRAINT_TYPE_SIZELIKE:
        if (searchtype == VISUALKEY_SCA) {
          return true;
        }
        break;
      case CONSTRAINT_TYPE_LOCKTRACK:
        if (searchtype == VISUALKEY_ROT) {
          return true;
        }
        break;
      case CONSTRAINT_TYPE_MINMAX:
        if (searchtype == VISUALKEY_LOC) {
          return true;
        }
        break;

      default:
        break;
    }
  }

  return false;
}

Vector<float> visualkey_get_values(PointerRNA *ptr, PropertyRNA *prop)
{
  Vector<float> values;
  const char *identifier = RNA_property_identifier(prop);
  float tmat[4][4];
  int rotmode;

  /* Handle for Objects or PoseChannels only
   * - only Location, Rotation or Scale keyframes are supported currently
   * - constraints can be on either Objects or PoseChannels, so we only check if the
   *   ptr->type is RNA_Object or RNA_PoseBone, which are the RNA wrapping-info for
   *       those structs, allowing us to identify the owner of the data
   * - assume that array_index will be sane
   */
  if (ptr->type == &RNA_Object) {
    Object *ob = static_cast<Object *>(ptr->data);
    /* Loc code is specific... */
    if (strstr(identifier, "location")) {
      values.extend({ob->object_to_world[3], 3});
      return values;
    }

    copy_m4_m4(tmat, ob->object_to_world);
    rotmode = ob->rotmode;
  }
  else if (ptr->type == &RNA_PoseBone) {
    bPoseChannel *pchan = static_cast<bPoseChannel *>(ptr->data);

    BKE_armature_mat_pose_to_bone(pchan, pchan->pose_mat, tmat);
    rotmode = pchan->rotmode;

    /* Loc code is specific... */
    if (strstr(identifier, "location")) {
      /* Only use for non-connected bones. */
      if ((pchan->bone->parent == nullptr) || !(pchan->bone->flag & BONE_CONNECTED)) {
        values.extend({tmat[3], 3});
        return values;
      }
    }
  }
  else {
    return get_rna_values(ptr, prop);
  }

  /* Rot/Scale code are common! */
  if (strstr(identifier, "rotation_euler")) {
    values.resize(3);
    mat4_to_eulO(values.data(), rotmode, tmat);
    return values;
  }

  if (strstr(identifier, "rotation_quaternion")) {
    values.resize(4);
    mat4_to_quat(values.data(), tmat);
    return values;
  }

  if (strstr(identifier, "rotation_axis_angle")) {
    /* w = 0, x,y,z = 1,2,3 */
    values.resize(4);
    mat4_to_axis_angle(&values[1], &values[0], tmat);
    return values;
  }

  if (strstr(identifier, "scale")) {
    values.resize(3);
    mat4_to_size(values.data(), tmat);
    return values;
  }

  /* As the function hasn't returned yet, read value from system in the default way. */
  return get_rna_values(ptr, prop);
}
}  // namespace blender::animrig
