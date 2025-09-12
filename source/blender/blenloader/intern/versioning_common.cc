/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */
/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include <cstring>

#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"

#include "BKE_animsys.h"
#include "BKE_grease_pencil_legacy_convert.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "ANIM_versioning.hh"

#include "NOD_socket.hh"

#include "BLT_translation.hh"

#include "SEQ_sequencer.hh"

#include "MEM_guardedalloc.h"

#include "BLO_readfile.hh"
#include "readfile.hh"
#include "versioning_common.hh"

using blender::Map;
using blender::StringRef;

short do_versions_new_to_old_idcode_get(const short id_code_new)
{
  switch (id_code_new) {
    case ID_GP:
      /* ID_GD_LEGACY (Grease Pencil v2) is now converted to ID_GP (Grease Pencil v3). */
      return ID_GD_LEGACY;
    default:
      return ID_LINK_PLACEHOLDER;
  }
}

ARegion *do_versions_add_region_if_not_found(ListBase *regionbase,
                                             int region_type,
                                             const char * /*allocname*/,
                                             int link_after_region_type)
{
  ARegion *link_after_region = nullptr;
  LISTBASE_FOREACH (ARegion *, region, regionbase) {
    if (region->regiontype == region_type) {
      return nullptr;
    }
    if (region->regiontype == link_after_region_type) {
      link_after_region = region;
    }
  }

  ARegion *new_region = BKE_area_region_new();
  new_region->regiontype = region_type;
  BLI_insertlinkafter(regionbase, link_after_region, new_region);
  return new_region;
}

ARegion *do_versions_ensure_region(ListBase *regionbase,
                                   int region_type,
                                   const char * /*allocname*/,
                                   int link_after_region_type)
{
  ARegion *link_after_region = nullptr;
  LISTBASE_FOREACH (ARegion *, region, regionbase) {
    if (region->regiontype == region_type) {
      return region;
    }
    if (region->regiontype == link_after_region_type) {
      link_after_region = region;
    }
  }

  ARegion *new_region = BKE_area_region_new();
  new_region->regiontype = region_type;
  BLI_insertlinkafter(regionbase, link_after_region, new_region);
  return new_region;
}

ID *do_versions_rename_id(Main *bmain,
                          const short id_type,
                          const char *name_src,
                          const char *name_dst)
{
  /* We can ignore libraries */
  ListBase *lb = which_libbase(bmain, id_type);
  ID *id = nullptr;
  LISTBASE_FOREACH (ID *, idtest, lb) {
    if (!ID_IS_LINKED(idtest)) {
      if (STREQ(idtest->name + 2, name_src)) {
        id = idtest;
      }
      if (STREQ(idtest->name + 2, name_dst)) {
        return nullptr;
      }
    }
  }
  if (id != nullptr) {
    BKE_libblock_rename(*bmain, *id, name_dst);
  }
  return id;
}

static void change_node_socket_name(ListBase *sockets, const char *old_name, const char *new_name)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, sockets) {
    if (STREQ(socket->name, old_name)) {
      STRNCPY_UTF8(socket->name, new_name);
    }
    if (STREQ(socket->identifier, old_name)) {
      STRNCPY_UTF8(socket->identifier, new_name);
    }
  }
}

bool version_node_socket_is_used(bNodeSocket *sock)
{
  BLI_assert(sock != nullptr);
  return sock->flag & SOCK_IS_LINKED;
}

void version_node_socket_id_delim(bNodeSocket *socket)
{
  StringRef name = socket->name;
  StringRef id = socket->identifier;

  if (!id.startswith(name)) {
    /* We only need to affect the case where the identifier starts with the name. */
    return;
  }

  StringRef id_number = id.drop_known_prefix(name);
  if (id_number.is_empty()) {
    /* The name was already unique, and didn't need numbers at the end for the id. */
    return;
  }

  if (id_number.startswith(".")) {
    socket->identifier[name.size()] = '_';
  }
}

void version_node_socket_name(bNodeTree *ntree,
                              const int node_type,
                              const char *old_name,
                              const char *new_name)
{
  for (bNode *node : ntree->all_nodes()) {
    if (node->type_legacy == node_type) {
      change_node_socket_name(&node->inputs, old_name, new_name);
      change_node_socket_name(&node->outputs, old_name, new_name);
    }
  }
}

