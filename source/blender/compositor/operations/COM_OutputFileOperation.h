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
 * Copyright 2011, Blender Foundation.
 */

#pragma once

#include "COM_MultiThreadedOperation.h"

#include "BLI_path_util.h"
#include "BLI_rect.h"

#include "DNA_color_types.h"

#include "intern/openexr/openexr_multi.h"

namespace blender::compositor {

/* Writes the image to a single-layer file. */
class OutputSingleLayerOperation : public MultiThreadedOperation {
 protected:
  const RenderData *rd_;
  const bNodeTree *tree_;

  ImageFormatData *format_;
  char path_[FILE_MAX];

  float *output_buffer_;
  DataType datatype_;
  SocketReader *image_input_;

  const ColorManagedViewSettings *view_settings_;
  const ColorManagedDisplaySettings *display_settings_;

  const char *view_name_;
  bool save_as_render_;

 public:
  OutputSingleLayerOperation(const RenderData *rd,
                             const bNodeTree *tree,
                             DataType datatype,
                             ImageFormatData *format,
                             const char *path,
                             const ColorManagedViewSettings *view_settings,
                             const ColorManagedDisplaySettings *display_settings,
                             const char *view_name,
                             bool save_as_render);

  void execute_region(rcti *rect, unsigned int tile_number) override;
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
};

/* extra info for OpenEXR layers */
struct OutputOpenExrLayer {
  OutputOpenExrLayer(const char *name, DataType datatype, bool use_layer);

  char name[EXR_TOT_MAXNAME - 2];
  DataType datatype;
  bool use_layer;

  /* internals */
  float *output_buffer;
  SocketReader *image_input;
};

/* Writes inputs into OpenEXR multilayer channels. */
class OutputOpenExrMultiLayerOperation : public MultiThreadedOperation {
 protected:
  const Scene *scene_;
  const RenderData *rd_;
  const bNodeTree *tree_;

  char path_[FILE_MAX];
  char exr_codec_;
  bool exr_half_float_;
  Vector<OutputOpenExrLayer> layers_;
  const char *view_name_;

  StampData *create_stamp_data() const;

 public:
  OutputOpenExrMultiLayerOperation(const Scene *scene,
                                   const RenderData *rd,
                                   const bNodeTree *tree,
                                   const char *path,
                                   char exr_codec,
                                   bool exr_half_float,
                                   const char *view_name);

  void add_layer(const char *name, DataType datatype, bool use_layer);

  void execute_region(rcti *rect, unsigned int tile_number) override;
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
};

void add_exr_channels(void *exrhandle,
                      const char *layer_name,
                      const DataType datatype,
                      const char *view_name,
                      size_t width,
                      bool use_half_float,
                      float *buf);
void free_exr_channels(void *exrhandle,
                       const RenderData *rd,
                       const char *layer_name,
                       const DataType datatype);
int get_datatype_size(DataType datatype);

}  // namespace blender::compositor
