/**
 * $Id: Noise.c 12056 2007-09-17 06:11:06Z aligorith $
 *
 * Blender.Noise BPython module implementation.
 * This submodule has functions to generate noise of various types.
 * 
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): eeshlo
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/************************/
/* Blender Noise Module */
/************************/

#include <Python.h>

#include "BLI_blenlib.h"
#include "DNA_texture_types.h"
#include "constant.h"

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

/* Period parameters */
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL	/* constant vector a */
#define UMASK 0x80000000UL	/* most significant w-r bits */
#define LMASK 0x7fffffffUL	/* least significant r bits */
#define MIXBITS(u,v) ( ((u) & UMASK) | ((v) & LMASK) )
#define TWIST(u,v) ((MIXBITS(u,v) >> 1) ^ ((v)&1UL ? MATRIX_A : 0UL))

static unsigned long state[N];	/* the array for the state vector  */
static int left = 1;
static int initf = 0;
static unsigned long *next;

PyObject *Noise_Init(void);

/* initializes state[N] with a seed */
static void init_genrand( unsigned long s )
{
	int j;
	state[0] = s & 0xffffffffUL;
	for( j = 1; j < N; j++ ) {
		state[j] =
			( 1812433253UL *
			  ( state[j - 1] ^ ( state[j - 1] >> 30 ) ) + j );
		/* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
		/* In the previous versions, MSBs of the seed affect   */
		/* only MSBs of the array state[].                        */
		/* 2002/01/09 modified by Makoto Matsumoto             */
		state[j] &= 0xffffffffUL;	/* for >32 bit machines */
	}
	left = 1;
	initf = 1;
}

static void next_state( void )
{
	unsigned long *p = state;
	int j;

	/* if init_genrand() has not been called, */
	/* a default initial seed is used         */
	if( initf == 0 )
		init_genrand( 5489UL );

	left = N;
	next = state;

	for( j = N - M + 1; --j; p++ )
		*p = p[M] ^ TWIST( p[0], p[1] );

	for( j = M; --j; p++ )
		*p = p[M - N] ^ TWIST( p[0], p[1] );

	*p = p[M - N] ^ TWIST( p[0], state[0] );
}

/*------------------------------------------------------------*/

static void setRndSeed( int seed )
{
	if( seed == 0 )
		init_genrand( time( NULL ) );
	else
		init_genrand( seed );
}

/* float number in range [0, 1) using the mersenne twister rng */
static float frand(  )
{
	unsigned long y;

	if( --left == 0 )
		next_state(  );
	y = *next++;

	/* Tempering */
	y ^= ( y >> 11 );
	y ^= ( y << 7 ) & 0x9d2c5680UL;
	y ^= ( y << 15 ) & 0xefc60000UL;
	y ^= ( y >> 18 );

	return ( float ) y / 4294967296.f;
}

/*------------------------------------------------------------*/

/* returns random unit vector */
static void randuvec( float v[3] )
{
	float r;
	v[2] = 2.f * frand(  ) - 1.f;
	if( ( r = 1.f - v[2] * v[2] ) > 0.f ) {
		float a = (float)(6.283185307f * frand(  ));
		r = (float)sqrt( r );
		v[0] = (float)(r * cos( a ));
		v[1] = (float)(r * sin( a ));
	} else
		v[2] = 1.f;
}

static PyObject *Noise_random( PyObject * self )
{
	return Py_BuildValue( "f", frand(  ) );
}

static PyObject *Noise_randuvec( PyObject * self )
{
	float v[3] = {0.0f, 0.0f, 0.0f};
	randuvec( v );
	return Py_BuildValue( "[fff]", v[0], v[1], v[2] );
}

/*---------------------------------------------------------------------*/

/* Random seed init. Only used for MT random() & randuvec() */

static PyObject *Noise_setRandomSeed( PyObject * self, PyObject * args )
{
	int s;
	if( !PyArg_ParseTuple( args, "i", &s ) )
		return NULL;
	setRndSeed( s );
	Py_INCREF( Py_None );
	return Py_None;
}

/*-------------------------------------------------------------------------*/

/* General noise */

