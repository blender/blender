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
 
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_math.h"
#include "BLI_memarena.h"

/********************************** Polygons *********************************/

void cent_tri_v3(float *cent, float *v1, float *v2, float *v3)
{
	cent[0]= 0.33333f*(v1[0]+v2[0]+v3[0]);
	cent[1]= 0.33333f*(v1[1]+v2[1]+v3[1]);
	cent[2]= 0.33333f*(v1[2]+v2[2]+v3[2]);
}

void cent_quad_v3(float *cent, float *v1, float *v2, float *v3, float *v4)
{
	cent[0]= 0.25f*(v1[0]+v2[0]+v3[0]+v4[0]);
	cent[1]= 0.25f*(v1[1]+v2[1]+v3[1]+v4[1]);
	cent[2]= 0.25f*(v1[2]+v2[2]+v3[2]+v4[2]);
}

float normal_tri_v3(float *n, float *v1, float *v2, float *v3)
{
	float n1[3],n2[3];

	n1[0]= v1[0]-v2[0];
	n2[0]= v2[0]-v3[0];
	n1[1]= v1[1]-v2[1];
	n2[1]= v2[1]-v3[1];
	n1[2]= v1[2]-v2[2];
	n2[2]= v2[2]-v3[2];
	n[0]= n1[1]*n2[2]-n1[2]*n2[1];
	n[1]= n1[2]*n2[0]-n1[0]*n2[2];
	n[2]= n1[0]*n2[1]-n1[1]*n2[0];
	return normalize_v3(n);
}

float normal_quad_v3(float *n, float *v1, float *v2, float *v3, float *v4)
{
	/* real cross! */
	float n1[3],n2[3];

	n1[0]= v1[0]-v3[0];
	n1[1]= v1[1]-v3[1];
	n1[2]= v1[2]-v3[2];

	n2[0]= v2[0]-v4[0];
	n2[1]= v2[1]-v4[1];
	n2[2]= v2[2]-v4[2];

	n[0]= n1[1]*n2[2]-n1[2]*n2[1];
	n[1]= n1[2]*n2[0]-n1[0]*n2[2];
	n[2]= n1[0]*n2[1]-n1[1]*n2[0];

	return normalize_v3(n);
}

float area_tri_v2(float *v1, float *v2, float *v3)
{
	return (float)(0.5*fabs((v1[0]-v2[0])*(v2[1]-v3[1]) + (v1[1]-v2[1])*(v3[0]-v2[0])));
}


float area_quad_v3(float *v1, float *v2, float *v3,  float *v4)  /* only convex Quadrilaterals */
{
	float len, vec1[3], vec2[3], n[3];

	sub_v3_v3v3(vec1, v2, v1);
	sub_v3_v3v3(vec2, v4, v1);
	cross_v3_v3v3(n, vec1, vec2);
	len= normalize_v3(n);

	sub_v3_v3v3(vec1, v4, v3);
	sub_v3_v3v3(vec2, v2, v3);
	cross_v3_v3v3(n, vec1, vec2);
	len+= normalize_v3(n);

	return (len/2.0f);
}

float area_tri_v3(float *v1, float *v2, float *v3)  /* Triangles */
{
	float len, vec1[3], vec2[3], n[3];

	sub_v3_v3v3(vec1, v3, v2);
	sub_v3_v3v3(vec2, v1, v2);
	cross_v3_v3v3(n, vec1, vec2);
	len= normalize_v3(n);

	return (len/2.0f);
}

#define MAX2(x,y)		((x)>(y) ? (x) : (y))
#define MAX3(x,y,z)		MAX2(MAX2((x),(y)) , (z))


float area_poly_v3(int nr, float verts[][3], float *normal)
{
	float x, y, z, area, max;
	float *cur, *prev;
	int a, px=0, py=1;

	/* first: find dominant axis: 0==X, 1==Y, 2==Z */
	x= (float)fabs(normal[0]);
	y= (float)fabs(normal[1]);
	z= (float)fabs(normal[2]);
	max = MAX3(x, y, z);
	if(max==y) py=2;
	else if(max==x) {
		px=1; 
		py= 2;
	}

	/* The Trapezium Area Rule */
	prev= verts[nr-1];
	cur= verts[0];
	area= 0;
	for(a=0; a<nr; a++) {
		area+= (cur[px]-prev[px])*(cur[py]+prev[py]);
		prev= verts[a];
		cur= verts[a+1];
	}

	return (float)fabs(0.5*area/max);
}

/********************************* Distance **********************************/

/* distance v1 to line v2-v3 */
/* using Hesse formula, NO LINE PIECE! */
float dist_to_line_v2(float *v1, float *v2, float *v3)
{
	float a[2],deler;

	a[0]= v2[1]-v3[1];
	a[1]= v3[0]-v2[0];
	deler= (float)sqrt(a[0]*a[0]+a[1]*a[1]);
	if(deler== 0.0f) return 0;

	return (float)(fabs((v1[0]-v2[0])*a[0]+(v1[1]-v2[1])*a[1])/deler);

}

/* distance v1 to line-piece v2-v3 */
float dist_to_line_segment_v2(float *v1, float *v2, float *v3) 
{
	float labda, rc[2], pt[2], len;
	
	rc[0]= v3[0]-v2[0];
	rc[1]= v3[1]-v2[1];
	len= rc[0]*rc[0]+ rc[1]*rc[1];
	if(len==0.0) {
		rc[0]= v1[0]-v2[0];
		rc[1]= v1[1]-v2[1];
		return (float)(sqrt(rc[0]*rc[0]+ rc[1]*rc[1]));
	}
	
	labda= (rc[0]*(v1[0]-v2[0]) + rc[1]*(v1[1]-v2[1]))/len;
	if(labda<=0.0) {
		pt[0]= v2[0];
		pt[1]= v2[1];
	}
	else if(labda>=1.0) {
		pt[0]= v3[0];
		pt[1]= v3[1];
	}
	else {
		pt[0]= labda*rc[0]+v2[0];
		pt[1]= labda*rc[1]+v2[1];
	}

	rc[0]= pt[0]-v1[0];
	rc[1]= pt[1]-v1[1];
	return (float)sqrt(rc[0]*rc[0]+ rc[1]*rc[1]);
}

/* point closest to v1 on line v2-v3 in 3D */
void closest_to_line_segment_v3(float *closest, float v1[3], float v2[3], float v3[3])
{
	float lambda, cp[3];

	lambda= closest_to_line_v3(cp,v1, v2, v3);

	if(lambda <= 0.0f)
		copy_v3_v3(closest, v2);
	else if(lambda >= 1.0f)
		copy_v3_v3(closest, v3);
	else
		copy_v3_v3(closest, cp);
}

/* distance v1 to line-piece v2-v3 in 3D */
float dist_to_line_segment_v3(float *v1, float *v2, float *v3) 
{
	float closest[3];

	closest_to_line_segment_v3(closest, v1, v2, v3);

	return len_v3v3(closest, v1);
}

/******************************* Intersection ********************************/

/* intersect Line-Line, shorts */
int isect_line_line_v2_short(short *v1, short *v2, short *v3, short *v4)
{
	/* return:
	-1: colliniar
	 0: no intersection of segments
	 1: exact intersection of segments
	 2: cross-intersection of segments
	*/
	float div, labda, mu;
	
	div= (float)((v2[0]-v1[0])*(v4[1]-v3[1])-(v2[1]-v1[1])*(v4[0]-v3[0]));
	if(div==0.0f) return -1;
	
	labda= ((float)(v1[1]-v3[1])*(v4[0]-v3[0])-(v1[0]-v3[0])*(v4[1]-v3[1]))/div;
	
	mu= ((float)(v1[1]-v3[1])*(v2[0]-v1[0])-(v1[0]-v3[0])*(v2[1]-v1[1]))/div;
	
	if(labda>=0.0f && labda<=1.0f && mu>=0.0f && mu<=1.0f) {
		if(labda==0.0f || labda==1.0f || mu==0.0f || mu==1.0f) return 1;
		return 2;
	}
	return 0;
}

