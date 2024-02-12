/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "RE_pipeline.h"

#include "COM_FileOutputOperation.h"
#include "COM_render_context.hh"

namespace blender::compositor {

FileOutputInput::FileOutputInput(NodeImageMultiFileSocket *data, DataType data_type)
    : data(data), data_type(data_type)
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

static void write_buffer_rect(
    rcti *rect, SocketReader *reader, float *buffer, uint width, DataType datatype)
{

  if (!buffer) {
    return;
  }
  int x1 = rect->xmin;
  int y1 = rect->ymin;
  int x2 = rect->xmax;
  int y2 = rect->ymax;

  int size = get_channels_count(datatype);
  int offset = (y1 * width + x1) * size;
  for (int y = y1; y < y2; y++) {
    for (int x = x1; x < x2; x++) {
      float color[4];
      reader->read_sampled(color, x, y, PixelSampler::Nearest);

      for (int i = 0; i < size; i++) {
        buffer[offset + i] = color[i];
      }
      offset += size;
    }
    offset += (width - (x2 - x1)) * size;
  }
}

FileOutputOperation::FileOutputOperation(const CompositorContext *context,
                                         const NodeImageMultiFile *node_data,
                                         Vector<FileOutputInput> inputs)
    : context_(context), node_data_(node_data), file_output_inputs_(inputs)
{
  for (const FileOutputInput &input : inputs) {
    add_input_socket(input.data_type);
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
    input.output_buffer = initialize_buffer(get_width(), get_height(), input.data_type);
  }
}

void FileOutputOperation::execute_region(rcti *rect, uint /*tile_number*/)
{
  for (int i = 0; i < file_output_inputs_.size(); i++) {
    const FileOutputInput &input = file_output_inputs_[i];
    if (!input.image_input || !input.output_buffer) {
      continue;
    }
    write_buffer_rect(rect, input.image_input, input.output_buffer, get_width(), input.data_type);
  }
}

void FileOutputOperation::update_memory_buffer_partial(MemoryBuffer * /*output*/,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  for (int i = 0; i < file_output_inputs_.size(); i++) {
    const FileOutputInput &input = file_output_inputs_[i];
    if (!input.output_buffer) {
      continue;
    }
    int channels_count = get_channels_count(input.data_type);
    MemoryBuffer output_buf(input.output_buffer, channels_count, get_width(), get_height());
    output_buf.copy_from(inputs[i], area, 0, inputs[i]->get_num_channels(), 0);
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
  const int2 size = int2(get_width(), get_height());
  for (const FileOutputInput &input : file_output_inputs_) {
    /* Unlinked input. */
    if (!input.image_input) {
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
    const bool is_exr = format.imtype == R_IMF_IMTYPE_OPENEXR;
    const int views_count = BKE_scene_multiview_num_views_get(context_->get_render_data());
    if (is_exr && !(format.views_format == R_IMF_VIEWS_STEREO_3D && views_count == 2)) {
      execute_single_layer_multi_view_exr(input, format, base_path);
      continue;
    }

    char image_path[FILE_MAX];
    get_single_layer_image_path(base_path, format, image_path);

    realtime_compositor::FileOutput &file_output = context_->get_render_context()->get_file_output(
        image_path, format, size, input.data->save_as_render);

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

  const int2 size = int2(get_width(), get_height());
  realtime_compositor::FileOutput &file_output = context_->get_render_context()->get_file_output(
      image_path, format, size, false);

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
      image_path, format, size, false);

  /* If we are saving views in separate files, we needn't store the view in the channel names, so
   * we add an unnamed view. */
  const char *pass_view = store_views_in_single_file ? view : "";
  file_output.add_view(pass_view);

  for (const FileOutputInput &input : file_output_inputs_) {
    /* Unlinked input. */
    if (!input.image_input) {
      continue;
    }

    const char *pass_name = input.data->layer;
    add_pass_for_input(file_output, input, pass_name, pass_view);

    add_meta_data_for_input(file_output, input);
  }
}

/* Add a pass of the given name, view, and input buffer. The pass channel identifiers follows the
 * EXR conventions. */
void FileOutputOperation::add_pass_for_input(realtime_compositor::FileOutput &file_output,
                                             const FileOutputInput &input,
                                             const char *pass_name,
                                             const char *view_name)
{
  switch (input.data_type) {
    case DataType::Color:
      file_output.add_pass(pass_name, view_name, "RGBA", input.output_buffer);
      break;
    case DataType::Vector:
      file_output.add_pass(pass_name, view_name, "XYZ", input.output_buffer);
      break;
    case DataType::Value:
      file_output.add_pass(pass_name, view_name, "V", input.output_buffer);
      break;
  }
}

/* Add a view of the given name and input buffer. */
void FileOutputOperation::add_view_for_input(realtime_compositor::FileOutput &file_output,
                                             const FileOutputInput &input,
                                             const char *view_name)
{
  switch (input.data_type) {
    case DataType::Color:
      file_output.add_view(view_name, 4, input.output_buffer);
      break;
    case DataType::Vector:
      file_output.add_view(view_name, 3, input.output_buffer);
      break;
    case DataType::Value:
      file_output.add_view(view_name, 1, input.output_buffer);
      break;
  }
}

/* Get the base path of the image to be saved, based on the base path of the node. The base name
 * is an optional initial name of the image, which will later be concatenated with other
 * information like the frame number, view, and extension. If the base name is empty, then the
 * base path represents a directory, so a trailing slash is ensured. */
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

/* Get the path of the image to be saved based on the given format. */
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

/* Get the path of the EXR image to be saved. If the given view is not empty, its corresponding
 * file suffix will be appended to the name. */
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

/* Add the file format extensions to the rendered file name. */
bool FileOutputOperation::use_file_extension()
{
  return context_->get_render_data()->scemode & R_EXTENSION;
}

/* If true, save views in a multi-view EXR file, otherwise, save each view in its own file. */
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
