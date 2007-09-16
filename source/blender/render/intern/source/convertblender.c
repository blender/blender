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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "blendef.h"
#include "MTC_matrixops.h"

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_memarena.h"
#include "BLI_ghash.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_image_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_object_fluidsim.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"

#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_colortools.h"
#include "BKE_constraint.h"
#include "BKE_displist.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_key.h"
#include "BKE_ipo.h"
#include "BKE_image.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_world.h"

#include "PIL_time.h"
#include "IMB_imbuf_types.h"

#include "envmap.h"
#include "multires.h"
#include "render_types.h"
#include "rendercore.h"
#include "renderdatabase.h"
#include "renderpipeline.h"
#include "radio.h"
#include "shadbuf.h"
#include "shading.h"
#include "texture.h"
#include "sss.h"
#include "zbuf.h"

#ifndef DISABLE_YAFRAY /* disable yafray */

#include "YafRay_Api.h"

/* yafray: Identity transform 'hack' removed, exporter now transforms vertices back to world.
 * Same is true for lamp coords & vec.
 * Duplicated data objects & dupliframe/duplivert objects are only stored once,
 * only the matrix is stored for all others, in yafray these objects are instances of the original.
 * The main changes are in RE_rotateBlenderScene().
 */

#endif /* disable yafray */

/* ------------------------------------------------------------------------- */
/* Local functions                                                           */
/* ------------------------------------------------------------------------- */
static short test_for_displace(Render *re, Object *ob);
static void do_displacement(Render *re, Object *ob, int startface, int numface, int startvert, int numvert );

/* 10 times larger than normal epsilon, test it on default nurbs sphere with ray_transp (for quad detection) */
/* or for checking vertex normal flips */
#define FLT_EPSILON10 1.19209290e-06F


/* ------------------------------------------------------------------------- */

/* Stuff for stars. This sits here because it uses gl-things. Part of
this code may move down to the converter.  */
/* ------------------------------------------------------------------------- */
/* this is a bad beast, since it is misused by the 3d view drawing as well. */

static HaloRen *initstar(Render *re, float *vec, float hasize)
{
	HaloRen *har;
	float hoco[4];
	
	projectverto(vec, re->winmat, hoco);
	
	har= RE_findOrAddHalo(re, re->tothalo++);
	
	/* projectvert is done in function zbufvlaggen again, because of parts */
	VECCOPY(har->co, vec);
	har->hasize= hasize;
	
	har->zd= 0.0;
	
	return har;
}

/* there must be a 'fixed' amount of stars generated between
*         near and far
* all stars must by preference lie on the far and solely
*        differ in clarity/color
*/

void RE_make_stars(Render *re, void (*initfunc)(void),
				   void (*vertexfunc)(float*),  void (*termfunc)(void))
{
	extern unsigned char hash[512];
	World *wrld= NULL;
	HaloRen *har;
	Scene *scene;
	Camera *camera;
	double dblrand, hlfrand;
	float vec[4], fx, fy, fz;
	float fac, starmindist, clipend;
	float mat[4][4], stargrid, maxrand, maxjit, force, alpha;
	int x, y, z, sx, sy, sz, ex, ey, ez, done = 0;
	
	if(initfunc) {
		scene= G.scene;
		wrld= G.scene->world;
	}
	else {
		scene= re->scene;
		wrld= &(re->wrld);
	}
	
	stargrid = wrld->stardist;			/* distance between stars */
	maxrand = 2.0;						/* amount a star can be shifted (in grid units) */
	maxjit = (wrld->starcolnoise);		/* amount a color is being shifted */
	
	/* size of stars */
	force = ( wrld->starsize );
	
	/* minimal free space (starting at camera) */
	starmindist= wrld->starmindist;
	
	if (stargrid <= 0.10) return;
	
	if (re) re->flag |= R_HALO;
	else stargrid *= 1.0;				/* then it draws fewer */
	
	if(re) MTC_Mat4Invert(mat, re->viewmat);
	
	/* BOUNDING BOX CALCULATION
		* bbox goes from z = loc_near_var | loc_far_var,
		* x = -z | +z,
		* y = -z | +z
		*/
	
	if(scene->camera==NULL)
		return;
	camera = scene->camera->data;
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
					MTC_Mat4MulVecfl(re->viewmat, vec);
					
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
						
						har = initstar(re, vec, fac);
						
						if (har) {
							har->alfa = sqrt(sqrt(alpha));
							har->add= 255;
							har->r = har->g = har->b = 1.0;
							if (maxjit) {
								har->r += ((maxjit * BLI_drand()) ) - maxjit;
								har->g += ((maxjit * BLI_drand()) ) - maxjit;
								har->b += ((maxjit * BLI_drand()) ) - maxjit;
							}
							har->hard = 32;
							har->lay= -1;
							har->type |= HA_ONLYSKY;
							done++;
						}
					}
				}
			}
			/* do not call blender_test_break() here, since it is used in UI as well, confusing the callback system */
			/* main cause is G.afbreek of course, a global again... (ton) */
		}
	}
	if (termfunc) termfunc();
}


/* ------------------------------------------------------------------------- */
/* tool functions/defines for ad hoc simplification and possible future 
   cleanup      */
/* ------------------------------------------------------------------------- */

#define UVTOINDEX(u,v) (startvlak + (u) * sizev + (v))
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

static void split_v_renderfaces(Render *re, int startvlak, int startvert, int usize, int vsize, int uIndex, int cyclu, int cyclv)
{
	int vLen = vsize-1+(!!cyclv);
	int v;

	for (v=0; v<vLen; v++) {
		VlakRen *vlr = RE_findOrAddVlak(re, startvlak + vLen*uIndex + v);
		VertRen *vert = RE_vertren_copy(re, vlr->v2);

		if (cyclv) {
			vlr->v2 = vert;

			if (v==vLen-1) {
				VlakRen *vlr = RE_findOrAddVlak(re, startvlak + vLen*uIndex + 0);
				vlr->v1 = vert;
			} else {
				VlakRen *vlr = RE_findOrAddVlak(re, startvlak + vLen*uIndex + v+1);
				vlr->v1 = vert;
			}
		} else {
			vlr->v2 = vert;

			if (v<vLen-1) {
				VlakRen *vlr = RE_findOrAddVlak(re, startvlak + vLen*uIndex + v+1);
				vlr->v1 = vert;
			}

			if (v==0) {
				vlr->v1 = RE_vertren_copy(re, vlr->v1);
			} 
		}
	}
}

/* ------------------------------------------------------------------------- */

static int check_vnormal(float *n, float *veno)
{
	float inp;

	inp=n[0]*veno[0]+n[1]*veno[1]+n[2]*veno[2];
	if(inp < -FLT_EPSILON10) return 1;
	return 0;
}

/* ------------------------------------------------------------------------- */

static void calc_edge_stress_add(float *accum, VertRen *v1, VertRen *v2)
{
	float len= VecLenf(v1->co, v2->co)/VecLenf(v1->orco, v2->orco);
	float *acc;
	
	acc= accum + 2*v1->index;
	acc[0]+= len;
	acc[1]+= 1.0f;
	
	acc= accum + 2*v2->index;
	acc[0]+= len;
	acc[1]+= 1.0f;
}

static void calc_edge_stress(Render *re, Mesh *me, int startvert, int startvlak)
{
	float loc[3], size[3], *accum, *acc, *accumoffs, *stress;
	int a;
	
	if(startvert==re->totvert) return;
	
	mesh_get_texspace(me, loc, NULL, size);
	
	accum= MEM_callocN(2*sizeof(float)*(re->totvert-startvert), "temp accum for stress");
	
	/* de-normalize orco */
	for(a=startvert; a<re->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(re, a);
		if(ver->orco) {
			ver->orco[0]= ver->orco[0]*size[0] +loc[0];
			ver->orco[1]= ver->orco[1]*size[1] +loc[1];
			ver->orco[2]= ver->orco[2]*size[2] +loc[2];
		}
	}
	
	/* add stress values */
	accumoffs= accum - 2*startvert;	/* so we can use vertex index */
	for(a=startvlak; a<re->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(re, a);

		if(vlr->v1->orco && vlr->v4) {
			calc_edge_stress_add(accumoffs, vlr->v1, vlr->v2);
			calc_edge_stress_add(accumoffs, vlr->v2, vlr->v3);
			calc_edge_stress_add(accumoffs, vlr->v3, vlr->v1);
			if(vlr->v4) {
				calc_edge_stress_add(accumoffs, vlr->v3, vlr->v4);
				calc_edge_stress_add(accumoffs, vlr->v4, vlr->v1);
				calc_edge_stress_add(accumoffs, vlr->v2, vlr->v4);
			}
		}
	}
	
	for(a=startvert; a<re->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(re, a);
		if(ver->orco) {
			/* find stress value */
			acc= accumoffs + 2*ver->index;
			if(acc[1]!=0.0f)
				acc[0]/= acc[1];
			stress= RE_vertren_get_stress(re, ver, 1);
			*stress= *acc;
			
			/* restore orcos */
			ver->orco[0] = (ver->orco[0]-loc[0])/size[0];
			ver->orco[1] = (ver->orco[1]-loc[1])/size[1];
			ver->orco[2] = (ver->orco[2]-loc[2])/size[2];
		}
	}
	
	MEM_freeN(accum);
}

/* gets tangent from tface or orco */
static void calc_tangent_vector(Render *re, VlakRen *vlr)
{
	MTFace *tface= RE_vlakren_get_tface(re, vlr, 0, NULL, 0);
	VertRen *v1=vlr->v1, *v2=vlr->v2, *v3=vlr->v3, *v4=vlr->v4;
	float tang[3], tangv[3], ct[3], e1[3], e2[3], *tav;
	float *uv1, *uv2, *uv3, *uv4;
	float s1, s2, t1, t2, det;
	float uv[4][2];
	
	if(tface) {
		uv1= tface->uv[0];
		uv2= tface->uv[1];
		uv3= tface->uv[2];
		uv4= tface->uv[3];
	}
	else if(v1->orco) {
		uv1= uv[0]; uv2= uv[1]; uv3= uv[2]; uv4= uv[3];
		spheremap(v1->orco[0], v1->orco[1], v1->orco[2], &uv[0][0], &uv[0][1]);
		spheremap(v2->orco[0], v2->orco[1], v2->orco[2], &uv[1][0], &uv[1][1]);
		spheremap(v3->orco[0], v3->orco[1], v3->orco[2], &uv[2][0], &uv[2][1]);
		if(v4)
			spheremap(v4->orco[0], v4->orco[1], v4->orco[2], &uv[3][0], &uv[3][1]);
	}
	else return;
	
	s1= uv2[0] - uv1[0];
	s2= uv3[0] - uv1[0];
	t1= uv2[1] - uv1[1];
	t2= uv3[1] - uv1[1];
	det= 1.0f / (s1 * t2 - s2 * t1);
	
	/* normals in render are inversed... */
	VecSubf(e1, v1->co, v2->co);
	VecSubf(e2, v1->co, v3->co);
	tang[0] = (t2*e1[0] - t1*e2[0])*det;
	tang[1] = (t2*e1[1] - t1*e2[1])*det;
	tang[2] = (t2*e1[2] - t1*e2[2])*det;
	tangv[0] = (s1*e2[0] - s2*e1[0])*det;
	tangv[1] = (s1*e2[1] - s2*e1[1])*det;
	tangv[2] = (s1*e2[2] - s2*e1[2])*det;
	Crossf(ct, tang, tangv);
	/* check flip */
	if ((ct[0]*vlr->n[0] + ct[1]*vlr->n[1] + ct[2]*vlr->n[2]) < 0.f)
		VecMulf(tang, -1.f);
	
	tav= RE_vertren_get_tangent(re, v1, 1);
	VECADD(tav, tav, tang);
	tav= RE_vertren_get_tangent(re, v2, 1);
	VECADD(tav, tav, tang);
	tav= RE_vertren_get_tangent(re, v3, 1);
	VECADD(tav, tav, tang);
	
	if(v4) {
		s1= uv3[0] - uv1[0];
		s2= uv4[0] - uv1[0];
		t1= uv3[1] - uv1[1];
		t2= uv4[1] - uv1[1];
		det= 1.0f / (s1 * t2 - s2 * t1);
		
		/* normals in render are inversed... */
		VecSubf(e1, v1->co, v3->co);
		VecSubf(e2, v1->co, v4->co);
		tang[0] = (t2*e1[0] - t1*e2[0])*det;
		tang[1] = (t2*e1[1] - t1*e2[1])*det;
		tang[2] = (t2*e1[2] - t1*e2[2])*det;
		tangv[0] = (s1*e2[0] - s2*e1[0])*det;
		tangv[1] = (s1*e2[1] - s2*e1[1])*det;
		tangv[2] = (s1*e2[2] - s2*e1[2])*det;
		Crossf(ct, tang, tangv);
		if ((ct[0]*vlr->n[0] + ct[1]*vlr->n[1] + ct[2]*vlr->n[2]) < 0.f)
			VecMulf(tang, -1.f);
		
		tav= RE_vertren_get_tangent(re, v1, 1);
		VECADD(tav, tav, tang);
		tav= RE_vertren_get_tangent(re, v3, 1);
		VECADD(tav, tav, tang);
		tav= RE_vertren_get_tangent(re, v4, 1);
		VECADD(tav, tav, tang);
	}
}


static void calc_vertexnormals(Render *re, int startvert, int startvlak, int do_tangent)
{
	int a;

		/* clear all vertex normals */
	for(a=startvert; a<re->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(re, a);
		ver->n[0]=ver->n[1]=ver->n[2]= 0.0f;
	}

		/* calculate cos of angles and point-masses, use as weight factor to
		   add face normal to vertex */
	for(a=startvlak; a<re->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(re, a);
		if(vlr->flag & ME_SMOOTH) {
			VertRen *v1= vlr->v1;
			VertRen *v2= vlr->v2;
			VertRen *v3= vlr->v3;
			VertRen *v4= vlr->v4;
			float n1[3], n2[3], n3[3], n4[3];
			float fac1, fac2, fac3, fac4=0.0f;
			
			if(re->flag & R_GLOB_NOPUNOFLIP)
				vlr->flag |= R_NOPUNOFLIP;
			
			VecSubf(n1, v2->co, v1->co);
			Normalize(n1);
			VecSubf(n2, v3->co, v2->co);
			Normalize(n2);
			if(v4==NULL) {
				VecSubf(n3, v1->co, v3->co);
				Normalize(n3);

				fac1= saacos(-n1[0]*n3[0]-n1[1]*n3[1]-n1[2]*n3[2]);
				fac2= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
				fac3= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			}
			else {
				VecSubf(n3, v4->co, v3->co);
				Normalize(n3);
				VecSubf(n4, v1->co, v4->co);
				Normalize(n4);

				fac1= saacos(-n4[0]*n1[0]-n4[1]*n1[1]-n4[2]*n1[2]);
				fac2= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
				fac3= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
				fac4= saacos(-n3[0]*n4[0]-n3[1]*n4[1]-n3[2]*n4[2]);

				if(!(vlr->flag & R_NOPUNOFLIP)) {
					if( check_vnormal(vlr->n, v4->n) ) fac4= -fac4;
				}

				v4->n[0] +=fac4*vlr->n[0];
				v4->n[1] +=fac4*vlr->n[1];
				v4->n[2] +=fac4*vlr->n[2];
			}

			if(!(vlr->flag & R_NOPUNOFLIP)) {
				if( check_vnormal(vlr->n, v1->n) ) fac1= -fac1;
				if( check_vnormal(vlr->n, v2->n) ) fac2= -fac2;
				if( check_vnormal(vlr->n, v3->n) ) fac3= -fac3;
			}

			v1->n[0] +=fac1*vlr->n[0];
			v1->n[1] +=fac1*vlr->n[1];
			v1->n[2] +=fac1*vlr->n[2];

			v2->n[0] +=fac2*vlr->n[0];
			v2->n[1] +=fac2*vlr->n[1];
			v2->n[2] +=fac2*vlr->n[2];

			v3->n[0] +=fac3*vlr->n[0];
			v3->n[1] +=fac3*vlr->n[1];
			v3->n[2] +=fac3*vlr->n[2];
			
		}
		if(do_tangent) {
			/* tangents still need to be calculated for flat faces too */
			/* weighting removed, they are not vertexnormals */
			calc_tangent_vector(re, vlr);
		}
	}

		/* do solid faces */
	for(a=startvlak; a<re->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(re, a);
		if((vlr->flag & ME_SMOOTH)==0) {
			float *f1= vlr->v1->n;
			if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) VECCOPY(f1, vlr->n);
			f1= vlr->v2->n;
			if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) VECCOPY(f1, vlr->n);
			f1= vlr->v3->n;
			if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) VECCOPY(f1, vlr->n);
			if(vlr->v4) {
				f1= vlr->v4->n;
				if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) VECCOPY(f1, vlr->n);
			}
		}
	}
	
		/* normalize vertex normals */
	for(a=startvert; a<re->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(re, a);
		Normalize(ver->n);
		if(do_tangent) {
			float *tav= RE_vertren_get_tangent(re, ver, 0);
			if (tav) {
				/* orthonorm. */
				float tdn = tav[0]*ver->n[0] + tav[1]*ver->n[1] + tav[2]*ver->n[2];
				tav[0] -= ver->n[0]*tdn;
				tav[1] -= ver->n[1]*tdn;
				tav[2] -= ver->n[2]*tdn;
				Normalize(tav);
			}
		}
	}

		/* vertex normal (puno) switch flags for during render */
	for(a=startvlak; a<re->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(re, a);

		if((vlr->flag & R_NOPUNOFLIP)==0) {
			float *v1= vlr->v1->n;
			float *v2= vlr->v2->n;
			float *v3= vlr->v3->n;
			float *v4= vlr->v4?vlr->v4->n:NULL;
			float *nor= vlr->n;
			vlr->puno &= ~15;
			if ((nor[0]*v1[0] + nor[1]*v1[1] + nor[2]*v1[2]) < -FLT_EPSILON10) vlr->puno= 1;
			if ((nor[0]*v2[0] + nor[1]*v2[1] + nor[2]*v2[2]) < -FLT_EPSILON10) vlr->puno+= 2;
			if ((nor[0]*v3[0] + nor[1]*v3[1] + nor[2]*v3[2]) < -FLT_EPSILON10) vlr->puno+= 4;
			if(v4) {
				if((nor[0]*v4[0] + nor[1]*v4[1] + nor[2]*v4[2]) < -FLT_EPSILON10 ) vlr->puno+= 8;
			}
		}
	}
}

