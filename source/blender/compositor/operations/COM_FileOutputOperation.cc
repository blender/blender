/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>

#include "BLI_assert.h"
#include "BLI_fileops.h"
#include "BLI_index_range.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "RE_pipeline.h"

#include "COM_FileOutputOperation.h"
#include "COM_render_context.hh"

namespace blender::compositor {

FileOutputInput::FileOutputInput(NodeImageMultiFileSocket *data,
                                 DataType data_type,
                                 DataType original_data_type)
    : data(data), data_type(data_type), original_data_type(original_data_type)
{
}

static int get_channels_count(DataType datatype)
{
  switch (datatype) {
    case DataType::Value:
      return 1;
    case DataType::Vector:
      return 3;
    case DataType::Color:
      return 4;
    default:
      return 0;
  }
}

static float *initialize_buffer(uint width, uint height, DataType datatype)
{
  const int size = get_channels_count(datatype);
  return static_cast<float *>(
      MEM_malloc_arrayN(size_t(width) * height, sizeof(float) * size, "File Output Buffer."));
}

FileOutputOperation::FileOutputOperation(const CompositorContext *context,
                                         const NodeImageMultiFile *node_data,
                                         Vector<FileOutputInput> inputs)
    : context_(context), node_data_(node_data), file_output_inputs_(inputs)
{
  /* Inputs for multi-layer files need to be the same size, while they can be different for
   * individual file outputs. */
  const ResizeMode resize_mode = this->is_multi_layer() ? ResizeMode::Center : ResizeMode::None;

  for (const FileOutputInput &input : inputs) {
    add_input_socket(input.data_type, resize_mode);
  }
  this->set_canvas_input_index(RESOLUTION_INPUT_ANY);
}

void FileOutputOperation::init_execution()
{
  for (int i = 0; i < file_output_inputs_.size(); i++) {
    FileOutputInput &input = file_output_inputs_[i];
    input.image_input = get_input_socket_reader(i);
    if (!input.image_input) {
      continue;
    }
    input.output_buffer = initialize_buffer(
        input.image_input->get_width(), input.image_input->get_height(), input.data_type);
  }
}

void FileOutputOperation::update_memory_buffer(MemoryBuffer * /*output*/,
                                               const rcti & /*area*/,
                                               Span<MemoryBuffer *> inputs)
{
  for (int i = 0; i < file_output_inputs_.size(); i++) {
    const FileOutputInput &input = file_output_inputs_[i];
    if (!input.output_buffer) {
      continue;
    }

    int channels_count = get_channels_count(input.data_type);
    MemoryBuffer output_buf(
        input.output_buffer, channels_count, inputs[i]->get_width(), inputs[i]->get_height());
    output_buf.copy_from(inputs[i], inputs[i]->get_rect(), 0, inputs[i]->get_num_channels(), 0);
  }
}

static void add_meta_data_for_input(realtime_compositor::FileOutput &file_output,
                                    const FileOutputInput &input)
{
  std::unique_ptr<MetaData> meta_data = input.image_input->get_meta_data();
  if (!meta_data) {
    return;
  }

  blender::StringRef layer_name = blender::bke::cryptomatte::BKE_cryptomatte_extract_layer_name(
      blender::StringRef(input.data->layer,
                         BLI_strnlen(input.data->layer, sizeof(input.data->layer))));
  meta_data->replace_hash_neutral_cryptomatte_keys(layer_name);
  meta_data->for_each_entry([&](const std::string &key, const std::string &value) {
    file_output.add_meta_data(key, value);
  });
}

void FileOutputOperation::deinit_execution()
{
  /* It is possible that none of the inputs would have an image connected, which will materialize
   * as a size of zero, so check this here and return early doing nothing. Just make sure to free
   * the allocated buffers. */
  const int2 size = int2(get_width(), get_height());
  if (size == int2(0)) {
    for (const FileOutputInput &input : file_output_inputs_) {
      /* Ownership of outputs buffers are transferred to file outputs, so if we are not writing a
       * file output, we need to free the output buffer here. */
      if (input.output_buffer) {
        MEM_freeN(input.output_buffer);
      }
    }
    return;
  }

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

void FileOutputOperation::execute_single_layer()
{
  for (const FileOutputInput &input : file_output_inputs_) {
    /* We only write images, not single values. */
    if (!input.image_input || input.image_input->get_flags().is_constant_operation) {
      /* Ownership of outputs buffers are transferred to file outputs, so if we are not writing a
       * file output, we need to free the output buffer here. */
      if (input.output_buffer) {
        MEM_freeN(input.output_buffer);
      }
      continue;
    }

    char base_path[FILE_MAX];
    get_single_layer_image_base_path(input.data->path, base_path);

    /* The image saving code expects EXR images to have a different structure than standard
     * images. In particular, in EXR images, the buffers need to be stored in passes that are, in
     * turn, stored in a render layer. On the other hand, in non-EXR images, the buffers need to
     * be stored in views. An exception to this is stereo images, which needs to have the same
     * structure as non-EXR images. */
    const auto &format = input.data->use_node_format ? node_data_->format : input.data->format;
    const bool save_as_render = input.data->use_node_format ? node_data_->save_as_render :
                                                              input.data->save_as_render;
    const bool is_exr = format.imtype == R_IMF_IMTYPE_OPENEXR;
    const int views_count = BKE_scene_multiview_num_views_get(context_->get_render_data());
    if (is_exr && !(format.views_format == R_IMF_VIEWS_STEREO_3D && views_count == 2)) {
      execute_single_layer_multi_view_exr(input, format, base_path);
      continue;
    }

    char image_path[FILE_MAX];
    get_single_layer_image_path(base_path, format, image_path);

    const int2 size = int2(input.image_input->get_width(), input.image_input->get_height());
    realtime_compositor::FileOutput &file_output = context_->get_render_context()->get_file_output(
        image_path, format, size, save_as_render);

    add_view_for_input(file_output, input, context_->get_view_name());

    add_meta_data_for_input(file_output, input);
  }
}

/* -----------------------------------
 * Single Layer Multi-View EXR Images.
 */

void FileOutputOperation::execute_single_layer_multi_view_exr(const FileOutputInput &input,
                                                              const ImageFormatData &format,
                                                              const char *base_path)
{
  const bool has_views = format.views_format != R_IMF_VIEWS_INDIVIDUAL;

  /* The EXR stores all views in the same file, so we supply an empty view to make sure the file
   * name does not contain a view suffix. */
  char image_path[FILE_MAX];
  const char *path_view = has_views ? "" : context_->get_view_name();
  get_multi_layer_exr_image_path(base_path, path_view, image_path);

  const int2 size = int2(input.image_input->get_width(), input.image_input->get_height());
  realtime_compositor::FileOutput &file_output = context_->get_render_context()->get_file_output(
      image_path, format, size, true);

  /* The EXR stores all views in the same file, so we add the actual render view. Otherwise, we
   * add a default unnamed view. */
  const char *view_name = has_views ? context_->get_view_name() : "";
  file_output.add_view(view_name);
  add_pass_for_input(file_output, input, "", view_name);

  add_meta_data_for_input(file_output, input);
}

/* -----------------------
 * Multi-Layer EXR Images.
 */

void FileOutputOperation::execute_multi_layer()
{
  const bool store_views_in_single_file = is_multi_view_exr();
  const char *view = context_->get_view_name();

  /* If we are saving all views in a single multi-layer file, we supply an empty view to make
   * sure the file name does not contain a view suffix. */
  char image_path[FILE_MAX];
  const char *write_view = store_views_in_single_file ? "" : view;
  get_multi_layer_exr_image_path(get_base_path(), write_view, image_path);

  const int2 size = int2(get_width(), get_height());
  const ImageFormatData format = node_data_->format;
  realtime_compositor::FileOutput &file_output = context_->get_render_context()->get_file_output(
      image_path, format, size, true);

  /* If we are saving views in separate files, we needn't store the view in the channel names, so
   * we add an unnamed view. */
  const char *pass_view = store_views_in_single_file ? view : "";
  file_output.add_view(pass_view);

  for (const FileOutputInput &input : file_output_inputs_) {
    if (!input.image_input) {
      /* Ownership of outputs buffers are transferred to file outputs, so if we are not writing a
       * file output, we need to free the output buffer here. */
      if (input.output_buffer) {
        MEM_freeN(input.output_buffer);
      }
      continue;
    }

    const char *pass_name = input.data->layer;
    add_pass_for_input(file_output, input, pass_name, pass_view);

    add_meta_data_for_input(file_output, input);
  }
}

/* Given a float4 image, return a newly allocated float3 image that ignores the last channel. The
 * input image is freed. */
static float *float4_to_float3_image(int2 size, float *float4_image)
{
  float *float3_image = static_cast<float *>(
      MEM_malloc_arrayN(size_t(size.x) * size.y, sizeof(float[3]), "File Output Vector Buffer."));

  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        for (int i = 0; i < 3; i++) {
          const int pixel_index = y * size.x + x;
          float3_image[pixel_index * 3 + i] = float4_image[pixel_index * 4 + i];
        }
      }
    }
  });

  MEM_freeN(float4_image);
  return float3_image;
}

