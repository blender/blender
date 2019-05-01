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

#include "COM_OutputFileOperation.h"
#include "COM_OutputFileMultiViewOperation.h"

#include <string.h>

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DNA_color_types.h"
#include "MEM_guardedalloc.h"

extern "C" {
#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"
}

/************************************ OpenEXR Singlelayer Multiview ******************************/

OutputOpenExrSingleLayerMultiViewOperation::OutputOpenExrSingleLayerMultiViewOperation(
    const RenderData *rd,
    const bNodeTree *tree,
    DataType datatype,
    ImageFormatData *format,
    const char *path,
    const ColorManagedViewSettings *viewSettings,
    const ColorManagedDisplaySettings *displaySettings,
    const char *viewName)
    : OutputSingleLayerOperation(
          rd, tree, datatype, format, path, viewSettings, displaySettings, viewName)
{
}

void *OutputOpenExrSingleLayerMultiViewOperation::get_handle(const char *filename)
{
  size_t width = this->getWidth();
  size_t height = this->getHeight();
  SceneRenderView *srv;

  if (width != 0 && height != 0) {
    void *exrhandle;

    exrhandle = IMB_exr_get_handle_name(filename);

    if (!BKE_scene_multiview_is_render_view_first(this->m_rd, this->m_viewName)) {
      return exrhandle;
    }

    IMB_exr_clear_channels(exrhandle);

    for (srv = (SceneRenderView *)this->m_rd->views.first; srv; srv = srv->next) {
      if (BKE_scene_multiview_is_render_view_active(this->m_rd, srv) == false) {
        continue;
      }

      IMB_exr_add_view(exrhandle, srv->name);
      add_exr_channels(exrhandle, NULL, this->m_datatype, srv->name, width, false, NULL);
    }

    BLI_make_existing_file(filename);

    /* prepare the file with all the channels */

    if (IMB_exr_begin_write(exrhandle, filename, width, height, this->m_format->exr_codec, NULL) ==
        0) {
      printf("Error Writing Singlelayer Multiview Openexr\n");
      IMB_exr_close(exrhandle);
    }
    else {
      IMB_exr_clear_channels(exrhandle);
      return exrhandle;
    }
  }
  return NULL;
}

void OutputOpenExrSingleLayerMultiViewOperation::deinitExecution()
{
  unsigned int width = this->getWidth();
  unsigned int height = this->getHeight();

  if (width != 0 && height != 0) {
    void *exrhandle;
    char filename[FILE_MAX];

    BKE_image_path_from_imtype(filename,
                               this->m_path,
                               BKE_main_blendfile_path_from_global(),
                               this->m_rd->cfra,
                               R_IMF_IMTYPE_OPENEXR,
                               (this->m_rd->scemode & R_EXTENSION) != 0,
                               true,
                               NULL);

    exrhandle = this->get_handle(filename);
    add_exr_channels(exrhandle,
                     NULL,
                     this->m_datatype,
                     this->m_viewName,
                     width,
                     this->m_format->depth == R_IMF_CHAN_DEPTH_16,
                     this->m_outputBuffer);

    /* memory can only be freed after we write all views to the file */
    this->m_outputBuffer = NULL;
    this->m_imageInput = NULL;

    /* ready to close the file */
    if (BKE_scene_multiview_is_render_view_last(this->m_rd, this->m_viewName)) {
      IMB_exr_write_channels(exrhandle);

      /* free buffer memory for all the views */
      free_exr_channels(exrhandle, this->m_rd, NULL, this->m_datatype);

      /* remove exr handle and data */
      IMB_exr_close(exrhandle);
    }
  }
}

/************************************ OpenEXR Multilayer Multiview *******************************/

OutputOpenExrMultiLayerMultiViewOperation::OutputOpenExrMultiLayerMultiViewOperation(
    const RenderData *rd,
    const bNodeTree *tree,
    const char *path,
    char exr_codec,
    bool exr_half_float,
    const char *viewName)
    : OutputOpenExrMultiLayerOperation(rd, tree, path, exr_codec, exr_half_float, viewName)
{
}

void *OutputOpenExrMultiLayerMultiViewOperation::get_handle(const char *filename)
{
  unsigned int width = this->getWidth();
  unsigned int height = this->getHeight();

  if (width != 0 && height != 0) {

    void *exrhandle;
    SceneRenderView *srv;

    /* get a new global handle */
    exrhandle = IMB_exr_get_handle_name(filename);

    if (!BKE_scene_multiview_is_render_view_first(this->m_rd, this->m_viewName)) {
      return exrhandle;
    }

    IMB_exr_clear_channels(exrhandle);

    /* check renderdata for amount of views */
    for (srv = (SceneRenderView *)this->m_rd->views.first; srv; srv = srv->next) {

      if (BKE_scene_multiview_is_render_view_active(this->m_rd, srv) == false) {
        continue;
      }

      IMB_exr_add_view(exrhandle, srv->name);

      for (unsigned int i = 0; i < this->m_layers.size(); ++i) {
        add_exr_channels(exrhandle,
                         this->m_layers[i].name,
                         this->m_layers[i].datatype,
                         srv->name,
                         width,
                         this->m_exr_half_float,
                         NULL);
      }
    }

    BLI_make_existing_file(filename);

    /* prepare the file with all the channels for the header */
    if (IMB_exr_begin_write(exrhandle, filename, width, height, this->m_exr_codec, NULL) == 0) {
      printf("Error Writing Multilayer Multiview Openexr\n");
      IMB_exr_close(exrhandle);
    }
    else {
      IMB_exr_clear_channels(exrhandle);
      return exrhandle;
    }
  }
  return NULL;
}

