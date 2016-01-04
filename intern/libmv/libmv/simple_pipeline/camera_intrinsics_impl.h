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

namespace libmv {

namespace {

// FIXME: C++ templates limitations makes thing complicated,
//        but maybe there is a simpler method.
struct ApplyIntrinsicsFunction {
  ApplyIntrinsicsFunction(const CameraIntrinsics &intrinsics,
                          double x,
                          double y,
                          double *warp_x,
                          double *warp_y) {
    double normalized_x, normalized_y;
    intrinsics.ImageSpaceToNormalized(x, y, &normalized_x, &normalized_y);
    intrinsics.ApplyIntrinsics(normalized_x, normalized_y, warp_x, warp_y);
  }
};

struct InvertIntrinsicsFunction {
  InvertIntrinsicsFunction(const CameraIntrinsics &intrinsics,
                           double x,
                           double y,
                           double *warp_x,
                           double *warp_y) {
    double normalized_x, normalized_y;
    intrinsics.InvertIntrinsics(x, y, &normalized_x, &normalized_y);
    intrinsics.NormalizedToImageSpace(normalized_x, normalized_y, warp_x, warp_y);
  }
};

}  // namespace

namespace internal {

// TODO(MatthiasF): downsample lookup
template<typename WarpFunction>
void LookupWarpGrid::Compute(const CameraIntrinsics &intrinsics,
                             int width,
                             int height,
                             double overscan) {
  double w = (double) width / (1.0 + overscan);
  double h = (double) height / (1.0 + overscan);
  double aspx = (double) w / intrinsics.image_width();
  double aspy = (double) h / intrinsics.image_height();
#if defined(_OPENMP)
#  pragma omp parallel for schedule(dynamic) num_threads(threads_) \
  if (threads_ > 1 && height > 100)
#endif
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      double src_x = (x - 0.5 * overscan * w) / aspx,
             src_y = (y - 0.5 * overscan * h) / aspy;
      double warp_x, warp_y;
      WarpFunction(intrinsics, src_x, src_y, &warp_x, &warp_y);
      warp_x = warp_x * aspx + 0.5 * overscan * w;
      warp_y = warp_y * aspy + 0.5 * overscan * h;
      int ix = int(warp_x), iy = int(warp_y);
      int fx = round((warp_x - ix) * 256), fy = round((warp_y - iy) * 256);
      if (fx == 256) { fx = 0; ix++; }  // NOLINT
      if (fy == 256) { fy = 0; iy++; }  // NOLINT
      // Use nearest border pixel
      if (ix < 0) { ix = 0, fx = 0; }  // NOLINT
      if (iy < 0) { iy = 0, fy = 0; }  // NOLINT
      if (ix >= width - 2) ix = width - 2;
      if (iy >= height - 2) iy = height - 2;

      Offset offset = { (short) (ix - x),
                        (short) (iy - y),
                        (unsigned char) fx,
                        (unsigned char) fy };
      offset_[y * width + x] = offset;
    }
  }
}

template<typename WarpFunction>
void LookupWarpGrid::Update(const CameraIntrinsics &intrinsics,
                            int width,
                            int height,
                            double overscan) {
  if (width_ != width ||
      height_ != height ||
      overscan_ != overscan) {
    Reset();
  }

  if (offset_ == NULL) {
    offset_ = new Offset[width * height];
    Compute<WarpFunction>(intrinsics,
                          width,
                          height,
                          overscan);
  }

  width_ = width;
  height_ = height;
  overscan_ = overscan;
}

// TODO(MatthiasF): cubic B-Spline image sampling, bilinear lookup
template<typename PixelType>
void LookupWarpGrid::Apply(const PixelType *input_buffer,
                           int width,
                           int height,
                           int channels,
                           PixelType *output_buffer) {
#if defined(_OPENMP)
#  pragma omp parallel for schedule(dynamic) num_threads(threads_) \
  if (threads_ > 1 && height > 100)
#endif
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      Offset offset = offset_[y * width + x];
      const int pixel_index = ((y + offset.iy) * width +
                               (x + offset.ix)) * channels;
      const PixelType *s = &input_buffer[pixel_index];
      for (int i = 0; i < channels; i++) {
        output_buffer[(y * width + x) * channels + i] =
          ((s[i] * (256 - offset.fx) +
            s[channels + i] * offset.fx) * (256 - offset.fy) +
           (s[width * channels + i] * (256 - offset.fx) +
            s[width * channels + channels + i] * offset.fx) * offset.fy)
          / (256 * 256);
      }
    }
  }
}

}  // namespace internal

template<typename PixelType>
void CameraIntrinsics::DistortBuffer(const PixelType *input_buffer,
                                     int width,
                                     int height,
                                     double overscan,
                                     int channels,
                                     PixelType *output_buffer) {
  assert(channels >= 1);
  assert(channels <= 4);
  distort_.Update<InvertIntrinsicsFunction>(*this,
                                            width,
                                            height,
                                            overscan);
  distort_.Apply<PixelType>(input_buffer,
                            width,
                            height,
                            channels,
                            output_buffer);
}

template<typename PixelType>
void CameraIntrinsics::UndistortBuffer(const PixelType *input_buffer,
                                       int width,
                                       int height,
                                       double overscan,
                                       int channels,
                                       PixelType *output_buffer) {
  assert(channels >= 1);
  assert(channels <= 4);
  undistort_.Update<ApplyIntrinsicsFunction>(*this,
                                             width,
                                             height,
                                             overscan);

  undistort_.Apply<PixelType>(input_buffer,
                              width,
                              height,
                              channels,
                              output_buffer);
}

}  // namespace libmv
