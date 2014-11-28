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
 * Contributor(s): eeshlo, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/mathutils/mathutils_noise.c
 *  \ingroup mathutils
 *
 * This file defines the 'noise' module, a general purpose module to access
 * blenders noise functions.
 */


/************************/
/* Blender Noise Module */
/************************/

#include <Python.h>

#include "BLI_math.h"
#include "BLI_noise.h"
#include "BLI_utildefines.h"

#include "DNA_texture_types.h"

#include "mathutils.h"
#include "mathutils_noise.h"

/* 2.6 update
 * Moved to submodule of mathutils.
 * All vector functions now return mathutils.Vector
 * Updated docs to be compatible with autodocs generation.
 * Updated vector functions to use nD array functions.
 * noise.vl_vector --> noise.variable_lacunarity
 * noise.vector --> noise.noise_vector
 */

/*-----------------------------------------*/
/* 'mersenne twister' random number generator */

/*
 * A C-program for MT19937, with initialization improved 2002/2/10.
 * Coded by Takuji Nishimura and Makoto Matsumoto.
 * This is a faster version by taking Shawn Cokus's optimization,
 * Matthe Bellew's simplification, Isaku Wada's real version.
 *
 * Before using, initialize the state by using init_genrand(seed) 
 * or init_by_array(init_key, key_length).
 *
 * Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
 * All rights reserved.                          
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. The names of its contributors may not be used to endorse or promote 
 *      products derived from this software without specific prior written 
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Any feedback is very welcome.
 * http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
 * email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
 */

/* Period parameters */
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UMASK 0x80000000UL  /* most significant w-r bits */
#define LMASK 0x7fffffffUL  /* least significant r bits */
#define MIXBITS(u, v) (((u) & UMASK) | ((v) & LMASK))
#define TWIST(u, v) ((MIXBITS(u, v) >> 1) ^ ((v) & 1UL ? MATRIX_A : 0UL))

static unsigned long state[N];  /* the array for the state vector  */
static int left = 1;
static int initf = 0;
static unsigned long *next;

/* initializes state[N] with a seed */
static void init_genrand(unsigned long s)
{
	int j;
	state[0] = s & 0xffffffffUL;
	for (j = 1; j < N; j++) {
		state[j] =
		    (1812433253UL *
		     (state[j - 1] ^ (state[j - 1] >> 30)) + j);
		/* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
		/* In the previous versions, MSBs of the seed affect   */
		/* only MSBs of the array state[].                        */
		/* 2002/01/09 modified by Makoto Matsumoto             */
		state[j] &= 0xffffffffUL;   /* for >32 bit machines */
	}
	left = 1;
	initf = 1;
}

static void next_state(void)
{
	unsigned long *p = state;
	int j;

	/* if init_genrand() has not been called, */
	/* a default initial seed is used         */
	if (initf == 0)
		init_genrand(5489UL);

	left = N;
	next = state;

	for (j = N - M + 1; --j; p++)
		*p = p[M] ^ TWIST(p[0], p[1]);

	for (j = M; --j; p++)
		*p = p[M - N] ^ TWIST(p[0], p[1]);

	*p = p[M - N] ^ TWIST(p[0], state[0]);
}

/*------------------------------------------------------------*/

static void setRndSeed(int seed)
{
	if (seed == 0)
		init_genrand(time(NULL));
	else
		init_genrand(seed);
}

/* float number in range [0, 1) using the mersenne twister rng */
static float frand(void)
{
	unsigned long y;

	if (--left == 0)
		next_state();
	y = *next++;

	/* Tempering */
	y ^= (y >> 11);
	y ^= (y << 7) & 0x9d2c5680UL;
	y ^= (y << 15) & 0xefc60000UL;
	y ^= (y >> 18);

	return (float) y / 4294967296.f;
}

/*------------------------------------------------------------*/
/* Utility Functions */
/*------------------------------------------------------------*/

