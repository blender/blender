/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct bNodeTreeType *ntreeType_Geometry;

void register_node_type_geo_custom_group(bNodeType *ntype);

/* -------------------------------------------------------------------- */
/** \name Simulation Input Node
 * \{ */

struct bNode *NOD_geometry_simulation_input_get_paired_output(
    struct bNodeTree *node_tree, const struct bNode *simulation_input_node);

/**
 * Pair a simulation input node with an output node.
 * \return True if pairing the node was successful.
 */
bool NOD_geometry_simulation_input_pair_with_output(const struct bNodeTree *node_tree,
                                                    struct bNode *simulation_input_node,
                                                    const struct bNode *simulation_output_node);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Simulation Output Node
 * \{ */

bool NOD_geometry_simulation_output_item_socket_type_supported(eNodeSocketDatatype socket_type);

/**
 * Set a unique item name.
 * \return True if the unique name differs from the original name.
 */
bool NOD_geometry_simulation_output_item_set_unique_name(struct NodeGeometrySimulationOutput *sim,
                                                         struct NodeSimulationItem *item,
                                                         const char *name,
                                                         const char *defname);

/**
 * Find the node owning this simulation state item.
 */
bNode *NOD_geometry_simulation_output_find_node_by_item(struct bNodeTree *ntree,
                                                        const struct NodeSimulationItem *item);

bool NOD_geometry_simulation_output_contains_item(struct NodeGeometrySimulationOutput *sim,
                                                  const struct NodeSimulationItem *item);
struct NodeSimulationItem *NOD_geometry_simulation_output_get_active_item(
    struct NodeGeometrySimulationOutput *sim);
void NOD_geometry_simulation_output_set_active_item(struct NodeGeometrySimulationOutput *sim,
                                                    struct NodeSimulationItem *item);
struct NodeSimulationItem *NOD_geometry_simulation_output_find_item(
    struct NodeGeometrySimulationOutput *sim, const char *name);
struct NodeSimulationItem *NOD_geometry_simulation_output_add_item(
    struct NodeGeometrySimulationOutput *sim, short socket_type, const char *name);
struct NodeSimulationItem *NOD_geometry_simulation_output_insert_item(
    struct NodeGeometrySimulationOutput *sim, short socket_type, const char *name, int index);
struct NodeSimulationItem *NOD_geometry_simulation_output_add_item_from_socket(
    struct NodeGeometrySimulationOutput *sim,
    const struct bNode *from_node,
    const struct bNodeSocket *from_sock);
struct NodeSimulationItem *NOD_geometry_simulation_output_insert_item_from_socket(
    struct NodeGeometrySimulationOutput *sim,
    const struct bNode *from_node,
    const struct bNodeSocket *from_sock,
    int index);
void NOD_geometry_simulation_output_remove_item(struct NodeGeometrySimulationOutput *sim,
                                                struct NodeSimulationItem *item);
void NOD_geometry_simulation_output_clear_items(struct NodeGeometrySimulationOutput *sim);
void NOD_geometry_simulation_output_move_item(struct NodeGeometrySimulationOutput *sim,
                                              int from_index,
                                              int to_index);

/** \} */

#ifdef __cplusplus
}
#endif
