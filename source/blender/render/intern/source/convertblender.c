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



#include "MEM_guardedalloc.h"

#include "BLI_math.h"
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
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_object_fluidsim.h"
#include "DNA_particle_types.h"
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
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_world.h"

#include "PIL_time.h"
#include "IMB_imbuf_types.h"

#include "envmap.h"
#include "occlusion.h"
#include "pointdensity.h"
#include "voxeldata.h"
#include "render_types.h"
#include "rendercore.h"
#include "renderdatabase.h"
#include "renderpipeline.h"
#include "shadbuf.h"
#include "shading.h"
#include "strand.h"
#include "texture.h"
#include "volume_precache.h"
#include "sss.h"
#include "strand.h"
#include "zbuf.h"
#include "sunsky.h"


/* 10 times larger than normal epsilon, test it on default nurbs sphere with ray_transp (for quad detection) */
/* or for checking vertex normal flips */
#define FLT_EPSILON10 1.19209290e-06F

/* ------------------------------------------------------------------------- */

/* Stuff for stars. This sits here because it uses gl-things. Part of
this code may move down to the converter.  */
/* ------------------------------------------------------------------------- */
/* this is a bad beast, since it is misused by the 3d view drawing as well. */

static HaloRen *initstar(Render *re, ObjectRen *obr, float *vec, float hasize)
{
	HaloRen *har;
	float hoco[4];
	
	projectverto(vec, re->winmat, hoco);
	
	har= RE_findOrAddHalo(obr, obr->tothalo++);
	
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

void RE_make_stars(Render *re, Scene *scenev3d, void (*initfunc)(void),
				   void (*vertexfunc)(float*),  void (*termfunc)(void))
{
	extern unsigned char hash[512];
	ObjectRen *obr= NULL;
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
		scene= scenev3d;
		wrld= scene->world;
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
	
	if(re) invert_m4_m4(mat, re->viewmat);
	else unit_m4(mat);
	
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

	if(re) /* add render object for stars */
		obr= RE_addRenderObject(re, NULL, NULL, 0, 0, 0);
	
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
					mul_m4_v3(re->viewmat, vec);
					
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
						
						har = initstar(re, obr, vec, fac);
						
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

	if(obr)
		re->tothalo += obr->tothalo;
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

static void split_v_renderfaces(ObjectRen *obr, int startvlak, int startvert, int usize, int vsize, int uIndex, int cyclu, int cyclv)
{
	int vLen = vsize-1+(!!cyclv);
	int v;

	for (v=0; v<vLen; v++) {
		VlakRen *vlr = RE_findOrAddVlak(obr, startvlak + vLen*uIndex + v);
		VertRen *vert = RE_vertren_copy(obr, vlr->v2);

		if (cyclv) {
			vlr->v2 = vert;

			if (v==vLen-1) {
				VlakRen *vlr = RE_findOrAddVlak(obr, startvlak + vLen*uIndex + 0);
				vlr->v1 = vert;
			} else {
				VlakRen *vlr = RE_findOrAddVlak(obr, startvlak + vLen*uIndex + v+1);
				vlr->v1 = vert;
			}
		} else {
			vlr->v2 = vert;

			if (v<vLen-1) {
				VlakRen *vlr = RE_findOrAddVlak(obr, startvlak + vLen*uIndex + v+1);
				vlr->v1 = vert;
			}

			if (v==0) {
				vlr->v1 = RE_vertren_copy(obr, vlr->v1);
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
/* Stress, tangents and normals                                              */
/* ------------------------------------------------------------------------- */

static void calc_edge_stress_add(float *accum, VertRen *v1, VertRen *v2)
{
	float len= len_v3v3(v1->co, v2->co)/len_v3v3(v1->orco, v2->orco);
	float *acc;
	
	acc= accum + 2*v1->index;
	acc[0]+= len;
	acc[1]+= 1.0f;
	
	acc= accum + 2*v2->index;
	acc[0]+= len;
	acc[1]+= 1.0f;
}

static void calc_edge_stress(Render *re, ObjectRen *obr, Mesh *me)
{
	float loc[3], size[3], *accum, *acc, *accumoffs, *stress;
	int a;
	
	if(obr->totvert==0) return;
	
	mesh_get_texspace(me, loc, NULL, size);
	
	accum= MEM_callocN(2*sizeof(float)*obr->totvert, "temp accum for stress");
	
	/* de-normalize orco */
	for(a=0; a<obr->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(obr, a);
		if(ver->orco) {
			ver->orco[0]= ver->orco[0]*size[0] +loc[0];
			ver->orco[1]= ver->orco[1]*size[1] +loc[1];
			ver->orco[2]= ver->orco[2]*size[2] +loc[2];
		}
	}
	
	/* add stress values */
	accumoffs= accum;	/* so we can use vertex index */
	for(a=0; a<obr->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(obr, a);

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
	
	for(a=0; a<obr->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(obr, a);
		if(ver->orco) {
			/* find stress value */
			acc= accumoffs + 2*ver->index;
			if(acc[1]!=0.0f)
				acc[0]/= acc[1];
			stress= RE_vertren_get_stress(obr, ver, 1);
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
static void calc_tangent_vector(ObjectRen *obr, VertexTangent **vtangents, MemArena *arena, VlakRen *vlr, int do_nmap_tangent, int do_tangent)
{
	MTFace *tface= RE_vlakren_get_tface(obr, vlr, obr->actmtface, NULL, 0);
	VertRen *v1=vlr->v1, *v2=vlr->v2, *v3=vlr->v3, *v4=vlr->v4;
	float tang[3], *tav;
	float *uv1, *uv2, *uv3, *uv4;
	float uv[4][2];
	
	if(tface) {
		uv1= tface->uv[0];
		uv2= tface->uv[1];
		uv3= tface->uv[2];
		uv4= tface->uv[3];
	}
	else if(v1->orco) {
		uv1= uv[0]; uv2= uv[1]; uv3= uv[2]; uv4= uv[3];
		map_to_sphere( &uv[0][0], &uv[0][1],v1->orco[0], v1->orco[1], v1->orco[2]);
		map_to_sphere( &uv[1][0], &uv[1][1],v2->orco[0], v2->orco[1], v2->orco[2]);
		map_to_sphere( &uv[2][0], &uv[2][1],v3->orco[0], v3->orco[1], v3->orco[2]);
		if(v4)
			map_to_sphere( &uv[3][0], &uv[3][1],v4->orco[0], v4->orco[1], v4->orco[2]);
	}
	else return;

	tangent_from_uv(uv1, uv2, uv3, v1->co, v2->co, v3->co, vlr->n, tang);
	
	if(do_tangent) {
		tav= RE_vertren_get_tangent(obr, v1, 1);
		VECADD(tav, tav, tang);
		tav= RE_vertren_get_tangent(obr, v2, 1);
		VECADD(tav, tav, tang);
		tav= RE_vertren_get_tangent(obr, v3, 1);
		VECADD(tav, tav, tang);
	}
	
	if(do_nmap_tangent) {
		sum_or_add_vertex_tangent(arena, &vtangents[v1->index], tang, uv1);
		sum_or_add_vertex_tangent(arena, &vtangents[v2->index], tang, uv2);
		sum_or_add_vertex_tangent(arena, &vtangents[v3->index], tang, uv3);
	}

	if(v4) {
		tangent_from_uv(uv1, uv3, uv4, v1->co, v3->co, v4->co, vlr->n, tang);
		
		if(do_tangent) {
			tav= RE_vertren_get_tangent(obr, v1, 1);
			VECADD(tav, tav, tang);
			tav= RE_vertren_get_tangent(obr, v3, 1);
			VECADD(tav, tav, tang);
			tav= RE_vertren_get_tangent(obr, v4, 1);
			VECADD(tav, tav, tang);
		}

		if(do_nmap_tangent) {
			sum_or_add_vertex_tangent(arena, &vtangents[v1->index], tang, uv1);
			sum_or_add_vertex_tangent(arena, &vtangents[v3->index], tang, uv3);
			sum_or_add_vertex_tangent(arena, &vtangents[v4->index], tang, uv4);
		}
	}
}


static void calc_vertexnormals(Render *re, ObjectRen *obr, int do_tangent, int do_nmap_tangent)
{
	MemArena *arena= NULL;
	VertexTangent **vtangents= NULL;
	int a;

	if(do_nmap_tangent) {
		arena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
		BLI_memarena_use_calloc(arena);

		vtangents= MEM_callocN(sizeof(VertexTangent*)*obr->totvert, "VertexTangent");
	}

		/* clear all vertex normals */
	for(a=0; a<obr->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(obr, a);
		ver->n[0]=ver->n[1]=ver->n[2]= 0.0f;
	}

		/* calculate cos of angles and point-masses, use as weight factor to
		   add face normal to vertex */
	for(a=0; a<obr->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(obr, a);
		if(vlr->flag & ME_SMOOTH) {
			VertRen *v1= vlr->v1;
			VertRen *v2= vlr->v2;
			VertRen *v3= vlr->v3;
			VertRen *v4= vlr->v4;
			float n1[3], n2[3], n3[3], n4[3];
			float fac1, fac2, fac3, fac4=0.0f;
			
			if(re->flag & R_GLOB_NOPUNOFLIP)
				vlr->flag |= R_NOPUNOFLIP;
			
			sub_v3_v3v3(n1, v2->co, v1->co);
			normalize_v3(n1);
			sub_v3_v3v3(n2, v3->co, v2->co);
			normalize_v3(n2);
			if(v4==NULL) {
				sub_v3_v3v3(n3, v1->co, v3->co);
				normalize_v3(n3);

				fac1= saacos(-n1[0]*n3[0]-n1[1]*n3[1]-n1[2]*n3[2]);
				fac2= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
				fac3= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			}
			else {
				sub_v3_v3v3(n3, v4->co, v3->co);
				normalize_v3(n3);
				sub_v3_v3v3(n4, v1->co, v4->co);
				normalize_v3(n4);

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
		if(do_nmap_tangent || do_tangent) {
			/* tangents still need to be calculated for flat faces too */
			/* weighting removed, they are not vertexnormals */
			calc_tangent_vector(obr, vtangents, arena, vlr, do_nmap_tangent, do_tangent);
		}
	}

		/* do solid faces */
	for(a=0; a<obr->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(obr, a);
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

		if(do_nmap_tangent) {
			VertRen *v1=vlr->v1, *v2=vlr->v2, *v3=vlr->v3, *v4=vlr->v4;
			MTFace *tface= RE_vlakren_get_tface(obr, vlr, obr->actmtface, NULL, 0);

			if(tface) {
				float *vtang, *ftang= RE_vlakren_get_nmap_tangent(obr, vlr, 1);

				vtang= find_vertex_tangent(vtangents[v1->index], tface->uv[0]);
				VECCOPY(ftang, vtang);
				normalize_v3(ftang);
				vtang= find_vertex_tangent(vtangents[v2->index], tface->uv[1]);
				VECCOPY(ftang+3, vtang);
				normalize_v3(ftang+3);
				vtang= find_vertex_tangent(vtangents[v3->index], tface->uv[2]);
				VECCOPY(ftang+6, vtang);
				normalize_v3(ftang+6);
				if(v4) {
					vtang= find_vertex_tangent(vtangents[v4->index], tface->uv[3]);
					VECCOPY(ftang+9, vtang);
					normalize_v3(ftang+9);
				}
			}
		}
	}
	
		/* normalize vertex normals */
	for(a=0; a<obr->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(obr, a);
		normalize_v3(ver->n);
		if(do_tangent) {
			float *tav= RE_vertren_get_tangent(obr, ver, 0);
			if (tav) {
				/* orthonorm. */
				float tdn = tav[0]*ver->n[0] + tav[1]*ver->n[1] + tav[2]*ver->n[2];
				tav[0] -= ver->n[0]*tdn;
				tav[1] -= ver->n[1]*tdn;
				tav[2] -= ver->n[2]*tdn;
				normalize_v3(tav);
			}
		}
	}


	if(arena)
		BLI_memarena_free(arena);
	if(vtangents)
		MEM_freeN(vtangents);
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
static void autosmooth(Render *re, ObjectRen *obr, float mat[][4], int degr)
{
	ASvert *asv, *asverts;
	ASface *asf;
	VertRen *ver, *v1;
	VlakRen *vlr;
	float thresh;
	int a, b, totvert;
	
	if(obr->totvert==0) return;
	asverts= MEM_callocN(sizeof(ASvert)*obr->totvert, "all smooth verts");
	
	thresh= cos( M_PI*(0.5f+(float)degr)/180.0 );
	
	/* step zero: give faces normals of original mesh, if this is provided */
	
	
	/* step one: construct listbase of all vertices and pointers to faces */
	for(a=0; a<obr->totvlak; a++) {
		vlr= RE_findOrAddVlak(obr, a);
		/* skip wire faces */
		if(vlr->v2 != vlr->v3) {
			as_addvert(asverts+vlr->v1->index, vlr->v1, vlr);
			as_addvert(asverts+vlr->v2->index, vlr->v2, vlr);
			as_addvert(asverts+vlr->v3->index, vlr->v3, vlr);
			if(vlr->v4) 
				as_addvert(asverts+vlr->v4->index, vlr->v4, vlr);
		}
	}
	
	totvert= obr->totvert;
	/* we now test all vertices, when faces have a normal too much different: they get a new vertex */
	for(a=0, asv=asverts; a<totvert; a++, asv++) {
		if(asv && asv->totface>1) {
			ver= RE_findOrAddVert(obr, a);

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
							v1= RE_vertren_copy(obr, ver);
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
	for(a=0; a<totvert; a++) {
		BLI_freelistN(&asverts[a].faces);
	}
	MEM_freeN(asverts);
	
	/* rotate vertices and calculate normal of faces */
	for(a=0; a<obr->totvert; a++) {
		ver= RE_findOrAddVert(obr, a);
		mul_m4_v3(mat, ver->co);
	}
	for(a=0; a<obr->totvlak; a++) {
		vlr= RE_findOrAddVlak(obr, a);
		
		/* skip wire faces */
		if(vlr->v2 != vlr->v3) {
			if(vlr->v4) 
				normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
			else 
				normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);
		}
	}		
}

/* ------------------------------------------------------------------------- */
/* Orco hash and Materials                                                   */
/* ------------------------------------------------------------------------- */

static float *get_object_orco(Render *re, Object *ob)
{
	float *orco;
	
	if (!re->orco_hash)
		re->orco_hash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	orco = BLI_ghash_lookup(re->orco_hash, ob);
	
	if (!orco) {
		if (ELEM(ob->type, OB_CURVE, OB_FONT)) {
			orco = make_orco_curve(re->scene, ob);
		} else if (ob->type==OB_SURF) {
			orco = make_orco_surf(ob);
		} else if (ob->type==OB_MBALL) {
			orco = make_orco_mball(ob);
		}
		
		if (orco)
			BLI_ghash_insert(re->orco_hash, ob, orco);
	}
	
	return orco;
}

static void set_object_orco(Render *re, void *ob, float *orco)
{
	if (!re->orco_hash)
		re->orco_hash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	BLI_ghash_insert(re->orco_hash, ob, orco);
}

static void free_mesh_orco_hash(Render *re) 
{
	if (re->orco_hash) {
		BLI_ghash_free(re->orco_hash, NULL, (GHashValFreeFP)MEM_freeN);
		re->orco_hash = NULL;
	}
}

static void check_material_mapto(Material *ma)
{
	int a;
	ma->mapto_textured = 0;
	
	/* cache which inputs are actually textured.
	 * this can avoid a bit of time spent iterating through all the texture slots, map inputs and map tos
	 * every time a property which may or may not be textured is accessed */
	
	for(a=0; a<MAX_MTEX; a++) {
		if(ma->mtex[a] && ma->mtex[a]->tex) {
			/* currently used only in volume render, so we'll check for those flags */
			if(ma->mtex[a]->mapto & MAP_DENSITY) ma->mapto_textured |= MAP_DENSITY;
			if(ma->mtex[a]->mapto & MAP_EMISSION) ma->mapto_textured |= MAP_EMISSION;
			if(ma->mtex[a]->mapto & MAP_EMISSION_COL) ma->mapto_textured |= MAP_EMISSION_COL;
			if(ma->mtex[a]->mapto & MAP_SCATTERING) ma->mapto_textured |= MAP_SCATTERING;
			if(ma->mtex[a]->mapto & MAP_TRANSMISSION_COL) ma->mapto_textured |= MAP_TRANSMISSION_COL;
			if(ma->mtex[a]->mapto & MAP_REFLECTION) ma->mapto_textured |= MAP_REFLECTION;
			if(ma->mtex[a]->mapto & MAP_REFLECTION_COL) ma->mapto_textured |= MAP_REFLECTION_COL;
		}
	}
}
static void flag_render_node_material(Render *re, bNodeTree *ntree)
{
	bNode *node;

	for(node=ntree->nodes.first; node; node= node->next) {
		if(node->id) {
			if(GS(node->id->name)==ID_MA) {
				Material *ma= (Material *)node->id;

				if((ma->mode & MA_TRANSP) && (ma->mode & MA_ZTRANSP))
					re->flag |= R_ZTRA;

				ma->flag |= MA_IS_USED;
			}
			else if(node->type==NODE_GROUP)
				flag_render_node_material(re, (bNodeTree *)node->id);
		}
	}
}

static Material *give_render_material(Render *re, Object *ob, int nr)
{
	extern Material defmaterial;	/* material.c */
	Material *ma;
	
	ma= give_current_material(ob, nr);
	if(ma==NULL) 
		ma= &defmaterial;
	
	if(re->r.mode & R_SPEED) ma->texco |= NEED_UV;
	
	if(ma->material_type == MA_TYPE_VOLUME) {
		ma->mode |= MA_TRANSP;
		ma->mode &= ~MA_SHADBUF;
	}
	if((ma->mode & MA_TRANSP) && (ma->mode & MA_ZTRANSP))
		re->flag |= R_ZTRA;
	
	/* for light groups */
	ma->flag |= MA_IS_USED;

	if(ma->nodetree && ma->use_nodes)
		flag_render_node_material(re, ma->nodetree);
	
	check_material_mapto(ma);
	
	return ma;
}

/* ------------------------------------------------------------------------- */
/* Particles                                                                 */
/* ------------------------------------------------------------------------- */
typedef struct ParticleStrandData
{
	struct MCol *mcol;
	float *orco, *uvco, *surfnor;
	float time, adapt_angle, adapt_pix, size;
	int totuv, totcol;
	int first, line, adapt, override_uv;
}
ParticleStrandData;
/* future thread problem... */
static void static_particle_strand(Render *re, ObjectRen *obr, Material *ma, ParticleStrandData *sd, float *vec, float *vec1)
{
	static VertRen *v1= NULL, *v2= NULL;
	VlakRen *vlr= NULL;
	float nor[3], cross[3], crosslen, w, dx, dy, width;
	static float anor[3], avec[3];
	int flag, i;
	static int second=0;
	
	sub_v3_v3v3(nor, vec, vec1);
	normalize_v3(nor);		// nor needed as tangent 
	cross_v3_v3v3(cross, vec, nor);

	/* turn cross in pixelsize */
	w= vec[2]*re->winmat[2][3] + re->winmat[3][3];
	dx= re->winx*cross[0]*re->winmat[0][0];
	dy= re->winy*cross[1]*re->winmat[1][1];
	w= sqrt(dx*dx + dy*dy)/w;
	
	if(w!=0.0f) {
		float fac;
		if(ma->strand_ease!=0.0f) {
			if(ma->strand_ease<0.0f)
				fac= pow(sd->time, 1.0+ma->strand_ease);
			else
				fac= pow(sd->time, 1.0/(1.0f-ma->strand_ease));
		}
		else fac= sd->time;

		width= ((1.0f-fac)*ma->strand_sta + (fac)*ma->strand_end);

		/* use actual Blender units for strand width and fall back to minimum width */
		if(ma->mode & MA_STR_B_UNITS){
            crosslen= len_v3(cross);
            w= 2.0f*crosslen*ma->strand_min/w;

			if(width < w)
				width= w;

			/*cross is the radius of the strand so we want it to be half of full width */
			mul_v3_fl(cross,0.5/crosslen);
		}
		else
			width/=w;

		mul_v3_fl(cross, width);
	}
	else width= 1.0f;
	
	if(ma->mode & MA_TANGENT_STR)
		flag= R_SMOOTH|R_NOPUNOFLIP|R_TANGENT;
	else
		flag= R_SMOOTH;
	
	/* only 1 pixel wide strands filled in as quads now, otherwise zbuf errors */
	if(ma->strand_sta==1.0f)
		flag |= R_STRAND;
	
	/* single face line */
	if(sd->line) {
		vlr= RE_findOrAddVlak(obr, obr->totvlak++);
		vlr->flag= flag;
		vlr->v1= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v2= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v3= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v4= RE_findOrAddVert(obr, obr->totvert++);
		
		VECCOPY(vlr->v1->co, vec);
		add_v3_v3v3(vlr->v1->co, vlr->v1->co, cross);
		VECCOPY(vlr->v1->n, nor);
		vlr->v1->orco= sd->orco;
		vlr->v1->accum= -1.0f;	// accum abuse for strand texco
		
		VECCOPY(vlr->v2->co, vec);
		sub_v3_v3v3(vlr->v2->co, vlr->v2->co, cross);
		VECCOPY(vlr->v2->n, nor);
		vlr->v2->orco= sd->orco;
		vlr->v2->accum= vlr->v1->accum;

		VECCOPY(vlr->v4->co, vec1);
		add_v3_v3v3(vlr->v4->co, vlr->v4->co, cross);
		VECCOPY(vlr->v4->n, nor);
		vlr->v4->orco= sd->orco;
		vlr->v4->accum= 1.0f;	// accum abuse for strand texco
		
		VECCOPY(vlr->v3->co, vec1);
		sub_v3_v3v3(vlr->v3->co, vlr->v3->co, cross);
		VECCOPY(vlr->v3->n, nor);
		vlr->v3->orco= sd->orco;
		vlr->v3->accum= vlr->v4->accum;

		normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
		
		vlr->mat= ma;
		vlr->ec= ME_V2V3;

		if(sd->surfnor) {
			float *snor= RE_vlakren_get_surfnor(obr, vlr, 1);
			VECCOPY(snor, sd->surfnor);
		}

		if(sd->uvco){
			for(i=0; i<sd->totuv; i++){
				MTFace *mtf;
				mtf=RE_vlakren_get_tface(obr,vlr,i,NULL,1);
				mtf->uv[0][0]=mtf->uv[1][0]=
				mtf->uv[2][0]=mtf->uv[3][0]=(sd->uvco+2*i)[0];
				mtf->uv[0][1]=mtf->uv[1][1]=
				mtf->uv[2][1]=mtf->uv[3][1]=(sd->uvco+2*i)[1];
			}
			if(sd->override_uv>=0){
				MTFace *mtf;
				mtf=RE_vlakren_get_tface(obr,vlr,sd->override_uv,NULL,0);
				
				mtf->uv[0][0]=mtf->uv[3][0]=0.0f;
				mtf->uv[1][0]=mtf->uv[2][0]=1.0f;

				mtf->uv[0][1]=mtf->uv[1][1]=0.0f;
				mtf->uv[2][1]=mtf->uv[3][1]=1.0f;
			}
		}
		if(sd->mcol){
			for(i=0; i<sd->totcol; i++){
				MCol *mc;
				mc=RE_vlakren_get_mcol(obr,vlr,i,NULL,1);
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
			}
		}
	}
	/* first two vertices of a strand */
	else if(sd->first) {
		if(sd->adapt){
			VECCOPY(anor, nor);
			VECCOPY(avec, vec);
			second=1;
		}

		v1= RE_findOrAddVert(obr, obr->totvert++);
		v2= RE_findOrAddVert(obr, obr->totvert++);
		
		VECCOPY(v1->co, vec);
		add_v3_v3v3(v1->co, v1->co, cross);
		VECCOPY(v1->n, nor);
		v1->orco= sd->orco;
		v1->accum= -1.0f;	// accum abuse for strand texco
		
		VECCOPY(v2->co, vec);
		sub_v3_v3v3(v2->co, v2->co, cross);
		VECCOPY(v2->n, nor);
		v2->orco= sd->orco;
		v2->accum= v1->accum;
	}
	/* more vertices & faces to strand */
	else {
		if(sd->adapt==0 || second){
			vlr= RE_findOrAddVlak(obr, obr->totvlak++);
			vlr->flag= flag;
			vlr->v1= v1;
			vlr->v2= v2;
			vlr->v3= RE_findOrAddVert(obr, obr->totvert++);
			vlr->v4= RE_findOrAddVert(obr, obr->totvert++);
			
			v1= vlr->v4; // cycle
			v2= vlr->v3; // cycle

			
			if(sd->adapt){
				second=0;
				VECCOPY(anor,nor);
				VECCOPY(avec,vec);
			}

		}
		else if(sd->adapt){
			float dvec[3],pvec[3];
			sub_v3_v3v3(dvec,avec,vec);
			project_v3_v3v3(pvec,dvec,vec);
			sub_v3_v3v3(dvec,dvec,pvec);

			w= vec[2]*re->winmat[2][3] + re->winmat[3][3];
			dx= re->winx*dvec[0]*re->winmat[0][0]/w;
			dy= re->winy*dvec[1]*re->winmat[1][1]/w;
			w= sqrt(dx*dx + dy*dy);
			if(dot_v3v3(anor,nor)<sd->adapt_angle && w>sd->adapt_pix){
				vlr= RE_findOrAddVlak(obr, obr->totvlak++);
				vlr->flag= flag;
				vlr->v1= v1;
				vlr->v2= v2;
				vlr->v3= RE_findOrAddVert(obr, obr->totvert++);
				vlr->v4= RE_findOrAddVert(obr, obr->totvert++);
				
				v1= vlr->v4; // cycle
				v2= vlr->v3; // cycle

				VECCOPY(anor,nor);
				VECCOPY(avec,vec);
			}
			else{
				vlr= RE_findOrAddVlak(obr, obr->totvlak-1);
			}
		}
	
		VECCOPY(vlr->v4->co, vec);
		add_v3_v3v3(vlr->v4->co, vlr->v4->co, cross);
		VECCOPY(vlr->v4->n, nor);
		vlr->v4->orco= sd->orco;
		vlr->v4->accum= -1.0f + 2.0f*sd->time;	// accum abuse for strand texco
		
		VECCOPY(vlr->v3->co, vec);
		sub_v3_v3v3(vlr->v3->co, vlr->v3->co, cross);
		VECCOPY(vlr->v3->n, nor);
		vlr->v3->orco= sd->orco;
		vlr->v3->accum= vlr->v4->accum;
		
		normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
		
		vlr->mat= ma;
		vlr->ec= ME_V2V3;

		if(sd->surfnor) {
			float *snor= RE_vlakren_get_surfnor(obr, vlr, 1);
			VECCOPY(snor, sd->surfnor);
		}

		if(sd->uvco){
			for(i=0; i<sd->totuv; i++){
				MTFace *mtf;
				mtf=RE_vlakren_get_tface(obr,vlr,i,NULL,1);
				mtf->uv[0][0]=mtf->uv[1][0]=
				mtf->uv[2][0]=mtf->uv[3][0]=(sd->uvco+2*i)[0];
				mtf->uv[0][1]=mtf->uv[1][1]=
				mtf->uv[2][1]=mtf->uv[3][1]=(sd->uvco+2*i)[1];
			}
			if(sd->override_uv>=0){
				MTFace *mtf;
				mtf=RE_vlakren_get_tface(obr,vlr,sd->override_uv,NULL,0);
				
				mtf->uv[0][0]=mtf->uv[3][0]=0.0f;
				mtf->uv[1][0]=mtf->uv[2][0]=1.0f;

				mtf->uv[0][1]=mtf->uv[1][1]=(vlr->v1->accum+1.0f)/2.0f;
				mtf->uv[2][1]=mtf->uv[3][1]=(vlr->v3->accum+1.0f)/2.0f;
			}
		}
		if(sd->mcol){
			for(i=0; i<sd->totcol; i++){
				MCol *mc;
				mc=RE_vlakren_get_mcol(obr,vlr,i,NULL,1);
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
			}
		}
	}
}

static void static_particle_wire(ObjectRen *obr, Material *ma, float *vec, float *vec1, int first, int line)
{
	VlakRen *vlr;
	static VertRen *v1;

	if(line) {
		vlr= RE_findOrAddVlak(obr, obr->totvlak++);
		vlr->v1= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v2= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v3= vlr->v2;
		vlr->v4= NULL;
		
		VECCOPY(vlr->v1->co, vec);
		VECCOPY(vlr->v2->co, vec1);
		
		sub_v3_v3v3(vlr->n, vec, vec1);
		normalize_v3(vlr->n);
		VECCOPY(vlr->v1->n, vlr->n);
		VECCOPY(vlr->v2->n, vlr->n);
		
		vlr->mat= ma;
		vlr->ec= ME_V1V2;

	}
	else if(first) {
		v1= RE_findOrAddVert(obr, obr->totvert++);
		VECCOPY(v1->co, vec);
	}
	else {
		vlr= RE_findOrAddVlak(obr, obr->totvlak++);
		vlr->v1= v1;
		vlr->v2= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v3= vlr->v2;
		vlr->v4= NULL;
		
		v1= vlr->v2; // cycle
		VECCOPY(v1->co, vec);
		
		sub_v3_v3v3(vlr->n, vec, vec1);
		normalize_v3(vlr->n);
		VECCOPY(v1->n, vlr->n);
		
		vlr->mat= ma;
		vlr->ec= ME_V1V2;
	}

}

static void particle_curve(Render *re, ObjectRen *obr, DerivedMesh *dm, Material *ma, ParticleStrandData *sd, float *loc, float *loc1,	int seed)
{
	HaloRen *har=0;

	if(ma->material_type == MA_TYPE_WIRE)
		static_particle_wire(obr, ma, loc, loc1, sd->first, sd->line);
	else if(ma->material_type == MA_TYPE_HALO) {
		har= RE_inithalo_particle(re, obr, dm, ma, loc, loc1, sd->orco, sd->uvco, sd->size, 1.0, seed);
		if(har) har->lay= obr->ob->lay;
	}
	else
		static_particle_strand(re, obr, ma, sd, loc, loc1);
}
static void particle_billboard(Render *re, ObjectRen *obr, Material *ma, ParticleBillboardData *bb)
{
	VlakRen *vlr;
	MTFace *mtf;
	float xvec[3], yvec[3], zvec[3], bb_center[3];
	float uvx = 0.0f, uvy = 0.0f, uvdx = 1.0f, uvdy = 1.0f, time = 0.0f;

	vlr= RE_findOrAddVlak(obr, obr->totvlak++);
	vlr->v1= RE_findOrAddVert(obr, obr->totvert++);
	vlr->v2= RE_findOrAddVert(obr, obr->totvert++);
	vlr->v3= RE_findOrAddVert(obr, obr->totvert++);
	vlr->v4= RE_findOrAddVert(obr, obr->totvert++);

	psys_make_billboard(bb, xvec, yvec, zvec, bb_center);

	VECADD(vlr->v1->co, bb_center, xvec);
	VECADD(vlr->v1->co, vlr->v1->co, yvec);
	mul_m4_v3(re->viewmat, vlr->v1->co);

	VECSUB(vlr->v2->co, bb_center, xvec);
	VECADD(vlr->v2->co, vlr->v2->co, yvec);
	mul_m4_v3(re->viewmat, vlr->v2->co);

	VECSUB(vlr->v3->co, bb_center, xvec);
	VECSUB(vlr->v3->co, vlr->v3->co, yvec);
	mul_m4_v3(re->viewmat, vlr->v3->co);

	VECADD(vlr->v4->co, bb_center, xvec);
	VECSUB(vlr->v4->co, vlr->v4->co, yvec);
	mul_m4_v3(re->viewmat, vlr->v4->co);

	normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
	VECCOPY(vlr->v1->n,vlr->n);
	VECCOPY(vlr->v2->n,vlr->n);
	VECCOPY(vlr->v3->n,vlr->n);
	VECCOPY(vlr->v4->n,vlr->n);
	
	vlr->mat= ma;
	vlr->ec= ME_V2V3;

	if(bb->uv_split > 1){
		uvdx = uvdy = 1.0f / (float)bb->uv_split;
		if(bb->anim == PART_BB_ANIM_TIME) {
			if(bb->split_offset == PART_BB_OFF_NONE)
				time = bb->time;
			else if(bb->split_offset == PART_BB_OFF_LINEAR)
				time = (float)fmod(bb->time + (float)bb->num / (float)(bb->uv_split * bb->uv_split), 1.0f);
			else /* split_offset==PART_BB_OFF_RANDOM */
				time = (float)fmod(bb->time + bb->random, 1.0f);

		}
		else if(bb->anim == PART_BB_ANIM_ANGLE) {
			if(bb->align == PART_BB_VIEW) {
				time = (float)fmod((bb->tilt + 1.0f) / 2.0f, 1.0);
			}
			else{
				float axis1[3] = {0.0f,0.0f,0.0f};
				float axis2[3] = {0.0f,0.0f,0.0f};
				axis1[(bb->align + 1) % 3] = 1.0f;
				axis2[(bb->align + 2) % 3] = 1.0f;
				if(bb->lock == 0) {
					zvec[bb->align] = 0.0f;
					normalize_v3(zvec);
				}
				time = saacos(dot_v3v3(zvec, axis1)) / (float)M_PI;
				if(dot_v3v3(zvec, axis2) < 0.0f)
					time = 1.0f - time / 2.0f;
				else
					time = time / 2.0f;
			}
			if(bb->split_offset == PART_BB_OFF_LINEAR)
				time = (float)fmod(bb->time + (float)bb->num / (float)(bb->uv_split * bb->uv_split), 1.0f);
			else if(bb->split_offset == PART_BB_OFF_RANDOM)
				time = (float)fmod(bb->time + bb->random, 1.0f);
		}
		else{
			if(bb->split_offset == PART_BB_OFF_NONE)
				time = 0.0f;
			else if(bb->split_offset == PART_BB_OFF_LINEAR)
				time = (float)fmod((float)bb->num /(float)(bb->uv_split * bb->uv_split) , 1.0f);
			else /* split_offset==PART_BB_OFF_RANDOM */
				time = bb->random;
		}
		uvx = uvdx * floor((float)(bb->uv_split * bb->uv_split) * (float)fmod((double)time, (double)uvdx));
		uvy = uvdy * floor((1.0f - time) * (float)bb->uv_split);
		if(fmod(time, 1.0f / bb->uv_split) == 0.0f)
			uvy -= uvdy;
	}

	/* normal UVs */
	if(bb->uv[0] >= 0){
		mtf = RE_vlakren_get_tface(obr, vlr, bb->uv[0], NULL, 1);
		mtf->uv[0][0] = 1.0f;
		mtf->uv[0][1] = 1.0f;
		mtf->uv[1][0] = 0.0f;
		mtf->uv[1][1] = 1.0f;
		mtf->uv[2][0] = 0.0f;
		mtf->uv[2][1] = 0.0f;
		mtf->uv[3][0] = 1.0f;
		mtf->uv[3][1] = 0.0f;
	}

	/* time-index UVs */
	if(bb->uv[1] >= 0){
		mtf = RE_vlakren_get_tface(obr, vlr, bb->uv[1], NULL, 1);
		mtf->uv[0][0] = mtf->uv[1][0] = mtf->uv[2][0] = mtf->uv[3][0] = bb->time;
		mtf->uv[0][1] = mtf->uv[1][1] = mtf->uv[2][1] = mtf->uv[3][1] = (float)bb->num/(float)bb->totnum;
	}

	/* split UVs */
	if(bb->uv_split > 1 && bb->uv[2] >= 0){
		mtf = RE_vlakren_get_tface(obr, vlr, bb->uv[2], NULL, 1);
		mtf->uv[0][0] = uvx + uvdx;
		mtf->uv[0][1] = uvy + uvdy;
		mtf->uv[1][0] = uvx;
		mtf->uv[1][1] = uvy + uvdy;
		mtf->uv[2][0] = uvx;
		mtf->uv[2][1] = uvy;
		mtf->uv[3][0] = uvx + uvdx;
		mtf->uv[3][1] = uvy;
	}
}
static void particle_normal_ren(short ren_as, ParticleSettings *part, Render *re, ObjectRen *obr, DerivedMesh *dm, Material *ma, ParticleStrandData *sd, ParticleBillboardData *bb, ParticleKey *state, int seed, float hasize)
{
	float loc[3], loc0[3], loc1[3], vel[3];
	
	VECCOPY(loc, state->co);

	if(ren_as != PART_DRAW_BB)
		mul_m4_v3(re->viewmat, loc);

	switch(ren_as) {
		case PART_DRAW_LINE:
			sd->line = 1;
			sd->time = 0.0f;
			sd->size = hasize;

			VECCOPY(vel, state->vel);
			mul_mat3_m4_v3(re->viewmat, vel);
			normalize_v3(vel);

			if(part->draw & PART_DRAW_VEL_LENGTH)
				mul_v3_fl(vel, len_v3(state->vel));

			VECADDFAC(loc0, loc, vel, -part->draw_line[0]);
			VECADDFAC(loc1, loc, vel, part->draw_line[1]);

			particle_curve(re, obr, dm, ma, sd, loc0, loc1, seed);

			break;

		case PART_DRAW_BB:

			VECCOPY(bb->vec, loc);
			VECCOPY(bb->vel, state->vel);

			particle_billboard(re, obr, ma, bb);

			break;

		default:
		{
			HaloRen *har=0;

			har = RE_inithalo_particle(re, obr, dm, ma, loc, NULL, sd->orco, sd->uvco, hasize, 0.0, seed);
			
			if(har) har->lay= obr->ob->lay;

			break;
		}
	}
}
static void get_particle_uvco_mcol(short from, DerivedMesh *dm, float *fuv, int num, ParticleStrandData *sd)
{
	int i;

	/* get uvco */
	if(sd->uvco && ELEM(from,PART_FROM_FACE,PART_FROM_VOLUME)) {
		for(i=0; i<sd->totuv; i++) {
			if(num != DMCACHE_NOTFOUND) {
				MFace *mface = dm->getFaceData(dm, num, CD_MFACE);
				MTFace *mtface = (MTFace*)CustomData_get_layer_n(&dm->faceData, CD_MTFACE, i);
				mtface += num;
				
				psys_interpolate_uvs(mtface, mface->v4, fuv, sd->uvco + 2 * i);
			}
			else {
				sd->uvco[2*i] = 0.0f;
				sd->uvco[2*i + 1] = 0.0f;
			}
		}
	}

	/* get mcol */
	if(sd->mcol && ELEM(from,PART_FROM_FACE,PART_FROM_VOLUME)) {
		for(i=0; i<sd->totcol; i++) {
			if(num != DMCACHE_NOTFOUND) {
				MFace *mface = dm->getFaceData(dm, num, CD_MFACE);
				MCol *mc = (MCol*)CustomData_get_layer_n(&dm->faceData, CD_MCOL, i);
				mc += num * 4;

				psys_interpolate_mcol(mc, mface->v4, fuv, sd->mcol + i);
			}
			else
				memset(&sd->mcol[i], 0, sizeof(MCol));
		}
	}
}
static int render_new_particle_system(Render *re, ObjectRen *obr, ParticleSystem *psys, int timeoffset)
{
	Object *ob= obr->ob;
//	Object *tob=0;
	Material *ma=0;
	ParticleSystemModifierData *psmd;
	ParticleSystem *tpsys=0;
	ParticleSettings *part, *tpart=0;
	ParticleData *pars, *pa=0,*tpa=0;
	ParticleKey *states=0;
	ParticleKey state;
	ParticleCacheKey *cache=0;
	ParticleBillboardData bb;
	ParticleSimulationData sim = {re->scene, ob, psys, NULL};
	ParticleStrandData sd;
	StrandBuffer *strandbuf=0;
	StrandVert *svert=0;
	StrandBound *sbound= 0;
	StrandRen *strand=0;
	RNG *rng= 0;
	float loc[3],loc1[3],loc0[3],mat[4][4],nmat[3][3],co[3],nor[3],time;
	float strandlen=0.0f, curlen=0.0f;
	float hasize, pa_size, r_tilt, r_length, cfra=bsystem_time(re->scene, ob, (float)re->scene->r.cfra, 0.0);
	float pa_time, pa_birthtime, pa_dietime;
	float random, simplify[2];
	int i, a, k, max_k=0, totpart, dosimplify = 0, dosurfacecache = 0;
	int totchild=0;
	int seed, path_nbr=0, orco1=0, num;
	int totface, *origindex = 0;
	char **uv_name=0;

/* 1. check that everything is ok & updated */
	if(psys==NULL)
		return 0;
	
	totchild=psys->totchild;

	part=psys->part;
	pars=psys->particles;

	if(part==NULL || pars==NULL || !psys_check_enabled(ob, psys))
		return 0;
	
	if(part->ren_as==PART_DRAW_OB || part->ren_as==PART_DRAW_GR || part->ren_as==PART_DRAW_NOT)
		return 1;

/* 2. start initialising things */

	/* last possibility to bail out! */
	sim.psmd = psmd = psys_get_modifier(ob,psys);
	if(!(psmd->modifier.mode & eModifierMode_Render))
		return 0;

	if(part->phystype==PART_PHYS_KEYED)
		psys_count_keyed_targets(&sim);


	if(G.rendering == 0) { /* preview render */
		totchild = (int)((float)totchild * (float)part->disp / 100.0f);
	}

	psys->flag |= PSYS_DRAWING;

	rng= rng_new(psys->seed);

	totpart=psys->totpart;

	memset(&sd, 0, sizeof(ParticleStrandData));
	sd.override_uv = -1;

/* 2.1 setup material stff */
	ma= give_render_material(re, ob, part->omat);
	
#if 0 // XXX old animation system
	if(ma->ipo){
		calc_ipo(ma->ipo, cfra);
		execute_ipo((ID *)ma, ma->ipo);
	}
#endif // XXX old animation system

	hasize = ma->hasize;
	seed = ma->seed1;

	re->flag |= R_HALO;

	RE_set_customdata_names(obr, &psmd->dm->faceData);
	sd.totuv = CustomData_number_of_layers(&psmd->dm->faceData, CD_MTFACE);
	sd.totcol = CustomData_number_of_layers(&psmd->dm->faceData, CD_MCOL);

	if(ma->texco & TEXCO_UV && sd.totuv) {
		sd.uvco = MEM_callocN(sd.totuv * 2 * sizeof(float), "particle_uvs");

		if(ma->strand_uvname[0]) {
			sd.override_uv = CustomData_get_named_layer_index(&psmd->dm->faceData, CD_MTFACE, ma->strand_uvname);
			sd.override_uv -= CustomData_get_layer_index(&psmd->dm->faceData, CD_MTFACE);
		}
	}
	else
		sd.uvco = NULL;

	if(sd.totcol)
		sd.mcol = MEM_callocN(sd.totcol * sizeof(MCol), "particle_mcols");

/* 2.2 setup billboards */
	if(part->ren_as == PART_DRAW_BB) {
		int first_uv = CustomData_get_layer_index(&psmd->dm->faceData, CD_MTFACE);

		bb.uv[0] = CustomData_get_named_layer_index(&psmd->dm->faceData, CD_MTFACE, psys->bb_uvname[0]);
		if(bb.uv[0] < 0)
			bb.uv[0] = CustomData_get_active_layer_index(&psmd->dm->faceData, CD_MTFACE);

		bb.uv[1] = CustomData_get_named_layer_index(&psmd->dm->faceData, CD_MTFACE, psys->bb_uvname[1]);

		bb.uv[2] = CustomData_get_named_layer_index(&psmd->dm->faceData, CD_MTFACE, psys->bb_uvname[2]);

		if(first_uv >= 0) {
			bb.uv[0] -= first_uv;
			bb.uv[1] -= first_uv;
			bb.uv[2] -= first_uv;
		}

		bb.align = part->bb_align;
		bb.anim = part->bb_anim;
		bb.lock = part->draw & PART_DRAW_BB_LOCK;
		bb.ob = (part->bb_ob ? part->bb_ob : re->scene->camera);
		bb.offset[0] = part->bb_offset[0];
		bb.offset[1] = part->bb_offset[1];
		bb.split_offset = part->bb_split_offset;
		bb.totnum = totpart+totchild;
		bb.uv_split = part->bb_uv_split;
	}

#if 0 // XXX old animation system
/* 2.3 setup time */
	if(part->flag&PART_ABS_TIME && part->ipo) {
		calc_ipo(part->ipo, cfra);
		execute_ipo((ID *)part, part->ipo);
	}

	if(part->flag & PART_GLOB_TIME)
#endif // XXX old animation system
	cfra = bsystem_time(re->scene, 0, (float)re->scene->r.cfra, 0.0);

///* 2.4 setup reactors */
//	if(part->type == PART_REACTOR){
//		psys_get_reactor_target(ob, psys, &tob, &tpsys);
//		if(tpsys && (part->from==PART_FROM_PARTICLE || part->phystype==PART_PHYS_NO)){
//			psmd = psys_get_modifier(tob,tpsys);
//			tpart = tpsys->part;
//		}
//	}
	
/* 2.5 setup matrices */
	mul_m4_m4m4(mat, ob->obmat, re->viewmat);
	invert_m4_m4(ob->imat, mat);	/* need to be that way, for imat texture */
	copy_m3_m4(nmat, ob->imat);
	transpose_m3(nmat);

/* 2.6 setup strand rendering */
	if(part->ren_as == PART_DRAW_PATH && psys->pathcache){
		path_nbr=(int)pow(2.0,(double) part->ren_step);

		if(path_nbr) {
			if(!ELEM(ma->material_type, MA_TYPE_HALO, MA_TYPE_WIRE)) {
				sd.orco = MEM_mallocN(3*sizeof(float)*(totpart+totchild), "particle orcos");
				set_object_orco(re, psys, sd.orco);
			}
		}

		if(part->draw & PART_DRAW_REN_ADAPT) {
			sd.adapt = 1;
			sd.adapt_pix = (float)part->adapt_pix;
			sd.adapt_angle = cos((float)part->adapt_angle * (float)(M_PI / 180.0));
		}

		if(re->r.renderer==R_INTERN && part->draw&PART_DRAW_REN_STRAND) {
			strandbuf= RE_addStrandBuffer(obr, (totpart+totchild)*(path_nbr+1));
			strandbuf->ma= ma;
			strandbuf->lay= ob->lay;
			copy_m4_m4(strandbuf->winmat, re->winmat);
			strandbuf->winx= re->winx;
			strandbuf->winy= re->winy;
			strandbuf->maxdepth= 2;
			strandbuf->adaptcos= cos((float)part->adapt_angle*(float)(M_PI/180.0));
			strandbuf->overrideuv= sd.override_uv;
			strandbuf->minwidth= ma->strand_min;

			if(ma->strand_widthfade == 0.0f)
				strandbuf->widthfade= 0.0f;
			else if(ma->strand_widthfade >= 1.0f)
				strandbuf->widthfade= 2.0f - ma->strand_widthfade;
			else
				strandbuf->widthfade= 1.0f/MAX2(ma->strand_widthfade, 1e-5f);

			if(part->flag & PART_HAIR_BSPLINE)
				strandbuf->flag |= R_STRAND_BSPLINE;
			if(ma->mode & MA_STR_B_UNITS)
				strandbuf->flag |= R_STRAND_B_UNITS;

			svert= strandbuf->vert;

			if(re->r.mode & R_SPEED)
				dosurfacecache= 1;
			else if((re->wrld.mode & WO_AMB_OCC) && (re->wrld.ao_gather_method == WO_AOGATHER_APPROX))
				if(ma->amb != 0.0f)
					dosurfacecache= 1;

			totface= psmd->dm->getNumFaces(psmd->dm);
			origindex= psmd->dm->getFaceDataArray(psmd->dm, CD_ORIGINDEX);
			for(a=0; a<totface; a++)
				strandbuf->totbound= MAX2(strandbuf->totbound, (origindex)? origindex[a]: a);

			strandbuf->totbound++;
			strandbuf->bound= MEM_callocN(sizeof(StrandBound)*strandbuf->totbound, "StrandBound");
			sbound= strandbuf->bound;
			sbound->start= sbound->end= 0;
		}
	}

	if(sd.orco == 0) {
		sd.orco = MEM_mallocN(3 * sizeof(float), "particle orco");
		orco1 = 1;
	}

	if(path_nbr == 0)
		psys->lattice = psys_get_lattice(&sim);

/* 3. start creating renderable things */
	for(a=0,pa=pars; a<totpart+totchild; a++, pa++, seed++) {
		random = rng_getFloat(rng);
		/* setup per particle individual stuff */
		if(a<totpart){
			if(pa->flag & PARS_UNEXIST) continue;

			pa_time=(cfra-pa->time)/pa->lifetime;
			pa_birthtime = pa->time;
			pa_dietime = pa->dietime;
#if 0 // XXX old animation system
			if((part->flag&PART_ABS_TIME) == 0){
				if(ma->ipo) {
					/* correction for lifetime */
					calc_ipo(ma->ipo, 100.0f * pa_time);
					execute_ipo((ID *)ma, ma->ipo);
				}
				if(part->ipo){
					/* correction for lifetime */
					calc_ipo(part->ipo, 100.0f*pa_time);
					execute_ipo((ID *)part, part->ipo);
				}
			}
#endif // XXX old animation system

			hasize = ma->hasize;

			/* get orco */
			if(tpsys && (part->from==PART_FROM_PARTICLE || part->phystype==PART_PHYS_NO)){
				tpa=tpsys->particles+pa->num;
				psys_particle_on_emitter(psmd,tpart->from,tpa->num,pa->num_dmcache,tpa->fuv,tpa->foffset,co,nor,0,0,sd.orco,0);
			}
			else
				psys_particle_on_emitter(psmd,part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co,nor,0,0,sd.orco,0);

			/* get uvco & mcol */
			num= pa->num_dmcache;

			if(num == DMCACHE_NOTFOUND)
				if(pa->num < psmd->dm->getNumFaces(psmd->dm))
					num= pa->num;

			get_particle_uvco_mcol(part->from, psmd->dm, pa->fuv, num, &sd);

			pa_size = pa->size;

			BLI_srandom(psys->seed+a);

			r_tilt = 2.0f*(BLI_frand() - 0.5f);
			r_length = BLI_frand();

			if(path_nbr) {
				cache = psys->pathcache[a];
				max_k = (int)cache->steps;
			}

			if(totchild && (part->draw&PART_DRAW_PARENT)==0) continue;
		}
		else {
			ChildParticle *cpa= psys->child+a-totpart;

			if(path_nbr) {
				cache = psys->childcache[a-totpart];

				if(cache->steps < 0)
					continue;

				max_k = (int)cache->steps;
			}
			
			pa_time = psys_get_child_time(psys, cpa, cfra, &pa_birthtime, &pa_dietime);

#if 0 // XXX old animation system
			if((part->flag & PART_ABS_TIME) == 0) {
				if(ma->ipo){
					/* correction for lifetime */
					calc_ipo(ma->ipo, 100.0f * pa_time);
					execute_ipo((ID *)ma, ma->ipo);
				}
				if(part->ipo) {
					/* correction for lifetime */
					calc_ipo(part->ipo, 100.0f * pa_time);
					execute_ipo((ID *)part, part->ipo);
				}
			}
#endif // XXX old animation system

			pa_size = psys_get_child_size(psys, cpa, cfra, &pa_time);

			r_tilt = 2.0f*(PSYS_FRAND(a + 21) - 0.5f);
			r_length = PSYS_FRAND(a + 22);

			num = cpa->num;

			/* get orco */
			if(part->childtype == PART_CHILD_FACES) {
				psys_particle_on_emitter(psmd,
					PART_FROM_FACE, cpa->num,DMCACHE_ISCHILD,
					cpa->fuv,cpa->foffset,co,nor,0,0,sd.orco,0);
			}
			else {
				ParticleData *par = psys->particles + cpa->parent;
				psys_particle_on_emitter(psmd, part->from,
					par->num,DMCACHE_ISCHILD,par->fuv,
					par->foffset,co,nor,0,0,sd.orco,0);
			}

			/* get uvco & mcol */
			if(part->from!=PART_FROM_PARTICLE && part->childtype==PART_CHILD_FACES) {
				get_particle_uvco_mcol(PART_FROM_FACE, psmd->dm, cpa->fuv, cpa->num, &sd);
			}
			else {
				ParticleData *parent = psys->particles + cpa->parent;
				num = parent->num_dmcache;

				if(num == DMCACHE_NOTFOUND)
					if(parent->num < psmd->dm->getNumFaces(psmd->dm))
						num = parent->num;

				get_particle_uvco_mcol(part->from, psmd->dm, parent->fuv, num, &sd);
			}

			dosimplify = psys_render_simplify_params(psys, cpa, simplify);

			if(strandbuf) {
				int orignum= (origindex)? origindex[cpa->num]: cpa->num;

				if(orignum > sbound - strandbuf->bound) {
					sbound= strandbuf->bound + orignum;
					sbound->start= sbound->end= obr->totstrand;
				}
			}
		}

		/* surface normal shading setup */
		if(ma->mode_l & MA_STR_SURFDIFF) {
			mul_m3_v3(nmat, nor);
			sd.surfnor= nor;
		}
		else
			sd.surfnor= NULL;

		/* strand render setup */
		if(strandbuf) {
			strand= RE_findOrAddStrand(obr, obr->totstrand++);
			strand->buffer= strandbuf;
			strand->vert= svert;
			VECCOPY(strand->orco, sd.orco);

			if(dosimplify) {
				float *ssimplify= RE_strandren_get_simplify(obr, strand, 1);
				ssimplify[0]= simplify[0];
				ssimplify[1]= simplify[1];
			}

			if(sd.surfnor) {
				float *snor= RE_strandren_get_surfnor(obr, strand, 1);
				VECCOPY(snor, sd.surfnor);
			}

			if(dosurfacecache && num >= 0) {
				int *facenum= RE_strandren_get_face(obr, strand, 1);
				*facenum= num;
			}

			if(sd.uvco) {
				for(i=0; i<sd.totuv; i++) {
					if(i != sd.override_uv) {
						float *uv= RE_strandren_get_uv(obr, strand, i, NULL, 1);

						uv[0]= sd.uvco[2*i];
						uv[1]= sd.uvco[2*i+1];
					}
				}
			}
			if(sd.mcol) {
				for(i=0; i<sd.totcol; i++) {
					MCol *mc= RE_strandren_get_mcol(obr, strand, i, NULL, 1);
					*mc = sd.mcol[i];
				}
			}

			sbound->end++;
		}

		/* strandco computation setup */
		if(path_nbr) {
			strandlen= 0.0f;
			curlen= 0.0f;
			for(k=1; k<=path_nbr; k++)
				if(k<=max_k)
					strandlen += len_v3v3((cache+k-1)->co, (cache+k)->co);
		}

		if(path_nbr) {
			/* render strands */
			for(k=0; k<=path_nbr; k++){
				if(k<=max_k){
					VECCOPY(state.co,(cache+k)->co);
					VECCOPY(state.vel,(cache+k)->vel);
				}
				else
					continue;	

				if(k > 0)
					curlen += len_v3v3((cache+k-1)->co, (cache+k)->co);
				time= curlen/strandlen;

				VECCOPY(loc,state.co);
				mul_m4_v3(re->viewmat,loc);

				if(strandbuf) {
					VECCOPY(svert->co, loc);
					svert->strandco= -1.0f + 2.0f*time;
					svert++;
					strand->totvert++;
				}
				else{
					sd.size = hasize;

					if(k==1){
						sd.first = 1;
						sd.time = 0.0f;
						VECSUB(loc0,loc1,loc);
						VECADD(loc0,loc1,loc0);

						particle_curve(re, obr, psmd->dm, ma, &sd, loc1, loc0, seed);
					}

					sd.first = 0;
					sd.time = time;

					if(k)
						particle_curve(re, obr, psmd->dm, ma, &sd, loc, loc1, seed);

					VECCOPY(loc1,loc);
				}
			}

		}
		else {
			/* render normal particles */
			if(part->trail_count > 1) {
				float length = part->path_end * (1.0 - part->randlength * r_length);
				int trail_count = part->trail_count * (1.0 - part->randlength * r_length);
				float ct = (part->draw & PART_ABS_PATH_TIME) ? cfra : pa_time;
				float dt = length / (trail_count ? (float)trail_count : 1.0f);

				for(i=0; i < trail_count; i++, ct -= dt) {
					if(part->draw & PART_ABS_PATH_TIME) {
						if(ct < pa_birthtime || ct > pa_dietime)
							continue;
					}
					else if(ct < 0.0f || ct > 1.0f)
						continue;

					state.time = (part->draw & PART_ABS_PATH_TIME) ? -ct : ct;
					psys_get_particle_on_path(&sim,a,&state,1);

					if(psys->parent)
						mul_m4_v3(psys->parent->obmat, state.co);

					if(part->ren_as == PART_DRAW_BB) {
						bb.random = random;
						bb.size = pa_size;
						bb.tilt = part->bb_tilt * (1.0f - part->bb_rand_tilt * r_tilt);
						bb.time = ct;
						bb.num = a;
					}

					particle_normal_ren(part->ren_as, part, re, obr, psmd->dm, ma, &sd, &bb, &state, seed, hasize);
				}
			}
			else {
				time=0.0f;
				state.time=cfra;
				if(psys_get_particle_state(&sim,a,&state,0)==0)
					continue;

				if(psys->parent)
					mul_m4_v3(psys->parent->obmat, state.co);

				if(part->ren_as == PART_DRAW_BB) {
					bb.random = random;
					bb.size = pa_size;
					bb.tilt = part->bb_tilt * (1.0f - part->bb_rand_tilt * r_tilt);
					bb.time = pa_time;
					bb.num = a;
				}

				particle_normal_ren(part->ren_as, part, re, obr, psmd->dm, ma, &sd, &bb, &state, seed, hasize);
			}
		}

		if(orco1==0)
			sd.orco+=3;

		if(re->test_break(re->tbh))
			break;
	}

	if(dosurfacecache)
		strandbuf->surface= cache_strand_surface(re, obr, psmd->dm, mat, timeoffset);

/* 4. clean up */
#if 0 // XXX old animation system
	if(ma) do_mat_ipo(re->scene, ma);
#endif // XXX old animation system
	
	if(orco1)
		MEM_freeN(sd.orco);

	if(sd.uvco)
		MEM_freeN(sd.uvco);
	
	if(sd.mcol)
		MEM_freeN(sd.mcol);

	if(uv_name)
		MEM_freeN(uv_name);

	if(states)
		MEM_freeN(states);
	
	rng_free(rng);

	psys->flag &= ~PSYS_DRAWING;

	if(psys->lattice){
		end_latt_deform(psys->lattice);
		psys->lattice= NULL;
	}

	if(path_nbr && (ma->mode_l & MA_TANGENT_STR)==0)
		calc_vertexnormals(re, obr, 0, 0);

	return 1;
}

/* ------------------------------------------------------------------------- */
/* Halo's   																 */
/* ------------------------------------------------------------------------- */

static void make_render_halos(Render *re, ObjectRen *obr, Mesh *me, int totvert, MVert *mvert, Material *ma, float *orco)
{
	Object *ob= obr->ob;
	HaloRen *har;
	float xn, yn, zn, nor[3], view[3];
	float vec[3], hasize, mat[4][4], imat[3][3];
	int a, ok, seed= ma->seed1;

	mul_m4_m4m4(mat, ob->obmat, re->viewmat);
	copy_m3_m4(imat, ob->imat);

	re->flag |= R_HALO;

	for(a=0; a<totvert; a++, mvert++) {
		ok= 1;

		if(ok) {
			hasize= ma->hasize;

			VECCOPY(vec, mvert->co);
			mul_m4_v3(mat, vec);

			if(ma->mode & MA_HALOPUNO) {
				xn= mvert->no[0];
				yn= mvert->no[1];
				zn= mvert->no[2];

				/* transpose ! */
				nor[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
				nor[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
				nor[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
				normalize_v3(nor);

				VECCOPY(view, vec);
				normalize_v3(view);

				zn= nor[0]*view[0]+nor[1]*view[1]+nor[2]*view[2];
				if(zn>=0.0) hasize= 0.0;
				else hasize*= zn*zn*zn*zn;
			}

			if(orco) har= RE_inithalo(re, obr, ma, vec, NULL, orco, hasize, 0.0, seed);
			else har= RE_inithalo(re, obr, ma, vec, NULL, mvert->co, hasize, 0.0, seed);
			if(har) har->lay= ob->lay;
		}
		if(orco) orco+= 3;
		seed++;
	}
}

static int verghalo(const void *a1, const void *a2)
{
	const HaloRen *har1= *(const HaloRen**)a1;
	const HaloRen *har2= *(const HaloRen**)a2;
	
	if(har1->zs < har2->zs) return 1;
	else if(har1->zs > har2->zs) return -1;
	return 0;
}

static void sort_halos(Render *re, int totsort)
{
	ObjectRen *obr;
	HaloRen *har= NULL, **haso;
	int a;

	if(re->tothalo==0) return;

	re->sortedhalos= MEM_callocN(sizeof(HaloRen*)*re->tothalo, "sorthalos");
	haso= re->sortedhalos;

	for(obr=re->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->tothalo; a++) {
			if((a & 255)==0) har= obr->bloha[a>>8];
			else har++;

			*(haso++)= har;
		}
	}

	qsort(re->sortedhalos, totsort, sizeof(HaloRen*), verghalo);
}

/* ------------------------------------------------------------------------- */
/* Displacement Mapping														 */
/* ------------------------------------------------------------------------- */

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

static void displace_render_vert(Render *re, ObjectRen *obr, ShadeInput *shi, VertRen *vr, int vindex, float *scale, float mat[][4], float imat[][3])
{
	MTFace *tface;
	short texco= shi->mat->texco;
	float sample=0, displace[3];
	char *name;
	int i;

	/* shi->co is current render coord, just make sure at least some vector is here */
	VECCOPY(shi->co, vr->co);
	/* vertex normal is used for textures type 'col' and 'var' */
	VECCOPY(shi->vn, vr->n);

	if(mat)
		mul_m4_v3(mat, shi->co);

	if(imat) {
		shi->vn[0]= imat[0][0]*vr->n[0]+imat[0][1]*vr->n[1]+imat[0][2]*vr->n[2];
		shi->vn[1]= imat[1][0]*vr->n[0]+imat[1][1]*vr->n[1]+imat[1][2]*vr->n[2];
		shi->vn[2]= imat[2][0]*vr->n[0]+imat[2][1]*vr->n[1]+imat[2][2]*vr->n[2];
	}

	if (texco & TEXCO_UV) {
		shi->totuv= 0;
		shi->actuv= obr->actmtface;

		for (i=0; (tface=RE_vlakren_get_tface(obr, shi->vlr, i, &name, 0)); i++) {
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
		float *sticky= RE_vertren_get_sticky(obr, vr, 0);
		if(sticky) {
			shi->sticky[0]= sticky[0];
			shi->sticky[1]= sticky[1];
			shi->sticky[2]= 0.0f;
		}
	}
	if (texco & TEXCO_GLOB) {
		VECCOPY(shi->gl, shi->co);
		mul_m4_v3(re->viewinv, shi->gl);
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

	displace[0]= shi->displace[0] * scale[0];
	displace[1]= shi->displace[1] * scale[1];
	displace[2]= shi->displace[2] * scale[2];
	
	if(mat)
		mul_m3_v3(imat, displace);

	/* 0.5 could become button once?  */
	vr->co[0] += displace[0]; 
	vr->co[1] += displace[1];
	vr->co[2] += displace[2];
	
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

static void displace_render_face(Render *re, ObjectRen *obr, VlakRen *vlr, float *scale, float mat[][4], float imat[][3])
{
	ShadeInput shi;

	/* Warning, This is not that nice, and possibly a bit slow,
	however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
	memset(&shi, 0, sizeof(ShadeInput)); 
	/* end warning! - Campbell */
	
	/* set up shadeinput struct for multitex() */
	
	/* memset above means we dont need this */
	/*shi.osatex= 0;*/		/* signal not to use dx[] and dy[] texture AA vectors */

	shi.obr= obr;
	shi.vlr= vlr;		/* current render face */
	shi.mat= vlr->mat;		/* current input material */
	shi.thread= 0;
	
	/* TODO, assign these, displacement with new bumpmap is skipped without - campbell */
#if 0
	/* order is not known ? */
	shi.v1= vlr->v1;
	shi.v2= vlr->v2;
	shi.v3= vlr->v3;
#endif

	/* Displace the verts, flag is set when done */
	if (!vlr->v1->flag)
		displace_render_vert(re, obr, &shi, vlr->v1,0,  scale, mat, imat);
	
	if (!vlr->v2->flag)
		displace_render_vert(re, obr, &shi, vlr->v2, 1, scale, mat, imat);

	if (!vlr->v3->flag)
		displace_render_vert(re, obr, &shi, vlr->v3, 2, scale, mat, imat);

	if (vlr->v4) {
		if (!vlr->v4->flag)
			displace_render_vert(re, obr, &shi, vlr->v4, 3, scale, mat, imat);

		/*	closest in displace value.  This will help smooth edges.   */ 
		if ( fabs(vlr->v1->accum - vlr->v3->accum) > fabs(vlr->v2->accum - vlr->v4->accum)) 
			vlr->flag |= R_DIVIDE_24;
		else vlr->flag &= ~R_DIVIDE_24;
	}
	
	/* Recalculate the face normal  - if flipped before, flip now */
	if(vlr->v4) {
		normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
	}	
	else {
		normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);
	}
}

static void do_displacement(Render *re, ObjectRen *obr, float mat[][4], float imat[][3])
{
	VertRen *vr;
	VlakRen *vlr;
//	float min[3]={1e30, 1e30, 1e30}, max[3]={-1e30, -1e30, -1e30};
	float scale[3]={1.0f, 1.0f, 1.0f}, temp[3];//, xn
	int i; //, texflag=0;
	Object *obt;
		
	/* Object Size with parenting */
	obt=obr->ob;
	while(obt){
		add_v3_v3v3(temp, obt->size, obt->dsize);
		scale[0]*=temp[0]; scale[1]*=temp[1]; scale[2]*=temp[2];
		obt=obt->parent;
	}
	
	/* Clear all flags */
	for(i=0; i<obr->totvert; i++){ 
		vr= RE_findOrAddVert(obr, i);
		vr->flag= 0;
	}

	for(i=0; i<obr->totvlak; i++){
		vlr=RE_findOrAddVlak(obr, i);
		displace_render_face(re, obr, vlr, scale, mat, imat);
	}
	
	/* Recalc vertex normals */
	calc_vertexnormals(re, obr, 0, 0);
}

/* ------------------------------------------------------------------------- */
/* Metaball   																 */
/* ------------------------------------------------------------------------- */

static void init_render_mball(Render *re, ObjectRen *obr)
{
	Object *ob= obr->ob;
	DispList *dl;
	VertRen *ver;
	VlakRen *vlr, *vlr1;
	Material *ma;
	float *data, *nors, *orco, mat[4][4], imat[3][3], xn, yn, zn;
	int a, need_orco, vlakindex, *index;

	if (ob!=find_basis_mball(re->scene, ob))
		return;

	mul_m4_m4m4(mat, ob->obmat, re->viewmat);
	invert_m4_m4(ob->imat, mat);
	copy_m3_m4(imat, ob->imat);

	ma= give_render_material(re, ob, 1);

	need_orco= 0;
	if(ma->texco & TEXCO_ORCO) {
		need_orco= 1;
	}
	
	makeDispListMBall(re->scene, ob);
	dl= ob->disp.first;
	if(dl==0) return;

	data= dl->verts;
	nors= dl->nors;
	orco= get_object_orco(re, ob);

	for(a=0; a<dl->nr; a++, data+=3, nors+=3, orco+=3) {

		ver= RE_findOrAddVert(obr, obr->totvert++);
		VECCOPY(ver->co, data);
		mul_m4_v3(mat, ver->co);

		/* render normals are inverted */
		xn= -nors[0];
		yn= -nors[1];
		zn= -nors[2];

		/* transpose ! */
		ver->n[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
		ver->n[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
		ver->n[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
		normalize_v3(ver->n);
		//if(ob->transflag & OB_NEG_SCALE) mul_v3_fl(ver->n. -1.0);
		
		if(need_orco) ver->orco= orco;
	}

	index= dl->index;
	for(a=0; a<dl->parts; a++, index+=4) {

		vlr= RE_findOrAddVlak(obr, obr->totvlak++);
		vlr->v1= RE_findOrAddVert(obr, index[0]);
		vlr->v2= RE_findOrAddVert(obr, index[1]);
		vlr->v3= RE_findOrAddVert(obr, index[2]);
		vlr->v4= 0;

		if(ob->transflag & OB_NEG_SCALE) 
			normal_tri_v3( vlr->n,vlr->v1->co, vlr->v2->co, vlr->v3->co);
		else
			normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);

		vlr->mat= ma;
		vlr->flag= ME_SMOOTH+R_NOPUNOFLIP;
		vlr->ec= 0;

		/* mball -too bad- always has triangles, because quads can be non-planar */
		if(index[3] && index[3]!=index[2]) {
			vlr1= RE_findOrAddVlak(obr, obr->totvlak++);
			vlakindex= vlr1->index;
			*vlr1= *vlr;
			vlr1->index= vlakindex;
			vlr1->v2= vlr1->v3;
			vlr1->v3= RE_findOrAddVert(obr, index[3]);
			if(ob->transflag & OB_NEG_SCALE) 
				normal_tri_v3( vlr1->n,vlr1->v1->co, vlr1->v2->co, vlr1->v3->co);
			else
				normal_tri_v3( vlr1->n,vlr1->v3->co, vlr1->v2->co, vlr1->v1->co);
		}
	}

	/* enforce display lists remade */
	freedisplist(&ob->disp);
	
	/* this enforces remake for real, orco displist is small (in scale) */
	ob->recalc |= OB_RECALC_DATA;
}

/* ------------------------------------------------------------------------- */
/* Surfaces and Curves														 */
/* ------------------------------------------------------------------------- */

/* returns amount of vertices added for orco */
static int dl_surf_to_renderdata(ObjectRen *obr, DispList *dl, Material **matar, float *orco, float mat[4][4])
{
	Object *ob= obr->ob;
	VertRen *v1, *v2, *v3, *v4, *ver;
	VlakRen *vlr, *vlr1, *vlr2, *vlr3;
	Curve *cu= ob->data;
	float *data, n1[3];
	int u, v, orcoret= 0;
	int p1, p2, p3, p4, a;
	int sizeu, nsizeu, sizev, nsizev;
	int startvert, startvlak;
	
	startvert= obr->totvert;
	nsizeu = sizeu = dl->parts; nsizev = sizev = dl->nr; 
	
	data= dl->verts;
	for (u = 0; u < sizeu; u++) {
		v1 = RE_findOrAddVert(obr, obr->totvert++); /* save this for possible V wrapping */
		VECCOPY(v1->co, data); data += 3;
		if(orco) {
			v1->orco= orco; orco+= 3; orcoret++;
		}	
		mul_m4_v3(mat, v1->co);
		
		for (v = 1; v < sizev; v++) {
			ver= RE_findOrAddVert(obr, obr->totvert++);
			VECCOPY(ver->co, data); data += 3;
			if(orco) {
				ver->orco= orco; orco+= 3; orcoret++;
			}	
			mul_m4_v3(mat, ver->co);
		}
		/* if V-cyclic, add extra vertices at end of the row */
		if (dl->flag & DL_CYCL_U) {
			ver= RE_findOrAddVert(obr, obr->totvert++);
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
			v1= RE_findOrAddVert(obr, startvert + v);
			ver= RE_findOrAddVert(obr, obr->totvert++);
			VECCOPY(ver->co, v1->co);
			if(orco) {
				ver->orco= orco; orco+=3; orcoret++; //ver->orco= orcobase + 3*(0*sizev + v);
			}
		}
	}
	
	sizeu = nsizeu;
	sizev = nsizev;
	
	startvlak= obr->totvlak;
	
	for(u = 0; u < sizeu - 1; u++) {
		p1 = startvert + u * sizev; /* walk through face list */
		p2 = p1 + 1;
		p3 = p2 + sizev;
		p4 = p3 - 1;
		
		for(v = 0; v < sizev - 1; v++) {
			v1= RE_findOrAddVert(obr, p1);
			v2= RE_findOrAddVert(obr, p2);
			v3= RE_findOrAddVert(obr, p3);
			v4= RE_findOrAddVert(obr, p4);
			
			vlr= RE_findOrAddVlak(obr, obr->totvlak++);
			vlr->v1= v1; vlr->v2= v2; vlr->v3= v3; vlr->v4= v4;
			
			normal_quad_v3( n1,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
			
			VECCOPY(vlr->n, n1);
			
			vlr->mat= matar[ dl->col];
			vlr->ec= ME_V1V2+ME_V2V3;
			vlr->flag= dl->rt;
			if( (cu->flag & CU_NOPUNOFLIP) ) {
				vlr->flag |= R_NOPUNOFLIP;
			}
			
			add_v3_v3v3(v1->n, v1->n, n1);
			add_v3_v3v3(v2->n, v2->n, n1);
			add_v3_v3v3(v3->n, v3->n, n1);
			add_v3_v3v3(v4->n, v4->n, n1);
			
			p1++; p2++; p3++; p4++;
		}
	}	
	/* fix normals for U resp. V cyclic faces */
	sizeu--; sizev--;  /* dec size for face array */
	if (dl->flag & DL_CYCL_V) {
		
		for (v = 0; v < sizev; v++)
		{
			/* optimize! :*/
			vlr= RE_findOrAddVlak(obr, UVTOINDEX(sizeu - 1, v));
			vlr1= RE_findOrAddVlak(obr, UVTOINDEX(0, v));
			add_v3_v3v3(vlr1->v1->n, vlr1->v1->n, vlr->n);
			add_v3_v3v3(vlr1->v2->n, vlr1->v2->n, vlr->n);
			add_v3_v3v3(vlr->v3->n, vlr->v3->n, vlr1->n);
			add_v3_v3v3(vlr->v4->n, vlr->v4->n, vlr1->n);
		}
	}
	if (dl->flag & DL_CYCL_U) {
		
		for (u = 0; u < sizeu; u++)
		{
			/* optimize! :*/
			vlr= RE_findOrAddVlak(obr, UVTOINDEX(u, 0));
			vlr1= RE_findOrAddVlak(obr, UVTOINDEX(u, sizev-1));
			add_v3_v3v3(vlr1->v2->n, vlr1->v2->n, vlr->n);
			add_v3_v3v3(vlr1->v3->n, vlr1->v3->n, vlr->n);
			add_v3_v3v3(vlr->v1->n, vlr->v1->n, vlr1->n);
			add_v3_v3v3(vlr->v4->n, vlr->v4->n, vlr1->n);
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
		vlr= RE_findOrAddVlak(obr, UVTOINDEX(sizeu - 1, sizev - 1)); /* (m,n) */
		vlr1= RE_findOrAddVlak(obr, UVTOINDEX(0,0));  /* (0,0) */
		add_v3_v3v3(n1, vlr->n, vlr1->n);
		vlr2= RE_findOrAddVlak(obr, UVTOINDEX(0, sizev-1)); /* (0,n) */
		add_v3_v3v3(n1, n1, vlr2->n);
		vlr3= RE_findOrAddVlak(obr, UVTOINDEX(sizeu-1, 0)); /* (m,0) */
		add_v3_v3v3(n1, n1, vlr3->n);
		VECCOPY(vlr->v3->n, n1);
		VECCOPY(vlr1->v1->n, n1);
		VECCOPY(vlr2->v2->n, n1);
		VECCOPY(vlr3->v4->n, n1);
	}
	for(a = startvert; a < obr->totvert; a++) {
		ver= RE_findOrAddVert(obr, a);
		normalize_v3(ver->n);
	}
	
	
	return orcoret;
}

static void init_render_surf(Render *re, ObjectRen *obr)
{
	Object *ob= obr->ob;
	Nurb *nu=0;
	Curve *cu;
	ListBase displist;
	DispList *dl;
	Material **matar;
	float *orco=NULL, *orcobase=NULL, mat[4][4];
	int a, totmat, need_orco=0;

	cu= ob->data;
	nu= cu->nurb.first;
	if(nu==0) return;

	mul_m4_m4m4(mat, ob->obmat, re->viewmat);
	invert_m4_m4(ob->imat, mat);

	/* material array */
	totmat= ob->totcol+1;
	matar= MEM_callocN(sizeof(Material*)*totmat, "init_render_surf matar");

	for(a=0; a<totmat; a++) {
		matar[a]= give_render_material(re, ob, a+1);

		if(matar[a] && matar[a]->texco & TEXCO_ORCO)
			need_orco= 1;
	}

	if(ob->parent && (ob->parent->type==OB_LATTICE)) need_orco= 1;

	if(need_orco) orcobase= orco= get_object_orco(re, ob);

	displist.first= displist.last= 0;
	makeDispListSurf(re->scene, ob, &displist, 1, 0);

	/* walk along displaylist and create rendervertices/-faces */
	for(dl=displist.first; dl; dl=dl->next) {
		/* watch out: u ^= y, v ^= x !! */
		if(dl->type==DL_SURF)
			orco+= 3*dl_surf_to_renderdata(obr, dl, matar, orco, mat);
	}

	freedisplist(&displist);
	MEM_freeN(matar);
}

static void init_render_curve(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	Curve *cu;
	VertRen *ver;
	VlakRen *vlr;
	DispList *dl;
	ListBase olddl={NULL, NULL};
	Material **matar;
	float len, *data, *fp, *orco=NULL, *orcobase= NULL;
	float n[3], mat[4][4];
	int nr, startvert, startvlak, a, b;
	int frontside, need_orco=0, totmat;

	cu= ob->data;
	if(ob->type==OB_FONT && cu->str==NULL) return;
	else if(ob->type==OB_CURVE && cu->nurb.first==NULL) return;

	/* no modifier call here, is in makedisp */

	if(cu->resolu_ren) 
		SWAP(ListBase, olddl, cu->disp);
	
	/* test displist */
	if(cu->disp.first==NULL) 
		makeDispListCurveTypes(re->scene, ob, 0);
	dl= cu->disp.first;
	if(cu->disp.first==NULL) return;
	
	mul_m4_m4m4(mat, ob->obmat, re->viewmat);
	invert_m4_m4(ob->imat, mat);

	/* material array */
	totmat= ob->totcol+1;
	matar= MEM_callocN(sizeof(Material*)*totmat, "init_render_surf matar");

	for(a=0; a<totmat; a++) {
		matar[a]= give_render_material(re, ob, a+1);

		if(matar[a] && matar[a]->texco & TEXCO_ORCO)
			need_orco= 1;
	}

	if(need_orco) orcobase=orco= get_object_orco(re, ob);

	dl= cu->disp.first;
	while(dl) {
		if(dl->type==DL_INDEX3) {
			int *index;

			startvert= obr->totvert;
			data= dl->verts;

			n[0]= ob->imat[0][2];
			n[1]= ob->imat[1][2];
			n[2]= ob->imat[2][2];
			normalize_v3(n);

			for(a=0; a<dl->nr; a++, data+=3) {
				ver= RE_findOrAddVert(obr, obr->totvert++);
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

				mul_m4_v3(mat, ver->co);
				
				if (orco) {
					ver->orco = orco;
					orco += 3;
				}
			}
			
			if(timeoffset==0) {
				startvlak= obr->totvlak;
				index= dl->index;
				for(a=0; a<dl->parts; a++, index+=3) {

					vlr= RE_findOrAddVlak(obr, obr->totvlak++);
					vlr->v1= RE_findOrAddVert(obr, startvert+index[0]);
					vlr->v2= RE_findOrAddVert(obr, startvert+index[1]);
					vlr->v3= RE_findOrAddVert(obr, startvert+index[2]);
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
				}
			}
		}
		else if (dl->type==DL_SURF) {
			
			/* cyclic U means an extruded full circular curve, we skip bevel splitting then */
			if (dl->flag & DL_CYCL_U) {
				orco+= 3*dl_surf_to_renderdata(obr, dl, matar, orco, mat);
			}
			else {
				int p1,p2,p3,p4;

				fp= dl->verts;
				startvert= obr->totvert;
				nr= dl->nr*dl->parts;

				while(nr--) {
					ver= RE_findOrAddVert(obr, obr->totvert++);
						
					VECCOPY(ver->co, fp);
					mul_m4_v3(mat, ver->co);
					fp+= 3;

					if (orco) {
						ver->orco = orco;
						orco += 3;
					}
				}

				if(dl->bevelSplitFlag || timeoffset==0) {
					startvlak= obr->totvlak;

					for(a=0; a<dl->parts; a++) {

						frontside= (a >= dl->nr/2);
						
						if (surfindex_displist(dl, a, &b, &p1, &p2, &p3, &p4)==0)
							break;
						
						p1+= startvert;
						p2+= startvert;
						p3+= startvert;
						p4+= startvert;

						for(; b<dl->nr; b++) {
							vlr= RE_findOrAddVlak(obr, obr->totvlak++);
							vlr->v1= RE_findOrAddVert(obr, p2);
							vlr->v2= RE_findOrAddVert(obr, p1);
							vlr->v3= RE_findOrAddVert(obr, p3);
							vlr->v4= RE_findOrAddVert(obr, p4);
							vlr->ec= ME_V2V3+ME_V3V4;
							if(a==0) vlr->ec+= ME_V1V2;

							vlr->flag= dl->rt;

							/* this is not really scientific: the vertices
								* 2, 3 en 4 seem to give better vertexnormals than 1 2 3:
								* front and backside treated different!!
								*/

							if(frontside)
								normal_tri_v3( vlr->n,vlr->v2->co, vlr->v3->co, vlr->v4->co);
							else 
								normal_tri_v3( vlr->n,vlr->v1->co, vlr->v2->co, vlr->v3->co);

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
								split_v_renderfaces(obr, startvlak, startvert, dl->parts, dl->nr, a, dl->flag&DL_CYCL_V, dl->flag&DL_CYCL_U);
					}

					/* vertex normals */
					for(a= startvlak; a<obr->totvlak; a++) {
						vlr= RE_findOrAddVlak(obr, a);

						add_v3_v3v3(vlr->v1->n, vlr->v1->n, vlr->n);
						add_v3_v3v3(vlr->v3->n, vlr->v3->n, vlr->n);
						add_v3_v3v3(vlr->v2->n, vlr->v2->n, vlr->n);
						add_v3_v3v3(vlr->v4->n, vlr->v4->n, vlr->n);
					}
					for(a=startvert; a<obr->totvert; a++) {
						ver= RE_findOrAddVert(obr, a);
						len= normalize_v3(ver->n);
						if(len==0.0) ver->flag= 1;	/* flag abuse, its only used in zbuf now  */
						else ver->flag= 0;
					}
					for(a= startvlak; a<obr->totvlak; a++) {
						vlr= RE_findOrAddVlak(obr, a);
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

	MEM_freeN(matar);
}

/* ------------------------------------------------------------------------- */
/* Mesh     																 */
/* ------------------------------------------------------------------------- */

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

static void use_mesh_edge_lookup(ObjectRen *obr, DerivedMesh *dm, MEdge *medge, VlakRen *vlr, struct edgesort *edgetable, int totedge)
{
	struct edgesort ed, *edp;
	CustomDataLayer *layer;
	MTFace *mtface, *mtf;
	MCol *mcol, *mc;
	int index, mtfn, mcn;
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
				mtf= RE_vlakren_get_tface(obr, vlr, mtfn++, &name, 1);

				*mtf= *mtface;

				memcpy(mtf->uv[0], mtface->uv[edp->i1], sizeof(float)*2);
				memcpy(mtf->uv[1], mtface->uv[edp->i2], sizeof(float)*2);
				memcpy(mtf->uv[2], mtface->uv[1], sizeof(float)*2);
				memcpy(mtf->uv[3], mtface->uv[1], sizeof(float)*2);
			}
			else if(layer->type == CD_MCOL && mcn < MAX_MCOL) {
				mcol= &((MCol*)layer->data)[edp->f*4];
				mc= RE_vlakren_get_mcol(obr, vlr, mcn++, &name, 1);

				mc[0]= mcol[edp->i1];
				mc[1]= mc[2]= mc[3]= mcol[edp->i2];
			}
		}
	}
}

static void free_camera_inside_volumes(Render *re)
{
	BLI_freelistN(&re->render_volumes_inside);
}

static void init_camera_inside_volumes(Render *re)
{
	ObjectInstanceRen *obi;
	VolumeOb *vo;
	float co[3] = {0.f, 0.f, 0.f};

	for(vo= re->volumes.first; vo; vo= vo->next) {
		for(obi= re->instancetable.first; obi; obi= obi->next) {
			if (obi->obr == vo->obr) {
				if (point_inside_volume_objectinstance(re, obi, co)) {
					MatInside *mi;
					
					mi = MEM_mallocN(sizeof(MatInside), "camera inside material");
					mi->ma = vo->ma;
					mi->obi = obi;
					
					BLI_addtail(&(re->render_volumes_inside), mi);
				}
			}
		}
	}
	
	/* debug {
	MatInside *m;
	for (m=re->render_volumes_inside.first; m; m=m->next) {
		printf("matinside: ma: %s \n", m->ma->id.name+2);
	}
	}*/
}

static void add_volume(Render *re, ObjectRen *obr, Material *ma)
{
	struct VolumeOb *vo;
	
	vo = MEM_mallocN(sizeof(VolumeOb), "volume object");
	
	vo->ma = ma;
	vo->obr = obr;
	
	BLI_addtail(&re->volumes, vo);
}

static void init_render_mesh(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	Mesh *me;
	MVert *mvert = NULL;
	MFace *mface;
	VlakRen *vlr; //, *vlr1;
	VertRen *ver;
	Material *ma;
	MSticky *ms = NULL;
	DerivedMesh *dm;
	CustomDataMask mask;
	float xn, yn, zn,  imat[3][3], mat[4][4];  //nor[3],
	float *orco=0;
	int need_orco=0, need_stress=0, need_nmap_tangent=0, need_tangent=0;
	int a, a1, ok, vertofs;
	int end, do_autosmooth=0, totvert = 0;
	int use_original_normals= 0;

	me= ob->data;

	mul_m4_m4m4(mat, ob->obmat, re->viewmat);
	invert_m4_m4(ob->imat, mat);
	copy_m3_m4(imat, ob->imat);

	if(me->totvert==0)
		return;
	
	need_orco= 0;
	for(a=1; a<=ob->totcol; a++) {
		ma= give_render_material(re, ob, a);
		if(ma) {
			if(ma->texco & (TEXCO_ORCO|TEXCO_STRESS))
				need_orco= 1;
			if(ma->texco & TEXCO_STRESS)
				need_stress= 1;
			/* normalmaps, test if tangents needed, separated from shading */
			if(ma->mode_l & MA_TANGENT_V) {
				need_tangent= 1;
				if(me->mtface==NULL)
					need_orco= 1;
			}
			if(ma->mode_l & MA_NORMAP_TANG) {
				if(me->mtface==NULL) {
					need_orco= 1;
					need_tangent= 1;
				}
				need_nmap_tangent= 1;
			}
		}
	}

	if(re->flag & R_NEED_TANGENT) {
		/* exception for tangent space baking */
		if(me->mtface==NULL) {
			need_orco= 1;
			need_tangent= 1;
		}
		need_nmap_tangent= 1;
	}
	
	/* check autosmooth and displacement, we then have to skip only-verts optimize */
	do_autosmooth |= (me->flag & ME_AUTOSMOOTH);
	if(do_autosmooth)
		timeoffset= 0;
	if(test_for_displace(re, ob ) )
		timeoffset= 0;
	
	mask= CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL;
	if(!timeoffset)
		if(need_orco)
			mask |= CD_MASK_ORCO;

	dm= mesh_create_derived_render(re->scene, ob, mask);
	if(dm==NULL) return;	/* in case duplicated object fails? */

	if(mask & CD_MASK_ORCO) {
		orco= dm->getVertDataArray(dm, CD_ORCO);
		if(orco) {
			orco= MEM_dupallocN(orco);
			set_object_orco(re, ob, orco);
		}
	}

	mvert= dm->getVertArray(dm);
	totvert= dm->getNumVerts(dm);

	/* attempt to autsmooth on original mesh, only without subsurf */
	if(do_autosmooth && me->totvert==totvert && me->totface==dm->getNumFaces(dm))
		use_original_normals= 1;
	
	ms = (totvert==me->totvert)?me->msticky:NULL;
	
	ma= give_render_material(re, ob, 1);

	if(ma->material_type == MA_TYPE_HALO) {
		make_render_halos(re, obr, me, totvert, mvert, ma, orco);
	}
	else {

		for(a=0; a<totvert; a++, mvert++) {
			ver= RE_findOrAddVert(obr, obr->totvert++);
			VECCOPY(ver->co, mvert->co);
			if(do_autosmooth==0)	/* autosmooth on original unrotated data to prevent differences between frames */
				mul_m4_v3(mat, ver->co);
  
			if(orco) {
				ver->orco= orco;
				orco+=3;
			}
			if(ms) {
				float *sticky= RE_vertren_get_sticky(obr, ver, 1);
				sticky[0]= ms->co[0];
				sticky[1]= ms->co[1];
				ms++;
			}
		}
		
		if(!timeoffset) {
			/* store customdata names, because DerivedMesh is freed */
			RE_set_customdata_names(obr, &dm->faceData);
			
			/* still to do for keys: the correct local texture coordinate */

			/* faces in order of color blocks */
			vertofs= obr->totvert - totvert;
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
				if(ma->material_type == MA_TYPE_WIRE) {
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

							vlr= RE_findOrAddVlak(obr, obr->totvlak++);
							vlr->v1= RE_findOrAddVert(obr, vertofs+v1);
							vlr->v2= RE_findOrAddVert(obr, vertofs+v2);
							vlr->v3= RE_findOrAddVert(obr, vertofs+v3);
							if(v4) vlr->v4= RE_findOrAddVert(obr, vertofs+v4);
							else vlr->v4= 0;

							/* render normals are inverted in render */
							if(use_original_normals) {
								MFace *mf= me->mface+a;
								MVert *mv= me->mvert;
								
								if(vlr->v4) 
									len= normal_quad_v3( vlr->n, mv[mf->v4].co, mv[mf->v3].co, mv[mf->v2].co, mv[mf->v1].co);
								else 
									len= normal_tri_v3( vlr->n,mv[mf->v3].co, mv[mf->v2].co, mv[mf->v1].co);
							}
							else {
								if(vlr->v4) 
									len= normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
								else 
									len= normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);
							}

							vlr->mat= ma;
							vlr->flag= flag;
							if((me->flag & ME_NOPUNOFLIP) ) {
								vlr->flag |= R_NOPUNOFLIP;
							}
							vlr->ec= 0; /* mesh edges rendered separately */

							if(len==0) obr->totvlak--;
							else {
								CustomDataLayer *layer;
								MTFace *mtface, *mtf;
								MCol *mcol, *mc;
								int index, mtfn= 0, mcn= 0;
								char *name;

								for(index=0; index<dm->faceData.totlayer; index++) {
									layer= &dm->faceData.layers[index];
									name= layer->name;
									
									if(layer->type == CD_MTFACE && mtfn < MAX_MTFACE) {
										mtf= RE_vlakren_get_tface(obr, vlr, mtfn++, &name, 1);
										mtface= (MTFace*)layer->data;
										*mtf= mtface[a];
									}
									else if(layer->type == CD_MCOL && mcn < MAX_MCOL) {
										mc= RE_vlakren_get_mcol(obr, vlr, mcn++, &name, 1);
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
			if(end && (ma->material_type == MA_TYPE_WIRE)) {
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

						vlr= RE_findOrAddVlak(obr, obr->totvlak++);
						vlr->v1= RE_findOrAddVert(obr, vertofs+medge->v1);
						vlr->v2= RE_findOrAddVert(obr, vertofs+medge->v2);
						vlr->v3= vlr->v2;
						vlr->v4= NULL;
						
						if(edgetable)
							use_mesh_edge_lookup(obr, dm, medge, vlr, edgetable, totedge);
						
						xn= -(v0->no[0]+v1->no[0]);
						yn= -(v0->no[1]+v1->no[1]);
						zn= -(v0->no[2]+v1->no[2]);
						/* transpose ! */
						vlr->n[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
						vlr->n[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
						vlr->n[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
						normalize_v3(vlr->n);
						
						vlr->mat= ma;
						vlr->flag= 0;
						vlr->ec= ME_V1V2;
					}
				}
				if(edgetable)
					MEM_freeN(edgetable);
			}
		}
	}
	
	if(!timeoffset) {
		if (test_for_displace(re, ob ) ) {
			calc_vertexnormals(re, obr, 0, 0);
			if(do_autosmooth)
				do_displacement(re, obr, mat, imat);
			else
				do_displacement(re, obr, NULL, NULL);
		}

		if(do_autosmooth) {
			autosmooth(re, obr, mat, me->smoothresh);
		}

		calc_vertexnormals(re, obr, need_tangent, need_nmap_tangent);

		if(need_stress)
			calc_edge_stress(re, obr, me);
	}

	dm->release(dm);
}

/* ------------------------------------------------------------------------- */
/* Lamps and Shadowbuffers													 */
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
	
	if(shb->size<512) shb->size= 512;
	else if(shb->size > lar->bufsize) shb->size= lar->bufsize;
	
	shb->size &= ~15;	/* make sure its multiples of 16 */
	
	shb->samp= lar->samp;
	shb->soft= lar->soft;
	shb->shadhalostep= lar->shadhalostep;
	
	normalize_m4(mat);
	invert_m4_m4(shb->winmat, mat);	/* winmat is temp */
	
	/* matrix: combination of inverse view and lampmat */
	/* calculate again: the ortho-render has no correct viewinv */
	invert_m4_m4(viewinv, re->viewmat);
	mul_m4_m4m4(shb->viewmat, viewinv, shb->winmat);
	
	/* projection */
	shb->d= lar->clipsta;
	shb->clipend= lar->clipend;
	
	/* bias is percentage, made 2x larger because of correction for angle of incidence */
	/* when a ray is closer to parallel of a face, bias value is increased during render */
	shb->bias= (0.02*lar->bias)*0x7FFFFFFF;
	shb->bias= shb->bias*(100/re->r.size);
	
	/* halfway method (average of first and 2nd z) reduces bias issues */
	if(ELEM(lar->buftype, LA_SHADBUF_HALFWAY, LA_SHADBUF_DEEP))
		shb->bias= 0.1f*shb->bias;
	
	shb->compressthresh= lar->compressthresh;
}

static void area_lamp_vectors(LampRen *lar)
{
	float xsize= 0.5*lar->area_size, ysize= 0.5*lar->area_sizey, multifac;

	/* make it smaller, so area light can be multisampled */
	multifac= 1.0f/sqrt((float)lar->ray_totsamp);
	xsize *= multifac;
	ysize *= multifac;
	
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
	float vec[3];
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

	mul_m4_m4m4(mat, ob->obmat, re->viewmat);
	invert_m4_m4(ob->imat, mat);

	copy_m3_m4(lar->mat, mat);
	copy_m3_m4(lar->imat, ob->imat);

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
	lar->compressthresh = la->compressthresh;

	lar->type= la->type;
	lar->mode= la->mode;

	lar->energy= la->energy;
	if(la->mode & LA_NEG) lar->energy= -lar->energy;

	lar->vec[0]= -mat[2][0];
	lar->vec[1]= -mat[2][1];
	lar->vec[2]= -mat[2][2];
	normalize_v3(lar->vec);
	lar->co[0]= mat[3][0];
	lar->co[1]= mat[3][1];
	lar->co[2]= mat[3][2];
	lar->dist= la->dist;
	lar->haint= la->haint;
	lar->distkw= lar->dist*lar->dist;
	lar->r= lar->energy*la->r;
	lar->g= lar->energy*la->g;
	lar->b= lar->energy*la->b;
	lar->shdwr= la->shdwr;
	lar->shdwg= la->shdwg;
	lar->shdwb= la->shdwb;
	lar->k= la->k;

	// area
	lar->ray_samp= la->ray_samp;
	lar->ray_sampy= la->ray_sampy;
	lar->ray_sampz= la->ray_sampz;
	
	lar->area_size= la->area_size;
	lar->area_sizey= la->area_sizey;
	lar->area_sizez= la->area_sizez;

	lar->area_shape= la->area_shape;
	
	/* Annoying, lamp UI does this, but the UI might not have been used? - add here too.
	 * make sure this matches buttons_shading.c's logic */
	if(ELEM4(la->type, LA_AREA, LA_SPOT, LA_SUN, LA_LOCAL) && (la->mode & LA_SHAD_RAY))
		if (ELEM3(la->type, LA_SPOT, LA_SUN, LA_LOCAL))
			if (la->ray_samp_method == LA_SAMP_CONSTANT) la->ray_samp_method = LA_SAMP_HALTON;
	
	lar->ray_samp_method= la->ray_samp_method;
	lar->ray_samp_type= la->ray_samp_type;
	
	lar->adapt_thresh= la->adapt_thresh;
	lar->sunsky = NULL;
	
	if( ELEM(lar->type, LA_SPOT, LA_LOCAL)) {
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
		init_jitter_plane(lar);	// subsamples
	}
	else if(lar->type==LA_SUN){
		lar->ray_totsamp= lar->ray_samp*lar->ray_samp;
		lar->area_shape = LA_AREA_SQUARE;
		lar->area_sizey= lar->area_size;

		if((la->sun_effect_type & LA_SUN_EFFECT_SKY) ||
				(la->sun_effect_type & LA_SUN_EFFECT_AP)){
			lar->sunsky = (struct SunSky*)MEM_callocN(sizeof(struct SunSky), "sunskyren");
			lar->sunsky->effect_type = la->sun_effect_type;
		
			VECCOPY(vec,ob->obmat[2]);
		    normalize_v3(vec);
		    
			InitSunSky(lar->sunsky, la->atm_turbidity, vec, la->horizon_brightness, 
					la->spread, la->sun_brightness, la->sun_size, la->backscattered_light,
					   la->skyblendfac, la->skyblendtype, la->sky_exposure, la->sky_colorspace);
			
			InitAtmosphere(lar->sunsky, la->sun_intensity, 1.0, 1.0, la->atm_inscattering_factor, la->atm_extinction_factor,
					la->atm_distance_factor);
		}
	}
	else lar->ray_totsamp= 0;
	
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

		normalize_v3(lar->imat[0]);
		normalize_v3(lar->imat[1]);
		normalize_v3(lar->imat[2]);

		xn= saacos(lar->spotsi);
		xn= sin(xn)/cos(xn);
		lar->spottexfac= 1.0/(xn);

		if(lar->mode & LA_ONLYSHADOW) {
			if((lar->mode & (LA_SHAD_BUF|LA_SHAD_RAY))==0) lar->mode -= LA_ONLYSHADOW;
		}

	}

	/* set flag for spothalo en initvars */
	if(la->type==LA_SPOT && (la->mode & LA_HALO) && (la->buftype != LA_SHADBUF_DEEP)) {
		if(la->haint>0.0) {
			re->flag |= R_LAMPHALO;

			/* camera position (0,0,0) rotate around lamp */
			lar->sh_invcampos[0]= -lar->co[0];
			lar->sh_invcampos[1]= -lar->co[1];
			lar->sh_invcampos[2]= -lar->co[2];
			mul_m3_v3(lar->imat, lar->sh_invcampos);

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
			if (la->mtex[c]->mapto & LAMAP_COL) 
				lar->mode |= LA_TEXTURE;
			if (la->mtex[c]->mapto & LAMAP_SHAD)
				lar->mode |= LA_SHAD_TEX;

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
			
			if(la->type==LA_AREA && (lar->mode & LA_SHAD_RAY) && (lar->ray_samp_method == LA_SAMP_CONSTANT)) {
				init_jitter_plane(lar);
			}
			else if (la->type==LA_SPOT && (lar->mode & LA_SHAD_BUF) ) {
				/* Per lamp, one shadow buffer is made. */
				lar->bufflag= la->bufflag;
				copy_m4_m4(mat, ob->obmat);
				initshadowbuf(re, lar, mat);	// mat is altered
			}
			
			
			/* this is the way used all over to check for shadow */
			if(lar->shb || (lar->mode & LA_SHAD_RAY)) {
				LampShadowSample *ls;
				LampShadowSubSample *lss;
				int a, b;

				memset(re->shadowsamplenr, 0, sizeof(re->shadowsamplenr));
				
				lar->shadsamp= MEM_mallocN(re->r.threads*sizeof(LampShadowSample), "lamp shadow sample");
				ls= lar->shadsamp;

				/* shadfacs actually mean light, let's put them to 1 to prevent unitialized accidents */
				for(a=0; a<re->r.threads; a++, ls++) {
					lss= ls->s;
					for(b=0; b<re->r.osa; b++, lss++) {
						lss->samplenr= -1;	/* used to detect whether we store or read */
						lss->shadfac[0]= 1.0f;
						lss->shadfac[1]= 1.0f;
						lss->shadfac[2]= 1.0f;
						lss->shadfac[3]= 1.0f;
					}
				}
			}
		}
	}
	
	return go;
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

/* ------------------------------------------------------------------------- */
/* World																	 */
/* ------------------------------------------------------------------------- */

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
		normalize_v3(re->grvec);
		copy_m3_m4(re->imat, re->viewinv);
		
		for(a=0; a<MAX_MTEX; a++) 
			if(re->wrld.mtex[a] && re->wrld.mtex[a]->tex) re->wrld.skytype |= WO_SKYTEX;
		
		/* AO samples should be OSA minimum */
		if(re->osa)
			while(re->wrld.aosamp*re->wrld.aosamp < re->osa) 
				re->wrld.aosamp++;
		if(!(re->r.mode & R_RAYTRACE) && (re->wrld.ao_gather_method == WO_AOGATHER_RAYTRACE))
			re->wrld.mode &= ~WO_AMB_OCC;
	}
	else {
		memset(&re->wrld, 0, sizeof(World));
		re->wrld.exp= 0.0f;
		re->wrld.range= 1.0f;
		
		/* for mist pass */
		re->wrld.miststa= re->clipsta;
		re->wrld.mistdist= re->clipend-re->clipsta;
		re->wrld.misi= 1.0f;
	}
	
	re->wrld.linfac= 1.0 + pow((2.0*re->wrld.exp + 0.5), -10);
	re->wrld.logfac= log( (re->wrld.linfac-1.0)/re->wrld.linfac )/re->wrld.range;
}



/* ------------------------------------------------------------------------- */
/* Object Finalization														 */
/* ------------------------------------------------------------------------- */

/* prevent phong interpolation for giving ray shadow errors (terminator problem) */
static void set_phong_threshold(ObjectRen *obr)
{
//	VertRen *ver;
	VlakRen *vlr;
	float thresh= 0.0, dot;
	int tot=0, i;
	
	/* Added check for 'pointy' situations, only dotproducts of 0.9 and larger 
	   are taken into account. This threshold is meant to work on smooth geometry, not
	   for extreme cases (ton) */
	
	for(i=0; i<obr->totvlak; i++) {
		vlr= RE_findOrAddVlak(obr, i);
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
		obr->ob->smoothresh= cos(0.5*M_PI-saacos(thresh));
	}
}

/* per face check if all samples should be taken.
   if raytrace or multisample, do always for raytraced material, or when material full_osa set */
static void set_fullsample_flag(Render *re, ObjectRen *obr)
{
	VlakRen *vlr;
	int a, trace, mode;

	if(re->osa==0)
		return;
	
	trace= re->r.mode & R_RAYTRACE;
	
	for(a=obr->totvlak-1; a>=0; a--) {
		vlr= RE_findOrAddVlak(obr, a);
		mode= vlr->mat->mode;
		
		if(mode & MA_FULL_OSA) 
			vlr->flag |= R_FULL_OSA;
		else if(trace) {
			if(mode & MA_SHLESS);
			else if(vlr->mat->material_type == MA_TYPE_VOLUME);
			else if((mode & MA_RAYMIRROR) || ((mode & MA_TRANSP) && (mode & MA_RAYTRANSP)))
				/* for blurry reflect/refract, better to take more samples 
				 * inside the raytrace than as OSA samples */
				if ((vlr->mat->gloss_mir == 1.0) && (vlr->mat->gloss_tra == 1.0)) 
					vlr->flag |= R_FULL_OSA;
		}
	}
}

/* split quads for pradictable baking
 * dir 1 == (0,1,2) (0,2,3),  2 == (1,3,0) (1,2,3) 
 */
static void split_quads(ObjectRen *obr, int dir) 
{
	VlakRen *vlr, *vlr1;
	int a;

	for(a=obr->totvlak-1; a>=0; a--) {
		vlr= RE_findOrAddVlak(obr, a);
		
		/* test if rendering as a quad or triangle, skip wire */
		if(vlr->v4 && (vlr->flag & R_STRAND)==0 && (vlr->mat->material_type != MA_TYPE_WIRE)) {
			
			if(vlr->v4) {

				vlr1= RE_vlakren_copy(obr, vlr);
				vlr1->flag |= R_FACE_SPLIT;
				
				if( dir==2 ) vlr->flag |= R_DIVIDE_24;
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
				normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);
				normal_tri_v3( vlr1->n,vlr1->v3->co, vlr1->v2->co, vlr1->v1->co);
			}
			/* clear the flag when not divided */
			else vlr->flag &= ~R_DIVIDE_24;
		}
	}
}

static void check_non_flat_quads(ObjectRen *obr)
{
	VlakRen *vlr, *vlr1;
	VertRen *v1, *v2, *v3, *v4;
	float nor[3], xn, flen;
	int a;

	for(a=obr->totvlak-1; a>=0; a--) {
		vlr= RE_findOrAddVlak(obr, a);
		
		/* test if rendering as a quad or triangle, skip wire */
		if(vlr->v4 && (vlr->flag & R_STRAND)==0 && (vlr->mat->material_type != MA_TYPE_WIRE)) {
			
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
				flen= normal_tri_v3( nor,vlr->v4->co, vlr->v3->co, vlr->v1->co);
				if(flen==0.0) normal_tri_v3( nor,vlr->v4->co, vlr->v2->co, vlr->v1->co);
				
				xn= nor[0]*vlr->n[0] + nor[1]*vlr->n[1] + nor[2]*vlr->n[2];

				if(ABS(xn) < 0.999995 ) {	// checked on noisy fractal grid
					
					float d1, d2;

					vlr1= RE_vlakren_copy(obr, vlr);
					vlr1->flag |= R_FACE_SPLIT;
					
					/* split direction based on vnorms */
					normal_tri_v3( nor,vlr->v1->co, vlr->v2->co, vlr->v3->co);
					d1= nor[0]*vlr->v1->n[0] + nor[1]*vlr->v1->n[1] + nor[2]*vlr->v1->n[2];

					normal_tri_v3( nor,vlr->v2->co, vlr->v3->co, vlr->v4->co);
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
					normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);
					normal_tri_v3( vlr1->n,vlr1->v3->co, vlr1->v2->co, vlr1->v1->co);
				}
				/* clear the flag when not divided */
				else vlr->flag &= ~R_DIVIDE_24;
			}
		}
	}
}

static void finalize_render_object(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	VertRen *ver= NULL;
	StrandRen *strand= NULL;
	StrandBound *sbound= NULL;
	float min[3], max[3], smin[3], smax[3];
	int a, b;

	if(obr->totvert || obr->totvlak || obr->tothalo || obr->totstrand) {
		/* the exception below is because displace code now is in init_render_mesh call, 
		I will look at means to have autosmooth enabled for all object types 
		and have it as general postprocess, like displace */
		if(ob->type!=OB_MESH && test_for_displace(re, ob)) 
			do_displacement(re, obr, NULL, NULL);
	
		if(!timeoffset) {
			/* phong normal interpolation can cause error in tracing
			 * (terminator problem) */
			ob->smoothresh= 0.0;
			if((re->r.mode & R_RAYTRACE) && (re->r.mode & R_SHADOW)) 
				set_phong_threshold(obr);
			
			if (re->flag & R_BAKING && re->r.bake_quad_split != 0) {
				/* Baking lets us define a quad split order */
				split_quads(obr, re->r.bake_quad_split);
			} else {
				check_non_flat_quads(obr);
			}
			
			set_fullsample_flag(re, obr);

			/* compute bounding boxes for clipping */
			INIT_MINMAX(min, max);
			for(a=0; a<obr->totvert; a++) {
				if((a & 255)==0) ver= obr->vertnodes[a>>8].vert;
				else ver++;

				DO_MINMAX(ver->co, min, max);
			}

			if(obr->strandbuf) {
				sbound= obr->strandbuf->bound;
				for(b=0; b<obr->strandbuf->totbound; b++, sbound++) {
					INIT_MINMAX(smin, smax);

					for(a=sbound->start; a<sbound->end; a++) {
						strand= RE_findOrAddStrand(obr, a);
						strand_minmax(strand, smin, smax);
					}

					VECCOPY(sbound->boundbox[0], smin);
					VECCOPY(sbound->boundbox[1], smax);

					DO_MINMAX(smin, min, max);
					DO_MINMAX(smax, min, max);
				}
			}

			VECCOPY(obr->boundbox[0], min);
			VECCOPY(obr->boundbox[1], max);
		}
	}
}

/* ------------------------------------------------------------------------- */
/* Database																	 */
/* ------------------------------------------------------------------------- */

static int render_object_type(int type) 
{
	return ELEM5(type, OB_FONT, OB_CURVE, OB_SURF, OB_MESH, OB_MBALL);
}

static void find_dupli_instances(Render *re, ObjectRen *obr)
{
	ObjectInstanceRen *obi;
	float imat[4][4], obmat[4][4], obimat[4][4], nmat[3][3];
	int first = 1;

	mul_m4_m4m4(obmat, obr->obmat, re->viewmat);
	invert_m4_m4(imat, obmat);

	/* for objects instanced by dupliverts/faces/particles, we go over the
	 * list of instances to find ones that instance obr, and setup their
	 * matrices and obr pointer */
	for(obi=re->instancetable.last; obi; obi=obi->prev) {
		if(!obi->obr && obi->ob == obr->ob && obi->psysindex == obr->psysindex) {
			obi->obr= obr;

			/* compute difference between object matrix and
			 * object matrix with dupli transform, in viewspace */
			copy_m4_m4(obimat, obi->mat);
			mul_m4_m4m4(obi->mat, imat, obimat);

			copy_m3_m4(nmat, obi->mat);
			invert_m3_m3(obi->nmat, nmat);
			transpose_m3(obi->nmat);

			if(!first) {
				re->totvert += obr->totvert;
				re->totvlak += obr->totvlak;
				re->tothalo += obr->tothalo;
				re->totstrand += obr->totstrand;
			}
			else
				first= 0;
		}
	}
}

static void assign_dupligroup_dupli(Render *re, ObjectInstanceRen *obi, ObjectRen *obr)
{
	float imat[4][4], obmat[4][4], obimat[4][4], nmat[3][3];

	mul_m4_m4m4(obmat, obr->obmat, re->viewmat);
	invert_m4_m4(imat, obmat);

	obi->obr= obr;

	/* compute difference between object matrix and
	 * object matrix with dupli transform, in viewspace */
	copy_m4_m4(obimat, obi->mat);
	mul_m4_m4m4(obi->mat, imat, obimat);

	copy_m3_m4(nmat, obi->mat);
	invert_m3_m3(obi->nmat, nmat);
	transpose_m3(obi->nmat);

	re->totvert += obr->totvert;
	re->totvlak += obr->totvlak;
	re->tothalo += obr->tothalo;
	re->totstrand += obr->totstrand;
}

static ObjectRen *find_dupligroup_dupli(Render *re, Object *ob, int psysindex)
{
	ObjectRen *obr;

	/* if the object is itself instanced, we don't want to create an instance
	 * for it */
	if(ob->transflag & OB_RENDER_DUPLI)
		return NULL;

	/* try to find an object that was already created so we can reuse it
	 * and save memory */
	for(obr=re->objecttable.first; obr; obr=obr->next)
		if(obr->ob == ob && obr->psysindex == psysindex && (obr->flag & R_INSTANCEABLE))
			return obr;
	
	return NULL;
}

static void set_dupli_tex_mat(Render *re, ObjectInstanceRen *obi, DupliObject *dob)
{
	/* For duplis we need to have a matrix that transform the coordinate back
	 * to it's original position, without the dupli transforms. We also check
	 * the matrix is actually needed, to save memory on lots of dupliverts for
	 * example */
	static Object *lastob= NULL;
	static int needtexmat= 0;

	/* init */
	if(!re) {
		lastob= NULL;
		needtexmat= 0;
		return;
	}

	/* check if we actually need it */
	if(lastob != dob->ob) {
		Material ***material;
		short a, *totmaterial;

		lastob= dob->ob;
		needtexmat= 0;

		totmaterial= give_totcolp(dob->ob);
		material= give_matarar(dob->ob);

		if(totmaterial && material)
			for(a= 0; a<*totmaterial; a++)
				if((*material)[a] && (*material)[a]->texco & TEXCO_OBJECT)
					needtexmat= 1;
	}

	if(needtexmat) {
		float imat[4][4];

		obi->duplitexmat= BLI_memarena_alloc(re->memArena, sizeof(float)*4*4);
		invert_m4_m4(imat, dob->mat);
		mul_serie_m4(obi->duplitexmat, re->viewmat, dob->omat, imat, re->viewinv, 0, 0, 0, 0);
	}
}

static void init_render_object_data(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	ParticleSystem *psys;
	int i;

	if(obr->psysindex) {
		if((!obr->prev || obr->prev->ob != ob) && ob->type==OB_MESH) {
			/* the emitter mesh wasn't rendered so the modifier stack wasn't
			 * evaluated with render settings */
			DerivedMesh *dm;
			dm = mesh_create_derived_render(re->scene, ob,	CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL);
			dm->release(dm);
		}

		for(psys=ob->particlesystem.first, i=0; i<obr->psysindex-1; i++)
			psys= psys->next;

		render_new_particle_system(re, obr, psys, timeoffset);
	}
	else {
		if ELEM(ob->type, OB_FONT, OB_CURVE)
			init_render_curve(re, obr, timeoffset);
		else if(ob->type==OB_SURF)
			init_render_surf(re, obr);
		else if(ob->type==OB_MESH)
			init_render_mesh(re, obr, timeoffset);
		else if(ob->type==OB_MBALL)
			init_render_mball(re, obr);
	}

	finalize_render_object(re, obr, timeoffset);
	
	re->totvert += obr->totvert;
	re->totvlak += obr->totvlak;
	re->tothalo += obr->tothalo;
	re->totstrand += obr->totstrand;
}

static void add_render_object(Render *re, Object *ob, Object *par, DupliObject *dob, int timeoffset, int vectorlay)
{
	ObjectRen *obr;
	ObjectInstanceRen *obi;
	ParticleSystem *psys;
	int show_emitter, allow_render= 1, index, psysindex, i;

	index= (dob)? dob->index: 0;

	/* the emitter has to be processed first (render levels of modifiers) */
	/* so here we only check if the emitter should be rendered */
	if(ob->particlesystem.first) {
		show_emitter= 0;
		for(psys=ob->particlesystem.first; psys; psys=psys->next) {
			show_emitter += psys->part->draw & PART_DRAW_EMITTER;
			psys_render_set(ob, psys, re->viewmat, re->winmat, re->winx, re->winy, timeoffset);
		}

		/* if no psys has "show emitter" selected don't render emitter */
		if(show_emitter == 0)
			allow_render= 0;
	}

	/* one render object for the data itself */
	if(allow_render) {
		obr= RE_addRenderObject(re, ob, par, index, 0, ob->lay);
		if((dob && !dob->animated) || (ob->transflag & OB_RENDER_DUPLI)) {
			obr->flag |= R_INSTANCEABLE;
			copy_m4_m4(obr->obmat, ob->obmat);
		}
		if(obr->lay & vectorlay)
			obr->flag |= R_NEED_VECTORS;
		init_render_object_data(re, obr, timeoffset);

		/* only add instance for objects that have not been used for dupli */
		if(!(ob->transflag & OB_RENDER_DUPLI)) {
			obi= RE_addRenderInstance(re, obr, ob, par, index, 0, NULL, ob->lay);
			if(dob) set_dupli_tex_mat(re, obi, dob);
		}
		else
			find_dupli_instances(re, obr);
			
		for (i=1; i<=ob->totcol; i++) {
			Material* ma = give_render_material(re, ob, i);
			if (ma && ma->material_type == MA_TYPE_VOLUME)
				add_volume(re, obr, ma);
		}
	}

	/* and one render object per particle system */
	if(ob->particlesystem.first) {
		psysindex= 1;
		for(psys=ob->particlesystem.first; psys; psys=psys->next, psysindex++) {
			obr= RE_addRenderObject(re, ob, par, index, psysindex, ob->lay);
			if((dob && !dob->animated) || (ob->transflag & OB_RENDER_DUPLI)) {
				obr->flag |= R_INSTANCEABLE;
				copy_m4_m4(obr->obmat, ob->obmat);
			}
			if(obr->lay & vectorlay)
				obr->flag |= R_NEED_VECTORS;
			init_render_object_data(re, obr, timeoffset);
			psys_render_restore(ob, psys);

			/* only add instance for objects that have not been used for dupli */
			if(!(ob->transflag & OB_RENDER_DUPLI)) {
				obi= RE_addRenderInstance(re, obr, ob, par, index, psysindex, NULL, ob->lay);
				if(dob) set_dupli_tex_mat(re, obi, dob);
			}
			else
				find_dupli_instances(re, obr);
		}
	}
}

/* par = pointer to duplicator parent, needed for object lookup table */
/* index = when duplicater copies same object (particle), the counter */
static void init_render_object(Render *re, Object *ob, Object *par, DupliObject *dob, int timeoffset, int vectorlay)
{
	static double lasttime= 0.0;
	double time;
	float mat[4][4];

	if(ob->type==OB_LAMP)
		add_render_lamp(re, ob);
	else if(render_object_type(ob->type))
		add_render_object(re, ob, par, dob, timeoffset, vectorlay);
	else {
		mul_m4_m4m4(mat, ob->obmat, re->viewmat);
		invert_m4_m4(ob->imat, mat);
	}
	
	time= PIL_check_seconds_timer();
	if(time - lasttime > 1.0) {
		lasttime= time;
		/* clumsy copying still */
		re->i.totvert= re->totvert;
		re->i.totface= re->totvlak;
		re->i.totstrand= re->totstrand;
		re->i.tothalo= re->tothalo;
		re->i.totlamp= re->totlamp;
		re->stats_draw(re->sdh, &re->i);
	}

	ob->flag |= OB_DONE;
}

void RE_Database_Free(Render *re)
{
	Object *ob = NULL;
	LampRen *lar;
	
	/* statistics for debugging render memory usage */
	if((G.f & G_DEBUG) && (G.rendering)) {
		if((re->r.scemode & R_PREVIEWBUTS)==0) {
			BKE_image_print_memlist();
			MEM_printmemlist_stats();
		}
	}

	/* FREE */
	
	for(lar= re->lampren.first; lar; lar= lar->next) {
		freeshadowbuf(lar);
		if(lar->jitter) MEM_freeN(lar->jitter);
		if(lar->shadsamp) MEM_freeN(lar->shadsamp);
		if(lar->sunsky) MEM_freeN(lar->sunsky);
		curvemapping_free(lar->curfalloff);
	}
	
	free_volume_precache(re);
	
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
#if 0	/* radio can be redone better */
	end_radio_render();
#endif
	end_render_materials();
	end_render_textures();
	
	free_pointdensities(re);
	free_voxeldata(re);
	
	free_camera_inside_volumes(re);
	
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
	if(re->r.mode & R_RAYTRACE)
		free_render_qmcsampler(re);
	
	if(re->r.mode & R_RAYTRACE) freeraytree(re);

	free_sss(re);
	free_occ(re);
	free_strand_surface(re);
	
	re->totvlak=re->totvert=re->totstrand=re->totlamp=re->tothalo= 0;
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

static int allow_render_object(Render *re, Object *ob, int nolamps, int onlyselected, Object *actob)
{
	/* override not showing object when duplis are used with particles */
	if(ob->transflag & OB_DUPLIPARTS)
		; /* let particle system(s) handle showing vs. not showing */
	else if((ob->transflag & OB_DUPLI) && !(ob->transflag & OB_DUPLIFRAMES))
		return 0;
	
	/* don't add non-basic meta objects, ends up having renderobjects with no geometry */
	if (ob->type == OB_MBALL && ob!=find_basis_mball(re->scene, ob))
		return 0;
	
	if(nolamps && (ob->type==OB_LAMP))
		return 0;
	
	if(onlyselected && (ob!=actob && !(ob->flag & SELECT)))
		return 0;
	
	return 1;
}

static int allow_render_dupli_instance(Render *re, DupliObject *dob, Object *obd)
{
	ParticleSystem *psys;
	Material *ma;
	short a, *totmaterial;

	/* don't allow objects with halos. we need to have
	 * all halo's to sort them globally in advance */
	totmaterial= give_totcolp(obd);

	if(totmaterial) {
		for(a= 0; a<*totmaterial; a++) {
			ma= give_current_material(obd, a);
			if(ma && (ma->material_type == MA_TYPE_HALO))
				return 0;
		}
	}

	for(psys=obd->particlesystem.first; psys; psys=psys->next)
		if(!ELEM5(psys->part->ren_as, PART_DRAW_BB, PART_DRAW_LINE, PART_DRAW_PATH, PART_DRAW_OB, PART_DRAW_GR))
			return 0;

	/* don't allow lamp, animated duplis, or radio render */
	return (render_object_type(obd->type) &&
	        (!(dob->type == OB_DUPLIGROUP) || !dob->animated) &&
	        !(re->r.mode & R_RADIO));
}

static void dupli_render_particle_set(Render *re, Object *ob, int timeoffset, int level, int enable)
{
	/* ugly function, but we need to set particle systems to their render
	 * settings before calling object_duplilist, to get render level duplis */
	Group *group;
	GroupObject *go;
	ParticleSystem *psys;
	DerivedMesh *dm;

	if(level >= MAX_DUPLI_RECUR)
		return;
	
	if(ob->transflag & OB_DUPLIPARTS) {
		for(psys=ob->particlesystem.first; psys; psys=psys->next) {
			if(ELEM(psys->part->ren_as, PART_DRAW_OB, PART_DRAW_GR)) {
				if(enable)
					psys_render_set(ob, psys, re->viewmat, re->winmat, re->winx, re->winy, timeoffset);
				else
					psys_render_restore(ob, psys);
			}
		}

		if(level == 0 && enable) {
			/* this is to make sure we get render level duplis in groups:
			* the derivedmesh must be created before init_render_mesh,
			* since object_duplilist does dupliparticles before that */
			dm = mesh_create_derived_render(re->scene, ob, CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL);
			dm->release(dm);

			for(psys=ob->particlesystem.first; psys; psys=psys->next)
				psys_get_modifier(ob, psys)->flag &= ~eParticleSystemFlag_psys_updated;
		}
	}

	if(ob->dup_group==NULL) return;
	group= ob->dup_group;

	for(go= group->gobject.first; go; go= go->next)
		dupli_render_particle_set(re, go->ob, timeoffset, level+1, enable);
}

static int get_vector_renderlayers(Scene *sce)
{
	SceneRenderLayer *srl;
	int lay= 0;

    for(srl= sce->r.layers.first; srl; srl= srl->next)
		if(srl->passflag & SCE_PASS_VECTOR)
			lay |= srl->lay;

	return lay;
}

static void add_group_render_dupli_obs(Render *re, Group *group, int nolamps, int onlyselected, Object *actob, int timeoffset, int vectorlay, int level)
{
	GroupObject *go;
	Object *ob;

	/* simple preventing of too deep nested groups */
	if(level>MAX_DUPLI_RECUR) return;

	/* recursively go into dupligroups to find objects with OB_RENDER_DUPLI
	 * that were not created yet */
	for(go= group->gobject.first; go; go= go->next) {
		ob= go->ob;

		if(ob->flag & OB_DONE) {
			if(ob->transflag & OB_RENDER_DUPLI) {
				if(allow_render_object(re, ob, nolamps, onlyselected, actob)) {
					init_render_object(re, ob, NULL, 0, timeoffset, vectorlay);
					ob->transflag &= ~OB_RENDER_DUPLI;

					if(ob->dup_group)
						add_group_render_dupli_obs(re, ob->dup_group, nolamps, onlyselected, actob, timeoffset, vectorlay, level+1);
				}
			}
		}
	}
}

static void database_init_objects(Render *re, unsigned int renderlay, int nolamps, int onlyselected, Object *actob, int timeoffset)
{
	Base *base;
	Object *ob;
	Group *group;
	ObjectInstanceRen *obi;
	Scene *sce;
	float mat[4][4];
	int lay, vectorlay, redoimat= 0;

	/* for duplis we need the Object texture mapping to work as if
	 * untransformed, set_dupli_tex_mat sets the matrix to allow that
	 * NULL is just for init */
	set_dupli_tex_mat(NULL, NULL, NULL);

	for(SETLOOPER(re->scene, base)) {
		ob= base->object;
		/* imat objects has to be done here, since displace can have texture using Object map-input */
		mul_m4_m4m4(mat, ob->obmat, re->viewmat);
		invert_m4_m4(ob->imat, mat);
		/* each object should only be rendered once */
		ob->flag &= ~OB_DONE;
		ob->transflag &= ~OB_RENDER_DUPLI;
	}

	for(SETLOOPER(re->scene, base)) {
		ob= base->object;

		/* in the prev/next pass for making speed vectors, avoid creating
		 * objects that are not on a renderlayer with a vector pass, can
		 * save a lot of time in complex scenes */
		vectorlay= get_vector_renderlayers(sce);
		lay= (timeoffset)? renderlay & vectorlay: renderlay;

		/* if the object has been restricted from rendering in the outliner, ignore it */
		if(ob->restrictflag & OB_RESTRICT_RENDER) continue;

		/* OB_DONE means the object itself got duplicated, so was already converted */
		if(ob->flag & OB_DONE) {
			/* OB_RENDER_DUPLI means instances for it were already created, now
			 * it still needs to create the ObjectRen containing the data */
			if(ob->transflag & OB_RENDER_DUPLI) {
				if(allow_render_object(re, ob, nolamps, onlyselected, actob)) {
					init_render_object(re, ob, NULL, 0, timeoffset, vectorlay);
					ob->transflag &= ~OB_RENDER_DUPLI;
				}
			}
		}
		else if((base->lay & lay) || (ob->type==OB_LAMP && (base->lay & re->scene->lay)) ) {
			if((ob->transflag & OB_DUPLI) && (ob->type!=OB_MBALL)) {
				DupliObject *dob;
				ListBase *lb;

				redoimat= 1;

				/* create list of duplis generated by this object, particle
				 * system need to have render settings set for dupli particles */
				dupli_render_particle_set(re, ob, timeoffset, 0, 1);
				lb= object_duplilist(sce, ob);
				dupli_render_particle_set(re, ob, timeoffset, 0, 0);

				for(dob= lb->first; dob; dob= dob->next) {
					Object *obd= dob->ob;
					
					copy_m4_m4(obd->obmat, dob->mat);

					/* group duplis need to set ob matrices correct, for deform. so no_draw is part handled */
					if(!(obd->transflag & OB_RENDER_DUPLI) && dob->no_draw)
						continue;

					if(obd->restrictflag & OB_RESTRICT_RENDER)
						continue;

					if(obd->type==OB_MBALL)
						continue;

					if(!allow_render_object(re, obd, nolamps, onlyselected, actob))
						continue;

					if(allow_render_dupli_instance(re, dob, obd)) {
						ParticleSystem *psys;
						ObjectRen *obr = NULL;
						int psysindex;
						float mat[4][4];

						/* instances instead of the actual object are added in two cases, either
						 * this is a duplivert/face/particle, or it is a non-animated object in
						 * a dupligroup that has already been created before */
						if(dob->type != OB_DUPLIGROUP || (obr=find_dupligroup_dupli(re, obd, 0))) {
							mul_m4_m4m4(mat, dob->mat, re->viewmat);
							obi= RE_addRenderInstance(re, NULL, obd, ob, dob->index, 0, mat, obd->lay);

							/* fill in instance variables for texturing */
							set_dupli_tex_mat(re, obi, dob);
							if(dob->type != OB_DUPLIGROUP) {
								VECCOPY(obi->dupliorco, dob->orco);
								obi->dupliuv[0]= dob->uv[0];
								obi->dupliuv[1]= dob->uv[1];
							}
							else {
								/* for the second case, setup instance to point to the already
								 * created object, and possibly setup instances if this object
								 * itself was duplicated. for the first case find_dupli_instances
								 * will be called later. */
								assign_dupligroup_dupli(re, obi, obr);
								if(obd->transflag & OB_RENDER_DUPLI)
									find_dupli_instances(re, obr);
							}
						}
						else
							/* can't instance, just create the object */
							init_render_object(re, obd, ob, dob, timeoffset, vectorlay);

						/* same logic for particles, each particle system has it's own object, so
						 * need to go over them separately */
						psysindex= 1;
						for(psys=obd->particlesystem.first; psys; psys=psys->next) {
							if(dob->type != OB_DUPLIGROUP || (obr=find_dupligroup_dupli(re, ob, psysindex))) {
								obi= RE_addRenderInstance(re, NULL, obd, ob, dob->index, psysindex++, mat, obd->lay);

								set_dupli_tex_mat(re, obi, dob);
								if(dob->type != OB_DUPLIGROUP) {
									VECCOPY(obi->dupliorco, dob->orco);
									obi->dupliuv[0]= dob->uv[0];
									obi->dupliuv[1]= dob->uv[1];
								}
								else {
									assign_dupligroup_dupli(re, obi, obr);
									if(obd->transflag & OB_RENDER_DUPLI)
										find_dupli_instances(re, obr);
								}
							}
						}
						
						if(dob->type != OB_DUPLIGROUP) {
							obd->flag |= OB_DONE;
							obd->transflag |= OB_RENDER_DUPLI;
						}
					}
					else
						init_render_object(re, obd, ob, dob, timeoffset, vectorlay);
					
					if(re->test_break(re->tbh)) break;
				}
				free_object_duplilist(lb);

				if(allow_render_object(re, ob, nolamps, onlyselected, actob))
					init_render_object(re, ob, NULL, 0, timeoffset, vectorlay);
			}
			else if(allow_render_object(re, ob, nolamps, onlyselected, actob))
				init_render_object(re, ob, NULL, 0, timeoffset, vectorlay);
		}

		if(re->test_break(re->tbh)) break;
	}

	/* objects in groups with OB_RENDER_DUPLI set still need to be created,
	 * since they may not be part of the scene */
	for(group= G.main->group.first; group; group=group->id.next)
		add_group_render_dupli_obs(re, group, nolamps, onlyselected, actob, timeoffset, renderlay, 0);

	/* imat objects has to be done again, since groups can mess it up */
	if(redoimat) {
		for(SETLOOPER(re->scene, base)) {
			ob= base->object;
			mul_m4_m4m4(mat, ob->obmat, re->viewmat);
			invert_m4_m4(ob->imat, mat);
		}
	}

	if(!re->test_break(re->tbh))
		RE_makeRenderInstances(re);
}

/* used to be 'rotate scene' */
void RE_Database_FromScene(Render *re, Scene *scene, int use_camera_view)
{
	extern int slurph_opt;	/* key.c */
	Scene *sce;
	float mat[4][4];
	float amb[3];
	unsigned int lay;

	re->scene= scene;
	
	/* per second, per object, stats print this */
	re->i.infostr= "Preparing Scene data";
	re->i.cfra= scene->r.cfra;
	strncpy(re->i.scenename, scene->id.name+2, 20);
	
	/* XXX add test if dbase was filled already? */
	
	re->memArena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	re->totvlak=re->totvert=re->totstrand=re->totlamp=re->tothalo= 0;
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
		normalize_m4(re->scene->camera->obmat);
		invert_m4_m4(mat, re->scene->camera->obmat);
		RE_SetView(re, mat);
		re->scene->camera->recalc= OB_RECALC_OB; /* force correct matrix for scaled cameras */
	}
	
	init_render_world(re);	/* do first, because of ambient. also requires re->osa set correct */
	if(re->r.mode & R_RAYTRACE) {
		init_render_qmcsampler(re);

		if(re->wrld.mode & WO_AMB_OCC)
			if (re->wrld.ao_samp_method == WO_AOSAMP_CONSTANT)
				init_ao_sphere(&re->wrld);
	}
	
	/* still bad... doing all */
	init_render_textures(re);
	VECCOPY(amb, &re->wrld.ambr);
	init_render_materials(re->r.mode, amb);
	set_node_shader_lamp_loop(shade_material_loop);

	/* MAKE RENDER DATA */
	database_init_objects(re, lay, 0, 0, 0, 0);
	
	if(!re->test_break(re->tbh)) {
		int tothalo;

		set_material_lightgroups(re);
		for(sce= re->scene; sce; sce= sce->set)
			set_renderlayer_lightgroups(re, sce);
		
		slurph_opt= 1;
		
		/* for now some clumsy copying still */
		re->i.totvert= re->totvert;
		re->i.totface= re->totvlak;
		re->i.totstrand= re->totstrand;
		re->i.tothalo= re->tothalo;
		re->i.totlamp= re->totlamp;
		re->stats_draw(re->sdh, &re->i);
		
		/* don't sort stars */
		tothalo= re->tothalo;
		if(!re->test_break(re->tbh))
			if(re->wrld.mode & WO_STARS)
				RE_make_stars(re, NULL, NULL, NULL, NULL);
		sort_halos(re, tothalo);
		
		init_camera_inside_volumes(re);
		
		re->i.infostr= "Creating Shadowbuffers";
		re->stats_draw(re->sdh, &re->i);

		/* SHADOW BUFFER */
		threaded_makeshadowbufs(re);
		
		/* yafray: 'direct' radiosity, environment maps and raytree init not needed for yafray render */
		/* although radio mode could be useful at some point, later */
		if (re->r.renderer==R_INTERN) {
#if 0		/* RADIO was removed */
			/* RADIO (uses no R anymore) */
			if(!re->test_break(re->tbh))
				if(re->r.mode & R_RADIO) do_radio_render(re);
#endif
			/* raytree */
			if(!re->test_break(re->tbh)) {
				if(re->r.mode & R_RAYTRACE) {
					makeraytree(re);
				}
			}
			/* ENVIRONMENT MAPS */
			if(!re->test_break(re->tbh))
				make_envmaps(re);
				
			/* point density texture */
			if(!re->test_break(re->tbh))
				make_pointdensities(re);
			/* voxel data texture */
			if(!re->test_break(re->tbh))
				make_voxeldata(re);
		}
		
		if(!re->test_break(re->tbh))
			project_renderdata(re, projectverto, re->r.mode & R_PANORAMA, 0, 1);
		
		/* Occlusion */
		if((re->wrld.mode & WO_AMB_OCC) && !re->test_break(re->tbh))
			if(re->wrld.ao_gather_method == WO_AOGATHER_APPROX)
				if(re->r.renderer==R_INTERN)
					if(re->r.mode & R_SHADOW)
						make_occ_tree(re);

		/* SSS */
		if((re->r.mode & R_SSS) && !re->test_break(re->tbh))
			if(re->r.renderer==R_INTERN)
				make_sss_tree(re);
		
		if(!re->test_break(re->tbh))
			if(re->r.mode & R_RAYTRACE)
				volume_precache(re);
		
	}
	
	if(re->test_break(re->tbh))
		RE_Database_Free(re);
	else
		re->i.convertdone= 1;
	
	re->i.infostr= NULL;
	re->stats_draw(re->sdh, &re->i);
}

/* exported call to recalculate hoco for vertices, when winmat changed */
void RE_DataBase_ApplyWindow(Render *re)
{
	project_renderdata(re, projectverto, 0, 0, 0);
}

void RE_DataBase_GetView(Render *re, float mat[][4])
{
	copy_m4_m4(mat, re->viewmat);
}

/* ------------------------------------------------------------------------- */
/* Speed Vectors															 */
/* ------------------------------------------------------------------------- */

static void database_fromscene_vectors(Render *re, Scene *scene, int timeoffset)
{
	extern int slurph_opt;	/* key.c */
	float mat[4][4];
	unsigned int lay;
	
	re->scene= scene;
	
	/* XXX add test if dbase was filled already? */
	
	re->memArena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	re->totvlak=re->totvert=re->totstrand=re->totlamp=re->tothalo= 0;
	re->i.totface=re->i.totvert=re->i.totstrand=re->i.totlamp=re->i.tothalo= 0;
	re->lights.first= re->lights.last= NULL;

	slurph_opt= 0;
	
	/* in localview, lamps are using normal layers, objects only local bits */
	if(re->scene->lay & 0xFF000000) lay= re->scene->lay & 0xFF000000;
	else lay= re->scene->lay;
	
	/* applies changes fully */
	scene->r.cfra += timeoffset;
	scene_update_for_newframe(re->scene, lay);
	
	/* if no camera, viewmat should have been set! */
	if(re->scene->camera) {
		normalize_m4(re->scene->camera->obmat);
		invert_m4_m4(mat, re->scene->camera->obmat);
		RE_SetView(re, mat);
	}
	
	/* MAKE RENDER DATA */
	database_init_objects(re, lay, 0, 0, 0, timeoffset);
	
	if(!re->test_break(re->tbh))
		project_renderdata(re, projectverto, re->r.mode & R_PANORAMA, 0, 1);

	/* do this in end, particles for example need cfra */
	scene->r.cfra -= timeoffset;
}

/* choose to use static, to prevent giving too many args to this call */
static void speedvector_project(Render *re, float *zco, float *co, float *ho)
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
	if(ho[3]<0.1f) div= 10.0f;
	else div= 1.0f/ho[3];
	
	/* use cylinder projection */
	if(pano) {
		float vec[3], ang;
		/* angle between (0,0,-1) and (co) */
		VECCOPY(vec, co);

		ang= saacos(-vec[2]/sqrt(vec[0]*vec[0] + vec[2]*vec[2]));
		if(vec[0]<0.0f) ang= -ang;
		zco[0]= ang/pixelphix + zmulx;
		
		ang= 0.5f*M_PI - saacos(vec[1]/sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]));
		zco[1]= ang/pixelphiy + zmuly;
		
	}
	else {
		zco[0]= zmulx*(1.0f+ho[0]*div);
		zco[1]= zmuly*(1.0f+ho[1]*div);
	}
}

static void calculate_speedvector(float *vectors, int step, float winsq, float winroot, float *co, float *ho, float *speed)
{
	float zco[2], len;

	speedvector_project(NULL, zco, co, ho);
	
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

static float *calculate_strandsurface_speedvectors(Render *re, ObjectInstanceRen *obi, StrandSurface *mesh)
{
	float winsq= re->winx*re->winy, winroot= sqrt(winsq), (*winspeed)[4];
	float ho[4], prevho[4], nextho[4], winmat[4][4], vec[2];
	int a;

	if(mesh->co && mesh->prevco && mesh->nextco) {
		if(obi->flag & R_TRANSFORMED)
			mul_m4_m4m4(winmat, obi->mat, re->winmat);
		else
			copy_m4_m4(winmat, re->winmat);

		winspeed= MEM_callocN(sizeof(float)*4*mesh->totvert, "StrandSurfWin");

		for(a=0; a<mesh->totvert; a++) {
			projectvert(mesh->co[a], winmat, ho);

			projectvert(mesh->prevco[a], winmat, prevho);
			speedvector_project(NULL, vec, mesh->prevco[a], prevho);
			calculate_speedvector(vec, 0, winsq, winroot, mesh->co[a], ho, winspeed[a]);

			projectvert(mesh->nextco[a], winmat, nextho);
			speedvector_project(NULL, vec, mesh->nextco[a], nextho);
			calculate_speedvector(vec, 1, winsq, winroot, mesh->co[a], ho, winspeed[a]);
		}

		return (float*)winspeed;
	}

	return NULL;
}

static void calculate_speedvectors(Render *re, ObjectInstanceRen *obi, float *vectors, int step)
{
	ObjectRen *obr= obi->obr;
	VertRen *ver= NULL;
	StrandRen *strand= NULL;
	StrandBuffer *strandbuf;
	StrandSurface *mesh= NULL;
	float *speed, (*winspeed)[4]=NULL, ho[4], winmat[4][4];
	float *co1, *co2, *co3, *co4, w[4];
	float winsq= re->winx*re->winy, winroot= sqrt(winsq);
	int a, *face, *index;

	if(obi->flag & R_TRANSFORMED)
		mul_m4_m4m4(winmat, obi->mat, re->winmat);
	else
		copy_m4_m4(winmat, re->winmat);

	if(obr->vertnodes) {
		for(a=0; a<obr->totvert; a++, vectors+=2) {
			if((a & 255)==0) ver= obr->vertnodes[a>>8].vert;
			else ver++;

			speed= RE_vertren_get_winspeed(obi, ver, 1);
			projectvert(ver->co, winmat, ho);
			calculate_speedvector(vectors, step, winsq, winroot, ver->co, ho, speed);
		}
	}

	if(obr->strandnodes) {
		strandbuf= obr->strandbuf;
		mesh= (strandbuf)? strandbuf->surface: NULL;

		/* compute speed vectors at surface vertices */
		if(mesh)
			winspeed= (float(*)[4])calculate_strandsurface_speedvectors(re, obi, mesh);

		if(winspeed) {
			for(a=0; a<obr->totstrand; a++, vectors+=2) {
				if((a & 255)==0) strand= obr->strandnodes[a>>8].strand;
				else strand++;

				index= RE_strandren_get_face(obr, strand, 0);
				if(index && *index < mesh->totface) {
					speed= RE_strandren_get_winspeed(obi, strand, 1);

					/* interpolate speed vectors from strand surface */
					face= mesh->face[*index];

					co1= mesh->co[face[0]];
					co2= mesh->co[face[1]];
					co3= mesh->co[face[2]];
					co4= (face[3])? mesh->co[face[3]]: NULL;

					interp_weights_face_v3( w,co1, co2, co3, co4, strand->vert->co);

					speed[0]= speed[1]= speed[2]= speed[3]= 0.0f;
					QUATADDFAC(speed, speed, winspeed[face[0]], w[0]);
					QUATADDFAC(speed, speed, winspeed[face[1]], w[1]);
					QUATADDFAC(speed, speed, winspeed[face[2]], w[2]);
					if(face[3])
						QUATADDFAC(speed, speed, winspeed[face[3]], w[3]);
				}
			}

			MEM_freeN(winspeed);
		}
	}
}

static int load_fluidsimspeedvectors(Render *re, ObjectInstanceRen *obi, float *vectors, int step)
{
	ObjectRen *obr= obi->obr;
	Object *fsob= obr->ob;
	VertRen *ver= NULL;
	float *speed, div, zco[2], avgvel[4] = {0.0, 0.0, 0.0, 0.0};
	float zmulx= re->winx/2, zmuly= re->winy/2, len;
	float winsq= re->winx*re->winy, winroot= sqrt(winsq);
	int a, j;
	float hoco[4], ho[4], fsvec[4], camco[4];
	float mat[4][4], winmat[4][4];
	float imat[4][4];
	FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(fsob, eModifierType_Fluidsim);
	FluidsimSettings *fss;
	float *velarray = NULL;
	
	/* only one step needed */
	if(step) return 1;
	
	if(fluidmd)
		fss = fluidmd->fss;
	else
		return 0;
	
	copy_m4_m4(mat, re->viewmat);
	invert_m4_m4(imat, mat);

	/* set first vertex OK */
	if(!fss->meshSurfNormals) return 0;
	
	if( obr->totvert != GET_INT_FROM_POINTER(fss->meshSurface) ) {
		//fprintf(stderr, "load_fluidsimspeedvectors - modified fluidsim mesh, not using speed vectors (%d,%d)...\n", obr->totvert, fsob->fluidsimSettings->meshSurface->totvert); // DEBUG
		return 0;
	}
	
	velarray = (float *)fss->meshSurfNormals;

	if(obi->flag & R_TRANSFORMED)
		mul_m4_m4m4(winmat, obi->mat, re->winmat);
	else
		copy_m4_m4(winmat, re->winmat);
	
	/* (bad) HACK calculate average velocity */
	/* better solution would be fixing getVelocityAt() in intern/elbeem/intern/solver_util.cpp
	so that also small drops/little water volumes return a velocity != 0. 
	But I had no luck in fixing that function - DG */
	for(a=0; a<obr->totvert; a++) {
		for(j=0;j<3;j++) avgvel[j] += velarray[3*a + j];
		
	}
	for(j=0;j<3;j++) avgvel[j] /= (float)(obr->totvert);
	
	
	for(a=0; a<obr->totvert; a++, vectors+=2) {
		if((a & 255)==0)
			ver= obr->vertnodes[a>>8].vert;
		else
			ver++;

		// get fluid velocity
		fsvec[3] = 0.; 
		//fsvec[0] = fsvec[1] = fsvec[2] = fsvec[3] = 0.; fsvec[2] = 2.; // NT fixed test
		for(j=0;j<3;j++) fsvec[j] = velarray[3*a + j];
		
		/* (bad) HACK insert average velocity if none is there (see previous comment) */
		if((fsvec[0] == 0.0) && (fsvec[1] == 0.0) && (fsvec[2] == 0.0))
		{
			fsvec[0] = avgvel[0];
			fsvec[1] = avgvel[1];
			fsvec[2] = avgvel[2];
		}
		
		// transform (=rotate) to cam space
		camco[0]= imat[0][0]*fsvec[0] + imat[0][1]*fsvec[1] + imat[0][2]*fsvec[2];
		camco[1]= imat[1][0]*fsvec[0] + imat[1][1]*fsvec[1] + imat[1][2]*fsvec[2];
		camco[2]= imat[2][0]*fsvec[0] + imat[2][1]*fsvec[1] + imat[2][2]*fsvec[2];

		// get homogenous coordinates
		projectvert(camco, winmat, hoco);
		projectvert(ver->co, winmat, ho);
		
		/* now map hocos to screenspace, uses very primitive clip still */
		// use ho[3] of original vertex, xy component of vel. direction
		if(ho[3]<0.1f) div= 10.0f;
		else div= 1.0f/ho[3];
		zco[0]= zmulx*hoco[0]*div;
		zco[1]= zmuly*hoco[1]*div;
		
		// maximize speed as usual
		len= zco[0]*zco[0] + zco[1]*zco[1];
		if(len > winsq) {
			len= winroot/sqrt(len);
			zco[0]*= len; zco[1]*= len;
		}
		
		speed= RE_vertren_get_winspeed(obi, ver, 1);
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
	ObjectInstanceRen *obi, *obilb;
	ObjectRen *obr;
	VertRen *ver= NULL;
	float *vec, ho[4], winmat[4][4];
	int a, totvector;

	for(obi= re->instancetable.first; obi; obi= obi->next) {
		obr= obi->obr;

		obilb= MEM_mallocN(sizeof(ObjectInstanceRen), "ObInstanceVector");
		memcpy(obilb, obi, sizeof(ObjectInstanceRen));
		BLI_addtail(lb, obilb);

		obilb->totvector= totvector= obr->totvert;

		if(totvector > 0) {
			vec= obilb->vectors= MEM_mallocN(2*sizeof(float)*totvector, "vector array");

			if(obi->flag & R_TRANSFORMED)
				mul_m4_m4m4(winmat, obi->mat, re->winmat);
			else
				copy_m4_m4(winmat, re->winmat);

			for(a=0; a<obr->totvert; a++, vec+=2) {
				if((a & 255)==0) ver= obr->vertnodes[a>>8].vert;
				else ver++;
				
				projectvert(ver->co, winmat, ho);
				speedvector_project(NULL, vec, ver->co, ho);
			}
		}
	}
}

static void free_dbase_object_vectors(ListBase *lb)
{
	ObjectInstanceRen *obi;
	
	for(obi= lb->first; obi; obi= obi->next)
		if(obi->vectors)
			MEM_freeN(obi->vectors);
	BLI_freelistN(lb);
}

void RE_Database_FromScene_Vectors(Render *re, Scene *sce)
{
	ObjectInstanceRen *obi, *oldobi;
	StrandSurface *mesh;
	ListBase *table;
	ListBase oldtable= {NULL, NULL}, newtable= {NULL, NULL};
	ListBase strandsurface;
	int step;
	
	re->i.infostr= "Calculating previous vectors";
	re->r.mode |= R_SPEED;
	
	speedvector_project(re, NULL, NULL, NULL);	/* initializes projection code */
	
	/* creates entire dbase */
	database_fromscene_vectors(re, sce, -1);
	
	/* copy away vertex info */
	copy_dbase_object_vectors(re, &oldtable);
		
	/* free dbase and make the future one */
	strandsurface= re->strandsurface;
	memset(&re->strandsurface, 0, sizeof(ListBase));
	RE_Database_Free(re);
	re->strandsurface= strandsurface;
	
	if(!re->test_break(re->tbh)) {
		/* creates entire dbase */
		re->i.infostr= "Calculating next frame vectors";
		
		database_fromscene_vectors(re, sce, +1);
	}	
	/* copy away vertex info */
	copy_dbase_object_vectors(re, &newtable);
	
	/* free dbase and make the real one */
	strandsurface= re->strandsurface;
	memset(&re->strandsurface, 0, sizeof(ListBase));
	RE_Database_Free(re);
	re->strandsurface= strandsurface;
	
	if(!re->test_break(re->tbh))
		RE_Database_FromScene(re, sce, 1);
	
	if(!re->test_break(re->tbh)) {
		for(step= 0; step<2; step++) {
			
			if(step)
				table= &newtable;
			else
				table= &oldtable;
			
			oldobi= table->first;
			for(obi= re->instancetable.first; obi && oldobi; obi= obi->next) {
				int ok= 1;
				FluidsimModifierData *fluidmd;

				if(!(obi->obr->flag & R_NEED_VECTORS))
					continue;

				obi->totvector= obi->obr->totvert;

				/* find matching object in old table */
				if(oldobi->ob!=obi->ob || oldobi->par!=obi->par || oldobi->index!=obi->index || oldobi->psysindex!=obi->psysindex) {
					ok= 0;
					for(oldobi= table->first; oldobi; oldobi= oldobi->next)
						if(oldobi->ob==obi->ob && oldobi->par==obi->par && oldobi->index==obi->index && oldobi->psysindex==obi->psysindex)
							break;
					if(oldobi==NULL)
						oldobi= table->first;
					else
						ok= 1;
				}
				if(ok==0) {
					 printf("speed table: missing object %s\n", obi->ob->id.name+2);
					continue;
				}

				// NT check for fluidsim special treatment
				fluidmd = (FluidsimModifierData *)modifiers_findByType(obi->ob, eModifierType_Fluidsim);
				if(fluidmd && fluidmd->fss && (fluidmd->fss->type & OB_FLUIDSIM_DOMAIN)) {
					// use preloaded per vertex simulation data , only does calculation for step=1
					// NOTE/FIXME - velocities and meshes loaded unnecessarily often during the database_fromscene_vectors calls...
					load_fluidsimspeedvectors(re, obi, oldobi->vectors, step);
				}
				else {
					/* check if both have same amounts of vertices */
					if(obi->totvector==oldobi->totvector)
						calculate_speedvectors(re, obi, oldobi->vectors, step);
					else
						printf("Warning: object %s has different amount of vertices or strands on other frame\n", obi->ob->id.name+2);
				} // not fluidsim

				oldobi= oldobi->next;
			}
		}
	}
	
	free_dbase_object_vectors(&oldtable);
	free_dbase_object_vectors(&newtable);

	for(mesh=re->strandsurface.first; mesh; mesh=mesh->next) {
		if(mesh->prevco) {
			MEM_freeN(mesh->prevco);
			mesh->prevco= NULL;
		}
		if(mesh->nextco) {
			MEM_freeN(mesh->nextco);
			mesh->nextco= NULL;
		}
	}
	
	re->i.infostr= NULL;
	re->stats_draw(re->sdh, &re->i);
}


/* ------------------------------------------------------------------------- */
/* Baking																	 */
/* ------------------------------------------------------------------------- */

/* setup for shaded view or bake, so only lamps and materials are initialized */
/* type:
   RE_BAKE_LIGHT:  for shaded view, only add lamps
   RE_BAKE_ALL:    for baking, all lamps and objects
   RE_BAKE_NORMALS:for baking, no lamps and only selected objects
   RE_BAKE_AO:     for baking, no lamps, but all objects
   RE_BAKE_TEXTURE:for baking, no lamps, only selected objects
   RE_BAKE_DISPLACEMENT:for baking, no lamps, only selected objects
   RE_BAKE_SHADOW: for baking, only shadows, but all objects
*/
void RE_Database_Baking(Render *re, Scene *scene, int type, Object *actob)
{
	float mat[4][4];
	float amb[3];
	unsigned int lay;
	int onlyselected, nolamps;
	
	re->scene= scene;

	/* renderdata setup and exceptions */
	re->r= scene->r;
	
	RE_init_threadcount(re);
	
	re->flag |= R_GLOB_NOPUNOFLIP;
	re->flag |= R_BAKING;
	re->excludeob= actob;
	if(actob)
		re->flag |= R_BAKE_TRACE;

	if(type==RE_BAKE_NORMALS && re->r.bake_normal_space==R_BAKE_SPACE_TANGENT)
		re->flag |= R_NEED_TANGENT;
	
	if(!actob && ELEM4(type, RE_BAKE_LIGHT, RE_BAKE_NORMALS, RE_BAKE_TEXTURE, RE_BAKE_DISPLACEMENT)) {
		re->r.mode &= ~R_SHADOW;
		re->r.mode &= ~R_RAYTRACE;
	}
	
	if(!actob && (type==RE_BAKE_SHADOW)) {
		re->r.mode |= R_SHADOW;
	}
	
	/* setup render stuff */
	re->memArena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	
	re->totvlak=re->totvert=re->totstrand=re->totlamp=re->tothalo= 0;
	re->lights.first= re->lights.last= NULL;
	re->lampren.first= re->lampren.last= NULL;

	/* in localview, lamps are using normal layers, objects only local bits */
	if(re->scene->lay & 0xFF000000) lay= re->scene->lay & 0xFF000000;
	else lay= re->scene->lay;
	
	/* if no camera, set unit */
	if(re->scene->camera) {
		normalize_m4(re->scene->camera->obmat);
		invert_m4_m4(mat, re->scene->camera->obmat);
		RE_SetView(re, mat);
	}
	else {
		unit_m4(mat);
		RE_SetView(re, mat);
	}
	
	init_render_world(re);	/* do first, because of ambient. also requires re->osa set correct */
	if(re->r.mode & R_RAYTRACE) {
		init_render_qmcsampler(re);
		
		if(re->wrld.mode & WO_AMB_OCC)
			if (re->wrld.ao_samp_method == WO_AOSAMP_CONSTANT)
				init_ao_sphere(&re->wrld);
	}
	
	/* still bad... doing all */
	init_render_textures(re);
	
	VECCOPY(amb, &re->wrld.ambr);
	init_render_materials(re->r.mode, amb);
	
	set_node_shader_lamp_loop(shade_material_loop);
	
	/* MAKE RENDER DATA */
	nolamps= !ELEM3(type, RE_BAKE_LIGHT, RE_BAKE_ALL, RE_BAKE_SHADOW);
	onlyselected= ELEM3(type, RE_BAKE_NORMALS, RE_BAKE_TEXTURE, RE_BAKE_DISPLACEMENT);

	database_init_objects(re, lay, nolamps, onlyselected, actob, 0);

	set_material_lightgroups(re);
	
	/* SHADOW BUFFER */
	if(type!=RE_BAKE_LIGHT)
		if(re->r.mode & R_SHADOW)
			threaded_makeshadowbufs(re);

	/* raytree */
	if(!re->test_break(re->tbh))
		if(re->r.mode & R_RAYTRACE)
			makeraytree(re);
	
	/* occlusion */
	if((re->wrld.mode & WO_AMB_OCC) && !re->test_break(re->tbh))
		if(re->wrld.ao_gather_method == WO_AOGATHER_APPROX)
			if(re->r.mode & R_SHADOW)
				make_occ_tree(re);
}

/* ------------------------------------------------------------------------- */
/* Sticky texture coords													 */
/* ------------------------------------------------------------------------- */

void RE_make_sticky(Scene *scene, View3D *v3d)
{
	Object *ob;
	Base *base;
	MVert *mvert;
	Mesh *me;
	MSticky *ms;
	Render *re;
	float ho[4], mat[4][4];
	int a;
	
	if(v3d==NULL) {
		printf("Need a 3d view to make sticky\n");
		return;
	}
	
	if(scene->camera==NULL) {
		printf("Need camera to make sticky\n");
		return;
	}
	if(scene->obedit) {
		printf("Unable to make sticky in Edit Mode\n");
		return;
	}
	
	re= RE_NewRender("_make sticky_");
	RE_InitState(re, NULL, &scene->r, scene->r.xsch, scene->r.ysch, NULL);
	
	/* use renderdata and camera to set viewplane */
	RE_SetCamera(re, scene->camera);

	/* and set view matrix */
	normalize_m4(scene->camera->obmat);
	invert_m4_m4(mat, scene->camera->obmat);
	RE_SetView(re, mat);
	
	for(base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(v3d, base) {
			if(base->object->type==OB_MESH) {
				ob= base->object;
				
				me= ob->data;
				mvert= me->mvert;
				if(me->msticky)
					CustomData_free_layer_active(&me->vdata, CD_MSTICKY, me->totvert);
				me->msticky= CustomData_add_layer(&me->vdata, CD_MSTICKY,
					CD_CALLOC, NULL, me->totvert);
				
				where_is_object(scene, ob);
				mul_m4_m4m4(mat, ob->obmat, re->viewmat);
				
				ms= me->msticky;
				for(a=0; a<me->totvert; a++, ms++, mvert++) {
					VECCOPY(ho, mvert->co);
					mul_m4_v3(mat, ho);
					projectverto(ho, re->winmat, ho);
					ms->co[0]= ho[0]/ho[3];
					ms->co[1]= ho[1]/ho[3];
				}
			}
		}
	}
}

