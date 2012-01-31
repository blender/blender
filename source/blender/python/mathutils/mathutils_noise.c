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
 * This is a new part of Blender.
 *
 * Contributor(s): eeshlo, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/generic/noise_py_api.c
 *  \ingroup pygen
 *
 * This file defines the 'noise' module, a general purpose module to access
 * blenders noise functions.
 */


/************************/
/* Blender Noise Module */
/************************/

#include <Python.h>

#include "structseq.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_texture_types.h"

#include "noise_py_api.h"

/*-----------------------------------------*/
/* 'mersenne twister' random number generator */

/* 
   A C-program for MT19937, with initialization improved 2002/2/10.
   Coded by Takuji Nishimura and Makoto Matsumoto.
   This is a faster version by taking Shawn Cokus's optimization,
   Matthe Bellew's simplification, Isaku Wada's real version.

   Before using, initialize the state by using init_genrand(seed) 
   or init_by_array(init_key, key_length).

   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.                          

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote 
        products derived from this software without specific prior written 
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


   Any feedback is very welcome.
   http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
   email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
*/

/* 2.5 update
 * Noise.setRandomSeed --> seed_set
 * Noise.randuvec --> random_unit_vector
 * Noise.vNoise --> noise_vector
 * Noise.vTurbulence --> turbulence_vector
 * Noise.multiFractal --> multi_fractal
 * Noise.cellNoise --> cell
 * Noise.cellNoiseV --> cell_vector
 * Noise.vlNoise --> vl_vector
 * Noise.heteroTerrain --> hetero_terrain
 * Noise.hybridMFractal --> hybrid_multi_fractal
 * Noise.fBm --> fractal
 * Noise.ridgedMFractal --> ridged_multi_fractal
 *
 * Const's *
 * Noise.NoiseTypes --> types
 * Noise.DistanceMetrics --> distance_metrics
 */

/* Period parameters */
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL	/* constant vector a */
#define UMASK 0x80000000UL	/* most significant w-r bits */
#define LMASK 0x7fffffffUL	/* least significant r bits */
#define MIXBITS(u,v) (((u) & UMASK) | ((v) & LMASK))
#define TWIST(u,v) ((MIXBITS(u,v) >> 1) ^ ((v)&1UL ? MATRIX_A : 0UL))

static unsigned long state[N];	/* the array for the state vector  */
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
		state[j] &= 0xffffffffUL;	/* for >32 bit machines */
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

/* returns random unit vector */
static void randuvec(float v[3])
{
	float r;
	v[2] = 2.f * frand() - 1.f;
	if ((r = 1.f - v[2] * v[2]) > 0.f) {
		float a = (float)(6.283185307f * frand());
		r = (float)sqrt(r);
		v[0] = (float)(r * cosf(a));
		v[1] = (float)(r * sinf(a));
	}
	else {
		v[2] = 1.f;
	}
}

static PyObject *Noise_random(PyObject *UNUSED(self))
{
	return PyFloat_FromDouble(frand());
}

static PyObject *Noise_random_unit_vector(PyObject *UNUSED(self))
{
	float v[3] = {0.0f, 0.0f, 0.0f};
	randuvec(v);
	return Py_BuildValue("[fff]", v[0], v[1], v[2]);
}

/*---------------------------------------------------------------------*/

/* Random seed init. Only used for MT random() & randuvec() */

static PyObject *Noise_seed_set(PyObject *UNUSED(self), PyObject *args)
{
	int s;
	if (!PyArg_ParseTuple(args, "i:seed_set", &s))
		return NULL;
	setRndSeed(s);
	Py_RETURN_NONE;
}

/*-------------------------------------------------------------------------*/

/* General noise */

static PyObject *Noise_noise(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z;
	int nb = 1;
	if (!PyArg_ParseTuple(args, "(fff)|i:noise", &x, &y, &z, &nb))
		return NULL;

	return PyFloat_FromDouble((2.0f * BLI_gNoise(1.0f, x, y, z, 0, nb) - 1.0f));
}

/*-------------------------------------------------------------------------*/

/* General Vector noise */

