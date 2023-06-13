/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

#include "app/oiio_output_driver.h"

#include "scene/colorspace.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

CCL_NAMESPACE_BEGIN

OIIOOutputDriver::OIIOOutputDriver(const string_view filepath,
                                   const string_view pass,
                                   LogFunction log)
    : filepath_(filepath), pass_(pass), log_(log)
{
}

OIIOOutputDriver::~OIIOOutputDriver() {}

void OIIOOutputDriver::write_render_tile(const Tile &tile)
{
  /* Only write the full buffer, no intermediate tiles. */
  if (!(tile.size == tile.full_size)) {
    return;
  }

  log_(string_printf("Writing image %s", filepath_.c_str()));

  unique_ptr<ImageOutput> image_output(ImageOutput::create(filepath_));
  if (image_output == nullptr) {
    log_("Failed to create image file");
    return;
  }

  const int width = tile.size.x;
  const int height = tile.size.y;

  ImageSpec spec(width, height, 4, TypeDesc::FLOAT);
  if (!image_output->open(filepath_, spec)) {
    log_("Failed to create image file");
    return;
  }

  vector<float> pixels(width * height * 4);
  if (!tile.get_pass_pixels(pass_, 4, pixels.data())) {
    log_("Failed to read render pass pixels");
    return;
  }

  /* Manipulate offset and stride to convert from bottom-up to top-down convention. */
  ImageBuf image_buffer(spec,
                        pixels.data() + (height - 1) * width * 4,
                        AutoStride,
                        -width * 4 * sizeof(float),
                        AutoStride);

  /* Apply gamma correction for (some) non-linear file formats.
   * TODO: use OpenColorIO view transform if available. */
  if (ColorSpaceManager::detect_known_colorspace(
          u_colorspace_auto, "", image_output->format_name(), true) == u_colorspace_srgb)
  {
    const float g = 1.0f / 2.2f;
    ImageBufAlgo::pow(image_buffer, image_buffer, {g, g, g, 1.0f});
  }

  /* Write to disk and close */
  image_buffer.set_write_format(TypeDesc::FLOAT);
  image_buffer.write(image_output.get());
  image_output->close();
}

CCL_NAMESPACE_END
