/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015 Blender Foundation. */

#include "COM_OutputFileMultiViewOperation.h"

#include "BLI_fileops.h"

#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::compositor {

/************************************ OpenEXR Singlelayer Multiview ******************************/

OutputOpenExrSingleLayerMultiViewOperation::OutputOpenExrSingleLayerMultiViewOperation(
    const Scene *scene,
    const RenderData *rd,
    const bNodeTree *tree,
    DataType datatype,
    const ImageFormatData *format,
    const char *path,
    const char *view_name,
    const bool save_as_render)
    : OutputSingleLayerOperation(
          scene, rd, tree, datatype, format, path, view_name, save_as_render)
{
}

void *OutputOpenExrSingleLayerMultiViewOperation::get_handle(const char *filepath)
{
  size_t width = this->get_width();
  size_t height = this->get_height();
  SceneRenderView *srv;

  if (width != 0 && height != 0) {
    void *exrhandle;

    exrhandle = IMB_exr_get_handle_name(filepath);

    if (!BKE_scene_multiview_is_render_view_first(rd_, view_name_)) {
      return exrhandle;
    }

    IMB_exr_clear_channels(exrhandle);

    for (srv = (SceneRenderView *)rd_->views.first; srv; srv = srv->next) {
      if (BKE_scene_multiview_is_render_view_active(rd_, srv) == false) {
        continue;
      }

      IMB_exr_add_view(exrhandle, srv->name);
      add_exr_channels(exrhandle, nullptr, datatype_, srv->name, width, false, nullptr);
    }

    BLI_file_ensure_parent_dir_exists(filepath);

    /* prepare the file with all the channels */

    if (!IMB_exr_begin_write(exrhandle, filepath, width, height, format_.exr_codec, nullptr)) {
      printf("Error Writing Singlelayer Multiview Openexr\n");
      IMB_exr_close(exrhandle);
    }
    else {
      IMB_exr_clear_channels(exrhandle);
      return exrhandle;
    }
  }
  return nullptr;
}

void OutputOpenExrSingleLayerMultiViewOperation::deinit_execution()
{
  uint width = this->get_width();
  uint height = this->get_height();

  if (width != 0 && height != 0) {
    void *exrhandle;
    char filepath[FILE_MAX];

    BKE_image_path_from_imtype(filepath,
                               path_,
                               BKE_main_blendfile_path_from_global(),
                               rd_->cfra,
                               R_IMF_IMTYPE_OPENEXR,
                               (rd_->scemode & R_EXTENSION) != 0,
                               true,
                               nullptr);

    exrhandle = this->get_handle(filepath);
    add_exr_channels(exrhandle,
                     nullptr,
                     datatype_,
                     view_name_,
                     width,
                     format_.depth == R_IMF_CHAN_DEPTH_16,
                     output_buffer_);

    /* memory can only be freed after we write all views to the file */
    output_buffer_ = nullptr;
    image_input_ = nullptr;

    /* ready to close the file */
    if (BKE_scene_multiview_is_render_view_last(rd_, view_name_)) {
      IMB_exr_write_channels(exrhandle);

      /* free buffer memory for all the views */
      free_exr_channels(exrhandle, rd_, nullptr, datatype_);

      /* remove exr handle and data */
      IMB_exr_close(exrhandle);
    }
  }
}

/************************************ OpenEXR Multilayer Multiview *******************************/

OutputOpenExrMultiLayerMultiViewOperation::OutputOpenExrMultiLayerMultiViewOperation(
    const Scene *scene,
    const RenderData *rd,
    const bNodeTree *tree,
    const char *path,
    char exr_codec,
    bool exr_half_float,
    const char *view_name)
    : OutputOpenExrMultiLayerOperation(scene, rd, tree, path, exr_codec, exr_half_float, view_name)
{
}

void *OutputOpenExrMultiLayerMultiViewOperation::get_handle(const char *filepath)
{
  uint width = this->get_width();
  uint height = this->get_height();

  if (width != 0 && height != 0) {

    void *exrhandle;
    SceneRenderView *srv;

    /* get a new global handle */
    exrhandle = IMB_exr_get_handle_name(filepath);

    if (!BKE_scene_multiview_is_render_view_first(rd_, view_name_)) {
      return exrhandle;
    }

    IMB_exr_clear_channels(exrhandle);

    /* check renderdata for amount of views */
    for (srv = (SceneRenderView *)rd_->views.first; srv; srv = srv->next) {

      if (BKE_scene_multiview_is_render_view_active(rd_, srv) == false) {
        continue;
      }

      IMB_exr_add_view(exrhandle, srv->name);

      for (uint i = 0; i < layers_.size(); i++) {
        add_exr_channels(exrhandle,
                         layers_[i].name,
                         layers_[i].datatype,
                         srv->name,
                         width,
                         exr_half_float_,
                         nullptr);
      }
    }

    BLI_file_ensure_parent_dir_exists(filepath);

    /* prepare the file with all the channels for the header */
    StampData *stamp_data = create_stamp_data();
    if (!IMB_exr_begin_write(exrhandle, filepath, width, height, exr_codec_, stamp_data)) {
      printf("Error Writing Multilayer Multiview Openexr\n");
      IMB_exr_close(exrhandle);
      BKE_stamp_data_free(stamp_data);
    }
    else {
      IMB_exr_clear_channels(exrhandle);
      BKE_stamp_data_free(stamp_data);
      return exrhandle;
    }
  }
  return nullptr;
}