static PyObject *Noise_noise( PyObject * self, PyObject * args )
{
	float x, y, z;
	int nb = 1;
	if( !PyArg_ParseTuple( args, "(fff)|i", &x, &y, &z, &nb ) )
		return NULL;
	
	return PyFloat_FromDouble(
			(double)(2.0 * BLI_gNoise( 1.0, x, y, z, 0, nb ) - 1.0) );
}

/*-------------------------------------------------------------------------*/

/* General Vector noise */

static void vNoise( float x, float y, float z, int nb, float v[3] )
{
	/* Simply evaluate noise at 3 different positions */
	v[0] = (float)(2.0 * BLI_gNoise( 1.f, x + 9.321f, y - 1.531f, z - 7.951f, 0,
				 nb ) - 1.0);
	v[1] = (float)(2.0 * BLI_gNoise( 1.f, x, y, z, 0, nb ) - 1.0);
	v[2] = (float)(2.0 * BLI_gNoise( 1.f, x + 6.327f, y + 0.1671f, z - 2.672f, 0,
				 nb ) - 1.0);
}

static PyObject *Noise_vNoise( PyObject * self, PyObject * args )
{
	float x, y, z, v[3];
	int nb = 1;
	if( !PyArg_ParseTuple( args, "(fff)|i", &x, &y, &z, &nb ) )
		return NULL;
	vNoise( x, y, z, nb, v );
	return Py_BuildValue( "[fff]", v[0], v[1], v[2] );
}

/*---------------------------------------------------------------------------*/

/* General turbulence */

static float turb( float x, float y, float z, int oct, int hard, int nb,
		   float ampscale, float freqscale )
{
	float amp, out, t;
	int i;
	amp = 1.f;
	out = (float)(2.0 * BLI_gNoise( 1.f, x, y, z, 0, nb ) - 1.0);
	if( hard )
		out = (float)fabs( out );
	for( i = 1; i < oct; i++ ) {
		amp *= ampscale;
		x *= freqscale;
		y *= freqscale;
		z *= freqscale;
		t = (float)(amp * ( 2.0 * BLI_gNoise( 1.f, x, y, z, 0, nb ) - 1.0 ));
		if( hard )
			t = (float)fabs( t );
		out += t;
	}
	return out;
}

static PyObject *Noise_turbulence( PyObject * self, PyObject * args )
{
	float x, y, z;
	int oct, hd, nb = 1;
	float as = 0.5, fs = 2.0;
	if( !PyArg_ParseTuple
	    ( args, "(fff)ii|iff", &x, &y, &z, &oct, &hd, &nb, &as, &fs ) )
		return NULL;
	return PyFloat_FromDouble( (double)turb( x, y, z, oct, hd, nb, as, fs ) );
}

/*--------------------------------------------------------------------------*/

/* Turbulence Vector */

static void vTurb( float x, float y, float z, int oct, int hard, int nb,
		   float ampscale, float freqscale, float v[3] )
{
	float amp, t[3];
	int i;
	amp = 1.f;
	vNoise( x, y, z, nb, v );
	if( hard ) {
		v[0] = (float)fabs( v[0] );
		v[1] = (float)fabs( v[1] );
		v[2] = (float)fabs( v[2] );
	}
	for( i = 1; i < oct; i++ ) {
		amp *= ampscale;
		x *= freqscale;
		y *= freqscale;
		z *= freqscale;
		vNoise( x, y, z, nb, t );
		if( hard ) {
			t[0] = (float)fabs( t[0] );
			t[1] = (float)fabs( t[1] );
			t[2] = (float)fabs( t[2] );
		}
		v[0] += amp * t[0];
		v[1] += amp * t[1];
		v[2] += amp * t[2];
	}
}

static PyObject *Noise_vTurbulence( PyObject * self, PyObject * args )
{
	float x, y, z, v[3];
	int oct, hd, nb = 1;
	float as = 0.5, fs = 2.0;
	if( !PyArg_ParseTuple
	    ( args, "(fff)ii|iff", &x, &y, &z, &oct, &hd, &nb, &as, &fs ) )
		return NULL;
	vTurb( x, y, z, oct, hd, nb, as, fs, v );
	return Py_BuildValue( "[fff]", v[0], v[1], v[2] );
}

/*---------------------------------------------------------------------*/

/* F. Kenton Musgrave's fractal functions */

