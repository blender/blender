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
 * Contributor(s): 2004-2006, Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/shadbuf.c
 *  \ingroup render
 */


#include <math.h>
#include <string.h>


#include "MEM_guardedalloc.h"

#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"

#include "BKE_global.h"
#include "BKE_scene.h"


#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_jitter.h"
#include "BLI_memarena.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "renderpipeline.h"
#include "render_types.h"
#include "renderdatabase.h"
#include "rendercore.h"
#include "shadbuf.h"
#include "shading.h"
#include "zbuf.h"

/* XXX, could be better implemented... this is for endian issues
*/
#ifdef __BIG_ENDIAN__
#  define RCOMP	3
#  define GCOMP	2
#  define BCOMP	1
#  define ACOMP	0
#else
#  define RCOMP	0
#  define GCOMP	1
#  define BCOMP	2
#  define ACOMP	3
#endif

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ------------------------------------------------------------------------- */

/* initshadowbuf() in convertBlenderScene.c */

/* ------------------------------------------------------------------------- */

static void copy_to_ztile(int *rectz, int size, int x1, int y1, int tile, char *r1)
{
	int len4, *rz;	
	int x2, y2;
	
	x2= x1+tile;
	y2= y1+tile;
	if(x2>=size) x2= size-1;
	if(y2>=size) y2= size-1;

	if(x1>=x2 || y1>=y2) return;

	len4= 4*(x2- x1);
	rz= rectz + size*y1 + x1;
	for(; y1<y2; y1++) {
		memcpy(r1, rz, len4);
		rz+= size;
		r1+= len4;
	}
}

#if 0
static int sizeoflampbuf(ShadBuf *shb)
{
	int num,count=0;
	char *cp;
	
	cp= shb->cbuf;
	num= (shb->size*shb->size)/256;

	while(num--) count+= *(cp++);
	
	return 256*count;
}
#endif

