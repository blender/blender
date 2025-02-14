/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cstring>

#include "BLI_assert.h"
#include "BLI_index_range.hh"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_cryptomatte.hh"
#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_main.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_scene.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "WM_api.hh"

#include "IMB_imbuf.hh"

#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "NOD_socket_search_link.hh"

#include "node_composite_util.hh"

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
                                              const ImageFormatData *im_format)
{
  NodeImageMultiFile *nimf = (NodeImageMultiFile *)node->storage;
  bNodeSocket *sock = blender::bke::node_add_static_socket(
      ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, "", name);

  /* create format data for the input socket */
  NodeImageMultiFileSocket *sockdata = MEM_cnew<NodeImageMultiFileSocket>(__func__);
  sock->storage = sockdata;

  STRNCPY_UTF8(sockdata->path, name);
  ntreeCompositOutputFileUniquePath(&node->inputs, sock, name, '_');
  STRNCPY_UTF8(sockdata->layer, name);
  ntreeCompositOutputFileUniqueLayer(&node->inputs, sock, name, '_');

  if (im_format) {
    BKE_image_format_copy(&sockdata->format, im_format);
    sockdata->format.color_management = R_IMF_COLOR_MANAGEMENT_FOLLOW_SCENE;
    if (BKE_imtype_is_movie(sockdata->format.imtype)) {
      sockdata->format.imtype = R_IMF_IMTYPE_OPENEXR;
    }
  }
  else {
    BKE_image_format_init(&sockdata->format, false);
  }
  BKE_image_format_update_color_space_for_type(&sockdata->format);

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

  blender::bke::node_remove_socket(ntree, node, sock);
  return 1;
}

void ntreeCompositOutputFileSetPath(bNode *node, bNodeSocket *sock, const char *name)
{
  NodeImageMultiFileSocket *sockdata = (NodeImageMultiFileSocket *)sock->storage;
  STRNCPY_UTF8(sockdata->path, name);
  ntreeCompositOutputFileUniquePath(&node->inputs, sock, name, '_');
}

void ntreeCompositOutputFileSetLayer(bNode *node, bNodeSocket *sock, const char *name)
{
  NodeImageMultiFileSocket *sockdata = (NodeImageMultiFileSocket *)sock->storage;
  STRNCPY_UTF8(sockdata->layer, name);
  ntreeCompositOutputFileUniqueLayer(&node->inputs, sock, name, '_');
}

namespace blender::nodes::node_composite_file_output_cc {

NODE_STORAGE_FUNCS(NodeImageMultiFile)

/* XXX uses initfunc_api callback, regular initfunc does not support context yet */
static void init_output_file(const bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  bNodeTree *ntree = (bNodeTree *)ptr->owner_id;
  bNode *node = (bNode *)ptr->data;
  NodeImageMultiFile *nimf = MEM_cnew<NodeImageMultiFile>(__func__);
  nimf->save_as_render = true;
  ImageFormatData *format = nullptr;
  node->storage = nimf;

  if (scene) {
    RenderData *rd = &scene->r;

    STRNCPY(nimf->base_path, rd->pic);
    BKE_image_format_copy(&nimf->format, &rd->im_format);
    nimf->format.color_management = R_IMF_COLOR_MANAGEMENT_FOLLOW_SCENE;
    if (BKE_imtype_is_movie(nimf->format.imtype)) {
      nimf->format.imtype = R_IMF_IMTYPE_OPENEXR;
    }

    format = &nimf->format;
  }
  else {
    BKE_image_format_init(&nimf->format, false);
  }
  BKE_image_format_update_color_space_for_type(&nimf->format);

  /* add one socket by default */
  ntreeCompositOutputFileAddSocket(ntree, node, "Image", format);
}

static void free_output_file(bNode *node)
{
  /* free storage data in sockets */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    NodeImageMultiFileSocket *sockdata = (NodeImageMultiFileSocket *)sock->storage;
    BKE_image_format_free(&sockdata->format);
    MEM_freeN(sock->storage);
  }

  NodeImageMultiFile *nimf = (NodeImageMultiFile *)node->storage;
  BKE_image_format_free(&nimf->format);
  MEM_freeN(node->storage);
}

