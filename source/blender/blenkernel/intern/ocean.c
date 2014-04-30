/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: Matt Ebb, Hamed Zaghaghi
 * Based on original code by Drew Whitehouse / Houdini Ocean Toolkit
 * OpenMP hints by Christian Schnellhammer
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/ocean.c
 *  \ingroup bke
 */

#include <math.h>
#include <stdlib.h>

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_rand.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_image.h"
#include "BKE_ocean.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_render_ext.h"

#ifdef WITH_OCEANSIM

/* Ocean code */
#include "fftw3.h"

#define GRAVITY  9.81f

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

	float normalize_factor;                 /* init w */
	float time;

	short _do_disp_y;
	short _do_normals;
	short _do_chop;
	short _do_jacobian;

	/* mutex for threaded texture access */
	ThreadRWMutex oceanmutex;

	/* ********* sim data arrays ********* */

	/* two dimensional arrays of complex */
	fftw_complex *_fft_in;          /* init w	sim w */
	fftw_complex *_fft_in_x;        /* init w	sim w */
	fftw_complex *_fft_in_z;        /* init w	sim w */
	fftw_complex *_fft_in_jxx;      /* init w	sim w */
	fftw_complex *_fft_in_jzz;      /* init w	sim w */
	fftw_complex *_fft_in_jxz;      /* init w	sim w */
	fftw_complex *_fft_in_nx;       /* init w	sim w */
	fftw_complex *_fft_in_nz;       /* init w	sim w */
	fftw_complex *_htilda;          /* init w	sim w (only once) */

	/* fftw "plans" */
	fftw_plan _disp_y_plan;         /* init w	sim r */
	fftw_plan _disp_x_plan;         /* init w	sim r */
	fftw_plan _disp_z_plan;         /* init w	sim r */
	fftw_plan _N_x_plan;            /* init w	sim r */
	fftw_plan _N_z_plan;            /* init w	sim r */
	fftw_plan _Jxx_plan;            /* init w	sim r */
	fftw_plan _Jxz_plan;            /* init w	sim r */
	fftw_plan _Jzz_plan;            /* init w	sim r */

	/* two dimensional arrays of float */
	double *_disp_y;                /* init w	sim w via plan? */
	double *_N_x;                   /* init w	sim w via plan? */
	/* all member of this array has same values, so convert this array to a float to reduce memory usage (MEM01)*/
	/*float * _N_y; */
	double _N_y;                    /*			sim w ********* can be rearranged? */
	double *_N_z;                   /* init w	sim w via plan? */
	double *_disp_x;                /* init w	sim w via plan? */
	double *_disp_z;                /* init w	sim w via plan? */

	/* two dimensional arrays of float */
	/* Jacobian and minimum eigenvalue */
	double *_Jxx;                   /* init w	sim w */
	double *_Jzz;                   /* init w	sim w */
	double *_Jxz;                   /* init w	sim w */

	/* one dimensional float array */
	float *_kx;                     /* init w	sim r */
	float *_kz;                     /* init w	sim r */

	/* two dimensional complex array */
	fftw_complex *_h0;              /* init w	sim r */
	fftw_complex *_h0_minus;        /* init w	sim r */

	/* two dimensional float array */
	float *_k;                      /* init w	sim r */
} Ocean;



static float nextfr(RNG *rng, float min, float max)
{
	return BLI_rng_get_float(rng) * (min - max) + max;
}

static float gaussRand(RNG *rng)
{
	/* Note: to avoid numerical problems with very small numbers, we make these variables singe-precision floats,
	 * but later we call the double-precision log() and sqrt() functions instead of logf() and sqrtf().
	 */ 
	float x;
	float y;
	float length2;

	do {
		x = (float) (nextfr(rng, -1, 1));
		y = (float)(nextfr(rng, -1, 1));
		length2 = x * x + y * y;
	} while (length2 >= 1 || length2 == 0);

	return x * sqrtf(-2.0f * logf(length2) / length2);
}

/**
 * Some useful functions
 */
