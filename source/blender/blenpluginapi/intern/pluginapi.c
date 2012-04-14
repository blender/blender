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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Wrappers for the plugin api. This api is up for removal.
 */

/** \file blender/blenpluginapi/intern/pluginapi.c
 *  \ingroup blenpluginapi
 */


/* There are four headers making up the plugin api:
 * - floatpatch.h : Wraps math functions for mips platforms, no code
 *                  required.
 * - iff.h        : Defines, macros and functions for dealing
 *                  with image buffer things.
 * - plugin.h     : Wraps some plugin handling types, accesses noise
 *                  functions.
 * - util.h       : Useful defines, memory management.
 */

#define PLUGIN_INTERN /* This tells the LIBEXPORT macro to compile with
                       * dll export set on windows */

#ifdef WIN32
#include "blenpluginapi/util.h"
#else
#include "blenpluginapi/util.h"
#endif
#include "iff.h"
#include "plugin.h"
#include "MEM_guardedalloc.h"

#include "BLO_sys_types.h" // needed for intptr_t

#include "BLI_blenlib.h"  /* util and noise functions */
#include "BLI_threads.h"  /* For threadsfe guardedalloc malloc/calloc/free */
#include "IMB_imbuf.h"    /* image buffer stuff       */
#define GET_INT_FROM_POINTER(i) ((int)(intptr_t)(i)) /* should use BKE_utildefines.h */

/* -------------------------------------------------------------------------- */
/* stuff from util.h                                                          */ 
/* -------------------------------------------------------------------------- */

LIBEXPORT void *mallocN(int len, char *str)
{
	return MEM_mallocN(len, str);
}

LIBEXPORT void *callocN(int len, char *str)
{
	return MEM_callocN(len, str);
}

LIBEXPORT short freeN(void *vmemh)
{
	return MEM_freeN(vmemh);
}

/* these are not needed anymore, mallocN/callocN/freeN is now threadsafe */
LIBEXPORT void *mallocT(int len, char *str)
{
	return MEM_mallocN(len, str);
}

LIBEXPORT void *callocT(int len, char *str)
{
	return MEM_callocN(len, str);
}

LIBEXPORT void freeT(void *vmemh)
{
	MEM_freeN(vmemh);
	return;
}


/* -------------------------------------------------------------------------- */
/* stuff from iff.h                                                           */ 
/* -------------------------------------------------------------------------- */

LIBEXPORT struct ImBuf *allocImBuf(short x,
						 short y,
						 uchar d,
						 uint flags)
{
	return IMB_allocImBuf(x, y, d, flags);
}


LIBEXPORT struct ImBuf *dupImBuf(struct ImBuf *ib)
{
	return IMB_dupImBuf(ib);
}
	
LIBEXPORT void freeImBuf(struct ImBuf* ib)
{
	IMB_freeImBuf(ib);
}

LIBEXPORT short saveiff(struct ImBuf *ib,
			  char *c,
			  int i)
{
	return IMB_saveiff(ib, c, i);
}

LIBEXPORT struct ImBuf *loadifffile(int a,
						  int b)
{
	return IMB_loadifffile(a, b, "loadifffile");
}

LIBEXPORT struct ImBuf *loadiffname(char *n,
						  int flags)
{
	return IMB_loadiffname(n, flags);
}
	
LIBEXPORT struct ImBuf *testiffname(char *n,
						  int flags)
{
	return IMB_testiffname(n, flags);
}

LIBEXPORT struct ImBuf *onehalf(struct ImBuf *ib)
{
	return IMB_onehalf(ib);
}

LIBEXPORT struct ImBuf *half_x(struct ImBuf *ib)
{
	return IMB_half_x(ib);
}

LIBEXPORT struct ImBuf *half_y(struct ImBuf *ib)
{
	return IMB_half_y(ib);
}

LIBEXPORT struct ImBuf *double_x(struct ImBuf *ib)
{
	return IMB_double_x(ib);
}

LIBEXPORT struct ImBuf *double_y(struct ImBuf *ib)
{
	return IMB_double_y(ib);
}

LIBEXPORT struct ImBuf *double_fast_x(struct ImBuf *ib)
{
	return IMB_double_fast_x(ib);
}

