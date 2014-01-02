// Copyright (c) 2011 libmv authors.
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

#include "libmv/simple_pipeline/camera_intrinsics.h"
#include "libmv/numeric/levenberg_marquardt.h"

namespace libmv {

struct Offset {
  short ix, iy;
  unsigned char fx, fy;
};

struct Grid {
  struct Offset *offset;
  int width, height;
  double overscan;
};

static struct Grid *copyGrid(struct Grid *from) {
  struct Grid *to = NULL;

  if (from) {
    to = new Grid;

    to->width = from->width;
    to->height = from->height;
    to->overscan = from->overscan;

    to->offset = new Offset[to->width*to->height];
    memcpy(to->offset, from->offset, sizeof(struct Offset)*to->width*to->height);
  }

  return to;
}

CameraIntrinsics::CameraIntrinsics()
    : K_(Mat3::Identity()),
      image_width_(0),
      image_height_(0),
      k1_(0),
      k2_(0),
      k3_(0),
      p1_(0),
      p2_(0),
      distort_(0),
      undistort_(0),
      threads_(1) {}

CameraIntrinsics::CameraIntrinsics(const CameraIntrinsics &from)
    : K_(from.K_),
      image_width_(from.image_width_),
      image_height_(from.image_height_),
      k1_(from.k1_),
      k2_(from.k2_),
      k3_(from.k3_),
      p1_(from.p1_),
      p2_(from.p2_),
      threads_(from.threads_) {
  distort_ = copyGrid(from.distort_);
  undistort_ = copyGrid(from.undistort_);
}

CameraIntrinsics::~CameraIntrinsics() {
  FreeLookupGrid();
}

/// Set the entire calibration matrix at once.
void CameraIntrinsics::SetK(const Mat3 new_k) {
  K_ = new_k;
  FreeLookupGrid();
}

/// Set both x and y focal length in pixels.
void CameraIntrinsics::SetFocalLength(double focal_x, double focal_y) {
  K_(0, 0) = focal_x;
  K_(1, 1) = focal_y;
  FreeLookupGrid();
}

void CameraIntrinsics::SetPrincipalPoint(double cx, double cy) {
  K_(0, 2) = cx;
  K_(1, 2) = cy;
  FreeLookupGrid();
}

void CameraIntrinsics::SetImageSize(int width, int height) {
  image_width_ = width;
  image_height_ = height;
  FreeLookupGrid();
}

void CameraIntrinsics::SetRadialDistortion(double k1, double k2, double k3) {
  k1_ = k1;
  k2_ = k2;
  k3_ = k3;
  FreeLookupGrid();
}

void CameraIntrinsics::SetTangentialDistortion(double p1, double p2) {
  p1_ = p1;
  p2_ = p2;
  FreeLookupGrid();
}

void CameraIntrinsics::SetThreads(int threads) {
  threads_ = threads;
}

void CameraIntrinsics::ApplyIntrinsics(double normalized_x,
                                       double normalized_y,
                                       double *image_x,
                                       double *image_y) const {
  ApplyRadialDistortionCameraIntrinsics(focal_length_x(),
                                        focal_length_y(),
                                        principal_point_x(),
                                        principal_point_y(),
                                        k1(), k2(), k3(),
                                        p1(), p2(),
                                        normalized_x,
                                        normalized_y,
                                        image_x,
                                        image_y);
}

struct InvertIntrinsicsCostFunction {
 public:
  typedef Vec2 FMatrixType;
  typedef Vec2 XMatrixType;

  InvertIntrinsicsCostFunction(const CameraIntrinsics &intrinsics,
                               double image_x, double image_y)
    : intrinsics(intrinsics), x(image_x), y(image_y) {}