/* intersect Line-Line, floats */
int isect_line_line_v2(float *v1, float *v2, float *v3, float *v4)
{
	/* return:
	-1: colliniar
0: no intersection of segments
1: exact intersection of segments
2: cross-intersection of segments
	*/
	float div, labda, mu;
	
	div= (v2[0]-v1[0])*(v4[1]-v3[1])-(v2[1]-v1[1])*(v4[0]-v3[0]);
	if(div==0.0) return -1;
	
	labda= ((float)(v1[1]-v3[1])*(v4[0]-v3[0])-(v1[0]-v3[0])*(v4[1]-v3[1]))/div;
	
	mu= ((float)(v1[1]-v3[1])*(v2[0]-v1[0])-(v1[0]-v3[0])*(v2[1]-v1[1]))/div;
	
	if(labda>=0.0 && labda<=1.0 && mu>=0.0 && mu<=1.0) {
		if(labda==0.0 || labda==1.0 || mu==0.0 || mu==1.0) return 1;
		return 2;
	}
	return 0;
}

/*
-1: colliniar
 1: intersection

*/
static short IsectLLPt2Df(float x0,float y0,float x1,float y1,
					 float x2,float y2,float x3,float y3, float *xi,float *yi)

{
	/*
	this function computes the intersection of the sent lines
	and returns the intersection point, note that the function assumes
	the lines intersect. the function can handle vertical as well
	as horizontal lines. note the function isn't very clever, it simply
	applies the math, but we don't need speed since this is a
	pre-processing step
	*/
	float c1,c2, // constants of linear equations
	det_inv,  // the inverse of the determinant of the coefficient
	m1,m2;    // the slopes of each line
	/*
	compute slopes, note the cludge for infinity, however, this will
	be close enough
	*/
	if (fabs(x1-x0) > 0.000001)
		m1 = (y1-y0) / (x1-x0);
	else
		return -1; /*m1 = (float) 1e+10;*/   // close enough to infinity

	if (fabs(x3-x2) > 0.000001)
		m2 = (y3-y2) / (x3-x2);
	else
		return -1; /*m2 = (float) 1e+10;*/   // close enough to infinity

	if (fabs(m1-m2) < 0.000001)
		return -1; /* paralelle lines */
	
// compute constants

	c1 = (y0-m1*x0);
	c2 = (y2-m2*x2);

// compute the inverse of the determinate

	det_inv = 1.0f / (-m1 + m2);

// use Kramers rule to compute xi and yi

	*xi= ((-c2 + c1) *det_inv);
	*yi= ((m2*c1 - m1*c2) *det_inv);
	
	return 1; 
} // end Intersect_Lines

#define SIDE_OF_LINE(pa,pb,pp)	((pa[0]-pp[0])*(pb[1]-pp[1]))-((pb[0]-pp[0])*(pa[1]-pp[1]))
/* point in tri */
// XXX was called IsectPT2Df
int isect_point_tri_v2(float pt[2], float v1[2], float v2[2], float v3[2])
{
	if (SIDE_OF_LINE(v1,v2,pt)>=0.0) {
		if (SIDE_OF_LINE(v2,v3,pt)>=0.0) {
			if (SIDE_OF_LINE(v3,v1,pt)>=0.0) {
				return 1;
			}
		}
	} else {
		if (! (SIDE_OF_LINE(v2,v3,pt)>=0.0)) {
			if (! (SIDE_OF_LINE(v3,v1,pt)>=0.0)) {
				return -1;
			}
		}
	}
	
	return 0;
}
/* point in quad - only convex quads */
int isect_point_quad_v2(float pt[2], float v1[2], float v2[2], float v3[2], float v4[2])
{
	if (SIDE_OF_LINE(v1,v2,pt)>=0.0) {
		if (SIDE_OF_LINE(v2,v3,pt)>=0.0) {
			if (SIDE_OF_LINE(v3,v4,pt)>=0.0) {
				if (SIDE_OF_LINE(v4,v1,pt)>=0.0) {
					return 1;
				}
			}
		}
	} else {
		if (! (SIDE_OF_LINE(v2,v3,pt)>=0.0)) {
			if (! (SIDE_OF_LINE(v3,v4,pt)>=0.0)) {
				if (! (SIDE_OF_LINE(v4,v1,pt)>=0.0)) {
					return -1;
				}
			}
		}
	}
	
	return 0;
}

/* moved from effect.c
   test if the line starting at p1 ending at p2 intersects the triangle v0..v2
   return non zero if it does 
*/
int isect_line_tri_v3(float p1[3], float p2[3], float v0[3], float v1[3], float v2[3], float *lambda, float *uv)
{

	float p[3], s[3], d[3], e1[3], e2[3], q[3];
	float a, f, u, v;
	
	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);
	sub_v3_v3v3(d, p2, p1);
	
	cross_v3_v3v3(p, d, e2);
	a = dot_v3v3(e1, p);
	if ((a > -0.000001) && (a < 0.000001)) return 0;
	f = 1.0f/a;
	
	sub_v3_v3v3(s, p1, v0);
	
	cross_v3_v3v3(q, s, e1);
	*lambda = f * dot_v3v3(e2, q);
	if ((*lambda < 0.0)||(*lambda > 1.0)) return 0;
	
	u = f * dot_v3v3(s, p);
	if ((u < 0.0)||(u > 1.0)) return 0;
	
	v = f * dot_v3v3(d, q);
	if ((v < 0.0)||((u + v) > 1.0)) return 0;

	if(uv) {
		uv[0]= u;
		uv[1]= v;
	}
	
	return 1;
}

/* moved from effect.c
   test if the ray starting at p1 going in d direction intersects the triangle v0..v2
   return non zero if it does 
*/
int isect_ray_tri_v3(float p1[3], float d[3], float v0[3], float v1[3], float v2[3], float *lambda, float *uv)
{
	float p[3], s[3], e1[3], e2[3], q[3];
	float a, f, u, v;
	
	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);
	
	cross_v3_v3v3(p, d, e2);
	a = dot_v3v3(e1, p);
	/* note: these values were 0.000001 in 2.4x but for projection snapping on
	 * a human head (1BU==1m), subsurf level 2, this gave many errors - campbell */
	if ((a > -0.00000001) && (a < 0.00000001)) return 0;
	f = 1.0f/a;
	
	sub_v3_v3v3(s, p1, v0);
	
	cross_v3_v3v3(q, s, e1);
	*lambda = f * dot_v3v3(e2, q);
	if ((*lambda < 0.0)) return 0;
	
	u = f * dot_v3v3(s, p);
	if ((u < 0.0)||(u > 1.0)) return 0;
	
	v = f * dot_v3v3(d, q);
	if ((v < 0.0)||((u + v) > 1.0)) return 0;

	if(uv) {
		uv[0]= u;
		uv[1]= v;
	}
	
	return 1;
}

int isect_ray_tri_threshold_v3(float p1[3], float d[3], float v0[3], float v1[3], float v2[3], float *lambda, float *uv, float threshold)
{
	float p[3], s[3], e1[3], e2[3], q[3];
	float a, f, u, v;
	float du = 0, dv = 0;
	
	sub_v3_v3v3(e1, v1, v0);
	sub_v3_v3v3(e2, v2, v0);
	
	cross_v3_v3v3(p, d, e2);
	a = dot_v3v3(e1, p);
	if ((a > -0.000001) && (a < 0.000001)) return 0;
	f = 1.0f/a;
	
	sub_v3_v3v3(s, p1, v0);
	
	cross_v3_v3v3(q, s, e1);
	*lambda = f * dot_v3v3(e2, q);
	if ((*lambda < 0.0)) return 0;
	
	u = f * dot_v3v3(s, p);
	v = f * dot_v3v3(d, q);
	
	if (u < 0) du = u;
	if (u > 1) du = u - 1;
	if (v < 0) dv = v;
	if (v > 1) dv = v - 1;
	if (u > 0 && v > 0 && u + v > 1)
	{
		float t = u + v - 1;
		du = u - t/2;
		dv = v - t/2;
	}

	mul_v3_fl(e1, du);
	mul_v3_fl(e2, dv);
	
	if (dot_v3v3(e1, e1) + dot_v3v3(e2, e2) > threshold * threshold)
	{
		return 0;
	}

	if(uv) {
		uv[0]= u;
		uv[1]= v;
	}
	
	return 1;
}


