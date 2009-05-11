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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include "assert.h"

#include "BKE_utildefines.h"

#include "RE_raytrace.h"
#include "render_types.h"
#include "rayobject.h"

/* only for self-intersecting test with current render face (where ray left) */
static int intersection2(VlakRen *face, float r0, float r1, float r2, float rx1, float ry1, float rz1)
{
	float co1[3], co2[3], co3[3], co4[3];
	float x0,x1,x2,t00,t01,t02,t10,t11,t12,t20,t21,t22;
	float m0, m1, m2, divdet, det, det1;
	float u1, v, u2;

	VECCOPY(co1, face->v1->co);
	VECCOPY(co2, face->v2->co);
	if(face->v4)
	{
		VECCOPY(co3, face->v4->co);
		VECCOPY(co4, face->v3->co);
	}
	else
	{
		VECCOPY(co3, face->v3->co);
	}

	t00= co3[0]-co1[0];
	t01= co3[1]-co1[1];
	t02= co3[2]-co1[2];
	t10= co3[0]-co2[0];
	t11= co3[1]-co2[1];
	t12= co3[2]-co2[2];
	
	x0= t11*r2-t12*r1;
	x1= t12*r0-t10*r2;
	x2= t10*r1-t11*r0;

	divdet= t00*x0+t01*x1+t02*x2;

	m0= rx1-co3[0];
	m1= ry1-co3[1];
	m2= rz1-co3[2];
	det1= m0*x0+m1*x1+m2*x2;
	
	if(divdet!=0.0f) {
		u1= det1/divdet;

		if(u1<ISECT_EPSILON) {
			det= t00*(m1*r2-m2*r1);
			det+= t01*(m2*r0-m0*r2);
			det+= t02*(m0*r1-m1*r0);
			v= det/divdet;

			if(v<ISECT_EPSILON && (u1 + v) > -(1.0f+ISECT_EPSILON)) {
				return 1;
			}
		}
	}

	if(face->v4) {

		t20= co3[0]-co4[0];
		t21= co3[1]-co4[1];
		t22= co3[2]-co4[2];

		divdet= t20*x0+t21*x1+t22*x2;
		if(divdet!=0.0f) {
			u2= det1/divdet;
		
			if(u2<ISECT_EPSILON) {
				det= t20*(m1*r2-m2*r1);
				det+= t21*(m2*r0-m0*r2);
				det+= t22*(m0*r1-m1*r0);
				v= det/divdet;
	
				if(v<ISECT_EPSILON && (u2 + v) >= -(1.0f+ISECT_EPSILON)) {
					return 2;
				}
			}
		}
	}
	return 0;
}


