/**
 * Jitter offset table
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_jitter.h"


void BLI_jitterate1(float *jit1, float *jit2, int num, float rad1)
{
	int i , j , k;
	float vecx, vecy, dvecx, dvecy, x, y, len;

	for (i = 2*num-2; i>=0 ; i-=2) {
		dvecx = dvecy = 0.0;
		x = jit1[i];
		y = jit1[i+1];
		for (j = 2*num-2; j>=0 ; j-=2) {
			if (i != j){
				vecx = jit1[j] - x - 1.0;
				vecy = jit1[j+1] - y - 1.0;
				for (k = 3; k>0 ; k--){
					if( fabs(vecx)<rad1 && fabs(vecy)<rad1) {
						len=  sqrt(vecx*vecx + vecy*vecy);
						if(len>0 && len<rad1) {
							len= len/rad1;
							dvecx += vecx/len;
							dvecy += vecy/len;
						}
					}
					vecx += 1.0;

					if( fabs(vecx)<rad1 && fabs(vecy)<rad1) {
						len=  sqrt(vecx*vecx + vecy*vecy);
						if(len>0 && len<rad1) {
							len= len/rad1;
							dvecx += vecx/len;
							dvecy += vecy/len;
						}
					}
					vecx += 1.0;

					if( fabs(vecx)<rad1 && fabs(vecy)<rad1) {
						len=  sqrt(vecx*vecx + vecy*vecy);
						if(len>0 && len<rad1) {
							len= len/rad1;
							dvecx += vecx/len;
							dvecy += vecy/len;
						}
					}
					vecx -= 2.0;
					vecy += 1.0;
				}
			}
		}

		x -= dvecx/18.0 ;
		y -= dvecy/18.0;
		x -= floor(x) ;
		y -= floor(y);
		jit2[i] = x;
		jit2[i+1] = y;
	}
	memcpy(jit1,jit2,2 * num * sizeof(float));
}

void BLI_jitterate2(float *jit1, float *jit2, int num, float rad2)
{
	int i, j;
	float vecx, vecy, dvecx, dvecy, x, y;

	for (i=2*num -2; i>= 0 ; i-=2){
		dvecx = dvecy = 0.0;
		x = jit1[i];
		y = jit1[i+1];
		for (j =2*num -2; j>= 0 ; j-=2){
			if (i != j){
				vecx = jit1[j] - x - 1.0;
				vecy = jit1[j+1] - y - 1.0;

				if( fabs(vecx)<rad2) dvecx+= vecx*rad2;
				vecx += 1.0;
				if( fabs(vecx)<rad2) dvecx+= vecx*rad2;
				vecx += 1.0;
				if( fabs(vecx)<rad2) dvecx+= vecx*rad2;

				if( fabs(vecy)<rad2) dvecy+= vecy*rad2;
				vecy += 1.0;
				if( fabs(vecy)<rad2) dvecy+= vecy*rad2;
				vecy += 1.0;
				if( fabs(vecy)<rad2) dvecy+= vecy*rad2;

			}
		}

		x -= dvecx/2 ;
		y -= dvecy/2;
		x -= floor(x) ;
		y -= floor(y);
		jit2[i] = x;
		jit2[i+1] = y;
	}
	memcpy(jit1,jit2,2 * num * sizeof(float));
}


void BLI_initjit(float *jitarr, int num)
{
	float *jit2, x, rad1, rad2, rad3;
	int i;

	if(num==0) return;

	jit2= MEM_mallocN(12 + 2*sizeof(float)*num, "initjit");
	rad1=  1.0/sqrt((float)num);
	rad2= 1.0/((float)num);
	rad3= sqrt((float)num)/((float)num);

	BLI_srand(31415926 + num);
	x= 0;
	for(i=0; i<2*num; i+=2) {
		jitarr[i]= x+ rad1*(0.5-BLI_drand());
		jitarr[i+1]= ((float)i/2)/num +rad1*(0.5-BLI_drand());
		x+= rad3;
		x -= floor(x);
	}

	for (i=0 ; i<24 ; i++) {
		BLI_jitterate1(jitarr, jit2, num, rad1);
		BLI_jitterate1(jitarr, jit2, num, rad1);
		BLI_jitterate2(jitarr, jit2, num, rad2);
	}

	MEM_freeN(jit2);
	
	/* finally, move jittertab to be centered around (0,0) */
	for(i=0; i<2*num; i+=2) {
		jitarr[i] -= 0.5;
		jitarr[i+1] -= 0.5;
	}
	
}


/* eof */