/* Adapted from the paper by Kasper Fauerby */
/* "Improved Collision detection and Response" */
static int getLowestRoot(float a, float b, float c, float maxR, float* root)
{
	// Check if a solution exists
	float determinant = b*b - 4.0f*a*c;

	// If determinant is negative it means no solutions.
	if (determinant >= 0.0f)
	{
		// calculate the two roots: (if determinant == 0 then
		// x1==x2 but let’s disregard that slight optimization)
		float sqrtD = (float)sqrt(determinant);
		float r1 = (-b - sqrtD) / (2.0f*a);
		float r2 = (-b + sqrtD) / (2.0f*a);
		
		// Sort so x1 <= x2
		if (r1 > r2)
			SWAP(float, r1, r2);

		// Get lowest root:
		if (r1 > 0.0f && r1 < maxR)
		{
			*root = r1;
			return 1;
		}

		// It is possible that we want x2 - this can happen
		// if x1 < 0
		if (r2 > 0.0f && r2 < maxR)
		{
			*root = r2;
			return 1;
		}
	}
	// No (valid) solutions
	return 0;
}

int isect_sweeping_sphere_tri_v3(float p1[3], float p2[3], float radius, float v0[3], float v1[3], float v2[3], float *lambda, float *ipoint)
{
	float e1[3], e2[3], e3[3], point[3], vel[3], /*dist[3],*/ nor[3], temp[3], bv[3];
	float a, b, c, d, e, x, y, z, radius2=radius*radius;
	float elen2,edotv,edotbv,nordotv,vel2;
	float newLambda;
	int found_by_sweep=0;

	sub_v3_v3v3(e1,v1,v0);
	sub_v3_v3v3(e2,v2,v0);
	sub_v3_v3v3(vel,p2,p1);

/*---test plane of tri---*/
	cross_v3_v3v3(nor,e1,e2);
	normalize_v3(nor);

	/* flip normal */
	if(dot_v3v3(nor,vel)>0.0f) negate_v3(nor);
	
	a=dot_v3v3(p1,nor)-dot_v3v3(v0,nor);
	nordotv=dot_v3v3(nor,vel);

	if (fabs(nordotv) < 0.000001)
	{
		if(fabs(a)>=radius)
		{
			return 0;
		}
	}
	else
	{
		float t0=(-a+radius)/nordotv;
		float t1=(-a-radius)/nordotv;

		if(t0>t1)
			SWAP(float, t0, t1);

		if(t0>1.0f || t1<0.0f) return 0;

		/* clamp to [0,1] */
		CLAMP(t0, 0.0f, 1.0f);
		CLAMP(t1, 0.0f, 1.0f);

		/*---test inside of tri---*/
		/* plane intersection point */

		point[0] = p1[0] + vel[0]*t0 - nor[0]*radius;
		point[1] = p1[1] + vel[1]*t0 - nor[1]*radius;
		point[2] = p1[2] + vel[2]*t0 - nor[2]*radius;


		/* is the point in the tri? */
		a=dot_v3v3(e1,e1);
		b=dot_v3v3(e1,e2);
		c=dot_v3v3(e2,e2);

		sub_v3_v3v3(temp,point,v0);
		d=dot_v3v3(temp,e1);
		e=dot_v3v3(temp,e2);
		
		x=d*c-e*b;
		y=e*a-d*b;
		z=x+y-(a*c-b*b);


		if(z <= 0.0f && (x >= 0.0f && y >= 0.0f))
		{
		//(((unsigned int)z)& ~(((unsigned int)x)|((unsigned int)y))) & 0x80000000){
			*lambda=t0;
			copy_v3_v3(ipoint,point);
			return 1;
		}
	}


	*lambda=1.0f;

/*---test points---*/
	a=vel2=dot_v3v3(vel,vel);

	/*v0*/
	sub_v3_v3v3(temp,p1,v0);
	b=2.0f*dot_v3v3(vel,temp);
	c=dot_v3v3(temp,temp)-radius2;

	if(getLowestRoot(a, b, c, *lambda, lambda))
	{
		copy_v3_v3(ipoint,v0);
		found_by_sweep=1;
	}

	/*v1*/
	sub_v3_v3v3(temp,p1,v1);
	b=2.0f*dot_v3v3(vel,temp);
	c=dot_v3v3(temp,temp)-radius2;

	if(getLowestRoot(a, b, c, *lambda, lambda))
	{
		copy_v3_v3(ipoint,v1);
		found_by_sweep=1;
	}
	
	/*v2*/
	sub_v3_v3v3(temp,p1,v2);
	b=2.0f*dot_v3v3(vel,temp);
	c=dot_v3v3(temp,temp)-radius2;

	if(getLowestRoot(a, b, c, *lambda, lambda))
	{
		copy_v3_v3(ipoint,v2);
		found_by_sweep=1;
	}

/*---test edges---*/
	sub_v3_v3v3(e3,v2,v1); //wasnt yet calculated


	/*e1*/
	sub_v3_v3v3(bv,v0,p1);

	elen2 = dot_v3v3(e1,e1);
	edotv = dot_v3v3(e1,vel);
	edotbv = dot_v3v3(e1,bv);

	a=elen2*(-dot_v3v3(vel,vel))+edotv*edotv;
	b=2.0f*(elen2*dot_v3v3(vel,bv)-edotv*edotbv);
	c=elen2*(radius2-dot_v3v3(bv,bv))+edotbv*edotbv;

	if(getLowestRoot(a, b, c, *lambda, &newLambda))
	{
		e=(edotv*newLambda-edotbv)/elen2;

		if(e >= 0.0f && e <= 1.0f)
		{
			*lambda = newLambda;
			copy_v3_v3(ipoint,e1);
			mul_v3_fl(ipoint,e);
			add_v3_v3v3(ipoint,ipoint,v0);
			found_by_sweep=1;
		}
	}

	/*e2*/
	/*bv is same*/
	elen2 = dot_v3v3(e2,e2);
	edotv = dot_v3v3(e2,vel);
	edotbv = dot_v3v3(e2,bv);

	a=elen2*(-dot_v3v3(vel,vel))+edotv*edotv;
	b=2.0f*(elen2*dot_v3v3(vel,bv)-edotv*edotbv);
	c=elen2*(radius2-dot_v3v3(bv,bv))+edotbv*edotbv;

	if(getLowestRoot(a, b, c, *lambda, &newLambda))
	{
		e=(edotv*newLambda-edotbv)/elen2;

		if(e >= 0.0f && e <= 1.0f)
		{
			*lambda = newLambda;
			copy_v3_v3(ipoint,e2);
			mul_v3_fl(ipoint,e);
			add_v3_v3v3(ipoint,ipoint,v0);
			found_by_sweep=1;
		}
	}

	/*e3*/
	sub_v3_v3v3(bv,v0,p1);
	elen2 = dot_v3v3(e1,e1);
	edotv = dot_v3v3(e1,vel);
	edotbv = dot_v3v3(e1,bv);

	sub_v3_v3v3(bv,v1,p1);
	elen2 = dot_v3v3(e3,e3);
	edotv = dot_v3v3(e3,vel);
	edotbv = dot_v3v3(e3,bv);

	a=elen2*(-dot_v3v3(vel,vel))+edotv*edotv;
	b=2.0f*(elen2*dot_v3v3(vel,bv)-edotv*edotbv);
	c=elen2*(radius2-dot_v3v3(bv,bv))+edotbv*edotbv;

	if(getLowestRoot(a, b, c, *lambda, &newLambda))
	{
		e=(edotv*newLambda-edotbv)/elen2;

		if(e >= 0.0f && e <= 1.0f)
		{
			*lambda = newLambda;
			copy_v3_v3(ipoint,e3);
			mul_v3_fl(ipoint,e);
			add_v3_v3v3(ipoint,ipoint,v1);
			found_by_sweep=1;
		}
	}


	return found_by_sweep;
}
int isect_axial_line_tri_v3(int axis, float p1[3], float p2[3], float v0[3], float v1[3], float v2[3], float *lambda)
{
	float p[3], e1[3], e2[3];
	float u, v, f;
	int a0=axis, a1=(axis+1)%3, a2=(axis+2)%3;

	//return isect_line_tri_v3(p1,p2,v0,v1,v2,lambda);

	///* first a simple bounding box test */
	//if(MIN3(v0[a1],v1[a1],v2[a1]) > p1[a1]) return 0;
	//if(MIN3(v0[a2],v1[a2],v2[a2]) > p1[a2]) return 0;
	//if(MAX3(v0[a1],v1[a1],v2[a1]) < p1[a1]) return 0;
	//if(MAX3(v0[a2],v1[a2],v2[a2]) < p1[a2]) return 0;

	///* then a full intersection test */
	
	sub_v3_v3v3(e1,v1,v0);
	sub_v3_v3v3(e2,v2,v0);
	sub_v3_v3v3(p,v0,p1);

	f= (e2[a1]*e1[a2]-e2[a2]*e1[a1]);
	if ((f > -0.000001) && (f < 0.000001)) return 0;

	v= (p[a2]*e1[a1]-p[a1]*e1[a2])/f;
	if ((v < 0.0)||(v > 1.0)) return 0;
	
	f= e1[a1];
	if((f > -0.000001) && (f < 0.000001)){
		f= e1[a2];
		if((f > -0.000001) && (f < 0.000001)) return 0;
		u= (-p[a2]-v*e2[a2])/f;
	}
	else
		u= (-p[a1]-v*e2[a1])/f;

	if ((u < 0.0)||((u + v) > 1.0)) return 0;

	*lambda = (p[a0]+u*e1[a0]+v*e2[a0])/(p2[a0]-p1[a0]);

	if ((*lambda < 0.0)||(*lambda > 1.0)) return 0;

	return 1;
}

