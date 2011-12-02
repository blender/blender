/****************************************************************************
**
** Copyright (c) 2011 libmv authors.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to
** deal in the Software without restriction, including without limitation the
** rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
** sell copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
** IN THE SOFTWARE.
**
****************************************************************************/

#include "libmv/tracking/sad.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

namespace libmv {

void LaplaceFilter(ubyte* src, ubyte* dst, int width, int height, int strength) {
  for(int y=1; y<height-1; y++) for(int x=1; x<width-1; x++) {
    const ubyte* s = &src[y*width+x];
    int l = 128 +
        s[-width-1] + s[-width] + s[-width+1] +
        s[1]        - 8*s[0]    + s[1]        +
        s[ width-1] + s[ width] + s[ width+1] ;
    int d = ((256-strength)*s[0] + strength*l) / 256;
    if(d < 0) d=0;
    if(d > 255) d=255;
    dst[y*width+x] = d;
  }
}

struct vec2 {
  float x,y;
  inline vec2(float x, float y):x(x),y(y){}
};
inline vec2 operator*(mat32 m, vec2 v) {
  return vec2(v.x*m(0,0)+v.y*m(0,1)+m(0,2),v.x*m(1,0)+v.y*m(1,1)+m(1,2));
}

//! fixed point bilinear sample with precision k
template <int k> inline int sample(const ubyte* image,int stride, int x, int y, int u, int v) {
  const ubyte* s = &image[y*stride+x];
  return ((s[     0] * (k-u) + s[       1] * u) * (k-v)
        + (s[stride] * (k-u) + s[stride+1] * u) * (  v) ) / (k*k);
}

#ifdef __SSE__
#include <xmmintrin.h>
int lround(float x) { return _mm_cvtss_si32(_mm_set_ss(x)); }
#elif defined(_MSC_VER)
int lround(float x) { return x+0.5; }
#endif

//TODO(MatthiasF): SSE optimization
void SamplePattern(ubyte* image, int stride, mat32 warp, ubyte* pattern, int size) {
  const int k = 256;
  for (int i = 0; i < size; i++) for (int j = 0; j < size; j++) {
    vec2 p = warp*vec2(j-size/2,i-size/2);
    int fx = lround(p.x*k), fy = lround(p.y*k);
    int ix = fx/k, iy = fy/k;
    int u = fx%k, v = fy%k;
    pattern[i*size+j] = sample<k>(image,stride,ix,iy,u,v);
  }
}

#ifdef __SSE2__
#include <emmintrin.h>
  static uint SAD(/*const*/ ubyte* pattern, /*const*/ ubyte* image, int stride, int size) {
  uint sad = 0;
  __m128i a = _mm_setzero_si128();

  for(int i = 0; i < size; i++) {
    int j = 0;

    for(j = 0; j < size/16; j++) {
      if((i*size/16+j) % 32 == 0) {
        sad += _mm_extract_epi16(a,0) + _mm_extract_epi16(a,4);
        a = _mm_setzero_si128();
      }

      a = _mm_adds_epu16(a, _mm_sad_epu8( _mm_loadu_si128((__m128i*)(pattern+i*size+j*16)),
                                          _mm_loadu_si128((__m128i*)(image+i*stride+j*16))));
    }

    for(j = j*16; j < size; j++) {
      sad += abs((int)pattern[i*size+j] - image[i*stride+j]);
    }
  }

  sad += _mm_extract_epi16(a,0) + _mm_extract_epi16(a,4);

  return sad;
}
#else
static uint SAD(const ubyte* pattern, const ubyte* image, int stride, int size) {
  uint sad=0;
  for(int i = 0; i < size; i++) {
    for(int j = 0; j < size; j++) {
      sad += abs((int)pattern[i*size+j] - image[i*stride+j]);
    }
  }
  return sad;
}
#endif

float sq(float x) { return x*x; }
float Track(ubyte* reference, ubyte* warped, int size, ubyte* image, int stride, int w, int h, mat32* warp, float areaPenalty, float conditionPenalty) {
  mat32 m=*warp;
  uint min=-1;

  // exhaustive search integer pixel translation
  int ix = m(0,2), iy = m(1,2);
  for(int y = size/2; y < h-size/2; y++) {
    for(int x = size/2; x < w-size/2; x++) {
      m(0,2) = x, m(1,2) = y;
      uint sad = SAD(warped,&image[(y-size/2)*stride+(x-size/2)],stride,size);
      // TODO: using chroma could help disambiguate some cases
      if(sad < min) {
        min = sad;
        ix = x, iy = y;
      }
    }
  }
  m(0,2) = ix, m(1,2) = iy;
  min=-1; //reset score since direct warped search match too well (but the wrong pattern).

  // 6D coordinate descent to find affine transform
  ubyte* match = new ubyte[size*size];
  float step = 0.5;
  for(int p = 0; p < 8; p++) { //foreach precision level
    for(int i = 0; i < 2; i++) { // iterate twice per precision level
      //TODO: other sweep pattern might converge better
      for(int d=0; d < 6; d++) { // iterate dimension sequentially (cyclic descent)
        for(float e = -step; e <= step; e+=step) { //solve subproblem (evaluate only along one coordinate)
          mat32 t = m;
          t.data[d] += e;
          //TODO: better performance would also allow a more exhaustive search
          SamplePattern(image,stride,t,match,size);
          uint sad = SAD(reference,match,size,size);
          // regularization: keep constant area and good condition
          float area = t(0,0)*t(1,1)-t(0,1)*t(1,0);
          float x = sq(t(0,0))+sq(t(0,1)), y = sq(t(1,0))+sq(t(1,1));
          float condition = x>y ? x/y : y/x;
          sad += size*size*( areaPenalty*sq(area-1) + conditionPenalty*sq(condition-1) );
          if(sad < min) {
            min = sad;
            m = t;
          }
        }
      }
    }
    step /= 2;
  }
  *warp = m;

  // Compute Pearson product-moment correlation coefficient
  uint sX=0,sY=0,sXX=0,sYY=0,sXY=0;
  SamplePattern(image,stride,m,match,size);
  SAD(reference,match,size,size);
  for(int i = 0; i < size; i++) {
    for(int j = 0; j < size; j++) {
      int x = reference[i*size+j];
      int y = match[i*size+j];
      sX += x;
      sY += y;
      sXX += x*x;
      sYY += y*y;
      sXY += x*y;
    }
  }
  delete[] match;
  const int N = size*size;
  sX /= N, sY /= N, sXX /= N, sYY /= N, sXY /= N;
  return (sXY-sX*sY)/sqrt(double((sXX-sX*sX)*(sYY-sY*sY)));
}

}  // namespace libmv
