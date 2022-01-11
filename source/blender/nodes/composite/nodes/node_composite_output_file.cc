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
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include <cstring>

#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"

#include "node_composite_util.hh"

#include "intern/openexr/openexr_multi.h"

/* **************** OUTPUT FILE ******************** */

/* find unique path */
static bool unique_path_unique_check(void *arg, const char *name)
{
  struct Args {
    ListBase *lb;
    bNodeSocket *sock;
  };
  Args *data = (Args *)arg;

  LISTBASE_FOREACH (bNodeSocket *, sock, data->lb) {
    if (sock != data->sock) {
      NodeImageMultiFileSocket *sockdata = (NodeImageMultiFileSocket *)sock->storage;
      if (STREQ(sockdata->path, name)) {
        return true;
      }
    }
  }
  return false;
}
void ntreeCompositOutputFileUniquePath(ListBase *list,
                                       bNodeSocket *sock,
                                       const char defname[],
                                       char delim)
{
  NodeImageMultiFileSocket *sockdata;
  struct {
    ListBase *lb;
    bNodeSocket *sock;
  } data;
  data.lb = list;
  data.sock = sock;

  /* See if we are given an empty string */
  if (ELEM(nullptr, sock, defname)) {
    return;
  }

  sockdata = (NodeImageMultiFileSocket *)sock->storage;
  BLI_uniquename_cb(
      unique_path_unique_check, &data, defname, delim, sockdata->path, sizeof(sockdata->path));
}

/* find unique EXR layer */
static bool unique_layer_unique_check(void *arg, const char *name)
{
  struct Args {
    ListBase *lb;
    bNodeSocket *sock;
  };
  Args *data = (Args *)arg;

  LISTBASE_FOREACH (bNodeSocket *, sock, data->lb) {
    if (sock != data->sock) {
      NodeImageMultiFileSocket *sockdata = (NodeImageMultiFileSocket *)sock->storage;
      if (STREQ(sockdata->layer, name)) {
        return true;
      }
    }
  }
  return false;
}
void ntreeCompositOutputFileUniqueLayer(ListBase *list,
                                        bNodeSocket *sock,
                                        const char defname[],
                                        char delim)
{
  struct {
    ListBase *lb;
    bNodeSocket *sock;
  } data;
  data.lb = list;
  data.sock = sock;

  /* See if we are given an empty string */
  if (ELEM(nullptr, sock, defname)) {
    return;
  }

  NodeImageMultiFileSocket *sockdata = (NodeImageMultiFileSocket *)sock->storage;
  BLI_uniquename_cb(
      unique_layer_unique_check, &data, defname, delim, sockdata->layer, sizeof(sockdata->layer));
}

bNodeSocket *ntreeCompositOutputFileAddSocket(bNodeTree *ntree,
                                              bNode *node,
                                              const char *name,
                                              ImageFormatData *im_format)
{
  NodeImageMultiFile *nimf = (NodeImageMultiFile *)node->storage;
  bNodeSocket *sock = nodeAddStaticSocket(
      ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, nullptr, name);

  /* create format data for the input socket */
  NodeImageMultiFileSocket *sockdata = MEM_cnew<NodeImageMultiFileSocket>(__func__);
  sock->storage = sockdata;

  BLI_strncpy_utf8(sockdata->path, name, sizeof(sockdata->path));
  ntreeCompositOutputFileUniquePath(&node->inputs, sock, name, '_');
  BLI_strncpy_utf8(sockdata->layer, name, sizeof(sockdata->layer));
  ntreeCompositOutputFileUniqueLayer(&node->inputs, sock, name, '_');

  if (im_format) {
    sockdata->format = *im_format;
    if (BKE_imtype_is_movie(sockdata->format.imtype)) {
      sockdata->format.imtype = R_IMF_IMTYPE_OPENEXR;
    }
  }
  else {
    BKE_imformat_defaults(&sockdata->format);
  }
  /* use node data format by default */
  sockdata->use_node_format = true;
  sockdata->save_as_render = true;

  nimf->active_input = BLI_findindex(&node->inputs, sock);

  return sock;
}

int ntreeCompositOutputFileRemoveActiveSocket(bNodeTree *ntree, bNode *node)
{
  NodeImageMultiFile *nimf = (NodeImageMultiFile *)node->storage;
  bNodeSocket *sock = (bNodeSocket *)BLI_findlink(&node->inputs, nimf->active_input);
  int totinputs = BLI_listbase_count(&node->inputs);

  if (!sock) {
    return 0;
  }

  if (nimf->active_input == totinputs - 1) {
    --nimf->active_input;
  }

  /* free format data */
  MEM_freeN(sock->storage);

  nodeRemoveSocket(ntree, node, sock);
  return 1;
}

