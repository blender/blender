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

struct Offset { char ix,iy; unsigned char fx,fy; };

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
      undistort_(0) {}

CameraIntrinsics::~CameraIntrinsics() {
  if(distort_) delete[] distort_;
  if(undistort_) delete[] undistort_;
}

void CameraIntrinsics::ApplyIntrinsics(double normalized_x,
                                       double normalized_y,
                                       double *image_x,
                                       double *image_y) const {
  double x = normalized_x;
  double y = normalized_y;

  // Apply distortion to the normalized points to get (xd, yd).
  double r2 = x*x + y*y;
  double r4 = r2 * r2;
  double r6 = r4 * r2;
  double r_coeff = (1 + k1_*r2 + k2_*r4 + k3_*r6);
  double xd = x * r_coeff + 2*p1_*x*y + p2_*(r2 + 2*x*x);
  double yd = y * r_coeff + 2*p2_*x*y + p1_*(r2 + 2*y*y);

  // Apply focal length and principal point to get the final image coordinates.
  *image_x = focal_length_x() * xd + principal_point_x();
  *image_y = focal_length_y() * yd + principal_point_y();
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
void CameraIntrinsics::ComputeLookupGrid(Offset* grid, int width, int height) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      double warp_x, warp_y;
      WarpFunction(this,x,y,&warp_x,&warp_y);
      int ix = int(warp_x), iy = int(warp_y);
      int fx = round((warp_x-ix)*256), fy = round((warp_y-iy)*256);
      if(fx == 256) { fx=0; ix++; }
      if(fy == 256) { fy=0; iy++; }
#ifdef CLIP
      // Use nearest border pixel
      if( ix < 0 ) { ix = 0, fx = 0; }
      if( iy < 0 ) { iy = 0, fy = 0; }
      if( ix >= width-1 ) { ix = width-1, fx = 0; }
      if( iy >= height-1 ) { iy = height-1, fy = 0; }
#else
      // No offset: Avoid adding out of bounds to error.
      if( ix < 0 || iy < 0 || ix >= width-1 || iy >= height-1 ) {
        ix = x; iy = y; fx = fy = 0;
      }
#endif
      //assert( ix-x > -128 && ix-x < 128 && iy-y > -128 && iy-y < 128 );
      Offset offset = { ix-x, iy-y, fx, fy };
      grid[y*width+x] = offset;
    }
  }
}

// TODO(MatthiasF): cubic B-Spline image sampling, bilinear lookup
template<typename T,int N>
static void Warp(const Offset* grid, const T* src, T* dst,
                 int width, int height) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      Offset offset = grid[y*width+x];
      const T* s = &src[((y+offset.iy)*width+(x+offset.ix))*N];
      for (int i = 0; i < N; i++) {
        dst[(y*width+x)*N+i] = ((s[        i] * (256-offset.fx) + s[        N+i] * offset.fx) * (256-offset.fy)
                               +(s[width*N+i] * (256-offset.fx) + s[width*N+N+i] * offset.fx) * offset.fy) / (256*256);
      }
    }
  }
}

// FIXME: C++ templates limitations makes thing complicated, but maybe there is a simpler method.
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
    intrinsics->InvertIntrinsics(x,y,warp_x,warp_y);
    *warp_x = *warp_x*intrinsics->focal_length_x()+intrinsics->principal_point_x();
    *warp_y = *warp_y*intrinsics->focal_length_y()+intrinsics->principal_point_y();
  }
};

void CameraIntrinsics::Distort(const float* src, float* dst, int width, int height, int channels) {
  if(!distort_) {
    distort_ = new Offset[width*height];
    ComputeLookupGrid<InvertIntrinsicsFunction>(distort_,width,height);
  }
       if(channels==1) Warp<float,1>(distort_,src,dst,width,height);
  else if(channels==2) Warp<float,2>(distort_,src,dst,width,height);
  else if(channels==3) Warp<float,3>(distort_,src,dst,width,height);
  else if(channels==4) Warp<float,4>(distort_,src,dst,width,height);
  //else assert("channels must be between 1 and 4");
}

void CameraIntrinsics::Distort(const unsigned char* src, unsigned char* dst, int width, int height, int channels) {
  if(!distort_) {
    distort_ = new Offset[width*height];
    ComputeLookupGrid<InvertIntrinsicsFunction>(distort_,width,height);
  }
       if(channels==1) Warp<unsigned char,1>(distort_,src,dst,width,height);
  else if(channels==2) Warp<unsigned char,2>(distort_,src,dst,width,height);
  else if(channels==3) Warp<unsigned char,3>(distort_,src,dst,width,height);
  else if(channels==4) Warp<unsigned char,4>(distort_,src,dst,width,height);
  //else assert("channels must be between 1 and 4");
}

void CameraIntrinsics::Undistort(const float* src, float* dst, int width, int height, int channels) {
  if(!undistort_) {
    undistort_ = new Offset[width*height];
    ComputeLookupGrid<ApplyIntrinsicsFunction>(undistort_,width,height);
  }
       if(channels==1) Warp<float,1>(undistort_,src,dst,width,height);
  else if(channels==2) Warp<float,2>(undistort_,src,dst,width,height);
  else if(channels==3) Warp<float,3>(undistort_,src,dst,width,height);
  else if(channels==4) Warp<float,4>(undistort_,src,dst,width,height);
  //else assert("channels must be between 1 and 4");
}

void CameraIntrinsics::Undistort(const unsigned char* src, unsigned char* dst, int width, int height, int channels) {
  if(!undistort_) {
    undistort_ = new Offset[width*height];
    ComputeLookupGrid<ApplyIntrinsicsFunction>(undistort_,width,height);
  }
       if(channels==1) Warp<unsigned char,1>(undistort_,src,dst,width,height);
  else if(channels==2) Warp<unsigned char,2>(undistort_,src,dst,width,height);
  else if(channels==3) Warp<unsigned char,3>(undistort_,src,dst,width,height);
  else if(channels==4) Warp<unsigned char,4>(undistort_,src,dst,width,height);
  //else assert("channels must be between 1 and 4");
}

}  // namespace libmv