/* Fills an array of length size with random numbers in the range (-1, 1)*/
static void rand_vn(float *array_tar, const int size)
{
	float *array_pt = array_tar + (size - 1);
	int i = size;
	while (i--) { *(array_pt--) = 2.0f * frand() - 1.0f; }
}

/* Fills an array of length 3 with noise values */
static void noise_vector(float x, float y, float z, int nb, float v[3])
{
	/* Simply evaluate noise at 3 different positions */
	v[0] = (float)(2.0f * BLI_gNoise(1.f, x + 9.321f, y - 1.531f, z - 7.951f, 0, nb) - 1.0f);
	v[1] = (float)(2.0f * BLI_gNoise(1.f, x, y, z, 0, nb) - 1.0f);
	v[2] = (float)(2.0f * BLI_gNoise(1.f, x + 6.327f, y + 0.1671f, z - 2.672f, 0, nb) - 1.0f);
}

/* Returns a turbulence value for a given position (x, y, z) */
static float turb(float x, float y, float z, int oct, int hard, int nb,
                  float ampscale, float freqscale)
{
	float amp, out, t;
	int i;
	amp = 1.f;
	out = (float)(2.0f * BLI_gNoise(1.f, x, y, z, 0, nb) - 1.0f);
	if (hard)
		out = fabsf(out);
	for (i = 1; i < oct; i++) {
		amp *= ampscale;
		x *= freqscale;
		y *= freqscale;
		z *= freqscale;
		t = (float)(amp * (2.0f * BLI_gNoise(1.f, x, y, z, 0, nb) - 1.0f));
		if (hard)
			t = fabsf(t);
		out += t;
	}
	return out;
}

/* Fills an array of length 3 with the turbulence vector for a given
 * position (x, y, z) */
static void vTurb(float x, float y, float z, int oct, int hard, int nb,
                  float ampscale, float freqscale, float v[3])
{
	float amp, t[3];
	int i;
	amp = 1.f;
	noise_vector(x, y, z, nb, v);
	if (hard) {
		v[0] = fabsf(v[0]);
		v[1] = fabsf(v[1]);
		v[2] = fabsf(v[2]);
	}
	for (i = 1; i < oct; i++) {
		amp *= ampscale;
		x *= freqscale;
		y *= freqscale;
		z *= freqscale;
		noise_vector(x, y, z, nb, t);
		if (hard) {
			t[0] = fabsf(t[0]);
			t[1] = fabsf(t[1]);
			t[2] = fabsf(t[2]);
		}
		v[0] += amp * t[0];
		v[1] += amp * t[1];
		v[2] += amp * t[2];
	}
}

/*-------------------------DOC STRINGS ---------------------------*/
PyDoc_STRVAR(M_Noise_doc,
"The Blender noise module"
);

/*------------------------------------------------------------*/
/* Python Functions */
/*------------------------------------------------------------*/

PyDoc_STRVAR(M_Noise_random_doc,
".. function:: random()\n"
"\n"
"   Returns a random number in the range [0, 1].\n"
"\n"
"   :return: The random number.\n"
"   :rtype: float\n"
);
static PyObject *M_Noise_random(PyObject *UNUSED(self))
{
	return PyFloat_FromDouble(frand());
}

