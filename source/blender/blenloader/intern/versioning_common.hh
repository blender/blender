/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"

#include "BKE_fcurve.hh"

#include "RNA_define.hh"

#include "DNA_anim_types.h"
#include "DNA_node_types.h"

struct ARegion;
struct bNode;
struct bNodeSocket;
struct bNodeTree;
struct ID;
struct IDProperty;
struct ListBase;
struct Main;
struct ViewLayer;
struct SceneRenderLayer;

using blender::FunctionRef;

/**
 * Mapping from new ID types to old converted ID types.
 *
 * This is used by linking code, when a directly linked data comes from a library where it exists
 * as an older, different type of ID.
 *
 * Since the current blend-file will convert that older ID type to the new one as part of its
 * versioning process, when saved, it will store a reference to that linked ID using the _new_ ID
 * type, which will not be found on next blend-file opening/reloading when searching for that ID in
 * the library blend-file.
 *
 * \returns The old, deprecated ID type code if any matches the given `id_code_new` one, otherwise
 * `ID_LINK_PLACEHOLDER`.
 */
short do_versions_new_to_old_idcode_get(short id_code_new);

/**
 * Check if a region of type \a region_type exists in \a regionbase. Otherwise add it after the
 * first region of type \a link_after_region_type.
 * \returns null if a region of the given type already existed, otherwise the newly added region.
 */
ARegion *do_versions_add_region_if_not_found(ListBase *regionbase,
                                             int region_type,
                                             const char *allocname,
                                             int link_after_region_type);
/**
 * Check if a region of type \a region_type exists in \a regionbase. Otherwise add it after the
 * first region of type \a link_after_region_type.
 * \returns either a new, or already existing region.
 */
ARegion *do_versions_ensure_region(ListBase *regionbase,
                                   int region_type,
                                   const char *allocname,
                                   int link_after_region_type);

/**
 * Rename if the ID doesn't exist.
 *
 * \return the ID (if found).
 */
ID *do_versions_rename_id(Main *bmain, short id_type, const char *name_src, const char *name_dst);

bool version_node_socket_is_used(bNodeSocket *sock);

void version_node_socket_name(bNodeTree *ntree,
                              int node_type,
                              const char *old_name,
                              const char *new_name);
void version_node_input_socket_name(bNodeTree *ntree,
                                    int node_type,
                                    const char *old_name,
                                    const char *new_name);
void version_node_output_socket_name(bNodeTree *ntree,
                                     int node_type,
                                     const char *old_name,
                                     const char *new_name);

/**
 * Find the base socket name for an idname that may include a subtype.
 */
blender::StringRef legacy_socket_idname_to_socket_type(blender::StringRef idname);

/**
 * Adds a new node for versioning purposes. This is intended to be used to create raw DNA that
 * might have been read from a file. The created node does not have storage or sockets. Both have
 * to be added manually afterwards.
 *
 * This may seem redundant because the set of sockets is already part of the node declaration.
 * However, the declaration should not be used here, because it changes over time. The versioning
 * code generally expects to get the sockets that the node had at the time of writing the
 * versioning code. Changing the declaration later can break the versioning code in ways that are
 * hard to detect.
 *
 * When adding new nodes in versioning code that replace or belong to existing nodes, they should
 * be positioned so that it overlaps the existing node with just a slight offset. This is better
 * than putting them next to each other they way one would do it manually, because it messes up
 * more complex node trees significantly. In simple tests, putting the nodes next to each other
 * looks better, but in actual user-files it looks way worse and makes it less obvious what was
 * changed by versioning code.
 */
bNode &version_node_add_empty(bNodeTree &ntree, const char *idname);

/**
 * Removes a node for versioning purposes:
 * - Animation data (#AnimData) are not removed, because they might be using #bAction.id which
 *   is not be available before linking.
 * - User count is not updated. This is ensured after blend file reading is done.
 */
void version_node_remove(bNodeTree &ntree, bNode &node);
bNodeSocket &version_node_add_socket(bNodeTree &ntree,
                                     bNode &node,
                                     eNodeSocketInOut in_out,
                                     const char *idname,
                                     const char *identifier);
bNodeLink &version_node_add_link(
    bNodeTree &ntree, bNode &node_a, bNodeSocket &socket_a, bNode &node_b, bNodeSocket &socket_b);

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
void version_node_id(bNodeTree *ntree, int node_type, const char *new_name);