// NT same as calc_vertexnormals, but dont modify the existing vertex normals
// only recalculate other render data. If this is at some point used for other things than fluidsim,
// this could be made on option for the normal calc_vertexnormals
static void calc_fluidsimnormals(Render *re, int startvert, int startvlak, int do_tangent)
{
	int a;

	/* dont clear vertex normals here */
	// OFF for(a=startvert; a<re->totvert; a++) { VertRen *ver= RE_findOrAddVert(re, a); ver->n[0]=ver->n[1]=ver->n[2]= 0.0; }
	/* calculate cos of angles and point-masses, use as weight factor to add face normal to vertex */
	for(a=startvlak; a<re->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(re, a);
		if(vlr->flag & ME_SMOOTH) {
			VertRen *v1= vlr->v1;
			VertRen *v2= vlr->v2;
			VertRen *v3= vlr->v3;
			VertRen *v4= vlr->v4;
			float n1[3], n2[3], n3[3], n4[3];
			float fac1, fac2, fac3, fac4=0.0f;

			if(re->flag & R_GLOB_NOPUNOFLIP)
				vlr->flag |= R_NOPUNOFLIP;
			
			VecSubf(n1, v2->co, v1->co);
			Normalize(n1);
			VecSubf(n2, v3->co, v2->co);
			Normalize(n2);
			if(v4==NULL) {
				VecSubf(n3, v1->co, v3->co);
				Normalize(n3);
				fac1= saacos(-n1[0]*n3[0]-n1[1]*n3[1]-n1[2]*n3[2]);
				fac2= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
				fac3= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			}
			else {
				VecSubf(n3, v4->co, v3->co);
				Normalize(n3);
				VecSubf(n4, v1->co, v4->co);
				Normalize(n4);

				fac1= saacos(-n4[0]*n1[0]-n4[1]*n1[1]-n4[2]*n1[2]);
				fac2= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
				fac3= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
				fac4= saacos(-n3[0]*n4[0]-n3[1]*n4[1]-n3[2]*n4[2]);

				if(!(vlr->flag & R_NOPUNOFLIP)) {
					if( check_vnormal(vlr->n, v4->n) ) fac4= -fac4;
				}
			}

			//if(do_tangent)
			//	calc_tangent_vector(re, vlr, fac1, fac2, fac3, fac4);
		}
		if(do_tangent) {
			/* tangents still need to be calculated for flat faces too */
			/* weighting removed, they are not vertexnormals */
			calc_tangent_vector(re, vlr);
		}
	}

	/* do solid faces */
	for(a=startvlak; a<re->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(re, a);
		if((vlr->flag & ME_SMOOTH)==0) {
			float *f1= vlr->v1->n;
			if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) VECCOPY(f1, vlr->n);
			f1= vlr->v2->n;
			if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) VECCOPY(f1, vlr->n);
			f1= vlr->v3->n;
			if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) VECCOPY(f1, vlr->n);
			if(vlr->v4) {
				f1= vlr->v4->n;
				if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) VECCOPY(f1, vlr->n);
			}			
		}
	}
	
	/* normalize vertex normals */
	for(a=startvert; a<re->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(re, a);
		Normalize(ver->n);
		if(do_tangent) {
			float *tav= RE_vertren_get_tangent(re, ver, 0);
			if(tav) Normalize(tav);
		}
	}

	/* vertex normal (puno) switch flags for during render */
	for(a=startvlak; a<re->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(re, a);
		if((vlr->flag & R_NOPUNOFLIP)==0) {
			VertRen *v1= vlr->v1;
			VertRen *v2= vlr->v2;
			VertRen *v3= vlr->v3;
			VertRen *v4= vlr->v4;
			vlr->puno &= ~15;
			if ((vlr->n[0]*v1->n[0]+vlr->n[1]*v1->n[1]+vlr->n[2]*v1->n[2])<0.0) vlr->puno= 1;
			if ((vlr->n[0]*v2->n[0]+vlr->n[1]*v2->n[1]+vlr->n[2]*v2->n[2])<0.0) vlr->puno+= 2;
			if ((vlr->n[0]*v3->n[0]+vlr->n[1]*v3->n[1]+vlr->n[2]*v3->n[2])<0.0) vlr->puno+= 4;
			if(v4) {
				if((vlr->n[0]*v4->n[0]+vlr->n[1]*v4->n[1]+vlr->n[2]*v4->n[2])<0.0) vlr->puno+= 8;
			}
		}
	}
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

