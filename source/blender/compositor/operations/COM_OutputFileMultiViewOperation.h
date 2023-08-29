/* SPDX-FileCopyrightText: 2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"
#include "COM_OutputFileOperation.h"

#include "BLI_path_util.h"
#include "BLI_rect.h"

#include "DNA_color_types.h"

#include "IMB_openexr.h"

namespace blender::compositor {

class OutputOpenExrSingleLayerMultiViewOperation : public OutputSingleLayerOperation {
 private:
 public:
  OutputOpenExrSingleLayerMultiViewOperation(const Scene *scene,
                                             const RenderData *rd,
                                             const bNodeTree *tree,
                                             DataType datatype,
                                             const ImageFormatData *format,
                                             const char *path,
                                             const char *view_name,
                                             bool save_as_render);

  void *get_handle(const char *filepath);
  void deinit_execution() override;
};

/** Writes inputs into OpenEXR multi-layer channels. */
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

  void *get_handle(const char *filepath);
  void deinit_execution() override;
};

class OutputStereoOperation : public OutputSingleLayerOperation {
 private:
  /* NOTE: Using FILE_MAX here is misleading, this is not a file path. */
  char pass_name_[FILE_MAX];
  size_t channels_;

 public:
  OutputStereoOperation(const Scene *scene,
                        const RenderData *rd,
                        const bNodeTree *tree,
                        DataType datatype,
                        const struct ImageFormatData *format,
                        const char *path,
                        const char *pass_name,
                        const char *view_name,
                        bool save_as_render);
  void *get_handle(const char *filepath);
  void deinit_execution() override;
};

}  // namespace blender::compositor