void ntreeCompositOutputFileSetPath(bNode *node, bNodeSocket *sock, const char *name)
{
  NodeImageMultiFileSocket *sockdata = (NodeImageMultiFileSocket *)sock->storage;
  BLI_strncpy_utf8(sockdata->path, name, sizeof(sockdata->path));
  ntreeCompositOutputFileUniquePath(&node->inputs, sock, name, '_');
}

void ntreeCompositOutputFileSetLayer(bNode *node, bNodeSocket *sock, const char *name)
{
  NodeImageMultiFileSocket *sockdata = (NodeImageMultiFileSocket *)sock->storage;
  BLI_strncpy_utf8(sockdata->layer, name, sizeof(sockdata->layer));
  ntreeCompositOutputFileUniqueLayer(&node->inputs, sock, name, '_');
}

namespace blender::nodes::node_composite_output_file_cc {

/* XXX uses initfunc_api callback, regular initfunc does not support context yet */
static void init_output_file(const bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  bNodeTree *ntree = (bNodeTree *)ptr->owner_id;
  bNode *node = (bNode *)ptr->data;
  NodeImageMultiFile *nimf = MEM_cnew<NodeImageMultiFile>(__func__);
  ImageFormatData *format = nullptr;
  node->storage = nimf;

  if (scene) {
    RenderData *rd = &scene->r;

    BLI_strncpy(nimf->base_path, rd->pic, sizeof(nimf->base_path));
    nimf->format = rd->im_format;
    if (BKE_imtype_is_movie(nimf->format.imtype)) {
      nimf->format.imtype = R_IMF_IMTYPE_OPENEXR;
    }

    format = &nimf->format;
  }
  else {
    BKE_imformat_defaults(&nimf->format);
  }

  /* add one socket by default */
  ntreeCompositOutputFileAddSocket(ntree, node, "Image", format);
}

static void free_output_file(bNode *node)
{
  /* free storage data in sockets */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    MEM_freeN(sock->storage);
  }

  MEM_freeN(node->storage);
}

static void copy_output_file(bNodeTree *UNUSED(dest_ntree),
                             bNode *dest_node,
                             const bNode *src_node)
{
  bNodeSocket *src_sock, *dest_sock;

  dest_node->storage = MEM_dupallocN(src_node->storage);

  /* duplicate storage data in sockets */
  for (src_sock = (bNodeSocket *)src_node->inputs.first,
      dest_sock = (bNodeSocket *)dest_node->inputs.first;
       src_sock && dest_sock;
       src_sock = src_sock->next, dest_sock = (bNodeSocket *)dest_sock->next) {
    dest_sock->storage = MEM_dupallocN(src_sock->storage);
  }
}

static void update_output_file(bNodeTree *ntree, bNode *node)
{
  PointerRNA ptr;

  /* XXX fix for T36706: remove invalid sockets added with bpy API.
   * This is not ideal, but prevents crashes from missing storage.
   * FileOutput node needs a redesign to support this properly.
   */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (sock->storage == nullptr) {
      nodeRemoveSocket(ntree, node, sock);
    }
  }
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    nodeRemoveSocket(ntree, node, sock);
  }

  cmp_node_update_default(ntree, node);

  /* automatically update the socket type based on linked input */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (sock->link) {
      RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
      RNA_enum_set(&ptr, "type", sock->link->fromsock->type);
    }
  }
}