/* not threadsafe... */
static float *give_jitter_tab(int samp)
{
	/* these are all possible jitter tables, takes up some
	 * 12k, not really bad!
	 * For soft shadows, it saves memory and render time
	 */
	static int tab[17]={1, 4, 9, 16, 25, 36, 49, 64, 81, 100, 121, 144, 169, 196, 225, 256};
	static float jit[1496][2];
	static char ctab[17]= {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int a, offset=0;
	
	if(samp<2) samp= 2;
	else if(samp>16) samp= 16;

	for(a=0; a<samp-1; a++) offset+= tab[a];

	if(ctab[samp]==0) {
		ctab[samp]= 1;
		BLI_initjit(jit[offset], samp*samp);
	}
		
	return jit[offset];
	
}

static void make_jitter_weight_tab(Render *re, ShadBuf *shb, short filtertype) 
{
	float *jit, totw= 0.0f;
	int samp= get_render_shadow_samples(&re->r, shb->samp);
	int a, tot=samp*samp;
	
	shb->weight= MEM_mallocN(sizeof(float)*tot, "weight tab lamp");
	
	for(jit= shb->jit, a=0; a<tot; a++, jit+=2) {
		if(filtertype==LA_SHADBUF_TENT) 
			shb->weight[a]= 0.71f - sqrt(jit[0]*jit[0] + jit[1]*jit[1]);
		else if(filtertype==LA_SHADBUF_GAUSS) 
			shb->weight[a]= RE_filter_value(R_FILTER_GAUSS, 1.8f*sqrt(jit[0]*jit[0] + jit[1]*jit[1]));
		else
			shb->weight[a]= 1.0f;
		
		totw+= shb->weight[a];
	}
	
	totw= 1.0f/totw;
	for(a=0; a<tot; a++) {
		shb->weight[a]*= totw;
	}
}

static int verg_deepsample(const void *poin1, const void *poin2)
{
	const DeepSample *ds1= (const DeepSample*)poin1;
	const DeepSample *ds2= (const DeepSample*)poin2;

	if(ds1->z < ds2->z) return -1;
	else if(ds1->z == ds2->z) return 0;
	else return 1;
}

static int compress_deepsamples(DeepSample *dsample, int tot, float epsilon)
{
	/* uses doubles to avoid overflows and other numerical issues,
	   could be improved */
	DeepSample *ds, *newds;
	float v;
	double slope, slopemin, slopemax, min, max, div, newmin, newmax;
	int a, first, z, newtot= 0;

	/*if(print) {
		for(a=0, ds=dsample; a<tot; a++, ds++)
			printf("%lf,%f ", ds->z/(double)0x7FFFFFFF, ds->v);
		printf("\n");
	}*/

	/* read from and write into same array */
	ds= dsample;
	newds= dsample;
	a= 0;

	/* as long as we are not at the end of the array */
	for(a++, ds++; a<tot; a++, ds++) {
		slopemin= 0.0f;
		slopemax= 0.0f;
		first= 1;

		for(; a<tot; a++, ds++) {
			//dz= ds->z - newds->z;
			if(ds->z == newds->z) {
				/* still in same z position, simply check
				   visibility difference against epsilon */
				if(!(fabs(newds->v - ds->v) <= epsilon)) {
					break;
				}
			}
			else {
				/* compute slopes */
				div= (double)0x7FFFFFFF/((double)ds->z - (double)newds->z);
				min= ((ds->v - epsilon) - newds->v)*div;
				max= ((ds->v + epsilon) - newds->v)*div;

				/* adapt existing slopes */
				if(first) {
					newmin= min;
					newmax= max;
					first= 0;
				}
				else {
					newmin= MAX2(slopemin, min);
					newmax= MIN2(slopemax, max);

					/* verify if there is still space between the slopes */
					if(newmin > newmax) {
						ds--;
						a--;
						break;
					}
				}

				slopemin= newmin;
				slopemax= newmax;
			}
		}

		if(a == tot) {
			ds--;
			a--;
		}

		/* always previous z */
		z= ds->z;

		if(first || a==tot-1) {
			/* if slopes were not initialized, use last visibility */
			v= ds->v;
		}
		else {
			/* compute visibility at center between slopes at z */
			slope= (slopemin+slopemax)*0.5f;
			v= newds->v + slope*((z - newds->z)/(double)0x7FFFFFFF);
		}

		newds++;
		newtot++;

		newds->z= z;
		newds->v= v;
	}

	if(newtot == 0 || (newds->v != (newds-1)->v))
		newtot++;

	/*if(print) {
		for(a=0, ds=dsample; a<newtot; a++, ds++)
			printf("%lf,%f ", ds->z/(double)0x7FFFFFFF, ds->v);
		printf("\n");
	}*/

	return newtot;
}

static float deep_alpha(Render *re, int obinr, int facenr, int strand)
{
	ObjectInstanceRen *obi= &re->objectinstance[obinr];
	Material *ma;

	if(strand) {
		StrandRen *strand= RE_findOrAddStrand(obi->obr, facenr-1);
		ma= strand->buffer->ma;
	}
	else {
		VlakRen *vlr= RE_findOrAddVlak(obi->obr, (facenr-1) & RE_QUAD_MASK);
		ma= vlr->mat;
	}

	return ma->shad_alpha;
}

static void compress_deepshadowbuf(Render *re, ShadBuf *shb, APixstr *apixbuf, APixstrand *apixbufstrand)
{
	ShadSampleBuf *shsample;
	DeepSample *ds[RE_MAX_OSA], *sampleds[RE_MAX_OSA], *dsb, *newbuf;
	APixstr *ap, *apn;
	APixstrand *aps, *apns;
	float visibility;

	const int totbuf= shb->totbuf;
	const float totbuf_f= (float)shb->totbuf;
	const float totbuf_f_inv= 1.0f/totbuf_f;
	const int size= shb->size;

	int a, b, c, tot, minz, found, prevtot, newtot;
	int sampletot[RE_MAX_OSA], totsample = 0, totsamplec = 0;
	
	shsample= MEM_callocN( sizeof(ShadSampleBuf), "shad sample buf");
	BLI_addtail(&shb->buffers, shsample);

	shsample->totbuf= MEM_callocN(sizeof(int)*size*size, "deeptotbuf");
	shsample->deepbuf= MEM_callocN(sizeof(DeepSample*)*size*size, "deepbuf");

	ap= apixbuf;
	aps= apixbufstrand;
	for(a=0; a<size*size; a++, ap++, aps++) {
		/* count number of samples */
		for(c=0; c<totbuf; c++)
			sampletot[c]= 0;

		tot= 0;
		for(apn=ap; apn; apn=apn->next)
			for(b=0; b<4; b++)
				if(apn->p[b])
					for(c=0; c<totbuf; c++)
						if(apn->mask[b] & (1<<c))
							sampletot[c]++;

		if(apixbufstrand) {
			for(apns=aps; apns; apns=apns->next)
				for(b=0; b<4; b++)
					if(apns->p[b])
						for(c=0; c<totbuf; c++)
							if(apns->mask[b] & (1<<c))
								sampletot[c]++;
		}

		for(c=0; c<totbuf; c++)
			tot += sampletot[c];

		if(tot == 0) {
			shsample->deepbuf[a]= NULL;
			shsample->totbuf[a]= 0;
			continue;
		}

		/* fill samples */
		ds[0]= sampleds[0]= MEM_callocN(sizeof(DeepSample)*tot*2, "deepsample");
		for(c=1; c<totbuf; c++)
			ds[c]= sampleds[c]= sampleds[c-1] + sampletot[c-1]*2;

		for(apn=ap; apn; apn=apn->next) {
			for(b=0; b<4; b++) {
				if(apn->p[b]) {
					for(c=0; c<totbuf; c++) {
						if(apn->mask[b] & (1<<c)) {
							/* two entries to create step profile */
							ds[c]->z= apn->z[b];
							ds[c]->v= 1.0f; /* not used */
							ds[c]++;
							ds[c]->z= apn->z[b];
							ds[c]->v= deep_alpha(re, apn->obi[b], apn->p[b], 0);
							ds[c]++;
						}
					}
				}
			}
		}

		if(apixbufstrand) {
			for(apns=aps; apns; apns=apns->next) {
				for(b=0; b<4; b++) {
					if(apns->p[b]) {
						for(c=0; c<totbuf; c++) {
							if(apns->mask[b] & (1<<c)) {
								/* two entries to create step profile */
								ds[c]->z= apns->z[b];
								ds[c]->v= 1.0f; /* not used */
								ds[c]++;
								ds[c]->z= apns->z[b];
								ds[c]->v= deep_alpha(re, apns->obi[b], apns->p[b], 1);
								ds[c]++;
							}
						}
					}
				}
			}
		}

		for(c=0; c<totbuf; c++) {
			/* sort by increasing z */
			qsort(sampleds[c], sampletot[c], sizeof(DeepSample)*2, verg_deepsample);

			/* sum visibility, replacing alpha values */
			visibility= 1.0f;
			ds[c]= sampleds[c];

			for(b=0; b<sampletot[c]; b++) {
				/* two entries creating step profile */
				ds[c]->v= visibility;
				ds[c]++;

				visibility *= 1.0f-ds[c]->v;
				ds[c]->v= visibility;
				ds[c]++;
			}

			/* halfway trick, probably won't work well for volumes? */
			ds[c]= sampleds[c];
			for(b=0; b<sampletot[c]; b++) {
				if(b+1 < sampletot[c]) {
					ds[c]->z= (ds[c]->z>>1) + ((ds[c]+2)->z>>1);
					ds[c]++;
					ds[c]->z= (ds[c]->z>>1) + ((ds[c]+2)->z>>1);
					ds[c]++;
				}
				else {
					ds[c]->z= (ds[c]->z>>1) + (0x7FFFFFFF>>1);
					ds[c]++;
					ds[c]->z= (ds[c]->z>>1) + (0x7FFFFFFF>>1);
					ds[c]++;
				}
			}

			/* init for merge loop */
			ds[c]= sampleds[c];
			sampletot[c] *= 2;
		}

		shsample->deepbuf[a]= MEM_callocN(sizeof(DeepSample)*tot*2, "deepsample");
		shsample->totbuf[a]= 0;

		/* merge buffers */
		dsb= shsample->deepbuf[a];
		while(1) {
			minz= 0;
			found= 0;

			for(c=0; c<totbuf; c++) {
				if(sampletot[c] && (!found || ds[c]->z < minz)) {
					minz= ds[c]->z;
					found= 1;
				}
			}

			if(!found)
				break;

			dsb->z= minz;
			dsb->v= 0.0f;

			visibility= 0.0f;
			for(c=0; c<totbuf; c++) {
				if(sampletot[c] && ds[c]->z == minz) {
					ds[c]++;
					sampletot[c]--;
				}

				if(sampleds[c] == ds[c])
					visibility += totbuf_f_inv;
				else
					visibility += (ds[c]-1)->v / totbuf_f;
			}

			dsb->v= visibility;
			dsb++;
			shsample->totbuf[a]++;
		}

		prevtot= shsample->totbuf[a];
		totsample += prevtot;

		newtot= compress_deepsamples(shsample->deepbuf[a], prevtot, shb->compressthresh);
		shsample->totbuf[a]= newtot;
		totsamplec += newtot;

		if(newtot < prevtot) {
			newbuf= MEM_mallocN(sizeof(DeepSample)*newtot, "cdeepsample");
			memcpy(newbuf, shsample->deepbuf[a], sizeof(DeepSample)*newtot);
			MEM_freeN(shsample->deepbuf[a]);
			shsample->deepbuf[a]= newbuf;
		}

		MEM_freeN(sampleds[0]);
	}

	//printf("%d -> %d, ratio %f\n", totsample, totsamplec, (float)totsamplec/(float)totsample);
}

/* create Z tiles (for compression): this system is 24 bits!!! */
static void compress_shadowbuf(ShadBuf *shb, int *rectz, int square)
{
	ShadSampleBuf *shsample;
	float dist;
	uintptr_t *ztile;
	int *rz, *rz1, verg, verg1, size= shb->size;
	int a, x, y, minx, miny, byt1, byt2;
	char *rc, *rcline, *ctile, *zt;
	
	shsample= MEM_callocN( sizeof(ShadSampleBuf), "shad sample buf");
	BLI_addtail(&shb->buffers, shsample);
	
	shsample->zbuf= MEM_mallocN( sizeof(uintptr_t)*(size*size)/256, "initshadbuf2");
	shsample->cbuf= MEM_callocN( (size*size)/256, "initshadbuf3");
	
	ztile= (uintptr_t *)shsample->zbuf;
	ctile= shsample->cbuf;
	
	/* help buffer */
	rcline= MEM_mallocN(256*4+sizeof(int), "makeshadbuf2");
	
	for(y=0; y<size; y+=16) {
		if(y< size/2) miny= y+15-size/2;
		else miny= y-size/2;	
		
		for(x=0; x<size; x+=16) {
			
			/* is tile within spotbundle? */
			a= size/2;
			if(x< a) minx= x+15-a;
			else minx= x-a;	
			
			dist= sqrt( (float)(minx*minx+miny*miny) );
			
			if(square==0 && dist>(float)(a+12)) {	/* 12, tested with a onlyshadow lamp */
				a= 256; verg= 0; /* 0x80000000; */ /* 0x7FFFFFFF; */
				rz1= (&verg)+1;
			} 
			else {
				copy_to_ztile(rectz, size, x, y, 16, rcline);
				rz1= (int *)rcline;
				
				verg= (*rz1 & 0xFFFFFF00);
				
				for(a=0;a<256;a++,rz1++) {
					if( (*rz1 & 0xFFFFFF00) !=verg) break;
				}
			}
			if(a==256) { /* complete empty tile */
				*ctile= 0;
				*ztile= *(rz1-1);
			}
			else {
				
				/* ACOMP etc. are defined to work L/B endian */
				
				rc= rcline;
				rz1= (int *)rcline;
				verg=  rc[ACOMP];
				verg1= rc[BCOMP];
				rc+= 4;
				byt1= 1; byt2= 1;
				for(a=1;a<256;a++,rc+=4) {
					byt1 &= (verg==rc[ACOMP]);
					byt2 &= (verg1==rc[BCOMP]);
					
					if(byt1==0) break;
				}
				if(byt1 && byt2) {	/* only store byte */
					*ctile= 1;
					*ztile= (uintptr_t)MEM_mallocN(256+4, "tile1");
					rz= (int *)*ztile;
					*rz= *rz1;
					
					zt= (char *)(rz+1);
					rc= rcline;
					for(a=0; a<256; a++, zt++, rc+=4) *zt= rc[GCOMP];	
				}
				else if(byt1) {		/* only store short */
					*ctile= 2;
					*ztile= (uintptr_t)MEM_mallocN(2*256+4,"Tile2");
					rz= (int *)*ztile;
					*rz= *rz1;
					
					zt= (char *)(rz+1);
					rc= rcline;
					for(a=0; a<256; a++, zt+=2, rc+=4) {
						zt[0]= rc[BCOMP];
						zt[1]= rc[GCOMP];
					}
				}
				else {			/* store triple */
					*ctile= 3;
					*ztile= (uintptr_t)MEM_mallocN(3*256,"Tile3");

					zt= (char *)*ztile;
					rc= rcline;
					for(a=0; a<256; a++, zt+=3, rc+=4) {
						zt[0]= rc[ACOMP];
						zt[1]= rc[BCOMP];
						zt[2]= rc[GCOMP];
					}
				}
			}
			ztile++;
			ctile++;
		}
	}

	MEM_freeN(rcline);
}

/* sets start/end clipping. lar->shb should be initialized */
static void shadowbuf_autoclip(Render *re, LampRen *lar)
{
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	VlakRen *vlr= NULL;
	VertRen *ver= NULL;
	Material *ma= NULL;
	float minz, maxz, vec[3], viewmat[4][4], obviewmat[4][4];
	unsigned int lay = -1;
	int i, a, maxtotvert, ok= 1;
	char *clipflag;
	
	minz= 1.0e30f; maxz= -1.0e30f;
	copy_m4_m4(viewmat, lar->shb->viewmat);
	
	if(lar->mode & (LA_LAYER|LA_LAYER_SHADOW)) lay= lar->lay;

	maxtotvert= 0;
	for(obr=re->objecttable.first; obr; obr=obr->next)
		maxtotvert= MAX2(obr->totvert, maxtotvert);

	clipflag= MEM_callocN(sizeof(char)*maxtotvert, "autoclipflag");

	/* set clip in vertices when face visible */
	for(i=0, obi=re->instancetable.first; obi; i++, obi=obi->next) {
		obr= obi->obr;

		if(obi->flag & R_TRANSFORMED)
			mult_m4_m4m4(obviewmat, viewmat, obi->mat);
		else
			copy_m4_m4(obviewmat, viewmat);

		memset(clipflag, 0, sizeof(char)*obr->totvert);

		/* clear clip, is being set if face is visible (clip is calculated for real later) */
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak;
			else vlr++;
			
			/* note; these conditions are copied from zbuffer_shadow() */
			if(vlr->mat!= ma) {
				ma= vlr->mat;
				ok= 1;
				if((ma->mode & MA_SHADBUF)==0) ok= 0;
			}
			
			if(ok && (obi->lay & lay)) {
				clipflag[vlr->v1->index]= 1;
				clipflag[vlr->v2->index]= 1;
				clipflag[vlr->v3->index]= 1;
				if(vlr->v4) clipflag[vlr->v4->index]= 1;
			}				
		}		
		
		/* calculate min and max */
		for(a=0; a< obr->totvert;a++) {
			if((a & 255)==0) ver= RE_findOrAddVert(obr, a);
			else ver++;
			
			if(clipflag[a]) {
				copy_v3_v3(vec, ver->co);
				mul_m4_v3(obviewmat, vec);
				/* Z on visible side of lamp space */
				if(vec[2] < 0.0f) {
					float inpr, z= -vec[2];
					
					/* since vec is rotated in lampspace, this is how to get the cosine of angle */
					/* precision is set 20% larger */
					vec[2]*= 1.2f;
					normalize_v3(vec);
					inpr= - vec[2];

					if(inpr>=lar->spotsi) {
						if(z<minz) minz= z;
						if(z>maxz) maxz= z;
					}
				}
			}
		}
	}

	MEM_freeN(clipflag);
	
	/* set clipping min and max */
	if(minz < maxz) {
		float delta= (maxz - minz);	/* threshold to prevent precision issues */
		
		//printf("minz %f maxz %f delta %f\n", minz, maxz, delta);
		if(lar->bufflag & LA_SHADBUF_AUTO_START)
			lar->shb->d= minz - delta*0.02f;	/* 0.02 is arbitrary... needs more thinking! */
		if(lar->bufflag & LA_SHADBUF_AUTO_END)
			lar->shb->clipend= maxz + delta*0.1f;
		
		/* bias was calculated as percentage, we scale it to prevent animation issues */
		delta= (lar->clipend-lar->clipsta)/(lar->shb->clipend-lar->shb->d);
		//printf("bias delta %f\n", delta);
		lar->shb->bias= (int) (delta*(float)lar->shb->bias);
	}
}

static void makeflatshadowbuf(Render *re, LampRen *lar, float *jitbuf)
{
	ShadBuf *shb= lar->shb;
	int *rectz, samples;

	/* zbuffering */
	rectz= MEM_mapallocN(sizeof(int)*shb->size*shb->size, "makeshadbuf");
	
	for(samples=0; samples<shb->totbuf; samples++) {
		zbuffer_shadow(re, shb->persmat, lar, rectz, shb->size, jitbuf[2*samples], jitbuf[2*samples+1]);
		/* create Z tiles (for compression): this system is 24 bits!!! */
		compress_shadowbuf(shb, rectz, lar->mode & LA_SQUARE);

		if(re->test_break(re->tbh))
			break;
	}
	
	MEM_freeN(rectz);
}