void version_node_input_socket_name(bNodeTree *ntree,
                                    const int node_type,
                                    const char *old_name,
                                    const char *new_name)
{
  for (bNode *node : ntree->all_nodes()) {
    if (node->type_legacy == node_type) {
      change_node_socket_name(&node->inputs, old_name, new_name);
    }
  }
}

void version_node_output_socket_name(bNodeTree *ntree,
                                     const int node_type,
                                     const char *old_name,
                                     const char *new_name)
{
  for (bNode *node : ntree->all_nodes()) {
    if (node->type_legacy == node_type) {
      change_node_socket_name(&node->outputs, old_name, new_name);
    }
  }
}

StringRef legacy_socket_idname_to_socket_type(StringRef idname)
{
  using string_pair = std::pair<const char *, const char *>;
  static const string_pair subtypes_map[] = {{"NodeSocketFloatUnsigned", "NodeSocketFloat"},
                                             {"NodeSocketFloatPercentage", "NodeSocketFloat"},
                                             {"NodeSocketFloatFactor", "NodeSocketFloat"},
                                             {"NodeSocketFloatAngle", "NodeSocketFloat"},
                                             {"NodeSocketFloatTime", "NodeSocketFloat"},
                                             {"NodeSocketFloatTimeAbsolute", "NodeSocketFloat"},
                                             {"NodeSocketFloatDistance", "NodeSocketFloat"},
                                             {"NodeSocketIntUnsigned", "NodeSocketInt"},
                                             {"NodeSocketIntPercentage", "NodeSocketInt"},
                                             {"NodeSocketIntFactor", "NodeSocketInt"},
                                             {"NodeSocketVectorTranslation", "NodeSocketVector"},
                                             {"NodeSocketVectorDirection", "NodeSocketVector"},
                                             {"NodeSocketVectorVelocity", "NodeSocketVector"},
                                             {"NodeSocketVectorAcceleration", "NodeSocketVector"},
                                             {"NodeSocketVectorEuler", "NodeSocketVector"},
                                             {"NodeSocketVectorXYZ", "NodeSocketVector"}};
  for (const string_pair &pair : subtypes_map) {
    if (pair.first == idname) {
      return pair.second;
    }
  }
  /* Unchanged socket idname. */
  return idname;
}

bNode &version_node_add_empty(bNodeTree &ntree, const char *idname)
{
  blender::bke::bNodeType *ntype = blender::bke::node_type_find(idname);

  bNode *node = MEM_callocN<bNode>(__func__);
  node->runtime = MEM_new<blender::bke::bNodeRuntime>(__func__);
  BLI_addtail(&ntree.nodes, node);
  blender::bke::node_unique_id(ntree, *node);

  STRNCPY(node->idname, idname);
  DATA_(ntype->ui_name).copy_utf8_truncated(node->name);
  blender::bke::node_unique_name(ntree, *node);

  node->flag = NODE_SELECT | NODE_OPTIONS | NODE_INIT;
  node->width = ntype->width;
  node->height = ntype->height;
  node->color[0] = node->color[1] = node->color[2] = 0.608;

  node->type_legacy = ntype->type_legacy;

  BKE_ntree_update_tag_node_new(&ntree, node);
  return *node;
}

void version_node_remove(bNodeTree &ntree, bNode &node)
{
  blender::bke::node_unlink_node(ntree, node);
  blender::bke::node_unlink_attached(&ntree, &node);

  blender::bke::node_free_node(&ntree, node);
  blender::bke::node_rebuild_id_vector(ntree);
}

bNodeSocket &version_node_add_socket(bNodeTree &ntree,
                                     bNode &node,
                                     const eNodeSocketInOut in_out,
                                     const char *idname,
                                     const char *identifier)
{
  blender::bke::bNodeSocketType *stype = blender::bke::node_socket_type_find(idname);

  bNodeSocket *socket = MEM_callocN<bNodeSocket>(__func__);
  socket->runtime = MEM_new<blender::bke::bNodeSocketRuntime>(__func__);
  socket->in_out = in_out;
  socket->limit = (in_out == SOCK_IN ? 1 : 0xFFF);
  socket->type = stype->type;

  STRNCPY_UTF8(socket->idname, idname);
  STRNCPY_UTF8(socket->identifier, identifier);
  STRNCPY_UTF8(socket->name, identifier);

  if (in_out == SOCK_IN) {
    BLI_addtail(&node.inputs, socket);
  }
  else {
    BLI_addtail(&node.outputs, socket);
  }

  node_socket_init_default_value_data(stype->type, stype->subtype, &socket->default_value);

  BKE_ntree_update_tag_socket_new(&ntree, socket);
  return *socket;
}

