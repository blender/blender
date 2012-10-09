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

#include "libmv/simple_pipeline/detect.h"
#include <third_party/fast/fast.h>
#include <stdlib.h>
#include <memory.h>

#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace libmv {

typedef unsigned int uint;

static int featurecmp(const void *a_v, const void *b_v)
{
  Feature *a = (Feature*)a_v;
  Feature *b = (Feature*)b_v;

  return b->score - a->score;
}

std::vector<Feature> DetectFAST(const unsigned char* data, int width, int height, int stride,
                           int min_trackness, int min_distance) {
  std::vector<Feature> features;
  // TODO(MatthiasF): Support targetting a feature count (binary search trackness)
  int num_features;
  xy* all = fast9_detect(data, width, height,
                         stride, min_trackness, &num_features);
  if(num_features == 0) {
    free(all);
    return features;
  }
  int* scores = fast9_score(data, stride, all, num_features, min_trackness);
  // TODO: merge with close feature suppression
  xy* nonmax = nonmax_suppression(all, scores, num_features, &num_features);
  free(all);
  // Remove too close features
  // TODO(MatthiasF): A resolution independent parameter would be better than distance
  // e.g. a coefficient going from 0 (no minimal distance) to 1 (optimal circle packing)
  // FIXME(MatthiasF): this method will not necessarily give all maximum markers
  if(num_features) {
    Feature *all_features = new Feature[num_features];

    for(int i = 0; i < num_features; ++i) {
      Feature a = { (float)nonmax[i].x, (float)nonmax[i].y, (float)scores[i], 0 };
      all_features[i] = a;
    }

    qsort((void *)all_features, num_features, sizeof(Feature), featurecmp);

    features.reserve(num_features);

    int prev_score = all_features[0].score;
    for(int i = 0; i < num_features; ++i) {
      bool ok = true;
      Feature a = all_features[i];
      if(a.score>prev_score)
        abort();
      prev_score = a.score;

      // compare each feature against filtered set
      for(int j = 0; j < features.size(); j++) {
        Feature& b = features[j];
        if ( (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y) < min_distance*min_distance ) {
          // already a nearby feature
          ok = false;
          break;
        }
      }

      if(ok) {
        // add the new feature
        features.push_back(a);
      }
    }

    delete [] all_features;
  }
  free(scores);
  free(nonmax);
  return features;
}

#ifdef __SSE2__
static uint SAD(const ubyte* imageA, const ubyte* imageB, int strideA, int strideB) {
  __m128i a = _mm_setzero_si128();
  for(int i = 0; i < 16; i++) {
    a = _mm_adds_epu16(a, _mm_sad_epu8( _mm_loadu_si128((__m128i*)(imageA+i*strideA)),
                                        _mm_loadu_si128((__m128i*)(imageB+i*strideB))));
  }
  return _mm_extract_epi16(a,0) + _mm_extract_epi16(a,4);
}
#else
static uint SAD(const ubyte* imageA, const ubyte* imageB, int strideA, int strideB) {
  uint sad=0;
  for(int i = 0; i < 16; i++) {
    for(int j = 0; j < 16; j++) {
      sad += abs((int)imageA[i*strideA+j] - imageB[i*strideB+j]);
    }
  }
  return sad;
}
#endif

void DetectMORAVEC(ubyte* image, int stride, int width, int height, Feature* detected, int* count, int distance, ubyte* pattern) {
  unsigned short histogram[256];
  memset(histogram,0,sizeof(histogram));
  ubyte* scores = new ubyte[width*height];
  memset(scores,0,width*height);
  const int r = 1; //radius for self similarity comparison
  for(int y=distance; y<height-distance; y++) {
    for(int x=distance; x<width-distance; x++) {
      ubyte* s = &image[y*stride+x];
      int score = // low self-similarity with overlapping patterns //OPTI: load pattern once
          SAD(s, s-r*stride-r, stride, stride)+SAD(s, s-r*stride, stride, stride)+SAD(s, s-r*stride+r, stride, stride)+
          SAD(s, s         -r, stride, stride)+                                   SAD(s, s         +r, stride, stride)+
          SAD(s, s+r*stride-r, stride, stride)+SAD(s, s+r*stride, stride, stride)+SAD(s, s+r*stride+r, stride, stride);
      score /= 256; // normalize
      if(pattern) score -= SAD(s, pattern, stride, 16); // find only features similar to pattern
      if(score<=16) continue; // filter very self-similar features
      score -= 16; // translate to score/histogram values
      if(score>255) score=255; // clip
      ubyte* c = &scores[y*width+x];
      for(int i=-distance; i<0; i++) {
        for(int j=-distance; j<distance; j++) {
          int s = c[i*width+j];
          if(s == 0) continue;
          if(s >= score) goto nonmax;
          c[i*width+j]=0, histogram[s]--;
        }
      }
      for(int i=0, j=-distance; j<0; j++) {
        int s = c[i*width+j];
        if(s == 0) continue;
        if(s >= score) goto nonmax;
        c[i*width+j]=0, histogram[s]--;
      }
      c[0] = score, histogram[score]++;
      nonmax:;
    }
  }
  int min=255, total=0;
  for(; min>0; min--) {
    int h = histogram[min];
    if(total+h > *count) break;
    total += h;
  }
  int i=0;
  for(int y=16; y<height-16; y++) {
    for(int x=16; x<width-16; x++) {
      int s = scores[y*width+x];
      Feature f = { (float)x+8.0f, (float)y+8.0f, (float)s, 16 };
      if(s>min) detected[i++] = f;
    }
  }
  *count = i;
  delete[] scores;
}

}