void OutputOpenExrMultiLayerMultiViewOperation::deinit_execution()
{
  uint width = this->get_width();
  uint height = this->get_height();

  if (width != 0 && height != 0) {
    void *exrhandle;
    char filepath[FILE_MAX];

    BKE_image_path_from_imtype(filepath,
                               path_,
                               BKE_main_blendfile_path_from_global(),
                               rd_->cfra,
                               R_IMF_IMTYPE_MULTILAYER,
                               (rd_->scemode & R_EXTENSION) != 0,
                               true,
                               nullptr);

    exrhandle = this->get_handle(filepath);

    for (uint i = 0; i < layers_.size(); i++) {
      add_exr_channels(exrhandle,
                       layers_[i].name,
                       layers_[i].datatype,
                       view_name_,
                       width,
                       exr_half_float_,
                       layers_[i].output_buffer);
    }

    for (uint i = 0; i < layers_.size(); i++) {
      /* memory can only be freed after we write all views to the file */
      layers_[i].output_buffer = nullptr;
      layers_[i].image_input = nullptr;
    }

    /* ready to close the file */
    if (BKE_scene_multiview_is_render_view_last(rd_, view_name_)) {
      IMB_exr_write_channels(exrhandle);

      /* free buffer memory for all the views */
      for (uint i = 0; i < layers_.size(); i++) {
        free_exr_channels(exrhandle, rd_, layers_[i].name, layers_[i].datatype);
      }

      IMB_exr_close(exrhandle);
    }
  }
}

/******************************** Stereo3D ******************************/

OutputStereoOperation::OutputStereoOperation(const Scene *scene,
                                             const RenderData *rd,
                                             const bNodeTree *tree,
                                             DataType datatype,
                                             const ImageFormatData *format,
                                             const char *path,
                                             const char *pass_name,
                                             const char *view_name,
                                             const bool save_as_render)
    : OutputSingleLayerOperation(
          scene, rd, tree, datatype, format, path, view_name, save_as_render)
{
  BLI_strncpy(pass_name_, pass_name, sizeof(pass_name_));
  channels_ = get_datatype_size(datatype);
}

void *OutputStereoOperation::get_handle(const char *filepath)
{
  size_t width = this->get_width();
  size_t height = this->get_height();
  const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
  size_t i;

  if (width != 0 && height != 0) {
    void *exrhandle;

    exrhandle = IMB_exr_get_handle_name(filepath);

    if (!BKE_scene_multiview_is_render_view_first(rd_, view_name_)) {
      return exrhandle;
    }

    IMB_exr_clear_channels(exrhandle);

    for (i = 0; i < 2; i++) {
      IMB_exr_add_view(exrhandle, names[i]);
    }

    return exrhandle;
  }
  return nullptr;
}

void OutputStereoOperation::deinit_execution()
{
  uint width = this->get_width();
  uint height = this->get_height();

  if (width != 0 && height != 0) {
    void *exrhandle;

    exrhandle = this->get_handle(path_);
    float *buf = output_buffer_;

    /* populate single EXR channel with view data */
    IMB_exr_add_channel(exrhandle,
                        nullptr,
                        pass_name_,
                        view_name_,
                        1,
                        channels_ * width * height,
                        buf,
                        format_.depth == R_IMF_CHAN_DEPTH_16);

    image_input_ = nullptr;
    output_buffer_ = nullptr;

    /* create stereo ibuf */
    if (BKE_scene_multiview_is_render_view_last(rd_, view_name_)) {
      ImBuf *ibuf[3] = {nullptr};
      const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
      char filepath[FILE_MAX];
      int i;

      /* get rectf from EXR */
      for (i = 0; i < 2; i++) {
        float *rectf = IMB_exr_channel_rect(exrhandle, nullptr, pass_name_, names[i]);
        ibuf[i] = IMB_allocImBuf(width, height, format_.planes, 0);

        ibuf[i]->channels = channels_;
        ibuf[i]->rect_float = rectf;
        ibuf[i]->mall |= IB_rectfloat;
        ibuf[i]->dither = rd_->dither_intensity;

        /* do colormanagement in the individual views, so it doesn't need to do in the stereo */
        IMB_colormanagement_imbuf_for_write(ibuf[i], true, false, &format_);
      }

      /* create stereo buffer */
      ibuf[2] = IMB_stereo3d_ImBuf(&format_, ibuf[0], ibuf[1]);

      BKE_image_path_from_imformat(filepath,
                                   path_,
                                   BKE_main_blendfile_path_from_global(),
                                   rd_->cfra,
                                   &format_,
                                   (rd_->scemode & R_EXTENSION) != 0,
                                   true,
                                   nullptr);

      BKE_imbuf_write(ibuf[2], filepath, &format_);

      /* imbuf knows which rects are not part of ibuf */
      for (i = 0; i < 3; i++) {
        IMB_freeImBuf(ibuf[i]);
      }

      IMB_exr_close(exrhandle);
    }
  }
}

}  // namespace blender::compositor
