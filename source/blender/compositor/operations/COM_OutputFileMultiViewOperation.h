/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015 Blender Foundation. */

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
                                             ImageFormatData *format,
                                             const char *path,
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
  OutputStereoOperation(const Scene *scene,
                        const RenderData *rd,
                        const bNodeTree *tree,
                        DataType datatype,
                        struct ImageFormatData *format,
                        const char *path,
                        const char *name,
                        const char *view_name,
                        bool save_as_render);
  void *get_handle(const char *filename);
  void deinit_execution() override;
};

}  // namespace blender::compositor