LIBEXPORT struct ImBuf *double_fast_y(struct ImBuf *ib)
{
	return IMB_double_fast_y(ib);
}

LIBEXPORT int ispic(char * name)
{
	return IMB_ispic(name);
}

/* still the same name */
/*  void (*ditherfunc)(struct ImBuf *, short, short) {} */

LIBEXPORT struct ImBuf *scaleImBuf(struct ImBuf *ib,
						 short nx,
						 short ny)
{
	return IMB_scaleImBuf(ib, nx, ny);
}

LIBEXPORT struct ImBuf *scalefastImBuf(struct ImBuf *ib,
							 short x,
							 short y)
{
	return IMB_scalefastImBuf(ib, x, y);
}

	/* Extra ones that some NaN (read Ton) plugins use,
	 * even though they aren't in the header
	 */

LIBEXPORT void interlace(struct ImBuf *ibuf)
{
	IMB_interlace(ibuf);
}

LIBEXPORT void de_interlace(struct ImBuf *ib)
{
	IMB_de_interlace(ib);
}

/* -------------------------------------------------------------------------- */
/* stuff from plugin.h                                                        */ 
/* -------------------------------------------------------------------------- */

/* These three need to be defined in the plugin itself. The plugin
 * loader looks for these functions to check whether it can use the
 * plugin. For sequences, something similar exists. */
/*  int plugin_tex_getversion(void); */
/*  int plugin_seq_getversion(void); */
/*  void plugin_getinfo(PluginInfo *); */

LIBEXPORT float hnoise(float noisesize,
			 float x,
			 float y,
			 float z)
{
	return BLI_hnoise(noisesize, x, y, z);
}

LIBEXPORT float hnoisep(float noisesize,
			  float x,
			  float y,
			  float z)
{
	return BLI_hnoisep(noisesize, x, y, z);
}

LIBEXPORT float turbulence(float noisesize,
				 float x,
				 float y,
				 float z,
				 int depth)
{
	return BLI_turbulence(noisesize, x, y, z, depth);
}

LIBEXPORT float turbulence1(float noisesize,
				  float x,
				  float y,
				  float z,
				  int depth)
{
	return BLI_turbulence1(noisesize, x, y, z, depth);
}

/* -------------------------------------------------------------------------- */

	/* Stupid hack - force the inclusion of all of the
	 * above functions in the binary by 'using' each one...
	 * Otherwise they will not be imported from the archive
	 * library on Unix. -zr
	 */
int pluginapi_force_ref(void); 

int pluginapi_force_ref(void) 
{
	return 
		GET_INT_FROM_POINTER( mallocN ) +
		GET_INT_FROM_POINTER( callocN ) +
		GET_INT_FROM_POINTER( freeN ) +
		GET_INT_FROM_POINTER( mallocT ) +
		GET_INT_FROM_POINTER( callocT ) +
		GET_INT_FROM_POINTER( freeT ) +
		GET_INT_FROM_POINTER( allocImBuf ) +
		GET_INT_FROM_POINTER( dupImBuf ) +
		GET_INT_FROM_POINTER( freeImBuf ) +
		GET_INT_FROM_POINTER( saveiff ) +
		GET_INT_FROM_POINTER( loadifffile ) +
		GET_INT_FROM_POINTER( loadiffname ) +
		GET_INT_FROM_POINTER( testiffname ) +
		GET_INT_FROM_POINTER( onehalf ) +
		GET_INT_FROM_POINTER( half_x ) +
		GET_INT_FROM_POINTER( half_y ) +
		GET_INT_FROM_POINTER( double_x ) +
		GET_INT_FROM_POINTER( double_y ) +
		GET_INT_FROM_POINTER( double_fast_x ) +
		GET_INT_FROM_POINTER( double_fast_y ) +
		GET_INT_FROM_POINTER( ispic ) +
		GET_INT_FROM_POINTER( scaleImBuf ) +
		GET_INT_FROM_POINTER( scalefastImBuf ) +
		GET_INT_FROM_POINTER( hnoise ) +
		GET_INT_FROM_POINTER( hnoisep ) +
		GET_INT_FROM_POINTER( turbulence ) +
		GET_INT_FROM_POINTER( turbulence1 ) +
		GET_INT_FROM_POINTER( de_interlace ) +
		GET_INT_FROM_POINTER( interlace );
}