/* Returns the number of point of interests
 * 0 - lines are colinear
 * 1 - lines are coplanar, i1 is set to intersection
 * 2 - i1 and i2 are the nearest points on line 1 (v1, v2) and line 2 (v3, v4) respectively 
 * */
int isect_line_line_v3(float v1[3], float v2[3], float v3[3], float v4[3], float i1[3], float i2[3])
{
	float a[3], b[3], c[3], ab[3], cb[3], dir1[3], dir2[3];
	float d;
	
	sub_v3_v3v3(c, v3, v1);
	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v4, v3);

	copy_v3_v3(dir1, a);
	normalize_v3(dir1);
	copy_v3_v3(dir2, b);
	normalize_v3(dir2);
	d = dot_v3v3(dir1, dir2);
	if (d == 1.0f || d == -1.0f) {
		/* colinear */
		return 0;
	}

	cross_v3_v3v3(ab, a, b);
	d = dot_v3v3(c, ab);

	/* test if the two lines are coplanar */
	if (d > -0.000001f && d < 0.000001f) {
		cross_v3_v3v3(cb, c, b);

		mul_v3_fl(a, dot_v3v3(cb, ab) / dot_v3v3(ab, ab));
		add_v3_v3v3(i1, v1, a);
		copy_v3_v3(i2, i1);
		
		return 1; /* one intersection only */
	}
	/* if not */
	else {
		float n[3], t[3];
		float v3t[3], v4t[3];
		sub_v3_v3v3(t, v1, v3);

		/* offset between both plane where the lines lies */
		cross_v3_v3v3(n, a, b);
		project_v3_v3v3(t, t, n);

		/* for the first line, offset the second line until it is coplanar */
		add_v3_v3v3(v3t, v3, t);
		add_v3_v3v3(v4t, v4, t);
		
		sub_v3_v3v3(c, v3t, v1);
		sub_v3_v3v3(a, v2, v1);
		sub_v3_v3v3(b, v4t, v3t);

		cross_v3_v3v3(ab, a, b);
		cross_v3_v3v3(cb, c, b);

		mul_v3_fl(a, dot_v3v3(cb, ab) / dot_v3v3(ab, ab));
		add_v3_v3v3(i1, v1, a);

		/* for the second line, just substract the offset from the first intersection point */
		sub_v3_v3v3(i2, i1, t);
		
		return 2; /* two nearest points */
	}
} 

/* Intersection point strictly between the two lines
 * 0 when no intersection is found 
 * */
int isect_line_line_strict_v3(float v1[3], float v2[3], float v3[3], float v4[3], float vi[3], float *lambda)
{
	float a[3], b[3], c[3], ab[3], cb[3], ca[3], dir1[3], dir2[3];
	float d;
	float d1;
	
	sub_v3_v3v3(c, v3, v1);
	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v4, v3);

	copy_v3_v3(dir1, a);
	normalize_v3(dir1);
	copy_v3_v3(dir2, b);
	normalize_v3(dir2);
	d = dot_v3v3(dir1, dir2);
	if (d == 1.0f || d == -1.0f || d == 0) {
		/* colinear or one vector is zero-length*/
		return 0;
	}
	
	d1 = d;

	cross_v3_v3v3(ab, a, b);
	d = dot_v3v3(c, ab);

	/* test if the two lines are coplanar */
	if (d > -0.000001f && d < 0.000001f) {
		float f1, f2;
		cross_v3_v3v3(cb, c, b);
		cross_v3_v3v3(ca, c, a);

		f1 = dot_v3v3(cb, ab) / dot_v3v3(ab, ab);
		f2 = dot_v3v3(ca, ab) / dot_v3v3(ab, ab);
		
		if (f1 >= 0 && f1 <= 1 &&
			f2 >= 0 && f2 <= 1)
		{
			mul_v3_fl(a, f1);
			add_v3_v3v3(vi, v1, a);
			
			if (lambda != NULL)
			{
				*lambda = f1;
			}
			
			return 1; /* intersection found */
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}
} 

int isect_aabb_aabb_v3(float min1[3], float max1[3], float min2[3], float max2[3])
{
	return (min1[0]<max2[0] && min1[1]<max2[1] && min1[2]<max2[2] &&
	        min2[0]<max1[0] && min2[1]<max1[1] && min2[2]<max1[2]);
}

/* find closest point to p on line through l1,l2 and return lambda,
 * where (0 <= lambda <= 1) when cp is in the line segement l1,l2
 */
float closest_to_line_v3(float cp[3],float p[3], float l1[3], float l2[3])
{
	float h[3],u[3],lambda;
	sub_v3_v3v3(u, l2, l1);
	sub_v3_v3v3(h, p, l1);
	lambda =dot_v3v3(u,h)/dot_v3v3(u,u);
	cp[0] = l1[0] + u[0] * lambda;
	cp[1] = l1[1] + u[1] * lambda;
	cp[2] = l1[2] + u[2] * lambda;
	return lambda;
}

#if 0
/* little sister we only need to know lambda */
static float lambda_cp_line(float p[3], float l1[3], float l2[3])
{
	float h[3],u[3];
	sub_v3_v3v3(u, l2, l1);
	sub_v3_v3v3(h, p, l1);
	return(dot_v3v3(u,h)/dot_v3v3(u,u));
}
#endif

