/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cstring>

#include "BLI_assert.h"
#include "BLI_cpp_type.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BLO_read_write.hh"

#include "BKE_context.hh"
#include "BKE_cryptomatte.hh"
#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"

#include "IMB_imbuf.hh"

#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "NOD_compositor_file_output.hh"
#include "NOD_node_extra_info.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "node_composite_util.hh"

namespace path_templates = blender::bke::path_templates;

namespace blender::nodes::node_composite_file_output_cc {

NODE_STORAGE_FUNCS(NodeCompositorFileOutput)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_default_layout();

  const bNodeTree *node_tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (!node_tree || !node) {
    return;
  }

  const NodeCompositorFileOutput &storage = node_storage(*node);

  /* Inputs for multi-layer files need to be the same size, while they can be different for
   * individual file outputs. */
  const bool is_multi_layer = storage.format.imtype == R_IMF_IMTYPE_MULTILAYER;
  const CompositorInputRealizationMode realization_mode =
      is_multi_layer ? CompositorInputRealizationMode::OperationDomain :
                       CompositorInputRealizationMode::Transforms;

  for (const int i : IndexRange(storage.items_count)) {
    const NodeCompositorFileOutputItem &item = storage.items[i];
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    const std::string identifier = FileOutputItemsAccessor::socket_identifier_for_item(item);
    BaseSocketDeclarationBuilder *declaration = nullptr;
    if (socket_type == SOCK_VECTOR) {
      declaration = &b.add_input<decl::Vector>(item.name, identifier)
                         .dimensions(item.vector_socket_dimensions);
    }
    else {
      declaration = &b.add_input(socket_type, item.name, identifier);
    }
    declaration->structure_type(StructureType::Dynamic)
        .compositor_realization_mode(realization_mode)
        .socket_name_ptr(&node_tree->id, FileOutputItemsAccessor::item_srna, &item, "name");
  }

  b.add_input<decl::Extend>("", "__extend__");
}

static void node_init(const bContext *C, PointerRNA *node_pointer)
{
  bNode *node = node_pointer->data_as<bNode>();
  NodeCompositorFileOutput *data = MEM_callocN<NodeCompositorFileOutput>(__func__);
  node->storage = data;
  data->save_as_render = true;
  data->file_name = BLI_strdup("file_name");

  BKE_image_format_init(&data->format);
  BKE_image_format_media_type_set(
      &data->format, node_pointer->owner_id, MEDIA_TYPE_MULTI_LAYER_IMAGE);
  BKE_image_format_update_color_space_for_type(&data->format);

  Scene *scene = CTX_data_scene(C);
  if (scene) {
    const RenderData *render_data = &scene->r;
    STRNCPY(data->directory, render_data->pic);
  }
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<FileOutputItemsAccessor>(*node);
  NodeCompositorFileOutput &data = node_storage(*node);
  BKE_image_format_free(&data.format);
  MEM_SAFE_FREE(data.file_name);
  MEM_freeN(&data);
}