static void makedeepshadowbuf(Render *re, LampRen *lar, float *jitbuf)
{
	ShadBuf *shb= lar->shb;
	APixstr *apixbuf;
	APixstrand *apixbufstrand= NULL;
	ListBase apsmbase= {NULL, NULL};

	/* zbuffering */
	apixbuf= MEM_callocN(sizeof(APixstr)*shb->size*shb->size, "APixbuf");
	if(re->totstrand)
		apixbufstrand= MEM_callocN(sizeof(APixstrand)*shb->size*shb->size, "APixbufstrand");

	zbuffer_abuf_shadow(re, lar, shb->persmat, apixbuf, apixbufstrand, &apsmbase, shb->size,
		shb->totbuf, (float(*)[2])jitbuf);

	/* create Z tiles (for compression): this system is 24 bits!!! */
	compress_deepshadowbuf(re, shb, apixbuf, apixbufstrand);
	
	MEM_freeN(apixbuf);
	if(apixbufstrand)
		MEM_freeN(apixbufstrand);
	freepsA(&apsmbase);
}

void makeshadowbuf(Render *re, LampRen *lar)
{
	ShadBuf *shb= lar->shb;
	float wsize, *jitbuf, twozero[2]= {0.0f, 0.0f}, angle, temp;
	
	if(lar->bufflag & (LA_SHADBUF_AUTO_START|LA_SHADBUF_AUTO_END))
		shadowbuf_autoclip(re, lar);
	
	/* just to enforce identical behaviour of all irregular buffers */
	if(lar->buftype==LA_SHADBUF_IRREGULAR)
		shb->size= 1024;
	
	/* matrices and window: in winmat the transformation is being put,
		transforming from observer view to lamp view, including lamp window matrix */
	
	angle= saacos(lar->spotsi);
	temp= 0.5f*shb->size*cos(angle)/sin(angle);
	shb->pixsize= (shb->d)/temp;
	wsize= shb->pixsize*(shb->size/2.0f);
	
	perspective_m4( shb->winmat,-wsize, wsize, -wsize, wsize, shb->d, shb->clipend);
	mult_m4_m4m4(shb->persmat, shb->winmat, shb->viewmat);

	if(ELEM3(lar->buftype, LA_SHADBUF_REGULAR, LA_SHADBUF_HALFWAY, LA_SHADBUF_DEEP)) {
		shb->totbuf= lar->buffers;

		/* jitter, weights - not threadsafe! */
		BLI_lock_thread(LOCK_CUSTOM1);
		shb->jit= give_jitter_tab(get_render_shadow_samples(&re->r, shb->samp));
		make_jitter_weight_tab(re, shb, lar->filtertype);
		BLI_unlock_thread(LOCK_CUSTOM1);
		
		if(shb->totbuf==4) jitbuf= give_jitter_tab(2);
		else if(shb->totbuf==9) jitbuf= give_jitter_tab(3);
		else jitbuf= twozero;
		
		/* zbuffering */
		if(lar->buftype == LA_SHADBUF_DEEP) {
			makedeepshadowbuf(re, lar, jitbuf);
			shb->totbuf= 1;
		}
		else
			makeflatshadowbuf(re, lar, jitbuf);

		/* printf("lampbuf %d\n", sizeoflampbuf(shb)); */
	}
}

static void *do_shadow_thread(void *re_v)
{
	Render *re= (Render*)re_v;
	LampRen *lar;

	do {
		BLI_lock_thread(LOCK_CUSTOM1);
		for(lar=re->lampren.first; lar; lar=lar->next) {
			if(lar->shb && !lar->thread_assigned) {
				lar->thread_assigned= 1;
				break;
			}
		}
		BLI_unlock_thread(LOCK_CUSTOM1);

		/* if type is irregular, this only sets the perspective matrix and autoclips */
		if(lar) {
			makeshadowbuf(re, lar);
			BLI_lock_thread(LOCK_CUSTOM1);
			lar->thread_ready= 1;
			BLI_unlock_thread(LOCK_CUSTOM1);
		}
	} while(lar && !re->test_break(re->tbh));

	return NULL;
}

static volatile int g_break= 0;
static int thread_break(void *UNUSED(arg))
{
	return g_break;
}

void threaded_makeshadowbufs(Render *re)
{
	ListBase threads;
	LampRen *lar;
	int a, totthread= 0;
	int (*test_break)(void *);

	/* count number of threads to use */
	if(G.rendering) {
		for(lar=re->lampren.first; lar; lar= lar->next)
			if(lar->shb)
				totthread++;
		
		totthread= MIN2(totthread, re->r.threads);
	}
	else
		totthread= 1; /* preview render */

	if(totthread <= 1) {
		for(lar=re->lampren.first; lar; lar= lar->next) {
			if(re->test_break(re->tbh)) break;
			if(lar->shb) {
				/* if type is irregular, this only sets the perspective matrix and autoclips */
				makeshadowbuf(re, lar);
			}
		}
	}
	else {
		/* swap test break function */
		test_break= re->test_break;
		re->test_break= thread_break;

		for(lar=re->lampren.first; lar; lar= lar->next) {
			lar->thread_assigned= 0;
			lar->thread_ready= 0;
		}

		BLI_init_threads(&threads, do_shadow_thread, totthread);
		
		for(a=0; a<totthread; a++)
			BLI_insert_thread(&threads, re);

		/* keep rendering as long as there are shadow buffers not ready */
		do {
			if((g_break=test_break(re->tbh)))
				break;

			PIL_sleep_ms(50);

			BLI_lock_thread(LOCK_CUSTOM1);
			for(lar=re->lampren.first; lar; lar= lar->next)
				if(lar->shb && !lar->thread_ready)
					break;
			BLI_unlock_thread(LOCK_CUSTOM1);
		} while(lar);
	
		BLI_end_threads(&threads);

		/* unset threadsafety */
		re->test_break= test_break;
		g_break= 0;
	}
}

void freeshadowbuf(LampRen *lar)
{
	if(lar->shb) {
		ShadBuf *shb= lar->shb;
		ShadSampleBuf *shsample;
		int b, v;
		
		for(shsample= shb->buffers.first; shsample; shsample= shsample->next) {
			if(shsample->deepbuf) {
				v= shb->size*shb->size;
				for(b=0; b<v; b++)
					if(shsample->deepbuf[b])
						MEM_freeN(shsample->deepbuf[b]);
					
				MEM_freeN(shsample->deepbuf);
				MEM_freeN(shsample->totbuf);
			}
			else {
				intptr_t *ztile= shsample->zbuf;
				char *ctile= shsample->cbuf;
				
				v= (shb->size*shb->size)/256;
				for(b=0; b<v; b++, ztile++, ctile++)
					if(*ctile) MEM_freeN((void *) *ztile);
				
				MEM_freeN(shsample->zbuf);
				MEM_freeN(shsample->cbuf);
			}
		}
		BLI_freelistN(&shb->buffers);
		
		if(shb->weight) MEM_freeN(shb->weight);
		MEM_freeN(lar->shb);
		
		lar->shb= NULL;
	}
}


static int firstreadshadbuf(ShadBuf *shb, ShadSampleBuf *shsample, int **rz, int xs, int ys, int nr)
{
	/* return a 1 if fully compressed shadbuf-tile && z==const */
	int ofs;
	char *ct;

	if(shsample->deepbuf)
		return 0;

	/* always test borders of shadowbuffer */
	if(xs<0) xs= 0; else if(xs>=shb->size) xs= shb->size-1;
	if(ys<0) ys= 0; else if(ys>=shb->size) ys= shb->size-1;

	/* calc z */
	ofs= (ys>>4)*(shb->size>>4) + (xs>>4);
	ct= shsample->cbuf+ofs;
	if(*ct==0) {
		if(nr==0) {
			*rz= *( (int **)(shsample->zbuf+ofs) );
			return 1;
		}
		else if(*rz!= *( (int **)(shsample->zbuf+ofs) )) return 0;
		
		return 1;
	}
	
	return 0;
}

static float readdeepvisibility(DeepSample *dsample, int tot, int z, int bias, float *biast)
{
	DeepSample *ds, *prevds;
	float t;
	int a;

	/* tricky stuff here; we use ints which can overflow easily with bias values */

	ds= dsample;
	for(a=0; a<tot && (z-bias > ds->z); a++, ds++)
		;

	if(a == tot) {
		if(biast)
			*biast= 0.0f;
		return (ds-1)->v; /* completely behind all samples */
	}
	
	/* check if this read needs bias blending */
	if(biast) {
		if(z > ds->z)
			*biast= (float)(z - ds->z)/(float)bias;
		else
			*biast= 0.0f;
	}

	if(a == 0)
		return 1.0f; /* completely in front of all samples */

	/* converting to float early here because ds->z - prevds->z can overflow */
	prevds= ds-1;
	t= ((float)(z-bias) - (float)prevds->z)/((float)ds->z - (float)prevds->z);
	return t*ds->v + (1.0f-t)*prevds->v;
}

static float readdeepshadowbuf(ShadBuf *shb, ShadSampleBuf *shsample, int bias, int xs, int ys, int zs)
{
	float v, biasv, biast;
	int ofs, tot;

	if(zs < - 0x7FFFFE00 + bias)
		return 1.0;	/* extreme close to clipstart */

	/* calc z */
	ofs= ys*shb->size + xs;
	tot= shsample->totbuf[ofs];
	if(tot == 0)
		return 1.0f;

	v= readdeepvisibility(shsample->deepbuf[ofs], tot, zs, bias, &biast);

	if(biast != 0.0f) {
		/* in soft bias area */
		biasv= readdeepvisibility(shsample->deepbuf[ofs], tot, zs, 0, 0);

		biast= biast*biast;
		return (1.0f-biast)*v + biast*biasv;
	}

	return v;
}

/* return 1.0 : fully in light */
static float readshadowbuf(ShadBuf *shb, ShadSampleBuf *shsample, int bias, int xs, int ys, int zs)	
{
	float temp;
	int *rz, ofs;
	int zsamp=0;
	char *ct, *cz;

	/* simpleclip */
	/* if(xs<0 || ys<0) return 1.0; */
	/* if(xs>=shb->size || ys>=shb->size) return 1.0; */
	
	/* always test borders of shadowbuffer */
	if(xs<0) xs= 0; else if(xs>=shb->size) xs= shb->size-1;
	if(ys<0) ys= 0; else if(ys>=shb->size) ys= shb->size-1;

	if(shsample->deepbuf)
		return readdeepshadowbuf(shb, shsample, bias, xs, ys, zs);

	/* calc z */
	ofs= (ys>>4)*(shb->size>>4) + (xs>>4);
	ct= shsample->cbuf+ofs;
	rz= *( (int **)(shsample->zbuf+ofs) );

	if(*ct==3) {
		ct= ((char *)rz)+3*16*(ys & 15)+3*(xs & 15);
		cz= (char *)&zsamp;
		cz[ACOMP]= ct[0];
		cz[BCOMP]= ct[1];
		cz[GCOMP]= ct[2];
	}
	else if(*ct==2) {
		ct= ((char *)rz);
		ct+= 4+2*16*(ys & 15)+2*(xs & 15);
		zsamp= *rz;
	
		cz= (char *)&zsamp;
		cz[BCOMP]= ct[0];
		cz[GCOMP]= ct[1];
	}
	else if(*ct==1) {
		ct= ((char *)rz);
		ct+= 4+16*(ys & 15)+(xs & 15);
		zsamp= *rz;

		cz= (char *)&zsamp;
		cz[GCOMP]= ct[0];

	}
	else {
		/* got warning on this for 64 bits.... */
		/* but it's working code! in this case rz is not a pointer but zvalue (ton) */
		zsamp= GET_INT_FROM_POINTER(rz);
	}

	/* tricky stuff here; we use ints which can overflow easily with bias values */
	
	if(zsamp > zs) return 1.0; 		/* absolute no shadow */
	else if(zs < - 0x7FFFFE00 + bias) return 1.0;	/* extreme close to clipstart */
	else if(zsamp < zs-bias) return 0.0 ;	/* absolute in shadow */
	else {					/* soft area */
		
		temp=  ( (float)(zs- zsamp) )/(float)bias;
		return 1.0f - temp*temp;
			
	}
}