MINLINE float catrom(float p0, float p1, float p2, float p3, float f)
{
	return 0.5f * ((2.0f * p1) +
	               (-p0 + p2) * f +
	               (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * f * f +
	               (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * f * f * f);
}

MINLINE float omega(float k, float depth)
{
	return sqrtf(GRAVITY * k * tanhf(k * depth));
}

/* modified Phillips spectrum */
static float Ph(struct Ocean *o, float kx, float kz)
{
	float tmp;
	float k2 = kx * kx + kz * kz;

	if (k2 == 0.0f) {
		return 0.0f; /* no DC component */
	}

	/* damp out the waves going in the direction opposite the wind */
	tmp = (o->_wx * kx + o->_wz * kz) / sqrtf(k2);
	if (tmp < 0) {
		tmp *= o->_damp_reflections;
	}

	return o->_A * expf(-1.0f / (k2 * (o->_L * o->_L))) * expf(-k2 * (o->_l * o->_l)) *
	       powf(fabsf(tmp), o->_wind_alignment) / (k2 * k2);
}

static void compute_eigenstuff(struct OceanResult *ocr, float jxx, float jzz, float jxz)
{
	float a, b, qplus, qminus;
	a = jxx + jzz;
	b = sqrt((jxx - jzz) * (jxx - jzz) + 4 * jxz * jxz);

	ocr->Jminus = 0.5f * (a - b);
	ocr->Jplus  = 0.5f * (a + b);

	qplus  = (ocr->Jplus  - jxx) / jxz;
	qminus = (ocr->Jminus - jxx) / jxz;

	a = sqrt(1 + qplus * qplus);
	b = sqrt(1 + qminus * qminus);

	ocr->Eplus[0] = 1.0f / a;
	ocr->Eplus[1] = 0.0f;
	ocr->Eplus[2] = qplus / a;

	ocr->Eminus[0] = 1.0f / b;
	ocr->Eminus[1] = 0.0f;
	ocr->Eminus[2] = qminus / b;
}

/*
 * instead of Complex.h
 * in fftw.h "fftw_complex" typedefed as double[2]
 * below you can see functions are needed to work with such complex numbers.
 * */
static void init_complex(fftw_complex cmpl, float real, float image)
{
	cmpl[0] = real;
	cmpl[1] = image;
}

#if 0   /* unused */
static void add_complex_f(fftw_complex res, fftw_complex cmpl, float f)
{
	res[0] = cmpl[0] + f;
	res[1] = cmpl[1];
}
#endif

static void add_comlex_c(fftw_complex res, fftw_complex cmpl1, fftw_complex cmpl2)
{
	res[0] = cmpl1[0] + cmpl2[0];
	res[1] = cmpl1[1] + cmpl2[1];
}

static void mul_complex_f(fftw_complex res, fftw_complex cmpl, float f)
{
	res[0] = cmpl[0] * (double)f;
	res[1] = cmpl[1] * (double)f;
}

static void mul_complex_c(fftw_complex res, fftw_complex cmpl1, fftw_complex cmpl2)
{
	fftwf_complex temp;
	temp[0] = cmpl1[0] * cmpl2[0] - cmpl1[1] * cmpl2[1];
	temp[1] = cmpl1[0] * cmpl2[1] + cmpl1[1] * cmpl2[0];
	res[0] = temp[0];
	res[1] = temp[1];
}

static float real_c(fftw_complex cmpl)
{
	return cmpl[0];
}

static float image_c(fftw_complex cmpl)
{
	return cmpl[1];
}

static void conj_complex(fftw_complex res, fftw_complex cmpl1)
{
	res[0] = cmpl1[0];
	res[1] = -cmpl1[1];
}

static void exp_complex(fftw_complex res, fftw_complex cmpl)
{
	float r = expf(cmpl[0]);

	res[0] = cosf(cmpl[1]) * r;
	res[1] = sinf(cmpl[1]) * r;
}

float BKE_ocean_jminus_to_foam(float jminus, float coverage)
{
	float foam = jminus * -0.005f + coverage;
	CLAMP(foam, 0.0f, 1.0f);
	return foam * foam;
}

void BKE_ocean_eval_uv(struct Ocean *oc, struct OceanResult *ocr, float u, float v)
{
	int i0, i1, j0, j1;
	float frac_x, frac_z;
	float uu, vv;

	/* first wrap the texture so 0 <= (u, v) < 1 */
	u = fmodf(u, 1.0f);
	v = fmodf(v, 1.0f);

	if (u < 0) u += 1.0f;
	if (v < 0) v += 1.0f;

	BLI_rw_mutex_lock(&oc->oceanmutex, THREAD_LOCK_READ);

	uu = u * oc->_M;
	vv = v * oc->_N;

	i0 = (int)floor(uu);
	j0 = (int)floor(vv);

	i1 = (i0 + 1);
	j1 = (j0 + 1);

	frac_x = uu - i0;
	frac_z = vv - j0;

	i0 = i0 % oc->_M;
	j0 = j0 % oc->_N;

	i1 = i1 % oc->_M;
	j1 = j1 % oc->_N;


#define BILERP(m) (interpf(interpf(m[i1 * oc->_N + j1], m[i0 * oc->_N + j1], frac_x), \
                           interpf(m[i1 * oc->_N + j0], m[i0 * oc->_N + j0], frac_x), \
                           frac_z))
	{
		if (oc->_do_disp_y) {
			ocr->disp[1] = BILERP(oc->_disp_y);
		}

		if (oc->_do_normals) {
			ocr->normal[0] = BILERP(oc->_N_x);
			ocr->normal[1] = oc->_N_y /*BILERP(oc->_N_y) (MEM01)*/;
			ocr->normal[2] = BILERP(oc->_N_z);
		}

		if (oc->_do_chop) {
			ocr->disp[0] = BILERP(oc->_disp_x);
			ocr->disp[2] = BILERP(oc->_disp_z);
		}
		else {
			ocr->disp[0] = 0.0;
			ocr->disp[2] = 0.0;
		}

		if (oc->_do_jacobian) {
			compute_eigenstuff(ocr, BILERP(oc->_Jxx), BILERP(oc->_Jzz), BILERP(oc->_Jxz));
		}
	}
#undef BILERP

	BLI_rw_mutex_unlock(&oc->oceanmutex);
}

/* use catmullrom interpolation rather than linear */
void BKE_ocean_eval_uv_catrom(struct Ocean *oc, struct OceanResult *ocr, float u, float v)
{
	int i0, i1, i2, i3, j0, j1, j2, j3;
	float frac_x, frac_z;
	float uu, vv;

	/* first wrap the texture so 0 <= (u, v) < 1 */
	u = fmod(u, 1.0f);
	v = fmod(v, 1.0f);

	if (u < 0) u += 1.0f;
	if (v < 0) v += 1.0f;

	BLI_rw_mutex_lock(&oc->oceanmutex, THREAD_LOCK_READ);

	uu = u * oc->_M;
	vv = v * oc->_N;

	i1 = (int)floor(uu);
	j1 = (int)floor(vv);

	i2 = (i1 + 1);
	j2 = (j1 + 1);

	frac_x = uu - i1;
	frac_z = vv - j1;

	i1 = i1 % oc->_M;
	j1 = j1 % oc->_N;

	i2 = i2 % oc->_M;
	j2 = j2 % oc->_N;

	i0 = (i1 - 1);
	i3 = (i2 + 1);
	i0 = i0 <   0 ? i0 + oc->_M : i0;
	i3 = i3 >= oc->_M ? i3 - oc->_M : i3;

	j0 = (j1 - 1);
	j3 = (j2 + 1);
	j0 = j0 <   0 ? j0 + oc->_N : j0;
	j3 = j3 >= oc->_N ? j3 - oc->_N : j3;

#define INTERP(m) catrom(catrom(m[i0 * oc->_N + j0], m[i1 * oc->_N + j0], \
                                m[i2 * oc->_N + j0], m[i3 * oc->_N + j0], frac_x), \
                         catrom(m[i0 * oc->_N + j1], m[i1 * oc->_N + j1], \
                                m[i2 * oc->_N + j1], m[i3 * oc->_N + j1], frac_x), \
                         catrom(m[i0 * oc->_N + j2], m[i1 * oc->_N + j2], \
                                m[i2 * oc->_N + j2], m[i3 * oc->_N + j2], frac_x), \
                         catrom(m[i0 * oc->_N + j3], m[i1 * oc->_N + j3], \
                                m[i2 * oc->_N + j3], m[i3 * oc->_N + j3], frac_x), \
                         frac_z)

	{
		if (oc->_do_disp_y) {
			ocr->disp[1] = INTERP(oc->_disp_y);
		}
		if (oc->_do_normals) {
			ocr->normal[0] = INTERP(oc->_N_x);
			ocr->normal[1] = oc->_N_y /*INTERP(oc->_N_y) (MEM01)*/;
			ocr->normal[2] = INTERP(oc->_N_z);
		}
		if (oc->_do_chop) {
			ocr->disp[0] = INTERP(oc->_disp_x);
			ocr->disp[2] = INTERP(oc->_disp_z);
		}
		else {
			ocr->disp[0] = 0.0;
			ocr->disp[2] = 0.0;
		}

		if (oc->_do_jacobian) {
			compute_eigenstuff(ocr, INTERP(oc->_Jxx), INTERP(oc->_Jzz), INTERP(oc->_Jxz));
		}
	}