static PyObject *Noise_fBm( PyObject * self, PyObject * args )
{
	float x, y, z, H, lac, oct;
	int nb = 1;
	if( !PyArg_ParseTuple
	    ( args, "(fff)fff|i", &x, &y, &z, &H, &lac, &oct, &nb ) )
		return NULL;
	return PyFloat_FromDouble( (double)mg_fBm( x, y, z, H, lac, oct, nb ) );
}

/*------------------------------------------------------------------------*/

static PyObject *Noise_multiFractal( PyObject * self, PyObject * args )
{
	float x, y, z, H, lac, oct;
	int nb = 1;
	if( !PyArg_ParseTuple
	    ( args, "(fff)fff|i", &x, &y, &z, &H, &lac, &oct, &nb ) )
		return NULL;
	return PyFloat_FromDouble( (double)mg_MultiFractal( x, y, z, H, lac, oct, nb ) );
}

/*------------------------------------------------------------------------*/

static PyObject *Noise_vlNoise( PyObject * self, PyObject * args )
{
	float x, y, z, d;
	int nt1 = 1, nt2 = 1;
	if( !PyArg_ParseTuple
	    ( args, "(fff)f|ii", &x, &y, &z, &d, &nt1, &nt2 ) )
		return NULL;
	return PyFloat_FromDouble( (double)mg_VLNoise( x, y, z, d, nt1, nt2 ) );
}

/*-------------------------------------------------------------------------*/

static PyObject *Noise_heteroTerrain( PyObject * self, PyObject * args )
{
	float x, y, z, H, lac, oct, ofs;
	int nb = 1;
	if( !PyArg_ParseTuple
	    ( args, "(fff)ffff|i", &x, &y, &z, &H, &lac, &oct, &ofs, &nb ) )
		return NULL;
	
	return PyFloat_FromDouble(
			(double)mg_HeteroTerrain( x, y, z, H, lac, oct, ofs, nb ) );
}

/*-------------------------------------------------------------------------*/

static PyObject *Noise_hybridMFractal( PyObject * self, PyObject * args )
{
	float x, y, z, H, lac, oct, ofs, gn;
	int nb = 1;
	if( !PyArg_ParseTuple
	    ( args, "(fff)fffff|i", &x, &y, &z, &H, &lac, &oct, &ofs, &gn,
	      &nb ) )
		return NULL;
	
	return PyFloat_FromDouble(
			(double)mg_HybridMultiFractal( x, y, z, H, lac, oct, ofs, gn, nb) );
}

/*------------------------------------------------------------------------*/

static PyObject *Noise_ridgedMFractal( PyObject * self, PyObject * args )
{
	float x, y, z, H, lac, oct, ofs, gn;
	int nb = 1;
	if( !PyArg_ParseTuple
	    ( args, "(fff)fffff|i", &x, &y, &z, &H, &lac, &oct, &ofs, &gn,
	      &nb ) )
		return NULL;
	return PyFloat_FromDouble(
			(double)mg_RidgedMultiFractal( x, y, z, H, lac, oct, ofs, gn, nb) );
}

/*-------------------------------------------------------------------------*/

static PyObject *Noise_voronoi( PyObject * self, PyObject * args )
{
	float x, y, z, da[4], pa[12];
	int dtype = 0;
	float me = 2.5;		/* default minkovsky exponent */
	if( !PyArg_ParseTuple( args, "(fff)|if", &x, &y, &z, &dtype, &me ) )
		return NULL;
	voronoi( x, y, z, da, pa, me, dtype );
	return Py_BuildValue( "[[ffff][[fff][fff][fff][fff]]]",
			      da[0], da[1], da[2], da[3],
			      pa[0], pa[1], pa[2],
			      pa[3], pa[4], pa[5],
			      pa[6], pa[7], pa[8], pa[9], pa[10], pa[11] );
}

/*-------------------------------------------------------------------------*/

static PyObject *Noise_cellNoise( PyObject * self, PyObject * args )
{
	float x, y, z;
	if( !PyArg_ParseTuple( args, "(fff)", &x, &y, &z ) )
		return NULL;
	return Py_BuildValue( "f", cellNoise( x, y, z ) );
}

/*--------------------------------------------------------------------------*/

static PyObject *Noise_cellNoiseV( PyObject * self, PyObject * args )
{
	float x, y, z, ca[3];
	if( !PyArg_ParseTuple( args, "(fff)", &x, &y, &z ) )
		return NULL;
	cellNoiseV( x, y, z, ca );
	return Py_BuildValue( "[fff]", ca[0], ca[1], ca[2] );
}