static void shadowbuf_project_co(float *x, float *y, float *z, ShadBuf *shb, const float co[3])
{
	float hco[4], size= 0.5f*(float)shb->size;

	copy_v3_v3(hco, co);
	hco[3]= 1.0f;

	mul_m4_v4(shb->persmat, hco);

	*x= size*(1.0f+hco[0]/hco[3]);
	*y= size*(1.0f+hco[1]/hco[3]);
	if(z) *z= (hco[2]/hco[3]);
}

/* the externally called shadow testing (reading) function */
/* return 1.0: no shadow at all */
float testshadowbuf(Render *re, ShadBuf *shb, const float co[3], const float dxco[3], const float dyco[3], float inp, float mat_bias)
{
	ShadSampleBuf *shsample;
	float fac, dco[3], dx[3], dy[3], shadfac=0.0f;
	float xs1, ys1, zs1, *jit, *weight, xres, yres, biasf;
	int xs, ys, zs, bias, *rz;
	short a, num;
	
	/* crash preventer */
	if(shb->buffers.first==NULL)
		return 1.0f;
	
	/* when facing away, assume fully in shadow */
	if(inp <= 0.0f)
		return 0.0f;

	/* project coordinate to pixel space */
	shadowbuf_project_co(&xs1, &ys1, &zs1, shb, co);

	/* clip z coordinate, z is projected so that (-1.0, 1.0) matches
	   (clipstart, clipend), so we can do this simple test */
	if(zs1>=1.0f)
		return 0.0f;
	else if(zs1<= -1.0f)
		return 1.0f;

	zs= ((float)0x7FFFFFFF)*zs1;

	/* take num*num samples, increase area with fac */
	num= get_render_shadow_samples(&re->r, shb->samp);
	num= num*num;
	fac= shb->soft;
	
	/* compute z bias */
	if(mat_bias!=0.0f) biasf= shb->bias*mat_bias;
	else biasf= shb->bias;
	/* with inp==1.0, bias is half the size. correction value was 1.1, giving errors 
	   on cube edges, with one side being almost frontal lighted (ton)  */
	bias= (1.5f-inp*inp)*biasf;
	
	/* in case of no filtering we can do things simpler */
	if(num==1) {
		for(shsample= shb->buffers.first; shsample; shsample= shsample->next)
			shadfac += readshadowbuf(shb, shsample, bias, (int)xs1, (int)ys1, zs);
		
		return shadfac/(float)shb->totbuf;
	}

	/* calculate filter size */
	add_v3_v3v3(dco, co, dxco);
	shadowbuf_project_co(&dx[0], &dx[1], NULL, shb, dco);
	dx[0]= xs1 - dx[0];
	dx[1]= ys1 - dx[1];

	add_v3_v3v3(dco, co, dyco);
	shadowbuf_project_co(&dy[0], &dy[1], NULL, shb, dco);
	dy[0]= xs1 - dy[0];
	dy[1]= ys1 - dy[1];
	
	xres= fac*(fabs(dx[0]) + fabs(dy[0]));
	yres= fac*(fabs(dx[1]) + fabs(dy[1]));
	if(xres<1.0f) xres= 1.0f;
	if(yres<1.0f) yres= 1.0f;
	
	/* make xs1/xs1 corner of sample area */
	xs1 -= xres*0.5f;
	ys1 -= yres*0.5f;

	/* in case we have a constant value in a tile, we can do quicker lookup */
	if(xres<16.0f && yres<16.0f) {
		shsample= shb->buffers.first;
		if(firstreadshadbuf(shb, shsample, &rz, (int)xs1, (int)ys1, 0)) {
			if(firstreadshadbuf(shb, shsample, &rz, (int)(xs1+xres), (int)ys1, 1)) {
				if(firstreadshadbuf(shb, shsample, &rz, (int)xs1, (int)(ys1+yres), 1)) {
					if(firstreadshadbuf(shb, shsample, &rz, (int)(xs1+xres), (int)(ys1+yres), 1)) {
						return readshadowbuf(shb, shsample, bias,(int)xs1, (int)ys1, zs);
					}
				}
			}
		}
	}
	
	/* full jittered shadow buffer lookup */
	for(shsample= shb->buffers.first; shsample; shsample= shsample->next) {
		jit= shb->jit;
		weight= shb->weight;
		
		for(a=num; a>0; a--, jit+=2, weight++) {
			/* instead of jit i tried random: ugly! */
			/* note: the plus 0.5 gives best sampling results, jit goes from -0.5 to 0.5 */
			/* xs1 and ys1 are already corrected to be corner of sample area */
			xs= xs1 + xres*(jit[0] + 0.5f);
			ys= ys1 + yres*(jit[1] + 0.5f);
			
			shadfac+= *weight * readshadowbuf(shb, shsample, bias, xs, ys, zs);
		}
	}

	/* Renormalizes for the sample number: */
	return shadfac/(float)shb->totbuf;
}

/* different function... sampling behind clipend can be LIGHT, bias is negative! */
/* return: light */
static float readshadowbuf_halo(ShadBuf *shb, ShadSampleBuf *shsample, int xs, int ys, int zs)
{
	float temp;
	int *rz, ofs;
	int bias, zbias, zsamp;
	char *ct, *cz;

	/* negative! The other side is more important */
	bias= -shb->bias;
	
	/* simpleclip */
	if(xs<0 || ys<0) return 0.0;
	if(xs>=shb->size || ys>=shb->size) return 0.0;

	/* calc z */
	ofs= (ys>>4)*(shb->size>>4) + (xs>>4);
	ct= shsample->cbuf+ofs;
	rz= *( (int **)(shsample->zbuf+ofs) );

	if(*ct==3) {
		ct= ((char *)rz)+3*16*(ys & 15)+3*(xs & 15);
		cz= (char *)&zsamp;
		zsamp= 0;
		cz[ACOMP]= ct[0];
		cz[BCOMP]= ct[1];
		cz[GCOMP]= ct[2];
	}
	else if(*ct==2) {
		ct= ((char *)rz);
		ct+= 4+2*16*(ys & 15)+2*(xs & 15);
		zsamp= *rz;
	
		cz= (char *)&zsamp;
		cz[BCOMP]= ct[0];
		cz[GCOMP]= ct[1];
	}
	else if(*ct==1) {
		ct= ((char *)rz);
		ct+= 4+16*(ys & 15)+(xs & 15);
		zsamp= *rz;

		cz= (char *)&zsamp;
		cz[GCOMP]= ct[0];

	}
	else {
		/* same as before */
		/* still working code! (ton) */
		zsamp= GET_INT_FROM_POINTER(rz);
	}

	/* NO schadow when sampled at 'eternal' distance */

	if(zsamp >= 0x7FFFFE00) return 1.0; 

	if(zsamp > zs) return 1.0; 		/* absolute no shadww */
	else {
		/* bias is negative, so the (zs-bias) can be beyond 0x7fffffff */
		zbias= 0x7fffffff - zs;
		if(zbias > -bias) {
			if( zsamp < zs-bias) return 0.0 ;	/* absolute in shadow */
		}
		else return 0.0 ;	/* absolute shadow */
	}

	/* soft area */
	
	temp=  ( (float)(zs- zsamp) )/(float)bias;
	return 1.0f - temp*temp;
}


float shadow_halo(LampRen *lar, const float p1[3], const float p2[3])
{
	/* p1 p2 already are rotated in spot-space */
	ShadBuf *shb= lar->shb;
	ShadSampleBuf *shsample;
	float co[4], siz;
	float labda, labdao, labdax, labday, ldx, ldy;
	float zf, xf1, yf1, zf1, xf2, yf2, zf2;
	float count, lightcount;
	int x, y, z, xs1, ys1;
	int dx = 0, dy = 0;
	
	siz= 0.5f*(float)shb->size;
	
	co[0]= p1[0];
	co[1]= p1[1];
	co[2]= p1[2]/lar->sh_zfac;
	co[3]= 1.0;
	mul_m4_v4(shb->winmat, co);	/* rational hom co */
	xf1= siz*(1.0f+co[0]/co[3]);
	yf1= siz*(1.0f+co[1]/co[3]);
	zf1= (co[2]/co[3]);


	co[0]= p2[0];
	co[1]= p2[1];
	co[2]= p2[2]/lar->sh_zfac;
	co[3]= 1.0;
	mul_m4_v4(shb->winmat, co);	/* rational hom co */
	xf2= siz*(1.0f+co[0]/co[3]);
	yf2= siz*(1.0f+co[1]/co[3]);
	zf2= (co[2]/co[3]);

	/* the 2dda (a pixel line formula) */

	xs1= (int)xf1;
	ys1= (int)yf1;

	if(xf1 != xf2) {
		if(xf2-xf1 > 0.0f) {
			labdax= (xf1-xs1-1.0f)/(xf1-xf2);
			ldx= -shb->shadhalostep/(xf1-xf2);
			dx= shb->shadhalostep;
		}
		else {
			labdax= (xf1-xs1)/(xf1-xf2);
			ldx= shb->shadhalostep/(xf1-xf2);
			dx= -shb->shadhalostep;
		}
	}
	else {
		labdax= 1.0;
		ldx= 0.0;
	}

	if(yf1 != yf2) {
		if(yf2-yf1 > 0.0f) {
			labday= (yf1-ys1-1.0f)/(yf1-yf2);
			ldy= -shb->shadhalostep/(yf1-yf2);
			dy= shb->shadhalostep;
		}
		else {
			labday= (yf1-ys1)/(yf1-yf2);
			ldy= shb->shadhalostep/(yf1-yf2);
			dy= -shb->shadhalostep;
		}
	}
	else {
		labday= 1.0;
		ldy= 0.0;
	}
	
	x= xs1;
	y= ys1;
	labda= count= lightcount= 0.0;

/* printf("start %x %x	\n", (int)(0x7FFFFFFF*zf1), (int)(0x7FFFFFFF*zf2)); */

	while(1) {
		labdao= labda;
		
		if(labdax==labday) {
			labdax+= ldx;
			x+= dx;
			labday+= ldy;
			y+= dy;
		}
		else {
			if(labdax<labday) {
				labdax+= ldx;
				x+= dx;
			} else {
				labday+= ldy;
				y+= dy;
			}
		}
		
		labda= MIN2(labdax, labday);
		if(labda==labdao || labda>=1.0f) break;
		
		zf= zf1 + labda*(zf2-zf1);
		count+= (float)shb->totbuf;

		if(zf<= -1.0f) lightcount += 1.0f;	/* close to the spot */
		else {
		
			/* make sure, behind the clipend we extend halolines. */
			if(zf>=1.0f) z= 0x7FFFF000;
			else z= (int)(0x7FFFF000*zf);
			
			for(shsample= shb->buffers.first; shsample; shsample= shsample->next)
				lightcount+= readshadowbuf_halo(shb, shsample, x, y, z);
			
		}
	}
	
	if(count!=0.0f) return (lightcount/count);
	return 0.0f;
	
}


