/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * Wrappers for the plugin api. This api is up for removal.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "blenpluginapi\util.h"
#else
#include "util.h"
#endif
#include "iff.h"
#include "plugin.h"
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"  /* util and noise functions */
#include "IMB_imbuf.h"    /* image buffer stuff       */

/* -------------------------------------------------------------------------- */
/* stuff from util.h                                                          */ 
/* -------------------------------------------------------------------------- */

void *mallocN(int len, char *str)
{
	return MEM_mallocN(len, str);
}

void *callocN(int len, char *str)
{
	return MEM_callocN(len, str);
}

short freeN(void *vmemh)
{
	return MEM_freeN(vmemh);
}

/* -------------------------------------------------------------------------- */
/* stuff from iff.h                                                           */ 
/* -------------------------------------------------------------------------- */

struct ImBuf *allocImBuf(short x,
						 short y,
						 uchar d,
						 uint flags,
						 uchar bitmap)
{
	return IMB_allocImBuf(x, y, d, flags, bitmap);
}


struct ImBuf *dupImBuf(struct ImBuf *ib)
{
	return IMB_dupImBuf(ib);
}
	
void freeImBuf(struct ImBuf* ib)
{
	IMB_freeImBuf(ib);
}

short converttocmap(struct ImBuf* ibuf)
{
	return IMB_converttocmap(ibuf);
}

short saveiff(struct ImBuf *ib,
			  char *c,
			  int i)
{
	return IMB_saveiff(ib, c, i);
}

struct ImBuf *loadiffmem(int *mem,int flags)
{
	return IMB_loadiffmem(mem, flags);
}
	
struct ImBuf *loadifffile(int a,
						  int b)
{
	return IMB_loadifffile(a, b);
}

struct ImBuf *loadiffname(char *n,
						  int flags)
{
	return IMB_loadiffname(n, flags);
}
	
struct ImBuf *testiffname(char *n,
						  int flags)
{
	return IMB_testiffname(n, flags);
}

struct ImBuf *onehalf(struct ImBuf *ib)
{
	return IMB_onehalf(ib);
}

struct ImBuf *onethird(struct ImBuf *ib)
{
	return IMB_onethird(ib);
}

struct ImBuf *halflace(struct ImBuf *ib)
{
	return IMB_halflace(ib);
}

struct ImBuf *half_x(struct ImBuf *ib)
{
	return IMB_half_x(ib);
}

struct ImBuf *half_y(struct ImBuf *ib)
{
	return IMB_half_y(ib);
}

struct ImBuf *double_x(struct ImBuf *ib)
{
	return IMB_double_x(ib);
}

struct ImBuf *double_y(struct ImBuf *ib)
{
	return IMB_double_y(ib);
}

struct ImBuf *double_fast_x(struct ImBuf *ib)
{
	return IMB_double_fast_x(ib);
}

struct ImBuf *double_fast_y(struct ImBuf *ib)
{
	return IMB_double_fast_y(ib);
}

int ispic(char * name)
{
	return IMB_ispic(name);
}

void dit2(struct ImBuf *ib,
		   short a,
		   short b)
{
	IMB_dit2(ib, a, b);
}

void dit0(struct ImBuf *ib,
		   short a,
		   short b)
{
	IMB_dit0(ib, a, b);
}

/* still the same name */
/*  void (*ditherfunc)(struct ImBuf *, short, short){} */

struct ImBuf *scaleImBuf(struct ImBuf *ib,
						 short nx,
						 short ny)
{
	return IMB_scaleImBuf(ib, nx, ny);
}

struct ImBuf *scalefastImBuf(struct ImBuf *ib,
							 short x,
							 short y)
{
	return IMB_scalefastImBuf(ib, x, y);
}


struct ImBuf *scalefieldImBuf(struct ImBuf *ib,
							  short x,
							  short y)
{
	return IMB_scalefieldImBuf(ib, x, y);
}

struct ImBuf *scalefastfieldImBuf(struct ImBuf *ib,
								  short x,
								  short y)
{
	return IMB_scalefastfieldImBuf(ib, x, y);
}

	/* Extra ones that some NaN (read Ton) plugins use,
	 * even though they aren't in the header
	 */

void interlace(struct ImBuf *ibuf)
{
	IMB_interlace(ibuf);
}

void gamwarp(struct ImBuf *ibuf, double gamma)
{
	IMB_gamwarp(ibuf,gamma);
}
	 
void de_interlace(struct ImBuf *ib)
{
	IMB_de_interlace(ib);
}

void rectop(struct ImBuf *dbuf,
			struct ImBuf *sbuf,
			int destx,
			int desty,
			int srcx,
			int srcy,
			int width,
			int height,
			void (*operation)(unsigned int *, unsigned int*, int, int),
			int value)
{
	IMB_rectop(dbuf, sbuf, destx, desty, srcx, srcy, width, height, operation, value);
}

/* -------------------------------------------------------------------------- */
/* stuff from plugin.h                                                        */ 
/* -------------------------------------------------------------------------- */

/* These three need to be defined in the plugion itself. The plugin
 * loader looks for these functions to check whether it can use the
 * plugin. For sequences, something similar exists. */
/*  int plugin_tex_getversion(void); */
/*  int plugin_seq_getversion(void); */
/*  void plugin_getinfo(PluginInfo *); */

float hnoise(float noisesize,
			 float x,
			 float y,
			 float z)
{
	return BLI_hnoise(noisesize, x, y, z);
}

float hnoisep(float noisesize,
			  float x,
			  float y,
			  float z)
{
	return BLI_hnoisep(noisesize, x, y, z);
}

float turbulence(float noisesize,
				 float x,
				 float y,
				 float z,
				 int depth)
{
	return BLI_turbulence(noisesize, x, y, z, depth);
}

float turbulence1(float noisesize,
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
int pluginapi_force_ref(void) 
{
	return (int) mallocN +
		(int) callocN +
		(int) freeN +
		(int) allocImBuf +
		(int) dupImBuf +
		(int) freeImBuf +
		(int) converttocmap +
		(int) saveiff +
		(int) loadiffmem +
		(int) loadifffile +
		(int) loadiffname +
		(int) testiffname +
		(int) onehalf +
		(int) onethird +
		(int) halflace +
		(int) half_x +
		(int) half_y +
		(int) double_x +
		(int) double_y +
		(int) double_fast_x +
		(int) double_fast_y +
		(int) ispic +
		(int) dit2 +
		(int) dit0 +
		(int) scaleImBuf +
		(int) scalefastImBuf +
		(int) scalefieldImBuf +
		(int) scalefastfieldImBuf +
		(int) hnoise +
		(int) hnoisep +
		(int) turbulence +
		(int) turbulence1 +
		(int) de_interlace +
		(int) interlace +
		(int) gamwarp +
		(int) rectop;
}
