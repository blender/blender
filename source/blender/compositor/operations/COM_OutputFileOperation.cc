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

#include "COM_OutputFileOperation.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DNA_color_types.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_pipeline.h"

namespace blender::compositor {

void add_exr_channels(void *exrhandle,
                      const char *layerName,
                      const DataType datatype,
                      const char *viewName,
                      const size_t width,
                      bool use_half_float,
                      float *buf)
{
  /* create channels */
  switch (datatype) {
    case DataType::Value:
      IMB_exr_add_channel(
          exrhandle, layerName, "V", viewName, 1, width, buf ? buf : nullptr, use_half_float);
      break;
    case DataType::Vector:
      IMB_exr_add_channel(
          exrhandle, layerName, "X", viewName, 3, 3 * width, buf ? buf : nullptr, use_half_float);
      IMB_exr_add_channel(exrhandle,
                          layerName,
                          "Y",
                          viewName,
                          3,
                          3 * width,
                          buf ? buf + 1 : nullptr,
                          use_half_float);
      IMB_exr_add_channel(exrhandle,
                          layerName,
                          "Z",
                          viewName,
                          3,
                          3 * width,
                          buf ? buf + 2 : nullptr,
                          use_half_float);
      break;
    case DataType::Color:
      IMB_exr_add_channel(
          exrhandle, layerName, "R", viewName, 4, 4 * width, buf ? buf : nullptr, use_half_float);
      IMB_exr_add_channel(exrhandle,
                          layerName,
                          "G",
                          viewName,
                          4,
                          4 * width,
                          buf ? buf + 1 : nullptr,
                          use_half_float);
      IMB_exr_add_channel(exrhandle,
                          layerName,
                          "B",
                          viewName,
                          4,
                          4 * width,
                          buf ? buf + 2 : nullptr,
                          use_half_float);
      IMB_exr_add_channel(exrhandle,
                          layerName,
                          "A",
                          viewName,
                          4,
                          4 * width,
                          buf ? buf + 3 : nullptr,
                          use_half_float);
      break;
    default:
      break;
  }
}

void free_exr_channels(void *exrhandle,
                       const RenderData *rd,
                       const char *layerName,
                       const DataType datatype)
{
  SceneRenderView *srv;

  /* check renderdata for amount of views */
  for (srv = (SceneRenderView *)rd->views.first; srv; srv = srv->next) {
    float *rect = nullptr;

    if (BKE_scene_multiview_is_render_view_active(rd, srv) == false) {
      continue;
    }

    /* the pointer is stored in the first channel of each datatype */
    switch (datatype) {
      case DataType::Value:
        rect = IMB_exr_channel_rect(exrhandle, layerName, "V", srv->name);
        break;
      case DataType::Vector:
        rect = IMB_exr_channel_rect(exrhandle, layerName, "X", srv->name);
        break;
      case DataType::Color:
        rect = IMB_exr_channel_rect(exrhandle, layerName, "R", srv->name);
        break;
      default:
        break;
    }
    if (rect) {
      MEM_freeN(rect);
    }
  }
}

int get_datatype_size(DataType datatype)
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

static float *init_buffer(unsigned int width, unsigned int height, DataType datatype)
{
  /* When initializing the tree during initial load the width and height can be zero. */
  if (width != 0 && height != 0) {
    int size = get_datatype_size(datatype);
    return (float *)MEM_callocN(width * height * size * sizeof(float), "OutputFile buffer");
  }

  return nullptr;
}

static void write_buffer_rect(rcti *rect,
                              const bNodeTree *tree,
                              SocketReader *reader,
                              float *buffer,
                              unsigned int width,
                              DataType datatype)
{
  float color[4];
  int i, size = get_datatype_size(datatype);

  if (!buffer) {
    return;
  }
  int x1 = rect->xmin;
  int y1 = rect->ymin;
  int x2 = rect->xmax;
  int y2 = rect->ymax;
  int offset = (y1 * width + x1) * size;
  int x;
  int y;
  bool breaked = false;

  for (y = y1; y < y2 && (!breaked); y++) {
    for (x = x1; x < x2 && (!breaked); x++) {
      reader->readSampled(color, x, y, PixelSampler::Nearest);

      for (i = 0; i < size; i++) {
        buffer[offset + i] = color[i];
      }
      offset += size;

      if (tree->test_break && tree->test_break(tree->tbh)) {
        breaked = true;
      }
    }
    offset += (width - (x2 - x1)) * size;
  }
}

OutputSingleLayerOperation::OutputSingleLayerOperation(
    const RenderData *rd,
    const bNodeTree *tree,
    DataType datatype,
    ImageFormatData *format,
    const char *path,
    const ColorManagedViewSettings *viewSettings,
    const ColorManagedDisplaySettings *displaySettings,
    const char *viewName,
    const bool saveAsRender)
{
  rd_ = rd;
  tree_ = tree;

  this->addInputSocket(datatype);

  outputBuffer_ = nullptr;
  datatype_ = datatype;
  imageInput_ = nullptr;

  format_ = format;
  BLI_strncpy(path_, path, sizeof(path_));

  viewSettings_ = viewSettings;
  displaySettings_ = displaySettings;
  viewName_ = viewName;
  saveAsRender_ = saveAsRender;
}

void OutputSingleLayerOperation::initExecution()
{
  imageInput_ = getInputSocketReader(0);
  outputBuffer_ = init_buffer(this->getWidth(), this->getHeight(), datatype_);
}

void OutputSingleLayerOperation::executeRegion(rcti *rect, unsigned int /*tileNumber*/)
{
  write_buffer_rect(rect, tree_, imageInput_, outputBuffer_, this->getWidth(), datatype_);
}

void OutputSingleLayerOperation::deinitExecution()
{
  if (this->getWidth() * this->getHeight() != 0) {

    int size = get_datatype_size(datatype_);
    ImBuf *ibuf = IMB_allocImBuf(this->getWidth(), this->getHeight(), format_->planes, 0);
    char filename[FILE_MAX];
    const char *suffix;

    ibuf->channels = size;
    ibuf->rect_float = outputBuffer_;
    ibuf->mall |= IB_rectfloat;
    ibuf->dither = rd_->dither_intensity;

    IMB_colormanagement_imbuf_for_write(
        ibuf, saveAsRender_, false, viewSettings_, displaySettings_, format_);

    suffix = BKE_scene_multiview_view_suffix_get(rd_, viewName_);

    BKE_image_path_from_imformat(filename,
                                 path_,
                                 BKE_main_blendfile_path_from_global(),
                                 rd_->cfra,
                                 format_,
                                 (rd_->scemode & R_EXTENSION) != 0,
                                 true,
                                 suffix);

    if (0 == BKE_imbuf_write(ibuf, filename, format_)) {
      printf("Cannot save Node File Output to %s\n", filename);
    }
    else {
      printf("Saved: %s\n", filename);
    }

    IMB_freeImBuf(ibuf);
  }
  outputBuffer_ = nullptr;
  imageInput_ = nullptr;
}

void OutputSingleLayerOperation::update_memory_buffer_partial(MemoryBuffer *UNUSED(output),
                                                              const rcti &area,
                                                              Span<MemoryBuffer *> inputs)
{
  if (!outputBuffer_) {
    return;
  }

  MemoryBuffer output_buf(
      outputBuffer_, COM_data_type_num_channels(datatype_), this->getWidth(), this->getHeight());
  const MemoryBuffer *input_image = inputs[0];
  output_buf.copy_from(input_image, area);
}

/******************************* MultiLayer *******************************/

OutputOpenExrLayer::OutputOpenExrLayer(const char *name_, DataType datatype_, bool use_layer_)
{
  BLI_strncpy(this->name, name_, sizeof(this->name));
  this->datatype = datatype_;
  this->use_layer = use_layer_;

  /* these are created in initExecution */
  this->outputBuffer = nullptr;
  this->imageInput = nullptr;
}

OutputOpenExrMultiLayerOperation::OutputOpenExrMultiLayerOperation(const Scene *scene,
                                                                   const RenderData *rd,
                                                                   const bNodeTree *tree,
                                                                   const char *path,
                                                                   char exr_codec,
                                                                   bool exr_half_float,
                                                                   const char *viewName)
{
  scene_ = scene;
  rd_ = rd;
  tree_ = tree;

  BLI_strncpy(path_, path, sizeof(path_));
  exr_codec_ = exr_codec;
  exr_half_float_ = exr_half_float;
  viewName_ = viewName;
  this->set_canvas_input_index(RESOLUTION_INPUT_ANY);
}

void OutputOpenExrMultiLayerOperation::add_layer(const char *name,
                                                 DataType datatype,
                                                 bool use_layer)
{
  this->addInputSocket(datatype);
  layers_.append(OutputOpenExrLayer(name, datatype, use_layer));
}

StampData *OutputOpenExrMultiLayerOperation::createStampData() const
{
  /* StampData API doesn't provide functions to modify an instance without having a RenderResult.
   */
  RenderResult render_result;
  StampData *stamp_data = BKE_stamp_info_from_scene_static(scene_);
  render_result.stamp_data = stamp_data;
  for (const OutputOpenExrLayer &layer : layers_) {
    /* Skip unconnected sockets. */
    if (layer.imageInput == nullptr) {
      continue;
    }
    std::unique_ptr<MetaData> meta_data = layer.imageInput->getMetaData();
    if (meta_data) {
      blender::StringRef layer_name =
          blender::bke::cryptomatte::BKE_cryptomatte_extract_layer_name(
              blender::StringRef(layer.name, BLI_strnlen(layer.name, sizeof(layer.name))));
      meta_data->replaceHashNeutralCryptomatteKeys(layer_name);
      meta_data->addToRenderResult(&render_result);
    }
  }
  return stamp_data;
}

void OutputOpenExrMultiLayerOperation::initExecution()
{
  for (unsigned int i = 0; i < layers_.size(); i++) {
    if (layers_[i].use_layer) {
      SocketReader *reader = getInputSocketReader(i);
      layers_[i].imageInput = reader;
      layers_[i].outputBuffer = init_buffer(
          this->getWidth(), this->getHeight(), layers_[i].datatype);
    }
  }
}

void OutputOpenExrMultiLayerOperation::executeRegion(rcti *rect, unsigned int /*tileNumber*/)
{
  for (unsigned int i = 0; i < layers_.size(); i++) {
    OutputOpenExrLayer &layer = layers_[i];
    if (layer.imageInput) {
      write_buffer_rect(
          rect, tree_, layer.imageInput, layer.outputBuffer, this->getWidth(), layer.datatype);
    }
  }
}

void OutputOpenExrMultiLayerOperation::deinitExecution()
{
  unsigned int width = this->getWidth();
  unsigned int height = this->getHeight();
  if (width != 0 && height != 0) {
    char filename[FILE_MAX];
    const char *suffix;
    void *exrhandle = IMB_exr_get_handle();

    suffix = BKE_scene_multiview_view_suffix_get(rd_, viewName_);
    BKE_image_path_from_imtype(filename,
                               path_,
                               BKE_main_blendfile_path_from_global(),
                               rd_->cfra,
                               R_IMF_IMTYPE_MULTILAYER,
                               (rd_->scemode & R_EXTENSION) != 0,
                               true,
                               suffix);
    BLI_make_existing_file(filename);

    for (unsigned int i = 0; i < layers_.size(); i++) {
      OutputOpenExrLayer &layer = layers_[i];
      if (!layer.imageInput) {
        continue; /* skip unconnected sockets */
      }

      add_exr_channels(exrhandle,
                       layers_[i].name,
                       layers_[i].datatype,
                       "",
                       width,
                       exr_half_float_,
                       layers_[i].outputBuffer);
    }

    /* when the filename has no permissions, this can fail */
    StampData *stamp_data = createStampData();
    if (IMB_exr_begin_write(exrhandle, filename, width, height, exr_codec_, stamp_data)) {
      IMB_exr_write_channels(exrhandle);
    }
    else {
      /* TODO: get the error from openexr's exception. */
      /* XXX: nice way to do report? */
      printf("Error Writing Render Result, see console\n");
    }

    IMB_exr_close(exrhandle);
    for (unsigned int i = 0; i < layers_.size(); i++) {
      if (layers_[i].outputBuffer) {
        MEM_freeN(layers_[i].outputBuffer);
        layers_[i].outputBuffer = nullptr;
      }

      layers_[i].imageInput = nullptr;
    }
    BKE_stamp_data_free(stamp_data);
  }
}

void OutputOpenExrMultiLayerOperation::update_memory_buffer_partial(MemoryBuffer *UNUSED(output),
                                                                    const rcti &area,
                                                                    Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_image = inputs[0];
  for (int i = 0; i < layers_.size(); i++) {
    OutputOpenExrLayer &layer = layers_[i];
    if (layer.outputBuffer) {
      MemoryBuffer output_buf(layer.outputBuffer,
                              COM_data_type_num_channels(layer.datatype),
                              this->getWidth(),
                              this->getHeight());
      output_buf.copy_from(input_image, area);
    }
  }
}

}  // namespace blender::compositor