/* ********************* Irregular Shadow Buffer (ISB) ************* */
/* ********** storage of all view samples in a raster of lists ***** */

/* based on several articles describing this method, like:
The Irregular Z-Buffer and its Application to Shadow Mapping
Gregory S. Johnson - William R. Mark - Christopher A. Burns 
and
Alias-Free Shadow Maps
Timo Aila and Samuli Laine
*/

/* bsp structure (actually kd tree) */

#define BSPMAX_SAMPLE	128
#define BSPMAX_DEPTH	32

/* aligned with struct rctf */
typedef struct Boxf {
	float xmin, xmax;
	float ymin, ymax;
	float zmin, zmax;
} Boxf;

typedef struct ISBBranch {
	struct ISBBranch *left, *right;
	float divider[2];
	Boxf box;
	short totsamp, index, full, unused;
	ISBSample **samples;
} ISBBranch;

typedef struct BSPFace {
	Boxf box;
	float *v1, *v2, *v3, *v4;
	int obi;		/* object for face lookup */
	int facenr;		/* index to retrieve VlakRen */
	int type;		/* only for strand now */
	short shad_alpha, is_full;
	
	/* strand caching data, optimize for point_behind_strand() */
	float radline, radline_end, len;
	float vec1[3], vec2[3], rc[3];
} BSPFace;

/* boxes are in lamp projection */
static void init_box(Boxf *box)
{
	box->xmin= 1000000.0f;
	box->xmax= 0;
	box->ymin= 1000000.0f;
	box->ymax= 0;
	box->zmin= 0x7FFFFFFF;
	box->zmax= - 0x7FFFFFFF;
}

/* use v1 to calculate boundbox */
static void bound_boxf(Boxf *box, const float v1[3])
{
	if(v1[0] < box->xmin) box->xmin= v1[0];
	if(v1[0] > box->xmax) box->xmax= v1[0];
	if(v1[1] < box->ymin) box->ymin= v1[1];
	if(v1[1] > box->ymax) box->ymax= v1[1];
	if(v1[2] < box->zmin) box->zmin= v1[2];
	if(v1[2] > box->zmax) box->zmax= v1[2];
}

/* use v1 to calculate boundbox */
static void bound_rectf(rctf *box, const float v1[2])
{
	if(v1[0] < box->xmin) box->xmin= v1[0];
	if(v1[0] > box->xmax) box->xmax= v1[0];
	if(v1[1] < box->ymin) box->ymin= v1[1];
	if(v1[1] > box->ymax) box->ymax= v1[1];
}


/* halfway splitting, for initializing a more regular tree */
static void isb_bsp_split_init(ISBBranch *root, MemArena *mem, int level)
{
	
	/* if level > 0 we create new branches and go deeper*/
	if(level > 0) {
		ISBBranch *left, *right;
		int i;
		
		/* splitpoint */
		root->divider[0]= 0.5f*(root->box.xmin+root->box.xmax);
		root->divider[1]= 0.5f*(root->box.ymin+root->box.ymax);
		
		/* find best splitpoint */
		if(root->box.xmax-root->box.xmin > root->box.ymax-root->box.ymin)
			i= root->index= 0;
		else
			i= root->index= 1;
		
		left= root->left= BLI_memarena_alloc(mem, sizeof(ISBBranch));
		right= root->right= BLI_memarena_alloc(mem, sizeof(ISBBranch));
		
		/* box info */
		left->box= root->box;
		right->box= root->box;
		if(i==0) {
			left->box.xmax= root->divider[0];
			right->box.xmin= root->divider[0];
		}
		else {
			left->box.ymax= root->divider[1];
			right->box.ymin= root->divider[1];
		}
		isb_bsp_split_init(left, mem, level-1);
		isb_bsp_split_init(right, mem, level-1);
	}
	else {
		/* we add sample array */
		root->samples= BLI_memarena_alloc(mem, BSPMAX_SAMPLE*sizeof(void *));
	}
}

/* note; if all samples on same location we just spread them over 2 new branches */
static void isb_bsp_split(ISBBranch *root, MemArena *mem)
{
	ISBBranch *left, *right;
	ISBSample *samples[BSPMAX_SAMPLE];
	int a, i;

	/* splitpoint */
	root->divider[0]= root->divider[1]= 0.0f;
	for(a=BSPMAX_SAMPLE-1; a>=0; a--) {
		root->divider[0]+= root->samples[a]->zco[0];
		root->divider[1]+= root->samples[a]->zco[1];
	}
	root->divider[0]/= BSPMAX_SAMPLE;
	root->divider[1]/= BSPMAX_SAMPLE;
	
	/* find best splitpoint */
	if(root->box.xmax-root->box.xmin > root->box.ymax-root->box.ymin)
		i= root->index= 0;
	else
		i= root->index= 1;
	
	/* new branches */
	left= root->left= BLI_memarena_alloc(mem, sizeof(ISBBranch));
	right= root->right= BLI_memarena_alloc(mem, sizeof(ISBBranch));

	/* new sample array */
	left->samples= BLI_memarena_alloc(mem, BSPMAX_SAMPLE*sizeof(void *));
	right->samples= samples; // tmp
	
	/* split samples */
	for(a=BSPMAX_SAMPLE-1; a>=0; a--) {
		int comp= 0;
		/* this prevents adding samples all to 1 branch when divider is equal to samples */
		if(root->samples[a]->zco[i] == root->divider[i])
			comp= a & 1;
		else if(root->samples[a]->zco[i] < root->divider[i])
			comp= 1;
		
		if(comp==1) {
			left->samples[left->totsamp]= root->samples[a];
			left->totsamp++;
		}
		else {
			right->samples[right->totsamp]= root->samples[a];
			right->totsamp++;
		}
	}
	
	/* copy samples from tmp */
	memcpy(root->samples, samples, right->totsamp*(sizeof(void *)));
	right->samples= root->samples;
	root->samples= NULL;
	
	/* box info */
	left->box= root->box;
	right->box= root->box;
	if(i==0) {
		left->box.xmax= root->divider[0];
		right->box.xmin= root->divider[0];
	}
	else {
		left->box.ymax= root->divider[1];
		right->box.ymin= root->divider[1];
	}
}

/* inserts sample in main tree, also splits on threshold */
/* returns 1 if error */
static int isb_bsp_insert(ISBBranch *root, MemArena *memarena, ISBSample *sample)
{
	ISBBranch *bspn= root;
	float *zco= sample->zco;
	int i= 0;
	
	/* debug counter, also used to check if something was filled in ever */
	root->totsamp++;
	
	/* going over branches until last one found */
	while(bspn->left) {
		if(zco[bspn->index] <= bspn->divider[bspn->index])
			bspn= bspn->left;
		else
			bspn= bspn->right;
		i++;
	}
	/* bspn now is the last branch */
	
	if(bspn->totsamp==BSPMAX_SAMPLE) {
		printf("error in bsp branch\n");	/* only for debug, cannot happen */
		return 1;
	}
	
	/* insert */
	bspn->samples[bspn->totsamp]= sample;
	bspn->totsamp++;

	/* split if allowed and needed */
	if(bspn->totsamp==BSPMAX_SAMPLE) {
		if(i==BSPMAX_DEPTH) {
			bspn->totsamp--;	/* stop filling in... will give errors */
			return 1;
		}
		isb_bsp_split(bspn, memarena);
	}
	return 0;
}

/* initialize vars in face, for optimal point-in-face test */
static void bspface_init_strand(BSPFace *face) 
{
	
	face->radline= 0.5f* len_v2v2(face->v1, face->v2);
	
	mid_v3_v3v3(face->vec1, face->v1, face->v2);
	if(face->v4)
		mid_v3_v3v3(face->vec2, face->v3, face->v4);
	else
		copy_v3_v3(face->vec2, face->v3);
	
	face->rc[0]= face->vec2[0]-face->vec1[0];
	face->rc[1]= face->vec2[1]-face->vec1[1];
	face->rc[2]= face->vec2[2]-face->vec1[2];
	
	face->len= face->rc[0]*face->rc[0]+ face->rc[1]*face->rc[1];
	
	if(face->len!=0.0f) {
		face->radline_end= face->radline/sqrt(face->len);
		face->len= 1.0f/face->len;
	}
}

/* brought back to a simple 2d case */
static int point_behind_strand(const float p[3], BSPFace *face)
{
	/* v1 - v2 is radius, v1 - v3 length */
	float dist, rc[2], pt[2];
	
	/* using code from dist_to_line_segment_v2(), distance vec to line-piece */

	if(face->len==0.0f) {
		rc[0]= p[0]-face->vec1[0];
		rc[1]= p[1]-face->vec1[1];
		dist= (float)(sqrt(rc[0]*rc[0]+ rc[1]*rc[1]));
		
		if(dist < face->radline)
			return 1;
	}
	else {
		float labda= ( face->rc[0]*(p[0]-face->vec1[0]) + face->rc[1]*(p[1]-face->vec1[1]) )*face->len;
		
		if(labda > -face->radline_end && labda < 1.0f+face->radline_end) { 
			/* hesse for dist: */
			//dist= (float)(fabs( (p[0]-vec2[0])*rc[1] + (p[1]-vec2[1])*rc[0])/len);
			
			pt[0]= labda*face->rc[0]+face->vec1[0];
			pt[1]= labda*face->rc[1]+face->vec1[1];
			
			rc[0]= pt[0]-p[0];
			rc[1]= pt[1]-p[1];
			dist= (float)sqrt(rc[0]*rc[0]+ rc[1]*rc[1]);
			
			if(dist < face->radline) {
				float zval= face->vec1[2] + labda*face->rc[2];
				if(p[2] > zval)
					return 1;
			}
		}
	}
	return 0;
}


/* return 1 if inside. code derived from src/parametrizer.c */
static int point_behind_tria2d(const float p[3], const float v1[3], const float v2[3], const float v3[3])
{
	float a[2], c[2], h[2], div;
	float u, v;
	
	a[0] = v2[0] - v1[0];
	a[1] = v2[1] - v1[1];
	c[0] = v3[0] - v1[0];
	c[1] = v3[1] - v1[1];
	
	div = a[0]*c[1] - a[1]*c[0];
	if(div==0.0f)
		return 0;
	
	h[0] = p[0] - v1[0];
	h[1] = p[1] - v1[1];
	
	div = 1.0f/div;
	
	u = (h[0]*c[1] - h[1]*c[0])*div;
	if(u >= 0.0f) {
		v = (a[0]*h[1] - a[1]*h[0])*div;
		if(v >= 0.0f) {
			if( u + v <= 1.0f) {
				/* inside, now check if point p is behind */
				float z=  (1.0f-u-v)*v1[2] + u*v2[2] + v*v3[2];
				if(z <= p[2])
					return 1;
			}
		}
	}
	
	return 0;
}

