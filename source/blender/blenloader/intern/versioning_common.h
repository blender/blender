/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#pragma once

#ifdef __cplusplus
#  include "BLI_map.hh"
#endif

struct ARegion;
struct ListBase;
struct Main;
struct bNodeTree;

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion *do_versions_add_region_if_not_found(struct ListBase *regionbase,
                                                    int region_type,
                                                    const char *name,
                                                    int link_after_region_type);

/**
 * Rename if the ID doesn't exist.
 *
 * \return the ID (if found).
 */
ID *do_versions_rename_id(Main *bmain, short id_type, const char *name_src, const char *name_dst);

void version_node_socket_name(struct bNodeTree *ntree,
                              int node_type,
                              const char *old_name,
                              const char *new_name);
void version_node_input_socket_name(struct bNodeTree *ntree,
                                    int node_type,
                                    const char *old_name,
                                    const char *new_name);
void version_node_output_socket_name(struct bNodeTree *ntree,
                                     int node_type,
                                     const char *old_name,
                                     const char *new_name);

/**
 * Adjust animation data for newly added node sockets.
 *
 * Node sockets are addressed by their index (in their RNA path, and thus FCurves/drivers), and
 * thus when a new node is added in the middle of the list, existing animation data needs to be
 * adjusted.
 *
 * Since this is about animation data, it only concerns input sockets.
 *
 * \param node_tree_type: Node tree type that has these nodes, for example #NTREE_SHADER.
 * \param node_type: Node type to adjust, for example #SH_NODE_BSDF_PRINCIPLED.
 * \param socket_index_orig: The original index of the moved socket; when socket 4 moved to 6,
 * pass 4 here.
 * \param socket_index_offset: The offset of the nodes, so when socket 4 moved to 6,
 * pass 2 here.
 * \param total_number_of_sockets: The total number of sockets in the node.
 */
void version_node_socket_index_animdata(
    Main *bmain,
    int node_tree_type, /* NTREE_....., e.g. NTREE_SHADER */
    int node_type,      /* SH_NODE_..., e.g. SH_NODE_BSDF_PRINCIPLED */
    int socket_index_orig,
    int socket_index_offset,
    int total_number_of_sockets);

/**
 * Replace the ID name of all nodes in the tree with the given type with the new name.
 */
void version_node_id(struct bNodeTree *ntree, int node_type, const char *new_name);

/**
 * Convert `SocketName.001` unique name format to `SocketName_001`. Previously both were used.
 */
void version_node_socket_id_delim(bNodeSocket *socket);

struct bNodeSocket *version_node_add_socket_if_not_exist(struct bNodeTree *ntree,
                                                         struct bNode *node,
                                                         eNodeSocketInOut in_out,
                                                         int type,
                                                         int subtype,
                                                         const char *identifier,
                                                         const char *name);

/**
 * The versioning code generally expects `SOCK_IS_LINKED` to be set correctly. This function
 * updates the flag on all sockets after changes to the node tree.
 */
void version_socket_update_is_used(bNodeTree *ntree);
ARegion *do_versions_add_region(int regiontype, const char *name);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
void node_tree_relink_with_socket_id_map(bNodeTree &ntree,
                                         bNode &old_node,
                                         bNode &new_node,
                                         const blender::Map<std::string, std::string> &map);
#endif