#undef INTERP

	BLI_rw_mutex_unlock(&oc->oceanmutex);

}

void BKE_ocean_eval_xz(struct Ocean *oc, struct OceanResult *ocr, float x, float z)
{
	BKE_ocean_eval_uv(oc, ocr, x / oc->_Lx, z / oc->_Lz);
}

void BKE_ocean_eval_xz_catrom(struct Ocean *oc, struct OceanResult *ocr, float x, float z)
{
	BKE_ocean_eval_uv_catrom(oc, ocr, x / oc->_Lx, z / oc->_Lz);
}

/* note that this doesn't wrap properly for i, j < 0, but its not really meant for that being just a way to get
 * the raw data out to save in some image format.
 */
void BKE_ocean_eval_ij(struct Ocean *oc, struct OceanResult *ocr, int i, int j)
{
	BLI_rw_mutex_lock(&oc->oceanmutex, THREAD_LOCK_READ);

	i = abs(i) % oc->_M;
	j = abs(j) % oc->_N;

	ocr->disp[1] = oc->_do_disp_y ? (float)oc->_disp_y[i * oc->_N + j] : 0.0f;

	if (oc->_do_chop) {
		ocr->disp[0] = oc->_disp_x[i * oc->_N + j];
		ocr->disp[2] = oc->_disp_z[i * oc->_N + j];
	}
	else {
		ocr->disp[0] = 0.0f;
		ocr->disp[2] = 0.0f;
	}

	if (oc->_do_normals) {
		ocr->normal[0] = oc->_N_x[i * oc->_N + j];
		ocr->normal[1] = oc->_N_y  /* oc->_N_y[i * oc->_N + j] (MEM01) */;
		ocr->normal[2] = oc->_N_z[i * oc->_N + j];

		normalize_v3(ocr->normal);
	}

	if (oc->_do_jacobian) {
		compute_eigenstuff(ocr, oc->_Jxx[i * oc->_N + j], oc->_Jzz[i * oc->_N + j], oc->_Jxz[i * oc->_N + j]);
	}

	BLI_rw_mutex_unlock(&oc->oceanmutex);
}