bNodeLink &version_node_add_link(
    bNodeTree &ntree, bNode &node_a, bNodeSocket &socket_a, bNode &node_b, bNodeSocket &socket_b)
{
  BLI_assert(socket_a.in_out != socket_b.in_out);
  if (socket_a.in_out == SOCK_IN) {
    return version_node_add_link(ntree, node_b, socket_b, node_a, socket_a);
  }
  bNode &node_from = node_a;
  bNodeSocket &socket_from = socket_a;
  bNode &node_to = node_b;
  bNodeSocket &socket_to = socket_b;

  bNodeLink *link = MEM_callocN<bNodeLink>(__func__);
  link->fromnode = &node_from;
  link->fromsock = &socket_from;
  link->tonode = &node_to;
  link->tosock = &socket_to;

  BLI_addtail(&ntree.links, link);

  BKE_ntree_update_tag_link_added(&ntree, link);
  return *link;
}

bNodeSocket *version_node_add_socket_if_not_exist(bNodeTree *ntree,
                                                  bNode *node,
                                                  int in_out,
                                                  int type,
                                                  int subtype,
                                                  const char *identifier,
                                                  const char *name)
{
  bNodeSocket *sock = blender::bke::node_find_socket(*node, eNodeSocketInOut(in_out), identifier);
  if (sock != nullptr) {
    return sock;
  }
  return blender::bke::node_add_static_socket(
      *ntree, *node, eNodeSocketInOut(in_out), type, subtype, identifier, name);
}

void version_node_id(bNodeTree *ntree, const int node_type, const char *new_name)
{
  for (bNode *node : ntree->all_nodes()) {
    if (node->type_legacy == node_type) {
      if (!STREQ(node->idname, new_name)) {
        STRNCPY(node->idname, new_name);
      }
    }
  }
}

void version_node_socket_index_animdata(Main *bmain,
                                        const int node_tree_type,
                                        const int node_type,
                                        const int socket_index_orig,
                                        const int socket_index_offset,
                                        const int total_number_of_sockets)
{

  /* The for loop for the input ids is at the top level otherwise we lose the animation
   * keyframe data. Not sure what causes that, so I (Sybren) moved the code here from
   * versioning_290.cc as-is (structure-wise). */
  for (int input_index = total_number_of_sockets - 1; input_index >= socket_index_orig;
       input_index--)
  {
    FOREACH_NODETREE_BEGIN (bmain, ntree, owner_id) {
      if (ntree->type != node_tree_type) {
        continue;
      }

      for (bNode *node : ntree->all_nodes()) {
        if (node->type_legacy != node_type) {
          continue;
        }

        char node_name_escaped[sizeof(node->name) * 2];
        BLI_str_escape(node_name_escaped, node->name, sizeof(node_name_escaped));
        char *rna_path_prefix = BLI_sprintfN("nodes[\"%s\"].inputs", node_name_escaped);

        const int new_index = input_index + socket_index_offset;
        BKE_animdata_fix_paths_rename_all_ex(
            bmain, owner_id, rna_path_prefix, nullptr, nullptr, input_index, new_index, false);
        MEM_freeN(rna_path_prefix);
      }
    }
    FOREACH_NODETREE_END;
  }
}

void version_socket_update_is_used(bNodeTree *ntree)
{
  for (bNode *node : ntree->all_nodes()) {
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
      socket->flag &= ~SOCK_IS_LINKED;
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
      socket->flag &= ~SOCK_IS_LINKED;
    }
  }
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    link->fromsock->flag |= SOCK_IS_LINKED;
    link->tosock->flag |= SOCK_IS_LINKED;
  }
}

ARegion *do_versions_add_region(int regiontype, const char * /*name*/)
{
  ARegion *region = BKE_area_region_new();
  region->regiontype = regiontype;
  return region;
}