PyDoc_STRVAR(M_Noise_random_unit_vector_doc,
".. function:: random_unit_vector(size=3)\n"
"\n"
"   Returns a unit vector with random entries.\n"
"\n"
"   :arg size: The size of the vector to be produced.\n"
"   :type size: Int\n"
"   :return: The random unit vector.\n"
"   :rtype: :class:`mathutils.Vector`\n"
);
static PyObject *M_Noise_random_unit_vector(PyObject *UNUSED(self), PyObject *args)
{
	float vec[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float norm = 2.0f;
	int size = 3;

	if (!PyArg_ParseTuple(args, "|i:random_vector", &size))
		return NULL;

	if (size > 4 || size < 2) {
		PyErr_SetString(PyExc_ValueError, "Vector(): invalid size");
		return NULL;
	}

	while (norm == 0.0f || norm >= 1.0f) {
		rand_vn(vec, size);
		norm = normalize_vn(vec, size);
	}

	return Vector_CreatePyObject(vec, size, Py_NEW, NULL);
}
/* This is dumb, most people will want a unit vector anyway, since this doesn't have uniform distribution over a sphere*/
#if 0
PyDoc_STRVAR(M_Noise_random_vector_doc,
".. function:: random_vector(size=3)\n"
"\n"
"   Returns a vector with random entries in the range [0, 1).\n"
"\n"
"   :arg size: The size of the vector to be produced.\n"
"   :type size: Int\n"
"   :return: The random vector.\n"
"   :rtype: :class:`mathutils.Vector`\n"
);
static PyObject *M_Noise_random_vector(PyObject *UNUSED(self), PyObject *args)
{
	float vec[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	int size = 3;

	if (!PyArg_ParseTuple(args, "|i:random_vector", &size))
		return NULL;

	if (size > 4 || size < 2) {
		PyErr_SetString(PyExc_ValueError, "Vector(): invalid size");
		return NULL;
	}

	rand_vn(vec, size);

	return Vector_CreatePyObject(vec, size, Py_NEW, NULL);
}
#endif

PyDoc_STRVAR(M_Noise_seed_set_doc,
".. function:: seed_set(seed)\n"
"\n"
"   Sets the random seed used for random_unit_vector, random_vector and random.\n"
"\n"
"   :arg seed: Seed used for the random generator.\n"
"      When seed is zero, the current time will be used instead.\n"
"   :type seed: Int\n"
);
static PyObject *M_Noise_seed_set(PyObject *UNUSED(self), PyObject *args)
{
	int s;
	if (!PyArg_ParseTuple(args, "i:seed_set", &s))
		return NULL;
	setRndSeed(s);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(M_Noise_noise_doc,
".. function:: noise(position, noise_basis=noise.types.STDPERLIN)\n"
"\n"
"   Returns noise value from the noise basis at the position specified.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :arg noise_basis: The type of noise to be evaluated.\n"
"   :type noise_basis: Value in noise.types or int\n"
"   :return: The noise value.\n"
"   :rtype: float\n"
);
static PyObject *M_Noise_noise(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3];
	int nb = 1;
	if (!PyArg_ParseTuple(args, "O|i:noise", &value, &nb))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "noise: invalid 'position' arg") == -1)
		return NULL;

	return PyFloat_FromDouble((2.0f * BLI_gNoise(1.0f, vec[0], vec[1], vec[2], 0, nb) - 1.0f));
}

PyDoc_STRVAR(M_Noise_noise_vector_doc,
".. function:: noise_vector(position, noise_basis=noise.types.STDPERLIN)\n"
"\n"
"   Returns the noise vector from the noise basis at the specified position.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :arg noise_basis: The type of noise to be evaluated.\n"
"   :type noise_basis: Value in noise.types or int\n"
"   :return: The noise vector.\n"
"   :rtype: :class:`mathutils.Vector`\n"
);
static PyObject *M_Noise_noise_vector(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3], r_vec[3];
	int nb = 1;

	if (!PyArg_ParseTuple(args, "O|i:noise_vector", &value, &nb))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "noise_vector: invalid 'position' arg") == -1)
		return NULL;

	noise_vector(vec[0], vec[1], vec[2], nb, r_vec);

	return Vector_CreatePyObject(r_vec, 3, Py_NEW, NULL);
}