void BKE_simulate_ocean(struct Ocean *o, float t, float scale, float chop_amount)
{
	int i, j;

	scale *= o->normalize_factor;

	BLI_rw_mutex_lock(&o->oceanmutex, THREAD_LOCK_WRITE);

	/* compute a new htilda */
#pragma omp parallel for private(i, j)
	for (i = 0; i < o->_M; ++i) {
		/* note the <= _N/2 here, see the fftw doco about the mechanics of the complex->real fft storage */
		for (j = 0; j <= o->_N / 2; ++j) {
			fftw_complex exp_param1;
			fftw_complex exp_param2;
			fftw_complex conj_param;


			init_complex(exp_param1, 0.0, omega(o->_k[i * (1 + o->_N / 2) + j], o->_depth) * t);
			init_complex(exp_param2, 0.0, -omega(o->_k[i * (1 + o->_N / 2) + j], o->_depth) * t);
			exp_complex(exp_param1, exp_param1);
			exp_complex(exp_param2, exp_param2);
			conj_complex(conj_param, o->_h0_minus[i * o->_N + j]);

			mul_complex_c(exp_param1, o->_h0[i * o->_N + j], exp_param1);
			mul_complex_c(exp_param2, conj_param, exp_param2);

			add_comlex_c(o->_htilda[i * (1 + o->_N / 2) + j], exp_param1, exp_param2);
			mul_complex_f(o->_fft_in[i * (1 + o->_N / 2) + j], o->_htilda[i * (1 + o->_N / 2) + j], scale);
		}
	}

#pragma omp parallel sections private(i, j)
	{

#pragma omp section
		{
			if (o->_do_disp_y) {
				/* y displacement */
				fftw_execute(o->_disp_y_plan);
			}
		} /* section 1 */

#pragma omp section
		{
			if (o->_do_chop) {
				/* x displacement */
				for (i = 0; i < o->_M; ++i) {
					for (j = 0; j <= o->_N / 2; ++j) {
						fftw_complex mul_param;
						fftw_complex minus_i;

						init_complex(minus_i, 0.0, -1.0);
						init_complex(mul_param, -scale, 0);
						mul_complex_f(mul_param, mul_param, chop_amount);
						mul_complex_c(mul_param, mul_param, minus_i);
						mul_complex_c(mul_param, mul_param, o->_htilda[i * (1 + o->_N / 2) + j]);
						mul_complex_f(mul_param, mul_param,
						              ((o->_k[i * (1 + o->_N / 2) + j] == 0.0f) ?
						               0.0f :
						               o->_kx[i] / o->_k[i * (1 + o->_N / 2) + j]));
						init_complex(o->_fft_in_x[i * (1 + o->_N / 2) + j], real_c(mul_param), image_c(mul_param));
					}
				}
				fftw_execute(o->_disp_x_plan);
			}
		} /* section 2 */

#pragma omp section
		{
			if (o->_do_chop) {
				/* z displacement */
				for (i = 0; i < o->_M; ++i) {
					for (j = 0; j <= o->_N / 2; ++j) {
						fftw_complex mul_param;
						fftw_complex minus_i;

						init_complex(minus_i, 0.0, -1.0);
						init_complex(mul_param, -scale, 0);
						mul_complex_f(mul_param, mul_param, chop_amount);
						mul_complex_c(mul_param, mul_param, minus_i);
						mul_complex_c(mul_param, mul_param, o->_htilda[i * (1 + o->_N / 2) + j]);
						mul_complex_f(mul_param, mul_param,
						              ((o->_k[i * (1 + o->_N / 2) + j] == 0.0f) ?
						               0.0f :
						               o->_kz[j] / o->_k[i * (1 + o->_N / 2) + j]));
						init_complex(o->_fft_in_z[i * (1 + o->_N / 2) + j], real_c(mul_param), image_c(mul_param));
					}
				}
				fftw_execute(o->_disp_z_plan);
			}
		} /* section 3 */

#pragma omp section
		{
			if (o->_do_jacobian) {
				/* Jxx */
				for (i = 0; i < o->_M; ++i) {
					for (j = 0; j <= o->_N / 2; ++j) {
						fftw_complex mul_param;

						/* init_complex(mul_param, -scale, 0); */
						init_complex(mul_param, -1, 0);

						mul_complex_f(mul_param, mul_param, chop_amount);
						mul_complex_c(mul_param, mul_param, o->_htilda[i * (1 + o->_N / 2) + j]);
						mul_complex_f(mul_param, mul_param,
						              ((o->_k[i * (1 + o->_N / 2) + j] == 0.0f) ?
						               0.0f :
						               o->_kx[i] * o->_kx[i] / o->_k[i * (1 + o->_N / 2) + j]));
						init_complex(o->_fft_in_jxx[i * (1 + o->_N / 2) + j], real_c(mul_param), image_c(mul_param));
					}
				}
				fftw_execute(o->_Jxx_plan);

				for (i = 0; i < o->_M; ++i) {
					for (j = 0; j < o->_N; ++j) {
						o->_Jxx[i * o->_N + j] += 1.0;
					}
				}
			}
		} /* section 4 */

#pragma omp section
		{
			if (o->_do_jacobian) {
				/* Jzz */
				for (i = 0; i < o->_M; ++i) {
					for (j = 0; j <= o->_N / 2; ++j) {
						fftw_complex mul_param;

						/* init_complex(mul_param, -scale, 0); */
						init_complex(mul_param, -1, 0);

						mul_complex_f(mul_param, mul_param, chop_amount);
						mul_complex_c(mul_param, mul_param, o->_htilda[i * (1 + o->_N / 2) + j]);
						mul_complex_f(mul_param, mul_param,
						              ((o->_k[i * (1 + o->_N / 2) + j] == 0.0f) ?
						               0.0f :
						               o->_kz[j] * o->_kz[j] / o->_k[i * (1 + o->_N / 2) + j]));
						init_complex(o->_fft_in_jzz[i * (1 + o->_N / 2) + j], real_c(mul_param), image_c(mul_param));
					}
				}
				fftw_execute(o->_Jzz_plan);
				for (i = 0; i < o->_M; ++i) {
					for (j = 0; j < o->_N; ++j) {
						o->_Jzz[i * o->_N + j] += 1.0;
					}
				}
			}
		} /* section 5 */

#pragma omp section
		{
			if (o->_do_jacobian) {
				/* Jxz */
				for (i = 0; i < o->_M; ++i) {
					for (j = 0; j <= o->_N / 2; ++j) {
						fftw_complex mul_param;

						/* init_complex(mul_param, -scale, 0); */
						init_complex(mul_param, -1, 0);

						mul_complex_f(mul_param, mul_param, chop_amount);
						mul_complex_c(mul_param, mul_param, o->_htilda[i * (1 + o->_N / 2) + j]);
						mul_complex_f(mul_param, mul_param,
						              ((o->_k[i * (1 + o->_N / 2) + j] == 0.0f) ?
						               0.0f :
						               o->_kx[i] * o->_kz[j] / o->_k[i * (1 + o->_N / 2) + j]));
						init_complex(o->_fft_in_jxz[i * (1 + o->_N / 2) + j], real_c(mul_param), image_c(mul_param));
					}
				}
				fftw_execute(o->_Jxz_plan);
			}
		} /* section 6 */

#pragma omp section
		{
			/* fft normals */
			if (o->_do_normals) {
				for (i = 0; i < o->_M; ++i) {
					for (j = 0; j <= o->_N / 2; ++j) {
						fftw_complex mul_param;

						init_complex(mul_param, 0.0, -1.0);
						mul_complex_c(mul_param, mul_param, o->_htilda[i * (1 + o->_N / 2) + j]);
						mul_complex_f(mul_param, mul_param, o->_kx[i]);
						init_complex(o->_fft_in_nx[i * (1 + o->_N / 2) + j], real_c(mul_param), image_c(mul_param));
					}
				}
				fftw_execute(o->_N_x_plan);

			}
		} /* section 7 */

#pragma omp section
		{
			if (o->_do_normals) {
				for (i = 0; i < o->_M; ++i) {
					for (j = 0; j <= o->_N / 2; ++j) {
						fftw_complex mul_param;

						init_complex(mul_param, 0.0, -1.0);
						mul_complex_c(mul_param, mul_param, o->_htilda[i * (1 + o->_N / 2) + j]);
						mul_complex_f(mul_param, mul_param, o->_kz[i]);
						init_complex(o->_fft_in_nz[i * (1 + o->_N / 2) + j], real_c(mul_param), image_c(mul_param));
					}
				}
				fftw_execute(o->_N_z_plan);

#if 0
				for (i = 0; i < o->_M; ++i) {
					for (j = 0; j < o->_N; ++j) {
						o->_N_y[i * o->_N + j] = 1.0f / scale;
					}
				}
				(MEM01)
#endif
				o->_N_y = 1.0f / scale;
			}
		} /* section 8 */

	} /* omp sections */

	BLI_rw_mutex_unlock(&o->oceanmutex);
}

static void set_height_normalize_factor(struct Ocean *oc)
{
	float res = 1.0;
	float max_h = 0.0;

	int i, j;

	if (!oc->_do_disp_y) return;

	oc->normalize_factor = 1.0;

	BKE_simulate_ocean(oc, 0.0, 1.0, 0);

	BLI_rw_mutex_lock(&oc->oceanmutex, THREAD_LOCK_READ);

	for (i = 0; i < oc->_M; ++i) {
		for (j = 0; j < oc->_N; ++j) {
			if (max_h < fabsf(oc->_disp_y[i * oc->_N + j])) {
				max_h = fabsf(oc->_disp_y[i * oc->_N + j]);
			}
		}
	}

	BLI_rw_mutex_unlock(&oc->oceanmutex);

	if (max_h == 0.0f)
		max_h = 0.00001f;  /* just in case ... */

	res = 1.0f / (max_h);

	oc->normalize_factor = res;
}

struct Ocean *BKE_add_ocean(void)
{
	Ocean *oc = MEM_callocN(sizeof(Ocean), "ocean sim data");

	BLI_rw_mutex_init(&oc->oceanmutex);

	return oc;
}

