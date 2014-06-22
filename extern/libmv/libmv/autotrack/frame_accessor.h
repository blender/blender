// Copyright (c) 2014 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// Author: mierle@gmail.com (Keir Mierle)

#ifndef LIBMV_AUTOTRACK_FRAME_ACCESSOR_H_
#define LIBMV_AUTOTRACK_FRAME_ACCESSOR_H_

#include <stdint.h>

#include "libmv/image/image.h"

namespace mv {

struct Region;

using libmv::FloatImage;

// This is the abstraction to different sources of images that will be part of
// a reconstruction. These may come from disk or they may come from Blender. In
// most cases it's expected that the implementation provides some caching
// otherwise performance will be terrible. Sometimes the images need to get
// filtered, and this interface provides for that as well (and permits
// implementations to cache filtered image pieces).
struct FrameAccessor {
  struct Transform {
    // The key should depend on the transform arguments. Must be non-zero.
    virtual int64_t key() const = 0;

    // Apply the expected transform. Output is sized correctly already.
    // TODO(keir): What about blurs that need to access pixels outside the ROI?
    virtual void run(const FloatImage& input, FloatImage* output) const = 0;
  };

  enum InputMode {
    MONO,
    RGBA
  };

  typedef void* Key;

  // Get a possibly-filtered version of a frame of a video. Downscale will
  // cause the input image to get downscaled by 2^downscale for pyramid access.
  // Region is always in original-image coordinates, and describes the
  // requested area. The transform describes an (optional) transform to apply
  // to the image before it is returned.
  //
  // When done with an image, you must call ReleaseImage with the returned key.
  virtual Key GetImage(int clip,
                       int frame,
                       InputMode input_mode,
                       int downscale,               // Downscale by 2^downscale.
                       const Region* region,        // Get full image if NULL.
                       const Transform* transform,  // May be NULL.
                       FloatImage* destination) = 0;

  // Releases an image from the frame accessor. Non-caching implementations may
  // free the image immediately; others may hold onto the image.
  virtual void ReleaseImage(Key) = 0;

  virtual bool GetClipDimensions(int clip, int* width, int* height) = 0;
  virtual int NumClips() = 0;
  virtual int NumFrames(int clip) = 0;
};

}  // namespace libmv

#endif  // LIBMV_AUTOTRACK_FRAME_ACCESSOR_H_