/* Allocates and fills an image buffer of the specified size with the value of the given constant
 * input. */
static float *inflate_input(const FileOutputInput &input, const int2 size)
{
  BLI_assert(input.image_input->get_flags().is_constant_operation);

  switch (input.data_type) {
    case DataType::Value: {
      float *buffer = static_cast<float *>(MEM_malloc_arrayN(
          size_t(size.x) * size.y, sizeof(float), "File Output Inflated Buffer."));

      const float value = input.image_input->get_constant_value_default(0.0f);
      threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
        for (const int64_t y : sub_y_range) {
          for (const int64_t x : IndexRange(size.x)) {
            buffer[y * size.x + x] = value;
          }
        }
      });
      return buffer;
    }
    case DataType::Color: {
      float *buffer = static_cast<float *>(MEM_malloc_arrayN(
          size_t(size.x) * size.y, sizeof(float[4]), "File Output Inflated Buffer."));

      const float *value = input.image_input->get_constant_elem_default(nullptr);
      threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
        for (const int64_t y : sub_y_range) {
          for (const int64_t x : IndexRange(size.x)) {
            copy_v4_v4(buffer + ((y * size.x + x) * 4), value);
          }
        }
      });
      return buffer;
    }
    default:
      /* Vector types are not possible for File output, see get_input_data_type in
       * COM_FileOutputNode.cc for more information. Other types are internal and needn't be
       * handled by operations. */
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