static void noise_vector(float x, float y, float z, int nb, float v[3])
{
	/* Simply evaluate noise at 3 different positions */
	v[0]= (float)(2.0f * BLI_gNoise(1.f, x + 9.321f, y - 1.531f, z - 7.951f, 0,
	                                nb) - 1.0f);
	v[1]= (float)(2.0f * BLI_gNoise(1.f, x, y, z, 0, nb) - 1.0f);
	v[2]= (float)(2.0f * BLI_gNoise(1.f, x + 6.327f, y + 0.1671f, z - 2.672f, 0,
	                                nb) - 1.0f);
}

static PyObject *Noise_vector(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z, v[3];
	int nb = 1;
	if (!PyArg_ParseTuple(args, "(fff)|i:vector", &x, &y, &z, &nb))
		return NULL;
	noise_vector(x, y, z, nb, v);
	return Py_BuildValue("[fff]", v[0], v[1], v[2]);
}

/*---------------------------------------------------------------------------*/

/* General turbulence */

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

static PyObject *Noise_turbulence(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z;
	int oct, hd, nb = 1;
	float as = 0.5, fs = 2.0;
	if (!PyArg_ParseTuple(args, "(fff)ii|iff:turbulence", &x, &y, &z, &oct, &hd, &nb, &as, &fs))
		return NULL;

	return PyFloat_FromDouble(turb(x, y, z, oct, hd, nb, as, fs));
}

/*--------------------------------------------------------------------------*/

/* Turbulence Vector */

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

static PyObject *Noise_turbulence_vector(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z, v[3];
	int oct, hd, nb = 1;
	float as = 0.5, fs = 2.0;
	if (!PyArg_ParseTuple(args, "(fff)ii|iff:turbulence_vector", &x, &y, &z, &oct, &hd, &nb, &as, &fs))
		return NULL;
	vTurb(x, y, z, oct, hd, nb, as, fs, v);
	return Py_BuildValue("[fff]", v[0], v[1], v[2]);
}

/*---------------------------------------------------------------------*/

/* F. Kenton Musgrave's fractal functions */

static PyObject *Noise_fractal(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z, H, lac, oct;
	int nb = 1;
	if (!PyArg_ParseTuple(args, "(fff)fff|i:fractal", &x, &y, &z, &H, &lac, &oct, &nb))
		return NULL;
	return PyFloat_FromDouble(mg_fBm(x, y, z, H, lac, oct, nb));
}

/*------------------------------------------------------------------------*/

static PyObject *Noise_multi_fractal(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z, H, lac, oct;
	int nb = 1;
	if (!PyArg_ParseTuple(args, "(fff)fff|i:multi_fractal", &x, &y, &z, &H, &lac, &oct, &nb))
		return NULL;

	return PyFloat_FromDouble(mg_MultiFractal(x, y, z, H, lac, oct, nb));
}

/*------------------------------------------------------------------------*/

static PyObject *Noise_vl_vector(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z, d;
	int nt1 = 1, nt2 = 1;
	if (!PyArg_ParseTuple(args, "(fff)f|ii:vl_vector", &x, &y, &z, &d, &nt1, &nt2))
		return NULL;
	return PyFloat_FromDouble(mg_VLNoise(x, y, z, d, nt1, nt2));
}

/*-------------------------------------------------------------------------*/

static PyObject *Noise_hetero_terrain(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z, H, lac, oct, ofs;
	int nb = 1;
	if (!PyArg_ParseTuple(args, "(fff)ffff|i:hetero_terrain", &x, &y, &z, &H, &lac, &oct, &ofs, &nb))
		return NULL;

	return PyFloat_FromDouble(mg_HeteroTerrain(x, y, z, H, lac, oct, ofs, nb));
}

/*-------------------------------------------------------------------------*/

static PyObject *Noise_hybrid_multi_fractal(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z, H, lac, oct, ofs, gn;
	int nb = 1;
	if (!PyArg_ParseTuple(args, "(fff)fffff|i:hybrid_multi_fractal", &x, &y, &z, &H, &lac, &oct, &ofs, &gn, &nb))
		return NULL;
	
	return PyFloat_FromDouble(mg_HybridMultiFractal(x, y, z, H, lac, oct, ofs, gn, nb));
}

/*------------------------------------------------------------------------*/