static void copy_output_file(bNodeTree * /*dst_ntree*/, bNode *dest_node, const bNode *src_node)
{
  bNodeSocket *src_sock, *dest_sock;

  dest_node->storage = MEM_dupallocN(src_node->storage);
  NodeImageMultiFile *dest_nimf = (NodeImageMultiFile *)dest_node->storage;
  NodeImageMultiFile *src_nimf = (NodeImageMultiFile *)src_node->storage;
  BKE_image_format_copy(&dest_nimf->format, &src_nimf->format);

  /* duplicate storage data in sockets */
  for (src_sock = (bNodeSocket *)src_node->inputs.first,
      dest_sock = (bNodeSocket *)dest_node->inputs.first;
       src_sock && dest_sock;
       src_sock = src_sock->next, dest_sock = (bNodeSocket *)dest_sock->next)
  {
    dest_sock->storage = MEM_dupallocN(src_sock->storage);
    NodeImageMultiFileSocket *dest_sockdata = (NodeImageMultiFileSocket *)dest_sock->storage;
    NodeImageMultiFileSocket *src_sockdata = (NodeImageMultiFileSocket *)src_sock->storage;
    BKE_image_format_copy(&dest_sockdata->format, &src_sockdata->format);
  }
}

static void update_output_file(bNodeTree *ntree, bNode *node)
{
  /* XXX fix for #36706: remove invalid sockets added with bpy API.
   * This is not ideal, but prevents crashes from missing storage.
   * FileOutput node needs a redesign to support this properly.
   */
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->inputs) {
    if (sock->storage == nullptr) {
      blender::bke::node_remove_socket(ntree, node, sock);
    }
  }
  LISTBASE_FOREACH_MUTABLE (bNodeSocket *, sock, &node->outputs) {
    blender::bke::node_remove_socket(ntree, node, sock);
  }

  cmp_node_update_default(ntree, node);

  /* automatically update the socket type based on linked input */
  ntree->ensure_topology_cache();
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (sock->is_logically_linked()) {
      const bNodeSocket *from_socket = sock->logically_linked_sockets()[0];
      if (sock->type != from_socket->type) {
        blender::bke::node_modify_socket_type_static(ntree, node, sock, from_socket->type, 0);
        BKE_ntree_update_tag_socket_property(ntree, sock);
      }
    }
  }
}

static void node_composit_buts_file_output(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
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
  const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;

  node_composit_buts_file_output(layout, C, ptr);

  {
    uiLayout *column = uiLayoutColumn(layout, true);
    uiLayoutSetPropSep(column, true);
    uiLayoutSetPropDecorate(column, false);
    uiItemR(column, ptr, "save_as_render", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }
  const bool save_as_render = RNA_boolean_get(ptr, "save_as_render");
  uiTemplateImageSettings(layout, &imfptr, save_as_render);

  if (!save_as_render) {
    uiLayout *col = uiLayoutColumn(layout, true);
    uiLayoutSetPropSep(col, true);
    uiLayoutSetPropDecorate(col, false);

    PointerRNA linear_settings_ptr = RNA_pointer_get(&imfptr, "linear_colorspace_settings");
    uiItemR(col, &linear_settings_ptr, "name", UI_ITEM_NONE, IFACE_("Color Space"), ICON_NONE);
  }

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
  uiItemFullO_ptr(col, ot, "", ICON_TRIA_UP, nullptr, WM_OP_INVOKE_DEFAULT, UI_ITEM_NONE, &op_ptr);
  RNA_enum_set(&op_ptr, "direction", 1);
  uiItemFullO_ptr(
      col, ot, "", ICON_TRIA_DOWN, nullptr, WM_OP_INVOKE_DEFAULT, UI_ITEM_NONE, &op_ptr);
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
              std::nullopt,
              ICON_NONE);

      const bool use_node_format = RNA_boolean_get(&active_input_ptr, "use_node_format");

      if (!use_node_format) {
        {
          uiLayout *column = uiLayoutColumn(layout, true);
          uiLayoutSetPropSep(column, true);
          uiLayoutSetPropDecorate(column, false);
          uiItemR(column,
                  &active_input_ptr,
                  "save_as_render",
                  UI_ITEM_R_SPLIT_EMPTY_NAME,
                  std::nullopt,
                  ICON_NONE);
        }

        const bool use_color_management = RNA_boolean_get(&active_input_ptr, "save_as_render");

        col = uiLayoutColumn(layout, false);
        uiTemplateImageSettings(col, &imfptr, use_color_management);

        if (!use_color_management) {
          uiLayout *col = uiLayoutColumn(layout, true);
          uiLayoutSetPropSep(col, true);
          uiLayoutSetPropDecorate(col, false);

          PointerRNA linear_settings_ptr = RNA_pointer_get(&imfptr, "linear_colorspace_settings");
          uiItemR(
              col, &linear_settings_ptr, "name", UI_ITEM_NONE, IFACE_("Color Space"), ICON_NONE);
        }

        if (is_multiview) {
          col = uiLayoutColumn(layout, false);
          uiTemplateImageFormatViews(col, &imfptr, nullptr);
        }
      }
    }
  }
}

