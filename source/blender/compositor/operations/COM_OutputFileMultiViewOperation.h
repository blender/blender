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
 * Copyright 2015, Blender Foundation.
 */

#pragma once

#include "COM_NodeOperation.h"
#include "COM_OutputFileOperation.h"

#include "BLI_path_util.h"
#include "BLI_rect.h"

#include "DNA_color_types.h"

#include "intern/openexr/openexr_multi.h"

namespace blender::compositor {

class OutputOpenExrSingleLayerMultiViewOperation : public OutputSingleLayerOperation {
 private:
 public:
  OutputOpenExrSingleLayerMultiViewOperation(const RenderData *rd,
                                             const bNodeTree *tree,
                                             DataType datatype,
                                             ImageFormatData *format,
                                             const char *path,
                                             const ColorManagedViewSettings *view_settings,
                                             const ColorManagedDisplaySettings *display_settings,
                                             const char *view_name,
                                             bool save_as_render);

  void *get_handle(const char *filename);
  void deinit_execution() override;
};

/* Writes inputs into OpenEXR multilayer channels. */
class OutputOpenExrMultiLayerMultiViewOperation : public OutputOpenExrMultiLayerOperation {
 private:
 public:
  OutputOpenExrMultiLayerMultiViewOperation(const Scene *scene,
                                            const RenderData *rd,
                                            const bNodeTree *tree,
                                            const char *path,
                                            char exr_codec,
                                            bool exr_half_float,
                                            const char *view_name);

  void *get_handle(const char *filename);
  void deinit_execution() override;
};

class OutputStereoOperation : public OutputSingleLayerOperation {
 private:
  char name_[FILE_MAX];
  size_t channels_;

 public:
  OutputStereoOperation(const RenderData *rd,
                        const bNodeTree *tree,
                        DataType datatype,
                        struct ImageFormatData *format,
                        const char *path,
                        const char *name,
                        const ColorManagedViewSettings *view_settings,
                        const ColorManagedDisplaySettings *display_settings,
                        const char *view_name,
                        bool save_as_render);
  void *get_handle(const char *filename);
  void deinit_execution() override;
};

}  // namespace blender::compositor