void FileOutputOperation::add_pass_for_input(realtime_compositor::FileOutput &file_output,
                                             const FileOutputInput &input,
                                             const char *pass_name,
                                             const char *view_name)
{
  /* For constant operations, we fill a buffer that covers the canvas of the operation with the
   * value of the operation. */
  const int2 size = input.image_input->get_flags().is_constant_operation ?
                        int2(this->get_width(), this->get_height()) :
                        int2(input.image_input->get_width(), input.image_input->get_height());

  /* The image buffer in the file output will take ownership of this buffer and freeing it will be
   * its responsibility. So if we don't use the output buffer, we need to free it here. */
  float *buffer = nullptr;
  if (input.image_input->get_flags().is_constant_operation) {
    buffer = inflate_input(input, size);
    MEM_freeN(input.output_buffer);
  }
  else {
    buffer = input.output_buffer;
  }

  switch (input.original_data_type) {
    case DataType::Color:
      /* Use lowercase rgba for Cryptomatte layers because the EXR internal compression rules
       * specify that all uppercase RGBA channels will be compressed, and Cryptomatte should not be
       * compressed. */
      if (input.image_input->get_meta_data() &&
          input.image_input->get_meta_data()->is_cryptomatte_layer())
      {
        file_output.add_pass(pass_name, view_name, "rgba", buffer);
      }
      else {
        file_output.add_pass(pass_name, view_name, "RGBA", buffer);
      }
      break;
    case DataType::Vector:
      if (input.image_input->get_meta_data() && input.image_input->get_meta_data()->is_4d_vector) {
        file_output.add_pass(pass_name, view_name, "XYZW", buffer);
      }
      else {
        file_output.add_pass(pass_name, view_name, "XYZ", float4_to_float3_image(size, buffer));
      }
      break;
    case DataType::Value:
      file_output.add_pass(pass_name, view_name, "V", buffer);
      break;
    case DataType::Float2:
      /* An internal type that needn't be handled. */
      BLI_assert_unreachable();
      break;
  }
}