/* Similar to LineIntersectsTriangleUV, except it operates on a quad and in 2d, assumes point is in quad */
void isect_point_quad_uv_v2(float v0[2], float v1[2], float v2[2], float v3[2], float pt[2], float *uv)
{
	float x0,y0, x1,y1, wtot, v2d[2], w1, w2;
	
	/* used for paralelle lines */
	float pt3d[3], l1[3], l2[3], pt_on_line[3];
	
	/* compute 2 edges  of the quad  intersection point */
	if (IsectLLPt2Df(v0[0],v0[1],v1[0],v1[1],  v2[0],v2[1],v3[0],v3[1], &x0,&y0) == 1) {
		/* the intersection point between the quad-edge intersection and the point in the quad we want the uv's for */
		/* should never be paralle !! */
		/*printf("\tnot paralelle 1\n");*/
		IsectLLPt2Df(pt[0],pt[1],x0,y0,  v0[0],v0[1],v3[0],v3[1], &x1,&y1);
		
		/* Get the weights from the new intersection point, to each edge */
		v2d[0] = x1-v0[0];
		v2d[1] = y1-v0[1];
		w1 = len_v2(v2d);
		
		v2d[0] = x1-v3[0]; /* some but for the other vert */
		v2d[1] = y1-v3[1];
		w2 = len_v2(v2d);
		wtot = w1+w2;
		/*w1 = w1/wtot;*/
		/*w2 = w2/wtot;*/
		uv[0] = w1/wtot;
	} else {
		/* lines are paralelle, lambda_cp_line_ex is 3d grrr */
		/*printf("\tparalelle1\n");*/
		pt3d[0] = pt[0];
		pt3d[1] = pt[1];
		pt3d[2] = l1[2] = l2[2] = 0.0f;
		
		l1[0] = v0[0]; l1[1] = v0[1];
		l2[0] = v1[0]; l2[1] = v1[1];
		closest_to_line_v3(pt_on_line,pt3d, l1, l2);
		v2d[0] = pt[0]-pt_on_line[0]; /* same, for the other vert */
		v2d[1] = pt[1]-pt_on_line[1];
		w1 = len_v2(v2d);
		
		l1[0] = v2[0]; l1[1] = v2[1];
		l2[0] = v3[0]; l2[1] = v3[1];
		closest_to_line_v3(pt_on_line,pt3d, l1, l2);
		v2d[0] = pt[0]-pt_on_line[0]; /* same, for the other vert */
		v2d[1] = pt[1]-pt_on_line[1];
		w2 = len_v2(v2d);
		wtot = w1+w2;
		uv[0] = w1/wtot;	
	}
	
	/* Same as above to calc the uv[1] value, alternate calculation */
	
	if (IsectLLPt2Df(v0[0],v0[1],v3[0],v3[1],  v1[0],v1[1],v2[0],v2[1], &x0,&y0) == 1) { /* was v0,v1  v2,v3  now v0,v3  v1,v2*/
		/* never paralle if above was not */
		/*printf("\tnot paralelle2\n");*/
		IsectLLPt2Df(pt[0],pt[1],x0,y0,  v0[0],v0[1],v1[0],v1[1], &x1,&y1);/* was v0,v3  now v0,v1*/
		
		v2d[0] = x1-v0[0];
		v2d[1] = y1-v0[1];
		w1 = len_v2(v2d);
		
		v2d[0] = x1-v1[0];
		v2d[1] = y1-v1[1];
		w2 = len_v2(v2d);
		wtot = w1+w2;
		uv[1] = w1/wtot;
	} else {
		/* lines are paralelle, lambda_cp_line_ex is 3d grrr */
		/*printf("\tparalelle2\n");*/
		pt3d[0] = pt[0];
		pt3d[1] = pt[1];
		pt3d[2] = l1[2] = l2[2] = 0.0f;
		
		
		l1[0] = v0[0]; l1[1] = v0[1];
		l2[0] = v3[0]; l2[1] = v3[1];
		closest_to_line_v3(pt_on_line,pt3d, l1, l2);
		v2d[0] = pt[0]-pt_on_line[0]; /* some but for the other vert */
		v2d[1] = pt[1]-pt_on_line[1];
		w1 = len_v2(v2d);
		
		l1[0] = v1[0]; l1[1] = v1[1];
		l2[0] = v2[0]; l2[1] = v2[1];
		closest_to_line_v3(pt_on_line,pt3d, l1, l2);
		v2d[0] = pt[0]-pt_on_line[0]; /* some but for the other vert */
		v2d[1] = pt[1]-pt_on_line[1];
		w2 = len_v2(v2d);
		wtot = w1+w2;
		uv[1] = w1/wtot;
	}
	/* may need to flip UV's here */
}

/* same as above but does tri's and quads, tri's are a bit of a hack */
void isect_point_face_uv_v2(int isquad, float v0[2], float v1[2], float v2[2], float v3[2], float pt[2], float *uv)
{
	if (isquad) {
		isect_point_quad_uv_v2(v0, v1, v2, v3, pt, uv);
	}
	else {
		/* not for quads, use for our abuse of LineIntersectsTriangleUV */
		float p1_3d[3], p2_3d[3], v0_3d[3], v1_3d[3], v2_3d[3], lambda;
			
		p1_3d[0] = p2_3d[0] = uv[0];
		p1_3d[1] = p2_3d[1] = uv[1];
		p1_3d[2] = 1.0f;
		p2_3d[2] = -1.0f;
		v0_3d[2] = v1_3d[2] = v2_3d[2] = 0.0;
		
		/* generate a new fuv, (this is possibly a non optimal solution,
		 * since we only need 2d calculation but use 3d func's)
		 * 
		 * this method makes an imaginary triangle in 2d space using the UV's from the derived mesh face
		 * Then find new uv coords using the fuv and this face with LineIntersectsTriangleUV.
		 * This means the new values will be correct in relation to the derived meshes face. 
		 */
		copy_v2_v2(v0_3d, v0);
		copy_v2_v2(v1_3d, v1);
		copy_v2_v2(v2_3d, v2);
		
		/* Doing this in 3D is not nice */
		isect_line_tri_v3(p1_3d, p2_3d, v0_3d, v1_3d, v2_3d, &lambda, uv);
	}
}

#if 0 // XXX this version used to be used in isect_point_tri_v2_int() and was called IsPointInTri2D
int isect_point_tri_v2(float pt[2], float v1[2], float v2[2], float v3[2])
{
	float inp1, inp2, inp3;
	
	inp1= (v2[0]-v1[0])*(v1[1]-pt[1]) + (v1[1]-v2[1])*(v1[0]-pt[0]);
	inp2= (v3[0]-v2[0])*(v2[1]-pt[1]) + (v2[1]-v3[1])*(v2[0]-pt[0]);
	inp3= (v1[0]-v3[0])*(v3[1]-pt[1]) + (v3[1]-v1[1])*(v3[0]-pt[0]);
	
	if(inp1<=0.0f && inp2<=0.0f && inp3<=0.0f) return 1;
	if(inp1>=0.0f && inp2>=0.0f && inp3>=0.0f) return 1;
	
	return 0;
}
#endif

#if 0
int isect_point_tri_v2(float v0[2], float v1[2], float v2[2], float pt[2])
{
		/* not for quads, use for our abuse of LineIntersectsTriangleUV */
		float p1_3d[3], p2_3d[3], v0_3d[3], v1_3d[3], v2_3d[3];
		/* not used */
		float lambda, uv[3];
			
		p1_3d[0] = p2_3d[0] = uv[0]= pt[0];
		p1_3d[1] = p2_3d[1] = uv[1]= uv[2]= pt[1];
		p1_3d[2] = 1.0f;
		p2_3d[2] = -1.0f;
		v0_3d[2] = v1_3d[2] = v2_3d[2] = 0.0;
		
		/* generate a new fuv, (this is possibly a non optimal solution,
		 * since we only need 2d calculation but use 3d func's)
		 * 
		 * this method makes an imaginary triangle in 2d space using the UV's from the derived mesh face
		 * Then find new uv coords using the fuv and this face with LineIntersectsTriangleUV.
		 * This means the new values will be correct in relation to the derived meshes face. 
		 */
		copy_v2_v2(v0_3d, v0);
		copy_v2_v2(v1_3d, v1);
		copy_v2_v2(v2_3d, v2);
		
		/* Doing this in 3D is not nice */
		return isect_line_tri_v3(p1_3d, p2_3d, v0_3d, v1_3d, v2_3d, &lambda, uv);
}
#endif