/* ray - triangle or quad intersection */
/* this function shall only modify Isect if it detects an hit */
static int intersect_rayface(RayFace *face, Isect *is)
{
	float co1[3],co2[3],co3[3],co4[3];
	float x0,x1,x2,t00,t01,t02,t10,t11,t12,t20,t21,t22,r0,r1,r2;
	float m0, m1, m2, divdet, det1;
	float labda, u, v;
	short ok=0;
	
	if(is->orig.ob == face->ob && is->orig.face == face->face)
		return 0;

	/* disabled until i got real & fast cylinder checking, this code doesnt work proper for faster strands */
	//	if(is->mode==RE_RAY_SHADOW && is->vlr->flag & R_STRAND) 
	//		return intersection_strand(is);
	

	VECCOPY(co1, face->v1);
	VECCOPY(co2, face->v2);
	if(face->v4)
	{
		VECCOPY(co3, face->v4);
		VECCOPY(co4, face->v3);
	}
	else
	{
		VECCOPY(co3, face->v3);
	}

	t00= co3[0]-co1[0];
	t01= co3[1]-co1[1];
	t02= co3[2]-co1[2];
	t10= co3[0]-co2[0];
	t11= co3[1]-co2[1];
	t12= co3[2]-co2[2];
	
	r0= is->vec[0];
	r1= is->vec[1];
	r2= is->vec[2];
	
	x0= t12*r1-t11*r2;
	x1= t10*r2-t12*r0;
	x2= t11*r0-t10*r1;

	divdet= t00*x0+t01*x1+t02*x2;

	m0= is->start[0]-co3[0];
	m1= is->start[1]-co3[1];
	m2= is->start[2]-co3[2];
	det1= m0*x0+m1*x1+m2*x2;
	
	if(divdet!=0.0f) {

		divdet= 1.0f/divdet;
		u= det1*divdet;
		if(u<ISECT_EPSILON && u>-(1.0f+ISECT_EPSILON)) {
			float cros0, cros1, cros2;
			
			cros0= m1*t02-m2*t01;
			cros1= m2*t00-m0*t02;
			cros2= m0*t01-m1*t00;
			v= divdet*(cros0*r0 + cros1*r1 + cros2*r2);

			if(v<ISECT_EPSILON && (u + v) > -(1.0f+ISECT_EPSILON)) {
				labda= divdet*(cros0*t10 + cros1*t11 + cros2*t12);

				if(labda>-ISECT_EPSILON && labda<1.0f+ISECT_EPSILON) {
					ok= 1;
				}
			}
		}
	}

	if(ok==0 && face->v4) {

		t20= co3[0]-co4[0];
		t21= co3[1]-co4[1];
		t22= co3[2]-co4[2];

		divdet= t20*x0+t21*x1+t22*x2;
		if(divdet!=0.0f) {
			divdet= 1.0f/divdet;
			u = det1*divdet;
			
			if(u<ISECT_EPSILON && u>-(1.0f+ISECT_EPSILON)) {
				float cros0, cros1, cros2;
				cros0= m1*t22-m2*t21;
				cros1= m2*t20-m0*t22;
				cros2= m0*t21-m1*t20;
				v= divdet*(cros0*r0 + cros1*r1 + cros2*r2);
	
				if(v<ISECT_EPSILON && (u + v) >-(1.0f+ISECT_EPSILON)) {
					labda= divdet*(cros0*t10 + cros1*t11 + cros2*t12);
					
					if(labda>-ISECT_EPSILON && labda<1.0f+ISECT_EPSILON) {
						ok= 2;
					}
				}
			}
		}
	}

	if(ok) {
	
		/* when a shadow ray leaves a face, it can be little outside the edges of it, causing
		intersection to be detected in its neighbour face */
		if(is->skip & RE_SKIP_VLR_NEIGHBOUR)
		{
			if(labda < 0.1f && is->orig.ob == face->ob)
			{
				VlakRen * a = is->orig.face;
				VlakRen * b = face->face;

				/* so there's a shared edge or vertex, let's intersect ray with face
				itself, if that's true we can safely return 1, otherwise we assume
				the intersection is invalid, 0 */
				if(a->v1==b->v1 || a->v2==b->v1 || a->v3==b->v1 || a->v4==b->v1
				|| a->v1==b->v2 || a->v2==b->v2 || a->v3==b->v2 || a->v4==b->v2
				|| a->v1==b->v3 || a->v2==b->v3 || a->v3==b->v3 || a->v4==b->v3
				|| a->v1==b->v4 || a->v2==b->v4 || a->v3==b->v4 || (a->v4 && a->v4==b->v4))
				if(!intersection2((VlakRen*)b, -r0, -r1, -r2, is->start[0], is->start[1], is->start[2]))
				{
					return 0;
				}
			}
		}
#if 0
		else if(labda < ISECT_EPSILON)
		{
			/* too close to origin */
			return 0;
		}
#endif

/*
		TODO
		if(is->mode!=RE_RAY_SHADOW) {
			/ * for mirror & tra-shadow: large faces can be filled in too often, this prevents
			   a face being detected too soon... * /
			if(is->labda > is->ddalabda) {
				return 0;
			}
		}
*/		
		is->isect= ok;	// wich half of the quad
		is->labda= labda;
		is->u= u; is->v= v;

		is->hit.ob   = face->ob;
		is->hit.face = face->face;
		return 1;
	}

	return 0;
}

int RayObject_raycast(RayObject *r, Isect *i)
{
	if(i->mode==RE_RAY_SHADOW && i->last_hit && RayObject_intersect(i->last_hit, i))
		return 1;

	return RayObject_intersect(r, i);
}

int RayObject_intersect(RayObject *r, Isect *i)
{
	assert(i->mode==RE_RAY_SHADOW);
	if(RayObject_isFace(r))
	{
		return intersect_rayface( (RayFace*) r, i);
	}
	else
	{
		//TODO should be done somewhere else
//		float len = Normalize( i->vec );
		int hit;
		i->vec[0] *= i->labda;
		i->vec[1] *= i->labda;
		i->vec[2] *= i->labda;
		i->labda = 1.0f; //RE_RAYTRACE_MAXDIST; //len;
		
		r = RayObject_align( r );

		hit = r->api->raycast( r, i );
//		i->labda /= len;
		
		return hit;
	}
}

void RayObject_add(RayObject *r, RayObject *o)
{
	r = RayObject_align( r );
	return r->api->add( r, o );
}

void RayObject_done(RayObject *r)
{
	r = RayObject_align( r );
	r->api->done( r );
}

void RayObject_free(RayObject *r)
{
	r = RayObject_align( r );
	r->api->free( r );
}

void RayObject_merge_bb(RayObject *r, float *min, float *max)
{
	if(RayObject_isFace(r))
	{
		RayFace *face = (RayFace*)r;
		DO_MINMAX( face->v1, min, max );
		DO_MINMAX( face->v2, min, max );
		DO_MINMAX( face->v3, min, max );
		if(face->v4) DO_MINMAX( face->v4, min, max );
	}
	else
	{
		r = RayObject_align( r );
		r->api->bb( r, min, max );
	}
}