void BKE_init_ocean(struct Ocean *o, int M, int N, float Lx, float Lz, float V, float l, float A, float w, float damp,
                    float alignment, float depth, float time, short do_height_field, short do_chop, short do_normals,
                    short do_jacobian, int seed)
{
	RNG *rng;
	int i, j, ii;

	BLI_rw_mutex_lock(&o->oceanmutex, THREAD_LOCK_WRITE);

	o->_M = M;
	o->_N = N;
	o->_V = V;
	o->_l = l;
	o->_A = A;
	o->_w = w;
	o->_damp_reflections = 1.0f - damp;
	o->_wind_alignment = alignment;
	o->_depth = depth;
	o->_Lx = Lx;
	o->_Lz = Lz;
	o->_wx = cos(w);
	o->_wz = -sin(w); /* wave direction */
	o->_L = V * V / GRAVITY;  /* largest wave for a given velocity V */
	o->time = time;

	o->_do_disp_y = do_height_field;
	o->_do_normals = do_normals;
	o->_do_chop = do_chop;
	o->_do_jacobian = do_jacobian;

	o->_k = (float *) MEM_mallocN(M * (1 + N / 2) * sizeof(float), "ocean_k");
	o->_h0 = (fftw_complex *) MEM_mallocN(M * N * sizeof(fftw_complex), "ocean_h0");
	o->_h0_minus = (fftw_complex *) MEM_mallocN(M * N * sizeof(fftw_complex), "ocean_h0_minus");
	o->_kx = (float *) MEM_mallocN(o->_M * sizeof(float), "ocean_kx");
	o->_kz = (float *) MEM_mallocN(o->_N * sizeof(float), "ocean_kz");

	/* make this robust in the face of erroneous usage */
	if (o->_Lx == 0.0f)
		o->_Lx = 0.001f;

	if (o->_Lz == 0.0f)
		o->_Lz = 0.001f;

	/* the +ve components and DC */
	for (i = 0; i <= o->_M / 2; ++i)
		o->_kx[i] = 2.0f * (float)M_PI * i / o->_Lx;

	/* the -ve components */
	for (i = o->_M - 1, ii = 0; i > o->_M / 2; --i, ++ii)
		o->_kx[i] = -2.0f * (float)M_PI * ii / o->_Lx;

	/* the +ve components and DC */
	for (i = 0; i <= o->_N / 2; ++i)
		o->_kz[i] = 2.0f * (float)M_PI * i / o->_Lz;

	/* the -ve components */
	for (i = o->_N - 1, ii = 0; i > o->_N / 2; --i, ++ii)
		o->_kz[i] = -2.0f * (float)M_PI * ii / o->_Lz;

	/* pre-calculate the k matrix */
	for (i = 0; i < o->_M; ++i)
		for (j = 0; j <= o->_N / 2; ++j)
			o->_k[i * (1 + o->_N / 2) + j] = sqrt(o->_kx[i] * o->_kx[i] + o->_kz[j] * o->_kz[j]);

	/*srand(seed);*/
	rng = BLI_rng_new(seed);

	for (i = 0; i < o->_M; ++i) {
		for (j = 0; j < o->_N; ++j) {
			float r1 = gaussRand(rng);
			float r2 = gaussRand(rng);

			fftw_complex r1r2;
			init_complex(r1r2, r1, r2);
			mul_complex_f(o->_h0[i * o->_N + j], r1r2, (float)(sqrt(Ph(o, o->_kx[i], o->_kz[j]) / 2.0f)));
			mul_complex_f(o->_h0_minus[i * o->_N + j], r1r2, (float)(sqrt(Ph(o, -o->_kx[i], -o->_kz[j]) / 2.0f)));
		}
	}

	o->_fft_in = (fftw_complex *)MEM_mallocN(o->_M * (1 + o->_N / 2) * sizeof(fftw_complex), "ocean_fft_in");
	o->_htilda = (fftw_complex *)MEM_mallocN(o->_M * (1 + o->_N / 2) * sizeof(fftw_complex), "ocean_htilda");

	BLI_lock_thread(LOCK_FFTW);

	if (o->_do_disp_y) {
		o->_disp_y = (double *)MEM_mallocN(o->_M * o->_N * sizeof(double), "ocean_disp_y");
		o->_disp_y_plan = fftw_plan_dft_c2r_2d(o->_M, o->_N, o->_fft_in, o->_disp_y, FFTW_ESTIMATE);
	}

	if (o->_do_normals) {
		o->_fft_in_nx = (fftw_complex *) MEM_mallocN(o->_M * (1 + o->_N / 2) * sizeof(fftw_complex), "ocean_fft_in_nx");
		o->_fft_in_nz = (fftw_complex *) MEM_mallocN(o->_M * (1 + o->_N / 2) * sizeof(fftw_complex), "ocean_fft_in_nz");

		o->_N_x = (double *)MEM_mallocN(o->_M * o->_N * sizeof(double), "ocean_N_x");
		/* o->_N_y = (float *) fftwf_malloc(o->_M * o->_N * sizeof(float)); (MEM01) */
		o->_N_z = (double *)MEM_mallocN(o->_M * o->_N * sizeof(double), "ocean_N_z");

		o->_N_x_plan = fftw_plan_dft_c2r_2d(o->_M, o->_N, o->_fft_in_nx, o->_N_x, FFTW_ESTIMATE);
		o->_N_z_plan = fftw_plan_dft_c2r_2d(o->_M, o->_N, o->_fft_in_nz, o->_N_z, FFTW_ESTIMATE);
	}

	if (o->_do_chop) {
		o->_fft_in_x = (fftw_complex *)MEM_mallocN(o->_M * (1 + o->_N / 2) * sizeof(fftw_complex), "ocean_fft_in_x");
		o->_fft_in_z = (fftw_complex *)MEM_mallocN(o->_M * (1 + o->_N / 2) * sizeof(fftw_complex), "ocean_fft_in_z");

		o->_disp_x = (double *)MEM_mallocN(o->_M * o->_N * sizeof(double), "ocean_disp_x");
		o->_disp_z = (double *)MEM_mallocN(o->_M * o->_N * sizeof(double), "ocean_disp_z");

		o->_disp_x_plan = fftw_plan_dft_c2r_2d(o->_M, o->_N, o->_fft_in_x, o->_disp_x, FFTW_ESTIMATE);
		o->_disp_z_plan = fftw_plan_dft_c2r_2d(o->_M, o->_N, o->_fft_in_z, o->_disp_z, FFTW_ESTIMATE);
	}
	if (o->_do_jacobian) {
		o->_fft_in_jxx = (fftw_complex *)MEM_mallocN(o->_M * (1 + o->_N / 2) * sizeof(fftw_complex),
		                                             "ocean_fft_in_jxx");
		o->_fft_in_jzz = (fftw_complex *)MEM_mallocN(o->_M * (1 + o->_N / 2) * sizeof(fftw_complex),
		                                             "ocean_fft_in_jzz");
		o->_fft_in_jxz = (fftw_complex *)MEM_mallocN(o->_M * (1 + o->_N / 2) * sizeof(fftw_complex),
		                                             "ocean_fft_in_jxz");

		o->_Jxx = (double *)MEM_mallocN(o->_M * o->_N * sizeof(double), "ocean_Jxx");
		o->_Jzz = (double *)MEM_mallocN(o->_M * o->_N * sizeof(double), "ocean_Jzz");
		o->_Jxz = (double *)MEM_mallocN(o->_M * o->_N * sizeof(double), "ocean_Jxz");

		o->_Jxx_plan = fftw_plan_dft_c2r_2d(o->_M, o->_N, o->_fft_in_jxx, o->_Jxx, FFTW_ESTIMATE);
		o->_Jzz_plan = fftw_plan_dft_c2r_2d(o->_M, o->_N, o->_fft_in_jzz, o->_Jzz, FFTW_ESTIMATE);
		o->_Jxz_plan = fftw_plan_dft_c2r_2d(o->_M, o->_N, o->_fft_in_jxz, o->_Jxz, FFTW_ESTIMATE);
	}

	BLI_unlock_thread(LOCK_FFTW);

	BLI_rw_mutex_unlock(&o->oceanmutex);

	set_height_normalize_factor(o);

	BLI_rng_free(rng);
}

