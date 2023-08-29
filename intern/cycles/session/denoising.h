/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __DENOISING_H__
#define __DENOISING_H__

/* TODO(sergey): Make it explicit and clear when something is a denoiser, its pipeline or
 * parameters. Currently it is an annoying mixture of terms used interchangeably. */

#include "device/device.h"
#include "integrator/denoiser.h"

#include "util/string.h"
#include "util/unique_ptr.h"
#include "util/vector.h"

#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING

CCL_NAMESPACE_BEGIN

/* Denoiser pipeline */

class DenoiserPipeline {
 public:
  DenoiserPipeline(DeviceInfo &device_info, const DenoiseParams &params);
  ~DenoiserPipeline();

  bool run();

  /* Error message after running, in case of failure. */
  string error;

  /* Sequential list of frame filepaths to denoise. */
  vector<string> input;
  /* Sequential list of frame filepaths to write result to. Empty entries
   * are skipped, so only a subset of the sequence can be denoised while
   * taking into account all input frames. */
  vector<string> output;

 protected:
  friend class DenoiseTask;

  Stats stats;
  Profiler profiler;
  Device *device;
  std::unique_ptr<Denoiser> denoiser;
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

  /* Write i-th channel of the processing output to output_to_image_channel[i]-th channel of the
   * file. */
  vector<int> output_to_image_channel;

  /* output_to_image_channel of the previous frame, if used. */
  vector<int> previous_output_to_image_channel;

  /* Detect whether this layer contains a full set of channels and set up the offsets accordingly.
   */
  bool detect_denoising_channels();

  /* Map the channels of a secondary frame to the channels that are required for processing,
   * fill neighbor_input_to_image_channel if all are present or return false if a channel are
   * missing. */
  bool match_channels(const std::vector<string> &channelnames,
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
  unique_ptr<ImageInput> in_previous;

  /* Render layers */
  vector<DenoiseImageLayer> layers;

  void free();

  /* Open the input image, parse its channels, open the output image and allocate the output
   * buffer. */
  bool load(const string &in_filepath, string &error);

  /* Load neighboring frames. */
  bool load_previous(const string &in_filepath, string &error);

  /* Load subset of pixels from file buffer into input buffer, as needed for denoising
   * on the device. Channels are reshuffled following the provided mapping. */
  void read_pixels(const DenoiseImageLayer &layer,
                   const BufferParams &params,
                   float *input_pixels);
  bool read_previous_pixels(const DenoiseImageLayer &layer,
                            const BufferParams &params,
                            float *input_pixels);

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
  DenoiseTask(Device *device, DenoiserPipeline *denoiser, int frame);
  ~DenoiseTask();

  /* Task stages */
  bool load();
  bool exec();
  bool save();
  void free();

  string error;

 protected:
  /* Denoiser parameters and device */
  DenoiserPipeline *denoiser;
  Device *device;

  /* Frame number to be denoised */
  int frame;

  /* Image file data */
  DenoiseImage image;
  int current_layer;

  RenderBuffers buffers;

  /* Task handling */
  bool load_input_pixels(int layer);
};

CCL_NAMESPACE_END

#endif /* __DENOISING_H__ */