static void node_composit_buts_file_output(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  PointerRNA imfptr = RNA_pointer_get(ptr, "format");
  const bool multilayer = RNA_enum_get(&imfptr, "file_format") == R_IMF_IMTYPE_MULTILAYER;

  if (multilayer) {
    uiItemL(layout, IFACE_("Path:"), ICON_NONE);
  }
  else {
    uiItemL(layout, IFACE_("Base Path:"), ICON_NONE);
  }
  uiItemR(layout, ptr, "base_path", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_composit_buts_file_output_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  PointerRNA imfptr = RNA_pointer_get(ptr, "format");
  PointerRNA active_input_ptr, op_ptr;
  uiLayout *row, *col;
  const bool multilayer = RNA_enum_get(&imfptr, "file_format") == R_IMF_IMTYPE_MULTILAYER;
  const bool is_exr = RNA_enum_get(&imfptr, "file_format") == R_IMF_IMTYPE_OPENEXR;
  const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;

  node_composit_buts_file_output(layout, C, ptr);
  uiTemplateImageSettings(layout, &imfptr, false);

  /* disable stereo output for multilayer, too much work for something that no one will use */
  /* if someone asks for that we can implement it */
  if (is_multiview) {
    uiTemplateImageFormatViews(layout, &imfptr, nullptr);
  }

  uiItemS(layout);

  uiItemO(layout, IFACE_("Add Input"), ICON_ADD, "NODE_OT_output_file_add_socket");

  row = uiLayoutRow(layout, false);
  col = uiLayoutColumn(row, true);

  const int active_index = RNA_int_get(ptr, "active_input_index");
  /* using different collection properties if multilayer format is enabled */
  if (multilayer) {
    uiTemplateList(col,
                   C,
                   "UI_UL_list",
                   "file_output_node",
                   ptr,
                   "layer_slots",
                   ptr,
                   "active_input_index",
                   nullptr,
                   0,
                   0,
                   0,
                   0,
                   UI_TEMPLATE_LIST_FLAG_NONE);
    RNA_property_collection_lookup_int(
        ptr, RNA_struct_find_property(ptr, "layer_slots"), active_index, &active_input_ptr);
  }
  else {
    uiTemplateList(col,
                   C,
                   "UI_UL_list",
                   "file_output_node",
                   ptr,
                   "file_slots",
                   ptr,
                   "active_input_index",
                   nullptr,
                   0,
                   0,
                   0,
                   0,
                   UI_TEMPLATE_LIST_FLAG_NONE);
    RNA_property_collection_lookup_int(
        ptr, RNA_struct_find_property(ptr, "file_slots"), active_index, &active_input_ptr);
  }
  /* XXX collection lookup does not return the ID part of the pointer,
   * setting this manually here */
  active_input_ptr.owner_id = ptr->owner_id;

  col = uiLayoutColumn(row, true);
  wmOperatorType *ot = WM_operatortype_find("NODE_OT_output_file_move_active_socket", false);
  uiItemFullO_ptr(col, ot, "", ICON_TRIA_UP, nullptr, WM_OP_INVOKE_DEFAULT, 0, &op_ptr);
  RNA_enum_set(&op_ptr, "direction", 1);
  uiItemFullO_ptr(col, ot, "", ICON_TRIA_DOWN, nullptr, WM_OP_INVOKE_DEFAULT, 0, &op_ptr);
  RNA_enum_set(&op_ptr, "direction", 2);

  if (active_input_ptr.data) {
    if (multilayer) {
      col = uiLayoutColumn(layout, true);

      uiItemL(col, IFACE_("Layer:"), ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &active_input_ptr, "name", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
      uiItemFullO(row,
                  "NODE_OT_output_file_remove_active_socket",
                  "",
                  ICON_X,
                  nullptr,
                  WM_OP_EXEC_DEFAULT,
                  UI_ITEM_R_ICON_ONLY,
                  nullptr);
    }
    else {
      col = uiLayoutColumn(layout, true);

      uiItemL(col, IFACE_("File Subpath:"), ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &active_input_ptr, "path", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
      uiItemFullO(row,
                  "NODE_OT_output_file_remove_active_socket",
                  "",
                  ICON_X,
                  nullptr,
                  WM_OP_EXEC_DEFAULT,
                  UI_ITEM_R_ICON_ONLY,
                  nullptr);

      /* format details for individual files */
      imfptr = RNA_pointer_get(&active_input_ptr, "format");

      col = uiLayoutColumn(layout, true);
      uiItemL(col, IFACE_("Format:"), ICON_NONE);
      uiItemR(col,
              &active_input_ptr,
              "use_node_format",
              UI_ITEM_R_SPLIT_EMPTY_NAME,
              nullptr,
              ICON_NONE);

      const bool is_socket_exr = RNA_enum_get(&imfptr, "file_format") == R_IMF_IMTYPE_OPENEXR;
      const bool use_node_format = RNA_boolean_get(&active_input_ptr, "use_node_format");

      if ((!is_exr && use_node_format) || (!is_socket_exr && !use_node_format)) {
        uiItemR(col,
                &active_input_ptr,
                "save_as_render",
                UI_ITEM_R_SPLIT_EMPTY_NAME,
                nullptr,
                ICON_NONE);
      }

      col = uiLayoutColumn(layout, false);
      uiLayoutSetActive(col, use_node_format == false);
      uiTemplateImageSettings(col, &imfptr, false);

      if (is_multiview) {
        uiTemplateImageFormatViews(layout, &imfptr, nullptr);
      }
    }
  }
}

}  // namespace blender::nodes::node_composite_output_file_cc

void register_node_type_cmp_output_file()
{
  namespace file_ns = blender::nodes::node_composite_output_file_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_OUTPUT_FILE, "File Output", NODE_CLASS_OUTPUT);
  ntype.draw_buttons = file_ns::node_composit_buts_file_output;
  ntype.draw_buttons_ex = file_ns::node_composit_buts_file_output_ex;
  ntype.initfunc_api = file_ns::init_output_file;
  ntype.flag |= NODE_PREVIEW;
  node_type_storage(
      &ntype, "NodeImageMultiFile", file_ns::free_output_file, file_ns::copy_output_file);
  node_type_update(&ntype, file_ns::update_output_file);

  nodeRegisterType(&ntype);
}