void BKE_free_ocean_data(struct Ocean *oc)
{
	if (!oc) return;

	BLI_rw_mutex_lock(&oc->oceanmutex, THREAD_LOCK_WRITE);

	BLI_lock_thread(LOCK_FFTW);

	if (oc->_do_disp_y) {
		fftw_destroy_plan(oc->_disp_y_plan);
		MEM_freeN(oc->_disp_y);
	}

	if (oc->_do_normals) {
		MEM_freeN(oc->_fft_in_nx);
		MEM_freeN(oc->_fft_in_nz);
		fftw_destroy_plan(oc->_N_x_plan);
		fftw_destroy_plan(oc->_N_z_plan);
		MEM_freeN(oc->_N_x);
		/*fftwf_free(oc->_N_y); (MEM01)*/
		MEM_freeN(oc->_N_z);
	}

	if (oc->_do_chop) {
		MEM_freeN(oc->_fft_in_x);
		MEM_freeN(oc->_fft_in_z);
		fftw_destroy_plan(oc->_disp_x_plan);
		fftw_destroy_plan(oc->_disp_z_plan);
		MEM_freeN(oc->_disp_x);
		MEM_freeN(oc->_disp_z);
	}

	if (oc->_do_jacobian) {
		MEM_freeN(oc->_fft_in_jxx);
		MEM_freeN(oc->_fft_in_jzz);
		MEM_freeN(oc->_fft_in_jxz);
		fftw_destroy_plan(oc->_Jxx_plan);
		fftw_destroy_plan(oc->_Jzz_plan);
		fftw_destroy_plan(oc->_Jxz_plan);
		MEM_freeN(oc->_Jxx);
		MEM_freeN(oc->_Jzz);
		MEM_freeN(oc->_Jxz);
	}

	BLI_unlock_thread(LOCK_FFTW);

	if (oc->_fft_in)
		MEM_freeN(oc->_fft_in);

	/* check that ocean data has been initialized */
	if (oc->_htilda) {
		MEM_freeN(oc->_htilda);
		MEM_freeN(oc->_k);
		MEM_freeN(oc->_h0);
		MEM_freeN(oc->_h0_minus);
		MEM_freeN(oc->_kx);
		MEM_freeN(oc->_kz);
	}

	BLI_rw_mutex_unlock(&oc->oceanmutex);
}

void BKE_free_ocean(struct Ocean *oc)
{
	if (!oc) return;

	BKE_free_ocean_data(oc);
	BLI_rw_mutex_end(&oc->oceanmutex);

	MEM_freeN(oc);
}

#undef GRAVITY


/* ********* Baking/Caching ********* */


#define CACHE_TYPE_DISPLACE 1
#define CACHE_TYPE_FOAM     2
#define CACHE_TYPE_NORMAL   3

static void cache_filename(char *string, const char *path, const char *relbase, int frame, int type)
{
	char cachepath[FILE_MAX];
	const char *fname;

	switch (type) {
		case CACHE_TYPE_FOAM:
			fname = "foam_";
			break;
		case CACHE_TYPE_NORMAL:
			fname = "normal_";
			break;
		case CACHE_TYPE_DISPLACE:
		default:
			fname = "disp_";
			break;
	}

	BLI_join_dirfile(cachepath, sizeof(cachepath), path, fname);

	BKE_makepicstring_from_type(string, cachepath, relbase, frame, R_IMF_IMTYPE_OPENEXR, true, true);
}

/* silly functions but useful to inline when the args do a lot of indirections */
MINLINE void rgb_to_rgba_unit_alpha(float r_rgba[4], const float rgb[3])
{
	r_rgba[0] = rgb[0];
	r_rgba[1] = rgb[1];
	r_rgba[2] = rgb[2];
	r_rgba[3] = 1.0f;
}
MINLINE void value_to_rgba_unit_alpha(float r_rgba[4], const float value)
{
	r_rgba[0] = value;
	r_rgba[1] = value;
	r_rgba[2] = value;
	r_rgba[3] = 1.0f;
}

void BKE_free_ocean_cache(struct OceanCache *och)
{
	int i, f = 0;

	if (!och) return;

	if (och->ibufs_disp) {
		for (i = och->start, f = 0; i <= och->end; i++, f++) {
			if (och->ibufs_disp[f]) {
				IMB_freeImBuf(och->ibufs_disp[f]);
			}
		}
		MEM_freeN(och->ibufs_disp);
	}

	if (och->ibufs_foam) {
		for (i = och->start, f = 0; i <= och->end; i++, f++) {
			if (och->ibufs_foam[f]) {
				IMB_freeImBuf(och->ibufs_foam[f]);
			}
		}
		MEM_freeN(och->ibufs_foam);
	}

	if (och->ibufs_norm) {
		for (i = och->start, f = 0; i <= och->end; i++, f++) {
			if (och->ibufs_norm[f]) {
				IMB_freeImBuf(och->ibufs_norm[f]);
			}
		}
		MEM_freeN(och->ibufs_norm);
	}

	if (och->time)
		MEM_freeN(och->time);
	MEM_freeN(och);
}

void BKE_ocean_cache_eval_uv(struct OceanCache *och, struct OceanResult *ocr, int f, float u, float v)
{
	int res_x = och->resolution_x;
	int res_y = och->resolution_y;
	float result[4];

	u = fmod(u, 1.0);
	v = fmod(v, 1.0);

	if (u < 0) u += 1.0f;
	if (v < 0) v += 1.0f;

	if (och->ibufs_disp[f]) {
		ibuf_sample(och->ibufs_disp[f], u, v, (1.0f / (float)res_x), (1.0f / (float)res_y), result);
		copy_v3_v3(ocr->disp, result);
	}

	if (och->ibufs_foam[f]) {
		ibuf_sample(och->ibufs_foam[f], u, v, (1.0f / (float)res_x), (1.0f / (float)res_y), result);
		ocr->foam = result[0];
	}

	if (och->ibufs_norm[f]) {
		ibuf_sample(och->ibufs_norm[f], u, v, (1.0f / (float)res_x), (1.0f / (float)res_y), result);
		copy_v3_v3(ocr->normal, result);
	}
}

