/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "COM_CompositorContext.h"
#include "COM_MultiThreadedOperation.h"

struct StampData;

namespace blender::realtime_compositor {
class FileOutput;
}

namespace blender::compositor {

struct FileOutputInput {
  FileOutputInput(NodeImageMultiFileSocket *data, DataType data_type);

  NodeImageMultiFileSocket *data;
  DataType data_type;

  float *output_buffer = nullptr;
  SocketReader *image_input = nullptr;
};

class FileOutputOperation : public MultiThreadedOperation {
 private:
  const CompositorContext *context_;
  const NodeImageMultiFile *node_data_;
  Vector<FileOutputInput> file_output_inputs_;

 public:
  FileOutputOperation(const CompositorContext *context,
                      const NodeImageMultiFile *node_data,
                      Vector<FileOutputInput> inputs);

  bool is_output_operation(bool /*rendering*/) const override
  {
    return true;
  }
  void init_execution() override;
  void deinit_execution() override;
  eCompositorPriority get_render_priority() const override
  {
    return eCompositorPriority::Low;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  void execute_single_layer();
  void execute_single_layer_multi_view_exr(const FileOutputInput &input,
                                           const ImageFormatData &format,
                                           const char *base_path);
  void execute_multi_layer();

  /* Add a pass of the given name, view, and input buffer. The pass channel identifiers follows the
   * EXR conventions. */
  void add_pass_for_input(realtime_compositor::FileOutput &file_output,
                          const FileOutputInput &input,
                          const char *pass_name,
                          const char *view_name);

  /* Add a view of the given name and input buffer. */
  void add_view_for_input(realtime_compositor::FileOutput &file_output,
                          const FileOutputInput &input,
                          const char *view_name);

  /* Get the base path of the image to be saved, based on the base path of the node. The base name
   * is an optional initial name of the image, which will later be concatenated with other
   * information like the frame number, view, and extension. If the base name is empty, then the
   * base path represents a directory, so a trailing slash is ensured. */
  void get_single_layer_image_base_path(const char *base_name, char *base_path);

  /* Get the path of the image to be saved based on the given format. */
  void get_single_layer_image_path(const char *base_path,
                                   const ImageFormatData &format,
                                   char *image_path);

  /* Get the path of the EXR image to be saved. If the given view is not empty, its corresponding
   * file suffix will be appended to the name. */
  void get_multi_layer_exr_image_path(const char *base_path, const char *view, char *image_path);

  bool is_multi_layer();

  const char *get_base_path();

  /* Add the file format extensions to the rendered file name. */
  bool use_file_extension();

  /* If true, save views in a multi-view EXR file, otherwise, save each view in its own file. */
  bool is_multi_view_exr();

  bool is_multi_view_scene();
};

}  // namespace blender::compositor
