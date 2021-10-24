/*
 * Copyright 2019 Blender Foundation
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

/* This file is based on "Progressive Multi-Jittered Sample Sequences"
 * by Per Christensen, Andrew Kensler and Charlie Kilpatrick.
 * http://graphics.pixar.com/library/ProgressiveMultiJitteredSampling/paper.pdf
 *
 * Performance can be improved in the future by implementing the new
 * algorithm from Matt Pharr in  http://jcgt.org/published/0008/01/04/
 * "Efficient Generation of Points that Satisfy Two-Dimensional Elementary Intervals"
 */

#include "scene/jitter.h"

#include <math.h>
#include <vector>

CCL_NAMESPACE_BEGIN

static uint cmj_hash(uint i, uint p)
{
  i ^= p;
  i ^= i >> 17;
  i ^= i >> 10;
  i *= 0xb36534e5;
  i ^= i >> 12;
  i ^= i >> 21;
  i *= 0x93fc4795;
  i ^= 0xdf6e307f;
  i ^= i >> 17;
  i *= 1 | p >> 18;

  return i;
}

static float cmj_randfloat(uint i, uint p)
{
  return cmj_hash(i, p) * (1.0f / 4294967808.0f);
}

class PMJ_Generator {
 public:
  static void generate_2D(float2 points[], int size, int rng_seed_in)
  {
    PMJ_Generator g(rng_seed_in);
    points[0].x = g.rnd();
    points[0].y = g.rnd();
    int N = 1;
    while (N < size) {
      g.extend_sequence_even(points, N);
      g.extend_sequence_odd(points, 2 * N);
      N = 4 * N;
    }
  }

 protected:
  PMJ_Generator(int rnd_seed_in) : num_samples(1), rnd_index(2), rnd_seed(rnd_seed_in)
  {
  }

  float rnd()
  {
    return cmj_randfloat(++rnd_index, rnd_seed);
  }

  virtual void mark_occupied_strata(float2 points[], int N)
  {
    int NN = 2 * N;
    for (int s = 0; s < NN; ++s) {
      occupied1Dx[s] = occupied1Dy[s] = false;
    }
    for (int s = 0; s < N; ++s) {
      int xstratum = (int)(NN * points[s].x);
      int ystratum = (int)(NN * points[s].y);
      occupied1Dx[xstratum] = true;
      occupied1Dy[ystratum] = true;
    }
  }

  virtual void generate_sample_point(
      float2 points[], float i, float j, float xhalf, float yhalf, int n, int N)
  {
    int NN = 2 * N;
    float2 pt;
    int xstratum, ystratum;
    do {
      pt.x = (i + 0.5f * (xhalf + rnd())) / n;
      xstratum = (int)(NN * pt.x);
    } while (occupied1Dx[xstratum]);
    do {
      pt.y = (j + 0.5f * (yhalf + rnd())) / n;
      ystratum = (int)(NN * pt.y);
    } while (occupied1Dy[ystratum]);
    occupied1Dx[xstratum] = true;
    occupied1Dy[ystratum] = true;
    points[num_samples] = pt;
    ++num_samples;
  }

  void extend_sequence_even(float2 points[], int N)
  {
    int n = (int)sqrtf(N);
    occupied1Dx.resize(2 * N);
    occupied1Dy.resize(2 * N);
    mark_occupied_strata(points, N);
    for (int s = 0; s < N; ++s) {
      float2 oldpt = points[s];
      float i = floorf(n * oldpt.x);
      float j = floorf(n * oldpt.y);
      float xhalf = floorf(2.0f * (n * oldpt.x - i));
      float yhalf = floorf(2.0f * (n * oldpt.y - j));
      xhalf = 1.0f - xhalf;
      yhalf = 1.0f - yhalf;
      generate_sample_point(points, i, j, xhalf, yhalf, n, N);
    }
  }

  void extend_sequence_odd(float2 points[], int N)
  {
    int n = (int)sqrtf(N / 2);
    occupied1Dx.resize(2 * N);
    occupied1Dy.resize(2 * N);
    mark_occupied_strata(points, N);
    std::vector<float> xhalves(N / 2);
    std::vector<float> yhalves(N / 2);
    for (int s = 0; s < N / 2; ++s) {
      float2 oldpt = points[s];
      float i = floorf(n * oldpt.x);
      float j = floorf(n * oldpt.y);
      float xhalf = floorf(2.0f * (n * oldpt.x - i));
      float yhalf = floorf(2.0f * (n * oldpt.y - j));
      if (rnd() > 0.5f) {
        xhalf = 1.0f - xhalf;
      }
      else {
        yhalf = 1.0f - yhalf;
      }
      xhalves[s] = xhalf;
      yhalves[s] = yhalf;
      generate_sample_point(points, i, j, xhalf, yhalf, n, N);
    }
    for (int s = 0; s < N / 2; ++s) {
      float2 oldpt = points[s];
      float i = floorf(n * oldpt.x);
      float j = floorf(n * oldpt.y);
      float xhalf = 1.0f - xhalves[s];
      float yhalf = 1.0f - yhalves[s];
      generate_sample_point(points, i, j, xhalf, yhalf, n, N);
    }
  }