void BKE_ocean_cache_eval_ij(struct OceanCache *och, struct OceanResult *ocr, int f, int i, int j)
{
	const int res_x = och->resolution_x;
	const int res_y = och->resolution_y;

	if (i < 0) i = -i;
	if (j < 0) j = -j;

	i = i % res_x;
	j = j % res_y;

	if (och->ibufs_disp[f]) {
		copy_v3_v3(ocr->disp, &och->ibufs_disp[f]->rect_float[4 * (res_x * j + i)]);
	}

	if (och->ibufs_foam[f]) {
		ocr->foam = och->ibufs_foam[f]->rect_float[4 * (res_x * j + i)];
	}

	if (och->ibufs_norm[f]) {
		copy_v3_v3(ocr->normal, &och->ibufs_norm[f]->rect_float[4 * (res_x * j + i)]);
	}
}

struct OceanCache *BKE_init_ocean_cache(const char *bakepath, const char *relbase, int start, int end, float wave_scale,
                                        float chop_amount, float foam_coverage, float foam_fade, int resolution)
{
	OceanCache *och = MEM_callocN(sizeof(OceanCache), "ocean cache data");

	och->bakepath = bakepath;
	och->relbase = relbase;

	och->start = start;
	och->end = end;
	och->duration = (end - start) + 1;
	och->wave_scale = wave_scale;
	och->chop_amount = chop_amount;
	och->foam_coverage = foam_coverage;
	och->foam_fade = foam_fade;
	och->resolution_x = resolution * resolution;
	och->resolution_y = resolution * resolution;

	och->ibufs_disp = MEM_callocN(sizeof(ImBuf *) * och->duration, "displacement imbuf pointer array");
	och->ibufs_foam = MEM_callocN(sizeof(ImBuf *) * och->duration, "foam imbuf pointer array");
	och->ibufs_norm = MEM_callocN(sizeof(ImBuf *) * och->duration, "normal imbuf pointer array");

	och->time = NULL;

	return och;
}

void BKE_simulate_ocean_cache(struct OceanCache *och, int frame)
{
	char string[FILE_MAX];
	int f = frame;

	/* ibufs array is zero based, but filenames are based on frame numbers */
	/* still need to clamp frame numbers to valid range of images on disk though */
	CLAMP(frame, och->start, och->end);
	f = frame - och->start; /* shift to 0 based */

	/* if image is already loaded in mem, return */
	if (och->ibufs_disp[f] != NULL) return;

	/* use default color spaces since we know for sure cache files were saved with default settings too */

	cache_filename(string, och->bakepath, och->relbase, frame, CACHE_TYPE_DISPLACE);
	och->ibufs_disp[f] = IMB_loadiffname(string, 0, NULL);
#if 0
	if (och->ibufs_disp[f] == NULL)
		printf("error loading %s\n", string);
	else
		printf("loaded cache %s\n", string);
#endif

	cache_filename(string, och->bakepath, och->relbase, frame, CACHE_TYPE_FOAM);
	och->ibufs_foam[f] = IMB_loadiffname(string, 0, NULL);
#if 0
	if (och->ibufs_foam[f] == NULL)
		printf("error loading %s\n", string);
	else
		printf("loaded cache %s\n", string);
#endif

	cache_filename(string, och->bakepath, och->relbase, frame, CACHE_TYPE_NORMAL);
	och->ibufs_norm[f] = IMB_loadiffname(string, 0, NULL);
#if 0
	if (och->ibufs_norm[f] == NULL)
		printf("error loading %s\n", string);
	else
		printf("loaded cache %s\n", string);
#endif
}


void BKE_bake_ocean(struct Ocean *o, struct OceanCache *och, void (*update_cb)(void *, float progress, int *cancel),
                    void *update_cb_data)
{
	/* note: some of these values remain uninitialized unless certain options
	 * are enabled, take care that BKE_ocean_eval_ij() initializes a member
	 * before use - campbell */
	OceanResult ocr;

	ImageFormatData imf = {0};

	int f, i = 0, x, y, cancel = 0;
	float progress;

	ImBuf *ibuf_foam, *ibuf_disp, *ibuf_normal;
	float *prev_foam;
	int res_x = och->resolution_x;
	int res_y = och->resolution_y;
	char string[FILE_MAX];
	//RNG *rng;

	if (!o) return;

	if (o->_do_jacobian) prev_foam = MEM_callocN(res_x * res_y * sizeof(float), "previous frame foam bake data");
	else prev_foam = NULL;

	//rng = BLI_rng_new(0);

	/* setup image format */
	imf.imtype = R_IMF_IMTYPE_OPENEXR;
	imf.depth =  R_IMF_CHAN_DEPTH_16;
	imf.exr_codec = R_IMF_EXR_CODEC_ZIP;

	for (f = och->start, i = 0; f <= och->end; f++, i++) {

		/* create a new imbuf to store image for this frame */
		ibuf_foam = IMB_allocImBuf(res_x, res_y, 32, IB_rectfloat);
		ibuf_disp = IMB_allocImBuf(res_x, res_y, 32, IB_rectfloat);
		ibuf_normal = IMB_allocImBuf(res_x, res_y, 32, IB_rectfloat);

		BKE_simulate_ocean(o, och->time[i], och->wave_scale, och->chop_amount);

		/* add new foam */
		for (y = 0; y < res_y; y++) {
			for (x = 0; x < res_x; x++) {

				BKE_ocean_eval_ij(o, &ocr, x, y);

				/* add to the image */
				rgb_to_rgba_unit_alpha(&ibuf_disp->rect_float[4 * (res_x * y + x)], ocr.disp);

				if (o->_do_jacobian) {
					/* TODO, cleanup unused code - campbell */

					float /*r, */ /* UNUSED */ pr = 0.0f, foam_result;
					float neg_disp, neg_eplus;

					ocr.foam = BKE_ocean_jminus_to_foam(ocr.Jminus, och->foam_coverage);

					/* accumulate previous value for this cell */
					if (i > 0) {
						pr = prev_foam[res_x * y + x];
					}

					/* r = BLI_rng_get_float(rng); */ /* UNUSED */ /* randomly reduce foam */

					/* pr = pr * och->foam_fade; */		/* overall fade */

					/* remember ocean coord sys is Y up!
					 * break up the foam where height (Y) is low (wave valley), and X and Z displacement is greatest
					 */

#if 0
					vec[0] = ocr.disp[0];
					vec[1] = ocr.disp[2];
					hor_stretch = len_v2(vec);
					CLAMP(hor_stretch, 0.0, 1.0);
#endif

					neg_disp = ocr.disp[1] < 0.0f ? 1.0f + ocr.disp[1] : 1.0f;
					neg_disp = neg_disp < 0.0f ? 0.0f : neg_disp;

					/* foam, 'ocr.Eplus' only initialized with do_jacobian */
					neg_eplus = ocr.Eplus[2] < 0.0f ? 1.0f + ocr.Eplus[2] : 1.0f;
					neg_eplus = neg_eplus < 0.0f ? 0.0f : neg_eplus;

#if 0
					if (ocr.disp[1] < 0.0 || r > och->foam_fade)
						pr *= och->foam_fade;


					pr = pr * (1.0 - hor_stretch) * ocr.disp[1];
					pr = pr * neg_disp * neg_eplus;
#endif

					if (pr < 1.0f)
						pr *= pr;

					pr *= och->foam_fade * (0.75f + neg_eplus * 0.25f);

					/* A full clamping should not be needed! */
					foam_result = min_ff(pr + ocr.foam, 1.0f);

					prev_foam[res_x * y + x] = foam_result;

					/*foam_result = min_ff(foam_result, 1.0f); */

					value_to_rgba_unit_alpha(&ibuf_foam->rect_float[4 * (res_x * y + x)], foam_result);
				}

				if (o->_do_normals) {
					rgb_to_rgba_unit_alpha(&ibuf_normal->rect_float[4 * (res_x * y + x)], ocr.normal);
				}
			}
		}

		/* write the images */
		cache_filename(string, och->bakepath, och->relbase, f, CACHE_TYPE_DISPLACE);
		if (0 == BKE_imbuf_write(ibuf_disp, string, &imf))
			printf("Cannot save Displacement File Output to %s\n", string);

		if (o->_do_jacobian) {
			cache_filename(string, och->bakepath, och->relbase, f, CACHE_TYPE_FOAM);
			if (0 == BKE_imbuf_write(ibuf_foam, string, &imf))
				printf("Cannot save Foam File Output to %s\n", string);
		}

		if (o->_do_normals) {
			cache_filename(string, och->bakepath, och->relbase, f, CACHE_TYPE_NORMAL);
			if (0 == BKE_imbuf_write(ibuf_normal, string, &imf))
				printf("Cannot save Normal File Output to %s\n", string);
		}

		IMB_freeImBuf(ibuf_disp);
		IMB_freeImBuf(ibuf_foam);
		IMB_freeImBuf(ibuf_normal);

		progress = (f - och->start) / (float)och->duration;

		update_cb(update_cb_data, progress, &cancel);

		if (cancel) {
			if (prev_foam) MEM_freeN(prev_foam);
			//BLI_rng_free(rng);
			return;
		}
	}

	//BLI_rng_free(rng);
	if (prev_foam) MEM_freeN(prev_foam);
	och->baked = 1;
}

