/*
 * Copyright 2011-2018 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __DENOISING_H__
#define __DENOISING_H__

#include "device/device.h"
#include "device/device_denoising.h"

#include "render/buffers.h"

#include "util/util_string.h"
#include "util/util_vector.h"
#include "util/util_unique_ptr.h"

#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING

CCL_NAMESPACE_BEGIN

/* Denoiser */

class Denoiser {
 public:
  Denoiser(DeviceInfo &device_info);
  ~Denoiser();

  bool run();

  /* Error message after running, in case of failure. */
  string error;

  /* Sequential list of frame filepaths to denoise. */
  vector<string> input;
  /* Sequential list of frame filepaths to write result to. Empty entries
   * are skipped, so only a subset of the sequence can be denoised while
   * taking into account all input frames. */
  vector<string> output;

  /* Sample number override, takes precedence over values from input frames. */
  int samples_override;
  /* Tile size for processing on device. */
  int2 tile_size;

  /* Equivalent to the settings in the regular denoiser. */
  DenoiseParams params;

 protected:
  friend class DenoiseTask;

  Stats stats;
  Profiler profiler;
  Device *device;

  int num_frames;
};

/* Denoise Image Layer */

struct DenoiseImageLayer {
  string name;
  /* All channels belonging to this DenoiseImageLayer. */
  vector<string> channels;
  /* Layer to image channel mapping. */
  vector<int> layer_to_image_channel;

  /* Sample amount that was used for rendering this layer. */
  int samples;

  /* Device input channel will be copied from image channel input_to_image_channel[i]. */
  vector<int> input_to_image_channel;

  /* input_to_image_channel of the secondary frames, if any are used. */
  vector<vector<int>> neighbor_input_to_image_channel;

  /* Write i-th channel of the processing output to output_to_image_channel[i]-th channel of the
   * file. */
  vector<int> output_to_image_channel;

  /* Detect whether this layer contains a full set of channels and set up the offsets accordingly.
   */
  bool detect_denoising_channels();

  /* Map the channels of a secondary frame to the channels that are required for processing,
   * fill neighbor_input_to_image_channel if all are present or return false if a channel are
   * missing. */
  bool match_channels(int neighbor,
                      const std::vector<string> &channelnames,
                      const std::vector<string> &neighbor_channelnames);
};

/* Denoise Image Data */

class DenoiseImage {
 public:
  DenoiseImage();
  ~DenoiseImage();

  /* Dimensions */
  int width, height, num_channels;

  /* Samples */
  int samples;

  /* Pixel buffer with interleaved channels. */
  array<float> pixels;

  /* Image file handles */
  ImageSpec in_spec;
  vector<unique_ptr<ImageInput>> in_neighbors;

  /* Render layers */
  vector<DenoiseImageLayer> layers;

  void free();

  /* Open the input image, parse its channels, open the output image and allocate the output
   * buffer. */
  bool load(const string &in_filepath, string &error);

  /* Load neighboring frames. */
  bool load_neighbors(const vector<string> &filepaths, const vector<int> &frames, string &error);

  /* Load subset of pixels from file buffer into input buffer, as needed for denoising
   * on the device. Channels are reshuffled following the provided mapping. */
  void read_pixels(const DenoiseImageLayer &layer, float *input_pixels);
  bool read_neighbor_pixels(int neighbor, const DenoiseImageLayer &layer, float *input_pixels);

  bool save_output(const string &out_filepath, string &error);

 protected:
  /* Parse input file channels, separate them into DenoiseImageLayers,
   * detect DenoiseImageLayers with full channel sets,
   * fill layers and set up the output channels and passthrough map. */
  bool parse_channels(const ImageSpec &in_spec, string &error);

  void close_input();
};

/* Denoise Task */

class DenoiseTask {
 public:
  DenoiseTask(Device *device, Denoiser *denoiser, int frame, const vector<int> &neighbor_frames);
  ~DenoiseTask();

  /* Task stages */
  bool load();
  bool exec();
  bool save();
  void free();

  string error;

 protected:
  /* Denoiser parameters and device */
  Denoiser *denoiser;
  Device *device;

  /* Frame number to be denoised */
  int frame;
  vector<int> neighbor_frames;

  /* Image file data */
  DenoiseImage image;
  int current_layer;

  /* Device input buffer */
  device_vector<float> input_pixels;

  /* Tiles */
  thread_mutex tiles_mutex;
  list<RenderTile> tiles;
  int num_tiles;

  thread_mutex output_mutex;
  map<int, device_vector<float> *> output_pixels;

  /* Task handling */
  bool load_input_pixels(int layer);
  void create_task(DeviceTask &task);

  /* Device task callbacks */
  bool acquire_tile(Device *device, Device *tile_device, RenderTile &tile);
  void map_neighboring_tiles(RenderTile *tiles, Device *tile_device);
  void unmap_neighboring_tiles(RenderTile *tiles);
  void release_tile();
  bool get_cancel();
};

CCL_NAMESPACE_END

#endif /* __DENOISING_H__ */