using namespace blender::compositor;

class FileOutputOperation : public NodeOperation {
 public:
  FileOutputOperation(Context &context, DNode node) : NodeOperation(context, node)
  {
    for (const bNodeSocket *input : node->input_sockets()) {
      InputDescriptor &descriptor = this->get_input_descriptor(input->identifier);
      /* Inputs for multi-layer files need to be the same size, while they can be different for
       * individual file outputs. */
      descriptor.realization_mode = this->is_multi_layer() ?
                                        InputRealizationMode::OperationDomain :
                                        InputRealizationMode::Transforms;
    }
  }

  void execute() override
  {
    if (is_multi_layer()) {
      execute_multi_layer();
    }
    else {
      execute_single_layer();
    }
  }

  /* --------------------
   * Single Layer Images.
   */

  void execute_single_layer()
  {
    for (const bNodeSocket *input : this->node()->input_sockets()) {
      const Result &result = get_input(input->identifier);
      /* We only write images, not single values. */
      if (result.is_single_value()) {
        continue;
      }

      char base_path[FILE_MAX];
      const auto &socket = *static_cast<NodeImageMultiFileSocket *>(input->storage);
      get_single_layer_image_base_path(socket.path, base_path);

      /* The image saving code expects EXR images to have a different structure than standard
       * images. In particular, in EXR images, the buffers need to be stored in passes that are, in
       * turn, stored in a render layer. On the other hand, in non-EXR images, the buffers need to
       * be stored in views. An exception to this is stereo images, which needs to have the same
       * structure as non-EXR images. */
      const auto &format = socket.use_node_format ? node_storage(bnode()).format : socket.format;
      const bool save_as_render = socket.use_node_format ? node_storage(bnode()).save_as_render :
                                                           socket.save_as_render;
      const bool is_exr = format.imtype == R_IMF_IMTYPE_OPENEXR;
      const int views_count = BKE_scene_multiview_num_views_get(&context().get_render_data());
      if (is_exr && !(format.views_format == R_IMF_VIEWS_STEREO_3D && views_count == 2)) {
        execute_single_layer_multi_view_exr(result, format, base_path, socket.layer);
        continue;
      }

      char image_path[FILE_MAX];
      get_single_layer_image_path(base_path, format, image_path);

      const int2 size = result.domain().size;
      FileOutput &file_output = context().render_context()->get_file_output(
          image_path, format, size, save_as_render);

      add_view_for_result(file_output, result, context().get_view_name().data());

      add_meta_data_for_result(file_output, result, socket.layer);
    }
  }

  /* -----------------------------------
   * Single Layer Multi-View EXR Images.
   */

  void execute_single_layer_multi_view_exr(const Result &result,
                                           const ImageFormatData &format,
                                           const char *base_path,
                                           const char *layer_name)
  {
    const bool has_views = format.views_format != R_IMF_VIEWS_INDIVIDUAL;

    /* The EXR stores all views in the same file, so we supply an empty view to make sure the file
     * name does not contain a view suffix. */
    char image_path[FILE_MAX];
    const char *path_view = has_views ? "" : context().get_view_name().data();
    get_multi_layer_exr_image_path(base_path, path_view, image_path);

    const int2 size = result.domain().size;
    FileOutput &file_output = context().render_context()->get_file_output(
        image_path, format, size, true);

    /* The EXR stores all views in the same file, so we add the actual render view. Otherwise, we
     * add a default unnamed view. */
    const char *view_name = has_views ? context().get_view_name().data() : "";
    file_output.add_view(view_name);
    add_pass_for_result(file_output, result, "", view_name);

    add_meta_data_for_result(file_output, result, layer_name);
  }