/*

	x1,y2
	|  \
	|   \     .(a,b)
	|    \
	x1,y1-- x2,y1

*/
int isect_point_tri_v2_int(int x1, int y1, int x2, int y2, int a, int b)
{
	float v1[2], v2[2], v3[2], p[2];
	
	v1[0]= (float)x1;
	v1[1]= (float)y1;
	
	v2[0]= (float)x1;
	v2[1]= (float)y2;
	
	v3[0]= (float)x2;
	v3[1]= (float)y1;
	
	p[0]= (float)a;
	p[1]= (float)b;
	
	return isect_point_tri_v2(p, v1, v2, v3);
}

static int point_in_slice(float p[3], float v1[3], float l1[3], float l2[3])
{
/* 
what is a slice ?
some maths:
a line including l1,l2 and a point not on the line 
define a subset of R3 delimeted by planes parallel to the line and orthogonal 
to the (point --> line) distance vector,one plane on the line one on the point, 
the room inside usually is rather small compared to R3 though still infinte
useful for restricting (speeding up) searches 
e.g. all points of triangular prism are within the intersection of 3 'slices'
onother trivial case : cube 
but see a 'spat' which is a deformed cube with paired parallel planes needs only 3 slices too
*/
	float h,rp[3],cp[3],q[3];

	closest_to_line_v3(cp,v1,l1,l2);
	sub_v3_v3v3(q,cp,v1);

	sub_v3_v3v3(rp,p,v1);
	h=dot_v3v3(q,rp)/dot_v3v3(q,q);
	if (h < 0.0f || h > 1.0f) return 0;
	return 1;
}

#if 0
/*adult sister defining the slice planes by the origin and the normal  
NOTE |normal| may not be 1 but defining the thickness of the slice*/
static int point_in_slice_as(float p[3],float origin[3],float normal[3])
{
	float h,rp[3];
	sub_v3_v3v3(rp,p,origin);
	h=dot_v3v3(normal,rp)/dot_v3v3(normal,normal);
	if (h < 0.0f || h > 1.0f) return 0;
	return 1;
}

/*mama (knowing the squared lenght of the normal)*/
static int point_in_slice_m(float p[3],float origin[3],float normal[3],float lns)
{
	float h,rp[3];
	sub_v3_v3v3(rp,p,origin);
	h=dot_v3v3(normal,rp)/lns;
	if (h < 0.0f || h > 1.0f) return 0;
	return 1;
}
#endif

int isect_point_tri_prism_v3(float p[3], float v1[3], float v2[3], float v3[3])
{
	if(!point_in_slice(p,v1,v2,v3)) return 0;
	if(!point_in_slice(p,v2,v3,v1)) return 0;
	if(!point_in_slice(p,v3,v1,v2)) return 0;
	return 1;
}

/****************************** Interpolation ********************************/

static float tri_signed_area(float *v1, float *v2, float *v3, int i, int j)
{
	return 0.5f*((v1[i]-v2[i])*(v2[j]-v3[j]) + (v1[j]-v2[j])*(v3[i]-v2[i]));
}

static int barycentric_weights(float *v1, float *v2, float *v3, float *co, float *n, float *w)
{
	float xn, yn, zn, a1, a2, a3, asum;
	short i, j;

	/* find best projection of face XY, XZ or YZ: barycentric weights of
	   the 2d projected coords are the same and faster to compute */
	xn= (float)fabs(n[0]);
	yn= (float)fabs(n[1]);
	zn= (float)fabs(n[2]);
	if(zn>=xn && zn>=yn) {i= 0; j= 1;}
	else if(yn>=xn && yn>=zn) {i= 0; j= 2;}
	else {i= 1; j= 2;} 

	a1= tri_signed_area(v2, v3, co, i, j);
	a2= tri_signed_area(v3, v1, co, i, j);
	a3= tri_signed_area(v1, v2, co, i, j);

	asum= a1 + a2 + a3;

	if (fabs(asum) < FLT_EPSILON) {
		/* zero area triangle */
		w[0]= w[1]= w[2]= 1.0f/3.0f;
		return 1;
	}

	asum= 1.0f/asum;
	w[0]= a1*asum;
	w[1]= a2*asum;
	w[2]= a3*asum;

	return 0;
}

void interp_weights_face_v3(float *w,float *v1, float *v2, float *v3, float *v4, float *co)
{
	float w2[3];

	w[0]= w[1]= w[2]= w[3]= 0.0f;

	/* first check for exact match */
	if(equals_v3v3(co, v1))
		w[0]= 1.0f;
	else if(equals_v3v3(co, v2))
		w[1]= 1.0f;
	else if(equals_v3v3(co, v3))
		w[2]= 1.0f;
	else if(v4 && equals_v3v3(co, v4))
		w[3]= 1.0f;
	else {
		/* otherwise compute barycentric interpolation weights */
		float n1[3], n2[3], n[3];
		int degenerate;

		sub_v3_v3v3(n1, v1, v3);
		if (v4) {
			sub_v3_v3v3(n2, v2, v4);
		}
		else {
			sub_v3_v3v3(n2, v2, v3);
		}
		cross_v3_v3v3(n, n1, n2);

		/* OpenGL seems to split this way, so we do too */
		if (v4) {
			degenerate= barycentric_weights(v1, v2, v4, co, n, w);
			SWAP(float, w[2], w[3]);

			if(degenerate || (w[0] < 0.0f)) {
				/* if w[1] is negative, co is on the other side of the v1-v3 edge,
				   so we interpolate using the other triangle */
				degenerate= barycentric_weights(v2, v3, v4, co, n, w2);

				if(!degenerate) {
					w[0]= 0.0f;
					w[1]= w2[0];
					w[2]= w2[1];
					w[3]= w2[2];
				}
			}
		}
		else
			barycentric_weights(v1, v2, v3, co, n, w);
	}
}

/* Mean value weights - smooth interpolation weights for polygons with
 * more than 3 vertices */
static float mean_value_half_tan(float *v1, float *v2, float *v3)
{
	float d2[3], d3[3], cross[3], area, dot, len;

	sub_v3_v3v3(d2, v2, v1);
	sub_v3_v3v3(d3, v3, v1);
	cross_v3_v3v3(cross, d2, d3);

	area= len_v3(cross);
	dot= dot_v3v3(d2, d3);
	len= len_v3(d2)*len_v3(d3);

	if(area == 0.0f)
		return 0.0f;
	else
		return (len - dot)/area;
}

void interp_weights_poly_v3(float *w,float v[][3], int n, float *co)
{
	float totweight, t1, t2, len, *vmid, *vprev, *vnext;
	int i;

	totweight= 0.0f;

	for(i=0; i<n; i++) {
		vmid= v[i];
		vprev= (i == 0)? v[n-1]: v[i-1];
		vnext= (i == n-1)? v[0]: v[i+1];

		t1= mean_value_half_tan(co, vprev, vmid);
		t2= mean_value_half_tan(co, vmid, vnext);

		len= len_v3v3(co, vmid);
		w[i]= (t1+t2)/len;
		totweight += w[i];
	}

	if(totweight != 0.0f)
		for(i=0; i<n; i++)
			w[i] /= totweight;
}