static PyObject *Noise_ridged_multi_fractal(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z, H, lac, oct, ofs, gn;
	int nb = 1;
	if (!PyArg_ParseTuple(args, "(fff)fffff|i:ridged_multi_fractal", &x, &y, &z, &H, &lac, &oct, &ofs, &gn, &nb))
		return NULL;
	return PyFloat_FromDouble(mg_RidgedMultiFractal(x, y, z, H, lac, oct, ofs, gn, nb));
}

/*-------------------------------------------------------------------------*/

static PyObject *Noise_voronoi(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z, da[4], pa[12];
	int dtype = 0;
	float me = 2.5;		/* default minkovsky exponent */
	if (!PyArg_ParseTuple(args, "(fff)|if:voronoi", &x, &y, &z, &dtype, &me))
		return NULL;
	voronoi(x, y, z, da, pa, me, dtype);
	return Py_BuildValue("[[ffff][[fff][fff][fff][fff]]]",
	                     da[0], da[1], da[2], da[3],
	                     pa[0], pa[1], pa[2],
	                     pa[3], pa[4], pa[5],
	                     pa[6], pa[7], pa[8], pa[9], pa[10], pa[11]);
}

/*-------------------------------------------------------------------------*/

static PyObject *Noise_cell(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z;
	if (!PyArg_ParseTuple(args, "(fff):cell", &x, &y, &z))
		return NULL;

	return PyFloat_FromDouble(cellNoise(x, y, z));
}

/*--------------------------------------------------------------------------*/

static PyObject *Noise_cell_vector(PyObject *UNUSED(self), PyObject *args)
{
	float x, y, z, ca[3];
	if (!PyArg_ParseTuple(args, "(fff):cell_vector", &x, &y, &z))
		return NULL;
	cellNoiseV(x, y, z, ca);
	return Py_BuildValue("[fff]", ca[0], ca[1], ca[2]);
}

/*--------------------------------------------------------------------------*/
/* For all other Blender modules, this stuff seems to be put in a header file.
   This doesn't seem really appropriate to me, so I just put it here, feel free to change it.
   In the original module I actually kept the docs stings with the functions themselves,
   but I grouped them here so that it can easily be moved to a header if anyone thinks that is necessary. */

PyDoc_STRVAR(random__doc__,
"() No arguments.\n\n\
Returns a random floating point number in the range [0, 1)"
);

PyDoc_STRVAR(random_unit_vector__doc__,
"() No arguments.\n\nReturns a random unit vector (3-float list)."
);

PyDoc_STRVAR(seed_set__doc__,
"(seed value)\n\n\
Initializes random number generator.\n\
if seed is zero, the current time will be used instead."
);

PyDoc_STRVAR(noise__doc__,
"((x,y,z) tuple, [noisetype])\n\n\
Returns general noise of the optional specified type.\n\
Optional argument noisetype determines the type of noise, STDPERLIN by default, see NoiseTypes."
);

PyDoc_STRVAR(noise_vector__doc__,
"((x,y,z) tuple, [noisetype])\n\n\
Returns noise vector (3-float list) of the optional specified type.\
Optional argument noisetype determines the type of noise, STDPERLIN by default, see NoiseTypes."
);

PyDoc_STRVAR(turbulence__doc__,
"((x,y,z) tuple, octaves, hard, [noisebasis], [ampscale], [freqscale])\n\n\
Returns general turbulence value using the optional specified noisebasis function.\n\
octaves (integer) is the number of noise values added.\n\
hard (bool), when false (0) returns 'soft' noise, when true (1) returns 'hard' noise (returned value always positive).\n\
Optional arguments:\n\
noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes.\n\
ampscale sets the amplitude scale value of the noise frequencies added, 0.5 by default.\n\
freqscale sets the frequency scale factor, 2.0 by default."
);

PyDoc_STRVAR(turbulence_vector__doc__,
"((x,y,z) tuple, octaves, hard, [noisebasis], [ampscale], [freqscale])\n\n\
Returns general turbulence vector (3-float list) using the optional specified noisebasis function.\n\
octaves (integer) is the number of noise values added.\n\
hard (bool), when false (0) returns 'soft' noise, when true (1) returns 'hard' noise (returned vector always positive).\n\
Optional arguments:\n\
noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes.\n\
ampscale sets the amplitude scale value of the noise frequencies added, 0.5 by default.\n\
freqscale sets the frequency scale factor, 2.0 by default."
);

