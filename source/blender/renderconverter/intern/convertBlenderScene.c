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
 * Interface to transform the Blender scene into renderable data.
 */

/* check for dl->flag, 1 or 2 should be replaced be the def's below */
#define STRUBI hack
#define DL_CYCLIC_U 1
#define DL_CYCLIC_V 2

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>          /* for INT_MAX                 */

#include "blendef.h"
#include "MTC_matrixops.h"

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"

#include "DNA_scene_types.h"
#include "DNA_lamp_types.h"
#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DNA_curve_types.h"
#include "DNA_texture_types.h"
#include "DNA_lattice_types.h"
#include "DNA_effect_types.h"
#include "DNA_ika_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_space_types.h"

#include "BKE_mesh.h"
#include "BKE_key.h"
#include "BKE_action.h"
#include "BKE_curve.h"
#include "BKE_armature.h"
#include "BKE_object.h"
#include "BKE_texture.h"
#include "BKE_material.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_anim.h"
#include "BKE_global.h"
#include "BKE_effect.h"
#include "BKE_world.h"
#include "BKE_ipo.h"
#include "BKE_ika.h"
#include "BKE_displist.h"
#include "BKE_lattice.h"
#include "BKE_constraint.h"
#include "BKE_utildefines.h"
#include "BKE_subsurf.h"
#include "BKE_world.h"

#include "render.h"

#include "RE_renderconverter.h"

#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_editkey.h"

#include "BSE_sequence.h"

#include "nla.h"

#include "BPY_extern.h"

#include "butspace.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* yafray: VertRen vertices are not transformed, transform is replaced with the identity matrix,
 * since yafray needs the untransformed meshes to calculate orco texture space as in Blender.
 * This is not true for lamps however, which use a copy of the original object matrix instead.
 * Duplicated data objects & dupliframe/duplivert objects are only stored once,
 * only the matrix is stored for all others, in yafray these objects are instances of the original.
 * The main changes are in RE_rotateBlenderScene().
 */

/* ------------------------------------------------------------------------- */
/* Local functions                                                           */
/* ------------------------------------------------------------------------- */
static Material *give_render_material(Object *ob, int nr);



/* blenderWorldManipulation.c */
static void split_u_renderfaces(int startvlak, int startvert, int usize, int plek, int cyclu);
static void split_v_renderfaces(int startvlak, int startvert, int usize, int vsize, int plek, int cyclu, int cyclv);
static int contrpuntnormr(float *n, float *puno);
static void normalenrender(int startvert, int startvlak);
static void as_addvert(VertRen *v1, VlakRen *vlr);
static void as_freevert(VertRen *ver);
static void autosmooth(int startvert, int startvlak, int degr);
static int mesh_test_flipnorm(Object *ob, MFace *mface, VlakRen *vlr, float imat[][3]);
static void render_particle_system(Object *ob, PartEff *paf);
static void render_static_particle_system(Object *ob, PartEff *paf);
static int verghalo(const void *a1, const void *a2);
static void sort_halos(void);
static void init_render_mball(Object *ob);
static void init_render_mesh(Object *ob);
static void init_render_surf(Object *ob);
static void init_render_curve(Object *ob);
static int test_flipnorm(float *v1, float *v2, float *v3, VlakRen *vlr, float imat[][3]);
static void init_render_object(Object *ob);
static HaloRen *initstar(float *vec, float hasize);

/* Displacement Texture */
void displace_render_face(VlakRen *vlr, float *scale);
void do_displacement(Object *ob, int startface, int numface, int startvert, int numvert);
short test_for_displace(Object *ob);
void displace_render_vert(ShadeInput *shi, VertRen *vr, float *scale);

/* more prototypes for autosmoothing below */


/* ------------------------------------------------------------------------- */
/* tool functions/defines for ad hoc simplification and possible future 
   cleanup      */
/* ------------------------------------------------------------------------- */

#define UVTOINDEX(u,v) (startvlak + (u) * sizev + (v))
#define GETNORMAL(face,normal) CalcNormFloat4(face->v1->co, face->v2->co, face->v3->co, face->v4->co, normal)
/*

NOTE THAT U/V COORDINATES ARE SOMETIMES SWAPPED !!
	
^	()----p4----p3----()
|	|     |     |     |
u	|     |  F1 |  F2 |
	|     |     |     |
	()----p1----p2----()
	       v ->
*/

/* ------------------------------------------------------------------------- */

/* Stuff for stars. This sits here because it uses gl-things. Part of
   this code may move down to the converter.  */
/* ------------------------------------------------------------------------- */
/* this is a bad beast, since it is misused by the 3d view drawing as well. */

extern unsigned char hash[512];

/* there must be a 'fixed' amount of stars generated between
 *         near and far
 * all stars must by preference lie on the far and solely
 *        differ in clarity/color
 */

void RE_make_stars(void (*initfunc)(void),
				   void (*vertexfunc)(float*),
				   void (*termfunc)(void))
{
	HaloRen *har;
	double dblrand, hlfrand;
	float vec[4], fx, fy, fz;
	float fac, starmindist, clipend;
	float mat[4][4], stargrid, maxrand, force, alpha;
/* 	float loc_far_var, loc_near_var; */
	int x, y, z, sx, sy, sz, ex, ey, ez, maxjit, done = 0;
	Camera * camera;

	if(initfunc) R.wrld= *(G.scene->world);

	stargrid = R.wrld.stardist;		/* distance between stars */
	maxrand = 2.0;						/* amount a star can be shifted (in grid units) */
	maxjit = (256.0* R.wrld.starcolnoise);			/* amount a color is being shifted */

/* 	loc_far_var = R.far; */
/* 	loc_near_var = R.near; */


	/* size of stars */
	force = ( R.wrld.starsize );

	/* minimal free space (starting at camera) */
	starmindist= R.wrld.starmindist;

	if (stargrid <= 0.10) return;

	if (!initfunc) R.flag |= R_HALO;
	else stargrid *= 1.0;				/* then it draws fewer */


	MTC_Mat4Invert(mat, R.viewmat);

	/* BOUNDING BOX CALCULATION
	 * bbox goes from z = loc_near_var | loc_far_var,
	 * x = -z | +z,
	 * y = -z | +z
	 */

	camera = G.scene->camera->data;
	clipend = camera->clipend;

	/* convert to grid coordinates */

	sx = ((mat[3][0] - clipend) / stargrid) - maxrand;
	sy = ((mat[3][1] - clipend) / stargrid) - maxrand;
	sz = ((mat[3][2] - clipend) / stargrid) - maxrand;

	ex = ((mat[3][0] + clipend) / stargrid) + maxrand;
	ey = ((mat[3][1] + clipend) / stargrid) + maxrand;
	ez = ((mat[3][2] + clipend) / stargrid) + maxrand;

	dblrand = maxrand * stargrid;
	hlfrand = 2.0 * dblrand;

	if (initfunc) {
		initfunc();	
	}

	for (x = sx, fx = sx * stargrid; x <= ex; x++, fx += stargrid) {
		for (y = sy, fy = sy * stargrid; y <= ey ; y++, fy += stargrid) {
			for (z = sz, fz = sz * stargrid; z <= ez; z++, fz += stargrid) {

				BLI_srand((hash[z & 0xff] << 24) + (hash[y & 0xff] << 16) + (hash[x & 0xff] << 8));
				vec[0] = fx + (hlfrand * BLI_drand()) - dblrand;
				vec[1] = fy + (hlfrand * BLI_drand()) - dblrand;
				vec[2] = fz + (hlfrand * BLI_drand()) - dblrand;
				vec[3] = 1.0;

				if (vertexfunc) {
					if(done & 1) vertexfunc(vec);
					done++;
				}
				else {
					MTC_Mat4MulVecfl(R.viewmat, vec);

					/* in vec are global coordinates
					 * calculate distance to camera
					 * and using that, define the alpha
					 */

					{
						float tx, ty, tz;

						tx = vec[0];
						ty = vec[1];
						tz = vec[2];

						alpha = sqrt(tx * tx + ty * ty + tz * tz);

						if (alpha >= clipend) alpha = 0.0;
						else if (alpha <= starmindist) alpha = 0.0;
						else if (alpha <= 2.0 * starmindist) {
							alpha = (alpha - starmindist) / starmindist;
						} else {
							alpha -= 2.0 * starmindist;
							alpha /= (clipend - 2.0 * starmindist);
							alpha = 1.0 - alpha;
						}
					}


					if (alpha != 0.0) {
						fac = force * BLI_drand();

						har = initstar(vec, fac);

						if (har) {
							har->alfa = sqrt(sqrt(alpha));
							har->add= 255;
							har->r = har->g = har->b = 255;
							if (maxjit) {
								har->r += ((maxjit * BLI_drand()) ) - maxjit;
								har->g += ((maxjit * BLI_drand()) ) - maxjit;
								har->b += ((maxjit * BLI_drand()) ) - maxjit;
							}
							har->hard = 32;

							har->type |= HA_ONLYSKY;
							done++;
						}
					}
				}
			}
//			if(done > MAXVERT) {
//				printf("Too many stars\n");
//				break;
//			}
			if(blender_test_break()) break;
		}
//		if(done > MAXVERT) break;

		if(blender_test_break()) break;
	}
	if (termfunc) termfunc();
}

/* ------------------------------------------------------------------------ */
/* more star stuff */
static HaloRen *initstar(float *vec, float hasize)
{
	HaloRen *har;
	float hoco[4];

	RE_projectverto(vec, hoco);

	if(	(R.r.mode & R_PANORAMA) ||  RE_testclip(hoco)==0 ) {
		har= RE_findOrAddHalo(R.tothalo++);
	
		/* projectvert is done in function zbufvlaggen again, because of parts */
		VECCOPY(har->co, vec);
		har->hasize= hasize;
	
		har->zd= 0.0;
	
		return har;
	}
	return NULL;
}



/* ------------------------------------------------------------------------- */

static void split_u_renderfaces(int startvlak, int startvert, int usize, int plek, int cyclu)
{
	VlakRen *vlr;
	VertRen *v1, *v2;
	int a, v;

	if(cyclu) cyclu= 1;

	/* first give all involved vertices a pointer to the new one */
	v= startvert+ plek*usize;
	for(a=0; a<usize; a++) {
		v2= RE_findOrAddVert(R.totvert++);
		v1= RE_findOrAddVert(v++);
		*v2= *v1;
		v1->sticky= (float *)v2;
	}

	/* check involved faces and replace pointers */
	v= startvlak+plek*(usize-1+cyclu);
	for(a=1-cyclu; a<usize; a++) {
		vlr= RE_findOrAddVlak(v++);
		vlr->v1= (VertRen *)(vlr->v1->sticky);
		vlr->v2= (VertRen *)(vlr->v2->sticky);
	}

}

/* ------------------------------------------------------------------------- */

static void split_v_renderfaces(int startvlak, int startvert, int usize, int vsize, int plek, int cyclu, int cyclv)
{
	VlakRen *vlr;
	VertRen *v1=0;
	int a, vlak, ofs;

	if(vsize<2) return;

	/* check involved faces and create doubles */
	/* because (evt) split_u already has been done, you cannot work with vertex->sticky pointers */
	/* because faces do not share vertices anymore */

	if(plek+cyclu==usize) plek= -1;

	vlak= startvlak+(plek+cyclu);
	ofs= (usize-1+cyclu);

	for(a=1; a<vsize; a++) {

		vlr= RE_findOrAddVlak(vlak);
		if (vlr->v1 == 0) return; /* OOPS, when not cyclic */

		v1= RE_findOrAddVert(R.totvert++);
		*v1= *(vlr->v1);

		vlr->v1= v1;

		/* vlr= findOrAddVlak(vlak+1); */
		/* vlr->v1= v1; */

		if(a>1) {
			vlr= RE_findOrAddVlak(vlak-ofs);
			if(vlr->v4->sticky) {
				v1= RE_findOrAddVert(R.totvert++);
				*v1= *(vlr->v4);
				vlr->v4= v1;
			}
			else vlr->v4= v1;
		}

		if(a== vsize-1) {
			if(cyclv) {
				;
			}
			else {
				vlr= RE_findOrAddVlak(vlak);
				v1= RE_findOrAddVert(R.totvert++);
				*v1= *(vlr->v4);
				vlr->v4= v1;
			}
		}

		vlak+= ofs;
	}

}

/* ------------------------------------------------------------------------- */

static int contrpuntnormr(float *n, float *puno)
{
	float inp;

	inp=n[0]*puno[0]+n[1]*puno[1]+n[2]*puno[2];
	if(inp<0.0) return 1;
	return 0;
}

/* ------------------------------------------------------------------------- */

