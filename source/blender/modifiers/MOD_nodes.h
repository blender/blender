/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct Main;
struct NodesModifierData;
struct Object;
struct RigidBodyMap;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Rebuild the list of properties based on the sockets exposed as the modifier's node group
 * inputs. If any properties correspond to the old properties by name and type, carry over
 * the values.
 */
void MOD_nodes_update_interface(struct Object *object, struct NodesModifierData *nmd);

/* Update simulation dependencies. */
void MOD_nodes_update_world(struct Main *bmain,
                            struct Scene *scene,
                            struct Object *object,
                            struct NodesModifierData *nmd);

/* Modifier needs rigid body simulation depsgraph nodes. */
bool MOD_nodes_needs_rigid_body_sim(struct Object *object, struct NodesModifierData *nmd);

#ifdef __cplusplus
}
#endif