PyDoc_STRVAR(fractal__doc__,
"((x,y,z) tuple, H, lacunarity, octaves, [noisebasis])\n\n\
Returns Fractal Brownian Motion noise value(fBm).\n\
H is the fractal increment parameter.\n\
lacunarity is the gap between successive frequencies.\n\
octaves is the number of frequencies in the fBm.\n\
Optional argument noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes."
);

PyDoc_STRVAR(multi_fractal__doc__,
"((x,y,z) tuple, H, lacunarity, octaves, [noisebasis])\n\n\
Returns Multifractal noise value.\n\
H determines the highest fractal dimension.\n\
lacunarity is gap between successive frequencies.\n\
octaves is the number of frequencies in the fBm.\n\
Optional argument noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes."
);

PyDoc_STRVAR(vl_vector__doc__,
"((x,y,z) tuple, distortion, [noisetype1], [noisetype2])\n\n\
Returns Variable Lacunarity Noise value, a distorted variety of noise.\n\
distortion sets the amount of distortion.\n\
Optional arguments noisetype1 and noisetype2 set the noisetype to distort and the noisetype used for the distortion respectively.\n\
See NoiseTypes, both are STDPERLIN by default."
);

PyDoc_STRVAR(hetero_terrain__doc__,
"((x,y,z) tuple, H, lacunarity, octaves, offset, [noisebasis])\n\n\
returns Heterogeneous Terrain value\n\
H determines the fractal dimension of the roughest areas.\n\
lacunarity is the gap between successive frequencies.\n\
octaves is the number of frequencies in the fBm.\n\
offset raises the terrain from 'sea level'.\n\
Optional argument noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes."
);

PyDoc_STRVAR(hybrid_multi_fractal__doc__,
"((x,y,z) tuple, H, lacunarity, octaves, offset, gain, [noisebasis])\n\n\
returns Hybrid Multifractal value.\n\
H determines the fractal dimension of the roughest areas.\n\
lacunarity is the gap between successive frequencies.\n\
octaves is the number of frequencies in the fBm.\n\
offset raises the terrain from 'sea level'.\n\
gain scales the values.\n\
Optional argument noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes."
);

PyDoc_STRVAR(ridged_multi_fractal__doc__,
"((x,y,z) tuple, H, lacunarity, octaves, offset, gain [noisebasis])\n\n\
returns Ridged Multifractal value.\n\
H determines the fractal dimension of the roughest areas.\n\
lacunarity is the gap between successive frequencies.\n\
octaves is the number of frequencies in the fBm.\n\
offset raises the terrain from 'sea level'.\n\
gain scales the values.\n\
Optional argument noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes."
);

PyDoc_STRVAR(voronoi__doc__,
"((x,y,z) tuple, distance_metric, [exponent])\n\n\
returns a list, containing a list of distances in order of closest feature,\n\
and a list containing the positions of the four closest features\n\
Optional arguments:\n\
distance_metric: see DistanceMetrics, default is DISTANCE\n\
exponent is only used with MINKOVSKY, default is 2.5."
);

PyDoc_STRVAR(cell__doc__,
"((x,y,z) tuple)\n\n\
returns cellnoise float value."
);

PyDoc_STRVAR(cell_vector__doc__,
"((x,y,z) tuple)\n\n\
returns cellnoise vector/point/color (3-float list)."
);

PyDoc_STRVAR(Noise__doc__,
"Blender Noise and Turbulence Module\n\n\
This module can be used to generate noise of various types.\n\
This can be used for terrain generation, to create textures,\n\
make animations more 'animated', object deformation, etc.\n\
As an example, this code segment when scriptlinked to a framechanged event,\n\
will make the camera sway randomly about, by changing parameters this can\n\
look like anything from an earthquake to a very nervous or maybe even drunk cameraman...\n\
(the camera needs an ipo with at least one Loc & Rot key for this to work!):\n\
\n\
\tfrom Blender import Get, Scene, Noise\n\
\n\
\t####################################################\n\
\t# This controls jitter speed\n\
\tsl = 0.025\n\
\t# This controls the amount of position jitter\n\
\tsp = 0.1\n\
\t# This controls the amount of rotation jitter\n\
\tsr = 0.25\n\
\t####################################################\n\
\n\
\ttime = Get('curtime')\n\
\tob = Scene.GetCurrent().getCurrentCamera()\n\
\tps = (sl*time, sl*time, sl*time)\n\
\t# To add jitter only when the camera moves, use this next line instead\n\
\t#ps = (sl*ob.LocX, sl*ob.LocY, sl*ob.LocZ)\n\
\trv = Noise.turbulence_vector(ps, 3, 0, Noise.NoiseTypes.NEWPERLIN)\n\
\tob.dloc = (sp*rv[0], sp*rv[1], sp*rv[2])\n\
\tob.drot = (sr*rv[0], sr*rv[1], sr*rv[2])\n\
\n"
);