  Vec2 operator()(const Vec2 &u) const {
    double xx, yy;
    intrinsics.ApplyIntrinsics(u(0), u(1), &xx, &yy);
    Vec2 fx;
    fx << (xx - x), (yy - y);
    return fx;
  }
  const CameraIntrinsics &intrinsics;
  double x, y;
};

void CameraIntrinsics::InvertIntrinsics(double image_x,
                                        double image_y,
                                        double *normalized_x,
                                        double *normalized_y) const {
  // Compute the initial guess. For a camera with no distortion, this will also
  // be the final answer; the LM iteration will terminate immediately.
  Vec2 normalized;
  normalized(0) = (image_x - principal_point_x()) / focal_length_x();
  normalized(1) = (image_y - principal_point_y()) / focal_length_y();

  typedef LevenbergMarquardt<InvertIntrinsicsCostFunction> Solver;

  InvertIntrinsicsCostFunction intrinsics_cost(*this, image_x, image_y);
  Solver::SolverParameters params;
  Solver solver(intrinsics_cost);

  /*Solver::Results results =*/ solver.minimize(params, &normalized);

  // TODO(keir): Better error handling.

  *normalized_x = normalized(0);
  *normalized_y = normalized(1);
}

// TODO(MatthiasF): downsample lookup
template<typename WarpFunction>
void CameraIntrinsics::ComputeLookupGrid(Grid* grid, int width, int height,
                                         double overscan) {
  double w = (double)width / (1 + overscan);
  double h = (double)height / (1 + overscan);
  double aspx = (double)w / image_width_;
  double aspy = (double)h / image_height_;
#if defined(_OPENMP)
#  pragma omp parallel for schedule(dynamic) num_threads(threads_) \
  if (threads_ > 1 && height > 100)
#endif
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      double src_x = (x - 0.5 * overscan * w) / aspx,
             src_y = (y - 0.5 * overscan * h) / aspy;
      double warp_x, warp_y;
      WarpFunction(this, src_x, src_y, &warp_x, &warp_y);
      warp_x = warp_x*aspx + 0.5 * overscan * w;
      warp_y = warp_y*aspy + 0.5 * overscan * h;
      int ix = int(warp_x), iy = int(warp_y);
      int fx = round((warp_x-ix)*256), fy = round((warp_y-iy)*256);
      if (fx == 256) { fx = 0; ix++; }  // NOLINT
      if (fy == 256) { fy = 0; iy++; }  // NOLINT
      // Use nearest border pixel
      if (ix < 0) { ix = 0, fx = 0; }  // NOLINT
      if (iy < 0) { iy = 0, fy = 0; }  // NOLINT
      if (ix >= width - 2) ix = width-2;
      if (iy >= height - 2) iy = height-2;

      Offset offset = { (short)(ix-x), (short)(iy-y),
                        (unsigned char)fx, (unsigned char)fy };
      grid->offset[y*width+x] = offset;
    }
  }
}

// TODO(MatthiasF): cubic B-Spline image sampling, bilinear lookup
template<typename T>
static void Warp(const Grid* grid, const T* src, T* dst,
                 const int width, const int height, const int channels,
                 const int threads) {
  (void) threads;  // Ignored if OpenMP is disabled
#if defined(_OPENMP)
#  pragma omp parallel for schedule(dynamic) num_threads(threads) \
  if (threads > 1 && height > 100)
#endif
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      Offset offset = grid->offset[y*width+x];
      const T* s = &src[((y + offset.iy) * width + (x + offset.ix)) * channels];
      for (int i = 0; i < channels; i++) {
        // TODO(sergey): Finally wrap this into ultiple lines nicely.
        dst[(y*width+x)*channels+i] =
          ((s[               i] * (256-offset.fx) + s[               channels+i] * offset.fx) * (256-offset.fy)         // NOLINT
          +(s[width*channels+i] * (256-offset.fx) + s[width*channels+channels+i] * offset.fx) * offset.fy) / (256*256); // NOLINT
      }
    }
  }
}

void CameraIntrinsics::FreeLookupGrid() {
  if (distort_) {
    delete distort_->offset;
    delete distort_;
    distort_ = NULL;
  }

  if (undistort_) {
    delete undistort_->offset;
    delete undistort_;
    undistort_ = NULL;
  }
}

// FIXME: C++ templates limitations makes thing complicated,
//        but maybe there is a simpler method.
struct ApplyIntrinsicsFunction {
  ApplyIntrinsicsFunction(CameraIntrinsics* intrinsics, double x, double y,
                           double *warp_x, double *warp_y) {
    intrinsics->ApplyIntrinsics(
          (x-intrinsics->principal_point_x())/intrinsics->focal_length_x(),
          (y-intrinsics->principal_point_y())/intrinsics->focal_length_y(),
          warp_x, warp_y);
  }
};
struct InvertIntrinsicsFunction {
  InvertIntrinsicsFunction(CameraIntrinsics* intrinsics, double x, double y,
                           double *warp_x, double *warp_y) {
    intrinsics->InvertIntrinsics(x, y, warp_x, warp_y);

    *warp_x = *warp_x * intrinsics->focal_length_x() +
              intrinsics->principal_point_x();

    *warp_y = *warp_y * intrinsics->focal_length_y() +
              intrinsics->principal_point_y();
  }
};