#if 0
/* tested these calls, but it gives inaccuracy, 'side' cannot be found reliably using v3 */

/* check if line v1-v2 has all rect points on other side of point v3 */
static int rect_outside_line(rctf *rect, const float v1[3], const float v2[3], const float v3[3])
{
	float a, b, c;
	int side;
	
	/* line formula for v1-v2 */
	a= v2[1]-v1[1];
	b= v1[0]-v2[0];
	c= -a*v1[0] - b*v1[1];
	side= a*v3[0] + b*v3[1] + c < 0.0f;
	
	/* the four quad points */
	if( side==(rect->xmin*a + rect->ymin*b + c >= 0.0f) )
		if( side==(rect->xmax*a + rect->ymin*b + c >= 0.0f) )
			if( side==(rect->xmax*a + rect->ymax*b + c >= 0.0f) )
				if( side==(rect->xmin*a + rect->ymax*b + c >= 0.0f) )
					return 1;
	return 0;
}

/* check if one of the triangle edges separates all rect points on 1 side */
static int rect_isect_tria(rctf *rect, const float v1[3], const float v2[3], const float v3[3])
{
	if(rect_outside_line(rect, v1, v2, v3))
		return 0;
	if(rect_outside_line(rect, v2, v3, v1))
		return 0;
	if(rect_outside_line(rect, v3, v1, v2))
		return 0;
	return 1;
}
#endif

/* if face overlaps a branch, it executes func. recursive */
static void isb_bsp_face_inside(ISBBranch *bspn, BSPFace *face)
{
	
	/* are we descending? */
	if(bspn->left) {
		/* hrmf, the box struct cannot be addressed with index */
		if(bspn->index==0) {
			if(face->box.xmin <= bspn->divider[0])
				isb_bsp_face_inside(bspn->left, face);
			if(face->box.xmax > bspn->divider[0])
				isb_bsp_face_inside(bspn->right, face);
		}
		else {
			if(face->box.ymin <= bspn->divider[1])
				isb_bsp_face_inside(bspn->left, face);
			if(face->box.ymax > bspn->divider[1])
				isb_bsp_face_inside(bspn->right, face);
		}
	}
	else {
		/* else: end branch reached */
		int a;
		
		if(bspn->totsamp==0) return;
		
		/* check for nodes entirely in shadow, can be skipped */
		if(bspn->totsamp==bspn->full)
			return;
		
		/* if bsp node is entirely in front of face, give up */
		if(bspn->box.zmax < face->box.zmin)
			return;
		
		/* if face boundbox is outside of branch rect, give up */
		if(0==BLI_isect_rctf((rctf *)&face->box, (rctf *)&bspn->box, NULL))
			return;
		
		/* test all points inside branch */
		for(a=bspn->totsamp-1; a>=0; a--) {
			ISBSample *samp= bspn->samples[a];
			
			if((samp->facenr!=face->facenr || samp->obi!=face->obi) && samp->shadfac) {
				if(face->box.zmin < samp->zco[2]) {
					if(BLI_in_rctf((rctf *)&face->box, samp->zco[0], samp->zco[1])) {
						int inshadow= 0;
						
						if(face->type) {
							if(point_behind_strand(samp->zco, face)) 
								inshadow= 1;
						}
						else if( point_behind_tria2d(samp->zco, face->v1, face->v2, face->v3))
							inshadow= 1;
						else if(face->v4 && point_behind_tria2d(samp->zco, face->v1, face->v3, face->v4))
							inshadow= 1;

						if(inshadow) {
							*(samp->shadfac) += face->shad_alpha;
							/* optimize; is_full means shad_alpha==4096 */
							if(*(samp->shadfac) >= 4096 || face->is_full) {
								bspn->full++;
								samp->shadfac= NULL;
							}
						}
					}
				}
			}
		}
	}
}

/* based on available samples, recalculate the bounding box for bsp nodes, recursive */
static void isb_bsp_recalc_box(ISBBranch *root)
{
	if(root->left) {
		isb_bsp_recalc_box(root->left);
		isb_bsp_recalc_box(root->right);
	}
	else if(root->totsamp) {
		int a;
		
		init_box(&root->box);
		for(a=root->totsamp-1; a>=0; a--)
			bound_boxf(&root->box, root->samples[a]->zco);
	}	
}

/* callback function for zbuf clip */
static void isb_bsp_test_strand(ZSpan *zspan, int obi, int zvlnr, float *v1, float *v2, float *v3, float *v4)
{
	BSPFace face;
	
	face.v1= v1;
	face.v2= v2;
	face.v3= v3;
	face.v4= v4;
	face.obi= obi;
	face.facenr= zvlnr & ~RE_QUAD_OFFS;
	face.type= R_STRAND;
	if(R.osa)
		face.shad_alpha= (short)ceil(4096.0f*zspan->shad_alpha/(float)R.osa);
	else
		face.shad_alpha= (short)ceil(4096.0f*zspan->shad_alpha);
	
	face.is_full= (zspan->shad_alpha==1.0f);
	
	/* setup boundbox */
	init_box(&face.box);
	bound_boxf(&face.box, v1);
	bound_boxf(&face.box, v2);
	bound_boxf(&face.box, v3);
	if(v4)
		bound_boxf(&face.box, v4);
	
	/* optimize values */
	bspface_init_strand(&face);
	
	isb_bsp_face_inside((ISBBranch *)zspan->rectz, &face);
	
}

/* callback function for zbuf clip */
static void isb_bsp_test_face(ZSpan *zspan, int obi, int zvlnr, float *v1, float *v2, float *v3, float *v4) 
{
	BSPFace face;
	
	face.v1= v1;
	face.v2= v2;
	face.v3= v3;
	face.v4= v4;
	face.obi= obi;
	face.facenr= zvlnr & ~RE_QUAD_OFFS;
	face.type= 0;
	if(R.osa)
		face.shad_alpha= (short)ceil(4096.0f*zspan->shad_alpha/(float)R.osa);
	else
		face.shad_alpha= (short)ceil(4096.0f*zspan->shad_alpha);
	
	face.is_full= (zspan->shad_alpha==1.0f);
	
	/* setup boundbox */
	init_box(&face.box);
	bound_boxf(&face.box, v1);
	bound_boxf(&face.box, v2);
	bound_boxf(&face.box, v3);
	if(v4)
		bound_boxf(&face.box, v4);

	isb_bsp_face_inside((ISBBranch *)zspan->rectz, &face);
}

static int testclip_minmax(const float ho[4], const float minmax[4])
{
	float wco= ho[3];
	int flag= 0;
	
	if( ho[0] > minmax[1]*wco) flag = 1;
	else if( ho[0]< minmax[0]*wco) flag = 2;
	
	if( ho[1] > minmax[3]*wco) flag |= 4;
	else if( ho[1]< minmax[2]*wco) flag |= 8;
	
	return flag;
}

/* main loop going over all faces and check in bsp overlaps, fill in shadfac values */
static void isb_bsp_fillfaces(Render *re, LampRen *lar, ISBBranch *root)
{
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	ShadBuf *shb= lar->shb;
	ZSpan zspan, zspanstrand;
	VlakRen *vlr= NULL;
	Material *ma= NULL;
	float minmaxf[4], winmat[4][4];
	int size= shb->size;
	int i, a, ok=1, lay= -1;
	
	/* further optimize, also sets minz maxz */
	isb_bsp_recalc_box(root);
	
	/* extra clipping for minmax */
	minmaxf[0]= (2.0f*root->box.xmin - size-2.0f)/size;
	minmaxf[1]= (2.0f*root->box.xmax - size+2.0f)/size;
	minmaxf[2]= (2.0f*root->box.ymin - size-2.0f)/size;
	minmaxf[3]= (2.0f*root->box.ymax - size+2.0f)/size;
	
	if(lar->mode & (LA_LAYER|LA_LAYER_SHADOW)) lay= lar->lay;
	
	/* (ab)use zspan, since we use zbuffer clipping code */
	zbuf_alloc_span(&zspan, size, size, re->clipcrop);
	
	zspan.zmulx=  ((float)size)/2.0f;
	zspan.zmuly=  ((float)size)/2.0f;
	zspan.zofsx= -0.5f;
	zspan.zofsy= -0.5f;
	
	/* pass on bsp root to zspan */
	zspan.rectz= (int *)root;
	
	/* filling methods */
	zspanstrand= zspan;
	//	zspan.zbuflinefunc= zbufline_onlyZ;
	zspan.zbuffunc= isb_bsp_test_face;
	zspanstrand.zbuffunc= isb_bsp_test_strand;
	
	for(i=0, obi=re->instancetable.first; obi; i++, obi=obi->next) {
		obr= obi->obr;

		if(obi->flag & R_TRANSFORMED)
			mult_m4_m4m4(winmat, shb->persmat, obi->mat);
		else
			copy_m4_m4(winmat, shb->persmat);

		for(a=0; a<obr->totvlak; a++) {
			
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak;
			else vlr++;
			
			/* note, these conditions are copied in shadowbuf_autoclip() */
			if(vlr->mat!= ma) {
				ma= vlr->mat;
				ok= 1;
				if((ma->mode & MA_SHADBUF)==0) ok= 0;
				if(ma->material_type == MA_TYPE_WIRE) ok= 0;
				zspanstrand.shad_alpha= zspan.shad_alpha= ma->shad_alpha;
			}
			
			if(ok && (obi->lay & lay)) {
				float hoco[4][4];
				int c1, c2, c3, c4=0;
				int d1, d2, d3, d4=0;
				int partclip;
				
				/* create hocos per face, it is while render */
				projectvert(vlr->v1->co, winmat, hoco[0]); d1= testclip_minmax(hoco[0], minmaxf);
				projectvert(vlr->v2->co, winmat, hoco[1]); d2= testclip_minmax(hoco[1], minmaxf);
				projectvert(vlr->v3->co, winmat, hoco[2]); d3= testclip_minmax(hoco[2], minmaxf);
				if(vlr->v4) {
					projectvert(vlr->v4->co, winmat, hoco[3]); d4= testclip_minmax(hoco[3], minmaxf);
				}

				/* minmax clipping */
				if(vlr->v4) partclip= d1 & d2 & d3 & d4;
				else partclip= d1 & d2 & d3;
				
				if(partclip==0) {
					
					/* window clipping */
					c1= testclip(hoco[0]); 
					c2= testclip(hoco[1]); 
					c3= testclip(hoco[2]); 
					if(vlr->v4)
						c4= testclip(hoco[3]); 
					
					/* ***** NO WIRE YET */			
					if(ma->material_type == MA_TYPE_WIRE) {
						if(vlr->v4)
							zbufclipwire(&zspan, i, a+1, vlr->ec, hoco[0], hoco[1], hoco[2], hoco[3], c1, c2, c3, c4);
						else
							zbufclipwire(&zspan, i, a+1, vlr->ec, hoco[0], hoco[1], hoco[2], 0, c1, c2, c3, 0);
					}
					else if(vlr->v4) {
						if(vlr->flag & R_STRAND)
							zbufclip4(&zspanstrand, i, a+1, hoco[0], hoco[1], hoco[2], hoco[3], c1, c2, c3, c4);
						else
							zbufclip4(&zspan, i, a+1, hoco[0], hoco[1], hoco[2], hoco[3], c1, c2, c3, c4);
					}
					else
						zbufclip(&zspan, i, a+1, hoco[0], hoco[1], hoco[2], c1, c2, c3);
					
				}
			}
		}
	}
	
	zbuf_free_span(&zspan);
}

