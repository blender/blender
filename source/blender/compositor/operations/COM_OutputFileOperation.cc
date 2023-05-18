/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#include "COM_OutputFileOperation.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DNA_color_types.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_pipeline.h"

namespace blender::compositor {

void add_exr_channels(void *exrhandle,
                      const char *layer_name,
                      const DataType datatype,
                      const char *view_name,
                      const size_t width,
                      bool use_half_float,
                      float *buf)
{
  /* create channels */
  switch (datatype) {
    case DataType::Value:
      IMB_exr_add_channel(
          exrhandle, layer_name, "V", view_name, 1, width, buf ? buf : nullptr, use_half_float);
      break;
    case DataType::Vector:
      IMB_exr_add_channel(exrhandle,
                          layer_name,
                          "X",
                          view_name,
                          3,
                          3 * width,
                          buf ? buf : nullptr,
                          use_half_float);
      IMB_exr_add_channel(exrhandle,
                          layer_name,
                          "Y",
                          view_name,
                          3,
                          3 * width,
                          buf ? buf + 1 : nullptr,
                          use_half_float);
      IMB_exr_add_channel(exrhandle,
                          layer_name,
                          "Z",
                          view_name,
                          3,
                          3 * width,
                          buf ? buf + 2 : nullptr,
                          use_half_float);
      break;
    case DataType::Color:
      IMB_exr_add_channel(exrhandle,
                          layer_name,
                          "R",
                          view_name,
                          4,
                          4 * width,
                          buf ? buf : nullptr,
                          use_half_float);
      IMB_exr_add_channel(exrhandle,
                          layer_name,
                          "G",
                          view_name,
                          4,
                          4 * width,
                          buf ? buf + 1 : nullptr,
                          use_half_float);
      IMB_exr_add_channel(exrhandle,
                          layer_name,
                          "B",
                          view_name,
                          4,
                          4 * width,
                          buf ? buf + 2 : nullptr,
                          use_half_float);
      IMB_exr_add_channel(exrhandle,
                          layer_name,
                          "A",
                          view_name,
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
                       const char *layer_name,
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
        rect = IMB_exr_channel_rect(exrhandle, layer_name, "V", srv->name);
        break;
      case DataType::Vector:
        rect = IMB_exr_channel_rect(exrhandle, layer_name, "X", srv->name);
        break;
      case DataType::Color:
        rect = IMB_exr_channel_rect(exrhandle, layer_name, "R", srv->name);
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

static float *init_buffer(uint width, uint height, DataType datatype)
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
                              uint width,
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
      reader->read_sampled(color, x, y, PixelSampler::Nearest);

      for (i = 0; i < size; i++) {
        buffer[offset + i] = color[i];
      }
      offset += size;

      if (tree->runtime->test_break && tree->runtime->test_break(tree->runtime->tbh)) {
        breaked = true;
      }
    }
    offset += (width - (x2 - x1)) * size;
  }
}

OutputSingleLayerOperation::OutputSingleLayerOperation(const Scene *scene,
                                                       const RenderData *rd,
                                                       const bNodeTree *tree,
                                                       DataType datatype,
                                                       const ImageFormatData *format,
                                                       const char *path,
                                                       const char *view_name,
                                                       const bool save_as_render)
{
  rd_ = rd;
  tree_ = tree;

  this->add_input_socket(datatype);

  output_buffer_ = nullptr;
  datatype_ = datatype;
  image_input_ = nullptr;

  BKE_image_format_init_for_write(&format_, scene, format);
  if (!save_as_render) {
    /* If not saving as render, stop IMB_colormanagement_imbuf_for_write using this
     * colorspace for conversion. */
    format_.linear_colorspace_settings.name[0] = '\0';
  }

  STRNCPY(path_, path);

  view_name_ = view_name;
  save_as_render_ = save_as_render;
}

OutputSingleLayerOperation::~OutputSingleLayerOperation()
{
  BKE_image_format_free(&format_);
}

void OutputSingleLayerOperation::init_execution()
{
  image_input_ = get_input_socket_reader(0);
  output_buffer_ = init_buffer(this->get_width(), this->get_height(), datatype_);
}

void OutputSingleLayerOperation::execute_region(rcti *rect, uint /*tile_number*/)
{
  write_buffer_rect(rect, tree_, image_input_, output_buffer_, this->get_width(), datatype_);
}

void OutputSingleLayerOperation::deinit_execution()
{
  if (this->get_width() * this->get_height() != 0) {

    int size = get_datatype_size(datatype_);
    ImBuf *ibuf = IMB_allocImBuf(this->get_width(), this->get_height(), format_.planes, 0);
    char filepath[FILE_MAX];
    const char *suffix;

    ibuf->channels = size;
    ibuf->dither = rd_->dither_intensity;

    IMB_assign_float_buffer(ibuf, output_buffer_, IB_TAKE_OWNERSHIP);

    IMB_colormanagement_imbuf_for_write(ibuf, save_as_render_, false, &format_);

    suffix = BKE_scene_multiview_view_suffix_get(rd_, view_name_);

    BKE_image_path_from_imformat(filepath,
                                 path_,
                                 BKE_main_blendfile_path_from_global(),
                                 rd_->cfra,
                                 &format_,
                                 (rd_->scemode & R_EXTENSION) != 0,
                                 true,
                                 suffix);

    if (0 == BKE_imbuf_write(ibuf, filepath, &format_)) {
      printf("Cannot save Node File Output to %s\n", filepath);
    }
    else {
      printf("Saved: %s\n", filepath);
    }

    IMB_freeImBuf(ibuf);
  }
  output_buffer_ = nullptr;
  image_input_ = nullptr;
}

void OutputSingleLayerOperation::update_memory_buffer_partial(MemoryBuffer * /*output*/,
                                                              const rcti &area,
                                                              Span<MemoryBuffer *> inputs)
{
  if (!output_buffer_) {
    return;
  }

  MemoryBuffer output_buf(output_buffer_,
                          COM_data_type_num_channels(datatype_),
                          this->get_width(),
                          this->get_height());
  const MemoryBuffer *input_image = inputs[0];
  output_buf.copy_from(input_image, area);
}

/******************************* MultiLayer *******************************/

OutputOpenExrLayer::OutputOpenExrLayer(const char *name_, DataType datatype_, bool use_layer_)
{
  STRNCPY(this->name, name_);
  this->datatype = datatype_;
  this->use_layer = use_layer_;

  /* these are created in init_execution */
  this->output_buffer = nullptr;
  this->image_input = nullptr;
}

OutputOpenExrMultiLayerOperation::OutputOpenExrMultiLayerOperation(const Scene *scene,
                                                                   const RenderData *rd,
                                                                   const bNodeTree *tree,
                                                                   const char *path,
                                                                   char exr_codec,
                                                                   bool exr_half_float,
                                                                   const char *view_name)
{
  scene_ = scene;
  rd_ = rd;
  tree_ = tree;

  STRNCPY(path_, path);
  exr_codec_ = exr_codec;
  exr_half_float_ = exr_half_float;
  view_name_ = view_name;
  this->set_canvas_input_index(RESOLUTION_INPUT_ANY);
}

void OutputOpenExrMultiLayerOperation::add_layer(const char *name,
                                                 DataType datatype,
                                                 bool use_layer)
{
  this->add_input_socket(datatype);
  layers_.append(OutputOpenExrLayer(name, datatype, use_layer));
}

StampData *OutputOpenExrMultiLayerOperation::create_stamp_data() const
{
  /* StampData API doesn't provide functions to modify an instance without having a RenderResult.
   */
  RenderResult render_result;
  StampData *stamp_data = BKE_stamp_info_from_scene_static(scene_);
  render_result.stamp_data = stamp_data;
  for (const OutputOpenExrLayer &layer : layers_) {
    /* Skip unconnected sockets. */
    if (layer.image_input == nullptr) {
      continue;
    }
    std::unique_ptr<MetaData> meta_data = layer.image_input->get_meta_data();
    if (meta_data) {
      blender::StringRef layer_name =
          blender::bke::cryptomatte::BKE_cryptomatte_extract_layer_name(
              blender::StringRef(layer.name, BLI_strnlen(layer.name, sizeof(layer.name))));
      meta_data->replace_hash_neutral_cryptomatte_keys(layer_name);
      meta_data->add_to_render_result(&render_result);
    }
  }
  return stamp_data;
}

void OutputOpenExrMultiLayerOperation::init_execution()
{
  for (uint i = 0; i < layers_.size(); i++) {
    if (layers_[i].use_layer) {
      SocketReader *reader = get_input_socket_reader(i);
      layers_[i].image_input = reader;
      layers_[i].output_buffer = init_buffer(
          this->get_width(), this->get_height(), layers_[i].datatype);
    }
  }
}

void OutputOpenExrMultiLayerOperation::execute_region(rcti *rect, uint /*tile_number*/)
{
  for (uint i = 0; i < layers_.size(); i++) {
    OutputOpenExrLayer &layer = layers_[i];
    if (layer.image_input) {
      write_buffer_rect(
          rect, tree_, layer.image_input, layer.output_buffer, this->get_width(), layer.datatype);
    }
  }
}

void OutputOpenExrMultiLayerOperation::deinit_execution()
{
  uint width = this->get_width();
  uint height = this->get_height();
  if (width != 0 && height != 0) {
    char filepath[FILE_MAX];
    const char *suffix;
    void *exrhandle = IMB_exr_get_handle();

    suffix = BKE_scene_multiview_view_suffix_get(rd_, view_name_);
    BKE_image_path_from_imtype(filepath,
                               path_,
                               BKE_main_blendfile_path_from_global(),
                               rd_->cfra,
                               R_IMF_IMTYPE_MULTILAYER,
                               (rd_->scemode & R_EXTENSION) != 0,
                               true,
                               suffix);
    BLI_file_ensure_parent_dir_exists(filepath);

    for (uint i = 0; i < layers_.size(); i++) {
      OutputOpenExrLayer &layer = layers_[i];
      if (!layer.image_input) {
        continue; /* skip unconnected sockets */
      }

      add_exr_channels(exrhandle,
                       layers_[i].name,
                       layers_[i].datatype,
                       "",
                       width,
                       exr_half_float_,
                       layers_[i].output_buffer);
    }

    /* When the filepath has no permissions, this can fail. */
    StampData *stamp_data = create_stamp_data();
    if (IMB_exr_begin_write(exrhandle, filepath, width, height, exr_codec_, stamp_data)) {
      IMB_exr_write_channels(exrhandle);
    }
    else {
      /* TODO: get the error from openexr's exception. */
      /* XXX: nice way to do report? */
      printf("Error Writing Render Result, see console\n");
    }

    IMB_exr_close(exrhandle);
    for (uint i = 0; i < layers_.size(); i++) {
      if (layers_[i].output_buffer) {
        MEM_freeN(layers_[i].output_buffer);
        layers_[i].output_buffer = nullptr;
      }

      layers_[i].image_input = nullptr;
    }
    BKE_stamp_data_free(stamp_data);
  }
}

void OutputOpenExrMultiLayerOperation::update_memory_buffer_partial(MemoryBuffer * /*output*/,
                                                                    const rcti &area,
                                                                    Span<MemoryBuffer *> inputs)
{
  for (int i = 0; i < layers_.size(); i++) {
    OutputOpenExrLayer &layer = layers_[i];
    int layer_num_channels = COM_data_type_num_channels(layer.datatype);
    if (layer.output_buffer) {
      MemoryBuffer output_buf(
          layer.output_buffer, layer_num_channels, this->get_width(), this->get_height());
      /* Input node always has 4 channels. Not all are needed depending on datatype. */
      output_buf.copy_from(inputs[i], area, 0, layer_num_channels, 0);
    }
  }
}

}  // namespace blender::compositor
