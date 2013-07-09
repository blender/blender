/*
 * Copyright 2011, Blender Foundation.
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
 */

/* Parts adapted from code in the public domain in NVidia Mesh Tools. */

#include "mesh.h"

#include "subd_patch.h"

#include "util_math.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

/* De Casteljau Evaluation */

static float3 decasteljau_quadratic(float t, const float3 cp[3])
{
	float3 d0 = cp[0] + t*(cp[1] - cp[0]);
	float3 d1 = cp[1] + t*(cp[2] - cp[1]);

	return d0 + t*(d1 - d0);
}

static void decasteljau_cubic(float3 *P, float3 *dt, float t, const float3 cp[4])
{
	float3 d0 = cp[0] + t*(cp[1] - cp[0]);
	float3 d1 = cp[1] + t*(cp[2] - cp[1]);
	float3 d2 = cp[2] + t*(cp[3] - cp[2]);

	d0 += t*(d1 - d0);
	d1 += t*(d2 - d1);

	*P = d0 + t*(d1 - d0);
	if(dt) *dt = d1 - d0;
}

static void decasteljau_bicubic(float3 *P, float3 *du, float3 *dv, const float3 cp[16], float u, float v)
{
	float3 ucp[4], utn[4];

	/* interpolate over u */
	decasteljau_cubic(ucp+0, utn+0, u, cp);
	decasteljau_cubic(ucp+1, utn+1, u, cp+4);
	decasteljau_cubic(ucp+2, utn+2, u, cp+8);
	decasteljau_cubic(ucp+3, utn+3, u, cp+12);

	/* interpolate over v */
	decasteljau_cubic(P, dv, v, ucp);
	if(du) decasteljau_cubic(du, NULL, v, utn);
}

static float3 decasteljau_tangent(const float3 cp[12], float u, float v)
{
	float3 ucp[3];

	decasteljau_cubic(ucp+0, NULL, v, cp);
	decasteljau_cubic(ucp+1, NULL, v, cp+4);
	decasteljau_cubic(ucp+2, NULL, v, cp+8);

	return decasteljau_quadratic(u, ucp);
}

/* Linear Quad Patch */

void LinearQuadPatch::eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v)
{
	float3 d0 = interp(hull[0], hull[1], u);
	float3 d1 = interp(hull[2], hull[3], u);

	*P = interp(d0, d1, v);

	if(dPdu && dPdv) {
		*dPdu = interp(hull[1] - hull[0], hull[3] - hull[2], v);
		*dPdv = interp(hull[2] - hull[0], hull[3] - hull[1], u);
	}
}

BoundBox LinearQuadPatch::bound()
{
	BoundBox bbox = BoundBox::empty;

	for(int i = 0; i < 4; i++)
		bbox.grow(hull[i]);
	
	return bbox;
}

/* Linear Triangle Patch */

void LinearTrianglePatch::eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v)
{
	*P = u*hull[0] + v*hull[1] + (1.0f - u - v)*hull[2];

	if(dPdu && dPdv) {
		*dPdu = hull[0] - hull[2];
		*dPdv = hull[1] - hull[2];
	}
}

BoundBox LinearTrianglePatch::bound()
{
	BoundBox bbox = BoundBox::empty;

	for(int i = 0; i < 3; i++)
		bbox.grow(hull[i]);
	
	return bbox;
}

/* Bicubic Patch */

void BicubicPatch::eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v)
{
	decasteljau_bicubic(P, dPdu, dPdv, hull, u, v);
}

BoundBox BicubicPatch::bound()
{
	BoundBox bbox = BoundBox::empty;

	for(int i = 0; i < 16; i++)
		bbox.grow(hull[i]);
	
	return bbox;
}

/* Bicubic Patch with Tangent Fields */

void BicubicTangentPatch::eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v)
{
	decasteljau_bicubic(P, NULL, NULL, hull, u, v);

	if(dPdu) *dPdu = decasteljau_tangent(utan, u, v);
	if(dPdv) *dPdv = decasteljau_tangent(vtan, v, u);
}

BoundBox BicubicTangentPatch::bound()
{
	BoundBox bbox = BoundBox::empty;

	for(int i = 0; i < 16; i++)
		bbox.grow(hull[i]);
	
	return bbox;
}

/* Gregory Patch */

static float no_zero_div(float f)
{
	if(f == 0.0f) return 1.0f;
	return f;
}