void FileOutputOperation::add_view_for_input(realtime_compositor::FileOutput &file_output,
                                             const FileOutputInput &input,
                                             const char *view_name)
{
  const int2 size = int2(input.image_input->get_width(), input.image_input->get_height());
  switch (input.original_data_type) {
    case DataType::Color:
      file_output.add_view(view_name, 4, input.output_buffer);
      break;
    case DataType::Vector:
      file_output.add_view(view_name, 3, float4_to_float3_image(size, input.output_buffer));
      break;
    case DataType::Value:
      file_output.add_view(view_name, 1, input.output_buffer);
      break;
    case DataType::Float2:
      /* An internal type that needn't be handled. */
      BLI_assert_unreachable();
      break;
  }
}

void FileOutputOperation::get_single_layer_image_base_path(const char *base_name, char *base_path)
{
  if (base_name[0]) {
    BLI_path_join(base_path, FILE_MAX, get_base_path(), base_name);
  }
  else {
    BLI_strncpy(base_path, get_base_path(), FILE_MAX);
    BLI_path_slash_ensure(base_path, FILE_MAX);
  }
}

void FileOutputOperation::get_single_layer_image_path(const char *base_path,
                                                      const ImageFormatData &format,
                                                      char *image_path)
{
  BKE_image_path_from_imformat(image_path,
                               base_path,
                               BKE_main_blendfile_path_from_global(),
                               context_->get_framenumber(),
                               &format,
                               use_file_extension(),
                               true,
                               nullptr);
}

void FileOutputOperation::get_multi_layer_exr_image_path(const char *base_path,
                                                         const char *view,
                                                         char *image_path)
{
  const char *suffix = BKE_scene_multiview_view_suffix_get(context_->get_render_data(), view);
  BKE_image_path_from_imtype(image_path,
                             base_path,
                             BKE_main_blendfile_path_from_global(),
                             context_->get_framenumber(),
                             R_IMF_IMTYPE_MULTILAYER,
                             use_file_extension(),
                             true,
                             suffix);
}

bool FileOutputOperation::is_multi_layer()
{
  return node_data_->format.imtype == R_IMF_IMTYPE_MULTILAYER;
}

const char *FileOutputOperation::get_base_path()
{
  return node_data_->base_path;
}

bool FileOutputOperation::use_file_extension()
{
  return context_->get_render_data()->scemode & R_EXTENSION;
}

bool FileOutputOperation::is_multi_view_exr()
{
  if (!is_multi_view_scene()) {
    return false;
  }

  return node_data_->format.views_format == R_IMF_VIEWS_MULTIVIEW;
}

bool FileOutputOperation::is_multi_view_scene()
{
  return context_->get_render_data()->scemode & R_MULTIVIEW;
}

}  // namespace blender::compositor