void node_tree_relink_with_socket_id_map(bNodeTree &ntree,
                                         bNode &old_node,
                                         bNode &new_node,
                                         const Map<std::string, std::string> &map)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree.links) {
    if (link->tonode == &old_node) {
      bNodeSocket *old_socket = link->tosock;
      if (old_socket->is_available()) {
        if (const std::string *new_identifier = map.lookup_ptr_as(old_socket->identifier)) {
          bNodeSocket *new_socket = blender::bke::node_find_socket(
              *&new_node, SOCK_IN, *new_identifier);
          link->tonode = &new_node;
          link->tosock = new_socket;
          old_socket->link = nullptr;
        }
      }
    }
    if (link->fromnode == &old_node) {
      bNodeSocket *old_socket = link->fromsock;
      if (old_socket->is_available()) {
        if (const std::string *new_identifier = map.lookup_ptr_as(old_socket->identifier)) {
          bNodeSocket *new_socket = blender::bke::node_find_socket(
              *&new_node, SOCK_OUT, *new_identifier);
          link->fromnode = &new_node;
          link->fromsock = new_socket;
          old_socket->link = nullptr;
        }
      }
    }
  }
}

static blender::Vector<bNodeLink *> find_connected_links(bNodeTree *ntree, bNodeSocket *in_socket)
{
  blender::Vector<bNodeLink *> links;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->tosock == in_socket) {
      links.append(link);
    }
  }
  return links;
}

void add_realize_instances_before_socket(bNodeTree *ntree,
                                         bNode *node,
                                         bNodeSocket *geometry_socket)
{
  BLI_assert(geometry_socket->type == SOCK_GEOMETRY);
  blender::Vector<bNodeLink *> links = find_connected_links(ntree, geometry_socket);
  for (bNodeLink *link : links) {
    /* If the realize instances node is already before this socket, no need to continue. */
    if (link->fromnode->type_legacy == GEO_NODE_REALIZE_INSTANCES) {
      return;
    }

    bNode *realize_node = blender::bke::node_add_static_node(
        nullptr, *ntree, GEO_NODE_REALIZE_INSTANCES);
    realize_node->parent = node->parent;
    realize_node->locx_legacy = node->locx_legacy - 100;
    realize_node->locy_legacy = node->locy_legacy;
    blender::bke::node_add_link(*ntree,
                                *link->fromnode,
                                *link->fromsock,
                                *realize_node,
                                *static_cast<bNodeSocket *>(realize_node->inputs.first));
    link->fromnode = realize_node;
    link->fromsock = static_cast<bNodeSocket *>(realize_node->outputs.first);
  }
}

float *version_cycles_node_socket_float_value(bNodeSocket *socket)
{
  bNodeSocketValueFloat *socket_data = static_cast<bNodeSocketValueFloat *>(socket->default_value);
  return &socket_data->value;
}

float *version_cycles_node_socket_rgba_value(bNodeSocket *socket)
{
  bNodeSocketValueRGBA *socket_data = static_cast<bNodeSocketValueRGBA *>(socket->default_value);
  return socket_data->value;
}

float *version_cycles_node_socket_vector_value(bNodeSocket *socket)
{
  bNodeSocketValueVector *socket_data = static_cast<bNodeSocketValueVector *>(
      socket->default_value);
  return socket_data->value;
}

IDProperty *version_cycles_properties_from_ID(ID *id)
{
  IDProperty *idprop = IDP_ID_system_properties_get(id);
  return (idprop) ? IDP_GetPropertyTypeFromGroup(idprop, "cycles", IDP_GROUP) : nullptr;
}

IDProperty *version_cycles_properties_from_view_layer(ViewLayer *view_layer)
{
  IDProperty *idprop = view_layer->system_properties;
  return (idprop) ? IDP_GetPropertyTypeFromGroup(idprop, "cycles", IDP_GROUP) : nullptr;
}

IDProperty *version_cycles_properties_from_render_layer(SceneRenderLayer *render_layer)
{
  IDProperty *idprop = render_layer->prop;
  return (idprop) ? IDP_GetPropertyTypeFromGroup(idprop, "cycles", IDP_GROUP) : nullptr;
}

float version_cycles_property_float(IDProperty *idprop, const char *name, float default_value)
{
  IDProperty *prop = IDP_GetPropertyTypeFromGroup(idprop, name, IDP_FLOAT);
  return (prop) ? IDP_float_get(prop) : default_value;
}

int version_cycles_property_int(IDProperty *idprop, const char *name, int default_value)
{
  IDProperty *prop = IDP_GetPropertyTypeFromGroup(idprop, name, IDP_INT);
  return (prop) ? IDP_int_get(prop) : default_value;
}