static void node_copy_storage(bNodeTree * /*destination_node_tree*/,
                              bNode *destination_node,
                              const bNode *source_node)
{
  const NodeCompositorFileOutput &source_storage = node_storage(*source_node);
  NodeCompositorFileOutput *destination_storage = MEM_dupallocN<NodeCompositorFileOutput>(
      __func__, source_storage);
  destination_storage->file_name = BLI_strdup_null(source_storage.file_name);
  BKE_image_format_copy(&destination_storage->format, &source_storage.format);
  destination_node->storage = destination_storage;
  socket_items::copy_array<FileOutputItemsAccessor>(*source_node, *destination_node);
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  return socket_items::try_add_item_via_any_extend_socket<FileOutputItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_operators()
{
  socket_items::ops::make_common_operators<FileOutputItemsAccessor>();
}

/* Computes the path of the image to be saved based on the given parameters. The given file name
 * suffix, if not empty, will be added to the file name. If the given view is not empty, its file
 * suffix will be appended to the name. The frame number, scene, and node are provides for variable
 * substitution in the path. If there are any errors processing the path, they will be returned. */
static Vector<path_templates::Error> compute_image_path(const StringRefNull directory,
                                                        const StringRefNull file_name,
                                                        const StringRefNull file_name_suffix,
                                                        const char *view,
                                                        const int frame_number,
                                                        const ImageFormatData &format,
                                                        const Scene &scene,
                                                        const bNode &node,
                                                        const bool is_animation_render,
                                                        char *r_image_path)
{
  char base_path[FILE_MAX] = "";
  STRNCPY(base_path, directory.c_str());
  const std::string full_file_name = file_name + file_name_suffix;
  BLI_path_append(base_path, FILE_MAX, full_file_name.c_str());

  path_templates::VariableMap template_variables;
  BKE_add_template_variables_general(template_variables, &node.owner_tree().id);
  BKE_add_template_variables_for_render_path(template_variables, scene);
  BKE_add_template_variables_for_node(template_variables, node);

  /* Substitute #### frame variables if not doing an animation render. For animation renders, this
   * is handled internally by the following function. */
  if (!is_animation_render) {
    BLI_path_frame(base_path, FILE_MAX, frame_number, 0);
  }

  return BKE_image_path_from_imformat(r_image_path,
                                      base_path,
                                      BKE_main_blendfile_path_from_global(),
                                      &template_variables,
                                      frame_number,
                                      &format,
                                      scene.r.scemode & R_EXTENSION,
                                      is_animation_render,
                                      BKE_scene_multiview_view_suffix_get(&scene.r, view));
}

static void node_layout(uiLayout *layout, bContext * /*context*/, PointerRNA *node_pointer)
{
  layout->prop(node_pointer, "directory", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  layout->prop(node_pointer, "file_name", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void format_layout(uiLayout *layout,
                          bContext *context,
                          PointerRNA *format_pointer,
                          PointerRNA *node_or_item_pointer)
{
  uiLayout *column = &layout->column(true);
  column->use_property_split_set(true);
  column->use_property_decorate_set(false);
  column->prop(
      node_or_item_pointer, "save_as_render", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  const bool save_as_render = RNA_boolean_get(node_or_item_pointer, "save_as_render");
  uiTemplateImageSettings(layout, context, format_pointer, save_as_render);

  if (!save_as_render) {
    uiLayout *column = &layout->column(true);
    column->use_property_split_set(true);
    column->use_property_decorate_set(false);

    PointerRNA linear_settings_ptr = RNA_pointer_get(format_pointer, "linear_colorspace_settings");
    column->prop(&linear_settings_ptr, "name", UI_ITEM_NONE, IFACE_("Color Space"), ICON_NONE);
  }

  Scene *scene = CTX_data_scene(context);
  const bool is_multiview = scene->r.scemode & R_MULTIVIEW;
  if (is_multiview) {
    uiTemplateImageFormatViews(layout, format_pointer, nullptr);
  }
}

static void output_path_layout(uiLayout *layout,
                               const StringRefNull directory,
                               const StringRefNull file_name,
                               const StringRefNull file_name_suffix,
                               const char *view,
                               const ImageFormatData &format,
                               const Scene &scene,
                               const bNode &node)
{

  char image_path[FILE_MAX];
  const Vector<path_templates::Error> path_errors = compute_image_path(directory,
                                                                       file_name,
                                                                       file_name_suffix,
                                                                       view,
                                                                       scene.r.cfra,
                                                                       format,
                                                                       scene,
                                                                       node,
                                                                       false,
                                                                       image_path);

  if (path_errors.is_empty()) {
    layout->label(image_path, ICON_FILE_IMAGE);
  }
  else {
    for (const path_templates::Error &error : path_errors) {
      layout->label(BKE_path_template_error_to_string(error, image_path).c_str(), ICON_ERROR);
    }
  }
}

static void output_paths_layout(uiLayout *layout,
                                bContext *context,
                                const StringRefNull file_name_suffix,
                                const bNode &node,
                                const ImageFormatData &format)
{
  const NodeCompositorFileOutput &storage = node_storage(node);
  const StringRefNull directory = storage.directory;
  const std::string file_name = storage.file_name ? storage.file_name : "";
  const Scene &scene = *CTX_data_scene(context);

  if (bool(scene.r.scemode & R_MULTIVIEW) && format.views_format == R_IMF_VIEWS_MULTIVIEW) {
    LISTBASE_FOREACH (SceneRenderView *, view, &scene.r.views) {
      if (!BKE_scene_multiview_is_render_view_active(&scene.r, view)) {
        continue;
      }

      output_path_layout(
          layout, directory, file_name, file_name_suffix, view->name, format, scene, node);
    }
  }
  else {
    output_path_layout(layout, directory, file_name, file_name_suffix, "", format, scene, node);
  }
}

static void item_layout(uiLayout *layout,
                        bContext *context,
                        PointerRNA *node_pointer,
                        PointerRNA *item_pointer,
                        const bool is_multi_layer)
{
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  layout->prop(item_pointer, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (RNA_enum_get(item_pointer, "socket_type") == SOCK_VECTOR) {
    layout->prop(item_pointer, "vector_socket_dimensions", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (is_multi_layer) {
    return;
  }

  layout->prop(
      item_pointer, "override_node_format", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  const bool override_node_format = RNA_boolean_get(item_pointer, "override_node_format");

  PointerRNA node_format_pointer = RNA_pointer_get(node_pointer, "format");
  PointerRNA item_format_pointer = RNA_pointer_get(item_pointer, "format");
  PointerRNA *format_pointer = override_node_format ? &item_format_pointer : &node_format_pointer;

  if (override_node_format) {
    if (uiLayout *panel = layout->panel(context, "item_format", false, IFACE_("Item Format"))) {
      format_layout(panel, context, format_pointer, item_pointer);
    }
  }
}

static void node_layout_ex(uiLayout *layout, bContext *context, PointerRNA *node_pointer)
{
  node_layout(layout, context, node_pointer);

  PointerRNA format_pointer = RNA_pointer_get(node_pointer, "format");
  const bool is_multi_layer = RNA_enum_get(&format_pointer, "file_format") ==
                              R_IMF_IMTYPE_MULTILAYER;
  layout->prop(&format_pointer, "media_type", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  if (uiLayout *panel = layout->panel(context, "node_format", false, IFACE_("Node Format"))) {
    format_layout(panel, context, &format_pointer, node_pointer);
  }

  const char *panel_name = is_multi_layer ? IFACE_("Layers") : IFACE_("Images");
  if (uiLayout *panel = layout->panel(context, "file_output_items", false, panel_name)) {
    bNodeTree &tree = *reinterpret_cast<bNodeTree *>(node_pointer->owner_id);
    bNode &node = *node_pointer->data_as<bNode>();
    socket_items::ui::draw_items_list_with_operators<FileOutputItemsAccessor>(
        context, panel, tree, node);
    socket_items::ui::draw_active_item_props<FileOutputItemsAccessor>(
        tree, node, [&](PointerRNA *item_pointer) {
          item_layout(panel, context, node_pointer, item_pointer, is_multi_layer);
        });
  }

  if (uiLayout *panel = layout->panel(context, "output_paths", true, IFACE_("Output Paths"))) {
    const bNode &node = *node_pointer->data_as<bNode>();
    const ImageFormatData &node_format = *format_pointer.data_as<ImageFormatData>();

    if (is_multi_layer) {
      output_paths_layout(panel, context, "", node, node_format);
    }
    else {
      const NodeCompositorFileOutput &storage = node_storage(node);
      for (const int i : IndexRange(storage.items_count)) {
        const NodeCompositorFileOutputItem &item = storage.items[i];
        const auto &format = item.override_node_format ? item.format : storage.format;
        output_paths_layout(panel, context, item.name, node, format);
      }
    }
  }
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  const NodeCompositorFileOutput &data = node_storage(node);
  BLO_write_string(&writer, data.file_name);
  BKE_image_format_blend_write(&writer, const_cast<ImageFormatData *>(&data.format));
  socket_items::blend_write<FileOutputItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  NodeCompositorFileOutput &data = node_storage(node);
  BLO_read_string(&reader, &data.file_name);
  BKE_image_format_blend_read_data(&reader, &data.format);
  socket_items::blend_read_data<FileOutputItemsAccessor>(&reader, node);
}

static void node_extra_info(NodeExtraInfoParams &parameters)
{
  SpaceNode *space_node = CTX_wm_space_node(&parameters.C);
  if (space_node->node_tree_sub_type != SNODE_COMPOSITOR_SCENE) {
    NodeExtraInfoRow row;
    row.text = RPT_("Node Unsupported");
    row.tooltip = TIP_("The File Output node is only supported for scene compositing");
    row.icon = ICON_ERROR;
    parameters.rows.append(std::move(row));
  }
}

using namespace blender::compositor;

class FileOutputOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (this->is_multi_layer()) {
      this->execute_multi_layer();
    }
    else {
      this->execute_single_layer();
    }
  }

  /* --------------------
   * Single Layer Images.
   */

  void execute_single_layer()
  {
    const NodeCompositorFileOutput &storage = node_storage(this->bnode());
    for (const int i : IndexRange(storage.items_count)) {
      const NodeCompositorFileOutputItem &item = storage.items[i];
      const std::string identifier = FileOutputItemsAccessor::socket_identifier_for_item(item);
      const Result &result = this->get_input(identifier);
      /* We only write images, not single values. */
      if (result.is_single_value()) {
        continue;
      }

      /* The image saving code expects EXR images to have a different structure than standard
       * images. In particular, in EXR images, the buffers need to be stored in passes that are, in
       * turn, stored in a render layer. On the other hand, in non-EXR images, the buffers need to
       * be stored in views. An exception to this is stereo images, which needs to have the same
       * structure as non-EXR images. */
      const auto &format = item.override_node_format ? item.format :
                                                       node_storage(this->bnode()).format;
      const bool save_as_render = item.override_node_format ?
                                      item.save_as_render :
                                      node_storage(this->bnode()).save_as_render;
      const bool is_exr = format.imtype == R_IMF_IMTYPE_OPENEXR;
      const int views_count = BKE_scene_multiview_num_views_get(
          &this->context().get_render_data());
      if (is_exr && !(format.views_format == R_IMF_VIEWS_STEREO_3D && views_count == 2)) {
        this->execute_single_layer_multi_view_exr(result, format, item.name);
        continue;
      }

      char image_path[FILE_MAX];
      Vector<path_templates::Error> path_errors = this->get_image_path(
          format, item.name, "", image_path);
      if (!path_errors.is_empty()) {
        continue;
      }

      const int2 size = result.domain().size;
      FileOutput &file_output = this->context().render_context()->get_file_output(
          image_path, format, size, save_as_render);

      this->add_view_for_result(file_output, result, context().get_view_name().data());

      this->add_meta_data_for_result(file_output, result, item.name);
    }
  }

  /* -----------------------------------
   * Single Layer Multi-View EXR Images.
   */

  void execute_single_layer_multi_view_exr(const Result &result,
                                           const ImageFormatData &format,
                                           const char *layer_name)
  {
    const bool has_views = format.views_format != R_IMF_VIEWS_INDIVIDUAL;

    /* The EXR stores all views in the same file, so we supply an empty view to make sure the file
     * name does not contain a view suffix. */
    const char *path_view = has_views ? "" : this->context().get_view_name().data();

    char image_path[FILE_MAX];
    Vector<path_templates::Error> path_errors = this->get_image_path(
        format, layer_name, path_view, image_path);
    if (!path_errors.is_empty()) {
      return;
    }

    const int2 size = result.domain().size;
    FileOutput &file_output = this->context().render_context()->get_file_output(
        image_path, format, size, true);

    /* The EXR stores all views in the same file, so we add the actual render view. Otherwise, we
     * add a default unnamed view. */
    const char *view_name = has_views ? this->context().get_view_name().data() : "";
    file_output.add_view(view_name);
    this->add_pass_for_result(file_output, result, "", view_name);

    this->add_meta_data_for_result(file_output, result, layer_name);
  }

  /* -----------------------
   * Multi-Layer EXR Images.
   */

  void execute_multi_layer()
  {
    /* We only write images, not single values. */
    const int2 size = this->compute_domain().size;
    if (size == int2(1)) {
      return;
    }

    const ImageFormatData format = node_storage(this->bnode()).format;
    const bool store_views_in_single_file = this->is_multi_view_exr();
    const char *view = this->context().get_view_name().data();

    /* If we are saving all views in a single multi-layer file, we supply an empty view to make
     * sure the file name does not contain a view suffix. */
    char image_path[FILE_MAX];
    const char *write_view = store_views_in_single_file ? "" : view;
    Vector<path_templates::Error> path_errors = this->get_image_path(
        format, "", write_view, image_path);
    if (!path_errors.is_empty()) {
      return;
    }

    FileOutput &file_output = this->context().render_context()->get_file_output(
        image_path, format, size, true);

    /* If we are saving views in separate files, we needn't store the view in the channel names, so
     * we add an unnamed view. */
    const char *pass_view = store_views_in_single_file ? view : "";
    file_output.add_view(pass_view);

    const NodeCompositorFileOutput &storage = node_storage(bnode());
    for (const int i : IndexRange(storage.items_count)) {
      const NodeCompositorFileOutputItem &item = storage.items[i];
      const std::string identifier = FileOutputItemsAccessor::socket_identifier_for_item(item);
      const Result &input_result = this->get_input(identifier);
      this->add_pass_for_result(file_output, input_result, item.name, pass_view);

      this->add_meta_data_for_result(file_output, input_result, item.name);
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
      if (this->context().use_gpu()) {
        GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
        buffer = static_cast<float *>(GPU_texture_read(result, GPU_DATA_FLOAT, 0));
      }
      else {
        /* Copy the result into a new buffer. */
        buffer = static_cast<float *>(MEM_dupallocN(result.cpu_data().data()));
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
      case ResultType::Float3:
        /* Float3 results might be stored in 4-component textures due to hardware limitations, so
         * we need to convert the buffer to a 3-component buffer on the host. */
        if (this->context().use_gpu() && GPU_texture_component_len(GPU_texture_format(result))) {
          file_output.add_pass(pass_name, view_name, "XYZ", float4_to_float3_image(size, buffer));
        }
        else {
          file_output.add_pass(pass_name, view_name, "XYZ", buffer);
        }
        break;
      case ResultType::Float4:
        file_output.add_pass(pass_name, view_name, "XYZW", buffer);
        break;
      case ResultType::Float:
        file_output.add_pass(pass_name, view_name, "V", buffer);
        break;
      case ResultType::Float2:
        file_output.add_pass(pass_name, view_name, "XY", buffer);
        break;
      case ResultType::Int2:
      case ResultType::Int:
      case ResultType::Bool:
      case ResultType::Menu:
      case ResultType::String:
        /* Not supported. */
        BLI_assert_unreachable();
        break;
    }
  }

  /* Allocates and fills an image buffer of the specified size with the value of the given single
   * value result. */
  float *inflate_result(const Result &result, const int2 size)
  {
    BLI_assert(result.is_single_value());

    const int64_t length = int64_t(size.x) * size.y;
    const int64_t buffer_size = length * result.channels_count();
    float *buffer = MEM_malloc_arrayN<float>(buffer_size, "File Output Inflated Buffer.");

    switch (result.type()) {
      case ResultType::Float:
      case ResultType::Float2:
      case ResultType::Float3:
      case ResultType::Float4:
      case ResultType::Color: {
        const GPointer single_value = result.single_value();
        single_value.type()->fill_assign_n(single_value.get(), buffer, length);
        return buffer;
      }
      case ResultType::Int:
      case ResultType::Int2:
      case ResultType::Bool:
      case ResultType::Menu:
      case ResultType::String:
        /* Not supported. */
        BLI_assert_unreachable();
        return nullptr;
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
    if (this->context().use_gpu()) {
      GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
      buffer = static_cast<float *>(GPU_texture_read(result, GPU_DATA_FLOAT, 0));
    }
    else {
      /* Copy the result into a new buffer. */
      buffer = static_cast<float *>(MEM_dupallocN(result.cpu_data().data()));
    }

    const int2 size = result.domain().size;
    switch (result.type()) {
      case ResultType::Color:
        file_output.add_view(view_name, 4, buffer);
        break;
      case ResultType::Float4:
        file_output.add_view(view_name, 4, buffer);
        break;
      case ResultType::Float3:
        /* Float3 results might be stored in 4-component textures due to hardware limitations, so
         * we need to convert the buffer to a 3-component buffer on the host. */
        if (this->context().use_gpu() && GPU_texture_component_len(GPU_texture_format(result))) {
          file_output.add_view(view_name, 3, float4_to_float3_image(size, buffer));
        }
        else {
          file_output.add_view(view_name, 3, buffer);
        }
        break;
      case ResultType::Float:
        file_output.add_view(view_name, 1, buffer);
        break;
      case ResultType::Float2:
      case ResultType::Int2:
      case ResultType::Int:
      case ResultType::Bool:
      case ResultType::Menu:
      case ResultType::String:
        /* Not supported. */
        BLI_assert_unreachable();
        break;
    }
  }

  /* Given a float4 image, return a newly allocated float3 image that ignores the last channel. The
   * input image is freed. */
  float *float4_to_float3_image(int2 size, float *float4_image)
  {
    float *float3_image = MEM_malloc_arrayN<float>(3 * size_t(size.x) * size_t(size.y),
                                                   "File Output Vector Buffer.");

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

  Vector<path_templates::Error> get_image_path(const ImageFormatData &format,
                                               const char *file_name_suffix,
                                               const char *view,
                                               char *r_image_path)
  {
    const Vector<path_templates::Error> path_errors = compute_image_path(
        this->get_directory(),
        this->get_file_name(),
        file_name_suffix,
        view,
        this->context().get_frame_number(),
        format,
        this->context().get_scene(),
        this->bnode(),
        this->is_animation_render(),
        r_image_path);

    if (!path_errors.is_empty()) {
      BKE_report(
          nullptr, RPT_ERROR, "Invalid path template in File Output node. Skipping writing file.");
    }

    return path_errors;
  }

  bool is_multi_layer()
  {
    return node_storage(this->bnode()).format.imtype == R_IMF_IMTYPE_MULTILAYER;
  }

  StringRefNull get_file_name()
  {
    const char *file_name = node_storage(this->bnode()).file_name;
    return file_name ? file_name : "";
  }

  StringRefNull get_directory()
  {
    return node_storage(this->bnode()).directory;
  }

  /* If true, save views in a multi-view EXR file, otherwise, save each view in its own file. */
  bool is_multi_view_exr()
  {
    if (!this->is_multi_view_scene()) {
      return false;
    }

    return node_storage(this->bnode()).format.views_format == R_IMF_VIEWS_MULTIVIEW;
  }

  bool is_multi_view_scene()
  {
    return this->context().get_render_data().scemode & R_MULTIVIEW;
  }

  Domain compute_domain() override
  {
    Domain domain = NodeOperation::compute_domain();
    if (!this->is_multi_layer()) {
      return domain;
    }

    /* Reset the location of the domain in multi-layer case such that translations take effect,
     * this will result in clipping but is more expected for the user. */
    domain.transformation.location() = float2(0.0f);
    return domain;
  }

  bool is_animation_render()
  {
    if (!this->context().render_context()) {
      return false;
    }
    return this->context().render_context()->is_animation_render;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new FileOutputOperation(context, node);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeOutputFile", CMP_NODE_OUTPUT_FILE);
  ntype.ui_name = "File Output";
  ntype.ui_description = "Write image file to disk";
  ntype.enum_name_legacy = "OUTPUT_FILE";
  ntype.nclass = NODE_CLASS_OUTPUT;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.insert_link = node_insert_link;
  ntype.register_operators = node_operators;
  ntype.initfunc_api = node_init;
  blender::bke::node_type_storage(
      ntype, "NodeCompositorFileOutput", node_free_storage, node_copy_storage);
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  ntype.get_extra_info = node_extra_info;
  ntype.get_compositor_operation = get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_file_output_cc

namespace blender::nodes {

StructRNA *FileOutputItemsAccessor::item_srna = &RNA_NodeCompositorFileOutputItem;

void FileOutputItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
  BKE_image_format_blend_write(writer, const_cast<ImageFormatData *>(&item.format));
}

void FileOutputItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
  BKE_image_format_blend_read_data(reader, &item.format);
}

std::string FileOutputItemsAccessor::validate_name(const StringRef name)
{
  char file_name[FILE_MAX] = "";
  STRNCPY(file_name, name.data());
  BLI_path_make_safe_filename(file_name);
  return file_name;
}

}  // namespace blender::nodes