/*--------------------------------------------------------------------------*/
/* For all other Blender modules, this stuff seems to be put in a header file.
   This doesn't seem really appropriate to me, so I just put it here, feel free to change it.
   In the original module I actually kept the docs stings with the functions themselves,
   but I grouped them here so that it can easily be moved to a header if anyone thinks that is necessary. */

static char random__doc__[] = "() No arguments.\n\n\
Returns a random floating point number in the range [0, 1)";

static char randuvec__doc__[] =
	"() No arguments.\n\nReturns a random unit vector (3-float list).";

static char setRandomSeed__doc__[] = "(seed value)\n\n\
Initializes random number generator.\n\
if seed is zero, the current time will be used instead.";

static char noise__doc__[] = "((x,y,z) tuple, [noisetype])\n\n\
Returns general noise of the optional specified type.\n\
Optional argument noisetype determines the type of noise, STDPERLIN by default, see NoiseTypes.";

static char vNoise__doc__[] = "((x,y,z) tuple, [noisetype])\n\n\
Returns noise vector (3-float list) of the optional specified type.\
Optional argument noisetype determines the type of noise, STDPERLIN by default, see NoiseTypes.";

static char turbulence__doc__[] =
	"((x,y,z) tuple, octaves, hard, [noisebasis], [ampscale], [freqscale])\n\n\
Returns general turbulence value using the optional specified noisebasis function.\n\
octaves (integer) is the number of noise values added.\n\
hard (bool), when false (0) returns 'soft' noise, when true (1) returns 'hard' noise (returned value always positive).\n\
Optional arguments:\n\
noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes.\n\
ampscale sets the amplitude scale value of the noise frequencies added, 0.5 by default.\n\
freqscale sets the frequency scale factor, 2.0 by default.";

static char vTurbulence__doc__[] =
	"((x,y,z) tuple, octaves, hard, [noisebasis], [ampscale], [freqscale])\n\n\
Returns general turbulence vector (3-float list) using the optional specified noisebasis function.\n\
octaves (integer) is the number of noise values added.\n\
hard (bool), when false (0) returns 'soft' noise, when true (1) returns 'hard' noise (returned vector always positive).\n\
Optional arguments:\n\
noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes.\n\
ampscale sets the amplitude scale value of the noise frequencies added, 0.5 by default.\n\
freqscale sets the frequency scale factor, 2.0 by default.";

static char fBm__doc__[] =
	"((x,y,z) tuple, H, lacunarity, octaves, [noisebasis])\n\n\
Returns Fractal Brownian Motion noise value(fBm).\n\
H is the fractal increment parameter.\n\
lacunarity is the gap between successive frequencies.\n\
octaves is the number of frequencies in the fBm.\n\
Optional argument noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes.";

static char multiFractal__doc__[] =
	"((x,y,z) tuple, H, lacunarity, octaves, [noisebasis])\n\n\
Returns Multifractal noise value.\n\
H determines the highest fractal dimension.\n\
lacunarity is gap between successive frequencies.\n\
octaves is the number of frequencies in the fBm.\n\
Optional argument noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes.";

static char vlNoise__doc__[] =
	"((x,y,z) tuple, distortion, [noisetype1], [noisetype2])\n\n\
Returns Variable Lacunarity Noise value, a distorted variety of noise.\n\
distortion sets the amount of distortion.\n\
Optional arguments noisetype1 and noisetype2 set the noisetype to distort and the noisetype used for the distortion respectively.\n\
See NoiseTypes, both are STDPERLIN by default.";

static char heteroTerrain__doc__[] =
	"((x,y,z) tuple, H, lacunarity, octaves, offset, [noisebasis])\n\n\
returns Heterogeneous Terrain value\n\
H determines the fractal dimension of the roughest areas.\n\
lacunarity is the gap between successive frequencies.\n\
octaves is the number of frequencies in the fBm.\n\
offset raises the terrain from 'sea level'.\n\
Optional argument noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes.";