void CameraIntrinsics::CheckDistortLookupGrid(int width, int height,
                                              double overscan) {
  if (distort_) {
    if (distort_->width != width || distort_->height != height ||
        distort_->overscan != overscan) {
      delete [] distort_->offset;
      distort_->offset = NULL;
    }
  } else {
    distort_ = new Grid;
    distort_->offset = NULL;
  }

  if (!distort_->offset) {
      distort_->offset = new Offset[width * height];
      ComputeLookupGrid<InvertIntrinsicsFunction>(distort_, width,
                                                  height, overscan);
  }

  distort_->width = width;
  distort_->height = height;
  distort_->overscan = overscan;
}

void CameraIntrinsics::CheckUndistortLookupGrid(int width, int height,
                                                double overscan) {
  if (undistort_) {
    if (undistort_->width != width || undistort_->height != height ||
        undistort_->overscan != overscan) {
      delete [] undistort_->offset;
      undistort_->offset = NULL;
    }
  } else {
    undistort_ = new Grid;
    undistort_->offset = NULL;
  }

  if (!undistort_->offset) {
      undistort_->offset = new Offset[width * height];
      ComputeLookupGrid<ApplyIntrinsicsFunction>(undistort_, width,
                                                 height, overscan);
  }

  undistort_->width = width;
  undistort_->height = height;
  undistort_->overscan = overscan;
}

void CameraIntrinsics::Distort(const float* src, float* dst,
                               int width, int height,
                               double overscan,
                               int channels) {
  assert(channels >= 1);
  assert(channels <= 4);
  CheckDistortLookupGrid(width, height, overscan);
  Warp<float>(distort_, src, dst, width, height, channels, threads_);
}

void CameraIntrinsics::Distort(const unsigned char* src,
                               unsigned char* dst,
                               int width, int height,
                               double overscan,
                               int channels) {
  assert(channels >= 1);
  assert(channels <= 4);
  CheckDistortLookupGrid(width, height, overscan);
  Warp<unsigned char>(distort_, src, dst, width, height, channels, threads_);
}

void CameraIntrinsics::Undistort(const float* src, float* dst,
                                 int width, int height,
                                 double overscan,
                                 int channels) {
  assert(channels >= 1);
  assert(channels <= 4);
  CheckUndistortLookupGrid(width, height, overscan);
  Warp<float>(undistort_, src, dst, width, height, channels, threads_);
}

void CameraIntrinsics::Undistort(const unsigned char* src,
                                 unsigned char* dst,
                                 int width, int height,
                                 double overscan,
                                 int channels) {
  assert(channels >= 1);
  assert(channels <= 4);
  CheckUndistortLookupGrid(width, height, overscan);
  Warp<unsigned char>(undistort_, src, dst, width, height, channels, threads_);
}

std::ostream& operator <<(std::ostream &os,
                          const CameraIntrinsics &intrinsics) {
  if (intrinsics.focal_length_x() == intrinsics.focal_length_x()) {
    os << "f=" << intrinsics.focal_length();
  } else {
    os <<  "fx=" << intrinsics.focal_length_x()
       << " fy=" << intrinsics.focal_length_y();
  }
  os << " cx=" << intrinsics.principal_point_x()
     << " cy=" << intrinsics.principal_point_y()
     << " w=" << intrinsics.image_width()
     << " h=" << intrinsics.image_height();

  if (intrinsics.k1() != 0.0) { os << " k1=" << intrinsics.k1(); }
  if (intrinsics.k2() != 0.0) { os << " k2=" << intrinsics.k2(); }
  if (intrinsics.k3() != 0.0) { os << " k3=" << intrinsics.k3(); }
  if (intrinsics.p1() != 0.0) { os << " p1=" << intrinsics.p1(); }
  if (intrinsics.p2() != 0.0) { os << " p2=" << intrinsics.p2(); }

  return os;
}

}  // namespace libmv