void OutputOpenExrMultiLayerMultiViewOperation::deinitExecution()
{
  unsigned int width = this->getWidth();
  unsigned int height = this->getHeight();

  if (width != 0 && height != 0) {
    void *exrhandle;
    char filename[FILE_MAX];

    BKE_image_path_from_imtype(filename,
                               this->m_path,
                               BKE_main_blendfile_path_from_global(),
                               this->m_rd->cfra,
                               R_IMF_IMTYPE_MULTILAYER,
                               (this->m_rd->scemode & R_EXTENSION) != 0,
                               true,
                               NULL);

    exrhandle = this->get_handle(filename);

    for (unsigned int i = 0; i < this->m_layers.size(); ++i) {
      add_exr_channels(exrhandle,
                       this->m_layers[i].name,
                       this->m_layers[i].datatype,
                       this->m_viewName,
                       width,
                       this->m_exr_half_float,
                       this->m_layers[i].outputBuffer);
    }

    for (unsigned int i = 0; i < this->m_layers.size(); ++i) {
      /* memory can only be freed after we write all views to the file */
      this->m_layers[i].outputBuffer = NULL;
      this->m_layers[i].imageInput = NULL;
    }

    /* ready to close the file */
    if (BKE_scene_multiview_is_render_view_last(this->m_rd, this->m_viewName)) {
      IMB_exr_write_channels(exrhandle);

      /* free buffer memory for all the views */
      for (unsigned int i = 0; i < this->m_layers.size(); ++i) {
        free_exr_channels(
            exrhandle, this->m_rd, this->m_layers[i].name, this->m_layers[i].datatype);
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
                                             const ColorManagedViewSettings *viewSettings,
                                             const ColorManagedDisplaySettings *displaySettings,
                                             const char *viewName)
    : OutputSingleLayerOperation(
          rd, tree, datatype, format, path, viewSettings, displaySettings, viewName)
{
  BLI_strncpy(this->m_name, name, sizeof(this->m_name));
  this->m_channels = get_datatype_size(datatype);
}

void *OutputStereoOperation::get_handle(const char *filename)
{
  size_t width = this->getWidth();
  size_t height = this->getHeight();
  const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
  size_t i;

  if (width != 0 && height != 0) {
    void *exrhandle;

    exrhandle = IMB_exr_get_handle_name(filename);

    if (!BKE_scene_multiview_is_render_view_first(this->m_rd, this->m_viewName)) {
      return exrhandle;
    }

    IMB_exr_clear_channels(exrhandle);

    for (i = 0; i < 2; i++) {
      IMB_exr_add_view(exrhandle, names[i]);
    }

    return exrhandle;
  }
  return NULL;
}

void OutputStereoOperation::deinitExecution()
{
  unsigned int width = this->getWidth();
  unsigned int height = this->getHeight();

  if (width != 0 && height != 0) {
    void *exrhandle;

    exrhandle = this->get_handle(this->m_path);
    float *buf = this->m_outputBuffer;

    /* populate single EXR channel with view data */
    IMB_exr_add_channel(exrhandle,
                        NULL,
                        this->m_name,
                        this->m_viewName,
                        1,
                        this->m_channels * width * height,
                        buf,
                        this->m_format->depth == R_IMF_CHAN_DEPTH_16);

    this->m_imageInput = NULL;
    this->m_outputBuffer = NULL;

    /* create stereo ibuf */
    if (BKE_scene_multiview_is_render_view_last(this->m_rd, this->m_viewName)) {
      ImBuf *ibuf[3] = {NULL};
      const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
      char filename[FILE_MAX];
      int i;

      /* get rectf from EXR */
      for (i = 0; i < 2; i++) {
        float *rectf = IMB_exr_channel_rect(exrhandle, NULL, this->m_name, names[i]);
        ibuf[i] = IMB_allocImBuf(width, height, this->m_format->planes, 0);

        ibuf[i]->channels = this->m_channels;
        ibuf[i]->rect_float = rectf;
        ibuf[i]->mall |= IB_rectfloat;
        ibuf[i]->dither = this->m_rd->dither_intensity;

        /* do colormanagement in the individual views, so it doesn't need to do in the stereo */
        IMB_colormanagement_imbuf_for_write(
            ibuf[i], true, false, this->m_viewSettings, this->m_displaySettings, this->m_format);
        IMB_prepare_write_ImBuf(IMB_isfloat(ibuf[i]), ibuf[i]);
      }

      /* create stereo buffer */
      ibuf[2] = IMB_stereo3d_ImBuf(this->m_format, ibuf[0], ibuf[1]);

      BKE_image_path_from_imformat(filename,
                                   this->m_path,
                                   BKE_main_blendfile_path_from_global(),
                                   this->m_rd->cfra,
                                   this->m_format,
                                   (this->m_rd->scemode & R_EXTENSION) != 0,
                                   true,
                                   NULL);

      BKE_imbuf_write(ibuf[2], filename, this->m_format);

      /* imbuf knows which rects are not part of ibuf */
      for (i = 0; i < 3; i++) {
        IMB_freeImBuf(ibuf[i]);
      }

      IMB_exr_close(exrhandle);
    }
  }
}