/* Just in case, declarations for a header file */
/*
static PyObject *Noise_random(PyObject *UNUSED(self));
static PyObject *Noise_random_unit_vector(PyObject *UNUSED(self));
static PyObject *Noise_seed_set(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_noise(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_vector(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_turbulence(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_turbulence_vector(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_fractal(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_multi_fractal(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_vl_vector(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_hetero_terrain(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_hybrid_multi_fractal(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_ridged_multi_fractal(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_voronoi(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_cell(PyObject *UNUSED(self), PyObject *args);
static PyObject *Noise_cell_vector(PyObject *UNUSED(self), PyObject *args);
*/

static PyMethodDef NoiseMethods[] = {
	{"seed_set", (PyCFunction) Noise_seed_set, METH_VARARGS, seed_set__doc__},
	{"random", (PyCFunction) Noise_random, METH_NOARGS, random__doc__},
	{"random_unit_vector", (PyCFunction) Noise_random_unit_vector, METH_NOARGS, random_unit_vector__doc__},
	{"noise", (PyCFunction) Noise_noise, METH_VARARGS, noise__doc__},
	{"vector", (PyCFunction) Noise_vector, METH_VARARGS, noise_vector__doc__},
	{"turbulence", (PyCFunction) Noise_turbulence, METH_VARARGS, turbulence__doc__},
	{"turbulence_vector", (PyCFunction) Noise_turbulence_vector, METH_VARARGS, turbulence_vector__doc__},
	{"fractal", (PyCFunction) Noise_fractal, METH_VARARGS, fractal__doc__},
	{"multi_fractal", (PyCFunction) Noise_multi_fractal, METH_VARARGS, multi_fractal__doc__},
	{"vl_vector", (PyCFunction) Noise_vl_vector, METH_VARARGS, vl_vector__doc__},
	{"hetero_terrain", (PyCFunction) Noise_hetero_terrain, METH_VARARGS, hetero_terrain__doc__},
	{"hybrid_multi_fractal", (PyCFunction) Noise_hybrid_multi_fractal, METH_VARARGS, hybrid_multi_fractal__doc__},
	{"ridged_multi_fractal", (PyCFunction) Noise_ridged_multi_fractal, METH_VARARGS, ridged_multi_fractal__doc__},
	{"voronoi", (PyCFunction) Noise_voronoi, METH_VARARGS, voronoi__doc__},
	{"cell", (PyCFunction) Noise_cell, METH_VARARGS, cell__doc__},
	{"cell_vector", (PyCFunction) Noise_cell_vector, METH_VARARGS, cell_vector__doc__},
	{NULL, NULL, 0, NULL}
};

/*----------------------------------------------------------------------*/