static void as_addvert(ASvert *asv, VertRen *v1, VlakRen *vlr)
{
	ASface *asf;
	int a;
	
	if(v1 == NULL) return;
	
	if(asv->faces.first==NULL) {
		asf= MEM_callocN(sizeof(ASface), "asface");
		BLI_addtail(&asv->faces, asf);
	}
	
	asf= asv->faces.last;
	for(a=0; a<4; a++) {
		if(asf->vlr[a]==NULL) {
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

/* note; autosmooth happens in object space still, after applying autosmooth we rotate */
/* note2; actually, when original mesh and displist are equal sized, face normals are from original mesh */
static void autosmooth(Render *re, float mat[][4], int startvert, int startvlak, int degr)
{
	ASvert *asv, *asverts, *asvertoffs;
	ASface *asf;
	VertRen *ver, *v1;
	VlakRen *vlr;
	float thresh;
	int a, b, totvert;
	
	if(startvert==re->totvert) return;
	asverts= MEM_callocN(sizeof(ASvert)*(re->totvert-startvert), "all smooth verts");
	asvertoffs= asverts-startvert;	 /* se we can use indices */
	
	thresh= cos( M_PI*(0.5f+(float)degr)/180.0 );
	
	/* step zero: give faces normals of original mesh, if this is provided */
	
	
	/* step one: construct listbase of all vertices and pointers to faces */
	for(a=startvlak; a<re->totvlak; a++) {
		vlr= RE_findOrAddVlak(re, a);
		/* skip wire faces */
		if(vlr->v2 != vlr->v3) {
			as_addvert(asvertoffs+vlr->v1->index, vlr->v1, vlr);
			as_addvert(asvertoffs+vlr->v2->index, vlr->v2, vlr);
			as_addvert(asvertoffs+vlr->v3->index, vlr->v3, vlr);
			if(vlr->v4) 
				as_addvert(asvertoffs+vlr->v4->index, vlr->v4, vlr);
		}
	}
	
	/* we now test all vertices, when faces have a normal too much different: they get a new vertex */
	totvert= re->totvert;
	for(a=startvert, asv=asverts; a<totvert; a++, asv++) {
		if(asv && asv->totface>1) {
			ver= RE_findOrAddVert(re, a);
			
			asf= asv->faces.first;
			while(asf) {
				for(b=0; b<4; b++) {
				
					/* is there a reason to make a new vertex? */
					vlr= asf->vlr[b];
					if( as_testvertex(vlr, ver, asv, thresh) ) {
						
						/* already made a new vertex within threshold? */
						v1= as_findvertex(vlr, ver, asv, thresh);
						if(v1==NULL) {
							/* make a new vertex */
							v1= RE_vertren_copy(re, ver);
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
	for(a=0; a<totvert-startvert; a++) {
		BLI_freelistN(&asverts[a].faces);
	}
	MEM_freeN(asverts);
	
	/* rotate vertices and calculate normal of faces */
	for(a=startvert; a<re->totvert; a++) {
		ver= RE_findOrAddVert(re, a);
		MTC_Mat4MulVecfl(mat, ver->co);
	}
	for(a=startvlak; a<re->totvlak; a++) {
		vlr= RE_findOrAddVlak(re, a);
		
		/* skip wire faces */
		if(vlr->v2 != vlr->v3) {
			if(vlr->v4) 
				CalcNormFloat4(vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co, vlr->n);
			else 
				CalcNormFloat(vlr->v3->co, vlr->v2->co, vlr->v1->co, vlr->n);
		}
	}		
}

/* ------------------------------------------------------------------------- */
/* End of autosmoothing:                                                     */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* Orco hash																 */
/* ------------------------------------------------------------------------- */


static float *get_object_orco(Render *re, Object *ob)
{
	float *orco;
	
	if (!re->orco_hash)
		re->orco_hash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	orco = BLI_ghash_lookup(re->orco_hash, ob);
	
	if (!orco) {
		if (ob->type==OB_MESH) {
			orco = mesh_create_orco_render(ob);
		} else if (ELEM(ob->type, OB_CURVE, OB_FONT)) {
			orco = make_orco_curve(ob);
		} else if (ob->type==OB_SURF) {
			orco = make_orco_surf(ob);
		}
		
		if (orco)
			BLI_ghash_insert(re->orco_hash, ob, orco);
	}
	
	return orco;
}

static void free_mesh_orco_hash(Render *re) 
{
	if (re->orco_hash) {
		BLI_ghash_free(re->orco_hash, NULL, (GHashValFreeFP)MEM_freeN);
		re->orco_hash = NULL;
	}
}

/* ******************** END ORCO HASH ***************** */


static void make_render_halos(Render *re, Object *ob, Mesh *me, int totvert, MVert *mvert, Material *ma, float *orco)
{
	HaloRen *har;
	float xn, yn, zn, nor[3], view[3];
	float vec[3], hasize, mat[4][4], imat[3][3];
	int a, ok, seed= ma->seed1;

	MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
	MTC_Mat3CpyMat4(imat, ob->imat);

	re->flag |= R_HALO;

	for(a=0; a<totvert; a++, mvert++) {
		ok= 1;

		if(ok) {
			hasize= ma->hasize;

			VECCOPY(vec, mvert->co);
			MTC_Mat4MulVecfl(mat, vec);

			if(ma->mode & MA_HALOPUNO) {
				xn= mvert->no[0];
				yn= mvert->no[1];
				zn= mvert->no[2];

				/* transpose ! */
				nor[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
				nor[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
				nor[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
				Normalize(nor);

				VECCOPY(view, vec);
				Normalize(view);

				zn= nor[0]*view[0]+nor[1]*view[1]+nor[2]*view[2];
				if(zn>=0.0) hasize= 0.0;
				else hasize*= zn*zn*zn*zn;
			}

			if(orco) har= RE_inithalo(re, ma, vec, NULL, orco, hasize, 0.0, seed);
			else har= RE_inithalo(re, ma, vec, NULL, mvert->co, hasize, 0.0, seed);
			if(har) har->lay= ob->lay;
		}
		if(orco) orco+= 3;
		seed++;
	}
}

/* ------------------------------------------------------------------------- */
static Material *give_render_material(Render *re, Object *ob, int nr)
{
	extern Material defmaterial;	/* material.c */
	Material *ma;
	
	ma= give_current_material(ob, nr);
	if(ma==NULL) 
		ma= &defmaterial;
	else
		if(ma->mode & MA_ZTRA)
			re->flag |= R_ZTRA;
	
	if(re->r.mode & R_SPEED) ma->texco |= NEED_UV;
	
	/* for light groups */
	ma->flag |= MA_IS_USED;
	
	return ma;
}



static void render_particle_system(Render *re, Object *ob, Object *par, PartEff *paf)
{
	Particle *pa=0;
	HaloRen *har=0;
	Material *ma=0;
	float xn, yn, zn, imat[3][3], tmat[4][4], mat[4][4], hasize, stime, ptime, ctime, vec[3], vec1[3], view[3], nor[3];
	float haloScale = 1.0;	//NT scale halos 
	float iniAlpha = 0.0; // restore material alpha 
	int a, mat_nr=1, seed;
	int useFluidsimParticles = 0; // FSPARTICLE

	ma= give_render_material(re, ob, paf->omat);
	
	if( (ob->fluidsimSettings) && (ob->fluidsimSettings->type == OB_FLUIDSIM_PARTICLE)) {
		useFluidsimParticles = 1;
		iniAlpha = ma->alpha;
	}
	
	pa= paf->keys;
	if(pa==NULL || paf->disp!=100 || useFluidsimParticles) {
		build_particle_system(ob);
		pa= paf->keys;
		if(pa==NULL) return;
	}

	MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
	MTC_Mat4Invert(ob->imat, mat);	/* this is correct, for imat texture */

	/* enable duplicators to work */
	if(par) {
		Mat4MulMat4(tmat, paf->imat, ob->obmat);
		MTC_Mat4MulMat4(mat, tmat, re->viewmat);
		
		MTC_Mat4Invert(tmat, mat);
		MTC_Mat3CpyMat4(imat, tmat);
	}
	else {
		MTC_Mat4CpyMat4(mat, re->viewmat);
		
		MTC_Mat4Invert(tmat, re->viewmat);
		MTC_Mat3CpyMat4(imat, tmat);
		
	}	

	re->flag |= R_HALO;

	if(ob->ipoflag & OB_OFFS_PARTICLE) ptime= ob->sf;
	else ptime= 0.0;
	ctime= bsystem_time(ob, (float)re->scene->r.cfra, ptime);
	seed= ma->seed1;
	
	for(a=0; a<paf->totpart; a++, pa+=paf->totkey, seed++) {

		/* offset time for calculating normal */
		stime= ctime;
		ptime= ctime+1.0f;
		if(ctime < pa->time) {
			if(paf->flag & PAF_UNBORN)
				ptime= pa->time+1.0f;
			else
				continue;
		}
		if(ctime > pa->time+pa->lifetime) {
			if(paf->flag & PAF_DIED)
				stime= pa->time+pa->lifetime-1.0f;
			else
				continue;
		}
		
		/* watch it: also calculate the normal of a particle */
		if(paf->stype==PAF_VECT || ma->mode & MA_HALO_SHADE) {
			where_is_particle(paf, pa, stime, vec);
			MTC_Mat4MulVecfl(mat, vec);
			where_is_particle(paf, pa, ptime, vec1);
			MTC_Mat4MulVecfl(mat, vec1);
		}
		else {
			where_is_particle(paf, pa, ctime, vec);
			MTC_Mat4MulVecfl(mat, vec);
		}

		if(pa->mat_nr != mat_nr) {
			mat_nr= pa->mat_nr;
			ma= give_render_material(re, ob, mat_nr);
		}

		if(ma->ipo) {
			/* correction for lifetime */
			ptime= 100.0*(ctime-pa->time)/pa->lifetime;
			calc_ipo(ma->ipo, ptime);
			execute_ipo((ID *)ma, ma->ipo);
		}

		//NT scale halos FSPARTICLE
		if(useFluidsimParticles) {
			// rescale to 1.0-10.0, then div by 5 afterwards, gives values in range 0.2-2.0
			double fspsize = ((double)pa->rt / 1000.0f) / 5.0 ; 
			haloScale = 1.0/(float)pow(fspsize, (double)ob->fluidsimSettings->particleInfSize);
			ma->alpha = iniAlpha / (float)pow( fspsize, (double)ob->fluidsimSettings->particleInfAlpha);
			if(ma->alpha>1.) ma->alpha = 1.;
		}

		hasize= ma->hasize * haloScale;

		if(ma->mode & MA_HALOPUNO) {
			xn= pa->no[0];
			yn= pa->no[1];
			zn= pa->no[2];

			/* transpose ! */
			nor[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
			nor[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
			nor[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
			Normalize(nor);

			VECCOPY(view, vec);
			Normalize(view);

			zn= nor[0]*view[0]+nor[1]*view[1]+nor[2]*view[2];
			if(zn>=0.0) hasize= 0.0;
			else hasize*= zn*zn*zn*zn;
		}

		if(paf->stype==PAF_VECT) har= RE_inithalo(re, ma, vec, vec1, pa->co, hasize, paf->vectsize, seed);
		else {
			har= RE_inithalo(re, ma, vec, NULL, pa->co, hasize, 0.0, seed);
			if(har && ma->mode & MA_HALO_SHADE) {
				VecSubf(har->no, vec, vec1);
				Normalize(har->no);
			}
		}
		if(har) har->lay= ob->lay;
	}

	/* restore material */
	for(a=1; a<=ob->totcol; a++) {
		ma= give_render_material(re, ob, a);
		if(ma) do_mat_ipo(ma);
	}
	
	if(paf->disp!=100) {
		MEM_freeN(paf->keys);
		paf->keys= NULL;
	}

	if(useFluidsimParticles) { ma->alpha = iniAlpha; }// FSPARTICLE restore...
}


/* ------------------------------------------------------------------------- */

/* future thread problem... */
static void static_particle_strand(Render *re, Object *ob, Material *ma, float *orco, float *vec, float *vec1, float ctime, int first)
{
	static VertRen *v1= NULL, *v2= NULL;
	VlakRen *vlr;
	float nor[3], cross[3], w, dx, dy, width;
	int flag;
	
	VecSubf(nor, vec, vec1);
	Normalize(nor);		// nor needed as tangent 
	Crossf(cross, vec, nor);
	
	/* turn cross in pixelsize */
	w= vec[2]*re->winmat[2][3] + re->winmat[3][3];
	dx= re->winx*cross[0]*re->winmat[0][0]/w;
	dy= re->winy*cross[1]*re->winmat[1][1]/w;
	w= sqrt(dx*dx + dy*dy);
	if(w!=0.0f) {
		float fac;
		if(ma->strand_ease!=0.0f) {
			if(ma->strand_ease<0.0f)
				fac= pow(ctime, 1.0+ma->strand_ease);
			else
				fac= pow(ctime, 1.0/(1.0f-ma->strand_ease));
		}
		else fac= ctime;
		
		width= ((1.0f-fac)*ma->strand_sta + (fac)*ma->strand_end)/w;
		VecMulf(cross, width);
	}
	else width= 1.0f;
	
	if(ma->mode & MA_TANGENT_STR)
		flag= R_SMOOTH|R_NOPUNOFLIP|R_TANGENT;
	else
		flag= R_SMOOTH;
	
	/* only 1 pixel wide strands filled in as quads now, otherwise zbuf errors */
	if(ma->strand_sta==1.0f)
		flag |= R_STRAND;
	
	/* first two vertices */
	if(first) {
		v1= RE_findOrAddVert(re, re->totvert++);
		v2= RE_findOrAddVert(re, re->totvert++);
		
		VECCOPY(v1->co, vec);
		VecAddf(v1->co, v1->co, cross);
		VECCOPY(v1->n, nor);
		v1->orco= orco;
		v1->accum= -1.0f;	// accum abuse for strand texco
		
		VECCOPY(v2->co, vec);
		VecSubf(v2->co, v2->co, cross);
		VECCOPY(v2->n, nor);
		v2->orco= orco;
		v2->accum= v1->accum;
	}
	else {
		
		vlr= RE_findOrAddVlak(re, re->totvlak++);
		vlr->flag= flag;
		vlr->ob= ob;
		vlr->v1= v1;
		vlr->v2= v2;
		vlr->v3= RE_findOrAddVert(re, re->totvert++);
		vlr->v4= RE_findOrAddVert(re, re->totvert++);
		
		v1= vlr->v4; // cycle
		v2= vlr->v3; // cycle
		
		VECCOPY(vlr->v4->co, vec);
		VecAddf(vlr->v4->co, vlr->v4->co, cross);
		VECCOPY(vlr->v4->n, nor);
		vlr->v4->orco= orco;
		vlr->v4->accum= -1.0f + 2.0f*ctime;	// accum abuse for strand texco
		
		VECCOPY(vlr->v3->co, vec);
		VecSubf(vlr->v3->co, vlr->v3->co, cross);
		VECCOPY(vlr->v3->n, nor);
		vlr->v3->orco= orco;
		vlr->v3->accum= vlr->v4->accum;
		
		CalcNormFloat4(vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co, vlr->n);
		
		vlr->mat= ma;
		vlr->ec= ME_V2V3;
		vlr->lay= ob->lay;
	}
}

static void render_static_particle_system(Render *re, Object *ob, PartEff *paf)
{
	Particle *pa=0;
	HaloRen *har=0;
	Material *ma=0;
	VertRen *v1= NULL;
	VlakRen *vlr;
	float xn, yn, zn, imat[3][3], mat[4][4], hasize;
	float mtime, ptime, ctime, vec[3], vec1[3], view[3], nor[3];
	float *orco= NULL, loc_tex[3], size_tex[3];
	int a, mat_nr=1, seed, totvlako, totverto, first;

	pa= paf->keys;
	if(pa==NULL || (paf->flag & PAF_ANIMATED) || paf->disp!=100) {
		build_particle_system(ob);
		pa= paf->keys;
		if(pa==NULL) return;
	}

	totvlako= re->totvlak;
	totverto= re->totvert;
	
	ma= give_render_material(re, ob, paf->omat);
	if(ma->mode & MA_HALO)
		re->flag |= R_HALO;

	MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
	MTC_Mat4Invert(ob->imat, mat);	/* need to be that way, for imat texture */

	MTC_Mat3CpyMat4(imat, ob->imat);
	
	/* orcos */
	if(!(ma->mode & (MA_HALO|MA_WIRE))) {
		orco= MEM_mallocN(3*sizeof(float)*paf->totpart, "static particle orcos");
		if (!re->orco_hash)
			re->orco_hash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
		BLI_ghash_insert(re->orco_hash, paf, orco);	/* pointer is particles, otherwise object uses it */
	}
	
	mesh_get_texspace(ob->data, loc_tex, NULL, size_tex);
	
	if(ob->ipoflag & OB_OFFS_PARTICLE) ptime= ob->sf;
	else ptime= 0.0;
	ctime= bsystem_time(ob, (float)re->scene->r.cfra, ptime);
	seed= ma->seed1;

	for(a=0; a<paf->totpart; a++, pa+=paf->totkey) {
		
		where_is_particle(paf, pa, pa->time, vec1);
		if(orco) {
			orco[0] = (vec1[0]-loc_tex[0])/size_tex[0];
			orco[1] = (vec1[1]-loc_tex[1])/size_tex[1];
			orco[2] = (vec1[2]-loc_tex[2])/size_tex[2];
		}
		MTC_Mat4MulVecfl(mat, vec1);
		mtime= pa->time+pa->lifetime+paf->staticstep-1;
		
		first= 1;
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
				ma= give_render_material(re, ob, mat_nr);
			}
			
			/* wires */
			if(ma->mode & MA_WIRE) {
				if(ctime == pa->time) {
					v1= RE_findOrAddVert(re, re->totvert++);
					VECCOPY(v1->co, vec);
				}
				else {
					vlr= RE_findOrAddVlak(re, re->totvlak++);
					vlr->ob= ob;
					vlr->v1= v1;
					vlr->v2= RE_findOrAddVert(re, re->totvert++);
					vlr->v3= vlr->v2;
					vlr->v4= NULL;
					
					v1= vlr->v2; // cycle
					VECCOPY(v1->co, vec);
					
					VecSubf(vlr->n, vec, vec1);
					Normalize(vlr->n);
					VECCOPY(v1->n, vlr->n);
					
					vlr->mat= ma;
					vlr->ec= ME_V1V2;
					vlr->lay= ob->lay;
				}
			}
			else {
				if(ma->ipo) {
					/* correction for lifetime */
					ptime= 100.0*(ctime-pa->time)/pa->lifetime;
					calc_ipo(ma->ipo, ptime);
					execute_ipo((ID *)ma, ma->ipo);
				}
				
				if(ma->mode & MA_HALO) {
					hasize= ma->hasize;

					if(ma->mode & MA_HALOPUNO) {
						xn= pa->no[0];
						yn= pa->no[1];
						zn= pa->no[2];

						/* transpose ! */
						nor[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
						nor[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
						nor[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
						Normalize(nor);

						VECCOPY(view, vec);
						Normalize(view);

						zn= nor[0]*view[0]+nor[1]*view[1]+nor[2]*view[2];
						if(zn>=0.0) hasize= 0.0;
						else hasize*= zn*zn*zn*zn;
					}

					if(paf->stype==PAF_VECT) har= RE_inithalo(re, ma, vec, vec1, pa->co, hasize, paf->vectsize, seed);
					else {
						har= RE_inithalo(re, ma, vec, NULL, pa->co, hasize, 0.0, seed);
						if(har && (ma->mode & MA_HALO_SHADE)) {
							VecSubf(har->no, vec, vec1);
							Normalize(har->no);
							har->lay= ob->lay;
						}
					}
					if(har) har->lay= ob->lay;
				}
				else {	/* generate pixel sized hair strand */
					float strandco= 1.0f;
					
					/* last strand, texco to end */
					if(ctime + paf->staticstep < mtime)
						strandco= (ctime-pa->time)/(mtime-pa->time);
					
					static_particle_strand(re, ob, ma, orco, vec, vec1, strandco, first);
				}
			}
			
			VECCOPY(vec1, vec);
			first= 0;
		}
		
		seed++;
		if(orco) orco+=3;
	}

	if(paf->disp!=100) {
		MEM_freeN(paf->keys);
		paf->keys= NULL;
	}

	if((ma->mode & MA_TANGENT_STR)==0)
		calc_vertexnormals(re, totverto, totvlako, 0);
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
static void sort_halos(Render *re)
{
	struct halosort *hablock, *haso;
	HaloRen *har = NULL, **bloha;
	int a;

	if(re->tothalo==0) return;

	/* make datablock with halo pointers, sort */
	haso= hablock= MEM_mallocN(sizeof(struct halosort)*re->tothalo, "hablock");

	for(a=0; a<re->tothalo; a++) {
		if((a & 255)==0) har= re->bloha[a>>8];
		else har++;
		haso->har= har;
		haso->z= har->zs;
		haso++;
	}

	qsort(hablock, re->tothalo, sizeof(struct halosort), verghalo);

	/* re-assamble re->bloha */

	bloha= re->bloha;
	re->bloha= (HaloRen **)MEM_callocN(sizeof(void *)*(re->blohalen),"Bloha");

	haso= hablock;
	for(a=0; a<re->tothalo; a++) {
		har= RE_findOrAddHalo(re, a);
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

/* ------------------------------------------------------------------------- */
static void init_render_mball(Render *re, Object *ob)
{
	DispList *dl;
	VertRen *ver;
	VlakRen *vlr, *vlr1;
	Material *ma;
	float *data, *nors, mat[4][4], imat[3][3], xn, yn, zn;
	int a, need_orco, startvert, *index;

	if (ob!=find_basis_mball(ob))
		return;

	MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
	MTC_Mat4Invert(ob->imat, mat);
	MTC_Mat3CpyMat4(imat, ob->imat);

	ma= give_render_material(re, ob, 1);

	need_orco= 0;
	if(ma->texco & TEXCO_ORCO) {
		need_orco= 1;
	}
	
	makeDispListMBall(ob);
	dl= ob->disp.first;
	if(dl==0) return;

	startvert= re->totvert;
	data= dl->verts;
	nors= dl->nors;

	for(a=0; a<dl->nr; a++, data+=3, nors+=3) {

		ver= RE_findOrAddVert(re, re->totvert++);
		VECCOPY(ver->co, data);
		MTC_Mat4MulVecfl(mat, ver->co);

		xn= nors[0];
		yn= nors[1];
		zn= nors[2];

		/* transpose ! */
		ver->n[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
		ver->n[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
		ver->n[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
		Normalize(ver->n);
		//if(ob->transflag & OB_NEG_SCALE) VecMulf(ver->n. -1.0);
		
		if(need_orco) ver->orco= data;
	}

	index= dl->index;
	for(a=0; a<dl->parts; a++, index+=4) {

		vlr= RE_findOrAddVlak(re, re->totvlak++);
		vlr->ob= ob;
		vlr->v1= RE_findOrAddVert(re, startvert+index[0]);
		vlr->v2= RE_findOrAddVert(re, startvert+index[1]);
		vlr->v3= RE_findOrAddVert(re, startvert+index[2]);
		vlr->v4= 0;

		if(ob->transflag & OB_NEG_SCALE) 
			CalcNormFloat(vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->n);
		else
			CalcNormFloat(vlr->v3->co, vlr->v2->co, vlr->v1->co, vlr->n);

		vlr->mat= ma;
		vlr->flag= ME_SMOOTH+R_NOPUNOFLIP;
		vlr->ec= 0;
		vlr->lay= ob->lay;

		/* mball -too bad- always has triangles, because quads can be non-planar */
		if(index[3] && index[3]!=index[2]) {
			vlr1= RE_findOrAddVlak(re, re->totvlak++);
			*vlr1= *vlr;
			vlr1->v2= vlr1->v3;
			vlr1->v3= RE_findOrAddVert(re, startvert+index[3]);
			if(ob->transflag & OB_NEG_SCALE) 
				CalcNormFloat(vlr1->v1->co, vlr1->v2->co, vlr1->v3->co, vlr1->n);
			else
				CalcNormFloat(vlr1->v3->co, vlr1->v2->co, vlr1->v1->co, vlr1->n);
		}
	}

	if(need_orco) {
		/* store displist and scale */
		make_orco_mball(ob);
	}
	else {
		/* enforce display lists remade */
		freedisplist(&ob->disp);
	}
	
	/* this enforces remake for real, orco displist is small (in scale) */
	ob->recalc |= OB_RECALC_DATA;
}
/* ------------------------------------------------------------------------- */
/* convert */

static int vlakren_customdata_layer_num(int n, int active)
{
	/* make the active layer the first */
	if (n == active) return 0;
	else if (n < active) return n+1;
	else return n;
}

struct edgesort {
	int v1, v2;
	int f;
	int i1, i2;
};

/* edges have to be added with lowest index first for sorting */
static void to_edgesort(struct edgesort *ed, int i1, int i2, int v1, int v2, int f)
{
	if(v1>v2) {
		SWAP(int, v1, v2);
		SWAP(int, i1, i2);
	}

	ed->v1= v1;
	ed->v2= v2;
	ed->i1= i1;
	ed->i2= i2;
	ed->f = f;
}

static int vergedgesort(const void *v1, const void *v2)
{
	const struct edgesort *x1=v1, *x2=v2;
	
	if( x1->v1 > x2->v1) return 1;
	else if( x1->v1 < x2->v1) return -1;
	else if( x1->v2 > x2->v2) return 1;
	else if( x1->v2 < x2->v2) return -1;
	
	return 0;
}

static struct edgesort *make_mesh_edge_lookup(DerivedMesh *dm, int *totedgesort)
{
	MFace *mf, *mface;
	MTFace *tface=NULL;
	struct edgesort *edsort, *ed;
	unsigned int *mcol=NULL;
	int a, totedge=0, totface;
	
	mface= dm->getFaceArray(dm);
	totface= dm->getNumFaces(dm);
	tface= dm->getFaceDataArray(dm, CD_MTFACE);
	mcol= dm->getFaceDataArray(dm, CD_MCOL);
	
	if(mcol==NULL && tface==NULL) return NULL;
	
	/* make sorted table with edges and face indices in it */
	for(a= totface, mf= mface; a>0; a--, mf++) {
		if(mf->v4) totedge+=4;
		else if(mf->v3) totedge+=3;
	}

	if(totedge==0)
		return NULL;
	
	ed= edsort= MEM_callocN(totedge*sizeof(struct edgesort), "edgesort");
	
	for(a=0, mf=mface; a<totface; a++, mf++) {
		to_edgesort(ed++, 0, 1, mf->v1, mf->v2, a);
		to_edgesort(ed++, 1, 2, mf->v2, mf->v3, a);
		if(mf->v4) {
			to_edgesort(ed++, 2, 3, mf->v3, mf->v4, a);
			to_edgesort(ed++, 3, 0, mf->v4, mf->v1, a);
		}
		else if(mf->v3)
			to_edgesort(ed++, 2, 3, mf->v3, mf->v1, a);
	}
	
	qsort(edsort, totedge, sizeof(struct edgesort), vergedgesort);
	
	*totedgesort= totedge;

	return edsort;
}

static void use_mesh_edge_lookup(Render *re, DerivedMesh *dm, MEdge *medge, VlakRen *vlr, struct edgesort *edgetable, int totedge)
{
	struct edgesort ed, *edp;
	CustomDataLayer *layer;
	MTFace *mtface, *mtf;
	MCol *mcol, *mc;
	int index, mtfn, mcn, n;
	char *name;
	
	if(medge->v1 < medge->v2) {
		ed.v1= medge->v1;
		ed.v2= medge->v2;
	}
	else {
		ed.v1= medge->v2;
		ed.v2= medge->v1;
	}
	
	edp= bsearch(&ed, edgetable, totedge, sizeof(struct edgesort), vergedgesort);

	/* since edges have different index ordering, we have to duplicate mcol and tface */
	if(edp) {
		mtfn= mcn= 0;

		for(index=0; index<dm->faceData.totlayer; index++) {
			layer= &dm->faceData.layers[index];
			name= layer->name;

			if(layer->type == CD_MTFACE && mtfn < MAX_MTFACE) {
				mtface= &((MTFace*)layer->data)[edp->f];
				n= vlakren_customdata_layer_num(mtfn++, layer->active_rnd);
				mtf= RE_vlakren_get_tface(re, vlr, n, &name, 1);

				*mtf= *mtface;

				memcpy(mtf->uv[0], mtface->uv[edp->i1], sizeof(float)*2);
				memcpy(mtf->uv[1], mtface->uv[edp->i2], sizeof(float)*2);
				memcpy(mtf->uv[2], mtface->uv[1], sizeof(float)*2);
				memcpy(mtf->uv[3], mtface->uv[1], sizeof(float)*2);
			}
			else if(layer->type == CD_MCOL && mcn < MAX_MCOL) {
				mcol= &((MCol*)layer->data)[edp->f*4];
				n= vlakren_customdata_layer_num(mcn++, layer->active_rnd);
				mc= RE_vlakren_get_mcol(re, vlr, n, &name, 1);

				mc[0]= mcol[edp->i1];
				mc[1]= mc[2]= mc[3]= mcol[edp->i2];
			}
		}
	}
}

static void init_render_mesh(Render *re, Object *ob, Object *par, int only_verts)
{
	Mesh *me;
	MVert *mvert = NULL;
	MFace *mface;
	VlakRen *vlr; //, *vlr1;
	VertRen *ver;
	Material *ma;
	MSticky *ms = NULL;
	PartEff *paf;
	DerivedMesh *dm;
	float xn, yn, zn,  imat[3][3], mat[4][4];  //nor[3],
	float *orco=0;
	int a, a1, ok, need_orco=0, need_stress=0, need_tangent=0, totvlako, totverto, vertofs;
	int end, do_autosmooth=0, totvert = 0;
	int useFluidmeshNormals= 0; // NT fluidsim, use smoothed normals?
	int use_original_normals= 0;

	me= ob->data;

	paf = give_parteff(ob);
	if(paf) {
		/* warning; build_particle_system does modifier calls itself */
		if(paf->flag & PAF_STATIC) render_static_particle_system(re, ob, paf);
		else render_particle_system(re, ob, par, paf);
		if((paf->flag & PAF_SHOWE)==0) return;
	}

	MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
	MTC_Mat4Invert(ob->imat, mat);
	MTC_Mat3CpyMat4(imat, ob->imat);

	if(me->totvert==0) {
		return;
	}
	
	totvlako= re->totvlak;
	totverto= re->totvert;
	
	need_orco= 0;
	for(a=1; a<=ob->totcol; a++) {
		ma= give_render_material(re, ob, a);
		if(ma) {
			if(ma->texco & (TEXCO_ORCO|TEXCO_STRESS))
				need_orco= 1;
			if(ma->texco & TEXCO_STRESS)
				need_stress= 1;
			/* normalmaps, test if tangents needed, separated from shading */
			if ((ma->mode_l & MA_TANGENT_V) || (ma->mode_l & MA_NORMAP_TANG)) {
				need_tangent= 1;
				if(me->mtface==NULL)
					need_orco= 1;
			}
			/* radio faces need autosmooth, to separate shared vertices in corners */
			if(re->r.mode & R_RADIO)
				if(ma->mode & MA_RADIO) 
					do_autosmooth= 1;
		}
	}
	
	/* check autosmooth and displacement, we then have to skip only-verts optimize */
	do_autosmooth |= (me->flag & ME_AUTOSMOOTH);
	if(do_autosmooth)
		only_verts= 0;
	if(test_for_displace(re, ob ) )
		only_verts= 0;
	
	if(!only_verts)
		if(need_orco) orco = get_object_orco(re, ob);

	dm = mesh_create_derived_render(ob,
	                    CD_MASK_BAREMESH | CD_MASK_MTFACE | CD_MASK_MCOL);
	
	if(dm==NULL) return;	/* in case duplicated object fails? */

	if((ob->fluidsimFlag & OB_FLUIDSIM_ENABLE) &&
		 (ob->fluidsimSettings->type & OB_FLUIDSIM_DOMAIN)&&
	   (ob->fluidsimSettings->meshSurface) ) {
		useFluidmeshNormals = 1;
	}

	mvert= dm->getVertArray(dm);
	totvert= dm->getNumVerts(dm);

	/* attempt to autsmooth on original mesh, only without subsurf */
	if(do_autosmooth && me->totvert==totvert && me->totface==dm->getNumFaces(dm))
		use_original_normals= 1;
	
	ms = (totvert==me->totvert)?me->msticky:NULL;
	
	ma= give_render_material(re, ob, 1);

	if(ma->mode & MA_HALO) {
		make_render_halos(re, ob, me, totvert, mvert, ma, orco);
	}
	else {

		for(a=0; a<totvert; a++, mvert++) {
			ver= RE_findOrAddVert(re, re->totvert++);
			VECCOPY(ver->co, mvert->co);
			if(do_autosmooth==0)	/* autosmooth on original unrotated data to prevent differences between frames */
				MTC_Mat4MulVecfl(mat, ver->co);

			if(useFluidmeshNormals) {
				xn = mvert->no[0]/ 32767.0;
				yn = mvert->no[1]/ 32767.0;
				zn = mvert->no[2]/ 32767.0;
				/* transfor to cam  space */
				ver->n[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
				ver->n[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
				ver->n[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
			} // useFluidmeshNormals

			if(orco) {
				ver->orco= orco;
				orco+=3;
			}
			if(ms) {
				float *sticky= RE_vertren_get_sticky(re, ver, 1);
				sticky[0]= ms->co[0];
				sticky[1]= ms->co[1];
				ms++;
			}
		}
		
		if(!only_verts) {
			/* store customdata names, because DerivedMesh is freed */
			RE_vlakren_set_customdata_names(re, &dm->faceData);
			
			/* still to do for keys: the correct local texture coordinate */

			/* faces in order of color blocks */
			vertofs= re->totvert - totvert;
			for(a1=0; (a1<ob->totcol || (a1==0 && ob->totcol==0)); a1++) {

				ma= give_render_material(re, ob, a1+1);
				
				/* test for 100% transparant */
				ok= 1;
				if(ma->alpha==0.0 && ma->spectra==0.0) {
					ok= 0;
					/* texture on transparency? */
					for(a=0; a<MAX_MTEX; a++) {
						if(ma->mtex[a] && ma->mtex[a]->tex) {
							if(ma->mtex[a]->mapto & MAP_ALPHA) ok= 1;
						}
					}
				}
				
				/* if wire material, and we got edges, don't do the faces */
				if(ma->mode & MA_WIRE) {
					end= dm->getNumEdges(dm);
					if(end) ok= 0;
				}

				if(ok) {
					end= dm->getNumFaces(dm);
					mface= dm->getFaceArray(dm);

					for(a=0; a<end; a++, mface++) {
						int v1, v2, v3, v4, flag;
						
						if( mface->mat_nr==a1 ) {
							float len;
								
							v1= mface->v1;
							v2= mface->v2;
							v3= mface->v3;
							v4= mface->v4;
							flag= mface->flag & ME_SMOOTH;
							
							vlr= RE_findOrAddVlak(re, re->totvlak++);
							vlr->ob= ob;
							vlr->v1= RE_findOrAddVert(re, vertofs+v1);
							vlr->v2= RE_findOrAddVert(re, vertofs+v2);
							vlr->v3= RE_findOrAddVert(re, vertofs+v3);
							if(v4) vlr->v4= RE_findOrAddVert(re, vertofs+v4);
							else vlr->v4= 0;

							/* render normals are inverted in render */
							if(use_original_normals) {
								MFace *mf= me->mface+a;
								MVert *mv= me->mvert;
								
								if(vlr->v4) 
									len= CalcNormFloat4( mv[mf->v4].co, mv[mf->v3].co, mv[mf->v2].co, mv[mf->v1].co, vlr->n);
								else 
									len= CalcNormFloat(mv[mf->v3].co, mv[mf->v2].co, mv[mf->v1].co, vlr->n);
							}
							else {
								if(vlr->v4) 
									len= CalcNormFloat4(vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co, vlr->n);
								else 
									len= CalcNormFloat(vlr->v3->co, vlr->v2->co, vlr->v1->co, vlr->n);
							}

							vlr->mat= ma;
							vlr->flag= flag;
							if((me->flag & ME_NOPUNOFLIP) ) {
								vlr->flag |= R_NOPUNOFLIP;
							}
							vlr->ec= 0; /* mesh edges rendered separately */
							vlr->lay= ob->lay;

							if(len==0) re->totvlak--;
							else {
								CustomDataLayer *layer;
								MTFace *mtface, *mtf;
								MCol *mcol, *mc;
								int index, mtfn= 0, mcn= 0, n;
								char *name;

								for(index=0; index<dm->faceData.totlayer; index++) {
									layer= &dm->faceData.layers[index];
									name= layer->name;
									
									if(layer->type == CD_MTFACE && mtfn < MAX_MTFACE) {
										n= vlakren_customdata_layer_num(mtfn++, layer->active_rnd);
										mtf= RE_vlakren_get_tface(re, vlr, n, &name, 1);
										mtface= (MTFace*)layer->data;
										*mtf= mtface[a];
									}
									else if(layer->type == CD_MCOL && mcn < MAX_MCOL) {
										n= vlakren_customdata_layer_num(mcn++, layer->active_rnd);
										mc= RE_vlakren_get_mcol(re, vlr, n, &name, 1);
										mcol= (MCol*)layer->data;
										memcpy(mc, &mcol[a*4], sizeof(MCol)*4);
									}
								}
							}
						}
					}
				}
			}
			
			/* exception... we do edges for wire mode. potential conflict when faces exist... */
			end= dm->getNumEdges(dm);
			mvert= dm->getVertArray(dm);
			ma= give_render_material(re, ob, 1);
			if(end && (ma->mode & MA_WIRE)) {
				MEdge *medge;
				struct edgesort *edgetable;
				int totedge= 0;
				
				medge= dm->getEdgeArray(dm);
				
				/* we want edges to have UV and vcol too... */
				edgetable= make_mesh_edge_lookup(dm, &totedge);
				
				for(a1=0; a1<end; a1++, medge++) {
					if (medge->flag&ME_EDGERENDER) {
						MVert *v0 = &mvert[medge->v1];
						MVert *v1 = &mvert[medge->v2];

						vlr= RE_findOrAddVlak(re, re->totvlak++);
						vlr->ob= ob;
						vlr->v1= RE_findOrAddVert(re, vertofs+medge->v1);
						vlr->v2= RE_findOrAddVert(re, vertofs+medge->v2);
						vlr->v3= vlr->v2;
						vlr->v4= NULL;
						
						if(edgetable) {
							use_mesh_edge_lookup(re, dm, medge, vlr, edgetable, totedge);
						}
						
						xn= -(v0->no[0]+v1->no[0]);
						yn= -(v0->no[1]+v1->no[1]);
						zn= -(v0->no[2]+v1->no[2]);
						/* transpose ! */
						vlr->n[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
						vlr->n[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
						vlr->n[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
						Normalize(vlr->n);
						
						vlr->mat= ma;
						vlr->flag= 0;
						vlr->ec= ME_V1V2;
						vlr->lay= ob->lay;
					}
				}
				if(edgetable)
					MEM_freeN(edgetable);
			}
		}
	}
	
	if(!only_verts) {
		if (test_for_displace(re, ob ) ) {
			calc_vertexnormals(re, totverto, totvlako, 0);
			do_displacement(re, ob, totvlako, re->totvlak-totvlako, totverto, re->totvert-totverto);
		}

		if(do_autosmooth) {
			autosmooth(re, mat, totverto, totvlako, me->smoothresh);
		}

		if(useFluidmeshNormals) {
			// do not recalculate, only init render data
			calc_fluidsimnormals(re, totverto, totvlako, need_tangent);
		} else {
			calc_vertexnormals(re, totverto, totvlako, need_tangent);
		}

		if(need_stress)
			calc_edge_stress(re, me, totverto, totvlako);
	}

	dm->release(dm);
}

/* ------------------------------------------------------------------------- */

static void initshadowbuf(Render *re, LampRen *lar, float mat[][4])
{
	struct ShadBuf *shb;
	float viewinv[4][4];
	
	/* if(la->spsi<16) return; */
	
	/* memory alloc */
	shb= (struct ShadBuf *)MEM_callocN( sizeof(struct ShadBuf),"initshadbuf");
	lar->shb= shb;
	
	if(shb==NULL) return;
	
	VECCOPY(shb->co, lar->co);
	
	/* percentage render: keep track of min and max */
	shb->size= (lar->bufsize*re->r.size)/100;
	
	if(lar->buffers>1) shb->size/= 2;
	
	if(shb->size<512) shb->size= 512;
	else if(shb->size > lar->bufsize) shb->size= lar->bufsize;
	
	shb->size &= ~15;	/* make sure its multiples of 16 */
	
	shb->samp= lar->samp;
	shb->soft= lar->soft;
	shb->shadhalostep= lar->shadhalostep;
	
	MTC_Mat4Ortho(mat);
	MTC_Mat4Invert(shb->winmat, mat);	/* winmat is temp */
	
	/* matrix: combination of inverse view and lampmat */
	/* calculate again: the ortho-render has no correct viewinv */
	MTC_Mat4Invert(viewinv, re->viewmat);
	MTC_Mat4MulMat4(shb->viewmat, viewinv, shb->winmat);
	
	/* projection */
	shb->d= lar->clipsta;
	shb->clipend= lar->clipend;
	
	/* bias is percentage, made 2x larger because of correction for angle of incidence */
	/* when a ray is closer to parallel of a face, bias value is increased during render */
	shb->bias= (0.02*lar->bias)*0x7FFFFFFF;
	shb->bias= shb->bias*(100/re->r.size);
	
	/* halfway method (average of first and 2nd z) reduces bias issues */
	if(lar->buftype==LA_SHADBUF_HALFWAY)
		shb->bias= 0.1f*shb->bias;
	
}


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
static GroupObject *add_render_lamp(Render *re, Object *ob)
{
	Lamp *la= ob->data;
	LampRen *lar;
	GroupObject *go;
	float mat[4][4], angle, xn, yn;
	int c;

	/* previewrender sets this to zero... prevent accidents */
	if(la==NULL) return NULL;
	
	/* prevent only shadow from rendering light */
	if(la->mode & LA_ONLYSHADOW)
		if((re->r.mode & R_SHADOW)==0)
			return NULL;
	
	re->totlamp++;
	
	/* groups is used to unify support for lightgroups, this is the global lightgroup */
	go= MEM_callocN(sizeof(GroupObject), "groupobject");
	BLI_addtail(&re->lights, go);
	go->ob= ob;
	/* lamprens are in own list, for freeing */
	lar= (LampRen *)MEM_callocN(sizeof(LampRen),"lampren");
	BLI_addtail(&re->lampren, lar);
	go->lampren= lar;

	MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
	MTC_Mat4Invert(ob->imat, mat);

	MTC_Mat3CpyMat4(lar->mat, mat);
	MTC_Mat3CpyMat4(lar->imat, ob->imat);

	lar->bufsize = la->bufsize;
	lar->samp = la->samp;
	lar->buffers= la->buffers;
	if(lar->buffers==0) lar->buffers= 1;
	lar->buftype= la->buftype;
	lar->filtertype= la->filtertype;
	lar->soft = la->soft;
	lar->shadhalostep = la->shadhalostep;
	lar->clipsta = la->clipsta;
	lar->clipend = la->clipend;
	
	lar->bias = la->bias;

	lar->type= la->type;
	lar->mode= la->mode;

	lar->energy= la->energy;
	if(la->mode & LA_NEG) lar->energy= -lar->energy;

	lar->vec[0]= -mat[2][0];
	lar->vec[1]= -mat[2][1];
	lar->vec[2]= -mat[2][2];
	Normalize(lar->vec);
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
	lar->ray_samp_method= la->ray_samp_method;
	lar->ray_samp_type= la->ray_samp_type;
	
	lar->adapt_thresh= la->adapt_thresh;
	
	if( ELEM3(lar->type, LA_SPOT, LA_SUN, LA_LOCAL)) {
		lar->ray_totsamp= lar->ray_samp*lar->ray_samp;
		lar->area_shape = LA_AREA_SQUARE;
		lar->area_sizey= lar->area_size;
	}
	else if(lar->type==LA_AREA) {
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
	
#ifndef DISABLE_YAFRAY
	/* yafray: photonlight and other params */
	if (re->r.renderer==R_YAFRAY) {
		lar->YF_numphotons = la->YF_numphotons;
		lar->YF_numsearch = la->YF_numsearch;
		lar->YF_phdepth = la->YF_phdepth;
		lar->YF_useqmc = la->YF_useqmc;
		lar->YF_causticblur = la->YF_causticblur;
		lar->YF_ltradius = la->YF_ltradius;
		lar->YF_bufsize = la->YF_bufsize;
		lar->YF_glowint = la->YF_glowint;
		lar->YF_glowofs = la->YF_glowofs;
		lar->YF_glowtype = la->YF_glowtype;
	}
#endif /* disable yafray */

	lar->spotsi= la->spotsize;
	if(lar->mode & LA_HALO) {
		if(lar->spotsi>170.0) lar->spotsi= 170.0;
	}
	lar->spotsi= cos( M_PI*lar->spotsi/360.0 );
	lar->spotbl= (1.0-lar->spotsi)*la->spotblend;

	memcpy(lar->mtex, la->mtex, MAX_MTEX*sizeof(void *));

	lar->lay= ob->lay & 0xFFFFFF;	// higher 8 bits are localview layers

	lar->falloff_type = la->falloff_type;
	lar->ld1= la->att1;
	lar->ld2= la->att2;
	lar->curfalloff = curvemapping_copy(la->curfalloff);

	if(lar->type==LA_SPOT) {

		Normalize(lar->imat[0]);
		Normalize(lar->imat[1]);
		Normalize(lar->imat[2]);

		xn= saacos(lar->spotsi);
		xn= sin(xn)/cos(xn);
		lar->spottexfac= 1.0/(xn);

		if(lar->mode & LA_ONLYSHADOW) {
			if((lar->mode & (LA_SHAD_BUF|LA_SHAD_RAY))==0) lar->mode -= LA_ONLYSHADOW;
		}

	}

	/* set flag for spothalo en initvars */
	if(la->type==LA_SPOT && (la->mode & LA_HALO)) {
		if(la->haint>0.0) {
			re->flag |= R_LAMPHALO;

			/* camera position (0,0,0) rotate around lamp */
			lar->sh_invcampos[0]= -lar->co[0];
			lar->sh_invcampos[1]= -lar->co[1];
			lar->sh_invcampos[2]= -lar->co[2];
			MTC_Mat3MulVecfl(lar->imat, lar->sh_invcampos);

			/* z factor, for a normalized volume */
			angle= saacos(lar->spotsi);
			xn= lar->spotsi;
			yn= sin(angle);
			lar->sh_zfac= yn/xn;
			/* pre-scale */
			lar->sh_invcampos[2]*= lar->sh_zfac;

		}
	}
	else if(la->type==LA_HEMI) {
		lar->mode &= ~(LA_SHAD_RAY|LA_SHAD_BUF);
	}

	for(c=0; c<MAX_MTEX; c++) {
		if(la->mtex[c] && la->mtex[c]->tex) {
			lar->mode |= LA_TEXTURE;

			if(G.rendering) {
				if(re->osa) {
					if(la->mtex[c]->tex->type==TEX_IMAGE) lar->mode |= LA_OSATEX;
				}
			}
		}
	}
	/* yafray: shadow flag should not be cleared, only used with internal renderer */
	if (re->r.renderer==R_INTERN) {
		/* to make sure we can check ray shadow easily in the render code */
		if(lar->mode & LA_SHAD_RAY) {
			if( (re->r.mode & R_RAYTRACE)==0)
				lar->mode &= ~LA_SHAD_RAY;
		}
	

		if(re->r.mode & R_SHADOW) {
			
			if ((lar->mode & LA_SHAD_RAY) && (lar->ray_samp_method == LA_SAMP_HAMMERSLEY)) {
				init_lamp_hammersley(lar);
			}
			if(la->type==LA_AREA && (lar->mode & LA_SHAD_RAY) && (lar->ray_samp_method == LA_SAMP_CONSTANT)) {
				init_jitter_plane(lar);
			}
			else if (la->type==LA_SPOT && (lar->mode & LA_SHAD_BUF) ) {
				/* Per lamp, one shadow buffer is made. */
				lar->bufflag= la->bufflag;
				Mat4CpyMat4(mat, ob->obmat);
				initshadowbuf(re, lar, mat);	// mat is altered
			}
			
			
			/* this is the way used all over to check for shadow */
			if(lar->shb || (lar->mode & LA_SHAD_RAY)) {
				LampShadowSubSample *lss;
				int a, b, tot= re->r.threads*re->r.osa;
				
				lar->shadsamp= MEM_mallocN(re->r.threads*sizeof(LampShadowSample), "lamp shadow sample");
				lss= lar->shadsamp[0].s;
				/* shadfacs actually mean light, let's put them to 1 to prevent unitialized accidents */
				for(a=0; a<tot; a++, lss++) {
					for(b=0; b<4; b++) {
						lss->samplenr= -1;	/* used to detect whether we store or read */
						lss->shadfac[b]= 1.0f;
					}
				}
			}
		}
	}
	
	return go;
}

/* ------------------------------------------------------------------------- */

/* returns amount of vertices added for orco */
static int dl_surf_to_renderdata(Render *re, Object *ob, DispList *dl, Material **matar, float *orco, float mat[4][4])
{
	VertRen *v1, *v2, *v3, *v4, *ver;
	VlakRen *vlr, *vlr1, *vlr2, *vlr3;
	Curve *cu= ob->data;
	float *data, n1[3], flen;
	int u, v, orcoret= 0;
	int p1, p2, p3, p4, a;
	int sizeu, nsizeu, sizev, nsizev;
	int startvert, startvlak;
	
	startvert= re->totvert;
	nsizeu = sizeu = dl->parts; nsizev = sizev = dl->nr; 
	
	data= dl->verts;
	for (u = 0; u < sizeu; u++) {
		v1 = RE_findOrAddVert(re, re->totvert++); /* save this for possible V wrapping */
		VECCOPY(v1->co, data); data += 3;
		if(orco) {
			v1->orco= orco; orco+= 3; orcoret++;
		}	
		MTC_Mat4MulVecfl(mat, v1->co);
		
		for (v = 1; v < sizev; v++) {
			ver= RE_findOrAddVert(re, re->totvert++);
			VECCOPY(ver->co, data); data += 3;
			if(orco) {
				ver->orco= orco; orco+= 3; orcoret++;
			}	
			MTC_Mat4MulVecfl(mat, ver->co);
		}
		/* if V-cyclic, add extra vertices at end of the row */
		if (dl->flag & DL_CYCL_U) {
			ver= RE_findOrAddVert(re, re->totvert++);
			VECCOPY(ver->co, v1->co);
			if(orco) {
				ver->orco= orco; orco+=3; orcoret++; //orcobase + 3*(u*sizev + 0);
			}
		}	
	}	
	
	/* Done before next loop to get corner vert */
	if (dl->flag & DL_CYCL_U) nsizev++;
	if (dl->flag & DL_CYCL_V) nsizeu++;
	
	/* if U cyclic, add extra row at end of column */
	if (dl->flag & DL_CYCL_V) {
		for (v = 0; v < nsizev; v++) {
			v1= RE_findOrAddVert(re, startvert + v);
			ver= RE_findOrAddVert(re, re->totvert++);
			VECCOPY(ver->co, v1->co);
			if(orco) {
				ver->orco= orco; orco+=3; orcoret++; //ver->orco= orcobase + 3*(0*sizev + v);
			}
		}
	}
	
	sizeu = nsizeu;
	sizev = nsizev;
	
	startvlak= re->totvlak;
	
	for(u = 0; u < sizeu - 1; u++) {
		p1 = startvert + u * sizev; /* walk through face list */
		p2 = p1 + 1;
		p3 = p2 + sizev;
		p4 = p3 - 1;
		
		for(v = 0; v < sizev - 1; v++) {
			v1= RE_findOrAddVert(re, p1);
			v2= RE_findOrAddVert(re, p2);
			v3= RE_findOrAddVert(re, p3);
			v4= RE_findOrAddVert(re, p4);
			
			vlr= RE_findOrAddVlak(re, re->totvlak++);
			vlr->ob= ob;
			vlr->v1= v1; vlr->v2= v2; vlr->v3= v3; vlr->v4= v4;
			
			flen= CalcNormFloat4(vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co, n1);
			VECCOPY(vlr->n, n1);
			
			vlr->lay= ob->lay;
			vlr->mat= matar[ dl->col];
			vlr->ec= ME_V1V2+ME_V2V3;
			vlr->flag= dl->rt;
			if( (cu->flag & CU_NOPUNOFLIP) ) {
				vlr->flag |= R_NOPUNOFLIP;
			}
			
			VecAddf(v1->n, v1->n, n1);
			VecAddf(v2->n, v2->n, n1);
			VecAddf(v3->n, v3->n, n1);
			VecAddf(v4->n, v4->n, n1);
			
			p1++; p2++; p3++; p4++;
		}
	}	
	/* fix normals for U resp. V cyclic faces */
	sizeu--; sizev--;  /* dec size for face array */
	if (dl->flag & DL_CYCL_V) {
		
		for (v = 0; v < sizev; v++)
		{
			/* optimize! :*/
			vlr= RE_findOrAddVlak(re, UVTOINDEX(sizeu - 1, v));
			vlr1= RE_findOrAddVlak(re, UVTOINDEX(0, v));
			VecAddf(vlr1->v1->n, vlr1->v1->n, vlr->n);
			VecAddf(vlr1->v2->n, vlr1->v2->n, vlr->n);
			VecAddf(vlr->v3->n, vlr->v3->n, vlr1->n);
			VecAddf(vlr->v4->n, vlr->v4->n, vlr1->n);
		}
	}
	if (dl->flag & DL_CYCL_U) {
		
		for (u = 0; u < sizeu; u++)
		{
			/* optimize! :*/
			vlr= RE_findOrAddVlak(re, UVTOINDEX(u, 0));
			vlr1= RE_findOrAddVlak(re, UVTOINDEX(u, sizev-1));
			VecAddf(vlr1->v2->n, vlr1->v2->n, vlr->n);
			VecAddf(vlr1->v3->n, vlr1->v3->n, vlr->n);
			VecAddf(vlr->v1->n, vlr->v1->n, vlr1->n);
			VecAddf(vlr->v4->n, vlr->v4->n, vlr1->n);
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
	
	if ((dl->flag & DL_CYCL_V) && (dl->flag & DL_CYCL_U))
	{
		vlr= RE_findOrAddVlak(re, UVTOINDEX(sizeu - 1, sizev - 1)); /* (m,n) */
		vlr1= RE_findOrAddVlak(re, UVTOINDEX(0,0));  /* (0,0) */
		VecAddf(n1, vlr->n, vlr1->n);
		vlr2= RE_findOrAddVlak(re, UVTOINDEX(0, sizev-1)); /* (0,n) */
		VecAddf(n1, n1, vlr2->n);
		vlr3= RE_findOrAddVlak(re, UVTOINDEX(sizeu-1, 0)); /* (m,0) */
		VecAddf(n1, n1, vlr3->n);
		VECCOPY(vlr->v3->n, n1);
		VECCOPY(vlr1->v1->n, n1);
		VECCOPY(vlr2->v2->n, n1);
		VECCOPY(vlr3->v4->n, n1);
	}
	for(a = startvert; a < re->totvert; a++) {
		ver= RE_findOrAddVert(re, a);
		Normalize(ver->n);
	}
	
	
	return orcoret;
}

static void init_render_surf(Render *re, Object *ob)
{
	Nurb *nu=0;
	Curve *cu;
	ListBase displist;
	DispList *dl;
	Material *matar[32];
	float *orco=NULL, *orcobase=NULL, mat[4][4];
	int a, need_orco=0;

	cu= ob->data;
	nu= cu->nurb.first;
	if(nu==0) return;

	MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
	MTC_Mat4Invert(ob->imat, mat);

	/* material array */
	memset(matar, 0, 4*32);
	matar[0]= give_render_material(re, ob, 0);
	for(a=0; a<ob->totcol; a++) {
		matar[a]= give_render_material(re, ob, a+1);
		if(matar[a] && matar[a]->texco & TEXCO_ORCO) {
			need_orco= 1;
		}
	}

	if(ob->parent && (ob->parent->type==OB_LATTICE)) need_orco= 1;

	if(need_orco) orcobase= orco= get_object_orco(re, ob);

	displist.first= displist.last= 0;
	makeDispListSurf(ob, &displist, 1);

	dl= displist.first;
	/* walk along displaylist and create rendervertices/-faces */
	while(dl) {
			/* watch out: u ^= y, v ^= x !! */
		if(dl->type==DL_SURF) {
			orco+= 3*dl_surf_to_renderdata(re, ob, dl, matar, orco, mat);
		}

		dl= dl->next;
	}
	freedisplist(&displist);
}

static void init_render_curve(Render *re, Object *ob, int only_verts)
{
	Curve *cu;
	VertRen *ver;
	VlakRen *vlr;
	DispList *dl;
	ListBase olddl={NULL, NULL};
	Material *matar[32];
	float len, *data, *fp, *orco=NULL, *orcobase= NULL;
	float n[3], mat[4][4];
	int nr, startvert, startvlak, a, b;
	int frontside, need_orco=0;

	cu= ob->data;
	if(cu->nurb.first==NULL) return;

	/* no modifier call here, is in makedisp */

	if(cu->resolu_ren) 
		SWAP(ListBase, olddl, cu->disp);
	
	/* test displist */
	if(cu->disp.first==NULL) 
		makeDispListCurveTypes(ob, 0);
	dl= cu->disp.first;
	if(cu->disp.first==NULL) return;
	
	MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
	MTC_Mat4Invert(ob->imat, mat);

	/* material array */
	memset(matar, 0, 4*32);
	matar[0]= give_render_material(re, ob, 0);
	for(a=0; a<ob->totcol; a++) {
		matar[a]= give_render_material(re, ob, a+1);
		if(matar[a]->texco & TEXCO_ORCO) {
			need_orco= 1;
		}
	}

	if(need_orco) orcobase=orco= get_object_orco(re, ob);

	dl= cu->disp.first;
	while(dl) {
		if(dl->type==DL_INDEX3) {
			int *index;

			startvert= re->totvert;
			data= dl->verts;

			n[0]= ob->imat[0][2];
			n[1]= ob->imat[1][2];
			n[2]= ob->imat[2][2];
			Normalize(n);

			for(a=0; a<dl->nr; a++, data+=3) {
				ver= RE_findOrAddVert(re, re->totvert++);
				VECCOPY(ver->co, data);

				/* flip normal if face is backfacing, also used in face loop below */
				if(ver->co[2] < 0.0) {
					VECCOPY(ver->n, n);
					ver->flag = 1;
				}
				else {
					ver->n[0]= -n[0]; ver->n[1]= -n[1]; ver->n[2]= -n[2];
					ver->flag = 0;
				}

				MTC_Mat4MulVecfl(mat, ver->co);
				
				if (orco) {
					ver->orco = orco;
					orco += 3;
				}
			}
			
			if(only_verts==0) {
				startvlak= re->totvlak;
				index= dl->index;
				for(a=0; a<dl->parts; a++, index+=3) {

					vlr= RE_findOrAddVlak(re, re->totvlak++);
					vlr->ob = ob;
					vlr->v1= RE_findOrAddVert(re, startvert+index[0]);
					vlr->v2= RE_findOrAddVert(re, startvert+index[1]);
					vlr->v3= RE_findOrAddVert(re, startvert+index[2]);
					vlr->v4= NULL;
					
					if(vlr->v1->flag) {
						VECCOPY(vlr->n, n);
					}
					else {
						vlr->n[0]= -n[0]; vlr->n[1]= -n[1]; vlr->n[2]= -n[2];
					}
					
					vlr->mat= matar[ dl->col ];
					vlr->flag= 0;
					if( (cu->flag & CU_NOPUNOFLIP) ) {
						vlr->flag |= R_NOPUNOFLIP;
					}
					vlr->ec= 0;
					vlr->lay= ob->lay;
				}
			}
		}
		else if (dl->type==DL_SURF) {
			
			/* cyclic U means an extruded full circular curve, we skip bevel splitting then */
			if (dl->flag & DL_CYCL_U) {
				orco+= 3*dl_surf_to_renderdata(re, ob, dl, matar, orco, mat);
			}
			else {
				int p1,p2,p3,p4;

				fp= dl->verts;
				startvert= re->totvert;
				nr= dl->nr*dl->parts;

				while(nr--) {
					ver= RE_findOrAddVert(re, re->totvert++);
						
					VECCOPY(ver->co, fp);
					MTC_Mat4MulVecfl(mat, ver->co);
					fp+= 3;

					if (orco) {
						ver->orco = orco;
						orco += 3;
					}
				}

				if(dl->bevelSplitFlag || only_verts==0) {
					startvlak= re->totvlak;

					for(a=0; a<dl->parts; a++) {

						frontside= (a >= dl->nr/2);

						DL_SURFINDEX(dl->flag & DL_CYCL_U, dl->flag & DL_CYCL_V, dl->nr, dl->parts);
						p1+= startvert;
						p2+= startvert;
						p3+= startvert;
						p4+= startvert;

						for(; b<dl->nr; b++) {
							vlr= RE_findOrAddVlak(re, re->totvlak++);
							vlr->ob= ob;
							vlr->v1= RE_findOrAddVert(re, p2);
							vlr->v2= RE_findOrAddVert(re, p1);
							vlr->v3= RE_findOrAddVert(re, p3);
							vlr->v4= RE_findOrAddVert(re, p4);
							vlr->ec= ME_V2V3+ME_V3V4;
							if(a==0) vlr->ec+= ME_V1V2;

							vlr->flag= dl->rt;
							vlr->lay= ob->lay;

							/* this is not really scientific: the vertices
								* 2, 3 en 4 seem to give better vertexnormals than 1 2 3:
								* front and backside treated different!!
								*/

							if(frontside)
								CalcNormFloat(vlr->v2->co, vlr->v3->co, vlr->v4->co, vlr->n);
							else 
								CalcNormFloat(vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->n);

							vlr->mat= matar[ dl->col ];

							p4= p3;
							p3++;
							p2= p1;
							p1++;
						}
					}

					if (dl->bevelSplitFlag) {
						for(a=0; a<dl->parts-1+!!(dl->flag&DL_CYCL_V); a++)
							if(dl->bevelSplitFlag[a>>5]&(1<<(a&0x1F)))
								split_v_renderfaces(re, startvlak, startvert, dl->parts, dl->nr, a, dl->flag&DL_CYCL_V, dl->flag&DL_CYCL_U);
					}

					/* vertex normals */
					for(a= startvlak; a<re->totvlak; a++) {
						vlr= RE_findOrAddVlak(re, a);

						VecAddf(vlr->v1->n, vlr->v1->n, vlr->n);
						VecAddf(vlr->v3->n, vlr->v3->n, vlr->n);
						VecAddf(vlr->v2->n, vlr->v2->n, vlr->n);
						VecAddf(vlr->v4->n, vlr->v4->n, vlr->n);
					}
					for(a=startvert; a<re->totvert; a++) {
						ver= RE_findOrAddVert(re, a);
						len= Normalize(ver->n);
						if(len==0.0) ver->flag= 1;	/* flag abuse, its only used in zbuf now  */
						else ver->flag= 0;
					}
					for(a= startvlak; a<re->totvlak; a++) {
						vlr= RE_findOrAddVlak(re, a);
						if(vlr->v1->flag) VECCOPY(vlr->v1->n, vlr->n);
						if(vlr->v2->flag) VECCOPY(vlr->v2->n, vlr->n);
						if(vlr->v3->flag) VECCOPY(vlr->v3->n, vlr->n);
						if(vlr->v4->flag) VECCOPY(vlr->v4->n, vlr->n);
					}
				}
			}
		}

		dl= dl->next;
	}
	
	/* not very elegant... but we want original displist in UI */
	if(cu->resolu_ren) {
		freedisplist(&cu->disp);
		SWAP(ListBase, olddl, cu->disp);
	}
}

/* prevent phong interpolation for giving ray shadow errors (terminator problem) */
static void set_phong_threshold(Render *re, Object *ob, int startface, int numface, int startvert, int numvert )
{
//	VertRen *ver;
	VlakRen *vlr;
	float thresh= 0.0, dot;
	int tot=0, i;
	
	/* Added check for 'pointy' situations, only dotproducts of 0.9 and larger 
	   are taken into account. This threshold is meant to work on smooth geometry, not
	   for extreme cases (ton) */
	
	for(i=startface; i<startface+numface; i++) {
		vlr= RE_findOrAddVlak(re, i);
		if(vlr->flag & R_SMOOTH) {
			dot= INPR(vlr->n, vlr->v1->n);
			dot= ABS(dot);
			if(dot>0.9) {
				thresh+= dot; tot++;
			}
			dot= INPR(vlr->n, vlr->v2->n);
			dot= ABS(dot);
			if(dot>0.9) {
				thresh+= dot; tot++;
			}

			dot= INPR(vlr->n, vlr->v3->n);
			dot= ABS(dot);
			if(dot>0.9) {
				thresh+= dot; tot++;
			}

			if(vlr->v4) {
				dot= INPR(vlr->n, vlr->v4->n);
				dot= ABS(dot);
				if(dot>0.9) {
					thresh+= dot; tot++;
				}
			}
		}
	}
	
	if(tot) {
		thresh/= (float)tot;
		ob->smoothresh= cos(0.5*M_PI-saacos(thresh));
	}
}

/* par = pointer to duplicator parent, needed for object lookup table */
/* index = when duplicater copies same object (particle), the counter */
static void init_render_object(Render *re, Object *ob, Object *par, int index, int only_verts)
{
	static double lasttime= 0.0;
	double time;
	float mat[4][4];
	int startface, startvert;
	
	startface=re->totvlak;
	startvert=re->totvert;

	ob->flag |= OB_DONE;

	if(ob->type==OB_LAMP)
		add_render_lamp(re, ob);
	else if ELEM(ob->type, OB_FONT, OB_CURVE)
		init_render_curve(re, ob, only_verts);
	else if(ob->type==OB_SURF)
		init_render_surf(re, ob);
	else if(ob->type==OB_MESH)
		init_render_mesh(re, ob, par, only_verts);
	else if(ob->type==OB_MBALL)
		init_render_mball(re, ob);
	else {
		MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
		MTC_Mat4Invert(ob->imat, mat);
	}
	
	/* generic post process here */
	if(startvert!=re->totvert) {
		
		RE_addRenderObject(re, ob, par, index, startvert, re->totvert, startface, re->totvlak);
		
		/* the exception below is because displace code now is in init_render_mesh call, 
		I will look at means to have autosmooth enabled for all object types 
		and have it as general postprocess, like displace */
		if (ob->type!=OB_MESH && test_for_displace(re, ob ) ) 
			do_displacement(re, ob, startface, re->totvlak-startface, startvert, re->totvert-startvert);
	
		/* phong normal interpolation can cause error in tracing (terminator prob) */
		ob->smoothresh= 0.0;
		if( (re->r.mode & R_RAYTRACE) && (re->r.mode & R_SHADOW) ) 
			set_phong_threshold(re, ob, startface, re->totvlak-startface, startvert, re->totvert-startvert);
	}
	
	time= PIL_check_seconds_timer();
	if(time - lasttime > 1.0) {
		lasttime= time;
		/* clumsy copying still */
		re->i.totvert= re->totvert;
		re->i.totface= re->totvlak;
		re->i.tothalo= re->tothalo;
		re->i.totlamp= re->totlamp;
		re->stats_draw(&re->i);
	}
}

void RE_Database_Free(Render *re)
{
	Object *ob = NULL;
	LampRen *lar;

	/* FREE */
	
	for(lar= re->lampren.first; lar; lar= lar->next) {
		freeshadowbuf(lar);
		if(lar->jitter) MEM_freeN(lar->jitter);
		if(lar->shadsamp) MEM_freeN(lar->shadsamp);
		if(lar->qsa) free_lamp_qmcsampler(lar);
		curvemapping_free(lar->curfalloff);
	}
	
	BLI_freelistN(&re->lampren);
	BLI_freelistN(&re->lights);

	free_renderdata_tables(re);
	
	/* free orco. check all objects because of duplis and sets */
	ob= G.main->object.first;
	while(ob) {
		if(ob->type==OB_MBALL) {
			if(ob->disp.first && ob->disp.first!=ob->disp.last) {
				DispList *dl= ob->disp.first;
				BLI_remlink(&ob->disp, dl);
				freedisplist(&ob->disp);
				BLI_addtail(&ob->disp, dl);
			}
		}
		ob= ob->id.next;
	}

	free_mesh_orco_hash(re);

	end_radio_render();
	end_render_materials();
	
	if(re->wrld.aosphere) {
		MEM_freeN(re->wrld.aosphere);
		re->wrld.aosphere= NULL;
		re->scene->world->aosphere= NULL;
	}
	if(re->wrld.aotables) {
		MEM_freeN(re->wrld.aotables);
		re->wrld.aotables= NULL;
		re->scene->world->aotables= NULL;
	}
	if((re->r.mode & R_RAYTRACE) && (re->wrld.mode & WO_AMB_OCC) &&
	    (re->wrld.ao_samp_method == WO_AOSAMP_HAMMERSLEY) && (re->qsa))
		free_render_qmcsampler(re);
	
	if(re->r.mode & R_RAYTRACE) freeraytree(re);

	free_sss(re);
	
	re->totvlak=re->totvert=re->totlamp=re->tothalo= 0;
	re->i.convertdone= 0;
	
	if(re->scene)
		if(re->scene->r.scemode & R_FREE_IMAGE)
			if((re->r.scemode & R_PREVIEWBUTS)==0)
				BKE_image_free_all_textures();

	if(re->memArena) {
		BLI_memarena_free(re->memArena);
		re->memArena = NULL;
	}
}

/* per face check if all samples should be taken.
   if raytrace, do always for raytraced material, or when material full_osa set */
static void set_fullsample_flag(Render *re)
{
	VlakRen *vlr;
	int a, trace;

	if(re->osa==0)
		return;
	
	trace= re->r.mode & R_RAYTRACE;
	
	for(a=re->totvlak-1; a>=0; a--) {
		vlr= RE_findOrAddVlak(re, a);
		
		if(vlr->mat->mode & MA_FULL_OSA) vlr->flag |= R_FULL_OSA;
		else if(trace) {
			if(vlr->mat->mode & MA_SHLESS);
			else if(vlr->mat->mode & (MA_RAYTRANSP|MA_RAYMIRROR))
				/* for blurry reflect/refract, better to take more samples 
				 * inside the raytrace than as OSA samples */
				if ((vlr->mat->gloss_mir == 1.0) && (vlr->mat->gloss_tra == 1.0)) 
					vlr->flag |= R_FULL_OSA;
		}
	}
}

static void check_non_flat_quads(Render *re)
{
	VlakRen *vlr, *vlr1;
	VertRen *v1, *v2, *v3, *v4;
	float nor[3], xn, flen;
	int a;

	for(a=re->totvlak-1; a>=0; a--) {
		vlr= RE_findOrAddVlak(re, a);
		
		/* test if rendering as a quad or triangle, skip wire */
		if(vlr->v4 && (vlr->flag & R_STRAND)==0 && (vlr->mat->mode & MA_WIRE)==0) {
			
			/* check if quad is actually triangle */
			v1= vlr->v1;
			v2= vlr->v2;
			v3= vlr->v3;
			v4= vlr->v4;
			VECSUB(nor, v1->co, v2->co);
			if( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
				vlr->v1= v2;
				vlr->v2= v3;
				vlr->v3= v4;
				vlr->v4= NULL;
			}
			else {
				VECSUB(nor, v2->co, v3->co);
				if( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
					vlr->v2= v3;
					vlr->v3= v4;
					vlr->v4= NULL;
				}
				else {
					VECSUB(nor, v3->co, v4->co);
					if( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
						vlr->v4= NULL;
					}
					else {
						VECSUB(nor, v4->co, v1->co);
						if( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
							vlr->v4= NULL;
						}
					}
				}
			}
			
			if(vlr->v4) {
				
				/* Face is divided along edge with the least gradient 		*/
				/* Flagged with R_DIVIDE_24 if divide is from vert 2 to 4 	*/
				/* 		4---3		4---3 */
				/*		|\ 1|	or  |1 /| */
				/*		|0\ |		|/ 0| */
				/*		1---2		1---2 	0 = orig face, 1 = new face */
				
				/* render normals are inverted in render! we calculate normal of single tria here */
				flen= CalcNormFloat(vlr->v4->co, vlr->v3->co, vlr->v1->co, nor);
				if(flen==0.0) CalcNormFloat(vlr->v4->co, vlr->v2->co, vlr->v1->co, nor);
				
				xn= nor[0]*vlr->n[0] + nor[1]*vlr->n[1] + nor[2]*vlr->n[2];

				if(ABS(xn) < 0.999995 ) {	// checked on noisy fractal grid
					float d1, d2;

					vlr1= RE_vlakren_copy(re, vlr);
					vlr1->flag |= R_FACE_SPLIT;
					
					/* split direction based on vnorms */
					CalcNormFloat(vlr->v1->co, vlr->v2->co, vlr->v3->co, nor);
					d1= nor[0]*vlr->v1->n[0] + nor[1]*vlr->v1->n[1] + nor[2]*vlr->v1->n[2];

					CalcNormFloat(vlr->v2->co, vlr->v3->co, vlr->v4->co, nor);
					d2= nor[0]*vlr->v2->n[0] + nor[1]*vlr->v2->n[1] + nor[2]*vlr->v2->n[2];
					
					if( fabs(d1) < fabs(d2) ) vlr->flag |= R_DIVIDE_24;
					else vlr->flag &= ~R_DIVIDE_24;
					
					/* new vertex pointers */
					if (vlr->flag & R_DIVIDE_24) {
						vlr1->v1= vlr->v2;
						vlr1->v2= vlr->v3;
						vlr1->v3= vlr->v4;

						vlr->v3 = vlr->v4;
						
						vlr1->flag |= R_DIVIDE_24;
					}
					else {
						vlr1->v1= vlr->v1;
						vlr1->v2= vlr->v3;
						vlr1->v3= vlr->v4;
						
						vlr1->flag &= ~R_DIVIDE_24;
					}
					vlr->v4 = vlr1->v4 = NULL;
					
					/* new normals */
					CalcNormFloat(vlr->v3->co, vlr->v2->co, vlr->v1->co, vlr->n);
					CalcNormFloat(vlr1->v3->co, vlr1->v2->co, vlr1->v1->co, vlr1->n);
				}
				/* clear the flag when not divided */
				else vlr->flag &= ~R_DIVIDE_24;
			}
		}
	}
}

/* layflag: allows material group to ignore layerflag */
static void add_lightgroup(Render *re, Group *group, int exclusive)
{
	GroupObject *go, *gol;
	
	group->id.flag &= ~LIB_DOIT;

	/* it's a bit too many loops in loops... but will survive */
	/* note that 'exclusive' will remove it from the global list */
	for(go= group->gobject.first; go; go= go->next) {
		go->lampren= NULL;
		
		if(go->ob->lay & re->scene->lay) {
			if(go->ob && go->ob->type==OB_LAMP) {
				for(gol= re->lights.first; gol; gol= gol->next) {
					if(gol->ob==go->ob) {
						go->lampren= gol->lampren;
						break;
					}
				}
				if(go->lampren==NULL) 
					gol= add_render_lamp(re, go->ob);
				if(gol && exclusive) {
					BLI_remlink(&re->lights, gol);
					MEM_freeN(gol);
				}
			}
		}
	}
}

static void set_material_lightgroups(Render *re)
{
	Group *group;
	Material *ma;
	
	/* not for preview render */
	if(re->scene->r.scemode & R_PREVIEWBUTS)
		return;
	
	for(group= G.main->group.first; group; group=group->id.next)
		group->id.flag |= LIB_DOIT;
	
	/* it's a bit too many loops in loops... but will survive */
	/* hola! materials not in use...? */
	for(ma= G.main->mat.first; ma; ma=ma->id.next) {
		if(ma->group && (ma->group->id.flag & LIB_DOIT))
			add_lightgroup(re, ma->group, ma->mode & MA_GROUP_NOLAY);
	}
}

static void set_renderlayer_lightgroups(Render *re, Scene *sce)
{
	SceneRenderLayer *srl;
	
	for(srl= sce->r.layers.first; srl; srl= srl->next) {
		if(srl->light_override)
			add_lightgroup(re, srl->light_override, 0);
	}
}

void init_render_world(Render *re)
{
	int a;
	char *cp;
	
	if(re->scene && re->scene->world) {
		re->wrld= *(re->scene->world);
		
		cp= (char *)&re->wrld.fastcol;
		
		cp[0]= 255.0*re->wrld.horr;
		cp[1]= 255.0*re->wrld.horg;
		cp[2]= 255.0*re->wrld.horb;
		cp[3]= 1;
		
		VECCOPY(re->grvec, re->viewmat[2]);
		Normalize(re->grvec);
		Mat3CpyMat4(re->imat, re->viewinv);
		
		for(a=0; a<MAX_MTEX; a++) 
			if(re->wrld.mtex[a] && re->wrld.mtex[a]->tex) re->wrld.skytype |= WO_SKYTEX;
		
		/* AO samples should be OSA minimum */
		if(re->osa)
			while(re->wrld.aosamp*re->wrld.aosamp < re->osa) 
				re->wrld.aosamp++;
		if(!(re->r.mode & R_RAYTRACE))
			re->wrld.mode &= ~WO_AMB_OCC;
	}
	else {
		memset(&re->wrld, 0, sizeof(World));
		re->wrld.exp= 0.0;
		re->wrld.range= 1.0;
	}
	
	re->wrld.linfac= 1.0 + pow((2.0*re->wrld.exp + 0.5), -10);
	re->wrld.logfac= log( (re->wrld.linfac-1.0)/re->wrld.linfac )/re->wrld.range;
}

/* used to be 'rotate scene' */
void RE_Database_FromScene(Render *re, Scene *scene, int use_camera_view)
{
	extern int slurph_opt;	/* key.c */
	Base *base;
	Object *ob;
	Scene *sce;
	float mat[4][4];
	unsigned int lay;

	re->scene= scene;
	
	/* per second, per object, stats print this */
	re->i.infostr= "Preparing Scene data";

	/* XXX add test if dbase was filled already? */
	
	re->memArena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	re->totvlak=re->totvert=re->totlamp=re->tothalo= 0;
	re->lights.first= re->lights.last= NULL;
	re->lampren.first= re->lampren.last= NULL;
	
	slurph_opt= 0;
	re->i.partsdone= 0;	/* signal now in use for previewrender */
	
	/* in localview, lamps are using normal layers, objects only local bits */
	if(re->scene->lay & 0xFF000000) lay= re->scene->lay & 0xFF000000;
	else lay= re->scene->lay;
	
	/* applies changes fully */
	if((re->r.scemode & R_PREVIEWBUTS)==0)
		scene_update_for_newframe(re->scene, lay);
	
	/* if no camera, viewmat should have been set! */
	if(use_camera_view && re->scene->camera) {
		Mat4Ortho(re->scene->camera->obmat);
		Mat4Invert(mat, re->scene->camera->obmat);
		RE_SetView(re, mat);
	}
	
	init_render_world(re);	/* do first, because of ambient. also requires re->osa set correct */
	if(re->wrld.mode & WO_AMB_OCC) {
		if (re->wrld.ao_samp_method == WO_AOSAMP_HAMMERSLEY)
			init_render_hammersley(re);
		else if (re->wrld.ao_samp_method == WO_AOSAMP_CONSTANT)
			init_ao_sphere(&re->wrld);
	}
	
	/* still bad... doing all */
	init_render_textures(re);
	init_render_materials(re->r.mode, &re->wrld.ambr);
	set_node_shader_lamp_loop(shade_material_loop);

	for(SETLOOPER(re->scene, base)) {
		ob= base->object;
		/* imat objects has to be done here, since displace can have texture using Object map-input */
		MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
		MTC_Mat4Invert(ob->imat, mat);
		/* each object should only be rendered once */
		ob->flag &= ~OB_DONE;
	}

	/* MAKE RENDER DATA */
	
	for(SETLOOPER(re->scene, base)) {
		ob= base->object;
		
		/* if the object has been restricted from rendering in the outliner, ignore it */
		if (ob->restrictflag & OB_RESTRICT_RENDER) continue;
		
		/* OB_DONE means the object itself got duplicated, so was already converted */
		if (ob->flag & OB_DONE) {
#ifndef DISABLE_YAFRAY
			/* yafray: for some reason this part was removed, but yafray really needs it...
			   Dupliverts objects are treated as instances of an original 'sourceobject',
			   which needs to be included in the renderlist here.
			   exception: lamps, lattices, armatures & camera's */
			if ((re->r.renderer==R_YAFRAY) && ((ob->type!=OB_LATTICE) && (ob->type!=OB_ARMATURE) &&
			    (ob->type!=OB_LAMP) && (ob->type!=OB_CAMERA)))
			{
				printf("Duplivert object %s, adding to renderlist\n", ob->id.name);
				ob->flag &= ~OB_DONE;
				init_render_object(re, ob, NULL, 0, 0);
				ob->flag |= OB_DONE;
			}
#endif /* disable yafray */
		}
		else if( (base->lay & lay) || (ob->type==OB_LAMP && (base->lay & re->scene->lay)) ) {
			if(ob->transflag & OB_DUPLI) {
				
				/* exception: mballs! */
#ifndef DISABLE_YAFRAY
				/* yafray: except for mballs, include at least one copy of a dupliframe object in the renderlist. */
				if (re->r.renderer==R_YAFRAY) {
					if ((ob->type!=OB_MBALL) && ((ob->transflag & OB_DUPLIFRAMES)!=0)) {
						printf("Dupliframe Object %s, adding to renderlist\n", ob->id.name);
						init_render_object(re, ob, NULL, 0, 0);
					}
				}
#endif /* disable yafray */
				/* before make duplis, update particle for current frame */
				if(ob->transflag & OB_DUPLIVERTS) {
					PartEff *paf= give_parteff(ob);
					if(paf) {
						if(paf->flag & PAF_ANIMATED) build_particle_system(ob);
					}
				}
				
				if(ob->type==OB_MBALL) {
					init_render_object(re, ob, NULL, 0, 0);
				}
				else {
					DupliObject *dob;
					ListBase *lb= object_duplilist(sce, ob);
					
					for(dob= lb->first; dob; dob= dob->next) {
						Object *obd= dob->ob;
						
						if (obd->restrictflag & OB_RESTRICT_RENDER) continue;
						
						Mat4CpyMat4(obd->obmat, dob->mat);
						
						/* group duplis need to set ob matrices correct, for deform. so no_draw is part handled */
						if(dob->no_draw)
							continue;
						
						if(obd->type!=OB_MBALL) {
#ifndef DISABLE_YAFRAY
							/* yafray: special case handling of duplivert/dupligroup objects.
							   Only one copy included in renderlist(see above), all others treated as instance of that.
							   So only need to store name and matrix. Exception are lamps. lattices, armatures and camera's */
							if (re->r.renderer==R_YAFRAY) {
								/* dupligroup obs are included directly */
								if (obd->flag & OB_FROMGROUP) {
									printf("Dupligroup object %s, adding to renderlist\n", obd->id.name);
									init_render_object(re, obd, ob, dob->index, 0);
								}
								else if ((obd->type!=OB_LATTICE) && (obd->type!=OB_ARMATURE) &&
								         (obd->type!=OB_LAMP) && (obd->type!=OB_CAMERA))
								{
									printf("Adding dupli matrix for object %s\n", obd->id.name);
									YAF_addDupliMtx(obd);
								}
								else init_render_object(re, obd, ob, dob->index, 0);
							}
							else init_render_object(re, obd, ob, dob->index, 0);
#else
							init_render_object(re, obd, ob, dob->index, 0);
#endif /* disable yafray */
						}
						
						if(re->test_break()) break;
					}
					free_object_duplilist(lb);
				}
			}
			else {
#ifndef DISABLE_YAFRAY
				/* yafray: linked data objects treated similarly to dupliverts,
				   If object not known yet (not in renderlist), include in the renderlist,
				   otherwise treat as instance of it, so only name and matrix are stored 
				   Exception: objects which have materials linked to object instead of mesh */
				if ((re->r.renderer==R_YAFRAY) && (ob->colbits==0))
				{
					/* Special case, parent object dupli's: ignore if object itself is lamp or parent is lattice or empty */
					if (ob->parent) {
						if ((ob->type!=OB_LAMP) && (ob->parent->type!=OB_EMPTY) &&
						    (ob->parent->type!=OB_LATTICE) && YAF_objectKnownData(ob))
							printf("From parent: Added dupli matrix for linked data object %s\n", ob->id.name);
						else
							init_render_object(re, ob, NULL, 0, 0);
					}
					else if ((ob->type!=OB_EMPTY) && (ob->type!=OB_LAMP) &&
					         (ob->type!=OB_ARMATURE) && YAF_objectKnownData(ob))
						printf("Added dupli matrix for linked data object %s\n", ob->id.name);
					else
						init_render_object(re, ob, NULL, 0, 0);
				}
				else init_render_object(re, ob, NULL, 0, 0);
#else
				init_render_object(re, ob, NULL, 0, 0);
#endif /* disable yafray */
			}

		}

		if(re->test_break()) break;
	}

	
	if(!re->test_break()) {
		LampRen *lar;
		
		sort_halos(re);
		
		set_material_lightgroups(re);
		for(sce= re->scene; sce; sce= sce->set)
			set_renderlayer_lightgroups(re, sce);
		
		slurph_opt= 1;
		
		/* for now some clumsy copying still */
		re->i.totvert= re->totvert;
		re->i.totface= re->totvlak;
		re->i.tothalo= re->tothalo;
		re->i.totlamp= re->totlamp;
		re->stats_draw(&re->i);
		
		set_fullsample_flag(re);
		check_non_flat_quads(re);
		set_normalflags(re);
		
		if(!re->test_break())
			if(re->wrld.mode & WO_STARS) 
				RE_make_stars(re, NULL, NULL, NULL);
		
		re->i.infostr= "Creating Shadowbuffers";
		re->stats_draw(&re->i);

		/* SHADOW BUFFER */
		for(lar=re->lampren.first; lar; lar= lar->next) {
			if(re->test_break()) break;
			if(lar->shb) {
				/* if type is irregular, this only sets the perspective matrix and autoclips */
				makeshadowbuf(re, lar);
			}
		}
		
		/* yafray: 'direct' radiosity, environment maps and raytree init not needed for yafray render */
		/* although radio mode could be useful at some point, later */
		if (re->r.renderer==R_INTERN) {
			/* RADIO (uses no R anymore) */
			if(!re->test_break())
				if(re->r.mode & R_RADIO) do_radio_render(re);
			
			/* raytree */
			if(!re->test_break()) {
				if(re->r.mode & R_RAYTRACE) {
					makeraytree(re);
				}
			}
			/* ENVIRONMENT MAPS */
			if(!re->test_break())
				make_envmaps(re);
		}
		
		if(!re->test_break())
			project_renderdata(re, projectverto, re->r.mode & R_PANORAMA, 0);

		/* SSS */
		if((re->r.mode & R_SSS) && !re->test_break())
			if (re->r.renderer==R_INTERN)
				make_sss_tree(re);
	}
	
	if(re->test_break())
		RE_Database_Free(re);
	else
		re->i.convertdone= 1;
	
	re->i.infostr= NULL;
	re->stats_draw(&re->i);
}

static void database_fromscene_vectors(Render *re, Scene *scene, int timeoffset)
{
	extern int slurph_opt;	/* key.c */
	Base *base;
	Object *ob;
	Scene *sce;
	float mat[4][4];
	unsigned int lay;
	
	re->scene= scene;
	
	/* XXX add test if dbase was filled already? */
	
	re->memArena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	re->totvlak=re->totvert=re->totlamp=re->tothalo= 0;
	re->i.totface=re->i.totvert=re->i.totlamp=re->i.tothalo= 0;
	re->lights.first= re->lights.last= NULL;

	slurph_opt= 0;
	
	/* in localview, lamps are using normal layers, objects only local bits */
	if(re->scene->lay & 0xFF000000) lay= re->scene->lay & 0xFF000000;
	else lay= re->scene->lay;
	
	/* applies changes fully, still using G.scene for timing... */
	G.scene->r.cfra+=timeoffset;
	scene_update_for_newframe(re->scene, lay);
	
	/* if no camera, viewmat should have been set! */
	if(re->scene->camera) {
		Mat4Ortho(re->scene->camera->obmat);
		Mat4Invert(mat, re->scene->camera->obmat);
		RE_SetView(re, mat);
	}
	
	for(SETLOOPER(re->scene, base)) {
		ob= base->object;
		/* imat objects has to be done here, since displace can have texture using Object map-input */
		MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
		MTC_Mat4Invert(ob->imat, mat);
		/* each object should only be rendered once */
		ob->flag &= ~OB_DONE;
	}
	
	/* MAKE RENDER DATA */
	
	for(SETLOOPER(re->scene, base)) {
		ob= base->object;
		
		if (ob->restrictflag & OB_RESTRICT_RENDER) continue;

		/* OB_DONE means the object itself got duplicated, so was already converted */
		if(ob->flag & OB_DONE);
		else if( (base->lay & lay) || (ob->type==OB_LAMP && (base->lay & re->scene->lay)) ) {
			if(ob->transflag & OB_DUPLI) {
				
				/* before make duplis, update particle for current frame */
				if(ob->transflag & OB_DUPLIVERTS) {
					PartEff *paf= give_parteff(ob);
					if(paf) {
						if(paf->flag & PAF_ANIMATED) build_particle_system(ob);
					}
				}
				
				if(ob->type==OB_MBALL) {
					init_render_object(re, ob, NULL, 0, 1);
				}
				else {
					DupliObject *dob;
					ListBase *lb= object_duplilist(sce, ob);
					
					for(dob= lb->first; dob; dob= dob->next) {
						Object *obd= dob->ob;
						
						if (obd->restrictflag & OB_RESTRICT_RENDER) continue;
						
						Mat4CpyMat4(obd->obmat, dob->mat);
						
						if(obd->type!=OB_MBALL) {
							init_render_object(re, obd, ob, dob->index, 1);
						}
					}
					free_object_duplilist(lb);
				}
			}
			else {
				init_render_object(re, ob, NULL, 0, 1);
			}
			
		}
		if(re->test_break()) break;
	}
	
	if(!re->test_break())
		project_renderdata(re, projectverto, re->r.mode & R_PANORAMA, 0);

	/* do this in end, particles for example need cfra */
	G.scene->r.cfra-=timeoffset;
}

/* choose to use static, to prevent giving too many args to this call */
static void speedvector_project(Render *re, float *zco, VertRen *ver)
{
	static float pixelphix=0.0f, pixelphiy=0.0f, zmulx=0.0f, zmuly=0.0f;
	static int pano= 0;
	float div;
	
	/* initialize */
	if(re) {
		pano= re->r.mode & R_PANORAMA;
		
		/* precalculate amount of radians 1 pixel rotates */
		if(pano) {
			/* size of 1 pixel mapped to viewplane coords */
			float psize= (re->viewplane.xmax-re->viewplane.xmin)/(float)re->winx;
			/* x angle of a pixel */
			pixelphix= atan(psize/re->clipsta);
			
			psize= (re->viewplane.ymax-re->viewplane.ymin)/(float)re->winy;
			/* y angle of a pixel */
			pixelphiy= atan(psize/re->clipsta);
		}
		zmulx= re->winx/2;
		zmuly= re->winy/2;
		
		return;
	}
	
	/* now map hocos to screenspace, uses very primitive clip still */
	if(ver->ho[3]<0.1f) div= 10.0f;
	else div= 1.0f/ver->ho[3];
	
	/* use cylinder projection */
	if(pano) {
		float vec[3], ang;
		/* angle between (0,0,-1) and (ver->co) */
		VECCOPY(vec, ver->co);

		ang= saacos(-vec[2]/sqrt(vec[0]*vec[0] + vec[2]*vec[2]));
		if(vec[0]<0.0f) ang= -ang;
		zco[0]= ang/pixelphix + zmulx;
		
		ang= 0.5f*M_PI - saacos(vec[1]/sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]));
		zco[1]= ang/pixelphiy + zmuly;
		
	}
	else {
		zco[0]= zmulx*(1.0f+ver->ho[0]*div);
		zco[1]= zmuly*(1.0f+ver->ho[1]*div);
	}
}

static void calculate_speedvectors(Render *re, float *vectors, int startvert, int endvert, int step)
{
	VertRen *ver= NULL;
	float *speed, zco[2];
	float len;
	float winsq= re->winx*re->winy, winroot= sqrt(winsq);
	int a;
	
	/* set first vertex OK */
	a= startvert-1;
	ver= re->vertnodes[a>>8].vert + (a & 255);
	
	for(a=startvert; a<endvert; a++, vectors+=2) {
		if((a & 255)==0)
			ver= re->vertnodes[a>>8].vert;
		else
			ver++;
		
		speedvector_project(NULL, zco, ver);
		
		zco[0]= vectors[0] - zco[0];
		zco[1]= vectors[1] - zco[1];
		
		/* enable nice masks for hardly moving stuff or float inaccuracy */
		if(zco[0]<0.1f && zco[0]>-0.1f && zco[1]<0.1f && zco[1]>-0.1f ) {
			zco[0]= 0.0f;
			zco[1]= 0.0f;
		}
		
		/* maximize speed for image width, otherwise it never looks good */
		len= zco[0]*zco[0] + zco[1]*zco[1];
		if(len > winsq) {
			len= winroot/sqrt(len);
			zco[0]*= len;
			zco[1]*= len;
		}
		
		speed= RE_vertren_get_winspeed(re, ver, 1);
		/* note; in main vecblur loop speedvec is negated again */
		if(step) {
			speed[2]= -zco[0];
			speed[3]= -zco[1];
		}
		else {
			speed[0]= zco[0];
			speed[1]= zco[1];
		}
	}
}

static int load_fluidsimspeedvectors(Render *re, float *vectors, int startvert, int endvert, int step, Object *fsob)
{
	VertRen *ver= NULL;
	float *speed, div, zco[2];
	float zmulx= re->winx/2, zmuly= re->winy/2, len;
	float winsq= re->winx*re->winy, winroot= sqrt(winsq);
	int a, j;
	float hoco[4], fsvec[4], camco[4];
	float mat[4][4];
	float imat[4][4];
	MVert *vverts;

	/* only one step needed */
	if(step) return 1;
	
	Mat4CpyMat4(mat, re->viewmat);
	MTC_Mat4Invert(imat, mat);

	/* set first vertex OK */
	a= startvert-1;
	ver= re->vertnodes[a>>8].vert + (a & 255);

	if( (!fsob->fluidsimSettings) || (!fsob->fluidsimSettings->meshSurfNormals) ) return 0;
	vverts = fsob->fluidsimSettings->meshSurfNormals;
	//fprintf(stderr, "GZ_VEL obj '%s', calc load_fluidsimspeedvectors\n",fsob->id.name); // NT DEBUG

	if( endvert-startvert != fsob->fluidsimSettings->meshSurface->totvert ) {
		//fprintf(stderr, "load_fluidsimspeedvectors - modified fluidsim mesh, not using speed vectors (%d,%d)...\n", endvert-startvert , fsob->fluidsimSettings->meshSurface->totvert); // DEBUG
		return 0;
	}
	
	for(a=startvert; a<endvert; a++, vectors+=2) {
		if((a & 255)==0)
			ver= re->vertnodes[a>>8].vert;
		else
			ver++;

		// get fluid velocity
		fsvec[3] = 0.; 
		//fsvec[0] = fsvec[1] = fsvec[2] = fsvec[3] = 0.; fsvec[2] = 2.; // NT fixed test
		for(j=0;j<3;j++) fsvec[j] = vverts[a-startvert].co[j];

		// transform (=rotate) to cam space
		camco[0]= imat[0][0]*fsvec[0] + imat[0][1]*fsvec[1] + imat[0][2]*fsvec[2];
		camco[1]= imat[1][0]*fsvec[0] + imat[1][1]*fsvec[1] + imat[1][2]*fsvec[2];
		camco[2]= imat[2][0]*fsvec[0] + imat[2][1]*fsvec[1] + imat[2][2]*fsvec[2];

		// get homogenous coordinates
		projectverto(camco, re->winmat, hoco);
		
		/* now map hocos to screenspace, uses very primitive clip still */
		// use ho[3] of original vertex, xy component of vel. direction
		if(ver->ho[3]<0.1f) div= 10.0f;
		else div= 1.0f/ver->ho[3];
		zco[0]= zmulx*hoco[0]*div;
		zco[1]= zmuly*hoco[1]*div;
		
		// maximize speed as usual
		len= zco[0]*zco[0] + zco[1]*zco[1];
		if(len > winsq) {
			len= winroot/sqrt(len);
			zco[0]*= len; zco[1]*= len;
		}
		
		speed= RE_vertren_get_winspeed(re, ver, 1);
		// set both to the same value
		speed[0]= speed[2]= zco[0];
		speed[1]= speed[3]= zco[1];
		//if(a<20) fprintf(stderr,"speed %d %f,%f | camco %f,%f,%f | hoco %f,%f,%f,%f  \n", a, speed[0], speed[1], camco[0],camco[1], camco[2], hoco[0],hoco[1], hoco[2],hoco[3]); // NT DEBUG
	}

	return 1;
}

/* makes copy per object of all vectors */
/* result should be that we can free entire database */
static void copy_dbase_object_vectors(Render *re, ListBase *lb)
{
	ObjectRen *obren, *obrenlb;
	VertRen *ver;
	float *vec;
	int a;
	
	for(obren= re->objecttable.first; obren; obren= obren->next) {
		obrenlb= MEM_dupallocN(obren);
		BLI_addtail(lb, obrenlb);
		if(obren->endvert>obren->startvert) {
			vec= obrenlb->vectors= MEM_mallocN(2*sizeof(float)*(obren->endvert- obren->startvert), "vector array");

			/* first vertex */
			a= obren->startvert-1;
			ver= re->vertnodes[a>>8].vert + (a & 255);

			for(a=obren->startvert; a<obren->endvert; a++, vec+=2) {
				if((a & 255)==0)
					ver= re->vertnodes[a>>8].vert;
				else
					ver++;
				
				speedvector_project(NULL, vec, ver);
			}
		}
	}
}

static void free_dbase_object_vectors(ListBase *lb)
{
	ObjectRen *obren;
	
	for(obren= lb->first; obren; obren= obren->next)
		if(obren->vectors)
			MEM_freeN(obren->vectors);
	BLI_freelistN(lb);
}

void RE_Database_FromScene_Vectors(Render *re, Scene *sce)
{
	ObjectRen *obren, *oldobren;
	ListBase *table;
	ListBase oldtable= {NULL, NULL}, newtable= {NULL, NULL};
	int step;
	
	re->i.infostr= "Calculating previous vectors";
	re->r.mode |= R_SPEED;
	
	speedvector_project(re, NULL, NULL);	/* initializes projection code */
	
	/* creates entire dbase */
	database_fromscene_vectors(re, sce, -1);
	
	/* copy away vertex info */
	copy_dbase_object_vectors(re, &oldtable);
		
	/* free dbase and make the future one */
	RE_Database_Free(re);
	
	if(!re->test_break()) {
		/* creates entire dbase */
		re->i.infostr= "Calculating next frame vectors";
		
		database_fromscene_vectors(re, sce, +1);
	}	
	/* copy away vertex info */
	copy_dbase_object_vectors(re, &newtable);
	
	/* free dbase and make the real one */
	RE_Database_Free(re);
	
	if(!re->test_break())
		RE_Database_FromScene(re, sce, 1);
	
	if(!re->test_break()) {
		for(step= 0; step<2; step++) {
			
			if(step)
				table= &newtable;
			else
				table= &oldtable;
			
			oldobren= table->first;
			for(obren= re->objecttable.first; obren && oldobren; obren= obren->next, oldobren= oldobren->next) {
				int ok= 1;

				/* find matching object in old table */
				if(oldobren->ob!=obren->ob || oldobren->par!=obren->par || oldobren->index!=obren->index) {
					ok= 0;
					for(oldobren= table->first; oldobren; oldobren= oldobren->next)
						if(oldobren->ob==obren->ob && oldobren->par==obren->par && oldobren->index==obren->index)
							break;
					if(oldobren==NULL)
						oldobren= table->first;
					else
						ok= 1;
				}
				if(ok==0) {
					 printf("speed table: missing object %s\n", obren->ob->id.name+2);
					continue;
				}

				// NT check for fluidsim special treatment
				if((obren->ob->fluidsimFlag & OB_FLUIDSIM_ENABLE) && (obren->ob->fluidsimSettings->type & OB_FLUIDSIM_DOMAIN)) {
					// use preloaded per vertex simulation data , only does calculation for step=1
					// NOTE/FIXME - velocities and meshes loaded unnecessarily often during the database_fromscene_vectors calls...
					load_fluidsimspeedvectors(re, oldobren->vectors, obren->startvert, obren->endvert, step, obren->ob);
				} else {
					/* check if both have same amounts of vertices */
					if(obren->endvert-obren->startvert != oldobren->endvert-oldobren->startvert) {
						printf("Warning: object %s has different amount of vertices on other frame\n", obren->ob->id.name+2);
						continue;
					}
					
					calculate_speedvectors(re, oldobren->vectors, obren->startvert, obren->endvert, step);
				} // not fluidsim
			}
		}
	}
	
	free_dbase_object_vectors(&oldtable);
	free_dbase_object_vectors(&newtable);
	
	re->i.infostr= NULL;
	re->stats_draw(&re->i);
}


/* exported call to recalculate hoco for vertices, when winmat changed */
void RE_DataBase_ApplyWindow(Render *re)
{
	project_renderdata(re, projectverto, 0, 0);
}

/* setup for shaded view or bake, so only lamps and materials are initialized */
/* type:
   RE_BAKE_LIGHT:  for shaded view, only add lamps
   RE_BAKE_ALL:    for baking, all lamps and objects
   RE_BAKE_NORMALS:for baking, no lamps and only selected objects
   RE_BAKE_AO:     for baking, no lamps, but all objects
   RE_BAKE_TEXTURE:for baking, no lamps, only selected objects
*/
void RE_Database_Baking(Render *re, Scene *scene, int type)
{
	Base *base;
	Object *ob;
	Scene *sce;
	float mat[4][4];
	unsigned int lay;
	
	re->scene= scene;

	/* renderdata setup and exceptions */
	re->r= scene->r;
	re->r.mode &= ~R_OSA;
	re->flag |= R_GLOB_NOPUNOFLIP;
	
	if( ELEM3(type, RE_BAKE_LIGHT, RE_BAKE_NORMALS, RE_BAKE_TEXTURE) ) {
		re->r.mode &= ~R_SHADOW;
		re->r.mode &= ~R_RAYTRACE;
	}
	
	/* setup render stuff */
	if(type!=RE_BAKE_LIGHT)
		re->memArena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	
	re->totvlak=re->totvert=re->totlamp=re->tothalo= 0;
	re->lights.first= re->lights.last= NULL;
	re->lampren.first= re->lampren.last= NULL;

	/* in localview, lamps are using normal layers, objects only local bits */
	if(re->scene->lay & 0xFF000000) lay= re->scene->lay & 0xFF000000;
	else lay= re->scene->lay;
	
	/* if no camera, set unit */
	if(re->scene->camera) {
		Mat4Ortho(re->scene->camera->obmat);
		Mat4Invert(mat, re->scene->camera->obmat);
		RE_SetView(re, mat);
	}
	else {
		Mat4One(mat);
		RE_SetView(re, mat);
	}
	
	init_render_world(re);	/* do first, because of ambient. also requires re->osa set correct */
	if(re->wrld.mode & WO_AMB_OCC) {
		if (re->wrld.ao_samp_method == WO_AOSAMP_HAMMERSLEY)
			init_render_hammersley(re);
		else if (re->wrld.ao_samp_method == WO_AOSAMP_CONSTANT)
			init_ao_sphere(&re->wrld);
	}
	
	
	/* still bad... doing all */
	init_render_textures(re);
	init_render_materials(re->r.mode, &re->wrld.ambr);
	set_node_shader_lamp_loop(shade_material_loop);
	
	for(SETLOOPER(re->scene, base)) {
		ob= base->object;
		/* imat objects has to be done here, since displace can have texture using Object map-input */
		MTC_Mat4MulMat4(mat, ob->obmat, re->viewmat);
		MTC_Mat4Invert(ob->imat, mat);
		/* each object should only be rendered once */
		ob->flag &= ~OB_DONE;
	}

	/* MAKE RENDER DATA */
	for(SETLOOPER(re->scene, base)) {
		ob= base->object;
		
		/* if the object has been restricted from rendering in the outliner, ignore it */
		if (ob->restrictflag & OB_RESTRICT_RENDER) continue;
		
		/* OB_DONE means the object itself got duplicated, so was already converted */
		if(ob->flag & OB_DONE);
		else if( (base->lay & lay) || ((base->lay & re->scene->lay)) ) {
			
			/* check for dupli lamps or non selected groups */
			if(ob->transflag & OB_DUPLI) {
				DupliObject *dob;
				ListBase *lb= object_duplilist(sce, ob);
				
				for(dob= lb->first; dob; dob= dob->next) {
					Object *obd= dob->ob;
					
					if(obd->type==OB_LAMP) {
						if( ELEM(type, RE_BAKE_LIGHT, RE_BAKE_ALL) ) {
							Mat4CpyMat4(obd->obmat, dob->mat);
							init_render_object(re, obd, ob, dob->index, 0);
						}
					}
					else if( ELEM(type, RE_BAKE_AO, RE_BAKE_ALL) ) {
						if((base->flag & SELECT)==0) {
							Mat4CpyMat4(obd->obmat, dob->mat);
							init_render_object(re, obd, ob, dob->index, 0);
						}
					}
				}
				free_object_duplilist(lb);
			}
			else {
				if(ob->type==OB_LAMP) {
					if( ELEM(type, RE_BAKE_LIGHT, RE_BAKE_ALL) )
						init_render_object(re, ob, NULL, 0, 0);
				}
				else if(type!=RE_BAKE_LIGHT) {
					if( !ELEM(type, RE_BAKE_NORMALS, RE_BAKE_TEXTURE) || (ob->flag & SELECT))
						init_render_object(re, ob, NULL, 0, 0);
				}
			}
		}
	}
	set_material_lightgroups(re);
	
	check_non_flat_quads(re);
	/* don't call set_normalflags(), no flipping */
	
	if(type!=RE_BAKE_LIGHT) {
		if(re->r.mode & R_SHADOW) {
			LampRen *lar;
			
			/* SHADOW BUFFER */
			for(lar=re->lampren.first; lar; lar= lar->next) {
				
				if(re->test_break()) break;
				if(lar->shb) {
					/* if type is irregular, this only sets the perspective matrix and autoclips */
					/* but, that's not supported for bake... */
					makeshadowbuf(re, lar);
				}
			}
		}
	}

	if(type!=RE_BAKE_LIGHT) {
		/* raytree */
		if(!re->test_break()) {
			if(re->r.mode & R_RAYTRACE) {
				makeraytree(re);
			}
		}
	}
}

void RE_DataBase_GetView(Render *re, float mat[][4])
{
	Mat4CpyMat4(mat, re->viewmat);
}




/* **************************************************************** */
/*                sticky texture coords                             */
/* **************************************************************** */

void RE_make_sticky(void)
{
	Object *ob;
	Base *base;
	MVert *mvert;
	Mesh *me;
	MSticky *ms;
	Render *re;
	float ho[4], mat[4][4];
	int a;
	
	if(G.vd==NULL) {
		printf("Need a 3d view to make sticky\n");
		return;
	}
	
	if(G.scene->camera==NULL) {
		printf("Need camera to make sticky\n");
		return;
	}
	if(G.obedit) {
		printf("Unable to make sticky in Edit Mode\n");
		return;
	}
	
	re= RE_NewRender("_make sticky_");
	RE_InitState(re, &G.scene->r, G.scene->r.xsch, G.scene->r.ysch, NULL);
	
	/* use renderdata and camera to set viewplane */
	RE_SetCamera(re, G.scene->camera);

	/* and set view matrix */
	Mat4Ortho(G.scene->camera->obmat);
	Mat4Invert(mat, G.scene->camera->obmat);
	RE_SetView(re, mat);
	
	for(base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(base) {
			if(base->object->type==OB_MESH) {
				ob= base->object;
				
				me= ob->data;
				mvert= me->mvert;
				if(me->msticky)
					CustomData_free_layer_active(&me->vdata, CD_MSTICKY, me->totvert);
				me->msticky= CustomData_add_layer(&me->vdata, CD_MSTICKY,
					CD_CALLOC, NULL, me->totvert);
				
				where_is_object(ob);
				Mat4MulMat4(mat, ob->obmat, re->viewmat);
				
				ms= me->msticky;
				for(a=0; a<me->totvert; a++, ms++, mvert++) {
					VECCOPY(ho, mvert->co);
					Mat4MulVecfl(mat, ho);
					projectverto(ho, re->winmat, ho);
					ms->co[0]= ho[0]/ho[3];
					ms->co[1]= ho[1]/ho[3];
				}
			}
		}
	}
}



/* **************************************************************** */
/*                Displacement mapping                              */
/* **************************************************************** */
static short test_for_displace(Render *re, Object *ob)
{
	/* return 1 when this object uses displacement textures. */
	Material *ma;
	int i;
	
	for (i=1; i<=ob->totcol; i++) {
		ma=give_render_material(re, ob, i);
		/* ma->mapto is ORed total of all mapto channels */
		if(ma && (ma->mapto & MAP_DISPLACE)) return 1;
	}
	return 0;
}

static void displace_render_vert(Render *re, ShadeInput *shi, VertRen *vr, int vindex, float *scale)
{
	MTFace *tface;
	short texco= shi->mat->texco;
	float sample=0;
	char *name;
	int i;

	/* shi->co is current render coord, just make sure at least some vector is here */
	VECCOPY(shi->co, vr->co);
	/* vertex normal is used for textures type 'col' and 'var' */
	VECCOPY(shi->vn, vr->n);

	if (texco & TEXCO_UV) {
		shi->totuv= 0;

		for (i=0; (tface=RE_vlakren_get_tface(re, shi->vlr, i, &name, 0)); i++) {
			ShadeInputUV *suv= &shi->uv[i];

			/* shi.uv needs scale correction from tface uv */
			suv->uv[0]= 2*tface->uv[vindex][0]-1.0f;
			suv->uv[1]= 2*tface->uv[vindex][1]-1.0f;
			suv->uv[2]= 0.0f;
			suv->name= name;
			shi->totuv++;
		}
	}

	/* set all rendercoords, 'texco' is an ORed value for all textures needed */
	if ((texco & TEXCO_ORCO) && (vr->orco)) {
		VECCOPY(shi->lo, vr->orco);
	}
	if (texco & TEXCO_STICKY) {
		float *sticky= RE_vertren_get_sticky(re, vr, 0);
		if(sticky) {
			shi->sticky[0]= sticky[0];
			shi->sticky[1]= sticky[1];
			shi->sticky[2]= 0.0f;
		}
	}
	if (texco & TEXCO_GLOB) {
		VECCOPY(shi->gl, shi->co);
		MTC_Mat4MulVecfl(re->viewinv, shi->gl);
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

static void displace_render_face(Render *re, VlakRen *vlr, float *scale)
{
	ShadeInput shi;

	/* set up shadeinput struct for multitex() */
	shi.osatex= 0;		/* signal not to use dx[] and dy[] texture AA vectors */
	shi.vlr= vlr;		/* current render face */
	shi.mat= vlr->mat;		/* current input material */
	
	/* Displace the verts, flag is set when done */
	if (!vlr->v1->flag)
		displace_render_vert(re, &shi, vlr->v1,0,  scale);
	
	if (!vlr->v2->flag)
		displace_render_vert(re, &shi, vlr->v2, 1, scale);

	if (!vlr->v3->flag)
		displace_render_vert(re, &shi, vlr->v3, 2, scale);

	if (vlr->v4) {
		if (!vlr->v4->flag)
			displace_render_vert(re, &shi, vlr->v4, 3, scale);

		/*	closest in displace value.  This will help smooth edges.   */ 
		if ( fabs(vlr->v1->accum - vlr->v3->accum) > fabs(vlr->v2->accum - vlr->v4->accum)) 
			vlr->flag |= R_DIVIDE_24;
		else vlr->flag &= ~R_DIVIDE_24;	// E: typo?, was missing '='
	}
	
	/* Recalculate the face normal  - if flipped before, flip now */
	if(vlr->v4) {
		CalcNormFloat4(vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co, vlr->n);
	}	
	else {
		CalcNormFloat(vlr->v3->co, vlr->v2->co, vlr->v1->co, vlr->n);
	}
}


static void do_displacement(Render *re, Object *ob, int startface, int numface, int startvert, int numvert )
{
	VertRen *vr;
	VlakRen *vlr;
//	float min[3]={1e30, 1e30, 1e30}, max[3]={-1e30, -1e30, -1e30};
	float scale[3]={1.0f, 1.0f, 1.0f}, temp[3];//, xn
	int i; //, texflag=0;
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
		vr= RE_findOrAddVert(re, i);
		vr->flag= 0;
	}

	for(i=startface; i<startface+numface; i++){
		vlr=RE_findOrAddVlak(re, i);
		displace_render_face(re, vlr, scale);
	}
	
	/* Recalc vertex normals */
	calc_vertexnormals(re, startvert, startface, 0);
}

