/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_modifier_types.h"

#include <pxr/base/tf/token.h>
#include <pxr/usd/usdSkel/animation.h>

#include <string>

struct Bone;
struct Depsgraph;
struct ModifierData;
struct Object;

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
 * \param use_deform: If true, use only deform bone names, including their parents, to match
 *                    armature export joint indices
 * \param r_names: The returned list of bone names
 */
void get_armature_bone_names(const Object *ob_arm, bool use_deform, Vector<std::string> &r_names);

/**
 * Return the USD joint path corresponding to the given bone. For example, for the bone
 * "Hand", this function might return the full path "Shoulder/Elbow/Hand" of the joint
 * in the hierarchy.
 *
 * \param bone: The bone whose path will be queried.
 * \return The path to the joint.
 */
pxr::TfToken build_usd_joint_path(const Bone *bone);

/**
 * Sets the USD joint paths as an attribute on the given USD animation,
 * where the paths correspond to the bones of the given armature.
 *
 * \param skel_anim: The animation whose joints attribute will be set
 * \param ob_arm: The armature object
 * \param deform_map: A pointer to a map associating bone names with
 *                    deform bones and their parents. If the pointer
 *                    is not null, assume only deform bones are to be
 *                    exported and bones not found in this map will be
 *                    skipped
 */
void create_pose_joints(pxr::UsdSkelAnimation &skel_anim,
                        const Object &obj,
                        const Map<StringRef, const Bone *> *deform_map);

/**
 * Return the modifier of the given type enabled for the given dependency graph's
 * evaluation mode (viewport or render).
 *
 * \param obj: Object to query for the modifier
 * \param depsgraph: The dependency graph where the object was evaluated
 * \return The modifier.
 */
const ModifierData *get_enabled_modifier(const Object &obj,
                                         ModifierType type,
                                         const Depsgraph *depsgraph);

/**
 * If the given object has an enabled armature modifier, return the
 * armature object bound to the modifier.
 *
 * \param: Object to check for the modifier
 * \param depsgraph: The dependency graph where the object was evaluated
 * \return The armature object.
 */
const Object *get_armature_modifier_obj(const Object &obj, const Depsgraph *depsgraph);

/**
 * If the given object has an armature modifier, query whether the given
 * name matches the name of a bone on the armature referenced by the modifier.
 *
 * \param obj: Object to query for the modifier
 * \param name: Name to check
 * \param depsgraph: The dependency graph where the object was evaluated
 * \return True if the name matches a bone name.  Return false if no matching
 *         bone name is found or if the object does not have an armature modifier
 */
bool is_armature_modifier_bone_name(const Object &obj,
                                    const StringRefNull name,
                                    const Depsgraph *depsgraph);

/**
 * Query whether exporting a skinned mesh is supported for the given object.
 * Currently, the object can be exported as a skinned mesh if it has an enabled
 * armature modifier and no other enabled modifiers.
 *
 * \param obj: Object to query
 * \param depsgraph: The dependency graph where the object was evaluated
 * \return True if skinned mesh export is supported, false otherwise.
 */
bool can_export_skinned_mesh(const Object &obj, const Depsgraph *depsgraph);

/**
 * Initialize the deform bones map:
 * - First: grab all bones marked for deforming and store them.
 * - Second: loop the deform bones you found and recursively walk up their parent
 *           hierarchies, marking those bones as deform as well.
 * \param obj: Object to query
 * \param deform_map: A pointer to the deform_map to fill with deform bones and
 *                    their parents found on the object
 */
void init_deform_bones_map(const Object *obj, Map<StringRef, const Bone *> *deform_map);

}  // namespace blender::io::usd
