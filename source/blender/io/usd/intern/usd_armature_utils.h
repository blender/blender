/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_function_ref.hh"
#include "BLI_vector.hh"

#include <pxr/base/tf/token.h>
#include <pxr/usd/usdSkel/animation.h>

#include <functional>

struct Bone;
struct Depsgraph;
struct ModifierData;
struct Object;
struct Scene;
struct USDExportParams;

namespace blender::io::usd {

/**
 * Recursively invoke the given function on the given armature object's bones.
 * This function is a no-op if the object isn't an armature.
 *
 * \param ob_arm: The armature object
 * \param visitor: The function to invoke on each bone
 */
void visit_bones(const Object *ob_arm, FunctionRef<void(const Bone *)> visitor);

/**
 * Return in 'r_names' the names of the given armature object's bones.
 *
 * \param ob_arm: The armature object
 * \param r_names: The returned list of bone names
 */
void get_armature_bone_names(const Object *ob_arm, Vector<std::string> &r_names);

/**
 * Return the USD joint path corresponding to the given bone. For example, for the bone
 * "Hand", this function might return the full path "Shoulder/Elbow/Hand" of the joint
 * in the hierachy.
 *
 * \param bone: The bone whose path will be queried.
 * \return: The path to the joint
 */
pxr::TfToken build_usd_joint_path(const Bone *bone);

/**
 * Sets the USD joint paths as an attribute on the given USD animation,
 * where the paths correspond to the bones of the given armature.
 *
 * \param skel_anim: The animation whose joints attribute will be set
 * \param ob_arm: The armature object
 */
void create_pose_joints(pxr::UsdSkelAnimation &skel_anim, const Object *obj);

/**
 * Return a list of all the modifiers on the given object enabled for the
 * given dependency graph's evaluation mode (viewport or render).
 *
 * \param obj: Object to query for the modifiers
 * \param depsgraph: The dependency graph where the object was evaluated
 * \return: The list of modifiers
 *
 */
Vector<ModifierData *> get_enabled_modifiers(const Object *obj, const Depsgraph *depsgraph);
/**
 * If the given object has an enabled armature modifier, return the
 * armature object bound to the modifier.
 *
 * \param: Object to check for the modifier
 * \param depsgraph: The dependency graph where the object was evaluated
 * \return: The armature object
 */
const Object *get_armature_modifier_obj(const Object *obj, const Depsgraph *depsgraph);

/**
 * If the given object has an armature modifier, query whether the given
 * name matches the name of a bone on the armature referenced by the modifier.
 *
 * \param obj: Object to query for the modifier
 * \param name: Name to check
 * \param depsgraph: The dependency graph where the object was evaluated
 * \return: True if the name matches a bone name.  Return false if no matching
 *          bone name is found or if the object does not have an armature modifier
 */
bool is_armature_modifier_bone_name(const Object *obj,
                                    const char *name,
                                    const Depsgraph *depsgraph);

/**
 * Query whether exporting a skinned mesh is supported for the given object.
 * Currently, the object can be exported as a skinned mesh if it has an enabled
 * armature modifier and no other enabled modifiers.
 *
 * \param obj: Object to query
 * \param depsgraph: The dependency graph where the object was evaluated
 * \return: True if skinned mesh export is supported, false otherwise
 */
bool can_export_skinned_mesh(const Object *obj, const Depsgraph *depsgraph);

}  // namespace blender::io::usd
