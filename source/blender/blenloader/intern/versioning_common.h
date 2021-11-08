/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup blenloader
 */

#pragma once

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

ID *do_versions_rename_id(Main *bmain,
                          const short id_type,
                          const char *name_src,
                          const char *name_dst);

void version_node_socket_name(struct bNodeTree *ntree,
                              const int node_type,
                              const char *old_name,
                              const char *new_name);
void version_node_input_socket_name(struct bNodeTree *ntree,
                                    const int node_type,
                                    const char *old_name,
                                    const char *new_name);
void version_node_output_socket_name(struct bNodeTree *ntree,
                                     const int node_type,
                                     const char *old_name,
                                     const char *new_name);

void version_node_socket_index_animdata(
    Main *bmain,
    int node_tree_type, /* NTREE_....., e.g. NTREE_SHADER */
    int node_type,      /* SH_NODE_..., e.g. SH_NODE_BSDF_PRINCIPLED */
    int socket_index_orig,
    int socket_index_offset,
    int total_number_of_sockets);

void version_node_id(struct bNodeTree *ntree, const int node_type, const char *new_name);

#ifdef __cplusplus
}
#endif