void version_cycles_property_int_set(IDProperty *idprop, const char *name, int value)
{
  if (IDProperty *prop = IDP_GetPropertyTypeFromGroup(idprop, name, IDP_INT)) {
    IDP_int_set(prop, value);
  }
  else {
    IDP_AddToGroup(idprop, blender::bke::idprop::create(name, value).release());
  }
}

bool version_cycles_property_boolean(IDProperty *idprop, const char *name, bool default_value)
{
  return version_cycles_property_int(idprop, name, default_value);
}

void version_cycles_property_boolean_set(IDProperty *idprop, const char *name, bool value)
{
  version_cycles_property_int_set(idprop, name, value);
}

IDProperty *version_cycles_visibility_properties_from_ID(ID *id)
{
  IDProperty *idprop = IDP_ID_system_properties_get(id);
  return (idprop) ? IDP_GetPropertyTypeFromGroup(idprop, "cycles_visibility", IDP_GROUP) : nullptr;
}

void version_update_node_input(
    bNodeTree *ntree,
    FunctionRef<bool(bNode *)> check_node,
    const char *socket_identifier,
    FunctionRef<void(bNode *, bNodeSocket *)> update_input,
    FunctionRef<void(bNode *, bNodeSocket *, bNode *, bNodeSocket *)> update_input_link)
{
  bool need_update = false;

  /* Iterate backwards from end so we don't encounter newly added links. */
  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
    /* Detect link to replace. */
    bNode *fromnode = link->fromnode;
    bNodeSocket *fromsock = link->fromsock;
    bNode *tonode = link->tonode;
    bNodeSocket *tosock = link->tosock;

    if (!(tonode != nullptr && check_node(tonode) && STREQ(tosock->identifier, socket_identifier)))
    {
      continue;
    }

    /* Replace links with updated equivalent */
    blender::bke::node_remove_link(ntree, *link);
    update_input_link(fromnode, fromsock, tonode, tosock);

    need_update = true;
  }

  /* Update sockets and/or their default values.
   * Do this after the link update in case it changes the identifier. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (check_node(node)) {
      bNodeSocket *input = blender::bke::node_find_socket(*node, SOCK_IN, socket_identifier);
      if (input != nullptr) {
        update_input(node, input);
      }
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

bNode *version_eevee_output_node_get(bNodeTree *ntree, int16_t node_type)
{
  bNode *output_node = nullptr;
  /* NOTE: duplicated from `ntreeShaderOutputNode` with small adjustments so it can be called
   * during versioning. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != node_type) {
      continue;
    }
    if (node->custom1 == SHD_OUTPUT_ALL) {
      if (output_node == nullptr) {
        output_node = node;
      }
      else if (output_node->custom1 == SHD_OUTPUT_ALL) {
        if ((node->flag & NODE_DO_OUTPUT) && !(output_node->flag & NODE_DO_OUTPUT)) {
          output_node = node;
        }
      }
    }
    else if (node->custom1 == SHD_OUTPUT_EEVEE) {
      if (output_node == nullptr) {
        output_node = node;
      }
      else if ((node->flag & NODE_DO_OUTPUT) && !(output_node->flag & NODE_DO_OUTPUT)) {
        output_node = node;
      }
    }
  }

  return output_node;
}

bool all_scenes_use(Main *bmain, const blender::Span<const char *> engines)
{
  if (!bmain->scenes.first) {
    return false;
  }

  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    bool match = false;
    for (const char *engine : engines) {
      if (STREQ(scene->r.engine, engine)) {
        match = true;
      }
    }
    if (!match) {
      return false;
    }
  }

  return true;
}

bNodeTree *version_get_scene_compositor_node_tree(Main *bmain, Scene *scene)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 4)) {
    return scene->nodetree;
  }

  return scene->compositing_node_group;
}

static bool blendfile_or_libraries_versions_atleast(Main *bmain,
                                                    const short versionfile,
                                                    const short subversionfile)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, versionfile, subversionfile)) {
    return false;
  }

  LISTBASE_FOREACH (Library *, library, &bmain->libraries) {
    if (!LIBRARY_VERSION_FILE_ATLEAST(library, versionfile, subversionfile)) {
      return false;
    }
  }

  return true;
}

void do_versions_after_setup(Main *new_bmain,
                             BlendfileLinkAppendContext *lapp_context,
                             BlendFileReadReport *reports)
{
  /* WARNING: The code below may add IDs. These IDs _will_ be (by definition) conforming to current
   * code's version already, and _must not_ be *versioned* again.
   *
   * This means that when adding code here, _extreme_ care must be taken that it will not badly
   * affect these 'modern' IDs potentially added by already existing processing.
   *
   * Adding code here should only be done in exceptional cases.
   *
   * Some further points to keep in mind:
   *   - While typically versioning order should be respected in code below (i.e. versioning
   *     affecting older versions should be done first), _this is not a hard rule_. And it should
   *     not be assumed older code must not be checked when adding newer code.
   *   - Do not rely strongly on versioning numbers here. This code may be run on data from
   *     different Blender versions (through the usage of linked data), and all existing data have
   *     already been processed through the whole do_version during blendfile reading itself. So
   *     decision to apply some versioning on some data should mostly rely on the data itself.
   *   - Unlike the regular do_version code, this one should _not_ be assumed as 'valid forever'.
   *     It is closer to the Editing or BKE code in that respect, changes to the logic or data
   *     model of an ID will require a careful update here as well.
   *
   * Another critical weakness of this code is that it is currently _not_ performed on data linked
   * during an editing session, but only on data linked while reading a whole blendfile. This will
   * have to be fixed at some point.
   */

  /* NOTE: Version number is checked against Main version (i.e. current blend file version), AND
   * the versions of all the linked libraries. */

  if (!blendfile_or_libraries_versions_atleast(new_bmain, 250, 0)) {
    /* This happens here, because at this point in the versioning code there's
     * 'reports' available. */
    reports->pre_animato_file_loaded = true;
  }

  if (!blendfile_or_libraries_versions_atleast(new_bmain, 250, 0)) {
    LISTBASE_FOREACH (Scene *, scene, &new_bmain->scenes) {
      if (scene->ed) {
        blender::seq::doversion_250_sound_proxy_update(new_bmain, scene->ed);
      }
    }
  }

  if (!blendfile_or_libraries_versions_atleast(new_bmain, 302, 1)) {
    BKE_lib_override_library_main_proxy_convert(new_bmain, reports);
    /* Currently liboverride code can generate invalid namemap. This is a known issue, requires
     * #107847 to be properly fixed. */
    BKE_main_namemap_validate_and_fix(*new_bmain);
  }

  if (!blendfile_or_libraries_versions_atleast(new_bmain, 302, 3)) {
    /* Does not add any new IDs, but needs the full Main data-base. */
    BKE_lib_override_library_main_hierarchy_root_ensure(new_bmain);
  }

  if (!blendfile_or_libraries_versions_atleast(new_bmain, 402, 22)) {
    /* Initial auto smooth versioning started at (401, 2), but a bug caused the legacy flag to not
     * be cleared, so it is re-run in a later version when the bug is fixed and the versioning has
     * been made idempotent. */
    BKE_main_mesh_legacy_convert_auto_smooth(*new_bmain);
  }

  if (!blendfile_or_libraries_versions_atleast(new_bmain, 404, 2)) {
    /* Version all the action assignments of just-versioned datablocks. This MUST happen before the
     * GreasePencil conversion, as that assumes the Action Slots have already been assigned. */
    blender::animrig::versioning::convert_legacy_action_assignments(*new_bmain, reports->reports);
  }

  if (!blendfile_or_libraries_versions_atleast(new_bmain, 403, 3)) {
    /* Convert all the legacy grease pencil objects. This does not touch annotations. */
    blender::bke::greasepencil::convert::legacy_main(*new_bmain, lapp_context, *reports);
  }

  if (!blendfile_or_libraries_versions_atleast(new_bmain, 500, 4)) {
    LISTBASE_FOREACH (Scene *, scene, &new_bmain->scenes) {
      bNodeTree *ntree = scene->nodetree;
      if (!ntree) {
        continue;
      }
      ntree->id.flag &= ~ID_FLAG_EMBEDDED_DATA;
      ntree->owner_id = nullptr;
      ntree->id.tag |= ID_TAG_NO_MAIN;

      scene->compositing_node_group = ntree;
      scene->nodetree = nullptr;

      BKE_libblock_management_main_add(new_bmain, ntree);

      /* NOTE: The user count remains zero at this point. It will get automatically updated after
       * blend file reading is done. */
    }
  }
}