  /* -----------------------
   * Multi-Layer EXR Images.
   */

  void execute_multi_layer()
  {
    const bool store_views_in_single_file = is_multi_view_exr();
    const char *view = context().get_view_name().data();

    /* If we are saving all views in a single multi-layer file, we supply an empty view to make
     * sure the file name does not contain a view suffix. */
    char image_path[FILE_MAX];
    const char *write_view = store_views_in_single_file ? "" : view;
    get_multi_layer_exr_image_path(get_base_path(), write_view, image_path);

    const int2 size = compute_domain().size;
    const ImageFormatData format = node_storage(bnode()).format;
    FileOutput &file_output = context().render_context()->get_file_output(
        image_path, format, size, true);

    /* If we are saving views in separate files, we needn't store the view in the channel names, so
     * we add an unnamed view. */
    const char *pass_view = store_views_in_single_file ? view : "";
    file_output.add_view(pass_view);

    for (const bNodeSocket *input : this->node()->input_sockets()) {
      const Result &input_result = get_input(input->identifier);
      const char *pass_name = (static_cast<NodeImageMultiFileSocket *>(input->storage))->layer;
      add_pass_for_result(file_output, input_result, pass_name, pass_view);

      add_meta_data_for_result(file_output, input_result, pass_name);
    }
  }

  /* Read the data stored in the given result and add a pass of the given name, view, and read
   * buffer. The pass channel identifiers follows the EXR conventions. */
  void add_pass_for_result(FileOutput &file_output,
                           const Result &result,
                           const char *pass_name,
                           const char *view_name)
  {
    /* For single values, we fill a buffer that covers the domain of the operation with the value
     * of the result. */
    const int2 size = result.is_single_value() ? this->compute_domain().size :
                                                 result.domain().size;

    /* The image buffer in the file output will take ownership of this buffer and freeing it will
     * be its responsibility. */
    float *buffer = nullptr;
    if (result.is_single_value()) {
      buffer = this->inflate_result(result, size);
    }
    else {
      if (context().use_gpu()) {
        GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
        buffer = static_cast<float *>(GPU_texture_read(result, GPU_DATA_FLOAT, 0));
      }
      else {
        /* Copy the result into a new buffer. */
        const int64_t buffer_size = int64_t(size.x) * size.y * result.channels_count();
        buffer = static_cast<float *>(
            MEM_malloc_arrayN(buffer_size, sizeof(float), "File Output Buffer Copy."));
        threading::parallel_for(IndexRange(buffer_size), 1024, [&](const IndexRange sub_range) {
          for (const int64_t i : sub_range) {
            buffer[i] = result.float_texture()[i];
          }
        });
      }
    }

    switch (result.type()) {
      case ResultType::Color:
        /* Use lowercase rgba for Cryptomatte layers because the EXR internal compression rules
         * specify that all uppercase RGBA channels will be compressed, and Cryptomatte should not
         * be compressed. */
        if (result.meta_data.is_cryptomatte_layer()) {
          file_output.add_pass(pass_name, view_name, "rgba", buffer);
        }
        else {
          file_output.add_pass(pass_name, view_name, "RGBA", buffer);
        }
        break;
      case ResultType::Vector:
        if (result.meta_data.is_4d_vector) {
          file_output.add_pass(pass_name, view_name, "XYZW", buffer);
        }
        else {
          file_output.add_pass(pass_name, view_name, "XYZ", float4_to_float3_image(size, buffer));
        }
        break;
      case ResultType::Float:
        file_output.add_pass(pass_name, view_name, "V", buffer);
        break;
      default:
        /* Other types are internal and needn't be handled by operations. */
        BLI_assert_unreachable();
        break;
    }
  }