/* returns 1 when the viewpixel is visible in lampbuffer */
static int viewpixel_to_lampbuf(ShadBuf *shb, ObjectInstanceRen *obi, VlakRen *vlr, float x, float y, float co_r[3])
{
	float hoco[4], v1[3], nor[3];
	float dface, fac, siz;
	
	RE_vlakren_get_normal(&R, obi, vlr, nor);
	copy_v3_v3(v1, vlr->v1->co);
	if(obi->flag & R_TRANSFORMED)
		mul_m4_v3(obi->mat, v1);

	/* from shadepixel() */
	dface = dot_v3v3(v1, nor);
	hoco[3]= 1.0f;
	
	/* ortho viewplane cannot intersect using view vector originating in (0,0,0) */
	if(R.r.mode & R_ORTHO) {
		/* x and y 3d coordinate can be derived from pixel coord and winmat */
		float fx= 2.0f/(R.winx*R.winmat[0][0]);
		float fy= 2.0f/(R.winy*R.winmat[1][1]);
		
		hoco[0]= (x - 0.5f*R.winx)*fx - R.winmat[3][0]/R.winmat[0][0];
		hoco[1]= (y - 0.5f*R.winy)*fy - R.winmat[3][1]/R.winmat[1][1];
		
		/* using a*x + b*y + c*z = d equation, (a b c) is normal */
		if(nor[2]!=0.0f)
			hoco[2]= (dface - nor[0]*hoco[0] - nor[1]*hoco[1])/nor[2];
		else
			hoco[2]= 0.0f;
	}
	else {
		float div, view[3];
		
		calc_view_vector(view, x, y);
		
		div = dot_v3v3(nor, view);
		if (div==0.0f) 
			return 0;
		
		fac= dface/div;
		
		hoco[0]= fac*view[0];
		hoco[1]= fac*view[1];
		hoco[2]= fac*view[2];
	}
	
	/* move 3d vector to lampbuf */
	mul_m4_v4(shb->persmat, hoco);	/* rational hom co */
	
	/* clip We can test for -1.0/1.0 because of the properties of the
	 * coordinate transformations. */
	fac= fabs(hoco[3]);
	if(hoco[0]<-fac || hoco[0]>fac)
		return 0;
	if(hoco[1]<-fac || hoco[1]>fac)
		return 0;
	if(hoco[2]<-fac || hoco[2]>fac)
		return 0;
	
	siz= 0.5f*(float)shb->size;
	co_r[0]= siz*(1.0f+hoco[0]/hoco[3]) -0.5f;
	co_r[1]= siz*(1.0f+hoco[1]/hoco[3]) -0.5f;
	co_r[2]= ((float)0x7FFFFFFF)*(hoco[2]/hoco[3]);
	
	/* XXXX bias, much less than normal shadbuf, or do we need a constant? */
	co_r[2] -= 0.05f*shb->bias;
	
	return 1;
}

/* storage of shadow results, solid osa and transp case */
static void isb_add_shadfac(ISBShadfacA **isbsapp, MemArena *mem, int obi, int facenr, short shadfac, short samples)
{
	ISBShadfacA *new;
	float shadfacf;
	
	/* in osa case, the samples were filled in with factor 1.0/R.osa. if fewer samples we have to correct */
	if(R.osa)
		shadfacf= ((float)shadfac*R.osa)/(4096.0f*samples);
	else
		shadfacf= ((float)shadfac)/(4096.0f);
	
	new= BLI_memarena_alloc(mem, sizeof(ISBShadfacA));
	new->obi= obi;
	new->facenr= facenr & ~RE_QUAD_OFFS;
	new->shadfac= shadfacf;
	if(*isbsapp)
		new->next= (*isbsapp);
	else
		new->next= NULL;
	
	*isbsapp= new;
}

/* adding samples, solid case */
static int isb_add_samples(RenderPart *pa, ISBBranch *root, MemArena *memarena, ISBSample **samplebuf)
{
	int xi, yi, *xcos, *ycos;
	int sample, bsp_err= 0;
	
	/* bsp split doesn't like to handle regular sequences */
	xcos= MEM_mallocN( pa->rectx*sizeof(int), "xcos");
	ycos= MEM_mallocN( pa->recty*sizeof(int), "ycos");
	for(xi=0; xi<pa->rectx; xi++)
		xcos[xi]= xi;
	for(yi=0; yi<pa->recty; yi++)
		ycos[yi]= yi;
	BLI_array_randomize(xcos, sizeof(int), pa->rectx, 12345);
	BLI_array_randomize(ycos, sizeof(int), pa->recty, 54321);
	
	for(sample=0; sample<(R.osa?R.osa:1); sample++) {
		ISBSample *samp= samplebuf[sample], *samp1;
		
		for(yi=0; yi<pa->recty; yi++) {
			int y= ycos[yi];
			for(xi=0; xi<pa->rectx; xi++) {
				int x= xcos[xi];
				samp1= samp + y*pa->rectx + x;
				if(samp1->facenr)
					bsp_err |= isb_bsp_insert(root, memarena, samp1);
			}
			if(bsp_err) break;
		}
	}	
	
	MEM_freeN(xcos);
	MEM_freeN(ycos);

	return bsp_err;
}

/* solid version */
/* lar->shb, pa->rectz and pa->rectp should exist */
static void isb_make_buffer(RenderPart *pa, LampRen *lar)
{
	ShadBuf *shb= lar->shb;
	ISBData *isbdata;
	ISBSample *samp, *samplebuf[16];	/* should be RE_MAX_OSA */
	ISBBranch root;
	MemArena *memarena;
	intptr_t *rd;
	int *recto, *rectp, x, y, sindex, sample, bsp_err=0;
	
	/* storage for shadow, per thread */
	isbdata= shb->isb_result[pa->thread];
	
	/* to map the shi->xs and ys coordinate */
	isbdata->minx= pa->disprect.xmin;
	isbdata->miny= pa->disprect.ymin;
	isbdata->rectx= pa->rectx;
	isbdata->recty= pa->recty;
	
	/* branches are added using memarena (32k branches) */
	memarena = BLI_memarena_new(0x8000 * sizeof(ISBBranch), "isb arena");
	BLI_memarena_use_calloc(memarena);
	
	/* samplebuf is in camera view space (pixels) */
	for(sample=0; sample<(R.osa?R.osa:1); sample++)
		samplebuf[sample]= MEM_callocN(sizeof(ISBSample)*pa->rectx*pa->recty, "isb samplebuf");
	
	/* for end result, ISBSamples point to this in non OSA case, otherwise to pixstruct->shadfac */
	if(R.osa==0)
		isbdata->shadfacs= MEM_callocN(pa->rectx*pa->recty*sizeof(short), "isb shadfacs");
	
	/* setup bsp root */
	memset(&root, 0, sizeof(ISBBranch));
	root.box.xmin= (float)shb->size;
	root.box.ymin= (float)shb->size;
	
	/* create the sample buffers */
	for(sindex=0, y=0; y<pa->recty; y++) {
		for(x=0; x<pa->rectx; x++, sindex++) {
			
			/* this makes it a long function, but splitting it out would mean 10+ arguments */
			/* first check OSA case */
			if(R.osa) {
				rd= pa->rectdaps + sindex;
				if(*rd) {
					float xs= (float)(x + pa->disprect.xmin);
					float ys= (float)(y + pa->disprect.ymin);
					
					for(sample=0; sample<R.osa; sample++) {
						PixStr *ps= (PixStr *)(*rd);
						int mask= (1<<sample);
						
						while(ps) {
							if(ps->mask & mask)
								break;
							ps= ps->next;
						}
						if(ps && ps->facenr>0) {
							ObjectInstanceRen *obi= &R.objectinstance[ps->obi];
							ObjectRen *obr= obi->obr;
							VlakRen *vlr= RE_findOrAddVlak(obr, (ps->facenr-1) & RE_QUAD_MASK);
							
							samp= samplebuf[sample] + sindex;
							/* convert image plane pixel location to lamp buffer space */
							if(viewpixel_to_lampbuf(shb, obi, vlr, xs + R.jit[sample][0], ys + R.jit[sample][1], samp->zco)) {
								samp->obi= ps->obi;
								samp->facenr= ps->facenr & ~RE_QUAD_OFFS;
								ps->shadfac= 0;
								samp->shadfac= &ps->shadfac;
								bound_rectf((rctf *)&root.box, samp->zco);
							}
						}
					}
				}
			}
			else {
				rectp= pa->rectp + sindex;
				recto= pa->recto + sindex;
				if(*rectp>0) {
					ObjectInstanceRen *obi= &R.objectinstance[*recto];
					ObjectRen *obr= obi->obr;
					VlakRen *vlr= RE_findOrAddVlak(obr, (*rectp-1) & RE_QUAD_MASK);
					float xs= (float)(x + pa->disprect.xmin);
					float ys= (float)(y + pa->disprect.ymin);
					
					samp= samplebuf[0] + sindex;
					/* convert image plane pixel location to lamp buffer space */
					if(viewpixel_to_lampbuf(shb, obi, vlr, xs, ys, samp->zco)) {
						samp->obi= *recto;
						samp->facenr= *rectp & ~RE_QUAD_OFFS;
						samp->shadfac= isbdata->shadfacs + sindex;
						bound_rectf((rctf *)&root.box, samp->zco);
					}
				}
			}
		}
	}
	
	/* simple method to see if we have samples */
	if(root.box.xmin != (float)shb->size) {
		/* now create a regular split, root.box has the initial bounding box of all pixels */
		/* split bsp 8 levels deep, in regular grid (16 x 16) */
		isb_bsp_split_init(&root, memarena, 8);
		
		/* insert all samples in BSP now */
		bsp_err= isb_add_samples(pa, &root, memarena, samplebuf);
			
		if(bsp_err==0) {
			/* go over all faces and fill in shadow values */
			
			isb_bsp_fillfaces(&R, lar, &root);	/* shb->persmat should have been calculated */
			
			/* copy shadow samples to persistent buffer, reduce memory overhead */
			if(R.osa) {
				ISBShadfacA **isbsa= isbdata->shadfaca= MEM_callocN(pa->rectx*pa->recty*sizeof(void *), "isb shadfacs");
				
				isbdata->memarena = BLI_memarena_new(0x8000 * sizeof(ISBSampleA), "isb arena");
				BLI_memarena_use_calloc(isbdata->memarena);

				for(rd= pa->rectdaps, x=pa->rectx*pa->recty; x>0; x--, rd++, isbsa++) {
					
					if(*rd) {
						PixStr *ps= (PixStr *)(*rd);
						while(ps) {
							if(ps->shadfac)
								isb_add_shadfac(isbsa, isbdata->memarena, ps->obi, ps->facenr, ps->shadfac, count_mask(ps->mask));
							ps= ps->next;
						}
					}
				}
			}
		}
	}
	else {
		if(isbdata->shadfacs) {
			MEM_freeN(isbdata->shadfacs);
			isbdata->shadfacs= NULL;
		}
	}

	/* free BSP */
	BLI_memarena_free(memarena);
	
	/* free samples */
	for(x=0; x<(R.osa?R.osa:1); x++)
		MEM_freeN(samplebuf[x]);
	
	if(bsp_err) printf("error in filling bsp\n");
}