static void normalenrender(int startvert, int startvlak)
{
	VlakRen *vlr;
	VertRen *ver, *adrve1, *adrve2, *adrve3, *adrve4;
	float n1[3], n2[3], n3[3], n4[3], *adrco, *tfl, fac, *temp;
	int a;

	if(R.totvlak==0 || R.totvert==0) return;
	if(startvert==R.totvert || startvlak==R.totvlak) return;

	adrco= (float *)MEM_callocN(12+4*sizeof(float)*(R.totvlak-startvlak), "normalen1");

	tfl= adrco;
	/* calculate cos of angles and point-masses */
	for(a= startvlak; a<R.totvlak; a++) {
		vlr= RE_findOrAddVlak(a);

		adrve1= vlr->v1;
		adrve2= vlr->v2;
		adrve3= vlr->v3;
		adrve4= vlr->v4;

		VecSubf(n1, adrve2->co, adrve1->co);
		Normalise(n1);
		VecSubf(n2, adrve3->co, adrve2->co);
		Normalise(n2);
		if(adrve4==0) {
			VecSubf(n3, adrve1->co, adrve3->co);
			Normalise(n3);

			*(tfl++)= saacos(-n1[0]*n3[0]-n1[1]*n3[1]-n1[2]*n3[2]);
			*(tfl++)= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
			*(tfl++)= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
		}
		else {
			VecSubf(n3, adrve4->co, adrve3->co);
			Normalise(n3);
			VecSubf(n4, adrve1->co, adrve4->co);
			Normalise(n4);

			*(tfl++)= saacos(-n4[0]*n1[0]-n4[1]*n1[1]-n4[2]*n1[2]);
			*(tfl++)= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
			*(tfl++)= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			*(tfl++)= saacos(-n3[0]*n4[0]-n3[1]*n4[1]-n3[2]*n4[2]);
		}
	}

	/* clear all vertex normals */
	for(a=startvert; a<R.totvert; a++) {
		ver= RE_findOrAddVert(a);

		ver->n[0]=ver->n[1]=ver->n[2]= 0.0;
	}

	/* calculate normals and add it to the vertex normals (dutch: punt normaal) */
	tfl= adrco;
	for(a=startvlak; a<R.totvlak; a++) {
		vlr= RE_findOrAddVlak(a);

		adrve1= vlr->v1;
		adrve2= vlr->v2;
		adrve3= vlr->v3;
		adrve4= vlr->v4;

		temp= adrve1->n;
		fac= *(tfl++);
		if( vlr->flag & R_NOPUNOFLIP);
		else if( contrpuntnormr(vlr->n, temp) ) fac= -fac ;
		*(temp++) +=fac*vlr->n[0];
		*(temp++) +=fac*vlr->n[1];
		*(temp)   +=fac*vlr->n[2];

		temp= adrve2->n;
		fac= *(tfl++);
		if( vlr->flag & R_NOPUNOFLIP);
		else  if( contrpuntnormr(vlr->n, temp) ) fac= -fac ;
		*(temp++) +=fac*vlr->n[0];
		*(temp++) +=fac*vlr->n[1];
		*(temp)   +=fac*vlr->n[2];

		temp= adrve3->n;
		fac= *(tfl++);
		if( vlr->flag & R_NOPUNOFLIP);
		else if( contrpuntnormr(vlr->n, temp) ) fac= -fac ;
		*(temp++) +=fac*vlr->n[0];
		*(temp++) +=fac*vlr->n[1];
		*(temp)   +=fac*vlr->n[2];

		if(adrve4) {
			temp= adrve4->n;
			fac= *(tfl++);
			if( vlr->flag & R_NOPUNOFLIP);
			else if( contrpuntnormr(vlr->n, temp) ) fac= -fac ;
			*(temp++) +=fac*vlr->n[0];
			*(temp++) +=fac*vlr->n[1];
			*(temp)   +=fac*vlr->n[2];
		}
	}

	/* normalise vertex normals */
	for(a=startvert; a<R.totvert; a++) {
		ver= RE_findOrAddVert(a);

		Normalise(ver->n);
	}

	/* vertex normal (puno) switch flags for during render */
	for(a=startvlak; a<R.totvlak; a++) {
		vlr= RE_findOrAddVlak(a);

		if((vlr->flag & R_NOPUNOFLIP)==0) {
			adrve1= vlr->v1;
			adrve2= vlr->v2;
			adrve3= vlr->v3;
			adrve4= vlr->v4;
	
			vlr->puno &= ~15;
			fac= vlr->n[0]*adrve1->n[0]+vlr->n[1]*adrve1->n[1]+vlr->n[2]*adrve1->n[2];
			if(fac<0.0) vlr->puno= 1;
			fac= vlr->n[0]*adrve2->n[0]+vlr->n[1]*adrve2->n[1]+vlr->n[2]*adrve2->n[2];
			if(fac<0.0) vlr->puno+= 2;
			fac= vlr->n[0]*adrve3->n[0]+vlr->n[1]*adrve3->n[1]+vlr->n[2]*adrve3->n[2];
			if(fac<0.0) vlr->puno+= 4;
	
			if(adrve4) {
				fac= vlr->n[0]*adrve4->n[0]+vlr->n[1]*adrve4->n[1]+vlr->n[2]*adrve4->n[2];
				if(fac<0.0) vlr->puno+= 8;
			}
		}
	}

	MEM_freeN(adrco);
}

/* ------------------------------------------------------------------------- */
/* Autosmoothing:                                                            */
/* ------------------------------------------------------------------------- */

typedef struct ASvert {
	int totface;
	ListBase faces;
} ASvert;

typedef struct ASface {
	struct ASface *next, *prev;
	VlakRen *vlr[4];
	VertRen *nver[4];
} ASface;

/* prototypes: */
static int as_testvertex(VlakRen *vlr, VertRen *ver, ASvert *asv, float thresh);
static VertRen *as_findvertex(VlakRen *vlr, VertRen *ver, ASvert *asv, float thresh);


static void as_addvert(VertRen *v1, VlakRen *vlr)
{
	ASvert *asv;
	ASface *asf;
	int a;
	
	if(v1 == NULL) return;
	
	if(v1->svert==0) {
		v1->svert= MEM_callocN(sizeof(ASvert), "asvert");
		asv= v1->svert;
		asf= MEM_callocN(sizeof(ASface), "asface");
		BLI_addtail(&asv->faces, asf);
	}
	
	asv= v1->svert;
	asf= asv->faces.last;
	for(a=0; a<4; a++) {
		if(asf->vlr[a]==0) {
			asf->vlr[a]= vlr;
			asv->totface++;
			break;
		}
	}
	
	/* new face struct */
	if(a==4) {
		asf= MEM_callocN(sizeof(ASface), "asface");
		BLI_addtail(&asv->faces, asf);
		asf->vlr[0]= vlr;
		asv->totface++;
	}
}

static void as_freevert(VertRen *ver)
{
	ASvert *asv;

	asv= ver->svert;
	BLI_freelistN(&asv->faces);
	MEM_freeN(asv);
	ver->svert= NULL;
}

static int as_testvertex(VlakRen *vlr, VertRen *ver, ASvert *asv, float thresh) 
{
	/* return 1: vertex needs a copy */
	ASface *asf;
	float inp;
	int a;
	
	if(vlr==0) return 0;
	
	asf= asv->faces.first;
	while(asf) {
		for(a=0; a<4; a++) {
			if(asf->vlr[a] && asf->vlr[a]!=vlr) {
				inp= fabs( vlr->n[0]*asf->vlr[a]->n[0] + vlr->n[1]*asf->vlr[a]->n[1] + vlr->n[2]*asf->vlr[a]->n[2] );
				if(inp < thresh) return 1;
			}
		}
		asf= asf->next;
	}
	
	return 0;
}

static VertRen *as_findvertex(VlakRen *vlr, VertRen *ver, ASvert *asv, float thresh) 
{
	/* return when new vertex already was made */
	ASface *asf;
	float inp;
	int a;
	
	asf= asv->faces.first;
	while(asf) {
		for(a=0; a<4; a++) {
			if(asf->vlr[a] && asf->vlr[a]!=vlr) {
				/* this face already made a copy for this vertex! */
				if(asf->nver[a]) {
					inp= fabs( vlr->n[0]*asf->vlr[a]->n[0] + vlr->n[1]*asf->vlr[a]->n[1] + vlr->n[2]*asf->vlr[a]->n[2] );
					if(inp >= thresh) {
						return asf->nver[a];
					}
				}
			}
		}
		asf= asf->next;
	}
	
	return NULL;
}

static void autosmooth(int startvert, int startvlak, int degr)
{
	ASvert *asv;
	ASface *asf;
	VertRen *ver, *v1;
	VlakRen *vlr;
	float thresh;
	int a, b, totvert;
	
	thresh= cos( M_PI*((float)degr)/180.0 );
	
	/* initialize */
	for(a=startvert; a<R.totvert; a++) {
		ver= RE_findOrAddVert(a);
		ver->svert= 0;
	}
	
	/* step one: construct listbase of all vertices and pointers to faces */
	for(a=startvlak; a<R.totvlak; a++) {
		vlr= RE_findOrAddVlak(a);
		
		as_addvert(vlr->v1, vlr);
		as_addvert(vlr->v2, vlr);
		as_addvert(vlr->v3, vlr);
		as_addvert(vlr->v4, vlr);
	}
	
	/* we now test all vertices, when faces have a normal too much different: they get a new vertex */
	totvert= R.totvert;
	for(a=startvert; a<totvert; a++) {
		ver= RE_findOrAddVert(a);
		asv= ver->svert;
		if(asv && asv->totface>1) {
			
			asf= asv->faces.first;
			while(asf) {
				for(b=0; b<4; b++) {
				
					/* is there a reason to make a new vertex? */
					vlr= asf->vlr[b];
					if( as_testvertex(vlr, ver, asv, thresh) ) {
						
						/* already made a new vertex within threshold? */
						v1= as_findvertex(vlr, ver, asv, thresh);
						if(v1==0) {
							/* make a new vertex */
							v1= RE_findOrAddVert(R.totvert++);
							*v1= *ver;
							v1->svert= 0;
						}
						asf->nver[b]= v1;
						if(vlr->v1==ver) vlr->v1= v1;
						if(vlr->v2==ver) vlr->v2= v1;
						if(vlr->v3==ver) vlr->v3= v1;
						if(vlr->v4==ver) vlr->v4= v1;
					}
				}
				asf= asf->next;
			}
		}
	}
	
	/* free */
	for(a=startvert; a<R.totvert; a++) {
		ver= RE_findOrAddVert(a);
		if(ver->svert) as_freevert(ver);
	}
	
}

/* ------------------------------------------------------------------------- */
/* End of autosmoothing:                                                     */
/* ------------------------------------------------------------------------- */