#else /* WITH_OCEANSIM */

/* stub */
typedef struct Ocean {
	/* need some data here, C does not allow empty struct */
	int stub;
} Ocean;


float BKE_ocean_jminus_to_foam(float UNUSED(jminus), float UNUSED(coverage))
{
	return 0.0f;
}

void BKE_ocean_eval_uv(struct Ocean *UNUSED(oc), struct OceanResult *UNUSED(ocr), float UNUSED(u), float UNUSED(v))
{
}

/* use catmullrom interpolation rather than linear */
void BKE_ocean_eval_uv_catrom(struct Ocean *UNUSED(oc), struct OceanResult *UNUSED(ocr), float UNUSED(u),
                              float UNUSED(v))
{
}

void BKE_ocean_eval_xz(struct Ocean *UNUSED(oc), struct OceanResult *UNUSED(ocr), float UNUSED(x), float UNUSED(z))
{
}

void BKE_ocean_eval_xz_catrom(struct Ocean *UNUSED(oc), struct OceanResult *UNUSED(ocr), float UNUSED(x),
                              float UNUSED(z))
{
}

void BKE_ocean_eval_ij(struct Ocean *UNUSED(oc), struct OceanResult *UNUSED(ocr), int UNUSED(i), int UNUSED(j))
{
}

void BKE_simulate_ocean(struct Ocean *UNUSED(o), float UNUSED(t), float UNUSED(scale), float UNUSED(chop_amount))
{
}

struct Ocean *BKE_add_ocean(void)
{
	Ocean *oc = MEM_callocN(sizeof(Ocean), "ocean sim data");

	return oc;
}

void BKE_init_ocean(struct Ocean *UNUSED(o), int UNUSED(M), int UNUSED(N), float UNUSED(Lx), float UNUSED(Lz),
                    float UNUSED(V), float UNUSED(l), float UNUSED(A), float UNUSED(w), float UNUSED(damp),
                    float UNUSED(alignment), float UNUSED(depth), float UNUSED(time), short UNUSED(do_height_field),
                    short UNUSED(do_chop), short UNUSED(do_normals), short UNUSED(do_jacobian), int UNUSED(seed))
{
}

void BKE_free_ocean_data(struct Ocean *UNUSED(oc))
{
}

void BKE_free_ocean(struct Ocean *oc)
{
	if (!oc) return;
	MEM_freeN(oc);
}


/* ********* Baking/Caching ********* */


void BKE_free_ocean_cache(struct OceanCache *och)
{
	if (!och) return;

	MEM_freeN(och);
}

void BKE_ocean_cache_eval_uv(struct OceanCache *UNUSED(och), struct OceanResult *UNUSED(ocr), int UNUSED(f),
                             float UNUSED(u), float UNUSED(v))
{
}

void BKE_ocean_cache_eval_ij(struct OceanCache *UNUSED(och), struct OceanResult *UNUSED(ocr), int UNUSED(f),
                             int UNUSED(i), int UNUSED(j))
{
}

OceanCache *BKE_init_ocean_cache(const char *UNUSED(bakepath), const char *UNUSED(relbase), int UNUSED(start),
                                 int UNUSED(end), float UNUSED(wave_scale), float UNUSED(chop_amount),
                                 float UNUSED(foam_coverage), float UNUSED(foam_fade), int UNUSED(resolution))
{
	OceanCache *och = MEM_callocN(sizeof(OceanCache), "ocean cache data");

	return och;
}

void BKE_simulate_ocean_cache(struct OceanCache *UNUSED(och), int UNUSED(frame))
{
}

void BKE_bake_ocean(struct Ocean *UNUSED(o), struct OceanCache *UNUSED(och),
                    void (*update_cb)(void *, float progress, int *cancel), void *UNUSED(update_cb_data))
{
	/* unused */
	(void)update_cb;
}
#endif /* WITH_OCEANSIM */
