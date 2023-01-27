/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct NodesModifierData;
struct Object;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Rebuild the list of properties based on the sockets exposed as the modifier's node group
 * inputs. If any properties correspond to the old properties by name and type, carry over
 * the values.
 */
void MOD_nodes_update_interface(struct Object *object, struct NodesModifierData *nmd);

#ifdef __cplusplus
}
#endif
