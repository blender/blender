/* SPDX-FileCopyrightText: 2014 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "intern/frame_accessor.h"
#include "intern/image.h"
#include "intern/utildefines.h"
#include "libmv/autotrack/frame_accessor.h"
#include "libmv/autotrack/region.h"
#include "libmv/image/image.h"

namespace {

using libmv::FloatImage;
using mv::FrameAccessor;
using mv::Region;

struct LibmvFrameAccessor : public FrameAccessor {
  LibmvFrameAccessor(libmv_FrameAccessorUserData* user_data,
                     libmv_GetImageCallback get_image_callback,
                     libmv_ReleaseImageCallback release_image_callback,
                     libmv_GetMaskForTrackCallback get_mask_for_track_callback,
                     libmv_ReleaseMaskCallback release_mask_callback)
      : user_data_(user_data),
        get_image_callback_(get_image_callback),
        release_image_callback_(release_image_callback),
        get_mask_for_track_callback_(get_mask_for_track_callback),
        release_mask_callback_(release_mask_callback) {}

  virtual ~LibmvFrameAccessor() {}

  libmv_InputMode get_libmv_input_mode(InputMode input_mode) {
    switch (input_mode) {
#define CHECK_INPUT_MODE(mode)                                                 \
  case mode: return LIBMV_IMAGE_MODE_##mode;
      CHECK_INPUT_MODE(MONO)
      CHECK_INPUT_MODE(RGBA)
#undef CHECK_INPUT_MODE
    }
    assert(!"unknown input mode passed from Libmv.");
    // TODO(sergey): Proper error handling here in the future.
    return LIBMV_IMAGE_MODE_MONO;
  }

  void get_libmv_region(const Region& region, libmv_Region* libmv_region) {
    libmv_region->min[0] = region.min(0);
    libmv_region->min[1] = region.min(1);
    libmv_region->max[0] = region.max(0);
    libmv_region->max[1] = region.max(1);
  }

  Key GetImage(int clip,
               int frame,
               InputMode input_mode,
               int downscale,
               const Region* region,
               const Transform* transform,
               FloatImage* destination) {
    float* float_buffer;
    int width, height, channels;
    libmv_Region libmv_region;
    if (region) {
      get_libmv_region(*region, &libmv_region);
    }
    Key cache_key = get_image_callback_(user_data_,
                                        clip,
                                        frame,
                                        get_libmv_input_mode(input_mode),
                                        downscale,
                                        region != NULL ? &libmv_region : NULL,
                                        (libmv_FrameTransform*)transform,
                                        &float_buffer,
                                        &width,
                                        &height,
                                        &channels);

    // TODO(sergey): Dumb code for until we can set data directly.
    FloatImage temp_image(float_buffer, height, width, channels);
    destination->CopyFrom(temp_image);

    return cache_key;
  }

  void ReleaseImage(Key cache_key) { release_image_callback_(cache_key); }

  Key GetMaskForTrack(int clip,
                      int frame,
                      int track,
                      const Region* region,
                      FloatImage* destination) {
    float* float_buffer;
    int width, height;
    libmv_Region libmv_region;
    if (region) {
      get_libmv_region(*region, &libmv_region);
    }
    Key cache_key =
        get_mask_for_track_callback_(user_data_,
                                     clip,
                                     frame,
                                     track,
                                     region != NULL ? &libmv_region : NULL,
                                     &float_buffer,
                                     &width,
                                     &height);

    if (cache_key == NULL) {
      // No mask for the given track.
      return NULL;
    }

    // TODO(sergey): Dumb code for until we can set data directly.
    FloatImage temp_image(float_buffer, height, width, 1);
    destination->CopyFrom(temp_image);

    return cache_key;
  }

  void ReleaseMask(Key key) { release_mask_callback_(key); }

  bool GetClipDimensions(int /*clip*/, int* /*width*/, int* /*height*/) {
    return false;
  }

  int NumClips() { return 1; }

  int NumFrames(int /*clip*/) { return 0; }

  libmv_FrameAccessorUserData* user_data_;
  libmv_GetImageCallback get_image_callback_;
  libmv_ReleaseImageCallback release_image_callback_;
  libmv_GetMaskForTrackCallback get_mask_for_track_callback_;
  libmv_ReleaseMaskCallback release_mask_callback_;
};

}  // namespace

libmv_FrameAccessor* libmv_FrameAccessorNew(
    libmv_FrameAccessorUserData* user_data,
    libmv_GetImageCallback get_image_callback,
    libmv_ReleaseImageCallback release_image_callback,
    libmv_GetMaskForTrackCallback get_mask_for_track_callback,
    libmv_ReleaseMaskCallback release_mask_callback) {
  return (libmv_FrameAccessor*)LIBMV_OBJECT_NEW(LibmvFrameAccessor,
                                                user_data,
                                                get_image_callback,
                                                release_image_callback,
                                                get_mask_for_track_callback,
                                                release_mask_callback);
}

void libmv_FrameAccessorDestroy(libmv_FrameAccessor* frame_accessor) {
  LIBMV_OBJECT_DELETE(frame_accessor, LibmvFrameAccessor);
}

int64_t libmv_frameAccessorgetTransformKey(
    const libmv_FrameTransform* transform) {
  return ((FrameAccessor::Transform*)transform)->key();
}

void libmv_frameAccessorgetTransformRun(const libmv_FrameTransform* transform,
                                        const libmv_FloatImage* input_image,
                                        libmv_FloatImage* output_image) {
  const FloatImage input(input_image->buffer,
                         input_image->height,
                         input_image->width,
                         input_image->channels);

  FloatImage output;
  ((FrameAccessor::Transform*)transform)->run(input, &output);

  int num_pixels = output.Width() * output.Height() * output.Depth();
  output_image->buffer = new float[num_pixels];
  memcpy(output_image->buffer, output.Data(), num_pixels * sizeof(float));
  output_image->width = output.Width();
  output_image->height = output.Height();
  output_image->channels = output.Depth();
}