void GregoryQuadPatch::eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v)
{
	float3 bicubic[16];

	float U = 1 - u;
	float V = 1 - v;

	/*  8     9     10     11
	 * 12   0\1     2/3    13
	 * 14   4/5     6\7    15
	 * 16    17     18     19
	 */

	bicubic[5] = (u*hull[1] + v*hull[0])/no_zero_div(u + v);
	bicubic[6] = (U*hull[2] + v*hull[3])/no_zero_div(U + v);
	bicubic[9] = (u*hull[5] + V*hull[4])/no_zero_div(u + V);
	bicubic[10] = (U*hull[6] + V*hull[7])/no_zero_div(U + V);

	// Map gregory control points to bezier control points.
	bicubic[0] = hull[8];
	bicubic[1] = hull[9];
	bicubic[2] = hull[10];
	bicubic[3] = hull[11];
	bicubic[4] = hull[12];
	bicubic[7] = hull[13];
	bicubic[8] = hull[14];
	bicubic[11] = hull[15];
	bicubic[12] = hull[16];
	bicubic[13] = hull[17];
	bicubic[14] = hull[18];
	bicubic[15] = hull[19];

	decasteljau_bicubic(P, dPdu, dPdv, bicubic, u, v);
}

BoundBox GregoryQuadPatch::bound()
{
	BoundBox bbox = BoundBox::empty;

	for(int i = 0; i < 20; i++)
		bbox.grow(hull[i]);
	
	return bbox;
}

void GregoryTrianglePatch::eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v)
{
	/*		      6
	 *		      
	 *		14   0/1   7
	 *						  
	 *    13   5/4     3\2   8
	 *		       
	 * 12      11       10      9
	 */

	float w = 1 - u - v;
	float uu = u * u;
	float vv = v * v;
	float ww = w * w;
	float uuu = uu * u;
	float vvv = vv * v;
	float www = ww * w;

	float U = 1 - u;
	float V = 1 - v;
	float W = 1 - w;

	float3 C0 = ( v*U * hull[5] + u*V * hull[4] ) / no_zero_div(v*U + u*V);
	float3 C1 = ( w*V * hull[3] + v*W * hull[2] ) / no_zero_div(w*V + v*W);
	float3 C2 = ( u*W * hull[1] + w*U * hull[0] ) / no_zero_div(u*W + w*U);

	*P =
		(hull[12] * www + 3*hull[11] * ww*u + 3*hull[10] * w*uu + hull[ 9]*uuu) * (w + u) +
		(hull[ 9] * uuu + 3*hull[ 8] * uu*v + 3*hull[ 7] * u*vv + hull[ 6]*vvv) * (u + v) +
		(hull[ 6] * vvv + 3*hull[14] * vv*w + 3*hull[13] * v*ww + hull[12]*www) * (v + w) -
		(hull[12] * www*w + hull[ 9] * uuu*u + hull[ 6] * vvv*v) +
		12*(C0 * u*v*ww + C1 * uu*v*w   + C2 * u*vv*w);

	if(dPdu || dPdv) {
		float3 E1 = (hull[12]*www + 3*hull[11]*ww*u + 3*hull[10]*w*uu + hull[ 9]*uuu);
		float3 E2 = (hull[ 9]*uuu + 3*hull[ 8]*uu*v + 3*hull[ 7]*u*vv + hull[ 6]*vvv);
		float3 E3 = (hull[ 6]*vvv + 3*hull[14]*vv*w + 3*hull[13]*v*ww + hull[12]*www);

		if(dPdu) {
			float3 E1u = 3*( - hull[12]*ww + hull[11]*(ww-2*u*w) +   hull[10]*(2*u*w-uu) + hull[ 9]*uu);
			float3 E2u = 3*(   hull[ 9]*uu + 2*hull[ 8]*u*v      +   hull[ 7]*vv		 );
			float3 E3u = 3*(		    - hull[14]*vv		 - 2*hull[13]*v*w		- hull[12]*ww);
			float3 Su  = 4*( -hull[12]*www + hull[9]*uuu);
			float3 Cu  = 12*( C0*(ww*v-2*u*v*w) + C1*(2*u*v*w-uu*v) + C2*vv*(w-u) );

			*dPdu = E1u*(w+u) + (E2+E2u*(u+v)) + (E3u*(v+w)-E3) - Su + Cu;
		}

		if(dPdv) {
			float3 E1v = 3*(-hull[12]*ww  - 2*hull[11]*w*u       -   hull[10]*uu		 );
			float3 E2v = 3*(		      hull[ 8]*uu		 + 2*hull[ 7]*u*v		+ hull[ 6]*vv);
			float3 E3v = 3*( hull[ 6]*vv  +  hull[14]*(2*w*v-vv) +   hull[13]*(ww-2*w*v) - hull[12]*ww);
			float3 Sv  = 4*(-hull[12]*www +  hull[ 6]*vvv);
			float3 Cv  = 12*(C0*(u*ww-2*u*v*w) + C1*uu*(w-v) + C2*(2*u*v*w-u*vv));

			*dPdv = ((E1v*(w+u)-E1) + (E2+E2v*(u+v)) + E3v*(v+w) - Sv + Cv );
		}
	}
}

BoundBox GregoryTrianglePatch::bound()
{
	BoundBox bbox = BoundBox::empty;

	for(int i = 0; i < 20; i++)
		bbox.grow(hull[i]);
	
	return bbox;
}

CCL_NAMESPACE_END