static char hybridMFractal__doc__[] =
	"((x,y,z) tuple, H, lacunarity, octaves, offset, gain, [noisebasis])\n\n\
returns Hybrid Multifractal value.\n\
H determines the fractal dimension of the roughest areas.\n\
lacunarity is the gap between successive frequencies.\n\
octaves is the number of frequencies in the fBm.\n\
offset raises the terrain from 'sea level'.\n\
gain scales the values.\n\
Optional argument noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes.";

static char ridgedMFractal__doc__[] =
	"((x,y,z) tuple, H, lacunarity, octaves, offset, gain [noisebasis])\n\n\
returns Ridged Multifractal value.\n\
H determines the fractal dimension of the roughest areas.\n\
lacunarity is the gap between successive frequencies.\n\
octaves is the number of frequencies in the fBm.\n\
offset raises the terrain from 'sea level'.\n\
gain scales the values.\n\
Optional argument noisebasis determines the type of noise used for the turbulence, STDPERLIN by default, see NoiseTypes.";

static char voronoi__doc__[] =
	"((x,y,z) tuple, distance_metric, [exponent])\n\n\
returns a list, containing a list of distances in order of closest feature,\n\
and a list containing the positions of the four closest features\n\
Optional arguments:\n\
distance_metric: see DistanceMetrics, default is DISTANCE\n\
exponent is only used with MINKOVSKY, default is 2.5.";

static char cellNoise__doc__[] = "((x,y,z) tuple)\n\n\
returns cellnoise float value.";

static char cellNoiseV__doc__[] = "((x,y,z) tuple)\n\n\
returns cellnoise vector/point/color (3-float list).";

static char Noise__doc__[] = "Blender Noise and Turbulence Module\n\n\
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
\trv = Noise.vTurbulence(ps, 3, 0, Noise.NoiseTypes.NEWPERLIN)\n\
\tob.dloc = (sp*rv[0], sp*rv[1], sp*rv[2])\n\
\tob.drot = (sr*rv[0], sr*rv[1], sr*rv[2])\n\
\n";

/* Just in case, declarations for a header file */
/*
static PyObject *Noise_random(PyObject *self);
static PyObject *Noise_randuvec(PyObject *self);
static PyObject *Noise_setRandomSeed(PyObject *self, PyObject *args);
static PyObject *Noise_noise(PyObject *self, PyObject *args);
static PyObject *Noise_vNoise(PyObject *self, PyObject *args);
static PyObject *Noise_turbulence(PyObject *self, PyObject *args);
static PyObject *Noise_vTurbulence(PyObject *self, PyObject *args);
static PyObject *Noise_fBm(PyObject *self, PyObject *args);
static PyObject *Noise_multiFractal(PyObject *self, PyObject *args);
static PyObject *Noise_vlNoise(PyObject *self, PyObject *args);
static PyObject *Noise_heteroTerrain(PyObject *self, PyObject *args);
static PyObject *Noise_hybridMFractal(PyObject *self, PyObject *args);
static PyObject *Noise_ridgedMFractal(PyObject *self, PyObject *args);
static PyObject *Noise_voronoi(PyObject *self, PyObject *args);
static PyObject *Noise_cellNoise(PyObject *self, PyObject *args);
static PyObject *Noise_cellNoiseV(PyObject *self, PyObject *args);
*/

static PyMethodDef NoiseMethods[] = {
	{"setRandomSeed", ( PyCFunction ) Noise_setRandomSeed, METH_VARARGS,
	 setRandomSeed__doc__},
	{"random", ( PyCFunction ) Noise_random, METH_NOARGS, random__doc__},
	{"randuvec", ( PyCFunction ) Noise_randuvec, METH_NOARGS,
	 randuvec__doc__},
	{"noise", ( PyCFunction ) Noise_noise, METH_VARARGS, noise__doc__},
	{"vNoise", ( PyCFunction ) Noise_vNoise, METH_VARARGS, vNoise__doc__},
	{"turbulence", ( PyCFunction ) Noise_turbulence, METH_VARARGS,
	 turbulence__doc__},
	{"vTurbulence", ( PyCFunction ) Noise_vTurbulence, METH_VARARGS,
	 vTurbulence__doc__},
	{"fBm", ( PyCFunction ) Noise_fBm, METH_VARARGS, fBm__doc__},
	{"multiFractal", ( PyCFunction ) Noise_multiFractal, METH_VARARGS,
	 multiFractal__doc__},
	{"vlNoise", ( PyCFunction ) Noise_vlNoise, METH_VARARGS,
	 vlNoise__doc__},
	{"heteroTerrain", ( PyCFunction ) Noise_heteroTerrain, METH_VARARGS,
	 heteroTerrain__doc__},
	{"hybridMFractal", ( PyCFunction ) Noise_hybridMFractal, METH_VARARGS,
	 hybridMFractal__doc__},
	{"ridgedMFractal", ( PyCFunction ) Noise_ridgedMFractal, METH_VARARGS,
	 ridgedMFractal__doc__},
	{"voronoi", ( PyCFunction ) Noise_voronoi, METH_VARARGS,
	 voronoi__doc__},
	{"cellNoise", ( PyCFunction ) Noise_cellNoise, METH_VARARGS,
	 cellNoise__doc__},
	{"cellNoiseV", ( PyCFunction ) Noise_cellNoiseV, METH_VARARGS,
	 cellNoiseV__doc__},
	{NULL, NULL, 0, NULL}
};

