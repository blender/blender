/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WITH_OCEANSIM
#  include "BLI_threads.h"
#  include "fftw3.h"
#  define GRAVITY 9.81f

typedef struct Ocean {
  /* ********* input parameters to the sim ********* */
  float _V;
  float _l;
  float _w;
  float _A;
  float _damp_reflections;
  float _wind_alignment;
  float _depth;

  float _wx;
  float _wz;

  float _L;

  /* dimensions of computational grid */
  int _M;
  int _N;

  /* spatial size of computational grid */
  float _Lx;
  float _Lz;

  float normalize_factor; /* init w */
  float time;

  short _do_disp_y;
  short _do_normals;
  short _do_spray;
  short _do_chop;
  short _do_jacobian;

  /* Which spectral model we are using. */
  int _spectrum;

  /* JONSWAP common parameters. */
  float _fetch_jonswap;
  float _sharpen_peak_jonswap;

  /* mutex for threaded texture access */
  ThreadRWMutex oceanmutex;

  /* ********* sim data arrays ********* */

  /* two dimensional arrays of complex */
  fftw_complex *_fft_in;     /* init w   sim w */
  fftw_complex *_fft_in_x;   /* init w   sim w */
  fftw_complex *_fft_in_z;   /* init w   sim w */
  fftw_complex *_fft_in_jxx; /* init w   sim w */
  fftw_complex *_fft_in_jzz; /* init w   sim w */
  fftw_complex *_fft_in_jxz; /* init w   sim w */
  fftw_complex *_fft_in_nx;  /* init w   sim w */
  fftw_complex *_fft_in_nz;  /* init w   sim w */
  fftw_complex *_htilda;     /* init w   sim w (only once) */

  /* fftw "plans" */
  fftw_plan _disp_y_plan; /* init w   sim r */
  fftw_plan _disp_x_plan; /* init w   sim r */
  fftw_plan _disp_z_plan; /* init w   sim r */
  fftw_plan _N_x_plan;    /* init w   sim r */
  fftw_plan _N_z_plan;    /* init w   sim r */
  fftw_plan _Jxx_plan;    /* init w   sim r */
  fftw_plan _Jxz_plan;    /* init w   sim r */
  fftw_plan _Jzz_plan;    /* init w   sim r */

  /* two dimensional arrays of float */
  double *_disp_y; /* init w   sim w via plan? */
  double *_N_x;    /* init w   sim w via plan? */
  /* all member of this array has same values,
   * so convert this array to a float to reduce memory usage (MEM01). */
  // float * _N_y;
  double _N_y;     /*          sim w ********* can be rearranged? */
  double *_N_z;    /* init w   sim w via plan? */
  double *_disp_x; /* init w   sim w via plan? */
  double *_disp_z; /* init w   sim w via plan? */

  /* two dimensional arrays of float */
  /* Jacobian and minimum eigenvalue */
  double *_Jxx; /* init w   sim w */
  double *_Jzz; /* init w   sim w */
  double *_Jxz; /* init w   sim w */

  /* one dimensional float array */
  float *_kx; /* init w   sim r */
  float *_kz; /* init w   sim r */

  /* two dimensional complex array */
  fftw_complex *_h0;       /* init w   sim r */
  fftw_complex *_h0_minus; /* init w   sim r */

  /* two dimensional float array */
  float *_k; /* init w   sim r */
} Ocean;
#else
/* stub */
typedef struct Ocean {
  /* need some data here, C does not allow empty struct */
  int stub;
} Ocean;
#endif

#ifdef __cplusplus
}
#endif