/* (x1,v1)(t1=0)------(x2,v2)(t2=1), 0<t<1 --> (x,v)(t) */
void interp_cubic_v3(float *x, float *v,float *x1, float *v1, float *x2, float *v2, float t)
{
	float a[3],b[3];
	float t2= t*t;
	float t3= t2*t;

	/* cubic interpolation */
	a[0]= v1[0] + v2[0] + 2*(x1[0] - x2[0]);
	a[1]= v1[1] + v2[1] + 2*(x1[1] - x2[1]);
	a[2]= v1[2] + v2[2] + 2*(x1[2] - x2[2]);

	b[0]= -2*v1[0] - v2[0] - 3*(x1[0] - x2[0]);
	b[1]= -2*v1[1] - v2[1] - 3*(x1[1] - x2[1]);
	b[2]= -2*v1[2] - v2[2] - 3*(x1[2] - x2[2]);

	x[0]= a[0]*t3 + b[0]*t2 + v1[0]*t + x1[0];
	x[1]= a[1]*t3 + b[1]*t2 + v1[1]*t + x1[1];
	x[2]= a[2]*t3 + b[2]*t2 + v1[2]*t + x1[2];

	v[0]= 3*a[0]*t2 + 2*b[0]*t + v1[0];
	v[1]= 3*a[1]*t2 + 2*b[1]*t + v1[1];
	v[2]= 3*a[2]*t2 + 2*b[2]*t + v1[2];
}

/***************************** View & Projection *****************************/

void orthographic_m4(float matrix[][4],float left, float right, float bottom, float top, float nearClip, float farClip)
{
    float Xdelta, Ydelta, Zdelta;
 
    Xdelta = right - left;
    Ydelta = top - bottom;
    Zdelta = farClip - nearClip;
    if (Xdelta == 0.0 || Ydelta == 0.0 || Zdelta == 0.0) {
		return;
    }
    unit_m4(matrix);
    matrix[0][0] = 2.0f/Xdelta;
    matrix[3][0] = -(right + left)/Xdelta;
    matrix[1][1] = 2.0f/Ydelta;
    matrix[3][1] = -(top + bottom)/Ydelta;
    matrix[2][2] = -2.0f/Zdelta;		/* note: negate Z	*/
    matrix[3][2] = -(farClip + nearClip)/Zdelta;
}

void perspective_m4(float mat[][4],float left, float right, float bottom, float top, float nearClip, float farClip)
{
	float Xdelta, Ydelta, Zdelta;

	Xdelta = right - left;
	Ydelta = top - bottom;
	Zdelta = farClip - nearClip;

	if (Xdelta == 0.0 || Ydelta == 0.0 || Zdelta == 0.0) {
		return;
	}
	mat[0][0] = nearClip * 2.0f/Xdelta;
	mat[1][1] = nearClip * 2.0f/Ydelta;
	mat[2][0] = (right + left)/Xdelta;		/* note: negate Z	*/
	mat[2][1] = (top + bottom)/Ydelta;
	mat[2][2] = -(farClip + nearClip)/Zdelta;
	mat[2][3] = -1.0f;
	mat[3][2] = (-2.0f * nearClip * farClip)/Zdelta;
	mat[0][1] = mat[0][2] = mat[0][3] =
	    mat[1][0] = mat[1][2] = mat[1][3] =
	    mat[3][0] = mat[3][1] = mat[3][3] = 0.0;

}

static void i_multmatrix(float icand[][4], float Vm[][4])
{
    int row, col;
    float temp[4][4];

    for(row=0 ; row<4 ; row++) 
        for(col=0 ; col<4 ; col++)
            temp[row][col] = icand[row][0] * Vm[0][col]
                           + icand[row][1] * Vm[1][col]
                           + icand[row][2] * Vm[2][col]
                           + icand[row][3] * Vm[3][col];
	copy_m4_m4(Vm, temp);
}


void polarview_m4(float Vm[][4],float dist, float azimuth, float incidence, float twist)
{

	unit_m4(Vm);

    translate_m4(Vm,0.0, 0.0, -dist);
    rotate_m4(Vm,'z',-twist);	
    rotate_m4(Vm,'x',-incidence);
    rotate_m4(Vm,'z',-azimuth);
}

void lookat_m4(float mat[][4],float vx, float vy, float vz, float px, float py, float pz, float twist)
{
	float sine, cosine, hyp, hyp1, dx, dy, dz;
	float mat1[4][4];
	
	unit_m4(mat);
	unit_m4(mat1);

	rotate_m4(mat,'z',-twist);

	dx = px - vx;
	dy = py - vy;
	dz = pz - vz;
	hyp = dx * dx + dz * dz;	/* hyp squared	*/
	hyp1 = (float)sqrt(dy*dy + hyp);
	hyp = (float)sqrt(hyp);		/* the real hyp	*/
	
	if (hyp1 != 0.0) {		/* rotate X	*/
		sine = -dy / hyp1;
		cosine = hyp /hyp1;
	} else {
		sine = 0;
		cosine = 1.0f;
	}
	mat1[1][1] = cosine;
	mat1[1][2] = sine;
	mat1[2][1] = -sine;
	mat1[2][2] = cosine;
	
	i_multmatrix(mat1, mat);

    mat1[1][1] = mat1[2][2] = 1.0f;	/* be careful here to reinit	*/
    mat1[1][2] = mat1[2][1] = 0.0;	/* those modified by the last	*/
	
	/* paragraph	*/
	if (hyp != 0.0f) {			/* rotate Y	*/
		sine = dx / hyp;
		cosine = -dz / hyp;
	} else {
		sine = 0;
		cosine = 1.0f;
	}
	mat1[0][0] = cosine;
	mat1[0][2] = -sine;
	mat1[2][0] = sine;
	mat1[2][2] = cosine;
	
	i_multmatrix(mat1, mat);
	translate_m4(mat,-vx,-vy,-vz);	/* translate viewpoint to origin */
}

/********************************** Mapping **********************************/

void map_to_tube(float *u, float *v,float x, float y, float z)
{
	float len;
	
	*v = (z + 1.0f) / 2.0f;
	
	len= (float)sqrt(x*x+y*y);
	if(len > 0.0f)
		*u = (float)((1.0 - (atan2(x/len,y/len) / M_PI)) / 2.0);
	else
		*v = *u = 0.0f; /* to avoid un-initialized variables */
}

void map_to_sphere(float *u, float *v,float x, float y, float z)
{
	float len;
	
	len= (float)sqrt(x*x+y*y+z*z);
	if(len > 0.0f) {
		if(x==0.0f && y==0.0f) *u= 0.0f;	/* othwise domain error */
		else *u = (float)((1.0 - (float)atan2(x,y) / M_PI) / 2.0);
		
		z/=len;
		*v = 1.0f - (float)saacos(z)/(float)M_PI;
	} else {
		*v = *u = 0.0f; /* to avoid un-initialized variables */
	}
}

/********************************************************/

/* Tangents */

/* For normal map tangents we need to detect uv boundaries, and only average
 * tangents in case the uvs are connected. Alternative would be to store 1 
 * tangent per face rather than 4 per face vertex, but that's not compatible
 * with games */


/* from BKE_mesh.h */
#define STD_UV_CONNECT_LIMIT	0.0001f

void sum_or_add_vertex_tangent(void *arena, VertexTangent **vtang, float *tang, float *uv)
{
	VertexTangent *vt;

	/* find a tangent with connected uvs */
	for(vt= *vtang; vt; vt=vt->next) {
		if(fabs(uv[0]-vt->uv[0]) < STD_UV_CONNECT_LIMIT && fabs(uv[1]-vt->uv[1]) < STD_UV_CONNECT_LIMIT) {
			add_v3_v3v3(vt->tang, vt->tang, tang);
			return;
		}
	}

	/* if not found, append a new one */
	vt= BLI_memarena_alloc((MemArena *)arena, sizeof(VertexTangent));
	copy_v3_v3(vt->tang, tang);
	vt->uv[0]= uv[0];
	vt->uv[1]= uv[1];

	if(*vtang)
		vt->next= *vtang;
	*vtang= vt;
}