PyDoc_STRVAR(M_Noise_turbulence_doc,
".. function:: turbulence(position, octaves, hard, noise_basis=noise.types.STDPERLIN, amplitude_scale=0.5, frequency_scale=2.0)\n"
"\n"
"   Returns the turbulence value from the noise basis at the specified position.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :arg octaves: The number of different noise frequencies used.\n"
"   :type octaves: int\n"
"   :arg hard: Specifies whether returned turbulence is hard (sharp transitions) or soft (smooth transitions).\n"
"   :type hard: :boolean\n"
"   :arg noise_basis: The type of noise to be evaluated.\n"
"   :type noise_basis: Value in mathutils.noise.types or int\n"
"   :arg amplitude_scale: The amplitude scaling factor.\n"
"   :type amplitude_scale: float\n"
"   :arg frequency_scale: The frequency scaling factor\n"
"   :type frequency_scale: Value in noise.types or int\n"
"   :return: The turbulence value.\n"
"   :rtype: float\n"
);
static PyObject *M_Noise_turbulence(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3];
	int oct, hd, nb = 1;
	float as = 0.5f, fs = 2.0f;

	if (!PyArg_ParseTuple(args, "Oii|iff:turbulence", &value, &oct, &hd, &nb, &as, &fs))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "turbulence: invalid 'position' arg") == -1)
		return NULL;

	return PyFloat_FromDouble(turb(vec[0], vec[1], vec[2], oct, hd, nb, as, fs));
}

PyDoc_STRVAR(M_Noise_turbulence_vector_doc,
".. function:: turbulence_vector(position, octaves, hard, noise_basis=noise.types.STDPERLIN, amplitude_scale=0.5, frequency_scale=2.0)\n"
"\n"
"   Returns the turbulence vector from the noise basis at the specified position.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :arg octaves: The number of different noise frequencies used.\n"
"   :type octaves: int\n"
"   :arg hard: Specifies whether returned turbulence is hard (sharp transitions) or soft (smooth transitions).\n"
"   :type hard: :boolean\n"
"   :arg noise_basis: The type of noise to be evaluated.\n"
"   :type noise_basis: Value in mathutils.noise.types or int\n"
"   :arg amplitude_scale: The amplitude scaling factor.\n"
"   :type amplitude_scale: float\n"
"   :arg frequency_scale: The frequency scaling factor\n"
"   :type frequency_scale: Value in noise.types or int\n"
"   :return: The turbulence vector.\n"
"   :rtype: :class:`mathutils.Vector`\n"
);
static PyObject *M_Noise_turbulence_vector(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3], r_vec[3];
	int oct, hd, nb = 1;
	float as = 0.5f, fs = 2.0f;
	if (!PyArg_ParseTuple(args, "Oii|iff:turbulence_vector", &value, &oct, &hd, &nb, &as, &fs))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "turbulence_vector: invalid 'position' arg") == -1)
		return NULL;

	vTurb(vec[0], vec[1], vec[2], oct, hd, nb, as, fs, r_vec);
	return Vector_CreatePyObject(r_vec, 3, Py_NEW, NULL);
}

/* F. Kenton Musgrave's fractal functions */
PyDoc_STRVAR(M_Noise_fractal_doc,
".. function:: fractal(position, H, lacunarity, octaves, noise_basis=noise.types.STDPERLIN)\n"
"\n"
"   Returns the fractal Brownian motion (fBm) noise value from the noise basis at the specified position.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :arg H: The fractal increment factor.\n"
"   :type H: float\n"
"   :arg lacunarity: The gap between successive frequencies.\n"
"   :type lacunarity: float\n"
"   :arg octaves: The number of different noise frequencies used.\n"
"   :type octaves: int\n"
"   :arg noise_basis: The type of noise to be evaluated.\n"
"   :type noise_basis: Value in noise.types or int\n"
"   :return: The fractal Brownian motion noise value.\n"
"   :rtype: float\n"
);
static PyObject *M_Noise_fractal(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3];
	float H, lac, oct;
	int nb = 1;

	if (!PyArg_ParseTuple(args, "Offf|i:fractal", &value, &H, &lac, &oct, &nb))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "fractal: invalid 'position' arg") == -1)
		return NULL;

	return PyFloat_FromDouble(mg_fBm(vec[0], vec[1], vec[2], H, lac, oct, nb));
}

