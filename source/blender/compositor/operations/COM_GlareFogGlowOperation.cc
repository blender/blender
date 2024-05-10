/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_vector.h"

#include "COM_GlareFogGlowOperation.h"

namespace blender::compositor {

/*
 *  2D Fast Hartley Transform, used for convolution
 */

using fREAL = float;

/* Returns next highest power of 2 of x, as well its log2 in L2. */
static uint next_pow2(uint x, uint *L2)
{
  uint pw, x_notpow2 = x & (x - 1);
  *L2 = 0;
  while (x >>= 1) {
    ++(*L2);
  }
  pw = 1 << (*L2);
  if (x_notpow2) {
    (*L2)++;
    pw <<= 1;
  }
  return pw;
}

//------------------------------------------------------------------------------

/* From FXT library by Joerg Arndt, faster in order bit-reversal
 * use: `r = revbin_upd(r, h)` where `h = N>>1`. */
static uint revbin_upd(uint r, uint h)
{
  while (!((r ^= h) & h)) {
    h >>= 1;
  }
  return r;
}
//------------------------------------------------------------------------------
static void FHT(fREAL *data, uint M, uint inverse)
{
  double tt, fc, dc, fs, ds, a = M_PI;
  fREAL t1, t2;
  int n2, bd, bl, istep, k, len = 1 << M, n = 1;

  int i, j = 0;
  uint Nh = len >> 1;
  for (i = 1; i < (len - 1); i++) {
    j = revbin_upd(j, Nh);
    if (j > i) {
      t1 = data[i];
      data[i] = data[j];
      data[j] = t1;
    }
  }

  do {
    fREAL *data_n = &data[n];

    istep = n << 1;
    for (k = 0; k < len; k += istep) {
      t1 = data_n[k];
      data_n[k] = data[k] - t1;
      data[k] += t1;
    }

    n2 = n >> 1;
    if (n > 2) {
      fc = dc = cos(a);
      fs = ds = sqrt(1.0 - fc * fc);  // sin(a);
      bd = n - 2;
      for (bl = 1; bl < n2; bl++) {
        fREAL *data_nbd = &data_n[bd];
        fREAL *data_bd = &data[bd];
        for (k = bl; k < len; k += istep) {
          t1 = fc * double(data_n[k]) + fs * double(data_nbd[k]);
          t2 = fs * double(data_n[k]) - fc * double(data_nbd[k]);
          data_n[k] = data[k] - t1;
          data_nbd[k] = data_bd[k] - t2;
          data[k] += t1;
          data_bd[k] += t2;
        }
        tt = fc * dc - fs * ds;
        fs = fs * dc + fc * ds;
        fc = tt;
        bd -= 2;
      }
    }

    if (n > 1) {
      for (k = n2; k < len; k += istep) {
        t1 = data_n[k];
        data_n[k] = data[k] - t1;
        data[k] += t1;
      }
    }

    n = istep;
    a *= 0.5;
  } while (n < len);

  if (inverse) {
    fREAL sc = (fREAL)1 / (fREAL)len;
    for (k = 0; k < len; k++) {
      data[k] *= sc;
    }
  }
}
//------------------------------------------------------------------------------
/* 2D Fast Hartley Transform, Mx/My -> log2 of width/height,
 * nzp -> the row where zero pad data starts,
 * inverse -> see above. */
static void FHT2D(fREAL *data, uint Mx, uint My, uint nzp, uint inverse)
{
  uint i, j, Nx, Ny, maxy;

  Nx = 1 << Mx;
  Ny = 1 << My;

  /* Rows (forward transform skips 0 pad data). */
  maxy = inverse ? Ny : nzp;
  for (j = 0; j < maxy; j++) {
    FHT(&data[Nx * j], Mx, inverse);
  }

  /* Transpose data. */
  if (Nx == Ny) { /* Square. */
    for (j = 0; j < Ny; j++) {
      for (i = j + 1; i < Nx; i++) {
        uint op = i + (j << Mx), np = j + (i << My);
        std::swap(data[op], data[np]);
      }
    }
  }
  else { /* Rectangular. */
    uint k, Nym = Ny - 1, stm = 1 << (Mx + My);
    for (i = 0; stm > 0; i++) {
#define PRED(k) (((k & Nym) << Mx) + (k >> My))
      for (j = PRED(i); j > i; j = PRED(j)) {
        /* Pass. */
      }
      if (j < i) {
        continue;
      }
      for (k = i, j = PRED(i); j != i; k = j, j = PRED(j), stm--) {
        std::swap(data[j], data[k]);
      }
#undef PRED
      stm--;
    }
  }

  std::swap(Nx, Ny);
  std::swap(Mx, My);

  /* Now columns == transposed rows. */
  for (j = 0; j < Ny; j++) {
    FHT(&data[Nx * j], Mx, inverse);
  }

  /* Finalize. */
  for (j = 0; j <= (Ny >> 1); j++) {
    uint jm = (Ny - j) & (Ny - 1);
    uint ji = j << Mx;
    uint jmi = jm << Mx;
    for (i = 0; i <= (Nx >> 1); i++) {
      uint im = (Nx - i) & (Nx - 1);
      fREAL A = data[ji + i];
      fREAL B = data[jmi + i];
      fREAL C = data[ji + im];
      fREAL D = data[jmi + im];
      fREAL E = (fREAL)0.5 * ((A + D) - (B + C));
      data[ji + i] = A - E;
      data[jmi + i] = B + E;
      data[ji + im] = C + E;
      data[jmi + im] = D - E;
    }
  }
}

//------------------------------------------------------------------------------

/* 2D convolution calc, d1 *= d2, M/N - > log2 of width/height. */
static void fht_convolve(fREAL *d1, const fREAL *d2, uint M, uint N)
{
  fREAL a, b;
  uint i, j, k, L, mj, mL;
  uint m = 1 << M, n = 1 << N;
  uint m2 = 1 << (M - 1), n2 = 1 << (N - 1);
  uint mn2 = m << (N - 1);

  d1[0] *= d2[0];
  d1[mn2] *= d2[mn2];
  d1[m2] *= d2[m2];
  d1[m2 + mn2] *= d2[m2 + mn2];
  for (i = 1; i < m2; i++) {
    k = m - i;
    a = d1[i] * d2[i] - d1[k] * d2[k];
    b = d1[k] * d2[i] + d1[i] * d2[k];
    d1[i] = (b + a) * (fREAL)0.5;
    d1[k] = (b - a) * (fREAL)0.5;
    a = d1[i + mn2] * d2[i + mn2] - d1[k + mn2] * d2[k + mn2];
    b = d1[k + mn2] * d2[i + mn2] + d1[i + mn2] * d2[k + mn2];
    d1[i + mn2] = (b + a) * (fREAL)0.5;
    d1[k + mn2] = (b - a) * (fREAL)0.5;
  }
  for (j = 1; j < n2; j++) {
    L = n - j;
    mj = j << M;
    mL = L << M;
    a = d1[mj] * d2[mj] - d1[mL] * d2[mL];
    b = d1[mL] * d2[mj] + d1[mj] * d2[mL];
    d1[mj] = (b + a) * (fREAL)0.5;
    d1[mL] = (b - a) * (fREAL)0.5;
    a = d1[m2 + mj] * d2[m2 + mj] - d1[m2 + mL] * d2[m2 + mL];
    b = d1[m2 + mL] * d2[m2 + mj] + d1[m2 + mj] * d2[m2 + mL];
    d1[m2 + mj] = (b + a) * (fREAL)0.5;
    d1[m2 + mL] = (b - a) * (fREAL)0.5;
  }
  for (i = 1; i < m2; i++) {
    k = m - i;
    for (j = 1; j < n2; j++) {
      L = n - j;
      mj = j << M;
      mL = L << M;
      a = d1[i + mj] * d2[i + mj] - d1[k + mL] * d2[k + mL];
      b = d1[k + mL] * d2[i + mj] + d1[i + mj] * d2[k + mL];
      d1[i + mj] = (b + a) * (fREAL)0.5;
      d1[k + mL] = (b - a) * (fREAL)0.5;
      a = d1[i + mL] * d2[i + mL] - d1[k + mj] * d2[k + mj];
      b = d1[k + mj] * d2[i + mL] + d1[i + mL] * d2[k + mj];
      d1[i + mL] = (b + a) * (fREAL)0.5;
      d1[k + mj] = (b - a) * (fREAL)0.5;
    }
  }
}
//------------------------------------------------------------------------------

static void convolve(float *dst, MemoryBuffer *in1, MemoryBuffer *in2)
{
  fREAL *data1, *data2, *fp;
  uint w2, h2, hw, hh, log2_w, log2_h;
  fRGB wt, *colp;
  int x, y, ch;
  int xbl, ybl, nxb, nyb, xbsz, ybsz;
  bool in2done = false;
  const uint kernel_width = in2->get_width();
  const uint kernel_height = in2->get_height();
  const uint image_width = in1->get_width();
  const uint image_height = in1->get_height();
  float *kernel_buffer = in2->get_buffer();
  float *image_buffer = in1->get_buffer();

  MemoryBuffer *rdst = new MemoryBuffer(DataType::Color, in1->get_rect());
  memset(rdst->get_buffer(),
         0,
         rdst->get_width() * rdst->get_height() * COM_DATA_TYPE_COLOR_CHANNELS * sizeof(float));

  /* Convolution result width & height. */
  w2 = 2 * kernel_width - 1;
  h2 = 2 * kernel_height - 1;
  /* FFT pow2 required size & log2. */
  w2 = next_pow2(w2, &log2_w);
  h2 = next_pow2(h2, &log2_h);

  /* Allocate space. */
  data1 = (fREAL *)MEM_callocN(3 * w2 * h2 * sizeof(fREAL), "convolve_fast FHT data1");
  data2 = (fREAL *)MEM_callocN(w2 * h2 * sizeof(fREAL), "convolve_fast FHT data2");

  /* Normalize convolution. */
  wt[0] = wt[1] = wt[2] = 0.0f;
  for (y = 0; y < kernel_height; y++) {
    colp = (fRGB *)&kernel_buffer[y * kernel_width * COM_DATA_TYPE_COLOR_CHANNELS];
    for (x = 0; x < kernel_width; x++) {
      add_v3_v3(wt, colp[x]);
    }
  }
  if (wt[0] != 0.0f) {
    wt[0] = 1.0f / wt[0];
  }
  if (wt[1] != 0.0f) {
    wt[1] = 1.0f / wt[1];
  }
  if (wt[2] != 0.0f) {
    wt[2] = 1.0f / wt[2];
  }
  for (y = 0; y < kernel_height; y++) {
    colp = (fRGB *)&kernel_buffer[y * kernel_width * COM_DATA_TYPE_COLOR_CHANNELS];
    for (x = 0; x < kernel_width; x++) {
      mul_v3_v3(colp[x], wt);
    }
  }

  /* Copy image data, unpacking interleaved RGBA into separate channels
   * only need to calc data1 once. */

  /* Block add-overlap. */
  hw = kernel_width >> 1;
  hh = kernel_height >> 1;
  xbsz = (w2 + 1) - kernel_width;
  ybsz = (h2 + 1) - kernel_height;
  nxb = image_width / xbsz;
  if (image_width % xbsz) {
    nxb++;
  }
  nyb = image_height / ybsz;
  if (image_height % ybsz) {
    nyb++;
  }
  for (ybl = 0; ybl < nyb; ybl++) {
    for (xbl = 0; xbl < nxb; xbl++) {

      /* Each channel one by one. */
      for (ch = 0; ch < 3; ch++) {
        fREAL *data1ch = &data1[ch * w2 * h2];

        /* Only need to calc fht data from in2 once, can re-use for every block. */
        if (!in2done) {
          /* in2, channel ch -> data1 */
          for (y = 0; y < kernel_height; y++) {
            fp = &data1ch[y * w2];
            colp = (fRGB *)&kernel_buffer[y * kernel_width * COM_DATA_TYPE_COLOR_CHANNELS];
            for (x = 0; x < kernel_width; x++) {
              fp[x] = colp[x][ch];
            }
          }
        }

        /* in1, channel ch -> data2 */
        memset(data2, 0, w2 * h2 * sizeof(fREAL));
        for (y = 0; y < ybsz; y++) {
          int yy = ybl * ybsz + y;
          if (yy >= image_height) {
            continue;
          }
          fp = &data2[y * w2];
          colp = (fRGB *)&image_buffer[yy * image_width * COM_DATA_TYPE_COLOR_CHANNELS];
          for (x = 0; x < xbsz; x++) {
            int xx = xbl * xbsz + x;
            if (xx >= image_width) {
              continue;
            }
            fp[x] = colp[xx][ch];
          }
        }

        /* Forward FHT
         * zero pad data start is different for each == height+1. */
        if (!in2done) {
          FHT2D(data1ch, log2_w, log2_h, kernel_height + 1, 0);
        }
        FHT2D(data2, log2_w, log2_h, kernel_height + 1, 0);

        /* FHT2D transposed data, row/col now swapped
         * convolve & inverse FHT. */
        fht_convolve(data2, data1ch, log2_h, log2_w);
        FHT2D(data2, log2_h, log2_w, 0, 1);
        /* Data again transposed, so in order again. */

        /* Overlap-add result. */
        for (y = 0; y < int(h2); y++) {
          const int yy = ybl * ybsz + y - hh;
          if ((yy < 0) || (yy >= image_height)) {
            continue;
          }
          fp = &data2[y * w2];
          colp = (fRGB *)&rdst->get_buffer()[yy * image_width * COM_DATA_TYPE_COLOR_CHANNELS];
          for (x = 0; x < int(w2); x++) {
            const int xx = xbl * xbsz + x - hw;
            if ((xx < 0) || (xx >= image_width)) {
              continue;
            }
            colp[xx][ch] += fp[x];
          }
        }
      }
      in2done = true;
    }
  }

  MEM_freeN(data2);
  MEM_freeN(data1);
  memcpy(dst,
         rdst->get_buffer(),
         sizeof(float) * image_width * image_height * COM_DATA_TYPE_COLOR_CHANNELS);
  delete (rdst);
}

void GlareFogGlowOperation::generate_glare(float *data,
                                           MemoryBuffer *input_image,
                                           const NodeGlare *settings)
{
  const int kernel_size = 1 << settings->size;
  MemoryBuffer kernel = MemoryBuffer(DataType::Color, kernel_size, kernel_size);

  const float scale = 0.25f * math::sqrt(math::square(kernel_size));

  for (int y = 0; y < kernel_size; y++) {
    const float v = 2.0f * (y / float(kernel_size)) - 1.0f;
    for (int x = 0; x < kernel_size; x++) {
      const float u = 2.0f * (x / float(kernel_size)) - 1.0f;
      const float r = (math::square(u) + math::square(v)) * scale;
      const float d = -math::sqrt(math::sqrt(math::sqrt(r))) * 9.0f;
      const float kernel_value = math::exp(d);

      const float window = (0.5f + 0.5f * math::cos(u * math::numbers::pi)) *
                           (0.5f + 0.5f * math::cos(v * math::numbers::pi));
      const float windowed_kernel_value = window * kernel_value;

      copy_v3_fl(kernel.get_elem(x, y), windowed_kernel_value);
      kernel.get_elem(x, y)[3] = 1.0f;
    }
  }

  convolve(data, input_image, &kernel);
}

}  // namespace blender::compositor