/* add sample to buffer, isbsa is the root sample in a buffer */
static ISBSampleA *isb_alloc_sample_transp(ISBSampleA **isbsa, MemArena *mem)
{
	ISBSampleA *new;
	
	new= BLI_memarena_alloc(mem, sizeof(ISBSampleA));
	if(*isbsa)
		new->next= (*isbsa);
	else
		new->next= NULL;
	
	*isbsa= new;
	return new;
}

/* adding samples in BSP, transparent case */
static int isb_add_samples_transp(RenderPart *pa, ISBBranch *root, MemArena *memarena, ISBSampleA ***samplebuf)
{
	int xi, yi, *xcos, *ycos;
	int sample, bsp_err= 0;
	
	/* bsp split doesn't like to handle regular sequences */
	xcos= MEM_mallocN( pa->rectx*sizeof(int), "xcos");
	ycos= MEM_mallocN( pa->recty*sizeof(int), "ycos");
	for(xi=0; xi<pa->rectx; xi++)
		xcos[xi]= xi;
	for(yi=0; yi<pa->recty; yi++)
		ycos[yi]= yi;
	BLI_array_randomize(xcos, sizeof(int), pa->rectx, 12345);
	BLI_array_randomize(ycos, sizeof(int), pa->recty, 54321);
	
	for(sample=0; sample<(R.osa?R.osa:1); sample++) {
		ISBSampleA **samp= samplebuf[sample], *samp1;
		
		for(yi=0; yi<pa->recty; yi++) {
			int y= ycos[yi];
			for(xi=0; xi<pa->rectx; xi++) {
				int x= xcos[xi];
				
				samp1= *(samp + y*pa->rectx + x);
				while(samp1) {
					bsp_err |= isb_bsp_insert(root, memarena, (ISBSample *)samp1);
					samp1= samp1->next;
				}
			}
			if(bsp_err) break;
		}
	}	
	
	MEM_freeN(xcos);
	MEM_freeN(ycos);
	
	return bsp_err;
}


/* Ztransp version */
/* lar->shb, pa->rectz and pa->rectp should exist */
static void isb_make_buffer_transp(RenderPart *pa, APixstr *apixbuf, LampRen *lar)
{
	ShadBuf *shb= lar->shb;
	ISBData *isbdata;
	ISBSampleA *samp, **samplebuf[16];	/* MAX_OSA */
	ISBBranch root;
	MemArena *memarena;
	APixstr *ap;
	int x, y, sindex, sample, bsp_err=0;
	
	/* storage for shadow, per thread */
	isbdata= shb->isb_result[pa->thread];
	
	/* to map the shi->xs and ys coordinate */
	isbdata->minx= pa->disprect.xmin;
	isbdata->miny= pa->disprect.ymin;
	isbdata->rectx= pa->rectx;
	isbdata->recty= pa->recty;
	
	/* branches are added using memarena (32k branches) */
	memarena = BLI_memarena_new(0x8000 * sizeof(ISBBranch), "isb arena");
	BLI_memarena_use_calloc(memarena);
	
	/* samplebuf is in camera view space (pixels) */
	for(sample=0; sample<(R.osa?R.osa:1); sample++)
		samplebuf[sample]= MEM_callocN(sizeof(void *)*pa->rectx*pa->recty, "isb alpha samplebuf");
	
	/* setup bsp root */
	memset(&root, 0, sizeof(ISBBranch));
	root.box.xmin= (float)shb->size;
	root.box.ymin= (float)shb->size;

	/* create the sample buffers */
	for(ap= apixbuf, sindex=0, y=0; y<pa->recty; y++) {
		for(x=0; x<pa->rectx; x++, sindex++, ap++) {
			
			if(ap->p[0]) {
				APixstr *apn;
				float xs= (float)(x + pa->disprect.xmin);
				float ys= (float)(y + pa->disprect.ymin);
				
				for(apn=ap; apn; apn= apn->next) {
					int a;
					for(a=0; a<4; a++) {
						if(apn->p[a]) {
							ObjectInstanceRen *obi= &R.objectinstance[apn->obi[a]];
							ObjectRen *obr= obi->obr;
							VlakRen *vlr= RE_findOrAddVlak(obr, (apn->p[a]-1) & RE_QUAD_MASK);
							float zco[3];
							
							/* here we store shadfac, easier to create the end storage buffer. needs zero'ed, multiple shadowbufs use it */
							apn->shadfac[a]= 0;
							
							if(R.osa) {
								for(sample=0; sample<R.osa; sample++) {
									int mask= (1<<sample);
									
									if(apn->mask[a] & mask) {
										
										/* convert image plane pixel location to lamp buffer space */
										if(viewpixel_to_lampbuf(shb, obi, vlr, xs + R.jit[sample][0], ys + R.jit[sample][1], zco)) {
											samp= isb_alloc_sample_transp(samplebuf[sample] + sindex, memarena);
											samp->obi= apn->obi[a];
											samp->facenr= apn->p[a] & ~RE_QUAD_OFFS;
											samp->shadfac= &apn->shadfac[a];
											
											copy_v3_v3(samp->zco, zco);
											bound_rectf((rctf *)&root.box, samp->zco);
										}
									}
								}
							}
							else {
								
								/* convert image plane pixel location to lamp buffer space */
								if(viewpixel_to_lampbuf(shb, obi, vlr, xs, ys, zco)) {
									
									samp= isb_alloc_sample_transp(samplebuf[0] + sindex, memarena);
									samp->obi= apn->obi[a];
									samp->facenr= apn->p[a] & ~RE_QUAD_OFFS;
									samp->shadfac= &apn->shadfac[a];
									
									copy_v3_v3(samp->zco, zco);
									bound_rectf((rctf *)&root.box, samp->zco);
								}
							}
						}
					}
				}
			}
		}
	}
	
	/* simple method to see if we have samples */
	if(root.box.xmin != (float)shb->size) {
		/* now create a regular split, root.box has the initial bounding box of all pixels */
		/* split bsp 8 levels deep, in regular grid (16 x 16) */
		isb_bsp_split_init(&root, memarena, 8);
		
		/* insert all samples in BSP now */
		bsp_err= isb_add_samples_transp(pa, &root, memarena, samplebuf);
		
		if(bsp_err==0) {
			ISBShadfacA **isbsa;
			
			/* go over all faces and fill in shadow values */
			isb_bsp_fillfaces(&R, lar, &root);	/* shb->persmat should have been calculated */
			
			/* copy shadow samples to persistent buffer, reduce memory overhead */
			isbsa= isbdata->shadfaca= MEM_callocN(pa->rectx*pa->recty*sizeof(void *), "isb shadfacs");
			
			isbdata->memarena = BLI_memarena_new(0x8000 * sizeof(ISBSampleA), "isb arena");
			
			for(ap= apixbuf, x=pa->rectx*pa->recty; x>0; x--, ap++, isbsa++) {
					
				if(ap->p[0]) {
					APixstr *apn;
					for(apn=ap; apn; apn= apn->next) {
						int a;
						for(a=0; a<4; a++) {
							if(apn->p[a] && apn->shadfac[a]) {
								if(R.osa)
									isb_add_shadfac(isbsa, isbdata->memarena, apn->obi[a], apn->p[a], apn->shadfac[a], count_mask(apn->mask[a]));
								else
									isb_add_shadfac(isbsa, isbdata->memarena, apn->obi[a], apn->p[a], apn->shadfac[a], 0);
							}
						}
					}
				}
			}
		}
	}

	/* free BSP */
	BLI_memarena_free(memarena);

	/* free samples */
	for(x=0; x<(R.osa?R.osa:1); x++)
		MEM_freeN(samplebuf[x]);

	if(bsp_err) printf("error in filling bsp\n");
}



/* exported */

/* returns amount of light (1.0 = no shadow) */
/* note, shadepixel() rounds the coordinate, not the real sample info */
float ISB_getshadow(ShadeInput *shi, ShadBuf *shb)
{
	/* if raytracing, we can't accept irregular shadow */
	if(shi->depth==0) {
		ISBData *isbdata= shb->isb_result[shi->thread];
		
		if(isbdata) {
			if(isbdata->shadfacs || isbdata->shadfaca) {
				int x= shi->xs - isbdata->minx;
				
				if(x >= 0 && x < isbdata->rectx) {
					int y= shi->ys - isbdata->miny;
			
					if(y >= 0 && y < isbdata->recty) {
						if(isbdata->shadfacs) {
							short *sp= isbdata->shadfacs + y*isbdata->rectx + x;
							return *sp>=4096?0.0f:1.0f - ((float)*sp)/4096.0f;
						}
						else {
							int sindex= y*isbdata->rectx + x;
							int obi= shi->obi - R.objectinstance;
							ISBShadfacA *isbsa= *(isbdata->shadfaca + sindex);
							
							while(isbsa) {
								if(isbsa->facenr==shi->facenr+1 && isbsa->obi==obi)
									return isbsa->shadfac>=1.0f?0.0f:1.0f - isbsa->shadfac;
								isbsa= isbsa->next;
							}
						}
					}
				}
			}
		}
	}
	return 1.0f;
}

/* part is supposed to be solid zbuffered (apixbuf==NULL) or transparent zbuffered */
void ISB_create(RenderPart *pa, APixstr *apixbuf)
{
	GroupObject *go;
	
	/* go over all lamps, and make the irregular buffers */
	for(go=R.lights.first; go; go= go->next) {
		LampRen *lar= go->lampren;
		
		if(lar->type==LA_SPOT && lar->shb && lar->buftype==LA_SHADBUF_IRREGULAR) {
			
			/* create storage for shadow, per thread */
			lar->shb->isb_result[pa->thread]= MEM_callocN(sizeof(ISBData), "isb data");
			
			if(apixbuf)
				isb_make_buffer_transp(pa, apixbuf, lar);
			else
				isb_make_buffer(pa, lar);
		}
	}
}


/* end of part rendering, free stored shadow data for this thread from all lamps */
void ISB_free(RenderPart *pa)
{
	GroupObject *go;
	
	/* go over all lamps, and free the irregular buffers */
	for(go=R.lights.first; go; go= go->next) {
		LampRen *lar= go->lampren;
		
		if(lar->type==LA_SPOT && lar->shb && lar->buftype==LA_SHADBUF_IRREGULAR) {
			ISBData *isbdata= lar->shb->isb_result[pa->thread];

			if(isbdata) {
				if(isbdata->shadfacs)
					MEM_freeN(isbdata->shadfacs);
				if(isbdata->shadfaca)
					MEM_freeN(isbdata->shadfaca);
				
				if(isbdata->memarena)
					BLI_memarena_free(isbdata->memarena);
				
				MEM_freeN(isbdata);
				lar->shb->isb_result[pa->thread]= NULL;
			}
		}
	}
}