PyDoc_STRVAR(M_Noise_multi_fractal_doc,
".. function:: multi_fractal(position, H, lacunarity, octaves, noise_basis=noise.types.STDPERLIN)\n"
"\n"
"   Returns multifractal noise value from the noise basis at the specified position.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :arg H: The fractal increment factor.\n"
"   :type H: float\n"
"   :arg lacunarity: The gap between successive frequencies.\n"
"   :type lacunarity: float\n"
"   :arg octaves: The number of different noise frequencies used.\n"
"   :type octaves: int\n"
"   :arg noise_basis: The type of noise to be evaluated.\n"
"   :type noise_basis: Value in noise.types or int\n"
"   :return: The multifractal noise value.\n"
"   :rtype: float\n"
);
static PyObject *M_Noise_multi_fractal(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3];
	float H, lac, oct;
	int nb = 1;

	if (!PyArg_ParseTuple(args, "Offf|i:multi_fractal", &value, &H, &lac, &oct, &nb))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "multi_fractal: invalid 'position' arg") == -1)
		return NULL;

	return PyFloat_FromDouble(mg_MultiFractal(vec[0], vec[1], vec[2], H, lac, oct, nb));
}

PyDoc_STRVAR(M_Noise_variable_lacunarity_doc,
".. function:: variable_lacunarity(position, distortion, noise_type1=noise.types.STDPERLIN, noise_type2=noise.types.STDPERLIN)\n"
"\n"
"   Returns variable lacunarity noise value, a distorted variety of noise, from noise type 1 distorted by noise type 2 at the specified position.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :arg distortion: The amount of distortion.\n"
"   :type distortion: float\n"
"   :arg noise_type1: The type of noise to be distorted.\n"
"   :type noise_type1: Value in noise.types or int\n"
"   :arg noise_type2: The type of noise used to distort noise_type1.\n"
"   :type noise_type2: Value in noise.types or int\n"
"   :return: The variable lacunarity noise value.\n"
"   :rtype: float\n"
);
static PyObject *M_Noise_variable_lacunarity(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3];
	float d;
	int nt1 = 1, nt2 = 1;

	if (!PyArg_ParseTuple(args, "Of|ii:variable_lacunarity", &value, &d, &nt1, &nt2))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "variable_lacunarity: invalid 'position' arg") == -1)
		return NULL;

	return PyFloat_FromDouble(mg_VLNoise(vec[0], vec[1], vec[2], d, nt1, nt2));
}

PyDoc_STRVAR(M_Noise_hetero_terrain_doc,
".. function:: hetero_terrain(position, H, lacunarity, octaves, offset, noise_basis=noise.types.STDPERLIN)\n"
"\n"
"   Returns the heterogeneous terrain value from the noise basis at the specified position.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :arg H: The fractal dimension of the roughest areas.\n"
"   :type H: float\n"
"   :arg lacunarity: The gap between successive frequencies.\n"
"   :type lacunarity: float\n"
"   :arg octaves: The number of different noise frequencies used.\n"
"   :type octaves: int\n"
"   :arg offset: The height of the terrain above 'sea level'.\n"
"   :type offset: float\n"
"   :arg noise_basis: The type of noise to be evaluated.\n"
"   :type noise_basis: Value in noise.types or int\n"
"   :return: The heterogeneous terrain value.\n"
"   :rtype: float\n"
);
static PyObject *M_Noise_hetero_terrain(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3];
	float H, lac, oct, ofs;
	int nb = 1;

	if (!PyArg_ParseTuple(args, "Offff|i:hetero_terrain", &value, &H, &lac, &oct, &ofs, &nb))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "hetero_terrain: invalid 'position' arg") == -1)
		return NULL;

	return PyFloat_FromDouble(mg_HeteroTerrain(vec[0], vec[1], vec[2], H, lac, oct, ofs, nb));
}