  /* Allocates and fills an image buffer of the specified size with the value of the given single
   * value result. */
  float *inflate_result(const Result &result, const int2 size)
  {
    BLI_assert(result.is_single_value());

    switch (result.type()) {
      case ResultType::Float: {
        float *buffer = static_cast<float *>(MEM_malloc_arrayN(
            size_t(size.x) * size.y, sizeof(float), "File Output Inflated Buffer."));

        const float value = result.get_single_value<float>();
        parallel_for(
            size, [&](const int2 texel) { buffer[int64_t(texel.y) * size.x + texel.x] = value; });
        return buffer;
      }
      case ResultType::Vector:
      case ResultType::Color: {
        float *buffer = static_cast<float *>(MEM_malloc_arrayN(
            size_t(size.x) * size.y, sizeof(float[4]), "File Output Inflated Buffer."));

        const float4 value = result.type() == ResultType::Color ?
                                 result.get_single_value<float4>() :
                                 result.get_single_value<float4>();
        parallel_for(size, [&](const int2 texel) {
          copy_v4_v4(buffer + ((int64_t(texel.y) * size.x + texel.x) * 4), value);
        });
        return buffer;
      }
      default:
        /* Other types are internal and needn't be handled by operations. */
        break;
    }

    BLI_assert_unreachable();
    return nullptr;
  }

  /* Read the data stored the given result and add a view of the given name and read buffer. */
  void add_view_for_result(FileOutput &file_output, const Result &result, const char *view_name)
  {
    /* The image buffer in the file output will take ownership of this buffer and freeing it will
     * be its responsibility. */
    float *buffer = nullptr;
    if (context().use_gpu()) {
      GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
      buffer = static_cast<float *>(GPU_texture_read(result, GPU_DATA_FLOAT, 0));
    }
    else {
      /* Copy the result into a new buffer. */
      const int2 size = result.domain().size;
      const int64_t buffer_size = int64_t(size.x) * size.y * result.channels_count();
      buffer = static_cast<float *>(
          MEM_malloc_arrayN(buffer_size, sizeof(float), "File Output Buffer Copy."));
      threading::parallel_for(IndexRange(buffer_size), 1024, [&](const IndexRange sub_range) {
        for (const int64_t i : sub_range) {
          buffer[i] = result.float_texture()[i];
        }
      });
    }

    const int2 size = result.domain().size;
    switch (result.type()) {
      case ResultType::Color:
        file_output.add_view(view_name, 4, buffer);
        break;
      case ResultType::Vector:
        file_output.add_view(view_name, 3, float4_to_float3_image(size, buffer));
        break;
      case ResultType::Float:
        file_output.add_view(view_name, 1, buffer);
        break;
      default:
        /* Other types are internal and needn't be handled by operations. */
        BLI_assert_unreachable();
        break;
    }
  }

  /* Given a float4 image, return a newly allocated float3 image that ignores the last channel. The
   * input image is freed. */
  float *float4_to_float3_image(int2 size, float *float4_image)
  {
    float *float3_image = static_cast<float *>(MEM_malloc_arrayN(
        size_t(size.x) * size.y, sizeof(float[3]), "File Output Vector Buffer."));

    parallel_for(size, [&](const int2 texel) {
      for (int i = 0; i < 3; i++) {
        const int64_t pixel_index = int64_t(texel.y) * size.x + texel.x;
        float3_image[pixel_index * 3 + i] = float4_image[pixel_index * 4 + i];
      }
    });

    MEM_freeN(float4_image);
    return float3_image;
  }