static struct PyModuleDef noise_module_def = {
	PyModuleDef_HEAD_INIT,
	"noise",  /* m_name */
	Noise__doc__,  /* m_doc */
	0,     /* m_size */
	NoiseMethods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

PyObject *BPyInit_noise(void)
{
	PyObject *submodule = PyModule_Create(&noise_module_def);

	/* use current time as seed for random number generator by default */
	setRndSeed(0);	

	/* Constant noisetype dictionary */
	if (submodule) {
		static PyStructSequence_Field noise_types_fields[] = {
			{(char *)"BLENDER", NULL},
			{(char *)"STDPERLIN", NULL},
			{(char *)"NEWPERLIN", NULL},
			{(char *)"VORONOI_F1", NULL},
			{(char *)"VORONOI_F2", NULL},
			{(char *)"VORONOI_F3", NULL},
			{(char *)"VORONOI_F4", NULL},
			{(char *)"VORONOI_F2F1", NULL},
			{(char *)"VORONOI_CRACKLE", NULL},
			{(char *)"CELLNOISE", NULL},
			{NULL}
		};

		static PyStructSequence_Desc noise_types_info_desc = {
			(char *)"noise.types",     /* name */
			(char *)"Noise type",    /* doc */
			noise_types_fields,    /* fields */
			(sizeof(noise_types_fields)/sizeof(PyStructSequence_Field)) - 1
		};

		static PyTypeObject NoiseType;

		PyObject *noise_types;
		
		int pos = 0;
		
		PyStructSequence_InitType(&NoiseType, &noise_types_info_desc);
	
		noise_types = PyStructSequence_New(&NoiseType);
		if (noise_types == NULL) {
			return NULL;
		}

		PyStructSequence_SET_ITEM(noise_types, pos++, PyLong_FromLong(TEX_BLENDER));
		PyStructSequence_SET_ITEM(noise_types, pos++, PyLong_FromLong(TEX_STDPERLIN));
		PyStructSequence_SET_ITEM(noise_types, pos++, PyLong_FromLong(TEX_NEWPERLIN));
		PyStructSequence_SET_ITEM(noise_types, pos++, PyLong_FromLong(TEX_VORONOI_F1));
		PyStructSequence_SET_ITEM(noise_types, pos++, PyLong_FromLong(TEX_VORONOI_F2));
		PyStructSequence_SET_ITEM(noise_types, pos++, PyLong_FromLong(TEX_VORONOI_F3));
		PyStructSequence_SET_ITEM(noise_types, pos++, PyLong_FromLong(TEX_VORONOI_F4));
		PyStructSequence_SET_ITEM(noise_types, pos++, PyLong_FromLong(TEX_VORONOI_F2F1));
		PyStructSequence_SET_ITEM(noise_types, pos++, PyLong_FromLong(TEX_VORONOI_CRACKLE));
		PyStructSequence_SET_ITEM(noise_types, pos++, PyLong_FromLong(TEX_CELLNOISE));

		PyModule_AddObject(submodule, "types", noise_types);
	}
	
	if (submodule) {
		static PyStructSequence_Field distance_metrics_fields[] = {
			{(char *)"DISTANCE", NULL},
			{(char *)"DISTANCE_SQUARED", NULL},
			{(char *)"MANHATTAN", NULL},
			{(char *)"CHEBYCHEV", NULL},
			{(char *)"MINKOVSKY_HALF", NULL},
			{(char *)"MINKOVSKY_FOUR", NULL},
			{(char *)"MINKOVSKY", NULL},
			{NULL}
		};

		static PyStructSequence_Desc noise_types_info_desc = {
			(char *)"noise.distance_metrics",     /* name */
			(char *)"Distance Metrics for noise module.",    /* doc */
			distance_metrics_fields,    /* fields */
			(sizeof(distance_metrics_fields)/sizeof(PyStructSequence_Field)) - 1
		};
		
		static PyTypeObject DistanceMetrics;
		
		PyObject *distance_metrics;
		
		int pos = 0;
		
		PyStructSequence_InitType(&DistanceMetrics, &noise_types_info_desc);
	
		distance_metrics = PyStructSequence_New(&DistanceMetrics);
		if (distance_metrics == NULL) {
			return NULL;
		}

		PyStructSequence_SET_ITEM(distance_metrics, pos++, PyLong_FromLong(TEX_DISTANCE));
		PyStructSequence_SET_ITEM(distance_metrics, pos++, PyLong_FromLong(TEX_DISTANCE_SQUARED));
		PyStructSequence_SET_ITEM(distance_metrics, pos++, PyLong_FromLong(TEX_MANHATTAN));
		PyStructSequence_SET_ITEM(distance_metrics, pos++, PyLong_FromLong(TEX_CHEBYCHEV));
		PyStructSequence_SET_ITEM(distance_metrics, pos++, PyLong_FromLong(TEX_MINKOVSKY_HALF));
		PyStructSequence_SET_ITEM(distance_metrics, pos++, PyLong_FromLong(TEX_MINKOVSKY_FOUR));
		PyStructSequence_SET_ITEM(distance_metrics, pos++, PyLong_FromLong(TEX_MINKOVSKY));

		PyModule_AddObject(submodule, "distance_metrics", distance_metrics);
	}

	return submodule;
}