/**
 * Convert `SocketName.001` unique name format to `SocketName_001`. Previously both were used.
 */
void version_node_socket_id_delim(bNodeSocket *socket);

bNodeSocket *version_node_add_socket_if_not_exist(bNodeTree *ntree,
                                                  bNode *node,
                                                  int in_out,
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

void sequencer_init_preview_region(ARegion *region);

void add_realize_instances_before_socket(bNodeTree *ntree,
                                         bNode *node,
                                         bNodeSocket *geometry_socket);

float *version_cycles_node_socket_float_value(bNodeSocket *socket);
float *version_cycles_node_socket_rgba_value(bNodeSocket *socket);
float *version_cycles_node_socket_vector_value(bNodeSocket *socket);

IDProperty *version_cycles_properties_from_ID(ID *id);
IDProperty *version_cycles_properties_from_view_layer(ViewLayer *view_layer);
IDProperty *version_cycles_properties_from_render_layer(SceneRenderLayer *render_layer);
IDProperty *version_cycles_visibility_properties_from_ID(ID *id);

float version_cycles_property_float(IDProperty *idprop, const char *name, float default_value);
int version_cycles_property_int(IDProperty *idprop, const char *name, int default_value);
void version_cycles_property_int_set(IDProperty *idprop, const char *name, int value);
bool version_cycles_property_boolean(IDProperty *idprop, const char *name, bool default_value);
void version_cycles_property_boolean_set(IDProperty *idprop, const char *name, bool value);

void node_tree_relink_with_socket_id_map(bNodeTree &ntree,
                                         bNode &old_node,
                                         bNode &new_node,
                                         const blender::Map<std::string, std::string> &map);
void version_update_node_input(
    bNodeTree *ntree,
    FunctionRef<bool(bNode *)> check_node,
    const char *socket_identifier,
    FunctionRef<void(bNode *, bNodeSocket *)> update_input,
    FunctionRef<void(bNode *, bNodeSocket *, bNode *, bNodeSocket *)> update_input_link);

bNode *version_eevee_output_node_get(bNodeTree *ntree, int16_t node_type);

/**
 * Allow 5.0+ to 'convert' older blendfiles' system properties storage.
 */
void version_system_idprops_generate(Main *bmain);
void version_system_idprops_nodes_generate(Main *bmain);
void version_system_idprops_children_bones_generate(Main *bmain);

bool all_scenes_use(Main *bmain, const blender::Span<const char *> engines);

/**
 * Adjust the values of the given FCurve key frames by applying the given function. The function is
 * expected to get and return a float representing the value of the key frame. The FCurve is
 * potentially changed to have the given property type, if not already the case.
 */
template<typename Function>
static void adjust_fcurve_key_frame_values(FCurve *fcurve,
                                           const PropertyType property_type,
                                           const Function &function)
{
  /* Adjust key frames. */
  if (fcurve->bezt) {
    for (int i = 0; i < fcurve->totvert; i++) {
      fcurve->bezt[i].vec[0][1] = function(fcurve->bezt[i].vec[0][1]);
      fcurve->bezt[i].vec[1][1] = function(fcurve->bezt[i].vec[1][1]);
      fcurve->bezt[i].vec[2][1] = function(fcurve->bezt[i].vec[2][1]);
    }
  }

  /* Adjust baked key frames. */
  if (fcurve->fpt) {
    for (int i = 0; i < fcurve->totvert; i++) {
      fcurve->fpt[i].vec[1] = function(fcurve->fpt[i].vec[1]);
    }
  }

  /* Setup the flags based on the property type. */
  fcurve->flag &= ~(FCURVE_INT_VALUES | FCURVE_DISCRETE_VALUES);
  switch (property_type) {
    case PROP_FLOAT:
      break;
    case PROP_INT:
      fcurve->flag |= FCURVE_INT_VALUES;
      break;
    default:
      fcurve->flag |= (FCURVE_DISCRETE_VALUES | FCURVE_INT_VALUES);
      break;
  }

  /* Recalculate the automatic handles of the FCurve after adjustments. */
  BKE_fcurve_handles_recalc(fcurve);
}

/* Gets the compositing node tree of the given scene. The deprecated node-tree member is returned
 * for older versions before reusable node trees were introduced in bd61e69be5, while the new
 * compositing_node_group is returned otherwise. */
bNodeTree *version_get_scene_compositor_node_tree(Main *bmain, Scene *scene);