  std::vector<bool> occupied1Dx, occupied1Dy;
  int num_samples;
  int rnd_index, rnd_seed;
};

class PMJ02_Generator : public PMJ_Generator {
 protected:
  void generate_sample_point(
      float2 points[], float i, float j, float xhalf, float yhalf, int n, int N) override
  {
    int NN = 2 * N;
    float2 pt;
    do {
      pt.x = (i + 0.5f * (xhalf + rnd())) / n;
      pt.y = (j + 0.5f * (yhalf + rnd())) / n;
    } while (is_occupied(pt, NN));
    mark_occupied_strata1(pt, NN);
    points[num_samples] = pt;
    ++num_samples;
  }

  void mark_occupied_strata(float2 points[], int N) override
  {
    int NN = 2 * N;
    int num_shapes = (int)log2f(NN) + 1;
    occupiedStrata.resize(num_shapes);
    for (int shape = 0; shape < num_shapes; ++shape) {
      occupiedStrata[shape].resize(NN);
      for (int n = 0; n < NN; ++n) {
        occupiedStrata[shape][n] = false;
      }
    }
    for (int s = 0; s < N; ++s) {
      mark_occupied_strata1(points[s], NN);
    }
  }

  void mark_occupied_strata1(float2 pt, int NN)
  {
    int shape = 0;
    int xdivs = NN;
    int ydivs = 1;
    do {
      int xstratum = (int)(xdivs * pt.x);
      int ystratum = (int)(ydivs * pt.y);
      size_t index = ystratum * xdivs + xstratum;
      assert(index < NN);
      occupiedStrata[shape][index] = true;
      shape = shape + 1;
      xdivs = xdivs / 2;
      ydivs = ydivs * 2;
    } while (xdivs > 0);
  }

  bool is_occupied(float2 pt, int NN)
  {
    int shape = 0;
    int xdivs = NN;
    int ydivs = 1;
    do {
      int xstratum = (int)(xdivs * pt.x);
      int ystratum = (int)(ydivs * pt.y);
      size_t index = ystratum * xdivs + xstratum;
      assert(index < NN);
      if (occupiedStrata[shape][index]) {
        return true;
      }
      shape = shape + 1;
      xdivs = xdivs / 2;
      ydivs = ydivs * 2;
    } while (xdivs > 0);
    return false;
  }

 private:
  std::vector<std::vector<bool>> occupiedStrata;
};

static void shuffle(float2 points[], int size, int rng_seed)
{
  if (rng_seed == 0) {
    return;
  }

  constexpr int odd[8] = {0, 1, 4, 5, 10, 11, 14, 15};
  constexpr int even[8] = {2, 3, 6, 7, 8, 9, 12, 13};

  int rng_index = 0;
  for (int yy = 0; yy < size / 16; ++yy) {
    for (int xx = 0; xx < 8; ++xx) {
      int other = (int)(cmj_randfloat(++rng_index, rng_seed) * (8.0f - xx) + xx);
      float2 tmp = points[odd[other] + yy * 16];
      points[odd[other] + yy * 16] = points[odd[xx] + yy * 16];
      points[odd[xx] + yy * 16] = tmp;
    }
    for (int xx = 0; xx < 8; ++xx) {
      int other = (int)(cmj_randfloat(++rng_index, rng_seed) * (8.0f - xx) + xx);
      float2 tmp = points[even[other] + yy * 16];
      points[even[other] + yy * 16] = points[even[xx] + yy * 16];
      points[even[xx] + yy * 16] = tmp;
    }
  }
}

void progressive_multi_jitter_generate_2D(float2 points[], int size, int rng_seed)
{
  PMJ_Generator::generate_2D(points, size, rng_seed);
  shuffle(points, size, rng_seed);
}

void progressive_multi_jitter_02_generate_2D(float2 points[], int size, int rng_seed)
{
  PMJ02_Generator::generate_2D(points, size, rng_seed);
  shuffle(points, size, rng_seed);
}

CCL_NAMESPACE_END
