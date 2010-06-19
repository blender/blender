 /**
 * $Id$
 *
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
 */

#include "math.h"
#include "plugin.h"

/* ******************** GLOBAL VARIABLES ***************** */

char name[]= "tiles";

/* Subtype names must be less than 15 characters */

#define NR_TYPES	2
char stnames[NR_TYPES][16]= {"Square", "Deformed"};

VarStruct varstr[]= {
	 NUM|FLO,	"size",			1.0,	 0.0, 1.0,  "The size of each tile", 
	 NUM|FLO,	"Noise",		1.0,	 0.01, 10.0, ""
};

/* The cast struct is for input in the main doit function
   Varstr and Cast must have the same variables in the same order */ 

typedef struct Cast {
	float size;
	float noise;
} Cast;

/* result: 
   Intensity, R, G, B, Alpha, nor.x, nor.y, nor.z
 */

float result[8];

/* cfra: the current frame */

float cfra;

int plugin_tex_doit(int, Cast *, float *, float *, float *, float *);
void plugin_instance_init(Cast*);

/* ******************** Fixed functions ***************** */

int plugin_tex_getversion(void) 
{	
	return B_PLUGIN_VERSION;
}

void plugin_but_changed(int but) 
{
}

void plugin_init(void)
{
}

/* 
 * initialize any data for a particular instance of
 * the plugin here
 */
void plugin_instance_init(Cast *cast)
{
}

/* this function should not be changed: */

void plugin_getinfo(PluginInfo *info)
{
	info->name= name;
	info->stypes= NR_TYPES;
	info->nvars= sizeof(varstr)/sizeof(VarStruct);
	
	info->snames= stnames[0];
	info->result= result;
	info->cfra= &cfra;
	info->varstr= varstr;

	info->init= plugin_init;
	info->tex_doit=  (TexDoit) plugin_tex_doit;
	info->callback= plugin_but_changed;
	info->instance_init= (void (*)(void *)) plugin_instance_init;

}

/* ************************************************************
	Tiles
	
	Demonstration of a simple square wave function sampled
	with anti-aliasing.
	It is not mipmapped yet...
	
   ************************************************************ */


/* square wave, antialiased, no mipmap! */

float sample_wave(float freq, float coord, float pixsize)
{
	float fac, frac,  retval;
	int part1, part2;
	
	if(pixsize > freq) return 0.5;
	
	pixsize/= freq;
	
	fac= coord/freq;
	part1= ffloor(fac);
	frac= fac - part1;

	if(part1 & 1) retval= 0.0;
	else retval= 1.0;
	
	if(pixsize != 0.0) {
		
		/* is coord+pixsize another value? */
		
		part2= ffloor(fac + pixsize);
		if(part1==part2) return retval;
		
		/* antialias */	
		if(retval==1.0) retval= (1.0-frac)/pixsize;
		else retval= 1.0-(1.0-frac)/pixsize;
	}
	return retval;
}

int plugin_tex_doit(int stype, Cast *cast, float *texvec, float *dxt, float *dyt, float *result)
{
	float xwave, ywave;
	
	if(stype==1) {
		texvec[0]+= hnoise(cast->noise, texvec[0], texvec[1], texvec[2]);
		texvec[1]+= hnoise(cast->noise, texvec[1], texvec[2], texvec[0]);
	}
	
	if(dxt && dyt) {
		xwave= sample_wave(cast->size, texvec[0], fabs(dxt[0]) + fabs(dyt[0]) );
		ywave= sample_wave(cast->size, texvec[1], fabs(dxt[1]) + fabs(dyt[1]) );

		if(xwave > ywave) result[0]= xwave-ywave;
		else result[0]= ywave-xwave;
	} 
	else {
		xwave= sample_wave(cast->size, texvec[0], 0.0 );
		ywave= sample_wave(cast->size, texvec[1], 0.0 );
		
		if(xwave > ywave) result[0]= xwave-ywave;
		else result[0]= ywave-xwave;
	}

	return TEX_INT;
}
