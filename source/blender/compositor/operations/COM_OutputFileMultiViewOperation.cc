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

#include "COM_OutputFileMultiViewOperation.h"

#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::compositor {

/************************************ OpenEXR Singlelayer Multiview ******************************/

OutputOpenExrSingleLayerMultiViewOperation::OutputOpenExrSingleLayerMultiViewOperation(
    const RenderData *rd,
    const bNodeTree *tree,
    DataType datatype,
    ImageFormatData *format,
    const char *path,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const char *view_name,
    const bool save_as_render)
    : OutputSingleLayerOperation(rd,
                                 tree,
                                 datatype,
                                 format,
                                 path,
                                 view_settings,
                                 display_settings,
                                 view_name,
                                 save_as_render)
{
}

void *OutputOpenExrSingleLayerMultiViewOperation::get_handle(const char *filename)
{
  size_t width = this->get_width();
  size_t height = this->get_height();
  SceneRenderView *srv;

  if (width != 0 && height != 0) {
    void *exrhandle;

    exrhandle = IMB_exr_get_handle_name(filename);

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

    BLI_make_existing_file(filename);

    /* prepare the file with all the channels */

    if (!IMB_exr_begin_write(exrhandle, filename, width, height, format_->exr_codec, nullptr)) {
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
  unsigned int width = this->get_width();
  unsigned int height = this->get_height();

  if (width != 0 && height != 0) {
    void *exrhandle;
    char filename[FILE_MAX];

    BKE_image_path_from_imtype(filename,
                               path_,
                               BKE_main_blendfile_path_from_global(),
                               rd_->cfra,
                               R_IMF_IMTYPE_OPENEXR,
                               (rd_->scemode & R_EXTENSION) != 0,
                               true,
                               nullptr);

    exrhandle = this->get_handle(filename);
    add_exr_channels(exrhandle,
                     nullptr,
                     datatype_,
                     view_name_,
                     width,
                     format_->depth == R_IMF_CHAN_DEPTH_16,
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

void *OutputOpenExrMultiLayerMultiViewOperation::get_handle(const char *filename)
{
  unsigned int width = this->get_width();
  unsigned int height = this->get_height();

  if (width != 0 && height != 0) {

    void *exrhandle;
    SceneRenderView *srv;

    /* get a new global handle */
    exrhandle = IMB_exr_get_handle_name(filename);

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

      for (unsigned int i = 0; i < layers_.size(); i++) {
        add_exr_channels(exrhandle,
                         layers_[i].name,
                         layers_[i].datatype,
                         srv->name,
                         width,
                         exr_half_float_,
                         nullptr);
      }
    }

    BLI_make_existing_file(filename);

    /* prepare the file with all the channels for the header */
    StampData *stamp_data = create_stamp_data();
    if (!IMB_exr_begin_write(exrhandle, filename, width, height, exr_codec_, stamp_data)) {
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
  unsigned int width = this->get_width();
  unsigned int height = this->get_height();

  if (width != 0 && height != 0) {
    void *exrhandle;
    char filename[FILE_MAX];

    BKE_image_path_from_imtype(filename,
                               path_,
                               BKE_main_blendfile_path_from_global(),
                               rd_->cfra,
                               R_IMF_IMTYPE_MULTILAYER,
                               (rd_->scemode & R_EXTENSION) != 0,
                               true,
                               nullptr);

    exrhandle = this->get_handle(filename);

    for (unsigned int i = 0; i < layers_.size(); i++) {
      add_exr_channels(exrhandle,
                       layers_[i].name,
                       layers_[i].datatype,
                       view_name_,
                       width,
                       exr_half_float_,
                       layers_[i].output_buffer);
    }

    for (unsigned int i = 0; i < layers_.size(); i++) {
      /* memory can only be freed after we write all views to the file */
      layers_[i].output_buffer = nullptr;
      layers_[i].image_input = nullptr;
    }

    /* ready to close the file */
    if (BKE_scene_multiview_is_render_view_last(rd_, view_name_)) {
      IMB_exr_write_channels(exrhandle);

      /* free buffer memory for all the views */
      for (unsigned int i = 0; i < layers_.size(); i++) {
        free_exr_channels(exrhandle, rd_, layers_[i].name, layers_[i].datatype);
      }

      IMB_exr_close(exrhandle);
    }
  }
}

/******************************** Stereo3D ******************************/

OutputStereoOperation::OutputStereoOperation(const RenderData *rd,
                                             const bNodeTree *tree,
                                             DataType datatype,
                                             ImageFormatData *format,
                                             const char *path,
                                             const char *name,
                                             const ColorManagedViewSettings *view_settings,
                                             const ColorManagedDisplaySettings *display_settings,
                                             const char *view_name,
                                             const bool save_as_render)
    : OutputSingleLayerOperation(rd,
                                 tree,
                                 datatype,
                                 format,
                                 path,
                                 view_settings,
                                 display_settings,
                                 view_name,
                                 save_as_render)
{
  BLI_strncpy(name_, name, sizeof(name_));
  channels_ = get_datatype_size(datatype);
}

void *OutputStereoOperation::get_handle(const char *filename)
{
  size_t width = this->get_width();
  size_t height = this->get_height();
  const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
  size_t i;

  if (width != 0 && height != 0) {
    void *exrhandle;

    exrhandle = IMB_exr_get_handle_name(filename);

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
  unsigned int width = this->get_width();
  unsigned int height = this->get_height();

  if (width != 0 && height != 0) {
    void *exrhandle;

    exrhandle = this->get_handle(path_);
    float *buf = output_buffer_;

    /* populate single EXR channel with view data */
    IMB_exr_add_channel(exrhandle,
                        nullptr,
                        name_,
                        view_name_,
                        1,
                        channels_ * width * height,
                        buf,
                        format_->depth == R_IMF_CHAN_DEPTH_16);

    image_input_ = nullptr;
    output_buffer_ = nullptr;

    /* create stereo ibuf */
    if (BKE_scene_multiview_is_render_view_last(rd_, view_name_)) {
      ImBuf *ibuf[3] = {nullptr};
      const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
      char filename[FILE_MAX];
      int i;

      /* get rectf from EXR */
      for (i = 0; i < 2; i++) {
        float *rectf = IMB_exr_channel_rect(exrhandle, nullptr, name_, names[i]);
        ibuf[i] = IMB_allocImBuf(width, height, format_->planes, 0);

        ibuf[i]->channels = channels_;
        ibuf[i]->rect_float = rectf;
        ibuf[i]->mall |= IB_rectfloat;
        ibuf[i]->dither = rd_->dither_intensity;

        /* do colormanagement in the individual views, so it doesn't need to do in the stereo */
        IMB_colormanagement_imbuf_for_write(
            ibuf[i], true, false, view_settings_, display_settings_, format_);
        IMB_prepare_write_ImBuf(IMB_isfloat(ibuf[i]), ibuf[i]);
      }

      /* create stereo buffer */
      ibuf[2] = IMB_stereo3d_ImBuf(format_, ibuf[0], ibuf[1]);

      BKE_image_path_from_imformat(filename,
                                   path_,
                                   BKE_main_blendfile_path_from_global(),
                                   rd_->cfra,
                                   format_,
                                   (rd_->scemode & R_EXTENSION) != 0,
                                   true,
                                   nullptr);

      BKE_imbuf_write(ibuf[2], filename, format_);

      /* imbuf knows which rects are not part of ibuf */
      for (i = 0; i < 3; i++) {
        IMB_freeImBuf(ibuf[i]);
      }

      IMB_exr_close(exrhandle);
    }
  }
}

}  // namespace blender::compositor
