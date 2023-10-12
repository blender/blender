/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include <cstdio>
#include <cstring>

#include "ANIM_visualkey.hh"

#include "BKE_animsys.h"
#include "BKE_armature.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"

#include "ED_keyframing.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"
#include "RNA_types.hh"

namespace blender::animrig {

/* internal status codes for visualkey_can_use */
enum {
  VISUALKEY_NONE = 0,
  VISUALKEY_LOC,
  VISUALKEY_ROT,
  VISUALKEY_SCA,
};

/**
 * This helper function determines if visual-keyframing should be used when
 * inserting keyframes for the given channel. As visual-keyframing only works
 * on Object and Pose-Channel blocks, this should only get called for those
 * block-types, when using "standard" keying but 'Visual Keying' option in Auto-Keying
 * settings is on.
 */
bool visualkey_can_use(PointerRNA *ptr, PropertyRNA *prop)
{
  bConstraint *con = nullptr;
  bool has_rigidbody = false;
  bool has_parent = false;

  /* validate data */
  if (ELEM(nullptr, ptr, ptr->data, prop)) {
    return false;
  }

  /* get first constraint and determine type of keyframe constraints to check for
   * - constraints can be on either Objects or PoseChannels, so we only check if the
   *   ptr->type is RNA_Object or RNA_PoseBone, which are the RNA wrapping-info for
   *   those structs, allowing us to identify the owner of the data
   */
  if (ptr->type == &RNA_Object) {
    /* Object */
    Object *ob = static_cast<Object *>(ptr->data);
    RigidBodyOb *rbo = ob->rigidbody_object;

    con = static_cast<bConstraint *>(ob->constraints.first);
    has_parent = (ob->parent != nullptr);

    /* active rigidbody objects only, as only those are affected by sim */
    has_rigidbody = ((rbo) && (rbo->type == RBO_TYPE_ACTIVE));
  }
  else if (ptr->type == &RNA_PoseBone) {
    /* Pose Channel */
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

    /* some constraints may alter these transforms */
    switch (con->type) {
      /* multi-transform constraints */
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

/**
 * This helper function extracts the value to use for visual-keyframing
 * In the event that it is not possible to perform visual keying, try to fall-back
 * to using the default method. Assumes that all data it has been passed is valid.
 */
float *visualkey_get_values(
    PointerRNA *ptr, PropertyRNA *prop, float *buffer, int buffer_size, int *r_count)
{
  BLI_assert(buffer_size >= 4);

  const char *identifier = RNA_property_identifier(prop);
  float tmat[4][4];
  int rotmode;

  /* handle for Objects or PoseChannels only
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
      copy_v3_v3(buffer, ob->object_to_world[3]);
      *r_count = 3;
      return buffer;
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
      /* only use for non-connected bones */
      if ((pchan->bone->parent == nullptr) || !(pchan->bone->flag & BONE_CONNECTED)) {
        copy_v3_v3(buffer, tmat[3]);
        *r_count = 3;
        return buffer;
      }
    }
  }
  else {
    return ANIM_setting_get_rna_values(ptr, prop, buffer, buffer_size, r_count);
  }

  /* Rot/Scale code are common! */
  if (strstr(identifier, "rotation_euler")) {
    mat4_to_eulO(buffer, rotmode, tmat);

    *r_count = 3;
    return buffer;
  }

  if (strstr(identifier, "rotation_quaternion")) {
    mat4_to_quat(buffer, tmat);

    *r_count = 4;
    return buffer;
  }

  if (strstr(identifier, "rotation_axis_angle")) {
    /* w = 0, x,y,z = 1,2,3 */
    mat4_to_axis_angle(buffer + 1, buffer, tmat);

    *r_count = 4;
    return buffer;
  }

  if (strstr(identifier, "scale")) {
    mat4_to_size(buffer, tmat);

    *r_count = 3;
    return buffer;
  }

  /* as the function hasn't returned yet, read value from system in the default way */
  return ANIM_setting_get_rna_values(ptr, prop, buffer, buffer_size, r_count);
}
}  // namespace blender::animrig