PyDoc_STRVAR(M_Noise_hybrid_multi_fractal_doc,
".. function:: hybrid_multi_fractal(position, H, lacunarity, octaves, offset, gain, noise_basis=noise.types.STDPERLIN)\n"
"\n"
"   Returns hybrid multifractal value from the noise basis at the specified position.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :arg H: The fractal dimension of the roughest areas.\n"
"   :type H: float\n"
"   :arg lacunarity: The gap between successive frequencies.\n"
"   :type lacunarity: float\n"
"   :arg octaves: The number of different noise frequencies used.\n"
"   :type octaves: int\n"
"   :arg offset: The height of the terrain above 'sea level'.\n"
"   :type offset: float\n"
"   :arg gain: Scaling applied to the values.\n"
"   :type gain: float\n"
"   :arg noise_basis: The type of noise to be evaluated.\n"
"   :type noise_basis: Value in noise.types or int\n"
"   :return: The hybrid multifractal value.\n"
"   :rtype: float\n"
);
static PyObject *M_Noise_hybrid_multi_fractal(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3];
	float H, lac, oct, ofs, gn;
	int nb = 1;

	if (!PyArg_ParseTuple(args, "Offfff|i:hybrid_multi_fractal", &value, &H, &lac, &oct, &ofs, &gn, &nb))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "hybrid_multi_fractal: invalid 'position' arg") == -1)
		return NULL;
	
	return PyFloat_FromDouble(mg_HybridMultiFractal(vec[0], vec[1], vec[2], H, lac, oct, ofs, gn, nb));
}

PyDoc_STRVAR(M_Noise_ridged_multi_fractal_doc,
".. function:: ridged_multi_fractal(position, H, lacunarity, octaves, offset, gain, noise_basis=noise.types.STDPERLIN)\n"
"\n"
"   Returns ridged multifractal value from the noise basis at the specified position.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :arg H: The fractal dimension of the roughest areas.\n"
"   :type H: float\n"
"   :arg lacunarity: The gap between successive frequencies.\n"
"   :type lacunarity: float\n"
"   :arg octaves: The number of different noise frequencies used.\n"
"   :type octaves: int\n"
"   :arg offset: The height of the terrain above 'sea level'.\n"
"   :type offset: float\n"
"   :arg gain: Scaling applied to the values.\n"
"   :type gain: float\n"
"   :arg noise_basis: The type of noise to be evaluated.\n"
"   :type noise_basis: Value in noise.types or int\n"
"   :return: The ridged multifractal value.\n"
"   :rtype: float\n"
);
static PyObject *M_Noise_ridged_multi_fractal(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3];
	float H, lac, oct, ofs, gn;
	int nb = 1;

	if (!PyArg_ParseTuple(args, "Offfff|i:ridged_multi_fractal", &value, &H, &lac, &oct, &ofs, &gn, &nb))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "ridged_multi_fractal: invalid 'position' arg") == -1)
		return NULL;

	return PyFloat_FromDouble(mg_RidgedMultiFractal(vec[0], vec[1], vec[2], H, lac, oct, ofs, gn, nb));
}

PyDoc_STRVAR(M_Noise_voronoi_doc,
".. function:: voronoi(position, distance_metric=noise.distance_metrics.DISTANCE, exponent=2.5)\n"
"\n"
"   Returns a list of distances to the four closest features and their locations.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :arg distance_metric: Method of measuring distance.\n"
"   :type distance_metric: Value in noise.distance_metrics or int\n"
"   :arg exponent: The exponent for Minkowski distance metric.\n"
"   :type exponent: float\n"
"   :return: A list of distances to the four closest features and their locations.\n"
"   :rtype: list of four floats, list of four :class:`mathutils.Vector` types\n"
);
static PyObject *M_Noise_voronoi(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	PyObject *list;
	float vec[3];
	float da[4], pa[12];
	int dtype = 0;
	float me = 2.5f;  /* default minkowski exponent */

	int i;

	if (!PyArg_ParseTuple(args, "O|if:voronoi", &value, &dtype, &me))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "voronoi: invalid 'position' arg") == -1)
		return NULL;

	list = PyList_New(4);

	voronoi(vec[0], vec[1], vec[2], da, pa, me, dtype);

	for (i = 0; i < 4; i++) {
		PyList_SET_ITEM(list, i, Vector_CreatePyObject(pa + 3 * i, 3, Py_NEW, NULL));
	}

	return Py_BuildValue("[[ffff]O]", da[0], da[1], da[2], da[3], list);
}