static void make_render_halos(Object *ob, Mesh *me, int totvert, MVert *mvert, Material *ma, float *extverts)
{
	HaloRen *har;
	float xn, yn, zn, nor[3], view[3];
	float *orco, vec[3], hasize, mat[4][4], imat[3][3];
	int start, end, a, ok;

	MTC_Mat4MulMat4(mat, ob->obmat, R.viewmat);
	MTC_Mat3CpyMat4(imat, ob->imat);

	R.flag |= R_HALO;

	orco= me->orco;

	start= 0;
	end= totvert;
	set_buildvars(ob, &start, &end);
	mvert+= start;
	if(extverts) extverts+= 3*start;

	ma->ren->seed1= ma->seed1;

	for(a=start; a<end; a++, mvert++) {
		ok= 1;

		if(ok) {
			hasize= ma->hasize;

			if(extverts) {
				VECCOPY(vec, extverts);
				extverts+= 3;
			}
			else {
				VECCOPY(vec, mvert->co);
			}
			MTC_Mat4MulVecfl(mat, vec);

			if(ma->mode & MA_HALOPUNO) {
				xn= mvert->no[0];
				yn= mvert->no[1];
				zn= mvert->no[2];

				/* transpose ! */
				nor[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
				nor[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
				nor[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
				Normalise(nor);

				VECCOPY(view, vec);
				Normalise(view);

				zn= nor[0]*view[0]+nor[1]*view[1]+nor[2]*view[2];
				if(zn>=0.0) hasize= 0.0;
				else hasize*= zn*zn*zn*zn;
			}

			if(orco) har= RE_inithalo(ma, vec, 0, orco, hasize, 0);
			else har= RE_inithalo(ma, vec, 0, mvert->co, hasize, 0);
			if(har) har->lay= ob->lay;
		}
		if(orco) orco+= 3;
		ma->ren->seed1++;
	}
}

/* ------------------------------------------------------------------------- */

static int test_flipnorm(float *v1, float *v2, float *v3, VlakRen *vlr, float imat[][3])
{
	float nor[3], vec[3];
	float xn;
	
	CalcNormFloat(v1, v2, v3, nor);
	vec[0]= imat[0][0]*nor[0]+ imat[0][1]*nor[1]+ imat[0][2]*nor[2];
	vec[1]= imat[1][0]*nor[0]+ imat[1][1]*nor[1]+ imat[1][2]*nor[2];
	vec[2]= imat[2][0]*nor[0]+ imat[2][1]*nor[1]+ imat[2][2]*nor[2];

	xn= vec[0]*vlr->n[0]+vec[1]*vlr->n[1]+vec[2]*vlr->n[2];

	return (xn<0.0);
}

static int mesh_test_flipnorm(Object *ob, MFace *mface, VlakRen *vlr, float imat[][3])
{
	DispList *dl;
	Mesh *me= ob->data;
	float *v1, *v2, *v3;	

	dl= find_displist(&ob->disp, DL_VERTS);
	if(dl) {
		v1= dl->verts + 3*mface->v1;
		v2= dl->verts + 3*mface->v2;
		v3= dl->verts + 3*mface->v3;
	}
	else {
		v1= (me->mvert+mface->v1)->co;
		v2= (me->mvert+mface->v2)->co;
		v3= (me->mvert+mface->v3)->co;
	}
	
	return test_flipnorm(v1, v2, v3, vlr, imat);
}


/* ------------------------------------------------------------------------- */



/* ------------------------------------------------------------------------- */

static void render_particle_system(Object *ob, PartEff *paf)
{
	Particle *pa=0;
	HaloRen *har=0;
	Material *ma=0;
	float xn, yn, zn, imat[3][3], mat[4][4], hasize, ptime, ctime, vec[3], vec1[3], view[3], nor[3];
	int a, mat_nr=1;

	pa= paf->keys;
	if(pa==0) {
		build_particle_system(ob);
		pa= paf->keys;
		if(pa==0) return;
	}

	ma= give_render_material(ob, 1);
	if(ma==0) ma= &defmaterial;

	MTC_Mat4MulMat4(mat, ob->obmat, R.viewmat);
	MTC_Mat4Invert(ob->imat, mat);	/* this is correct, for imat texture */

	MTC_Mat4Invert(mat, R.viewmat);	/* particles do not have a ob transform anymore */
	MTC_Mat3CpyMat4(imat, mat);

	R.flag |= R_HALO;

	if(ob->ipoflag & OB_OFFS_PARTICLE) ptime= ob->sf;
	else ptime= 0.0;
	ctime= bsystem_time(ob, 0, (float)G.scene->r.cfra, ptime);
	ma->ren->seed1= ma->seed1;

	for(a=0; a<paf->totpart; a++, pa+=paf->totkey) {

		if(ctime > pa->time) {
			if(ctime < pa->time+pa->lifetime) {

				/* watch it: also calculate the normal of a particle */
				if(paf->stype==PAF_VECT || ma->mode & MA_HALO_SHADE) {
					where_is_particle(paf, pa, ctime, vec);
					MTC_Mat4MulVecfl(R.viewmat, vec);
					where_is_particle(paf, pa, ctime+1.0, vec1);
					MTC_Mat4MulVecfl(R.viewmat, vec1);
				}
				else {
					where_is_particle(paf, pa, ctime, vec);
					MTC_Mat4MulVecfl(R.viewmat, vec);
				}

				if(pa->mat_nr != mat_nr) {
					mat_nr= pa->mat_nr;
					ma= give_render_material(ob, mat_nr);
					if(ma==0) ma= &defmaterial;
				}

				if(ma->ipo) {
					/* correction for lifetime */
					ptime= 100.0*(ctime-pa->time)/pa->lifetime;
					calc_ipo(ma->ipo, ptime);
					execute_ipo((ID *)ma, ma->ipo);
				}

				hasize= ma->hasize;

				if(ma->mode & MA_HALOPUNO) {
					xn= pa->no[0];
					yn= pa->no[1];
					zn= pa->no[2];

					/* transpose ! */
					nor[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
					nor[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
					nor[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
					Normalise(nor);

					VECCOPY(view, vec);
					Normalise(view);

					zn= nor[0]*view[0]+nor[1]*view[1]+nor[2]*view[2];
					if(zn>=0.0) hasize= 0.0;
					else hasize*= zn*zn*zn*zn;
				}

				if(paf->stype==PAF_VECT) har= RE_inithalo(ma, vec, vec1, pa->co, hasize, paf->vectsize);
				else {
					har= RE_inithalo(ma, vec, 0, pa->co, hasize, 0);
					if(har && ma->mode & MA_HALO_SHADE) {
						VecSubf(har->no, vec, vec1);
						Normalise(har->no);
					}
				}
				if(har) har->lay= ob->lay;
			}
		}
		ma->ren->seed1++;
	}

}


/* ------------------------------------------------------------------------- */

static void render_static_particle_system(Object *ob, PartEff *paf)
{
	Particle *pa=0;
	HaloRen *har=0;
	Material *ma=0;
	float xn, yn, zn, imat[3][3], mat[4][4], hasize;
	float mtime, ptime, ctime, vec[3], vec1[3], view[3], nor[3];
	int a, mat_nr=1;

	pa= paf->keys;
	if(pa==0) {
		build_particle_system(ob);
		pa= paf->keys;
		if(pa==0) return;
	}

	ma= give_render_material(ob, 1);
	if(ma==0) ma= &defmaterial;

	MTC_Mat4MulMat4(mat, ob->obmat, R.viewmat);
	MTC_Mat4Invert(ob->imat, mat);	/* need to be that way, for imat texture */

	MTC_Mat3CpyMat4(imat, ob->imat);

	R.flag |= R_HALO;

	if(ob->ipoflag & OB_OFFS_PARTICLE) ptime= ob->sf;
	else ptime= 0.0;
	ctime= bsystem_time(ob, 0, (float)G.scene->r.cfra, ptime);
	ma->ren->seed1= ma->seed1;

	for(a=0; a<paf->totpart; a++, pa+=paf->totkey) {

		where_is_particle(paf, pa, pa->time, vec1);
		MTC_Mat4MulVecfl(mat, vec1);
		
		mtime= pa->time+pa->lifetime+paf->staticstep-1;
		
		for(ctime= pa->time; ctime<mtime; ctime+=paf->staticstep) {
			
			/* make sure hair grows until the end.. */
			if(ctime>pa->time+pa->lifetime) ctime= pa->time+pa->lifetime;
			

			/* watch it: also calc the normal of a particle */
			if(paf->stype==PAF_VECT || ma->mode & MA_HALO_SHADE) {
				where_is_particle(paf, pa, ctime+1.0, vec);
				MTC_Mat4MulVecfl(mat, vec);
			}
			else {
				where_is_particle(paf, pa, ctime, vec);
				MTC_Mat4MulVecfl(mat, vec);
			}

			if(pa->mat_nr != mat_nr) {
				mat_nr= pa->mat_nr;
				ma= give_render_material(ob, mat_nr);
				if(ma==0) ma= &defmaterial;
			}

			if(ma->ipo) {
				/* correction for lifetime */
				ptime= 100.0*(ctime-pa->time)/pa->lifetime;
				calc_ipo(ma->ipo, ptime);
				execute_ipo((ID *)ma, ma->ipo);
			}

			hasize= ma->hasize;

			if(ma->mode & MA_HALOPUNO) {
				xn= pa->no[0];
				yn= pa->no[1];
				zn= pa->no[2];

				/* transpose ! */
				nor[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
				nor[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
				nor[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
				Normalise(nor);

				VECCOPY(view, vec);
				Normalise(view);

				zn= nor[0]*view[0]+nor[1]*view[1]+nor[2]*view[2];
				if(zn>=0.0) hasize= 0.0;
				else hasize*= zn*zn*zn*zn;
			}

			if(paf->stype==PAF_VECT) har= RE_inithalo(ma, vec, vec1, pa->co, hasize, paf->vectsize);
			else {
				har= RE_inithalo(ma, vec, 0, pa->co, hasize, 0);
				if(har && (ma->mode & MA_HALO_SHADE)) {
					VecSubf(har->no, vec, vec1);
					Normalise(har->no);
					har->lay= ob->lay;
				}
			}

			VECCOPY(vec1, vec);
		}
		ma->ren->seed1++;
	}

}


/* ------------------------------------------------------------------------- */

static int verghalo(const void *a1, const void *a2)
{
	const struct halosort *x1=a1, *x2=a2;
	
	if( x1->z < x2->z ) return 1;
	else if( x1->z > x2->z) return -1;
	return 0;
}

/* ------------------------------------------------------------------------- */
extern int rblohalen;
static void sort_halos(void)
{
	struct halosort *hablock, *haso;
	HaloRen *har = NULL, **bloha;
	int a;

	if(R.tothalo==0) return;

	/* make datablock with halo pointers, sort */
	haso= hablock= MEM_mallocN(sizeof(struct halosort)*R.tothalo, "hablock");

	for(a=0; a<R.tothalo; a++) {
		if((a & 255)==0) har= R.bloha[a>>8];
		else har++;
		haso->har= har;
		haso->z= har->zs;
		haso++;
	}

	qsort(hablock, R.tothalo, sizeof(struct halosort), verghalo);

	/* re-assamble R.bloha */

	bloha= R.bloha;
	R.bloha= (HaloRen **)MEM_callocN(sizeof(void *)*(rblohalen),"Bloha");

	haso= hablock;
	for(a=0; a<R.tothalo; a++) {
		har= RE_findOrAddHalo(a);
		*har= *(haso->har);

		haso++;
	}

	/* free */
	a= 0;
	while(bloha[a]) {
		MEM_freeN(bloha[a]);
		a++;
	}
	MEM_freeN(bloha);
	MEM_freeN(hablock);

}



static Material *give_render_material(Object *ob, int nr)
{
	Object *temp;

	if(ob->flag & OB_FROMDUPLI) {
		temp= (Object *)ob->id.newid;
		if(temp && temp->type==OB_FONT) {
			ob= temp;
		}
	}

	return give_current_material(ob, nr);
}

/* ------------------------------------------------------------------------- */
static void init_render_mball(Object *ob)
{
	DispList *dl, *dlo;
	VertRen *ver;
	VlakRen *vlr, *vlr1;
	Material *ma;
	float *data, *nors, mat[4][4], imat[3][3], xn, yn, zn;
	int a, need_orco, startvert, *index;

	if (ob!=find_basis_mball(ob))
		return;

	/* yafray: set transform to identity matrix */
	if (R.r.renderer==R_YAFRAY)
		MTC_Mat4One(mat);
	else
		MTC_Mat4MulMat4(mat, ob->obmat, R.viewmat);
	MTC_Mat4Invert(ob->imat, mat);
	MTC_Mat3CpyMat4(imat, ob->imat);

	ma= give_render_material(ob, 1);
	if(ma==0) ma= &defmaterial;

	need_orco= 0;
	if(ma->ren->texco & TEXCO_ORCO) {
		need_orco= 1;
	}

	dlo= ob->disp.first;
	if(dlo) BLI_remlink(&ob->disp, dlo);

	makeDispList(ob);
	dl= ob->disp.first;
	if(dl==0) return;

	startvert= R.totvert;
	data= dl->verts;
	nors= dl->nors;

	for(a=0; a<dl->nr; a++, data+=3, nors+=3) {

		ver= RE_findOrAddVert(R.totvert++);
		VECCOPY(ver->co, data);
		MTC_Mat4MulVecfl(mat, ver->co);

		/* render normals are inverted in render */
		xn= -nors[0];
		yn= -nors[1];
		zn= -nors[2];

		/* transpose ! */
		ver->n[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
		ver->n[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
		ver->n[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
		Normalise(ver->n);

		if(need_orco) ver->orco= data;
	}

	index= dl->index;
	for(a=0; a<dl->parts; a++, index+=4) {

		vlr= RE_findOrAddVlak(R.totvlak++);
		vlr->ob= ob;
		vlr->v1= RE_findOrAddVert(startvert+index[0]);
		vlr->v2= RE_findOrAddVert(startvert+index[1]);
		vlr->v3= RE_findOrAddVert(startvert+index[2]);
		vlr->v4= 0;

		/* render normal are inverted */
		vlr->len= CalcNormFloat(vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->n);

		vlr->mat= ma;
		vlr->puno= 0;
		vlr->flag= ME_SMOOTH+R_NOPUNOFLIP;
		vlr->ec= 0;
		vlr->lay= ob->lay;

		/* mball -too bad- always has triangles, because quads can be non-planar */
		if(index[3]) {
			vlr1= RE_findOrAddVlak(R.totvlak++);
			*vlr1= *vlr;
			vlr1->v2= vlr1->v3;
			vlr1->v3= RE_findOrAddVert(startvert+index[3]);
			vlr->len= CalcNormFloat(vlr1->v1->co, vlr1->v2->co, vlr1->v3->co, vlr1->n);
		}
	}

	if(need_orco) {
		/* store displist and scale */
		make_orco_mball(ob);
		if(dlo) BLI_addhead(&ob->disp, dlo);

	}
	else {
		freedisplist(&ob->disp);
		if(dlo) BLI_addtail(&ob->disp, dlo);
	}
}
/* ------------------------------------------------------------------------- */
/* convert */
static void init_render_mesh(Object *ob)
{
	MFace *mface;
	MVert *mvert;
	Mesh *me;
	VlakRen *vlr, *vlr1;
	VertRen *ver;
	Material *ma;
	MFaceInt *mfaceint = NULL;
	MSticky *ms;
	PartEff *paf;
	DispList *dl;
	TFace *tface;
	unsigned int *vertcol;
	float xn, yn, zn, nor[3], imat[3][3], mat[4][4];
	float *extverts=0, *orco;
	int a, a1, ok, do_puno, need_orco=0, totvlako, totverto, vertofs;
	int start, end, flipnorm, do_autosmooth=0, totvert;
	DispListMesh *dlm;

	me= ob->data;

	paf = give_parteff(ob);
	if(paf) {
		if(paf->flag & PAF_STATIC) render_static_particle_system(ob, paf);
		else render_particle_system(ob, paf);
		return;
	}

	/* object_deform changes imat */
	do_puno= object_deform(ob);

	/* yafray: set transform to identity matrix */
	if (R.r.renderer==R_YAFRAY)
		MTC_Mat4One(mat);
	else
		MTC_Mat4MulMat4(mat, ob->obmat, R.viewmat);
	MTC_Mat4Invert(ob->imat, mat);
	MTC_Mat3CpyMat4(imat, ob->imat);

	if(me->totvert==0) return;

	totvlako= R.totvlak;
	totverto= R.totvert;

	if(me->key) do_puno= 1;

	if(ob->effect.first) {
		Effect *eff= ob->effect.first;
		while(eff) {
			if(eff->type==EFF_WAVE) do_puno= 1;
			eff= eff->next;
		}
	}

	if(me->orco==0) {
		need_orco= 0;
		for(a=1; a<=ob->totcol; a++) {
			ma= give_render_material(ob, a);
			if(ma) {
				if(ma->ren->texco & TEXCO_ORCO) {
					need_orco= 1;
					break;
				}
			}
		}
	}

	if (mesh_uses_displist(me)) {
		dl= me->disp.first;

		/* Force a displist rebuild if this is a subsurf and we have a different subdiv level */

		if((dl==0) || ((me->subdiv != me->subdivr))) {
			/* prevent subsurf called again for duplicate use of mesh, tface pointers change */
			if((me->subdivdone-1)!=me->subdivr) {
				DispList *dlVerts;

				object_deform(ob);

				dlVerts= find_displist(&ob->disp, DL_VERTS);
				dlm= subsurf_make_dispListMesh_from_mesh(me, dlVerts?dlVerts->verts:NULL, me->subdivr, me->flag);
				dl= MEM_callocN(sizeof(*dl), "dl");
				dl->type= DL_MESH;
				dl->mesh= dlm;

				free_displist_by_type(&me->disp, DL_MESH);
				BLI_addtail(&me->disp, dl);

				me->subdivdone= me->subdivr+1;	/* stupid hack, add one because otherwise old files will get
												* subdivdone==0, so me->subdivr==0 won't work. proper caching
												* will remove this hack.
												*/
			}
		}

		if(dl==0 || dl->type!=DL_MESH) return;
		dlm= dl->mesh;

		mvert= dlm->mvert;
		totvert= dlm->totvert;

		if(need_orco) {
			make_orco_displist_mesh(ob, me->subdivr);
		}
		ms= NULL; // no stick in displistmesh
	} else {
		dlm= NULL;
		mvert= me->mvert;
		totvert= me->totvert;

		dl= find_displist(&ob->disp, DL_VERTS);
		if(dl) extverts= dl->verts;
	
		if(need_orco) {
			make_orco_mesh(me);
		}
		ms= me->msticky;
	}
	
	orco= me->orco;

	ma= give_render_material(ob, 1);
	if(ma==0) ma= &defmaterial;


	if(ma->mode & MA_HALO) {
		make_render_halos(ob, me, totvert, mvert, ma, extverts);
	}
	else {

		for(a=0; a<totvert; a++, mvert++) {

			ver= RE_findOrAddVert(R.totvert++);
			if(extverts) {
				VECCOPY(ver->co, extverts);
				extverts+= 3;
			}
			else {
				VECCOPY(ver->co, mvert->co);
			}
			MTC_Mat4MulVecfl(mat, ver->co);

			xn= mvert->no[0];
			yn= mvert->no[1];
			zn= mvert->no[2];
			if(do_puno==0) {
				/* transpose ! */
				ver->n[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
				ver->n[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
				ver->n[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
				Normalise(ver->n);
			}
			if(orco) {
				ver->orco= orco;
				orco+=3;
			}
			if(ms) {
				ver->sticky= (float *)ms;
				ms++;
			}
		}
		/* still to do for keys: the correct local texture coordinate */

		flipnorm= -1;
		/* test for a flip in the matrix: then flip face normal as well */

		/* faces in order of color blocks */
		vertofs= R.totvert - totvert;
		for(a1=0; (a1<ob->totcol || (a1==0 && ob->totcol==0)); a1++) {

			ma= give_render_material(ob, a1+1);
			if(ma==0) ma= &defmaterial;
			
			/* test for 100% transparant */
			ok= 1;
			if(ma->alpha==0.0 && ma->spectra==0.0) {
				ok= 0;
				/* texture on transparency? */
				for(a=0; a<8; a++) {
					if(ma->mtex[a] && ma->mtex[a]->tex) {
						if(ma->mtex[a]->mapto & MAP_ALPHA) ok= 1;
					}
				}
			}

			if(ok) {
				TFace *tface= NULL;

				/* radio faces need autosmooth, to separate shared vertices in corners */
				if(R.r.mode & R_RADIO)
					if(ma->mode & MA_RADIO) 
						do_autosmooth= 1;
				
				start= 0;
				end= dlm?dlm->totface:me->totface;
				set_buildvars(ob, &start, &end);
				if (dlm) {
					mfaceint= dlm->mface + start;
					if (dlm->tface) {
						tface= dlm->tface + start;
						vertcol= dlm->tface->col;
					} else if (dlm->mcol) {
						vertcol= (unsigned int *)dlm->mcol;
					} else {
						vertcol= NULL;
					}
				} else {
					mfaceint= NULL;
					mface= ((MFace*) me->mface) + start;
					if (me->tface) {
						tface= ((TFace*) me->tface) + start;
						vertcol= ((TFace*) me->tface)->col;
					} else if (me->mcol) {
						vertcol= (unsigned int *)me->mcol;
					} else {
						vertcol= NULL;
					}
				}
				
				for(a=start; a<end; a++) {
					int mat_nr;
					int v1, v2, v3, v4, puno, edcode, flag;
					if (mfaceint) {
						mat_nr= mfaceint->mat_nr;
						v1= mfaceint->v1;
						v2= mfaceint->v2;
						v3= mfaceint->v3;
						v4= mfaceint->v4;
						flag= mfaceint->flag;
						puno= mfaceint->puno;
						edcode= mfaceint->edcode;
					} else {
						mat_nr= mface->mat_nr;
						v1= mface->v1;
						v2= mface->v2;
						v3= mface->v3;
						v4= mface->v4;
						flag= mface->flag;
						puno= mface->puno;
						edcode= mface->edcode;
					}
					if( mat_nr==a1 ) {

						if(v3) {

							vlr= RE_findOrAddVlak(R.totvlak++);
							vlr->ob= ob;
							vlr->v1= RE_findOrAddVert(vertofs+v1);
							vlr->v2= RE_findOrAddVert(vertofs+v2);
							vlr->v3= RE_findOrAddVert(vertofs+v3);
							if(v4) vlr->v4= RE_findOrAddVert(vertofs+v4);
							else vlr->v4= 0;

							/* render normals are inverted in render */
							if(vlr->v4) vlr->len= CalcNormFloat4(vlr->v4->co, vlr->v3->co, vlr->v2->co,
							    vlr->v1->co, vlr->n);
							else vlr->len= CalcNormFloat(vlr->v3->co, vlr->v2->co, vlr->v1->co,
							    vlr->n);

							vlr->mat= ma;
							vlr->puno= puno;
							vlr->flag= flag;
							if((me->flag & ME_NOPUNOFLIP) || (ma->mode & MA_RAYTRANSP)) {
								vlr->flag |= R_NOPUNOFLIP;
								vlr->puno |= 15;  // no flip
							}
							vlr->ec= edcode;
							vlr->lay= ob->lay;

							if(vlr->len==0) R.totvlak--;
							else {
								if(flipnorm== -1) {	/* per object test once */
									if (mfaceint) {
										flipnorm= test_flipnorm(dlm->mvert[v1].co, dlm->mvert[v2].co, dlm->mvert[v3].co, vlr, imat);
									} else {
										flipnorm= mesh_test_flipnorm(ob, mface, vlr, imat);
									}
								}
								if(flipnorm) {
									vlr->n[0]= -vlr->n[0];
									vlr->n[1]= -vlr->n[1];
									vlr->n[2]= -vlr->n[2];
								}

								if(vertcol) {
									if(tface) vlr->vcol= vertcol+sizeof(TFace)*a/4;	/* vertcol is int */
									else vlr->vcol= vertcol+sizeof(int)*a;
								}
								else vlr->vcol= 0;
								
								vlr->tface= tface;
								
							}
						}
						else if(v2 && (ma->mode & MA_WIRE)) {
							vlr= RE_findOrAddVlak(R.totvlak++);
							vlr->ob= ob;
							vlr->v1= RE_findOrAddVert(vertofs+v1);
							vlr->v2= RE_findOrAddVert(vertofs+v2);
							vlr->v3= vlr->v2;
							vlr->v4= 0;

							vlr->n[0]=vlr->n[1]=vlr->n[2]= 0.0;

							vlr->mat= ma;
							vlr->puno= puno;
							vlr->flag= flag;
							vlr->ec= ME_V1V2;
							vlr->lay= ob->lay;
						}
					}

					if (mfaceint) {
						mfaceint++;
					} else {
						mface++;
					}
					if(tface) tface++;
				}
			}
		}
	}
	
	if(do_autosmooth || (me->flag & ME_AUTOSMOOTH)) {
		autosmooth(totverto, totvlako, me->smoothresh);
		do_puno= 1;
	}
	
	if(do_puno) normalenrender(totverto, totvlako);

}

/* ------------------------------------------------------------------------- */

static void area_lamp_vectors(LampRen *lar)
{
	float xsize= 0.5*lar->area_size, ysize= 0.5*lar->area_sizey;

	/* corner vectors */
	lar->area[0][0]= lar->co[0] - xsize*lar->mat[0][0] - ysize*lar->mat[1][0];
	lar->area[0][1]= lar->co[1] - xsize*lar->mat[0][1] - ysize*lar->mat[1][1];
	lar->area[0][2]= lar->co[2] - xsize*lar->mat[0][2] - ysize*lar->mat[1][2];	

	/* corner vectors */
	lar->area[1][0]= lar->co[0] - xsize*lar->mat[0][0] + ysize*lar->mat[1][0];
	lar->area[1][1]= lar->co[1] - xsize*lar->mat[0][1] + ysize*lar->mat[1][1];
	lar->area[1][2]= lar->co[2] - xsize*lar->mat[0][2] + ysize*lar->mat[1][2];	

	/* corner vectors */
	lar->area[2][0]= lar->co[0] + xsize*lar->mat[0][0] + ysize*lar->mat[1][0];
	lar->area[2][1]= lar->co[1] + xsize*lar->mat[0][1] + ysize*lar->mat[1][1];
	lar->area[2][2]= lar->co[2] + xsize*lar->mat[0][2] + ysize*lar->mat[1][2];	

	/* corner vectors */
	lar->area[3][0]= lar->co[0] + xsize*lar->mat[0][0] - ysize*lar->mat[1][0];
	lar->area[3][1]= lar->co[1] + xsize*lar->mat[0][1] - ysize*lar->mat[1][1];
	lar->area[3][2]= lar->co[2] + xsize*lar->mat[0][2] - ysize*lar->mat[1][2];	
	/* only for correction button size, matrix size works on energy */
	lar->areasize= lar->dist*lar->dist/(4.0*xsize*ysize);
}

/* If lar takes more lamp data, the decoupling will be better. */
void RE_add_render_lamp(Object *ob, int doshadbuf)
{
	Lamp *la;
	LampRen *lar, **temp;
	float mat[4][4], hoek, xn, yn;
	int c;
	static int rlalen=LAMPINITSIZE; /*number of currently allocated lampren pointers*/
	
	if(R.totlamp>=rlalen) { /* Need more Lamp pointers....*/
		printf("Alocating %i more lamp groups, %i total.\n", 
			LAMPINITSIZE, rlalen+LAMPINITSIZE);
		temp=R.la;
		R.la=(LampRen**)MEM_callocN(sizeof(void*)*(rlalen+LAMPINITSIZE) , "renderlamparray");
		memcpy(R.la, temp, rlalen*sizeof(void*));
		memset(&(R.la[R.totlamp]), 0, LAMPINITSIZE*sizeof(void*));
		rlalen+=LAMPINITSIZE;  
		MEM_freeN(temp);	
	}
	
	la= ob->data;
	lar= (LampRen *)MEM_callocN(sizeof(LampRen),"lampren");
	R.la[R.totlamp++]= lar;

	/* yafray: in this case the lamp matrix is a copy of the object matrix */
	if (R.r.renderer==R_YAFRAY)
		MTC_Mat4CpyMat4(mat, ob->obmat);
	else
		MTC_Mat4MulMat4(mat, ob->obmat, R.viewmat);
	MTC_Mat4Invert(ob->imat, mat);

	MTC_Mat3CpyMat4(lar->mat, mat);
	MTC_Mat3CpyMat4(lar->imat, ob->imat);

	lar->bufsize = la->bufsize;
	lar->samp = la->samp;
	lar->soft = la->soft;
	lar->shadhalostep = la->shadhalostep;
	lar->clipsta = la->clipsta;
	lar->clipend = la->clipend;
	lar->bias = la->bias;

	lar->type= la->type;
	lar->mode= la->mode;

	lar->energy= la->energy;
	lar->energy= la->energy;
	if(la->mode & LA_NEG) lar->energy= -lar->energy;

	lar->vec[0]= -mat[2][0];
	lar->vec[1]= -mat[2][1];
	lar->vec[2]= -mat[2][2];
	Normalise(lar->vec);
	lar->co[0]= mat[3][0];
	lar->co[1]= mat[3][1];
	lar->co[2]= mat[3][2];
	lar->dist= la->dist;
	lar->haint= la->haint;
	lar->distkw= lar->dist*lar->dist;
	lar->r= lar->energy*la->r;
	lar->g= lar->energy*la->g;
	lar->b= lar->energy*la->b;
	lar->k= la->k;

	// area
	lar->ray_samp= la->ray_samp;
	lar->ray_sampy= la->ray_sampy;
	lar->ray_sampz= la->ray_sampz;

	lar->area_size= la->area_size;
	lar->area_sizey= la->area_sizey;
	lar->area_sizez= la->area_sizez;

	lar->area_shape= la->area_shape;
	lar->ray_samp_type= la->ray_samp_type;

	if(lar->type==LA_AREA) {
		switch(lar->area_shape) {
		case LA_AREA_SQUARE:
			lar->ray_totsamp= lar->ray_samp*lar->ray_samp;
			lar->ray_sampy= lar->ray_samp;
			lar->area_sizey= lar->area_size;
			break;
		case LA_AREA_RECT:
			lar->ray_totsamp= lar->ray_samp*lar->ray_sampy;
			break;
		case LA_AREA_CUBE:
			lar->ray_totsamp= lar->ray_samp*lar->ray_samp*lar->ray_samp;
			lar->ray_sampy= lar->ray_samp;
			lar->ray_sampz= lar->ray_samp;
			lar->area_sizey= lar->area_size;
			lar->area_sizez= lar->area_size;
			break;
		case LA_AREA_BOX:
			lar->ray_totsamp= lar->ray_samp*lar->ray_sampy*lar->ray_sampz;
			break;
		}

		area_lamp_vectors(lar);
	}
	else lar->ray_totsamp= 0;

	lar->spotsi= la->spotsize;
	if(lar->mode & LA_HALO) {
		if(lar->spotsi>170.0) lar->spotsi= 170.0;
	}
	lar->spotsi= cos( M_PI*lar->spotsi/360.0 );
	lar->spotbl= (1.0-lar->spotsi)*la->spotblend;

	memcpy(lar->mtex, la->mtex, 8*4);

	lar->lay= ob->lay;

	lar->ld1= la->att1;
	lar->ld2= la->att2;

	if(lar->type==LA_SPOT) {

		Normalise(lar->imat[0]);
		Normalise(lar->imat[1]);
		Normalise(lar->imat[2]);

		xn= saacos(lar->spotsi);
		xn= sin(xn)/cos(xn);
		lar->spottexfac= 1.0/(xn);

		if(lar->mode & LA_ONLYSHADOW) {
			if((lar->mode & LA_SHAD)==0) lar->mode -= LA_ONLYSHADOW;
		}

	}

	/* set flag for spothalo en initvars */
	if(la->type==LA_SPOT && (la->mode & LA_HALO)) {
		if(la->haint>0.0) {
			R.flag |= R_LAMPHALO;

			/* camera position (0,0,0) rotate around lamp */
			lar->sh_invcampos[0]= -lar->co[0];
			lar->sh_invcampos[1]= -lar->co[1];
			lar->sh_invcampos[2]= -lar->co[2];
			MTC_Mat3MulVecfl(lar->imat, lar->sh_invcampos);

			/* z factor, for a normalized volume */
			hoek= saacos(lar->spotsi);
			xn= lar->spotsi;
			yn= sin(hoek);
			lar->sh_zfac= yn/xn;
			/* pre-scale */
			lar->sh_invcampos[2]*= lar->sh_zfac;

		}
	}

	for(c=0; c<6; c++) {
		if(la->mtex[c] && la->mtex[c]->tex) {
			lar->mode |= LA_TEXTURE;

			if(R.flag & R_RENDERING) {
				if(R.osa) {
					if(la->mtex[c]->tex->type==TEX_IMAGE) lar->mode |= LA_OSATEX;
				}
			}
		}
	}

	/* yafray: shadowbuffers only needed for internal render */
	if (R.r.renderer==R_INTERN)
	{
		if( (R.r.mode & R_SHADOW) && (lar->mode & LA_SHAD) && (la->type==LA_SPOT) && doshadbuf ) {
		/* Per lamp, one shadow buffer is made. */
			RE_initshadowbuf(lar, ob->obmat);
		}
	}
	
	/* yafray: shadow flag should not be cleared, only used with internal renderer */
	if (R.r.renderer==R_INTERN) {
		/* to make sure we can check ray shadow easily in the render code */
		if(lar->mode & LA_SHAD_RAY) {
			if( (R.r.mode & R_RAYTRACE)==0)
				lar->mode &= ~LA_SHAD_RAY;
		}
	}

	lar->org= MEM_dupallocN(lar);

}

/* ------------------------------------------------------------------------- */
static void init_render_surf(Object *ob)
{
	Nurb *nu=0;
	Curve *cu;
	ListBase displist;
	DispList *dl;
	VertRen *ver, *v1, *v2, *v3, *v4;
	VlakRen *vlr;
	Material *matar[32];
	float *data, *fp, *orco, n1[3], flen, mat[4][4];
	int len, a, need_orco=0, startvlak, startvert, p1, p2, p3, p4;
#ifdef STRUBI
	int u, v;
	int sizeu, sizev;
	VlakRen *vlr1, *vlr2, *vlr3;
	float n2[3], vn[3];
	int index;
#endif

	cu= ob->data;
	nu= cu->nurb.first;
	if(nu==0) return;

	/* yafray: set transform to identity matrix */
	if (R.r.renderer==R_YAFRAY)
		MTC_Mat4One(mat);
	else
		MTC_Mat4MulMat4(mat, ob->obmat, R.viewmat);
	MTC_Mat4Invert(ob->imat, mat);

	/* material array */
	memset(matar, 0, 4*32);
	matar[0]= &defmaterial;
	for(a=0; a<ob->totcol; a++) {
		matar[a]= give_render_material(ob, a+1);
		if(matar[a]==0) matar[a]= &defmaterial;
		if(matar[a] && matar[a]->ren->texco & TEXCO_ORCO) {
			need_orco= 1;
		}
	}

	if(ob->parent && (ob->parent->type==OB_IKA || ob->parent->type==OB_LATTICE)) need_orco= 1;

	if(cu->orco==0 && need_orco) make_orco_surf(cu);
	orco= cu->orco;

	/* make a complete new displist, the base-displist can be different */
	displist.first= displist.last= 0;
	nu= cu->nurb.first;
	while(nu) {
		if(nu->pntsv>1) {
	//		if (dl->flag & DL_CYCLIC_V) {
			len= nu->resolu*nu->resolv;
			/* makeNurbfaces wants zeros */

			dl= MEM_callocN(sizeof(DispList)+len*3*sizeof(float), "makeDispList1");
			dl->verts= MEM_callocN(len*3*sizeof(float), "makeDispList01");
			BLI_addtail(&displist, dl);

			dl->parts= nu->resolu;	/* switched order, makeNurbfaces works that way... */
			dl->nr= nu->resolv;
			dl->col= nu->mat_nr;
			dl->rt= nu->flag;

			data= dl->verts;
			dl->type= DL_SURF;
			/* if nurbs cyclic (u/v) set flags in displist accordingly */
			if(nu->flagv & 1) dl->flag |= DL_CYCLIC_V;	
			if(nu->flagu & 1) dl->flag |= DL_CYCLIC_U;

			makeNurbfaces(nu, data);
		}
		nu= nu->next;
	}

	if(ob->parent && ob->parent->type==OB_LATTICE) {
		init_latt_deform(ob->parent, ob);
		dl= displist.first;
		while(dl) {

			fp= dl->verts;
			len= dl->nr*dl->parts;
			for(a=0; a<len; a++, fp+=3)  calc_latt_deform(fp);

			dl= dl->next;
		}
		end_latt_deform();
	}
	
#ifdef __NLA
	if(ob->parent && ob->parent->type==OB_ARMATURE) {
/*  		bArmature *arm= ob->parent->data; */
		init_armature_deform(ob->parent, ob);
		dl= displist.first;
		while(dl) {

			fp= dl->verts;
			len= dl->nr*dl->parts;
			for(a=0; a<len; a++, fp+=3)
				calc_armature_deform(ob->parent, fp, a);

			dl= dl->next;
		}
	}
#endif

	if(ob->parent && ob->parent->type==OB_IKA) {
		Ika *ika= ob->parent->data;
		
		init_skel_deform(ob->parent, ob);
		dl= displist.first;
		while(dl) {

			fp= dl->verts;
			len= dl->nr*dl->parts;
			for(a=0; a<len; a++, fp+=3)  calc_skel_deform(ika, fp);

			dl= dl->next;
		}
	}

	dl= displist.first;
	/* walk along displaylist and create rendervertices/-faces */
	while(dl) {
#ifdef STRUBI
/* watch out: u ^= y, v ^= x !! */
		if(dl->type==DL_SURF) {
			startvert= R.totvert;
			sizeu = dl->parts; sizev = dl->nr; 

			data= dl->verts;
			for (u = 0; u < sizeu; u++) {
				v1 = RE_findOrAddVert(R.totvert++); /* save this for possible V wrapping */
				VECCOPY(v1->co, data); data += 3;
				if(orco) {
					v1->orco= orco; orco+= 3;
				}	
				MTC_Mat4MulVecfl(mat, v1->co);

				for (v = 1; v < sizev; v++) {
					ver= RE_findOrAddVert(R.totvert++);
					VECCOPY(ver->co, data); data += 3;
					if(orco) {
						ver->orco= orco; orco+= 3;
					}	
					MTC_Mat4MulVecfl(mat, ver->co);
				}
				/* if V-cyclic, add extra vertices at end of the row */
				if (dl->flag & DL_CYCLIC_V) {
					ver= RE_findOrAddVert(R.totvert++);
					VECCOPY(ver->co, v1->co);
					ver->orco= orco;
					orco+= 3;
				}	
			}	

			if (dl->flag & DL_CYCLIC_V)  sizev++; /* adapt U dimension */


			/* if U cyclic, add extra row at end of column */
			if (dl->flag & DL_CYCLIC_U) {
				for (v = 0; v < sizev; v++) {
					v1= RE_findOrAddVert(startvert + v);
					ver= RE_findOrAddVert(R.totvert++);
					VECCOPY(ver->co, v1->co);
					ver->orco= orco;
					orco +=3;

				}
				sizeu++;
			}	
					



			startvlak= R.totvlak;

			/* process generic surface */
			for(u = 0; u < sizeu - 1; u++) {

/*				DL_SURFINDEX(dl->flag & DL_CYCLIC_U, dl->flag & DL_CYCLIC_V, dl->nr, dl->parts);
				DL_SURFINDEX(0, 0, dl->nr, dl->parts);
*/


/*
				
			^	()----p4----p3----()
			|	|     |     |     |
			u	|     |     |     |
				|     |     |     |
				()----p1----p2----()
				       v ->
*/

				p1 = startvert + u * sizev; /* walk through face list */
				p2 = p1 + 1;
				p3 = p2 + sizev;
				p4 = p3 - 1;


				for(v = 0; v < sizev - 1; v++) {
					v1= RE_findOrAddVert(p1);
					v2= RE_findOrAddVert(p2);
					v3= RE_findOrAddVert(p3);
					v4= RE_findOrAddVert(p4);

					flen= CalcNormFloat4(v1->co, v2->co, v3->co, v4->co, n1);
/* flen can be 0 if there are double nurbs control vertices 
	so zero area faces can be generated
	->> there is at the moment no proper way to fix this except
	generating empty render faces */

//					if(flen!=0.0) {
						vlr= RE_findOrAddVlak(R.totvlak++);
						vlr->ob= ob;
						vlr->v1= v1; vlr->v2= v2; vlr->v3= v3; vlr->v4= v4;
						VECCOPY(vlr->n, n1);
						vlr->len= flen;
						vlr->lay= ob->lay;
						vlr->mat= matar[ dl->col];
						vlr->ec= ME_V1V2+ME_V2V3;
						vlr->flag= dl->rt;
						if( (cu->flag & CU_NOPUNOFLIP) || (vlr->mat->mode & MA_RAYTRANSP)) {
							vlr->flag |= R_NOPUNOFLIP;
							vlr->puno= 15;
						}
//					}

					VecAddf(v1->n, v1->n, n1);
					VecAddf(v2->n, v2->n, n1);
					VecAddf(v3->n, v3->n, n1);
					VecAddf(v4->n, v4->n, n1);

					p1++; p2++; p3++; p4++;
				}
			}	
			/* fix normals for U resp. V cyclic faces */
			sizeu--; sizev--;  /* dec size for face array */
			if (dl->flag & DL_CYCLIC_U) {

				for (v = 0; v < sizev; v++)
				{
					/* optimize! :*/
					index = startvlak + v;
					// vlr= RE_findOrAddVlak(index + (sizeu-1) * sizev);
					vlr= RE_findOrAddVlak(UVTOINDEX(sizeu - 1, v));
					GETNORMAL(vlr, n1);
					vlr1= RE_findOrAddVlak(UVTOINDEX(0, v));
					GETNORMAL(vlr1, n2);
					VecAddf(vlr1->v1->n, vlr1->v1->n, n1);
					VecAddf(vlr1->v2->n, vlr1->v2->n, n1);
					VecAddf(vlr->v3->n, vlr->v3->n, n2);
					VecAddf(vlr->v4->n, vlr->v4->n, n2);
				}
			}
			if (dl->flag & DL_CYCLIC_V) {

				for (u = 0; u < sizeu; u++)
				{
					/* optimize! :*/
					index = startvlak + u * sizev;
					//vlr= RE_findOrAddVlak(index);
					vlr= RE_findOrAddVlak(UVTOINDEX(u, 0));
					GETNORMAL(vlr, n1);
					vlr1= RE_findOrAddVlak(UVTOINDEX(u, sizev-1));
					// vlr1= RE_findOrAddVlak(index + (sizev - 1));
					GETNORMAL(vlr1, n2);
					VecAddf(vlr1->v2->n, vlr1->v2->n, n1);
					VecAddf(vlr1->v3->n, vlr1->v3->n, n1);
					VecAddf(vlr->v1->n, vlr->v1->n, n2);
					VecAddf(vlr->v4->n, vlr->v4->n, n2);
				}
			}
			/* last vertex is an extra case: 

			^	()----()----()----()
			|	|     |     ||     |
			u	|     |(0,n)||(0,0)|
				|     |     ||     |
			 	()====()====[]====()
			 	|     |     ||     |
			 	|     |(m,n)||(m,0)|
				|     |     ||     |
				()----()----()----()
				       v ->

			vertex [] is no longer shared, therefore distribute
			normals of the surrounding faces to all of the duplicates of []
			*/

			if (dl->flag & DL_CYCLIC_U && dl->flag & DL_CYCLIC_V)
			{
				vlr= RE_findOrAddVlak(UVTOINDEX(sizeu - 1, sizev - 1)); /* (m,n) */
				GETNORMAL(vlr, n1);
				vlr1= RE_findOrAddVlak(UVTOINDEX(0,0));  /* (0,0) */
				GETNORMAL(vlr1, vn);
				VecAddf(vn, vn, n1);
				vlr2= RE_findOrAddVlak(UVTOINDEX(0, sizev-1)); /* (0,n) */
				GETNORMAL(vlr2, n1);
				VecAddf(vn, vn, n1);
				vlr3= RE_findOrAddVlak(UVTOINDEX(sizeu-1, 0)); /* (m,0) */
				GETNORMAL(vlr3, n1);
				VecAddf(vn, vn, n1);
				VECCOPY(vlr->v3->n, vn);
				VECCOPY(vlr1->v1->n, vn);
				VECCOPY(vlr2->v2->n, vn);
				VECCOPY(vlr3->v4->n, vn);
			}
			for(a = startvert; a < R.totvert; a++) {
				ver= RE_findOrAddVert(a);
				Normalise(ver->n);
			}


		}
#else

		if(dl->type==DL_SURF) {
			startvert= R.totvert;
			a= dl->nr*dl->parts;
			data= dl->verts;
			while(a--) {
				ver= RE_findOrAddVert(R.totvert++);
				VECCOPY(ver->co, data);
				if(orco) {
					ver->orco= orco;
					orco+= 3;
				}
				MTC_Mat4MulVecfl(mat, ver->co);
				data+= 3;
			}

			startvlak= R.totvlak;

			for(a=0; a<dl->parts; a++) {

				DL_SURFINDEX(dl->flag & DL_CYCLIC_V, dl->flag & DL_CYCLIC_U, dl->nr, dl->parts);
				p1+= startvert;
				p2+= startvert;
				p3+= startvert;
				p4+= startvert;

				for(; b<dl->nr; b++) {
					v1= RE_findOrAddVert(p1);
					v2= RE_findOrAddVert(p2);
					v3= RE_findOrAddVert(p3);
					v4= RE_findOrAddVert(p4);

					flen= CalcNormFloat4(v1->co, v3->co, v4->co, v2->co, n1);
					if(flen!=0.0) {
						vlr= RE_findOrAddVlak(R.totvlak++);
						vlr->ob= ob;
						vlr->v1= v1;
						vlr->v2= v3;
						vlr->v3= v4;
						vlr->v4= v2;
						VECCOPY(vlr->n, n1);
						vlr->len= flen;
						vlr->lay= ob->lay;
						vlr->mat= matar[ dl->col];
						vlr->ec= ME_V1V2+ME_V2V3;
						vlr->flag= dl->rt;
						if( (cu->flag & CU_NOPUNOFLIP) || (vlr->mat->mode & MA_RAYTRANSP)) {
							vlr->flag |= R_NOPUNOFLIP;
							vlr->puno= 15;
						}
					}

					VecAddf(v1->n, v1->n, n1);
					VecAddf(v2->n, v2->n, n1);
					VecAddf(v3->n, v3->n, n1);
					VecAddf(v4->n, v4->n, n1);

					p4= p3;
					p3++;
					p2= p1;
					p1++;
				}
			}

			for(a=startvert; a<R.totvert; a++) {
				ver= RE_findOrAddVert(a);
				Normalise(ver->n);
			}


		}
#endif
		dl= dl->next;
	}
	freedisplist(&displist);
}

static void init_render_curve(Object *ob)
{
	Ika *ika=0;

	Lattice *lt=0;
	Curve *cu;
	VertRen *ver;
	VlakRen *vlr;
	ListBase dlbev;
	Nurb *nu=0;
	DispList *dlb, *dl;
	BevList *bl;
	BevPoint *bevp;
	Material *matar[32];
	float len, *data, *fp, *fp1, fac;
	float n[3], vec[3], widfac, size[3], mat[4][4];
	int nr, startvert, startvlak, a, b, p1, p2, p3, p4;
	int totvert, frontside, need_orco=0, firststartvert, *index;

	cu= ob->data;
	nu= cu->nurb.first;
	if(nu==0) return;

	/* test displist */
	if(cu->disp.first==0) makeDispList(ob);
	dl= cu->disp.first;
	if(cu->disp.first==0) return;

	if(dl->type!=DL_INDEX3) {
		curve_to_filledpoly(cu, &cu->disp);
	}

	if(cu->bev.first==0) makeBevelList(ob);

	firststartvert= R.totvert;

	/* yafray: set transform to identity matrix */
	if (R.r.renderer==R_YAFRAY)
		MTC_Mat4One(mat);
	else
		MTC_Mat4MulMat4(mat, ob->obmat, R.viewmat);
	MTC_Mat4Invert(ob->imat, mat);

	/* material array */
	memset(matar, 0, 4*32);
	matar[0]= &defmaterial;
	for(a=0; a<ob->totcol; a++) {
		matar[a]= give_render_material(ob, a+1);
		if(matar[a]==0) matar[a]= &defmaterial;
		if(matar[a]->ren->texco & TEXCO_ORCO) {
			need_orco= 1;
		}
	}

	/* bevelcurve in displist */
	dlbev.first= dlbev.last= 0;

	if(cu->ext1!=0.0 || cu->ext2!=0.0 || cu->bevobj!=0) {
		makebevelcurve(ob, &dlbev);
	}

	/* uv orcos? count amount of points and malloc */
	if(need_orco && (cu->flag & CU_UV_ORCO)) {
		if(cu->flag & CU_PATH);
		else {
			totvert= 0;
			bl= cu->bev.first;
			while(bl) {
				dlb= dlbev.first;
				while(dlb) {
					totvert+= dlb->nr*bl->nr;
					dlb= dlb->next;
				}
				bl= bl->next;
			}

			if(totvert) {
				fp= cu->orco= MEM_mallocN(3*sizeof(float)*totvert, "cu->orco");
	
				bl= cu->bev.first;
				while(bl) {
					dlb= dlbev.first;
					while(dlb) {
						for(b=0; b<dlb->nr; b++) {
							fac= (2.0*b/(float)(dlb->nr-1)) - 1.0;
							for(a=0; a<bl->nr; a++, fp+=3) {
								fp[0]= (2.0*a/(float)(bl->nr-1)) - 1.0;
								fp[1]= fac;
								fp[2]= 0.0;
							}
						}
						dlb= dlb->next;
					}
					bl= bl->next;
				}
			}
		}
	}

	if(ob->parent && ob->parent->type==OB_LATTICE) {
		lt= ob->parent->data;
		init_latt_deform(ob->parent, ob);
		need_orco= 1;
	}
	
	if(ob->parent && ob->parent->type==OB_IKA) {
		ika= ob->parent->data;
		init_skel_deform(ob->parent, ob);
		need_orco= 1;
	}

	if(ob->parent && ob->parent->type==OB_ARMATURE) {
		init_armature_deform(ob->parent, ob);
		need_orco= 1;
	}

	/* do keypos? NOTE: watch it : orcos */

	/* effect on text? */

	/* boundboxclip still todo */

	/* side faces of poly:  work with bevellist */
	widfac= (cu->width-1.0);

	bl= cu->bev.first;
	nu= cu->nurb.first;
	while(bl) {

		if(dlbev.first) {    /* otherwise just a poly */

			dlb= dlbev.first;   /* bevel loop */
			while(dlb) {
				data= MEM_mallocN(3*sizeof(float)*dlb->nr*bl->nr, "init_render_curve3");
				fp= data;

				/* for each point at bevelcurve do the entire poly */
				fp1= dlb->verts;
				b= dlb->nr;
				while(b--) {

					bevp= (BevPoint *)(bl+1);
					a= bl->nr;
					while(a--) {

						if(cu->flag & CU_3D) {
							vec[0]= fp1[1]+widfac;
							vec[1]= fp1[2];
							vec[2]= 0.0;

							MTC_Mat3MulVecfl(bevp->mat, vec);

							fp[0]= bevp->x+ vec[0];
							fp[1]= bevp->y+ vec[1];
							fp[2]= bevp->z+ vec[2];
						}
						else {

							fp[0]= bevp->x+ (widfac+fp1[1])*bevp->sina;
							fp[1]= bevp->y+ (widfac+fp1[1])*bevp->cosa;
							fp[2]= bevp->z+ fp1[2];
							/* do not MatMul here: polyfill should work uniform, independent which frame */
						}
						fp+= 3;
						bevp++;
					}
					fp1+=3;
				}

				/* make render vertices */
				fp= data;
				startvert= R.totvert;
				nr= dlb->nr*bl->nr;

				while(nr--) {
					ver= RE_findOrAddVert(R.totvert++);
					
					if(lt) calc_latt_deform(fp);
					else if(ika) calc_skel_deform(ika, fp);
					
					VECCOPY(ver->co, fp);
					MTC_Mat4MulVecfl(mat, ver->co);
					fp+= 3;
				}

				startvlak= R.totvlak;

				for(a=0; a<dlb->nr; a++) {

					frontside= (a >= dlb->nr/2);

					DL_SURFINDEX(bl->poly>0, dlb->type==DL_POLY, bl->nr, dlb->nr);
					p1+= startvert;
					p2+= startvert;
					p3+= startvert;
					p4+= startvert;

					for(; b<bl->nr; b++) {

						vlr= RE_findOrAddVlak(R.totvlak++);
						vlr->ob= ob;
						vlr->v1= RE_findOrAddVert(p2);
						vlr->v2= RE_findOrAddVert(p1);
						vlr->v3= RE_findOrAddVert(p3);
						vlr->v4= RE_findOrAddVert(p4);
						vlr->ec= ME_V2V3+ME_V3V4;
						if(a==0) vlr->ec+= ME_V1V2;

						vlr->flag= nu->flag;
						vlr->lay= ob->lay;

						/* this is not really scientific: the vertices
						 * 2, 3 en 4 seem to give better vertexnormals than 1 2 3:
						 * front and backside treated different!!
						 */

						if(frontside)
							vlr->len= CalcNormFloat(vlr->v2->co, vlr->v3->co, vlr->v4->co, vlr->n);
						else 
							vlr->len= CalcNormFloat(vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->n);

						vlr->mat= matar[ nu->mat_nr ];

						p4= p3;
						p3++;
						p2= p1;
						p1++;

					}

				}

				/* make double points: SPLIT POLY */
				if(dlb->nr==4 && cu->bevobj==0) {
					split_u_renderfaces(startvlak, startvert, bl->nr, 1, bl->poly>0);
					split_u_renderfaces(startvlak, startvert, bl->nr, 2, bl->poly>0);
				}
				/* make double points: SPLIT BEVELS */
				bevp= (BevPoint *)(bl+1);
				for(a=0; a<bl->nr; a++) {
					if(bevp->f1)
						split_v_renderfaces(startvlak, startvert, bl->nr, dlb->nr, a, bl->poly>0,
						    dlb->type==DL_POLY);
					bevp++;
				}

				/* vertex normals */
				for(a= startvlak; a<R.totvlak; a++) {
					vlr= RE_findOrAddVlak(a);

					VecAddf(vlr->v1->n, vlr->v1->n, vlr->n);
					VecAddf(vlr->v3->n, vlr->v3->n, vlr->n);
					VecAddf(vlr->v2->n, vlr->v2->n, vlr->n);
					VecAddf(vlr->v4->n, vlr->v4->n, vlr->n);
				}
				for(a=startvert; a<R.totvert; a++) {
					ver= RE_findOrAddVert(a);
					len= Normalise(ver->n);
					if(len==0.0) ver->sticky= (float *)1;
					else ver->sticky= 0;
				}
				for(a= startvlak; a<R.totvlak; a++) {
					vlr= RE_findOrAddVlak(a);
					if(vlr->v1->sticky) VECCOPY(vlr->v1->n, vlr->n);
					if(vlr->v2->sticky) VECCOPY(vlr->v2->n, vlr->n);
					if(vlr->v3->sticky) VECCOPY(vlr->v3->n, vlr->n);
					if(vlr->v4->sticky) VECCOPY(vlr->v4->n, vlr->n);
				}

				dlb= dlb->next;

				MEM_freeN(data);
			}

		}
		bl= bl->next;
		nu= nu->next;
	}

	if(dlbev.first) {
		freedisplist(&dlbev);
	}

	if(cu->flag & CU_PATH) return;

	/* from displist the filled faces can be extracted */
	dl= cu->disp.first;

	while(dl) {
		if(dl->type==DL_INDEX3) {

			startvert= R.totvert;
			data= dl->verts;

			n[0]= ob->imat[0][2];
			n[1]= ob->imat[1][2];
			n[2]= ob->imat[2][2];
			Normalise(n);

			for(a=0; a<dl->nr; a++, data+=3) {

				ver= RE_findOrAddVert(R.totvert++);
				VECCOPY(ver->co, data);
				MTC_Mat4MulVecfl(mat, ver->co);

				VECCOPY(ver->n, n);
			}

			startvlak= R.totvlak;
			index= dl->index;
			for(a=0; a<dl->parts; a++, index+=3) {

				vlr= RE_findOrAddVlak(R.totvlak++);
				vlr->v1= RE_findOrAddVert(startvert+index[0]);
				vlr->v2= RE_findOrAddVert(startvert+index[1]);
				vlr->v3= RE_findOrAddVert(startvert+index[2]);
				vlr->v4= 0;

				VECCOPY(vlr->n, n);

				vlr->mat= matar[ dl->col ];
				vlr->puno= 0;
				vlr->flag= 0;
				vlr->ec= 0;
				vlr->lay= ob->lay;
			}

		}
		dl= dl->next;
	}

	if(lt) {
		end_latt_deform();
	}

	if(need_orco) {	/* the stupid way: should be replaced; taking account for keys! */

		VECCOPY(size, cu->size);

		nr= R.totvert-firststartvert;
		if(nr) {
			if(cu->orco) {
				fp= cu->orco;
				while(nr--) {
					ver= RE_findOrAddVert(firststartvert++);
					ver->orco= fp;
					fp+= 3;
				}
			}
			else {
				fp= cu->orco= MEM_mallocN(sizeof(float)*3*nr, "cu orco");
				while(nr--) {
					ver= RE_findOrAddVert(firststartvert++);
					ver->orco= fp;

					VECCOPY(fp, ver->co);
					MTC_Mat4MulVecfl(ob->imat, fp);

					fp[0]= (fp[0]-cu->loc[0])/size[0];
					fp[1]= (fp[1]-cu->loc[1])/size[1];
					fp[2]= (fp[2]-cu->loc[2])/size[2];
					fp+= 3;
				}
			}
		}
	}
}

static void init_render_object(Object *ob)
{
	float mat[4][4];
	int startface, startvert;
	
	startface=R.totvlak;
	startvert=R.totvert;

	ob->flag |= OB_DONE;

	if(ob->type==OB_LAMP)
		RE_add_render_lamp(ob, 1);
	else if ELEM(ob->type, OB_FONT, OB_CURVE)
		init_render_curve(ob);
	else if(ob->type==OB_SURF)
		init_render_surf(ob);
	else if(ob->type==OB_MESH)
		init_render_mesh(ob);
	else if(ob->type==OB_MBALL)
		init_render_mball(ob);
	else {
		/* yafray: set transform to identity matrix */
		if (R.r.renderer==R_YAFRAY)
			MTC_Mat4One(mat);
		else
			MTC_Mat4MulMat4(mat, ob->obmat, R.viewmat);
		MTC_Mat4Invert(ob->imat, mat);
	}
	
	if (test_for_displace( ob ) ) 
		do_displacement(ob, startface, R.totvlak-startface, startvert, R.totvert-startvert);
}

void RE_freeRotateBlenderScene(void)
{
	ShadBuf *shb;
	Object *ob = NULL;
	Mesh *me;
	Curve *cu;
	DispList *dl;
	unsigned long *ztile;
	int a, b, v;
	char *ctile;

	/* FREE */

	for(a=0; a<R.totlamp; a++) {
		if(R.la[a]->shb) {
			shb= R.la[a]->shb;
			v= (shb->size*shb->size)/256;
			ztile= shb->zbuf;
			ctile= shb->cbuf;
			for(b=0; b<v; b++, ztile++, ctile++) {
				if(*ctile) MEM_freeN((void *) *ztile);
			}
			
			MEM_freeN(shb->zbuf);
			MEM_freeN(shb->cbuf);
			MEM_freeN(R.la[a]->shb);
		}
		if(R.la[a]->org) MEM_freeN(R.la[a]->org);
		if(R.la[a]->jitter) MEM_freeN(R.la[a]->jitter);
		MEM_freeN(R.la[a]);
	}
	a=0;
	while(R.blove[a]) {
		MEM_freeN(R.blove[a]);
		R.blove[a]=0;
		a++;
	}
	a=0;
	while(R.blovl[a]) {
		MEM_freeN(R.blovl[a]);
		R.blovl[a]=0;
		a++;
	}
	a=0;
	while(R.bloha[a]) {
		MEM_freeN(R.bloha[a]);
		R.bloha[a]=0;
		a++;
	}

	/* free orco. check all obejcts because of duplis and sets */
	ob= G.main->object.first;
	while(ob) {

		if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
			cu= ob->data;
			if(cu->orco) {
				MEM_freeN(cu->orco);
				cu->orco= 0;
			}
		}
		else if(ob->type==OB_MESH) {
			me= ob->data;
			if(me->orco) {
				MEM_freeN(me->orco);
				me->orco= 0;
			}
			if (mesh_uses_displist(me) && (me->subdiv!=me->subdivr)) {
				makeDispList(ob);  /* XXX this should be replaced with proper caching */
				me->subdivdone= 0;	/* needed to prevent multiple used meshes being recalculated */
			}
		}
		else if(ob->type==OB_MBALL) {
			if(ob->disp.first && ob->disp.first!=ob->disp.last) {
				dl= ob->disp.first;
				BLI_remlink(&ob->disp, dl);
				freedisplist(&ob->disp);
				BLI_addtail(&ob->disp, dl);
			}
		}
		ob= ob->id.next;
	}

	end_render_textures();
	end_render_materials();
	end_radio_render();
	
	R.totvlak=R.totvert=R.totlamp=R.tothalo= 0;
}


static void check_non_flat_quads(void)
{
	VlakRen *vlr, *vlr1;
	float nor[3], xn;
	int a, flipnorm;

	for(a=R.totvlak-1; a>=0; a--) {
		vlr= RE_findOrAddVlak(a);
		
		/* Face is divided along edge with the least gradient 		*/
		/* Flagged with R_DIVIDE_24 if divide is from vert 2 to 4 	*/
		/* 		4---3		4---3 */
		/*		|\ 1|	or  |1 /| */
		/*		|0\ |		|/ 0| */
		/*		1---2		1---2 	0 = orig face, 1 = new face */
		
		/* test if rendering as a quad or triangle */
		if(vlr->v4) {

			if(vlr->mat->mode & MA_WIRE);
			else {
				
				/* blahj, render normals are inverted in render */
				CalcNormFloat4(vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co, nor);
				
				xn= nor[0]*vlr->n[0] + nor[1]*vlr->n[1] + nor[2]*vlr->n[2];
				if( fabs(xn) < 0.990 ) {
				
					if( xn<0.0 ) flipnorm= 1; else flipnorm= 0;
					
					/* recalc this nor, previous calc was with calcnormfloat4 */
					if(flipnorm) {
						if (vlr->flag & R_DIVIDE_24) CalcNormFloat(vlr->v1->co, vlr->v2->co, vlr->v4->co, vlr->n);
						else CalcNormFloat(vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->n);
					}
					else {
						if (vlr->flag & R_DIVIDE_24) CalcNormFloat(vlr->v4->co, vlr->v2->co, vlr->v1->co, vlr->n);
						else CalcNormFloat(vlr->v3->co, vlr->v2->co, vlr->v1->co, vlr->n);
					}
					
					vlr1= RE_findOrAddVlak(R.totvlak++);
					*vlr1= *vlr;
					vlr1->flag |= R_FACE_SPLIT;
					
					if(flipnorm) VecMulf(nor, -1.0);
					VECCOPY(vlr1->n, nor);
										
					if (vlr->flag&R_DIVIDE_24) {
						vlr1->v1=vlr->v2;
						vlr1->v2=vlr->v3;
						vlr1->v3=vlr->v4;
						vlr->v3 =vlr->v4;
						
						vlr1->flag |= R_DIVIDE_24;	
					}
					else {
						vlr1->v1= vlr->v1;
						vlr1->v2= vlr->v3;
						vlr1->v3= vlr->v4;
						
						vlr1->flag &= ~R_DIVIDE_24;
					}
					
					vlr->v4 = vlr1->v4 = 0;
				/* so later UV can be pulled from original tface, look for R_DIVIDE_24 for direction */
					vlr1->tface=vlr->tface; 
					
					vlr1->puno= 0;
					if(vlr->puno & ME_FLIPV1) vlr1->puno |= ME_FLIPV1;
					if(vlr->puno & ME_FLIPV3) vlr1->puno |= ME_FLIPV2;
					if(vlr->puno & ME_FLIPV4) vlr1->puno |= ME_FLIPV3;
		
				}
				/* clear the flag when not divided */
				else vlr->flag &= ~R_DIVIDE_24;
			}
		}
	}
}



extern int slurph_opt;	/* key.c */
extern ListBase duplilist;
void RE_rotateBlenderScene(void)
{
	Base *base;
	Object *ob, *obd;
	Scene *sce;
	unsigned int lay;
	float mat[4][4];

	if(G.scene->camera==0) return;

	O.dxwin[0]= 0.5/(float)R.r.xsch;
	O.dywin[1]= 0.5/(float)R.r.ysch;

	slurph_opt= 0;

	R.totvlak=R.totvert=R.totlamp=R.tothalo= 0;

	do_all_ipos();
	BPY_do_all_scripts(SCRIPT_FRAMECHANGED);
	do_all_keys();
#ifdef __NLA
	do_all_actions();
	rebuild_all_armature_displists();
#endif
	do_all_ikas();
	test_all_displists();

	/* not really neat forcing of calc_ipo and where_is */
	ob= G.main->object.first;
	while(ob) {
		ob->ctime= -123.456;
		ob= ob->id.next;
	}

	/* because of optimal calculation tracking/lattices/etc: and extra where_is_ob here */

	base= G.scene->base.first;
	while(base) {
		clear_object_constraint_status(base->object);
		if (base->object->type==OB_ARMATURE) {
			where_is_armature (base->object);
		}
		else

		where_is_object(base->object);

		if(base->next==0 && G.scene->set && base==G.scene->base.last) base= G.scene->set->base.first;
		else base= base->next;
	}

	MTC_Mat4CpyMat4(R.viewinv, G.scene->camera->obmat);
	MTC_Mat4Ortho(R.viewinv);
	MTC_Mat4Invert(R.viewmat, R.viewinv);

	/* not so neat: now the viewinv is not equal to viewmat. used for Texcos and such. Improve! */
	if(R.r.mode & R_ORTHO) R.viewmat[3][2]*= 100.0;

	RE_setwindowclip(1,-1); /*  no jit:(-1) */

	/* clear imat flags */
	ob= G.main->object.first;
	while(ob) {
		ob->flag &= ~OB_DO_IMAT;
		ob= ob->id.next;
	}

	init_render_world();	/* do first, because of ambient */
	init_render_textures();
	init_render_materials();

	/* MAKE RENDER DATA */

	/* each object should only be rendered once */
	ob= G.main->object.first;
	while(ob) {
		ob->flag &= ~OB_DONE;
		ob= ob->id.next;
	}

	/* layers: render in foreground current 3D window */
	lay= G.scene->lay;
	if(G.vd) lay= G.vd->lay;
	sce= G.scene;

	base= G.scene->base.first;
	while(base) {

		ob= base->object;

		if(ob->flag & OB_DONE) {
			/* yafray: this object needs to be included in renderlist for duplivert instancing.
				 This only works for dupliverts, dupliframes handled below.
				 This is based on the assumption that OB_DONE is only set for duplivert objects,
				 before scene conversion, there are no other flags set to indicate it's use as far as I know...
				 NOT done for lamps, these are included as separate objects, see below.
				 correction: also ignore lattices, armatures and camera's (.....) */
			if ((ob->type!=OB_LATTICE) && (ob->type!=OB_ARMATURE) &&
					(ob->type!=OB_LAMP) && (ob->type!=OB_CAMERA) && (R.r.renderer==R_YAFRAY))
			{
				printf("Adding %s to renderlist\n", ob->id.name);
				ob->flag &= ~OB_DONE;
				init_render_object(ob);
				ob->flag |= OB_DONE;
			}
		}
		else {
			where_is_object(ob);

			if( (base->lay & lay) || (ob->type==OB_LAMP && (base->lay & G.scene->lay)) ) {

				if(ob->transflag & OB_DUPLI) {
					/* exception: mballs! */
					/* yafray: Include at least one copy of a dupliframe object for yafray in the renderlist.
					   mballs comment above true as well for yafray, they are not included, only all other object types */
					if (R.r.renderer==R_YAFRAY) {
						if ((ob->type!=OB_MBALL) && ((ob->transflag & OB_DUPLIFRAMES)!=0)) {
							printf("Object %s has OB_DUPLIFRAMES set, adding to renderlist\n", ob->id.name);
							init_render_object(ob);
						}
					}
					make_duplilist(sce, ob);
					if(ob->type==OB_MBALL) {
						init_render_object(ob);
					}
					else {
						obd= duplilist.first;
						if(obd) {
							/* exception, in background render it doesnt make the displist */
							if ELEM(obd->type, OB_CURVE, OB_SURF) {
								Curve *cu;

								cu= obd->data;
								if(cu->disp.first==0) {
									obd->flag &= ~OB_FROMDUPLI;
									makeDispList(obd);
									obd->flag |= OB_FROMDUPLI;
								}
							}
						}

						obd= duplilist.first;
						while(obd) {
							if(obd->type!=OB_MBALL) {
								/* yafray: special handling of duplivert objects for yafray:
								   only the matrix is stored, together with the source object name.
									 Since the original object is needed as well, it is included in the renderlist (see above)
									 NOT done for lamps, these need to be included as normal lamps separately
									 correction: also ignore lattices, armatures and cameras (....) */
								if ((obd->type!=OB_LATTICE) && (obd->type!=OB_ARMATURE) &&
										(obd->type!=OB_LAMP) && (obd->type!=OB_CAMERA) && (R.r.renderer==R_YAFRAY))
								{
									printf("Adding dupli matrix for object %s\n", obd->id.name);
									YAF_addDupliMtx(obd);
								}
								else init_render_object(obd);
							}
							obd= obd->id.next;
						}
					}
					free_duplilist();
				}
				else {
					/* yafray: if there are linked data objects (except lamps),
					   yafray only needs to know about one, the rest can be instanciated.
					   The dupliMtx list is used for this purpose */
					if (R.r.renderer==R_YAFRAY) {
						if ((ob->type!=OB_LAMP) && (YAF_objectKnownData(ob)))
							printf("Added dupli matrix for linked data object %s\n", ob->id.name);
						else
							init_render_object(ob);
					}
					else init_render_object(ob);
				}

			}
			else {
				/* yafray: set transform to identity matrix, not sure if this is needed here */
				if (R.r.renderer==R_YAFRAY)
					MTC_Mat4One(mat);
				else
					MTC_Mat4MulMat4(mat, ob->obmat, R.viewmat);
				MTC_Mat4Invert(ob->imat, mat);
			}

			ob->flag &= ~OB_DO_IMAT;
		}
		if(blender_test_break()) break;

		if(base->next==0 && G.scene->set && base==G.scene->base.last) {
			base= G.scene->set->base.first;
			sce= G.scene->set;
		}
		else base= base->next;

	}

	/* imat objects */
	ob= G.main->object.first;
	while(ob) {
		if(ob->flag & OB_DO_IMAT) {

			ob->flag &= ~OB_DO_IMAT;
			/* yafray: set transform to identity matrix, not sure if this is needed here */
			if (R.r.renderer==R_YAFRAY)
				MTC_Mat4One(mat);
			else
				MTC_Mat4MulMat4(mat, ob->obmat, R.viewmat);
			MTC_Mat4Invert(ob->imat, mat);
		}
		ob= ob->id.next;
	}

	sort_halos();

	if(R.wrld.mode & WO_STARS) RE_make_stars(NULL, NULL, NULL);

	slurph_opt= 1;

	if(blender_test_break()) return;

	check_non_flat_quads();
	set_normalflags();
}


/* **************************************************************** */
/*                Displacement mapping                              */
/* **************************************************************** */
short test_for_displace(Object *ob)
{
	/* return 1 when this object uses displacement textures. */
	Material *ma;
	int i;
	
	for (i=1; i<=ob->totcol; i++) {
		ma=give_render_material(ob, i);
		/* ma->mapto is ORed total of all mapto channels */
		if(ma && (ma->ren->mapto & MAP_DISPLACE)) return 1;
	}
	return 0;
}


void do_displacement(Object *ob, int startface, int numface, int startvert, int numvert )
{
	VertRen *vr;
	VlakRen *vlr;
	float min[3]={1e30, 1e30, 1e30}, max[3]={-1e30, -1e30, -1e30};
	float scale[3]={1.0f, 1.0f, 1.0f}, temp[3], xn;
	int i, texflag=0, flipnorm;
	Object *obt;
		
	/* Object Size with parenting */
	obt=ob;
	while(obt){
		VecAddf(temp, obt->size, obt->dsize);
		scale[0]*=temp[0]; scale[1]*=temp[1]; scale[2]*=temp[2];
		obt=obt->parent;
	}
	
	/* Clear all flags */
	for(i=startvert; i<startvert+numvert; i++){ 
		vr= RE_findOrAddVert(i);
		vr->flag= 0;
	}
	
	for(i=startface; i<startface+numface; i++){
		vlr=RE_findOrAddVlak(i);
		displace_render_face(vlr, scale);
	}
	
	/* Recalc vertex normals */
	normalenrender(startvert, startface);
}

void displace_render_face(VlakRen *vlr, float *scale)
{
	ShadeInput shi;
	VertRen vr;
	float samp1,samp2, samp3, samp4, nor[3], xn;
	short hasuv=0, flipnorm=0;
	/* set up shadeinput struct for multitex() */
	
	shi.osatex= 0;		/* signal not to use dx[] and dy[] texture AA vectors */
	shi.vlr= vlr;		/* current render face */
	shi.mat= vlr->mat;		/* current input material */
	shi.matren= shi.mat->ren;	/* material temp block where output is written into */
	
	/* Test for flipped normals */
	if(vlr->v4) CalcNormFloat4(vlr->v4->co, vlr->v3->co, vlr->v2->co,
							    vlr->v1->co, nor);
	else CalcNormFloat(vlr->v3->co, vlr->v2->co, vlr->v1->co,
							    nor);
								
	xn= vlr->n[0]*nor[0]+vlr->n[1]*nor[1]+vlr->n[2]*nor[2];
	if (xn<0.0) flipnorm=1;
	
	/* UV coords must come from face */
	hasuv = vlr->tface && (shi.matren->texco & TEXCO_UV);
	if (hasuv) shi.uv[2]=0.0f; 
	/* I don't think this is used, but seting it just in case */
	
	/* Displace the verts, flag is set when done */
	if (! (vlr->v1->flag)){		
		if (hasuv)	{
			shi.uv[0] = 2*vlr->tface->uv[0][0]-1.0f; /* shi.uv and tface->uv are */
			shi.uv[1]=  2*vlr->tface->uv[0][1]-1.0f; /* scalled differently 	 */
		}
		displace_render_vert(&shi, vlr->v1, scale);
	}
	
	if (! (vlr->v2->flag)) {
		if (hasuv)	{
			shi.uv[0] = 2*vlr->tface->uv[1][0]-1.0f; 
			shi.uv[1]=  2*vlr->tface->uv[1][1]-1.0f;
		}
		displace_render_vert(&shi, vlr->v2, scale);
	}
	
 	if (! (vlr->v3->flag)) {
		if (hasuv)	{
			shi.uv[0] = 2*vlr->tface->uv[2][0]-1.0f; 
			shi.uv[1]=  2*vlr->tface->uv[2][1]-1.0f;
		}	
		displace_render_vert(&shi, vlr->v3, scale);
	}
			
	if (vlr->v4) {
		if (! (vlr->v4->flag)) {
		 	if (hasuv)	{
				shi.uv[0] = 2*vlr->tface->uv[3][0]-1.0f; 
				shi.uv[1]=  2*vlr->tface->uv[3][1]-1.0f;
			}	
			displace_render_vert(&shi, vlr->v4, scale);
		}
		/* We want to split the quad along the opposite verts that are */
		/*	closest in displace value.  This will help smooth edges.   */ 
		if ( fabs(vlr->v1->accum - vlr->v3->accum) > fabs(vlr->v2->accum - vlr->v4->accum)) 
				vlr->flag |= R_DIVIDE_24;
		else 	vlr->flag & ~R_DIVIDE_24; 
	}
	
	/* Recalculate the face normal  - if flipped before, flip now */
	if(vlr->v4) {
		if (flipnorm) vlr->len = CalcNormFloat4(vlr->v1->co, vlr->v2->co, vlr->v3->co,
					    vlr->v4->co, vlr->n);
		else vlr->len = CalcNormFloat4(vlr->v4->co, vlr->v3->co, vlr->v2->co,
					    vlr->v1->co, vlr->n);
	}	
	else {
		if (flipnorm) vlr->len= CalcNormFloat(vlr->v1->co, vlr->v2->co, vlr->v3->co,
							    vlr->n);
		else vlr->len= CalcNormFloat(vlr->v3->co, vlr->v2->co, vlr->v1->co,
							    vlr->n);
	}
	
}

void displace_render_vert(ShadeInput *shi, VertRen *vr, float *scale)
{
	short texco= shi->matren->texco;
	float sample=0;
	/* shi->co is current render coord, just make sure at least some vector is here */
	VECCOPY(shi->co, vr->co);
	/* vertex normal is used for textures type 'col' and 'var' */
	VECCOPY(shi->vn, vr->n);
	
	/* set all rendercoords, 'texco' is an ORed value for all textures needed */
	if ((texco & TEXCO_ORCO) && (vr->orco)) {
		VECCOPY(shi->lo, vr->orco);
	}
	if ((texco & TEXCO_STICKY) && (vr->sticky)) {
		VECCOPY(shi->sticky, vr->sticky);
	}
	if (texco & TEXCO_GLOB) {
		VECCOPY(shi->gl, shi->co);
		MTC_Mat4MulVecfl(R.viewinv, shi->gl);
	}
	if (texco & TEXCO_NORM) {
		VECCOPY(shi->orn, shi->vn);
	}
	if(texco & TEXCO_REFL) {
		/* not (yet?) */
	}
	
	shi->displace[0]= shi->displace[1]= shi->displace[2]= 0.0;
	
	do_material_tex(shi);
	
	//printf("no=%f, %f, %f\nbefore co=%f, %f, %f\n", vr->n[0], vr->n[1], vr->n[2], 
	//vr->co[0], vr->co[1], vr->co[2]);
	
	/* 0.5 could become button once?  */
	vr->co[0] +=  shi->displace[0] * scale[0] ; 
	vr->co[1] +=  shi->displace[1] * scale[1] ; 
	vr->co[2] +=  shi->displace[2] * scale[2] ; 
	
	//printf("after co=%f, %f, %f\n", vr->co[0], vr->co[1], vr->co[2]); 

	/* we just don't do this vertex again, bad luck for other face using same vertex with
	   different material... */
	vr->flag |= 1;
	
	/* Pass sample back so displace_face can decide which way to split the quad */
	sample  = shi->displace[0]*shi->displace[0];
	sample += shi->displace[1]*shi->displace[1];
	sample += shi->displace[2]*shi->displace[2];
	
	vr->accum=sample; 
	/* Should be sqrt(sample), but I'm only looking for "bigger".  Save the cycles. */
	return;
}