  /* Add Cryptomatte meta data to the file if they exist for the given result of the given layer
   * name. We do not write any other meta data for now. */
  void add_meta_data_for_result(FileOutput &file_output, const Result &result, const char *name)
  {
    StringRef cryptomatte_layer_name = bke::cryptomatte::BKE_cryptomatte_extract_layer_name(name);

    if (result.meta_data.is_cryptomatte_layer()) {
      file_output.add_meta_data(
          bke::cryptomatte::BKE_cryptomatte_meta_data_key(cryptomatte_layer_name, "name"),
          cryptomatte_layer_name);
    }

    if (!result.meta_data.cryptomatte.manifest.empty()) {
      file_output.add_meta_data(
          bke::cryptomatte::BKE_cryptomatte_meta_data_key(cryptomatte_layer_name, "manifest"),
          result.meta_data.cryptomatte.manifest);
    }

    if (!result.meta_data.cryptomatte.hash.empty()) {
      file_output.add_meta_data(
          bke::cryptomatte::BKE_cryptomatte_meta_data_key(cryptomatte_layer_name, "hash"),
          result.meta_data.cryptomatte.hash);
    }

    if (!result.meta_data.cryptomatte.conversion.empty()) {
      file_output.add_meta_data(
          bke::cryptomatte::BKE_cryptomatte_meta_data_key(cryptomatte_layer_name, "conversion"),
          result.meta_data.cryptomatte.conversion);
    }
  }

  /* Get the base path of the image to be saved, based on the base path of the node. The base name
   * is an optional initial name of the image, which will later be concatenated with other
   * information like the frame number, view, and extension. If the base name is empty, then the
   * base path represents a directory, so a trailing slash is ensured. */
  void get_single_layer_image_base_path(const char *base_name, char *base_path)
  {
    if (base_name[0]) {
      BLI_path_join(base_path, FILE_MAX, get_base_path(), base_name);
    }
    else {
      BLI_strncpy(base_path, get_base_path(), FILE_MAX);
      BLI_path_slash_ensure(base_path, FILE_MAX);
    }
  }

  /* Get the path of the image to be saved based on the given format. */
  void get_single_layer_image_path(const char *base_path,
                                   const ImageFormatData &format,
                                   char *image_path)
  {
    BKE_image_path_from_imformat(image_path,
                                 base_path,
                                 BKE_main_blendfile_path_from_global(),
                                 context().get_frame_number(),
                                 &format,
                                 use_file_extension(),
                                 true,
                                 nullptr);
  }

  /* Get the path of the EXR image to be saved. If the given view is not empty, its corresponding
   * file suffix will be appended to the name. */
  void get_multi_layer_exr_image_path(const char *base_path, const char *view, char *image_path)
  {
    const char *suffix = BKE_scene_multiview_view_suffix_get(&context().get_render_data(), view);
    BKE_image_path_from_imtype(image_path,
                               base_path,
                               BKE_main_blendfile_path_from_global(),
                               context().get_frame_number(),
                               R_IMF_IMTYPE_MULTILAYER,
                               use_file_extension(),
                               true,
                               suffix);
  }

  bool is_multi_layer()
  {
    return node_storage(bnode()).format.imtype == R_IMF_IMTYPE_MULTILAYER;
  }

  const char *get_base_path()
  {
    return node_storage(bnode()).base_path;
  }

  /* Add the file format extensions to the rendered file name. */
  bool use_file_extension()
  {
    return context().get_render_data().scemode & R_EXTENSION;
  }

  /* If true, save views in a multi-view EXR file, otherwise, save each view in its own file. */
  bool is_multi_view_exr()
  {
    if (!is_multi_view_scene()) {
      return false;
    }

    return node_storage(bnode()).format.views_format == R_IMF_VIEWS_MULTIVIEW;
  }

  bool is_multi_view_scene()
  {
    return context().get_render_data().scemode & R_MULTIVIEW;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new FileOutputOperation(context, node);
}

}  // namespace blender::nodes::node_composite_file_output_cc

void register_node_type_cmp_output_file()
{
  namespace file_ns = blender::nodes::node_composite_file_output_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeOutputFile", CMP_NODE_OUTPUT_FILE);
  ntype.ui_name = "File Output";
  ntype.ui_description = "Write image file to disk";
  ntype.enum_name_legacy = "OUTPUT_FILE";
  ntype.nclass = NODE_CLASS_OUTPUT;
  ntype.draw_buttons = file_ns::node_composit_buts_file_output;
  ntype.draw_buttons_ex = file_ns::node_composit_buts_file_output_ex;
  ntype.initfunc_api = file_ns::init_output_file;
  ntype.flag |= NODE_PREVIEW;
  blender::bke::node_type_storage(
      &ntype, "NodeImageMultiFile", file_ns::free_output_file, file_ns::copy_output_file);
  ntype.updatefunc = file_ns::update_output_file;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