/*----------------------------------------------------------------------*/

PyObject *Noise_Init(void)
{
	PyObject *NoiseTypes, *DistanceMetrics,
		*md =
		Py_InitModule3( "Blender.Noise", NoiseMethods, Noise__doc__ );

        /* use current time as seed for random number generator by default */
	setRndSeed( 0 );	

	/* Constant noisetype dictionary */
	NoiseTypes = PyConstant_New(  );
	if( NoiseTypes ) {
		BPy_constant *nt = ( BPy_constant * ) NoiseTypes;
		PyConstant_Insert( nt, "BLENDER",
				 PyInt_FromLong( TEX_BLENDER ) );
		PyConstant_Insert( nt, "STDPERLIN",
				 PyInt_FromLong( TEX_STDPERLIN ) );
		PyConstant_Insert( nt, "NEWPERLIN",
				 PyInt_FromLong( TEX_NEWPERLIN ) );
		PyConstant_Insert( nt, "VORONOI_F1",
				 PyInt_FromLong( TEX_VORONOI_F1 ) );
		PyConstant_Insert( nt, "VORONOI_F2",
				 PyInt_FromLong( TEX_VORONOI_F2 ) );
		PyConstant_Insert( nt, "VORONOI_F3",
				 PyInt_FromLong( TEX_VORONOI_F3 ) );
		PyConstant_Insert( nt, "VORONOI_F4",
				 PyInt_FromLong( TEX_VORONOI_F4 ) );
		PyConstant_Insert( nt, "VORONOI_F2F1",
				 PyInt_FromLong( TEX_VORONOI_F2F1 ) );
		PyConstant_Insert( nt, "VORONOI_CRACKLE",
				 PyInt_FromLong( TEX_VORONOI_CRACKLE ) );
		PyConstant_Insert( nt, "CELLNOISE",
				 PyInt_FromLong( TEX_CELLNOISE ) );
		PyModule_AddObject( md, "NoiseTypes", NoiseTypes );
	}

	/* Constant distance metric dictionary for voronoi */
	DistanceMetrics = PyConstant_New(  );
	if( DistanceMetrics ) {
		BPy_constant *dm = ( BPy_constant * ) DistanceMetrics;
		PyConstant_Insert( dm, "DISTANCE",
				 PyInt_FromLong( TEX_DISTANCE ) );
		PyConstant_Insert( dm, "DISTANCE_SQUARED",
				 PyInt_FromLong( TEX_DISTANCE_SQUARED ) );
		PyConstant_Insert( dm, "MANHATTAN",
				 PyInt_FromLong( TEX_MANHATTAN ) );
		PyConstant_Insert( dm, "CHEBYCHEV",
				 PyInt_FromLong( TEX_CHEBYCHEV ) );
		PyConstant_Insert( dm, "MINKOVSKY_HALF",
				 PyInt_FromLong( TEX_MINKOVSKY_HALF ) );
		PyConstant_Insert( dm, "MINKOVSKY_FOUR",
				 PyInt_FromLong( TEX_MINKOVSKY_FOUR ) );
		PyConstant_Insert( dm, "MINKOVSKY",
				 PyInt_FromLong( TEX_MINKOVSKY ) );
		PyModule_AddObject( md, "DistanceMetrics", DistanceMetrics );
	}

	return md;
}