PyDoc_STRVAR(M_Noise_cell_doc,
".. function:: cell(position)\n"
"\n"
"   Returns cell noise value at the specified position.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :return: The cell noise value.\n"
"   :rtype: float\n"
);
static PyObject *M_Noise_cell(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3];

	if (!PyArg_ParseTuple(args, "O:cell", &value))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "cell: invalid 'position' arg") == -1)
		return NULL;

	return PyFloat_FromDouble(cellNoise(vec[0], vec[1], vec[2]));
}

PyDoc_STRVAR(M_Noise_cell_vector_doc,
".. function:: cell_vector(position)\n"
"\n"
"   Returns cell noise vector at the specified position.\n"
"\n"
"   :arg position: The position to evaluate the selected noise function at.\n"
"   :type position: :class:`mathutils.Vector`\n"
"   :return: The cell noise vector.\n"
"   :rtype: :class:`mathutils.Vector`\n"
);
static PyObject *M_Noise_cell_vector(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	float vec[3], r_vec[3];

	if (!PyArg_ParseTuple(args, "O:cell_vector", &value))
		return NULL;

	if (mathutils_array_parse(vec, 3, 3, value, "cell_vector: invalid 'position' arg") == -1)
		return NULL;

	cellNoiseV(vec[0], vec[1], vec[2], r_vec);
	return Vector_CreatePyObject(r_vec, 3, Py_NEW, NULL);
}