float *find_vertex_tangent(VertexTangent *vtang, float *uv)
{
	VertexTangent *vt;
	static float nulltang[3] = {0.0f, 0.0f, 0.0f};

	for(vt= vtang; vt; vt=vt->next)
		if(fabs(uv[0]-vt->uv[0]) < STD_UV_CONNECT_LIMIT && fabs(uv[1]-vt->uv[1]) < STD_UV_CONNECT_LIMIT)
			return vt->tang;

	return nulltang;	/* shouldn't happen, except for nan or so */
}

void tangent_from_uv(float *uv1, float *uv2, float *uv3, float *co1, float *co2, float *co3, float *n, float *tang)
{
	float tangv[3], ct[3], e1[3], e2[3], s1, t1, s2, t2, det;

	s1= uv2[0] - uv1[0];
	s2= uv3[0] - uv1[0];
	t1= uv2[1] - uv1[1];
	t2= uv3[1] - uv1[1];
	det= 1.0f / (s1 * t2 - s2 * t1);
	
	/* normals in render are inversed... */
	sub_v3_v3v3(e1, co1, co2);
	sub_v3_v3v3(e2, co1, co3);
	tang[0] = (t2*e1[0] - t1*e2[0])*det;
	tang[1] = (t2*e1[1] - t1*e2[1])*det;
	tang[2] = (t2*e1[2] - t1*e2[2])*det;
	tangv[0] = (s1*e2[0] - s2*e1[0])*det;
	tangv[1] = (s1*e2[1] - s2*e1[1])*det;
	tangv[2] = (s1*e2[2] - s2*e1[2])*det;
	cross_v3_v3v3(ct, tang, tangv);

	/* check flip */
	if ((ct[0]*n[0] + ct[1]*n[1] + ct[2]*n[2]) < 0.0f)
		negate_v3(tang);
}

/********************************************************/

/* vector clouds */
/* void vcloud_estimate_transform(int list_size, float (*pos)[3], float *weight,float (*rpos)[3], float *rweight,
  							   float lloc[3],float rloc[3],float lrot[3][3],float lscale[3][3])

input
(
int list_size
4 lists as pointer to array[list_size]
1. current pos array of 'new' positions 
2. current weight array of 'new'weights (may be NULL pointer if you have no weights )
3. reference rpos array of 'old' positions
4. reference rweight array of 'old'weights (may be NULL pointer if you have no weights )
)
output  
(
float lloc[3] center of mass pos
float rloc[3] center of mass rpos
float lrot[3][3] rotation matrix
float lscale[3][3] scale matrix
pointers may be NULL if not needed
)

*/
/* can't believe there is none in math utils */
float _det_m3(float m2[3][3])
{
    float det = 0.f;
    if (m2){
    det= m2[0][0]* (m2[1][1]*m2[2][2] - m2[1][2]*m2[2][1])
        -m2[1][0]* (m2[0][1]*m2[2][2] - m2[0][2]*m2[2][1])
        +m2[2][0]* (m2[0][1]*m2[1][2] - m2[0][2]*m2[1][1]);
    }
    return det;
}


void vcloud_estimate_transform(int list_size, float (*pos)[3], float *weight,float (*rpos)[3], float *rweight,
							float lloc[3],float rloc[3],float lrot[3][3],float lscale[3][3])
{
	float accu_com[3]= {0.0f,0.0f,0.0f}, accu_rcom[3]= {0.0f,0.0f,0.0f};
	float accu_weight = 0.0f,accu_rweight = 0.0f,eps = 0.000001f;

	int a;
	/* first set up a nice default response */
	if (lloc) zero_v3(lloc);
	if (rloc) zero_v3(rloc);
	if (lrot) unit_m3(lrot);
	if (lscale) unit_m3(lscale);
	/* do com for both clouds */
	if (pos && rpos && (list_size > 0)) /* paranoya check */
	{
		/* do com for both clouds */
		for(a=0; a<list_size; a++){
			if (weight){
				float v[3];
				copy_v3_v3(v,pos[a]);
				mul_v3_fl(v,weight[a]);
				add_v3_v3v3(accu_com,accu_com,v);
				accu_weight +=weight[a]; 
			}
			else add_v3_v3v3(accu_com,accu_com,pos[a]);

			if (rweight){
				float v[3];
				copy_v3_v3(v,rpos[a]);
				mul_v3_fl(v,rweight[a]);
				add_v3_v3v3(accu_rcom,accu_rcom,v);
				accu_rweight +=rweight[a]; 
			}
			else add_v3_v3v3(accu_rcom,accu_rcom,rpos[a]);

		}
		if (!weight || !rweight){
			accu_weight = accu_rweight = list_size;
		}

		mul_v3_fl(accu_com,1.0f/accu_weight);
		mul_v3_fl(accu_rcom,1.0f/accu_rweight);
		if (lloc) copy_v3_v3(lloc,accu_com);
		if (rloc) copy_v3_v3(rloc,accu_rcom);
		if (lrot || lscale){ /* caller does not want rot nor scale, strange but legal */
			/*so now do some reverse engeneering and see if we can split rotation from scale ->Polardecompose*/
			/* build 'projection' matrix */
			float m[3][3],mr[3][3],q[3][3],qi[3][3];
			float va[3],vb[3],stunt[3];
			float odet,ndet;
			int i=0,imax=15;
			zero_m3(m);
			zero_m3(mr);

			/* build 'projection' matrix */
			for(a=0; a<list_size; a++){
				sub_v3_v3v3(va,rpos[a],accu_rcom);
				/* mul_v3_fl(va,bp->mass);  mass needs renormalzation here ?? */
				sub_v3_v3v3(vb,pos[a],accu_com);
				/* mul_v3_fl(va,rp->mass); */
				m[0][0] += va[0] * vb[0];
				m[0][1] += va[0] * vb[1];
				m[0][2] += va[0] * vb[2];

				m[1][0] += va[1] * vb[0];
				m[1][1] += va[1] * vb[1];
				m[1][2] += va[1] * vb[2];

				m[2][0] += va[2] * vb[0];
				m[2][1] += va[2] * vb[1];
				m[2][2] += va[2] * vb[2];

				/* building the referenc matrix on the fly
				needed to scale properly later*/

				mr[0][0] += va[0] * va[0];
				mr[0][1] += va[0] * va[1];
				mr[0][2] += va[0] * va[2];

				mr[1][0] += va[1] * va[0];
				mr[1][1] += va[1] * va[1];
				mr[1][2] += va[1] * va[2];

				mr[2][0] += va[2] * va[0];
				mr[2][1] += va[2] * va[1];
				mr[2][2] += va[2] * va[2];
			}
			copy_m3_m3(q,m);
			stunt[0] = q[0][0]; stunt[1] = q[1][1]; stunt[2] = q[2][2];
			/* renormalizing for numeric stability */
			mul_m3_fl(q,1.f/len_v3(stunt)); 

			/* this is pretty much Polardecompose 'inline' the algo based on Higham's thesis */
			/* without the far case ... but seems to work here pretty neat                   */
			odet = 0.f;
			ndet = _det_m3(q);
			while((odet-ndet)*(odet-ndet) > eps && i<imax){
				invert_m3_m3(qi,q);
				transpose_m3(qi);
				add_m3_m3m3(q,q,qi);
				mul_m3_fl(q,0.5f);
				odet =ndet;
				ndet =_det_m3(q);
				i++;
			}

			if (i){
				float scale[3][3];
				float irot[3][3];
				if(lrot) copy_m3_m3(lrot,q);
				invert_m3_m3(irot,q);
				invert_m3_m3(qi,mr);
				mul_m3_m3m3(q,m,qi); 
				mul_m3_m3m3(scale,irot,q); 
				if(lscale) copy_m3_m3(lscale,scale);

			}
		}
	}
}