static PyMethodDef M_Noise_methods[] = {
	{"seed_set", (PyCFunction) M_Noise_seed_set, METH_VARARGS, M_Noise_seed_set_doc},
	{"random", (PyCFunction) M_Noise_random, METH_NOARGS, M_Noise_random_doc},
	{"random_unit_vector", (PyCFunction) M_Noise_random_unit_vector, METH_VARARGS, M_Noise_random_unit_vector_doc},
	/*{"random_vector", (PyCFunction) M_Noise_random_vector, METH_VARARGS, M_Noise_random_vector_doc},*/
	{"noise", (PyCFunction) M_Noise_noise, METH_VARARGS, M_Noise_noise_doc},
	{"noise_vector", (PyCFunction) M_Noise_noise_vector, METH_VARARGS, M_Noise_noise_vector_doc},
	{"turbulence", (PyCFunction) M_Noise_turbulence, METH_VARARGS, M_Noise_turbulence_doc},
	{"turbulence_vector", (PyCFunction) M_Noise_turbulence_vector, METH_VARARGS, M_Noise_turbulence_vector_doc},
	{"fractal", (PyCFunction) M_Noise_fractal, METH_VARARGS, M_Noise_fractal_doc},
	{"multi_fractal", (PyCFunction) M_Noise_multi_fractal, METH_VARARGS, M_Noise_multi_fractal_doc},
	{"variable_lacunarity", (PyCFunction) M_Noise_variable_lacunarity, METH_VARARGS, M_Noise_variable_lacunarity_doc},
	{"hetero_terrain", (PyCFunction) M_Noise_hetero_terrain, METH_VARARGS, M_Noise_hetero_terrain_doc},
	{"hybrid_multi_fractal", (PyCFunction) M_Noise_hybrid_multi_fractal, METH_VARARGS, M_Noise_hybrid_multi_fractal_doc},
	{"ridged_multi_fractal", (PyCFunction) M_Noise_ridged_multi_fractal, METH_VARARGS, M_Noise_ridged_multi_fractal_doc},
	{"voronoi", (PyCFunction) M_Noise_voronoi, METH_VARARGS, M_Noise_voronoi_doc},
	{"cell", (PyCFunction) M_Noise_cell, METH_VARARGS, M_Noise_cell_doc},
	{"cell_vector", (PyCFunction) M_Noise_cell_vector, METH_VARARGS, M_Noise_cell_vector_doc},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef M_Noise_module_def = {
	PyModuleDef_HEAD_INIT,
	"mathutils.noise",  /* m_name */
	M_Noise_doc,  /* m_doc */
	0,     /* m_size */
	M_Noise_methods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

/*----------------------------MODULE INIT-------------------------*/
PyMODINIT_FUNC PyInit_mathutils_noise(void)
{
	PyObject *submodule = PyModule_Create(&M_Noise_module_def);
	PyObject *item_types, *item_metrics;

	/* use current time as seed for random number generator by default */
	setRndSeed(0);

	PyModule_AddObject(submodule, "types", (item_types = PyInit_mathutils_noise_types()));
	PyDict_SetItemString(PyThreadState_GET()->interp->modules, "noise.types", item_types);
	Py_INCREF(item_types);

	PyModule_AddObject(submodule, "distance_metrics", (item_metrics = PyInit_mathutils_noise_metrics()));
	PyDict_SetItemString(PyThreadState_GET()->interp->modules, "noise.distance_metrics", item_metrics);
	Py_INCREF(item_metrics);

	return submodule;
}

/*----------------------------SUBMODULE INIT-------------------------*/
static struct PyModuleDef M_NoiseTypes_module_def = {
	PyModuleDef_HEAD_INIT,
	"mathutils.noise.types",  /* m_name */
	NULL,  /* m_doc */
	0,     /* m_size */
	NULL,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

PyMODINIT_FUNC PyInit_mathutils_noise_types(void)
{
	PyObject *submodule = PyModule_Create(&M_NoiseTypes_module_def);

	PyModule_AddIntConstant(submodule, "BLENDER", TEX_BLENDER);
	PyModule_AddIntConstant(submodule, "STDPERLIN", TEX_STDPERLIN);
	PyModule_AddIntConstant(submodule, "NEWPERLIN", TEX_NEWPERLIN);
	PyModule_AddIntConstant(submodule, "VORONOI_F1", TEX_VORONOI_F1);
	PyModule_AddIntConstant(submodule, "VORONOI_F2", TEX_VORONOI_F2);
	PyModule_AddIntConstant(submodule, "VORONOI_F3", TEX_VORONOI_F3);
	PyModule_AddIntConstant(submodule, "VORONOI_F4", TEX_VORONOI_F4);
	PyModule_AddIntConstant(submodule, "VORONOI_F2F1", TEX_VORONOI_F2F1);
	PyModule_AddIntConstant(submodule, "VORONOI_CRACKLE", TEX_VORONOI_CRACKLE);
	PyModule_AddIntConstant(submodule, "CELLNOISE", TEX_CELLNOISE);

	return submodule;
}

static struct PyModuleDef M_NoiseMetrics_module_def = {
	PyModuleDef_HEAD_INIT,
	"mathutils.noise.distance_metrics",  /* m_name */
	NULL,  /* m_doc */
	0,     /* m_size */
	NULL,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

PyMODINIT_FUNC PyInit_mathutils_noise_metrics(void)
{
	PyObject *submodule = PyModule_Create(&M_NoiseMetrics_module_def);

	PyModule_AddIntConstant(submodule, "DISTANCE", TEX_DISTANCE);
	PyModule_AddIntConstant(submodule, "DISTANCE_SQUARED", TEX_DISTANCE_SQUARED);
	PyModule_AddIntConstant(submodule, "MANHATTAN", TEX_MANHATTAN);
	PyModule_AddIntConstant(submodule, "CHEBYCHEV", TEX_CHEBYCHEV);
	PyModule_AddIntConstant(submodule, "MINKOVSKY_HALF", TEX_MINKOVSKY_HALF);
	PyModule_AddIntConstant(submodule, "MINKOVSKY_FOUR", TEX_MINKOVSKY_FOUR);
	PyModule_AddIntConstant(submodule, "MINKOVSKY", TEX_MINKOVSKY);

	return submodule;
}
